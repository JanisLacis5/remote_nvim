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

int open_sock(const std::string& ip, uint16_t port) {
    auto fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip.data(), &addr.sin_addr) <= 0) {
        perror("inet_pton");
        return -1;
    } 

    if (connect(fd, (sockaddr* )&addr, sizeof(addr)) < 0) {
        perror("connect");
        return -1;
    }

    return fd;
}

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

    // todo: send a message that this is for control (open_ui and stuff)
    auto ctrl_fd = open_sock("127.0.0.1", 7778);
    if (ctrl_fd == -1)
        return -1;

    // todo: send a message that this is for data
    auto data_fd = open_sock("127.0.0.1", 7778);
    if (ctrl_fd == -1)
        return -1;

    auto nvim_fd = open_sock("127.0.0.1", 7780);
    if (nvim_fd == -1)
        return -1;

    // see `man waitpid` to replace NULL with status, process status afterwards
    waitpid(nvim_server_pid, NULL, 0);
}
