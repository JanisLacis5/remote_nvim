#include "networking/socket.hpp"
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace networking
{

socket::socket(socket&& other) noexcept
    : sock_family_{ other.sock_family_ }
    , sock_type_{ other.sock_type_ }
    , sock_protocol_{ other.sock_protocol_ } {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }
}

socket& socket::operator=(socket&& other) noexcept {
    if (this != &other) {
        close();

        fd_ = std::exchange(other.fd_, -1);
        sock_family_ = other.sock_family_;
        sock_type_ = other.sock_type_;
        sock_protocol_ = other.sock_protocol_;
    }
    return *this;
}

socket::~socket() {
    close();
}

} // namespace networking
