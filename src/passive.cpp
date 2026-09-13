// janisnvimdaemon - runs on the client
//
// POC requirement:
// The user connects to the remote machine with:
//
//   ssh -R 7778:127.0.0.1:7777 SERVER
//
// This makes remote 127.0.0.1:7778 forward to the local daemon
// listening on 127.0.0.1:7777.
//
// main() {
//     // listen on 127.0.0.1:7777 for OPEN_UI requests
//
//     // receive OPEN_UI message containing:
//     //   - remote host
//     //   - remote Nvim RPC port
//     //   - cwd / session metadata as needed
//
//     // establish a connection/tunnel to the remote Nvim server where
//     // rpc commands will be forwarded from socket to this address:port
//
//     // create and listen on /tmp/janisnvim.sock
//
//     // run:
//     //   nvim --server /tmp/janisnvim.sock --remote-ui
//
//     // accept the local Nvim UI connection and read all bytes from
//     // the /tmp/janisnvim.sock and forward them to 127.0.0.1:7780
//
//     // proxy both directions:
//     //   local Nvim UI -> process/intercept -> remote Nvim
//     //   remote Nvim    -> process/intercept -> local Nvim UI
// }

int main() {}
