#include "networking/listener.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <iostream>
#include <utility>
#include <cerrno>
#include <cstring>

namespace networking {

listener::listener(std::uint16_t port) {
    fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd_ < 0) [[ unlikely ]] {
        std::cerr << "socket err, errno: " << std::strerror(errno) << std::endl;
        return;
    }

    int yes = 1;
    auto setsockopt_err = ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (setsockopt_err) [[ unlikely ]] {
        std::cerr << "setsockopt err, errno: " << std::strerror(errno) << std::endl;
        close();
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    auto bind_err = ::bind(fd_, (sockaddr *)&addr, sizeof(addr));
    if (bind_err) [[ unlikely ]] {
        std::cerr << "bind err, errno: " << std::strerror(errno) << std::endl;
        close();
        return;
    }

    auto listen_err = ::listen(fd_, LISTEN_BACKLOG);
    if (listen_err) [[ unlikely ]] {
        std::cerr << "listener err, errno: " << std::strerror(errno) << std::endl;
        close();
        return;
    }
}

std::optional<networking::socket> listener::accept() {
    if (!is_valid())
        return {};

    auto fd = ::accept4(fd_, nullptr, nullptr, SOCK_CLOEXEC);
    if (fd < 0)
        return {};

    return networking::socket{fd};
}

std::uint16_t listener::port() const noexcept {
    sockaddr_in addr{};
    socklen_t size = sizeof(addr);
    if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &size) < 0)
        return 0;
    return ntohs(addr.sin_port);
}

listener::~listener() {
    close();
}

listener::listener(listener&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }
}

listener& listener::operator=(listener&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }

    return *this;
}    

void listener::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

}
