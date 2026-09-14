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
#include <array>
#include <string>

#include "networking/socket.hpp"

int main() {
    auto nvim_server_pid = fork();
    std::array<std::string, 4> nvim_server_args{"nvim", "--listen", "127.0.0.1:7780", "--headless"};

    std::array<char*, nvim_server_args.size() + 1> nvim_server_cargs;
    for (std::size_t i = 0; i < nvim_server_args.size(); ++i) {
        nvim_server_cargs[i] = nvim_server_args[i].data();
    }
    nvim_server_cargs.back() = nullptr;

    if (nvim_server_pid == 0) {
        execvp(nvim_server_cargs[0], nvim_server_cargs.data());
        return 0;
    }

    networking::socket ctrl_sock{AF_INET, SOCK_STREAM};
    if (!ctrl_sock.is_valid())
        return -1;
    if (!ctrl_sock.connect(INADDR_LOOPBACK, 7778))
        return -1;
    ctrl_sock.write_all({ networking::type_to_byte(networking::sock_type::control) });

    networking::socket data_sock{AF_INET, SOCK_STREAM};
    if (!data_sock.is_valid())
        return -1;
    if (!data_sock.connect(INADDR_LOOPBACK, 7778))
        return -1;
    data_sock.write_all({ networking::type_to_byte(networking::sock_type::data) });

    networking::socket nvim_sock{AF_INET, SOCK_STREAM};
    if (!nvim_sock.is_valid())
        return -1;
    if (!nvim_sock.connect(INADDR_LOOPBACK, 7778))
        return -1;

    // see `man waitpid` to replace NULL with status, process status afterwards
    waitpid(nvim_server_pid, NULL, 0);
}
