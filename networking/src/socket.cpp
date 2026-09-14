#include "networking/socket.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <cerrno>
#include <utility>

namespace networking {

socket::socket(int family, int type, int protocol)
    : sock_family_{family}, sock_type_{type}, sock_protocol_{protocol}
{
    fd_ = ::socket(family, type, protocol);
};

socket::socket(int fd)
    : fd_{fd}
{
    if (fd_ < 0) [[ unlikely ]]
        return;

    socklen_t len = sizeof(sock_family_);
    auto err = ::getsockopt(fd, SOL_SOCKET, SO_DOMAIN, &sock_family_, &len);
    if (err) [[ unlikely ]] {
        close();
        return;
    }

    len = sizeof(sock_type_);
    err = ::getsockopt(fd, SOL_SOCKET, SO_TYPE, &sock_type_, &len);
    if (err) [[ unlikely ]] {
        close();
        return;
    }

    len = sizeof(sock_protocol_);
    err = ::getsockopt(fd, SOL_SOCKET, SO_PROTOCOL, &sock_protocol_, &len);
    if (err) [[ unlikely ]] {
        close();
        return;
    }
};

bool socket::bind(std::uint32_t addr, std::uint16_t port) {
    if (!is_valid())
        return false;

    auto addr_in = create_sockaddr_in(addr, port);
    return ::bind(fd_, (sockaddr *)&addr_in, sizeof(addr_in)) != -1;
}

bool socket::connect(std::uint32_t addr, std::uint16_t port) {
    if (!is_valid())
        return false;

    auto addr_in = create_sockaddr_in(addr, port);
    return ::connect(fd_, (sockaddr* )&addr_in, sizeof(addr_in)) != -1;
}

std::vector<std::byte> socket::read(std::size_t min_cnt) {
    if (!is_valid())
        return {};

    return {};
}

std::size_t socket::write_all(const std::vector<std::byte>& payload) {
    if (!is_valid())
        return {};
    return {};
}

sockaddr_in socket::create_sockaddr_in(std::uint32_t addr, std::uint16_t port) {
    sockaddr_in addr_in{};
    addr_in.sin_family = sock_family_;
    addr_in.sin_addr.s_addr = htonl(addr);
    addr_in.sin_port = htons(port);
    return addr_in;
}

socket::~socket() {
    close();
}

socket::socket(socket&& other) noexcept 
    : sock_family_{other.sock_family_},
      sock_type_{other.sock_type_},
      sock_protocol_{other.sock_protocol_}
{
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }
}

socket& socket::operator=(socket&& other) noexcept {
    if (this != &other) {
        close();

        fd_ = std::exchange(other.fd_, fd_);
        sock_family_ = other.sock_family_;
        sock_type_ = other.sock_type_;
        sock_protocol_ = other.sock_protocol_;
    }
    return *this;
}    

void socket::close() {
    if (is_valid()) {
        ::close(fd_);
        fd_ = -1;
    }
}

}
