#include "networking/listener.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <utility>
#include <cerrno>

namespace networking {

listener::listener() {
    fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd_ < 0) [[ unlikely ]]
        return;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(7777);

    auto bind_err = ::bind(fd_, (sockaddr *)&addr, sizeof(addr));
    if (bind_err) [[ unlikely ]] {
        close();
        return;
    }

    auto listen_err = ::listen(fd_, LISTEN_BACKLOG);
    if (listen_err) [[ unlikely ]] {
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
