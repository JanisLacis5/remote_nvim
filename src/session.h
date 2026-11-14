#ifndef SESSION_H
#define SESSION_H
#include "workspace.h"
#include <string.h>

static inline bool reply(int fd, enum message_type type, const char* text) {
    struct message message = { .type = type };
    if (text) {
        message.count = 1;
        message.fields[0] = field_string(text);
    }

    return message_write(fd, &message);
}

static inline bool message_is(const struct message* message, enum message_type type, size_t count) {
    return message->type == type && message->count == count;
}

static inline bool field_is_text(struct field field) {
    return field.data && !memchr(field.data, '\0', field.size);
}
#endif
