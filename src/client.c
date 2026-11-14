/* Local service and asynchronous save helper. */
#include "networking/listener.h"
#include "os/process.h"
#include "session.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define CHECK(condition, reason)                                                                                       \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            ws_fail(error, reason);                                                                                    \
            goto out;                                                                                                  \
        }                                                                                                              \
    } while (0)

static bool timeout(int fd) {
    const struct timeval limit = { 30, 0 };
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit)) == 0 &&
           setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &limit, sizeof(limit)) == 0;
}

static bool address(const char* path, struct sockaddr_un* result) {
    *result = (struct sockaddr_un){ .sun_family = AF_UNIX };
    if (strlen(path) >= sizeof(result->sun_path))
        return false;

    strcpy(result->sun_path, path);
    return true;
}

static int save(const char* socket_path, const char* file) {
    bool ok = false;
    const char* failure = NULL;
    const char** error = &failure;
    struct snapshot content = { 0 };
    struct net_socket connection = { .fd = -1 };
    struct message response = { 0 };
    struct sockaddr_un addr;

    if (!ws_read(file, &content, error))
        goto out;

    connection = net_socket_open(AF_UNIX, SOCK_STREAM, 0);
    CHECK(timeout(connection.fd), "cannot set transport timeout");
    CHECK(address(socket_path, &addr), "local socket path too long");
    CHECK(connect(connection.fd, (struct sockaddr*)&addr, sizeof(addr)) == 0,
          "session unavailable; saved local copy retained; reopen remotely to reconnect");

    struct message update = { .type = MSG_SAVE, .count = 1, .fields = { content.content } };
    CHECK(message_write(connection.fd, &update), "cannot queue save; local copy retained");
    CHECK(message_read(connection.fd, &response),
          "sync timed out or disconnected; remote outcome uncertain, local copy retained");
    if (message_is(&response, MSG_ERROR, 1) && field_is_text(response.fields[0])) {
        failure = response.fields[0].data;
        goto out;
    }
    CHECK(message_is(&response, MSG_OK, 0), "invalid sync response");
    puts("Remote file synchronized");
    ok = true;

out:
    if (!ok)
        fprintf(stderr, "janisnvimdaemon: %s\n", failure);
    message_clear(&response);
    ws_snapshot_clear(&content);
    net_socket_close(&connection);
    return ok ? 0 : 1;
}

static bool sync_update(int remote, int local, bool* connected, const char* base, const char** error) {
    bool ok = false;
    struct message update = { 0 }, response = { 0 };
    CHECK(message_read(local, &update) && message_is(&update, MSG_SAVE, 1), "invalid local save");
    CHECK(*connected, "remote connection lost; reopen remotely to reconnect; local copy retained");

    if (!message_write(remote, &update) || !message_read(remote, &response)) {
        *connected = false;
        ws_fail(error, "remote save outcome uncertain; local copy retained");
        goto out;
    }
    if (message_is(&response, MSG_ERROR, 1) && field_is_text(response.fields[0])) {
        /* Forward before freeing the response's owned field. */
        fprintf(stderr, "Sync: %s\n", response.fields[0].data);
        message_write(local, &response);
        goto done;
    }
    CHECK(message_is(&response, MSG_OK, 0), "invalid remote acknowledgement");
    if (!ws_atomic_write(base, update.fields[0], 0600, error))
        goto out;

    ok = true;

out:
    if (!ok)
        fprintf(stderr, "Sync: %s\n", *error);
    reply(local, ok ? MSG_OK : MSG_ERROR, ok ? NULL : *error);
done:
    message_clear(&update);
    message_clear(&response);
    return ok;
}

static bool session(int remote, const struct message* request, const char* root, const char** error) {
    bool ok = false;
    int lock = -1;
    struct net_socket listener = { .fd = -1 };
    struct child_process editor = { -1, -1 };
    char *entry = NULL, *working = NULL, *file = NULL, *base = NULL, *lock_path = NULL;
    char *executable = NULL, *socket_path = NULL;
    char runtime[] = "/tmp/janisnvim-XXXXXX";
    bool runtime_created = false;
    struct sockaddr_un addr;

    CHECK(message_is(request, MSG_OPEN, 4), "expected an open-file request");
    for (size_t i = 0; i < 3; ++i)
        CHECK(field_is_text(request->fields[i]), "invalid remote identity");

    entry = ws_cache_entry(root, request->fields[0].data, request->fields[1].data, request->fields[2].data, error);
    if (!entry)
        goto out;

    working = ws_join(entry, "working");
    base = ws_join(entry, "base");
    lock_path = ws_join(entry, "lock");
    CHECK(working && base && lock_path, "out of memory");
    if (!ws_private_directory(working, error))
        goto out;

    const char* name = strrchr(request->fields[2].data, '/') + 1;
    file = ws_join(working, name);
    CHECK(file, "out of memory");
    lock = open(lock_path, O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    CHECK(lock >= 0 && flock(lock, LOCK_EX | LOCK_NB) == 0, "cached file already open or cannot lock cache");
    if (!ws_prepare_cache(entry, file, request->fields[3], error)) {
        fprintf(stderr, "Cache retained: %s\n", file);
        goto out;
    }

    CHECK(mkdtemp(runtime), "cannot create session directory");
    runtime_created = true;
    socket_path = ws_join(runtime, "sync.sock");
    CHECK(socket_path, "out of memory");
    listener = net_socket_open(AF_UNIX, SOCK_STREAM, 0);
    CHECK(address(socket_path, &addr), "local socket path too long");
    CHECK(bind(listener.fd, (struct sockaddr*)&addr, sizeof(addr)) == 0 && listen(listener.fd, 16) == 0,
          "cannot listen for local saves");

    executable = realpath("/proc/self/exe", NULL);
    CHECK(executable, "cannot resolve executable");
    CHECK(setenv("JANISNVIM_HELPER", executable, 1) == 0 && setenv("JANISNVIM_SESSION", socket_path, 1) == 0 &&
              setenv("JANISNVIM_FILE", file, 1) == 0 && setenv("JANISNVIM_LUA", JANISNVIM_LUA, 1) == 0,
          "cannot configure editor environment");

    char* terminal = getenv("JANISNVIM_TERMINAL");
    if (!terminal || !*terminal)
        terminal = "kitty";

    char* argv[] = { terminal, "nvim", "-c", "lua dofile(vim.env.JANISNVIM_LUA)", "--", file, NULL };
    CHECK(process_spawn(&editor, argv), "cannot spawn editor");
    CHECK(reply(remote, MSG_OK, file), "open acknowledgement failed");
    fprintf(stderr, "Editing %s in %s\n", request->fields[2].data, file);

    bool connected = true;
    while (process_running(&editor)) {
        struct pollfd waiting = { listener.fd, POLLIN, 0 };
        int ready = poll(&waiting, 1, 200);
        if (ready < 0 && errno == EINTR)
            continue;
        CHECK(ready >= 0, "local save polling failed");
        if (!(waiting.revents & POLLIN))
            continue;

        struct net_socket peer = net_accept(listener.fd);
        if (peer.fd < 0)
            continue;
        if (timeout(peer.fd)) {
            const char* sync_error = NULL;
            sync_update(remote, peer.fd, &connected, base, &sync_error);
        } else {
            reply(peer.fd, MSG_ERROR, "cannot set transport timeout; local copy retained");
        }
        net_socket_close(&peer);
    }

    CHECK(process_wait(&editor) == 0, "terminal/editor exited unsuccessfully; cached file retained");
    if (connected)
        reply(remote, MSG_CLOSE, NULL);
    ok = true;

out:
    process_wait(&editor);
    net_socket_close(&listener);
    if (runtime_created) {
        if (socket_path)
            unlink(socket_path);
        rmdir(runtime);
    }
    if (lock >= 0)
        close(lock);
    free(entry);
    free(working);
    free(file);
    free(base);
    free(lock_path);
    free(executable);
    free(socket_path);
    return ok;
}

static bool create_parents(char* path) {
    for (char* p = path + 1;; ++p) {
        if (*p != '/' && *p != '\0')
            continue;

        char saved = *p;
        *p = '\0';
        int result = mkdir(path, 0777);
        *p = saved;
        if (result < 0 && errno != EEXIST)
            return false;
        if (!saved)
            return true;
    }
}

int main(int argc, char** argv) {
    if (argc == 4 && !strcmp(argv[1], "--sync"))
        return save(argv[2], argv[3]);

    const char* failure = NULL;
    const char** error = &failure;
    char *parent = NULL, *root = NULL, *secret = NULL;
    int listener = -1;
    CHECK(argc == 1, "usage: janisnvimdaemon");

    const char* token = getenv("JANISNVIM_TOKEN");
    CHECK(token && strlen(token) >= 32, "set JANISNVIM_TOKEN to a shared random secret (at least 32 characters)");
    secret = strdup(token);
    CHECK(secret, "out of memory");

    const char *cache = getenv("XDG_CACHE_HOME"), *home = getenv("HOME");
    CHECK((cache && *cache) || (home && *home), "HOME or XDG_CACHE_HOME is required");
    parent = cache && *cache ? strdup(cache) : ws_join(home, ".cache");
    CHECK(parent, "out of memory");
    CHECK(parent[0] == '/', "cache root must be absolute");
    CHECK(create_parents(parent), "cannot create cache root");
    root = ws_join(parent, "janisnvim");
    CHECK(root, "out of memory");
    if (!ws_private_directory(root, error))
        goto out;

    listener = net_listen(7777);
    CHECK(listener >= 0, "cannot listen on 127.0.0.1:7777");

    /* shortcut: one editor session at a time; add workers when simultaneous sessions are needed. */
    for (;;) {
        struct net_socket remote = net_accept(listener);
        if (remote.fd < 0)
            continue;

        struct message request = { 0 };
        const char* session_error = "authentication failed";
        bool authenticated = timeout(remote.fd) && message_read(remote.fd, &request) &&
                             message_is(&request, MSG_AUTHENTICATE, 1) && field_is_text(request.fields[0]) &&
                             !strcmp(request.fields[0].data, secret);
        message_clear(&request);

        if (authenticated && reply(remote.fd, MSG_OK, NULL)) {
            session_error = "file request missing or malformed";
            if (message_read(remote.fd, &request) && session(remote.fd, &request, root, &session_error))
                session_error = NULL;
        }
        if (session_error) {
            fprintf(stderr, "janisnvimdaemon: %s\n", session_error);
            reply(remote.fd, MSG_ERROR, session_error);
        }

        message_clear(&request);
        net_socket_close(&remote);
    }

out:
    if (failure)
        fprintf(stderr, "janisnvimdaemon: %s\n", failure);
    if (listener >= 0)
        close(listener);
    free(parent);
    free(root);
    free(secret);
    return 1;
}
