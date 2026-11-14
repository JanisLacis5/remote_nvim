#include "networking/listener.h"
#include <sys/socket.h>

int net_listen(uint16_t port) {
    struct net_socket listener = net_socket_open(AF_INET, SOCK_STREAM, 0);
    int yes = 1;

    if (listener.fd < 0)
        return -1;

    if (setsockopt(listener.fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0 ||
        !net_socket_bind(&listener, INADDR_LOOPBACK, port) || listen(listener.fd, 10) < 0) {
        net_socket_close(&listener);
        return -1;
    }

    return listener.fd;
}

uint16_t net_listener_port(int fd) {
    struct sockaddr_in addr = { 0 };
    socklen_t size = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &size) < 0)
        return 0;

    return ntohs(addr.sin_port);
}

struct net_socket net_accept(int fd) {
    return net_socket_adopt(accept4(fd, NULL, NULL, SOCK_CLOEXEC));
}
