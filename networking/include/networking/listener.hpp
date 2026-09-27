#ifndef LISTENER_H
#define LISTENER_H

#include "networking/socket.hpp"
#include <optional>

namespace networking
{

class listener
{
public:
    // Port zero asks the OS to choose a free loopback port.
    explicit listener(std::uint16_t port = 7777);
    ~listener();

    listener(listener& other) = delete;
    listener operator=(listener& other) = delete;
    listener(listener&& other) noexcept;
    listener& operator=(listener&& other) noexcept;

    bool is_valid() const noexcept { return fd_ >= 0; }
    std::uint16_t port() const noexcept;
    std::optional<networking::socket> accept();

private:
    static constexpr int LISTEN_BACKLOG = 10;

    int fd_{ -1 };

    void close();
};

} // namespace networking

#endif
