#ifndef TEST_LISTENER_HPP
#define TEST_LISTENER_HPP
#include "networking/listener.h"
#include "networking/socket.hpp"
#include <optional>
#include <unistd.h>
namespace networking {
class listener {
public:
    explicit listener(uint16_t port = 7777) : fd_(net_listen(port)) {}
    ~listener() { if (fd_ >= 0) ::close(fd_); }
    listener(const listener&) = delete;
    listener& operator=(const listener&) = delete;
    listener(listener&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    listener& operator=(listener&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) ::close(fd_);
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    bool is_valid() const { return fd_ >= 0; }
    uint16_t port() const { return net_listener_port(fd_); }
    std::optional<socket> accept() {
        auto value = net_accept(fd_);
        if (value.fd < 0) return {};
        return socket(value.fd);
    }
private:
    int fd_;
};
}
#endif
