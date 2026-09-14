#ifndef LISTENER_H
#define LISTENER_H

#include <optional>

#include "networking/socket.hpp"

namespace networking {

class listener
{
public:
    explicit listener();
    ~listener();

    listener(listener& other) = delete;
    listener operator=(listener& other) = delete;
    listener(listener&& other) noexcept;
    listener& operator=(listener&& other) noexcept;

    std::optional<networking::socket> accept();

private:
    static constexpr int LISTEN_BACKLOG = 10;

    int fd_{-1};
};

}

#endif
