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

#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h> 

#include <cstdlib>
#include <iostream>

#include "networking/socket.hpp"
#include "networking/listener.hpp"

int main() {
    networking::listener listener{};
    std::optional<networking::socket> ctrl_sock_optional;
    std::optional<networking::socket> data_sock_optional;

    for (auto i = 0; i < 2; ++i) {
        auto maybe_sock = listener.accept();
        if (!maybe_sock.has_value())
            return -1;

        networking::socket& sock = maybe_sock.value();

        auto response = sock.read(sizeof(networking::sock_type_underlying_t));
        if (response.size() != sizeof(networking::sock_type_underlying_t)) {
            std::cerr << "bad data, terminating" << std::endl;
            return -1;
        }

        auto type = networking::bytes_to_type(response);
        switch (type) {
        case networking::sock_type::data:
            data_sock_optional.emplace(std::move(sock));
            break;
        case networking::sock_type::control:
            ctrl_sock_optional.emplace(std::move(sock));
            break;
        default:
            std::cerr << "bad type" << std::endl;
            break;
        }
    }

    if (!ctrl_sock_optional.has_value() || !ctrl_sock_optional->is_valid() || 
            !data_sock_optional.has_value() || data_sock_optional->is_valid()) {
        std::cerr << "missing control or data connection" << std::endl;
        return -1;
    }

    auto& ctrl_sock = ctrl_sock_optional.value();
    auto& data_sock = data_sock_optional.value();
    // continue...
}
