#ifndef PROCESS_H
#define PROCESS_H

#include <filesystem>
#include <initializer_list>
#include <string_view>

// todo: add function to exectue after exec that is passed by the caller
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

private:
    int pid_{ -1 };
};

#endif
