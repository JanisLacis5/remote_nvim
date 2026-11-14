#include "networking/socket.h"
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

struct net_socket net_socket_open(int family, int type, int protocol) {
    return (struct net_socket){ socket(family, type | SOCK_CLOEXEC, protocol), family };
}

struct net_socket net_socket_adopt(int fd) {
    struct net_socket result = { fd, 0 };
    int value;
    socklen_t size = sizeof(value);

    if (fd >= 0 && (getsockopt(fd, SOL_SOCKET, SO_DOMAIN, &result.family, &size) < 0 ||
                    getsockopt(fd, SOL_SOCKET, SO_TYPE, &value, &size) < 0 ||
                    getsockopt(fd, SOL_SOCKET, SO_PROTOCOL, &value, &size) < 0))
        net_socket_close(&result);

    return result;
}

void net_socket_close(struct net_socket* socket) {
    if (socket->fd >= 0)
        close(socket->fd);

    socket->fd = -1;
}

static struct sockaddr_in address(int family, uint32_t host, uint16_t port) {
    struct sockaddr_in result = { 0 };
    result.sin_family = family;
    result.sin_addr.s_addr = htonl(host);
    result.sin_port = htons(port);

    return result;
}

bool net_socket_bind(const struct net_socket* socket, uint32_t host, uint16_t port) {
    struct sockaddr_in addr = address(socket->family, host, port);
    return socket->fd >= 0 && bind(socket->fd, (struct sockaddr*)&addr, sizeof(addr)) == 0;
}

bool net_socket_connect(const struct net_socket* socket, uint32_t host, uint16_t port) {
    struct sockaddr_in addr = address(socket->family, host, port);
    return socket->fd >= 0 && connect(socket->fd, (struct sockaddr*)&addr, sizeof(addr)) == 0;
}

bool net_read(int fd, void* buffer, size_t size) {
    if (fd < 0)
        return false;

    size_t offset = 0;
    while (offset < size) {
        ssize_t count = read(fd, (char*)buffer + offset, size - offset);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return false;

        offset += (size_t)count;
    }

    return true;
}

size_t net_write(int fd, const void* buffer, size_t size) {
    if (fd < 0)
        return 0;

    size_t offset = 0;
    while (offset < size) {
        ssize_t count = send(fd, (const char*)buffer + offset, size - offset, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            break;

        offset += (size_t)count;
    }

    return offset;
}
