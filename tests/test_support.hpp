#pragma once

#include "networking/socket.hpp"
#include <cerrno>
#include <span>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace test_support
{

class unique_fd
{
public:
    explicit unique_fd(int fd = -1)
        : fd_{ fd } {}

    ~unique_fd() { reset(); }

    unique_fd(const unique_fd&) = delete;
    unique_fd& operator=(const unique_fd&) = delete;

    unique_fd(unique_fd&& other) noexcept
        : fd_{ other.release() } {}

    unique_fd& operator=(unique_fd&& other) noexcept {
        if (this != &other) {
            const auto transferred_fd = other.release();
            reset(transferred_fd);
        }
        return *this;
    }

    int get() const { return fd_; }
    int release() { return std::exchange(fd_, -1); }

    void reset(int fd = -1) {
        if (fd_ >= 0)
            ::close(fd_);

        fd_ = fd;
    }

private:
    int fd_{ -1 };
};

inline void require_syscall_success(ssize_t result, const char* operation) {
    if (result < 0) {
        const std::error_code error{ errno, std::generic_category() };
        throw std::system_error{ error, operation };
    }
}

inline void set_socket_timeouts(int fd) {
    const timeval timeout{ 2, 0 };

    const auto receive_timeout_result = ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    require_syscall_success(receive_timeout_result, "receive timeout");

    const auto send_timeout_result = ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    require_syscall_success(send_timeout_result, "send timeout");
}

struct socket_pair {
    unique_fd local{};
    unique_fd peer{};

    socket_pair() {
        int fds[2]{};
        const auto create_result = ::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds);
        require_syscall_success(create_result, "socketpair");

        local.reset(fds[0]);
        peer.reset(fds[1]);

        set_socket_timeouts(local.get());
        set_socket_timeouts(peer.get());
    }
};

inline std::vector<std::byte> vector_byte(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result{};

    for (auto value : values) {
        const auto byte = static_cast<std::byte>(value);
        result.push_back(byte);
    }
    return result;
}

inline std::vector<std::byte> make_byte_pattern(std::size_t size) {
    std::vector<std::byte> result(size);

    for (std::size_t i{}; i < size; ++i)
        result[i] = static_cast<std::byte>((i * 37) % 256);
    return result;
}

// Independent of networking::socket and the protocol codec.
inline void write_all_bytes(int fd, std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
        const auto sent = ::write(fd, bytes.data(), bytes.size());
        if (sent < 0 && errno == EINTR)
            continue;

        require_syscall_success(sent, "write");
        if (sent == 0)
            throw std::runtime_error("write made no progress");

        const auto bytes_written = static_cast<std::size_t>(sent);
        bytes = bytes.subspan(bytes_written);
    }
}

inline std::vector<std::byte> read_exact_bytes(int fd, std::size_t size) {
    std::vector<std::byte> result(size);
    std::size_t offset{};

    while (offset < size) {
        const auto count = ::recv(fd, result.data() + offset, size - offset, 0);
        if (count < 0 && errno == EINTR)
            continue;

        require_syscall_success(count, "recv");
        if (count == 0)
            throw std::runtime_error("unexpected EOF");

        offset += static_cast<std::size_t>(count);
    }

    return result;
}

inline unique_fd connect_tcp_peer(std::uint16_t port) {
    const auto socket_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    unique_fd fd{ socket_fd };
    require_syscall_success(fd.get(), "socket");
    set_socket_timeouts(fd.get());

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);

    const auto* socket_address = reinterpret_cast<const sockaddr*>(&address);
    const auto connect_result = ::connect(fd.get(), socket_address, sizeof(address));
    require_syscall_success(connect_result, "connect");

    return fd;
}

} // namespace test_support
