#include "networking/listener.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <utility>
#include <cerrno>
#include <system_error>

namespace networking {

listener::listener() {
    fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd_ < 0) [[ unlikely ]]
        throw std::system_error(errno, std::system_category(), "listener::listener() socket");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(7777);

    auto bind_err = ::bind(fd_, (sockaddr *)&addr, sizeof(addr));
    if (bind_err) [[ unlikely ]] {
        ::close(fd_);
        throw std::system_error(errno, std::system_category(), "listener::listener() bind");
    }

    auto listen_err = ::listen(fd_, LISTEN_BACKLOG);
    if (listen_err) [[ unlikely ]] {
        ::close(fd_);
        throw std::system_error(errno, std::system_category(), "listener::listener() listen");
    }
}

std::optional<networking::socket> listener::accept() {
    auto fd = ::accept4(fd_, nullptr, nullptr, SOCK_CLOEXEC);
    if (fd < 0) {
        perror("accept");
        return {};
    }

    return networking::socket{fd};
}

listener::~listener() {
    if (fd_ >= 0)
        ::close(fd_);
}

listener::listener(listener&& other) noexcept 
    : fd_{std::exchange(other.fd_, -1)}
{}

listener& listener::operator=(listener&& other) noexcept {
    fd_ = std::exchange(other.fd_, -1);
    return *this;
}    

}
