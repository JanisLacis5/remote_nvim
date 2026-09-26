#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <filesystem>
#include <initializer_list>

// todo: add function to exectue after exec that is passed by the caller
class process
{
public:
    process(std::initializer_list<std::string> raw_args);
    ~process();

    std::filesystem::path cwd();

private:
    int pid_{};
};

#endif
