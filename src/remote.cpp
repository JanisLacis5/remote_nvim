// main for the server app
// main() {
    // exec `nvim --listen 127.0.0.1:7777 --headless`
    // open ssh -R CLIENT_USER@CLIENT
    // send a packet that gives info about the current dir and whatever esle may be needed 
// }

#include <unistd.h>
#include <sys/wait.h>

#include <array>
#include <string>

int main() {
    auto pid = fork();
    std::array<std::string, 4> args{"nvim", "--listen", "127.0.0.1:7777", "--headless"};

    std::array<char*, args.size() + 1> cargs;
    for (std::size_t i = 0; i < args.size(); ++i) {
        cargs[i] = args[i].data();
    }
    cargs.back() = nullptr;

    if (pid == 0) {
        execvp(cargs[0], cargs.data());
    } else{
        waitpid(pid, NULL, 0);
    }
}
