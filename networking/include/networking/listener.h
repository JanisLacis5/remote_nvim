#ifndef NETWORKING_LISTENER_H
#define NETWORKING_LISTENER_H
#include "networking/socket.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Returned descriptors belong to the caller. Port zero selects an ephemeral port. */
int net_listen(uint16_t port);
uint16_t net_listener_port(int fd);
struct net_socket net_accept(int fd);

#ifdef __cplusplus
}
#endif
#endif
