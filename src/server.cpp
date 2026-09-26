// janisnvim - runs on the remote machine
//
// main() {
//     // start:
//     //   nvim --headless --listen 127.0.0.1:7780
//
//     // connect to 127.0.0.1:7778 (forwarded by the user's ssh -R to janisnvimdaemon)
//     //   // open ctrl_fd for custom protocol messages
//     //   // open data_fd for neovim rpc messages that can be forwarded to nvim_fd
//     //   // with no edits
//
//     // send OPEN_UI containing (to ctrl_fd):
//     //   - Nvim RPC endpoint
//     //   - cwd
//     //   - other session metadata if needed
//
//     // wait for session / child processes as appropriate
// }

#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 

#include <cstdlib>
#include <iostream>
#include <utility>

#include "networking/socket.hpp"
#include "protocol/proto.hpp"
#include "os/process.hpp"

int main() {
    process nvim_proc{"nvim", "--listen", "127.0.0.1:7780", "--headless"};

    networking::socket ctrl_sock{AF_INET, SOCK_STREAM};
    if (!ctrl_sock.is_valid())
        return -1;
    if (!ctrl_sock.connect(INADDR_LOOPBACK, 7778))
        return -1;
    proto::message_handler ctrl_msg_handler{std::move(ctrl_sock)};

    auto ctrl_message = proto::tell_sck_type_message{
        .payload{ .type = networking::sock_type::control }
    };
    if (!ctrl_msg_handler.write_msg(ctrl_message))
        return -1;

    networking::socket data_sock{AF_INET, SOCK_STREAM};
    if (!data_sock.is_valid())
        return -1;
    if (!data_sock.connect(INADDR_LOOPBACK, 7778))
        return -1;
    proto::message_handler data_msg_handler{std::move(data_sock)};

    auto data_message = proto::tell_sck_type_message{
        .payload{ .type = networking::sock_type::data }
    };
    if (!data_msg_handler.write_msg(data_message))
        return -1;

    networking::socket nvim_sock{AF_INET, SOCK_STREAM};
    if (!nvim_sock.is_valid())
        return -1;
    if (!nvim_sock.connect(INADDR_LOOPBACK, 7778))
        return -1;
    proto::message_handler nvim_msg_handler{std::move(nvim_sock)};

    auto message = proto::open_ui_message{
        .payload{
            .port = 7777,
            .cwd = nvim_proc.cwd()
        }
    };
    if (!ctrl_msg_handler.write_msg(message)) {
        std::cerr << "write failed" << std::endl;
        return -1;
    }

}
