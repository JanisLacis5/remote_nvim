#include "networking/socket.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <cerrno>
#include <utility>
#include <system_error>

namespace networking {

socket::socket(int family, int type, int protocol)
    : sock_family_{family}, sock_type_{type}, sock_protocol_{protocol}
{
    fd_ = ::socket(family, type, protocol);
    if (fd_ < 0) [[ unlikely ]]
        throw std::system_error(errno, std::system_category(), "opening socket failed");
};

socket::socket(int fd)
    : fd_{fd}
{
    socklen_t len{};
    auto err = ::getsockopt(fd, SOL_SOCKET, SO_DOMAIN, &sock_family_, &len);
    if (err) [[ unlikely ]] {
        ::close(fd);
        throw std::system_error(errno, std::system_category(), "socket::socket(int fd): getsockopt::SO_DOMAIN");
    }

    err = ::getsockopt(fd, SOL_SOCKET, SO_TYPE, &sock_type_, &len);
    if (err) [[ unlikely ]] {
        ::close(fd);
        throw std::system_error(errno, std::system_category(), "socket::socket(int fd): getsockopt::SO_TYPE");
    }

    err = ::getsockopt(fd, SOL_SOCKET, SO_PROTOCOL, &sock_protocol_, &len);
    if (err) [[ unlikely ]] {
        ::close(fd);
        throw std::system_error(errno, std::system_category(), "socket::socket(int fd): getsockopt::SO_PROTOCOL");
    }
};

bool socket::bind(std::uint32_t addr, std::uint16_t port) {
    sockaddr_in addr_in{};
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = htonl(addr);
    addr_in.sin_port = htons(port);

    return ::bind(fd_, (sockaddr *)&addr_in, sizeof(addr_in)) != -1;
}

std::vector<std::byte> socket::read(std::size_t min_cnt) {
    return {};
}

std::size_t socket::write(const std::vector<std::byte>& payload, std::size_t min_cnt) {
    return {};
}

socket::~socket() {
    if (fd_ >= 0)
        ::close(fd_);
}

socket::socket(socket&& other) noexcept 
    : fd_{std::exchange(other.fd_, -1)},
      sock_family_{other.sock_family_},
      sock_type_{other.sock_type_},
      sock_protocol_{other.sock_protocol_}
{}

socket& socket::operator=(socket&& other) noexcept {
    fd_ = std::exchange(other.fd_, -1);
    sock_family_ = other.sock_family_;
    sock_type_ = other.sock_type_;
    sock_protocol_ = other.sock_protocol_;
    return *this;
}    

}
