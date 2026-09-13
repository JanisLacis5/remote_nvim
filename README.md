# Workflow
```text
+------------------------------------------------------------------+
|                              REMOTE                              |
|                                                                  |
|  1) User connects with:                                          |
|                                                                  |
|       ssh -R 7778:127.0.0.1:7777 SERVER                         |
|                                                                  |
|     This creates a remote listener at 127.0.0.1:7778 which       |
|     forwards back to the local janisnvimdaemon on :7777.         |
|                                                                  |
|  2) User enters `janisnvim` inside remote tmux                   |
|                                                                  |
|       -> start remote headless Nvim server                       |
|          127.0.0.1:7780                                          |
|                                                                  |
|       -> open CONTROL connection to 127.0.0.1:7778               |
|          and identify it as CONTROL                              |
|                                                                  |
|       -> send OPEN_UI message over control connection            |
|                                                                  |
|       -> open DATA connection to 127.0.0.1:7778                  |
|          and identify it as NVIM_DATA                            |
|                                                                  |
|       -> connect locally to remote Nvim at                       |
|          127.0.0.1:7780                                          |
|                                                                  |
|       -> proxy: DATA connection <-> remote Nvim                  |
|                                                                  |
+------------------------------------------------------------------+


                         one SSH connection
                  multiplexing multiple channels
                                |
                                v

+------------------------------------------------------------------+
|                         ACTUAL CLIENT                            |
|                                                                  |
|  1) janisnvimdaemon is running                                  |
|     and listening on 127.0.0.1:7777                             |
|                                                                  |
|  2) ssh -R forwards remote connections here                     |
|                                                                  |
|       -> daemon accept()s CONTROL connection                     |
|                                                                  |
|       -> daemon receives OPEN_UI                                 |
|                                                                  |
|       -> daemon accept()s NVIM_DATA connection                   |
|                                                                  |
|       -> daemon creates /tmp/janisnvim.sock                      |
|                                                                  |
|       -> daemon starts:                                          |
|                                                                  |
|          nvim --server /tmp/janisnvim.sock --remote-ui           |
|                                                                  |
|       -> local Nvim UI connects to /tmp/janisnvim.sock           |
|                                                                  |
|                                                                  |
|                    +----------------------+                      |
|                    |     Local Nvim UI    |                      |
|                    +----------+-----------+                      |
|                               |                                  |
|                               | Unix socket                      |
|                               | /tmp/janisnvim.sock              |
|                               v                                  |
|                    +----------------------+                      |
|                    |  janisnvimdaemon     |                      |
|                    |  message processor   |                      |
|                    +-----+----------+-----+                      |
|                          |          |                            |
|                    CONTROL fd   NVIM_DATA fd                     |
|                          |          |                            |
+--------------------------|----------|----------------------------+
                           |          |
                           |          |
                           v          v
                    SSH channel 1   SSH channel 2
                           |          |
                           +----+-----+
                                |
                                v
                         remote janisnvim
                                |
                                v
                     127.0.0.1:7780 Nvim
```

# Daemon data path:
```
local Nvim UI
      |
      v
/tmp/janisnvim.sock
      |
      v
janisnvimdaemon
      |
      | NVIM_DATA connection
      v
SSH reverse-forwarded channel
      |
      v
remote janisnvim
      |
      v
remote Nvim 127.0.0.1:7780
```

# Control path:
```
remote janisnvim
      |
      | CONTROL connection
      v
SSH reverse-forwarded channel
      |
      v
janisnvimdaemon
      |
      v
OPEN_UI / future control messages
```

# Dependencies:
### Build:
- CMake
- GCC/Clang with C++26 support

### Runtime (only linux supported):
- OpenSSH
- Neovim
- tmux (for intended workflow)

