#ifndef SOCKET_H
#define SOCKET_H

#include <arpa/inet.h>

#include <cstdint>
#include <utility>
#include <vector>
#include <span>

#include "protocol/proto.hpp"

namespace networking {

// todo: make a protocol lib and move sock_type and related stuff there
enum class sock_type : std::uint8_t { bad, no_type, data, control };
using sock_type_underlying_t = std::underlying_type_t<sock_type>;
static_assert(sizeof(sock_type_underlying_t) == 1);

constexpr std::byte type_to_byte(sock_type type) {
    return static_cast<std::byte>(std::to_underlying(type));
}
constexpr sock_type byte_to_type(std::byte byte) {
    auto integral = static_cast<sock_type_underlying_t>(byte);
    return static_cast<networking::sock_type>(integral);
};

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

    bool is_valid() const noexcept { return fd_ >= 0; };

    bool bind(std::uint32_t addr, std::uint16_t port);
    bool connect(std::uint32_t addr, std::uint16_t port);
    // todo: make read_all and read_exact functions
    std::vector<std::byte> read(std::size_t len);
    std::vector<std::byte> read_all();
    std::size_t write_all(const std::span<std::byte> payload);
    std::size_t write_all(const proto::encoded_message& message) {
        return write_all({ message.content, message.size });
    }

private:
    int fd_{-1};
    int sock_family_{};
    int sock_type_{};
    int sock_protocol_{};

    void close();
};

}

#endif
