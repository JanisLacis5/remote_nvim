#ifndef TEST_PROCESS_HPP
#define TEST_PROCESS_HPP
#include "os/process.h"
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

class process {
public:
    process(std::initializer_list<std::string_view> args) {
        std::vector<std::string> storage;
        for (auto arg : args) storage.emplace_back(arg);
        std::vector<char*> argv;
        for (auto& arg : storage) argv.push_back(arg.data());
        argv.push_back(nullptr);

        if (!process_spawn(&child_, argv.data())) {
            if (errno == EINVAL) throw std::invalid_argument("empty executable");
            throw std::system_error(errno, std::generic_category(), "fork");
        }
    }
    ~process() { process_wait(&child_); }
    process(const process&) = delete;
    process& operator=(const process&) = delete;
    process(process&& other) noexcept : child_(other.child_) { other.child_.pid = -1; }
    process& operator=(process&& other) noexcept {
        if (this != &other) {
            process_wait(&child_);
            child_ = other.child_;
            other.child_.pid = -1;
        }
        return *this;
    }

    bool running() { return process_running(&child_); }
    int wait() { return process_wait(&child_); }
    std::filesystem::path cwd() {
        char* path = process_cwd(&child_);
        if (!path) throw std::system_error(errno, std::generic_category());
        std::filesystem::path result(path);
        free(path);
        return result;
    }
private:
    child_process child_{ -1, -1 };
};
#endif
