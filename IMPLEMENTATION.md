# Implementation status and operating notes

Implementation progress belongs here; README.md describes the intended architecture.

## Implemented first slice

The executables now implement **local file editing with asynchronous saved-file sync** on Linux.
Remote LSP, search, builds and other project-aware plugin integrations remain unimplemented.
Only the requested file is transferred. There is no remote editor process.

Build on both machines (CMake 3.28+, a C++23 compiler):

```sh
cmake -S . -B build/linux/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux/debug --parallel
ctest --test-dir build/linux/debug --output-on-failure --parallel 4
```

Google Test is fetched when tests are enabled; `-DBUILD_TESTING=OFF` builds only the applications.
The local machine also needs Neovim and Kitty. Keep `sync.lua` beside the built binaries in their
build directory: its configured absolute path is compiled into the daemon. Installation/relocation
is not yet provided. `JANISNVIM_TERMINAL` can name a different launcher executable which accepts
`nvim [arguments...]`, passes arguments literally, and stays alive until Neovim exits.

1. Generate a random secret, for example with `openssl rand -hex 32`. Set the **same**
   `JANISNVIM_TOKEN` environment variable locally and remotely (at least 32 characters).
   Set it through your preferred secret handling; do not commit it to the repository.
2. Locally, start `build/linux/debug/janisnvimdaemon` with that environment.
3. Connect with `ssh -o ExitOnForwardFailure=yes -R 7778:127.0.0.1:7777 SERVER`.
   Leave the forward bound to remote loopback. SSH encrypts the transport; the token
   also prevents other remote users with access to that listener from opening sessions.
4. On the remote host, `cd` to the workspace root and run
   `/path/to/janisnvim path/to/existing-file.cpp`.
5. Edit in the local Neovim window. `:write` updates the local cache immediately and
   queues synchronization. Notifications report remote acknowledgement or failure;
   `b:janisnvim_sync` is `local`, `pending`, `synced`, or `failed`.
   `:JanisNvimSync` retries the saved working copy without uploading unsaved buffer contents.

The remote command stays alive for the session. The daemon currently handles one editor
session at a time. On exit, Neovim waits up to 35 seconds for pending sync jobs; ordinary editing
and saves do not wait. A timeout means the remote outcome may be unknown, not that it failed
before writing. Closing or losing the SSH connection never deletes cached files.

### Implemented consistency and recovery policies

- Identity is the remote Linux machine ID plus UID, canonical workspace directory, and canonical
  file path. These are length-prefixed and hex-encoded into private cache directories, avoiding
  hash collisions. Cloned machines must have distinct machine IDs. Very long identities may exceed
  filesystem path limits and are rejected by the filesystem.
- The cache lives under `$XDG_CACHE_HOME/janisnvim`, or `~/.cache/janisnvim`. Its directories must
  be owned by the local user with mode `0700`. Each entry contains `working/<filename>`, `base`
  (last acknowledged remote contents), and a session lock. The cache path is printed by the remote
  command and daemon. There is no automatic cleanup.
- Existing regular files up to 16 MiB are supported. Remote symlinks are resolved on open; the target
  must be inside the canonical workspace. Later replacement of that target by a symlink is rejected.
  Creating files, renaming files, and `:saveas` do not synchronize a new remote path.
- A save compares the remote file's contents and inode/timestamps against the session baseline.
  A mismatch rejects the upload and preserves both versions. Remote changes are checked on save
  and reopen, not pushed into an open local buffer.
- Successful writes use a temporary file in the remote file's directory, `fsync`, rename and directory
  `fsync`. Ordinary permissions and owner/group are preserved. Files not owned by the remote user,
  hard-linked files, special permission bits, and extended attributes (including ACLs) are rejected
  instead of silently losing metadata. Filesystem durability/rename guarantees still apply.
- A persistent `.janisnvim-<filename>.lock` sidecar serializes cooperating sessions. **Unrelated
  writers do not take this lock:** an optimistic recheck cannot eliminate the final check/rename
  race. Avoid concurrent external writers during synchronization; this is not a transactional
  compare-and-swap against arbitrary filesystem writers.
- After disconnect, close the local window and run the remote command again. Dirty cached contents
  are resumed when the remote still matches `base`. If remote and local contents agree (for example,
  an acknowledgement was lost), the baseline is recovered. A clean cache refreshes from remote.
  If both sides changed, opening is rejected with the cache path: copy out the local edits, reconcile
  them manually against the remote file, and only then remove that cache entry or restore its working
  copy to its baseline before reopening. Never delete an entry containing unrecovered edits.

See [tests/README.md](tests/README.md) for test coverage and execution requirements.
