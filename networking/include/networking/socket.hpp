#ifndef TEST_SOCKET_HPP
#define TEST_SOCKET_HPP
#include "networking/socket.h"
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace networking {
class socket {
public:
    explicit socket(int family, int type, int protocol = 0) : value_(net_socket_open(family, type, protocol)) {}
    explicit socket(int fd) : value_(net_socket_adopt(fd)) {}
    ~socket() { net_socket_close(&value_); }
    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;
    socket(socket&& other) noexcept : value_(other.value_) { other.value_.fd = -1; }
    socket& operator=(socket&& other) noexcept {
        if (this != &other) {
            net_socket_close(&value_);
            value_ = other.value_;
            other.value_.fd = -1;
        }
        return *this;
    }

    int native_handle() const { return value_.fd; }
    bool is_valid() const { return value_.fd >= 0; }
    bool bind(uint32_t host, uint16_t port) { return net_socket_bind(&value_, host, port); }
    bool connect(uint32_t host, uint16_t port) { return net_socket_connect(&value_, host, port); }
    std::vector<std::byte> read(size_t size) {
        std::vector<std::byte> result(size);
        if (!net_read(value_.fd, result.data(), size))
            return {};
        return result;
    }
    size_t write_all(std::span<const std::byte> data) { return net_write(value_.fd, data.data(), data.size()); }
private:
    net_socket value_;
};
}
#endif
