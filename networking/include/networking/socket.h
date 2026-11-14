#ifndef NETWORKING_SOCKET_H
#define NETWORKING_SOCKET_H

#include <arpa/inet.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct net_socket {
    int fd;
    int family;
};

struct net_socket net_socket_open(int family, int type, int protocol);
struct net_socket net_socket_adopt(int fd);
void net_socket_close(struct net_socket *socket);
bool net_socket_bind(const struct net_socket *socket, uint32_t address, uint16_t port);
bool net_socket_connect(const struct net_socket *socket, uint32_t address, uint16_t port);
bool net_read(int fd, void *buffer, size_t size);
size_t net_write(int fd, const void *buffer, size_t size);

#ifdef __cplusplus
}
#endif
#endif
