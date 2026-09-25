#include "os/process.hpp"

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
