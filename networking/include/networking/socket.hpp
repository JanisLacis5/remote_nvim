#ifndef SOCKET_H
#define SOCKET_H

#include <arpa/inet.h>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace networking
{

class socket
{
public:
    explicit socket(int family, int type, int protocol = 0);
    explicit socket(int fd);
    ~socket();

    socket(socket& other) = delete;
    socket operator=(socket& other) = delete;
    socket(socket&& other) noexcept;
    socket& operator=(socket&& other) noexcept;

    int native_handle() const noexcept { return fd_; }
    bool is_valid() const noexcept { return fd_ >= 0; }

    bool bind(std::uint32_t addr, std::uint16_t port);
    bool connect(std::uint32_t addr, std::uint16_t port);
    std::vector<std::byte> read(std::size_t len);
    std::size_t write_all(std::span<const std::byte> payload);

private:
    int fd_{ -1 };
    int sock_family_{};
    int sock_type_{};
    int sock_protocol_{};

    void close();
};

} // namespace networking

#endif
