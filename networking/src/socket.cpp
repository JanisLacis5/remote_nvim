#include "networking/socket.hpp"
#include <utility>

namespace networking
{

socket::socket(socket&& other) noexcept
    : fd_{ std::exchange(other.fd_, -1) }
    , sock_family_{ other.sock_family_ }
    , sock_type_{ other.sock_type_ }
    , sock_protocol_{ other.sock_protocol_ } {}

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
