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
#include <array>

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

    std::array<std::byte, 1> payload{ networking::type_to_byte(networking::sock_type::control) };
    auto written = ctrl_sock.write_all(payload);
    if (written != payload.size())
        return -1;

    networking::socket data_sock{AF_INET, SOCK_STREAM};
    if (!data_sock.is_valid())
        return -1;
    if (!data_sock.connect(INADDR_LOOPBACK, 7778))
        return -1;

    payload[0] = networking::type_to_byte(networking::sock_type::data);
    written = data_sock.write_all(payload);
    if (written != payload.size())
        return -1;

    networking::socket nvim_sock{AF_INET, SOCK_STREAM};
    if (!nvim_sock.is_valid())
        return -1;
    if (!nvim_sock.connect(INADDR_LOOPBACK, 7778))
        return -1;

    auto message = proto::encode(proto::open_ui_message{
        .payload{
            .port = 7777,
            .cwd = "/janis/laics/ir/forsakais"
        }
    });
    written = ctrl_sock.write_all(message);
    if (written != message.size) {
        std::cerr << "written does not match the size" << std::endl;
        return -1;
    }

}
