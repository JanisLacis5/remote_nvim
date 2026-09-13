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

#include <stdio.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 

#include <cstdlib>
#include <iostream>
#include <utility>

// todo: make this available for remote as well
enum class sock_type { bad, data, control };

std::pair<int, sock_type> wait_sock(int listener_fd) {
    auto fd = accept(listener_fd, nullptr, nullptr);
    if (fd < 0) {
        perror("accept");
        return {-1, sock_type::bad};
    }

    // todo: wait for the message to arrive identifying what socket this is (data or control)
    return {fd, sock_type::data};
}

bool set_fd(int& to_set, int new_val) {
    if (to_set >= 0) {
        std::cerr << "fd already initialized" << std::endl;
        return false;
    }

    to_set = new_val;
    return true;
}

int main() {
    auto listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_fd == -1) {
        perror("socket");
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(7777);

    if (bind(listener_fd, (sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listener_fd);
        return -1;
    }

    if (listen(listener_fd, 10) == -1) {
        perror("listen");
        close(listener_fd);
        return -1;
    }

    int ctrl_fd = -1;
    int data_fd = -1;
    for (auto i = 0; i < 2; ++i) {
        auto [fd, type] = wait_sock(listener_fd);
        switch (type) {
        case sock_type::data:
            set_fd(data_fd, fd);
            break;
        case sock_type::control:
            set_fd(ctrl_fd, fd);
            break;
        default:
            std::cerr << "bad type" << std::endl;
            break;
        }
    }

    if (ctrl_fd < 0 || data_fd < 0) {
        std::cerr << "missing control or data connection" << std::endl;
        return -1;
    }
}
