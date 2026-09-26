#include "os/process.hpp"

#include <filesystem>
#include <sys/wait.h>

#include <vector>

process::process(std::initializer_list<std::string> raw_args) {
    std::vector<std::string> args{raw_args};

    std::vector<char*> cargs{args.size() + 1, nullptr};
    for (std::size_t i = 0; i < args.size(); ++i)
        cargs[i] = args[i].data();

    pid_ = fork();
    if (pid_ == 0)
        execvp(cargs[0], cargs.data());
}

process::~process() {
    // see `man waitpid` to replace NULL with status, process the status afterwards
    waitpid(pid_, NULL, 0);
}

std::filesystem::path process::cwd() {
    std::filesystem::path path = std::filesystem::current_path().root_path() / "proc" / std::to_string(pid_) / "cwd";
    return std::filesystem::read_symlink(path);
}
