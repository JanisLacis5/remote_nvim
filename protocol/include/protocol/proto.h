#ifndef PROTOCOL_PROTO_H
#define PROTOCOL_PROTO_H
#include "networking/socket.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FIELD_SIZE (16U * 1024U * 1024U)
enum message_type { MSG_OPEN = 1, MSG_SAVE, MSG_OK, MSG_ERROR, MSG_CLOSE, MSG_AUTHENTICATE };
struct field {
    char* data;
    size_t size;
};
struct message {
    enum message_type type;
    size_t count;
    struct field fields[4];
};

/* Read requires an empty output and allocates fields (including a trailing NUL); clear releases them.
 * Write borrows fields. On any framing/I/O failure discard the connection. */
bool message_read(int fd, struct message* message);
bool message_write(int fd, const struct message* message);
void message_clear(struct message* message);
struct field field_string(const char* text);

#ifdef __cplusplus
}
#endif
#endif
