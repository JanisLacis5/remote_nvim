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

#include "networking/listener.hpp"
#include "networking/socket.hpp"
#include "os/process.hpp"
#include "protocol/proto.hpp"
#include <arpa/inet.h>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/wait.h>

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

        proto::message_handler tmp_msg_hanlder{ std::move(maybe_sock.value()) };
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

    proto::message_handler ctrl_msg_handler{ std::move(ctrl_sock_optional.value()) };
    proto::message_handler data_msg_handler{ std::move(data_sock_optional.value()) };

    // get openui message
    auto message = ctrl_msg_handler.read_msg();
    auto* open = std::get_if<proto::open_ui_message>(&message);
    if (!open) {
        std::cerr << "unexpected type of messsage" << std::endl;
        return -1;
    }

    // todo: use networking::socket for this
    static constexpr std::string_view nvim_sock_path = "/tmp/janisnvim.sock";
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, nvim_sock_path.data(), sizeof(addr.sun_path) - 1);
    ::unlink(addr.sun_path); // remove stale socket from previous run

    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cerr << "error binding nvim socket, errno: " << std::strerror(errno) << std::endl;
        return -1;
    }

    if (::listen(fd, SOMAXCONN) == -1) {
        std::cerr << "error listening nvim socket, errno: " << std::strerror(errno) << std::endl;
        return -1;
    }
    process nvim_proc{ "kitty", "nvim", "--server", nvim_sock_path, "--remote-ui" };

    /* accept the local Nvim UI connection and read all bytes from
        the /tmp/janisnvim.sock and forward them to 127.0.0.1:7780 */

    // on close, send close ui message
}
