```text
+------------------------------------------------------------------+
|                              REMOTE                              |
|                                                                  |
|  1) tmux session & work happens here                             |
|                                                                  |
|  2) `janisnvim` is entered                                       |
|                                                                  |
|       -> new nvim server is created at                           |
|          127.0.0.1:7777                                          |
|          (ssh -L needed)                                         |
|                                                                  |
|       -> ssh -R is opened to the client                          |
|                                                                  |
|       -> OPEN_UI message is sent to the client                   |
|                                                                  |
+------------------------------------------------------------------+


+------------------------------------------------------------------+
|                         ACTUAL CLIENT                            |
|                                                                  |
|  1) I sit at this machine                                        |
|                                                                  |
|  2) When ssh -R connects, systemd starts a daemon                |
|                                                                  |
|       -> daemon opens a new terminal (in tmux session)           |
|                                                                  |
|       -> terminal starts Neovim UI connected to the              |
|          Neovim server on the remote host                        |
|                                                                  |
|                                                                  |
|                    +----------------------+                      |
|                    |     Local Nvim UI    |                      |
|                    +----------+-----------+                      |
|                               |                                  |
|                               | /tmp/janisnvim.sock              |
|                               v                                  |
|                    +----------------------+                      |
|                    |   janisnvim daemon   |                      |
|                    | / message processor  |                      |
|                    +----------+-----------+                      |
|                               |                                  |
|                               | SSH / forwarded RPC              |
|                               v                                  |
+-------------------------------+----------------------------------+
                                |
                                v
                       +------------------+
                       | Remote Nvim      |
                       | server           |
                       +------------------+
```

Daemon behavior:

    local Nvim UI
          |
          v
    /tmp/janisnvim.sock
          |
          v
    janisnvim daemon
          |
          +----> forwards UI/input messages ----> remote server
          |
          <---- receives redraw messages <-------+

    daemon renders / forwards what the remote Nvim server sends
