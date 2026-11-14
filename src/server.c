/* Invoked on the remote host; owns one file session, never an editor. */
#include "session.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <unistd.h>

#define CHECK(condition, reason)                                                                                       \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            error = (reason);                                                                                          \
            goto out;                                                                                                  \
        }                                                                                                              \
    } while (0)

int main(int argc, char** argv) {
    int result = 1, lock = -1;
    struct net_socket socket = { .fd = -1 };
    struct snapshot baseline = { 0 }, machine = { 0 };
    struct message incoming = { 0 };
    char *directory = NULL, *path = NULL, *host = NULL, *lock_path = NULL;
    const char* error = NULL;
    const char* token = getenv("JANISNVIM_TOKEN");

    CHECK(argc == 2, "usage: janisnvim path/to/existing-file");
    CHECK(token && strlen(token) >= 32, "set JANISNVIM_TOKEN to the daemon's shared secret (at least 32 characters)");

    directory = realpath(".", NULL);
    path = realpath(argv[1], NULL);
    CHECK(directory && path, "cannot resolve workspace or file");

    size_t length = strlen(directory);
    CHECK(!strncmp(path, directory, length) && (length == 1 || path[length] == '/' || path[length] == '\0'),
          "file must resolve inside the current workspace directory");
    if (!ws_read(path, &baseline, &error) || !ws_read("/etc/machine-id", &machine, &error))
        goto out;

    while (machine.content.size && (machine.content.data[machine.content.size - 1] == '\n' ||
                                    machine.content.data[machine.content.size - 1] == '\r'))
        machine.content.data[--machine.content.size] = '\0';
    CHECK(machine.content.size, "machine-id is empty");
    if (asprintf(&host, "%s:%lu", machine.content.data, (unsigned long)geteuid()) < 0)
        host = NULL;
    CHECK(host, "out of memory");

    /* Cooperating sessions lock a stable sidecar across atomic replacements. */
    const char* name = strrchr(path, '/') + 1;
    if (asprintf(&lock_path, "%.*s.janisnvim-%s.lock", (int)(name - path), path, name) < 0)
        lock_path = NULL;
    CHECK(lock_path, "out of memory");
    lock = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    CHECK(lock >= 0 && flock(lock, LOCK_EX | LOCK_NB) == 0, "file already open or cannot lock file");

    socket = net_socket_open(AF_INET, SOCK_STREAM, 0);
    CHECK(net_socket_connect(&socket, INADDR_LOOPBACK, 7778),
          "cannot reach local daemon via SSH reverse forward on port 7778");
    CHECK(reply(socket.fd, MSG_AUTHENTICATE, token), "authentication send failed");
    CHECK(message_read(socket.fd, &incoming) && message_is(&incoming, MSG_OK, 0), "daemon authentication failed");
    message_clear(&incoming);

    struct message open = { .type = MSG_OPEN,
                            .count = 4,
                            .fields = { field_string(host), field_string(directory), field_string(path),
                                        baseline.content } };
    CHECK(message_write(socket.fd, &open), "file transfer failed");
    CHECK(message_read(socket.fd, &incoming), "local editor failed to open");
    if (message_is(&incoming, MSG_ERROR, 1) && field_is_text(incoming.fields[0])) {
        error = incoming.fields[0].data;
        goto out;
    }
    CHECK(message_is(&incoming, MSG_OK, 1) && field_is_text(incoming.fields[0]), "invalid open acknowledgement");
    printf("Local working copy: %s\n", incoming.fields[0].data);
    message_clear(&incoming);

    while (message_read(socket.fd, &incoming)) {
        if (message_is(&incoming, MSG_ERROR, 1) && field_is_text(incoming.fields[0])) {
            error = incoming.fields[0].data;
            goto out;
        }
        if (message_is(&incoming, MSG_CLOSE, 0)) {
            result = 0;
            goto out;
        }
        CHECK(message_is(&incoming, MSG_SAVE, 1), "invalid save request");

        const char* save_error = NULL;
        bool saved = ws_sync(path, &baseline, incoming.fields[0], &save_error);
        CHECK(reply(socket.fd, saved ? MSG_OK : MSG_ERROR, saved ? NULL : save_error), "connection lost during save");
        message_clear(&incoming);
    }
    error = "connection lost; local cached edits are retained";

out:
    if (error)
        fprintf(stderr, "janisnvim: %s\n", error);
    message_clear(&incoming);
    net_socket_close(&socket);
    if (lock >= 0)
        close(lock);
    ws_snapshot_clear(&baseline);
    ws_snapshot_clear(&machine);
    free(directory);
    free(path);
    free(host);
    free(lock_path);
    return result;
}
