#include "protocol/proto.h"
#include <stdlib.h>
#include <string.h>

struct field field_string(const char* text) {
    return (struct field){ (char*)text, strlen(text) };
}

void message_clear(struct message* message) {
    for (size_t i = 0; i < message->count; ++i)
        free(message->fields[i].data);

    *message = (struct message){ 0 };
}

bool message_read(int fd, struct message* message) {
    unsigned char header[6];
    *message = (struct message){ 0 };

    if (!net_read(fd, header, sizeof(header)) || header[0] != 'J' || header[1] != 'N' || header[2] != 1 ||
        header[3] < MSG_OPEN || header[3] > MSG_AUTHENTICATE || header[4] > 4 || header[5] != 0)
        return false;

    message->type = header[3];
    for (size_t i = 0; i < header[4]; ++i) {
        uint32_t size;
        if (!net_read(fd, &size, sizeof(size)))
            goto fail;

        size = ntohl(size);
        if (size > MAX_FIELD_SIZE)
            goto fail;

        char* data = malloc((size_t)size + 1);
        if (!data)
            goto fail;

        message->fields[message->count++] = (struct field){ data, size };
        if (!net_read(fd, data, size))
            goto fail;

        data[size] = '\0';
    }

    return true;

fail:
    message_clear(message);
    return false;
}

bool message_write(int fd, const struct message* message) {
    if (message->count > 4 || message->type < MSG_OPEN || message->type > MSG_AUTHENTICATE)
        return false;

    for (size_t i = 0; i < message->count; ++i)
        if (message->fields[i].size > MAX_FIELD_SIZE || (!message->fields[i].data && message->fields[i].size))
            return false;

    const unsigned char header[] = { 'J', 'N', 1, message->type, message->count, 0 };
    if (net_write(fd, header, sizeof(header)) != sizeof(header))
        return false;

    for (size_t i = 0; i < message->count; ++i) {
        const struct field* field = &message->fields[i];
        uint32_t size = htonl((uint32_t)field->size);
        if (net_write(fd, &size, sizeof(size)) != sizeof(size) ||
            net_write(fd, field->data, field->size) != field->size)
            return false;
    }

    return true;
}
