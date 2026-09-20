#include "networking/socket.hpp"

#include <stdexcept>

namespace networking {

socket::socket(int fd) {
    throw std::logic_error("windows support not implemented");
}

}
