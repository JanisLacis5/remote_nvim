#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <initializer_list>

// todo: add function to exectue after exec that is passed by the caller
class process
{
public:
    process(std::initializer_list<std::string> raw_args);
    ~process();

private:
    int pid_{};
};

#endif
