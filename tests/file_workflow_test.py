"""Real binaries and Neovim, with a loopback relay standing in for ssh -R."""
import os
import pathlib
import select
import socket
import subprocess
import sys
import tempfile
import threading
import time

server, daemon, nvim = map(os.path.abspath, sys.argv[1:])


def relay(listener):
    while True:
        try:
            remote, _ = listener.accept()
        except OSError:
            return

        mode = os.environ.get("JANISNVIM_TEST_MODE", "")

        def forward(remote, mode):
            with remote, socket.create_connection(('127.0.0.1', 7777)) as local:
                try:
                    while True:
                        ready, _, _ = select.select([remote, local], [], [], 10)
                        for source in ready:
                            data = source.recv(65536)
                            if not data:
                                return
                            if source is local and mode == 'disconnect' and data.startswith(b'JN\x01\x02'):
                                return

                            if source is remote:
                                time.sleep(0.75)  # A slow link must not make :write synchronous.
                            (local if source is remote else remote).sendall(data)
                except OSError:
                    return

        threading.Thread(target=forward, args=(remote, mode), daemon=True).start()


with tempfile.TemporaryDirectory(prefix='janisnvim-workflow-') as temp:
    root = pathlib.Path(temp)
    remote_root = root / 'remote'
    remote_root.mkdir()
    cache = root / 'cache'
    cache.mkdir()
    test_lua = root / 'check.lua'
    test_lua.write_text('''
local ok, error = pcall(function()
  assert(vim.api.nvim_buf_get_name(0) ~= vim.env.TEST_REMOTE_FILE)
  if vim.env.TEST_CASE == 'conflict' then
    vim.fn.writefile({'remote change'}, vim.env.TEST_REMOTE_FILE)
  end

  if vim.env.TEST_CASE == 'resume' then
    assert(vim.api.nvim_buf_get_lines(0, 0, -1, false)[1] == 'unsynced recovery')
  end

  vim.api.nvim_buf_set_lines(0, 0, -1, false, {'saved locally'})
  local start = vim.loop.hrtime()
  vim.cmd('write')
  assert((vim.loop.hrtime() - start) / 1e9 < 0.5, 'write blocked on transport')

  if vim.env.TEST_CASE ~= 'wq' then
    local expected = (vim.env.TEST_CASE == 'conflict' or vim.env.TEST_CASE == 'disconnect') and 'failed' or 'synced'
    assert(vim.wait(10000, function() return vim.b.janisnvim_sync == expected end), 'sync did not finish')
  end
end)

if not ok then
  io.stderr:write(tostring(error) .. '\\n')
  vim.cmd('cquit 1')
else
  vim.cmd('quit')
end
''')

    terminal = root / 'terminal'
    terminal.write_text('#!/usr/bin/env python3\n'
                        'import os, sys\n'
                        'args = sys.argv[2:]\n'
                        'i = args.index("--")\n'
                        'args[i:i] = ["-c", "lua dofile(vim.env.TEST_LUA)"]\n'
                        'os.execv(os.environ["TEST_NVIM"], ["nvim", "--headless", "-u", "NONE", "-i", "NONE", "-n"] + args)\n')
    terminal.chmod(0o700)

    env = os.environ | {
        'JANISNVIM_TOKEN': 'test-secret-only-' + 'a' * 32,
        'JANISNVIM_TERMINAL': str(terminal),
        'XDG_CACHE_HOME': str(cache),
        'TEST_LUA': str(test_lua), 'TEST_NVIM': nvim,
    }

    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(('127.0.0.1', 7778))
        listener.listen()

        threading.Thread(target=relay, args=(listener,), daemon=True).start()

        for case in ('save', 'wq', 'conflict', 'disconnect', 'resume'):
            # Reuse the first session's cache to exercise reconnect with dirty saved bytes.
            remote_file = remote_root / ('save.cpp' if case == 'resume' else case + ' ; $ file.cpp')
            if case == 'resume':
                remote_file = remote_root / 'save ; $ file.cpp'
                cached = list(cache.rglob(remote_file.name))
                assert len(cached) == 1
                cached[0].write_text('unsynced recovery\n')
            else:
                remote_file.write_text('original\n')

            env.update(TEST_CASE=case, TEST_REMOTE_FILE=str(remote_file))
            os.environ["JANISNVIM_TEST_MODE"] = case

            with open(root / (case + '.log'), 'w+') as log:
                child = subprocess.Popen([daemon], env=env, stdout=log, stderr=log, start_new_session=True)
                try:
                    for _ in range(100):
                        if child.poll() is not None:
                            raise AssertionError('daemon exited')
                        try:
                            with socket.create_connection(('127.0.0.1', 7777), timeout=0.1):
                                break
                        except OSError:
                            time.sleep(0.02)

                    result = subprocess.run([server, str(remote_file)], cwd=remote_root, env=env,
                                            capture_output=True, text=True, timeout=20)
                    assert result.returncode == (1 if case == "disconnect" else 0), result.stdout + result.stderr

                    expected = {'conflict': 'remote change\n', 'disconnect': 'original\n'}.get(case, 'saved locally\n')
                    assert remote_file.read_text() == expected
                    if case == 'disconnect':
                        time.sleep(1)  # Let the asynchronous failure notification and editor exit complete.

                    cached = list(cache.rglob(remote_file.name))
                    assert len(cached) == 1 and cached[0].read_text() == 'saved locally\n'

                    log.seek(0)
                    output = log.read()
                    assert 'sync did not finish' not in output and 'write blocked' not in output, output

                    print(case + ': passed')
                except BaseException:
                    log.seek(0)
                    print(log.read(), file=sys.stderr)
                    raise
                finally:
                    import signal
                    os.killpg(child.pid, signal.SIGTERM)
                    child.wait(timeout=5)
