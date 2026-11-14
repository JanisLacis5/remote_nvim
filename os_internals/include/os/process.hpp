#ifndef PROCESS_H
#define PROCESS_H

#include <filesystem>
#include <initializer_list>
#include <string_view>

class process
{
public:
    process(std::initializer_list<std::string_view> raw_args);
    ~process();

    process(const process&) = delete;
    process& operator=(const process&) = delete;
    process(process&& other) noexcept;
    process& operator=(process&& other) noexcept;

    std::filesystem::path cwd();
    bool running();
    int wait();

private:
    int pid_{ -1 };
    int status_{ -1 };
};

#endif
