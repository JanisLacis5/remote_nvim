# Running tests

```sh
cmake -S . -B build/linux/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux/debug --parallel
ctest --test-dir build/linux/debug --output-on-failure --parallel 4
```

Use `-DSANITIZER=address` for AddressSanitizer. Google Test 1.17.0 is fetched with a
pinned checksum; offline builds can set `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` to an
existing source directory. `-DBUILD_TESTING=OFF` omits tests.

- `c_api_test.cpp`: C ownership cleanup on truncated frames and failed reads, text validation,
  and repeated process polling/waits.
- `protocol_test.cpp`: independent wire fixtures, field bounds and malformed headers.
- `message_handler_test.cpp`: fragmentation, stream boundaries, truncated transfers,
  large and empty files, and disconnect handling.
- `workspace_test.cpp`: collision-free identities, path validation, private cache directories,
  safe replacement, permission preservation, metadata/content conflicts, symlink/hardlink
  rejection, dirty-cache recovery and lost acknowledgements.
- `socket_test.cpp`, `listener_test.cpp`: descriptor ownership and loopback/socketpair I/O.
- `process_test.cpp`, `syscall_test.cpp`: process lifecycle, failed exec/fork, interrupted waits,
  partial I/O and EINTR. Syscall tests wrap read/send/fork/waitpid in their executable only.
- `file_workflow_test.py`: both binaries plus real headless Neovim, saved-file sync,
  immediate editor exit, a delayed link, interrupted synchronization, remote conflicts and reconnect with dirty cached contents. A loopback
  TCP relay stands in for `ssh -R`; this does not validate SSH setup or the graphical terminal.

Tests require Linux local sockets. `FileWorkflow` is added only when Python 3 and Neovim
are available, runs serially, and requires free loopback ports 7777/7778. Other tests use
socketpairs or ephemeral ports. Run without a production daemon on those ports. A socket
sandbox requires granting the test process local socket access.

The C++ tests retain their existing assertions through adapters in `compat/`. These adapters
only translate values and ownership; socket I/O, framing, child management and filesystem
operations execute in the C production libraries. Production builds do not include the adapters.

CTest labels are `unit` and `integration`. Individual C++ tests have a 15-second timeout;
the complete file workflow has a 90-second timeout. Use `-R '^Workspace\.'` or
`-R '^FileWorkflow$'` for targeted runs.
