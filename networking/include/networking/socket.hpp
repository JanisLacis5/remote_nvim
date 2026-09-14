#ifndef SOCKET_H
#define SOCKET_H

#include <arpa/inet.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace networking {

enum class sock_type : std::uint8_t { bad, no_type, data, control };
using sock_type_underlying_t = std::underlying_type_t<sock_type>;

constexpr std::vector<std::byte> type_to_bytes(sock_type type) {
    std::vector<std::byte> bytes{sizeof(sock_type_underlying_t)};

    auto type_under = htonl(std::to_underlying(type));
    auto it = bytes.begin();
    while (type_under > 0) {
        *it++ = static_cast<std::byte>(type_under & 0xFF);
        type_under >>= 8;
    }

    return bytes;
};
constexpr sock_type bytes_to_type(std::vector<std::byte> bytes) {
    // expected that data in big endian is sent through the socket
    sock_type_underlying_t raw{};
    for (const auto byte : bytes) {
        raw = (raw << 8) | std::to_integer<sock_type_underlying_t>(byte);
    }

    return static_cast<networking::sock_type>(ntohl(raw));
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
    std::vector<std::byte> read(std::size_t min_cnt);
    std::size_t write(const std::vector<std::byte>& payload, std::size_t min_cnt);

private:
    int fd_{-1};
    int sock_family_;
    int sock_type_;
    int sock_protocol_;
};

}

#endif
