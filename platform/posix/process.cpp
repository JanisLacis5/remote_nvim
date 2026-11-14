#include "os/process.hpp"
#include <cerrno>
#include <filesystem>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

process::process(std::initializer_list<std::string_view> raw_args) {
    if (raw_args.size() == 0 || raw_args.begin()->empty())
        throw std::invalid_argument("empty executable");

    std::vector<std::string> args;
    args.reserve(raw_args.size());
    for (const auto& arg : raw_args)
        args.emplace_back(arg.begin(), arg.end());

    std::vector<char*> cargs(args.size() + 1, nullptr);
    for (std::size_t i = 0; i < args.size(); ++i)
        cargs[i] = args[i].data();

    pid_ = fork();
    if (pid_ < 0)
        throw std::system_error(errno, std::generic_category(), "fork");

    if (pid_ == 0) {
        execvp(cargs[0], cargs.data());
        ::_exit(127);
    }
}

process::~process() {
    wait();
}

process::process(process&& other) noexcept
    : pid_{ std::exchange(other.pid_, -1) }
    , status_{ other.status_ } {}

process& process::operator=(process&& other) noexcept {
    if (this != &other) {
        wait();
        pid_ = std::exchange(other.pid_, -1);
        status_ = other.status_;
    }

    return *this;
}

std::filesystem::path process::cwd() {
    std::filesystem::path path = std::filesystem::current_path().root_path() / "proc" / std::to_string(pid_) / "cwd";
    return std::filesystem::read_symlink(path);
}

bool process::running() {
    if (pid_ < 0)
        return false;

    int status;
    int result;
    do {
        result = ::waitpid(pid_, &status, WNOHANG);
    } while (result < 0 && errno == EINTR);
    if (result == 0)
        return true;

    status_ = result > 0 && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    pid_ = -1;

    return false;
}

int process::wait() {
    if (pid_ >= 0) {
        int status;
        int result;
        do {
            result = ::waitpid(pid_, &status, 0);
        } while (result < 0 && errno == EINTR);

        status_ = result > 0 && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        pid_ = -1;
    }

    return status_;
}
