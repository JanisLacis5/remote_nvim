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
#include "protocol/proto.hpp"

int main() {
    networking::listener listener{};
    if (!listener.is_valid()) {
        std::cerr << "listener invalid, terminating" << std::endl;
        return -1;
    }

    std::optional<networking::socket> ctrl_sock_optional;
    std::optional<networking::socket> data_sock_optional;

    for (auto i = 0; i < 2; ++i) {
        auto maybe_sock = listener.accept();
        if (!maybe_sock.has_value())
            return -1;

        proto::message_handler tmp_msg_hanlder{std::move(maybe_sock.value())};
        auto response = tmp_msg_hanlder.read_msg();
        auto* type_msg = std::get_if<proto::tell_sck_type_message>(&response);
        if (!type_msg)
            return -1;

        switch (type_msg->payload.type) {
        case networking::sock_type::data:
            data_sock_optional.emplace(std::move(tmp_msg_hanlder.release_socket()));
            break;
        case networking::sock_type::control:
            ctrl_sock_optional.emplace(std::move(tmp_msg_hanlder.release_socket()));
            break;
        default:
            std::cerr << "bad type" << std::endl;
            break;
        }
    }

    proto::message_handler ctrl_msg_handler{std::move(ctrl_sock_optional.value())};
    proto::message_handler data_msg_handler{std::move(data_sock_optional.value())};

    // get openui message
    auto message = ctrl_msg_handler.read_msg();
    if (auto* open = std::get_if<proto::open_ui_message>(&message)) {
        std::cout << open->payload.cwd << std::endl;
    }

    // open nvim ui and link it to /tmp/janisnvim.sock

    /* accept the local Nvim UI connection and read all bytes from
        the /tmp/janisnvim.sock and forward them to 127.0.0.1:7780 */
}
