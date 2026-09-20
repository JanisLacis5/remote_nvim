#include "networking/socket.hpp"
#include <stdexcept>

namespace networking {

socket::socket(int family, int type, int protocol)
{
    throw std::logic_error("windows support not implemented");
}

socket::socket(int fd)
{
    throw std::logic_error("windows support not implemented");
};

bool socket::bind(std::uint32_t addr, std::uint16_t port) {
    throw std::logic_error("windows support not implemented");
}

bool socket::connect(std::uint32_t addr, std::uint16_t port) {
    throw std::logic_error("windows support not implemented");
}

std::vector<std::byte> socket::read(std::size_t min_cnt) {
    throw std::logic_error("windows support not implemented");
}

std::size_t socket::write_all(const std::vector<std::byte>& payload) {
    throw std::logic_error("windows support not implemented");
}

void socket::close() {
    throw std::logic_error("windows support not implemented");
}

}
