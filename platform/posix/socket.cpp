#include "networking/socket.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <cerrno>

namespace {
    
sockaddr_in create_sockaddr_in(int family, std::uint32_t addr, std::uint16_t port) {
    sockaddr_in addr_in{};
    addr_in.sin_family = family;
    addr_in.sin_addr.s_addr = htonl(addr);
    addr_in.sin_port = htons(port);
    return addr_in;
}

}

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

    auto addr_in = create_sockaddr_in(sock_family_, addr, port);
    return ::bind(fd_, (sockaddr *)&addr_in, sizeof(addr_in)) != -1;
}

bool socket::connect(std::uint32_t addr, std::uint16_t port) {
    if (!is_valid())
        return false;

    auto addr_in = create_sockaddr_in(sock_family_, addr, port);
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


void socket::close() {
    if (is_valid()) {
        ::close(fd_);
        fd_ = -1;
    }
}

}
