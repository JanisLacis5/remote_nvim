#include "workspace.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/xattr.h>
#include <unistd.h>

bool ws_fail(const char **error, const char *message) {
    if (error)
        *error = message;
    return false;
}

char *ws_join(const char *parent, const char *name) {
    char *result = NULL;
    if (asprintf(&result, "%s%s%s", parent, parent[0] && parent[strlen(parent) - 1] == '/' ? "" : "/", name) < 0)
        return NULL;
    return result;
}

static bool same(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino && a->st_size == b->st_size &&
           a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
}

static bool equal(struct field a, struct field b) {
    return a.size == b.size && (!a.size || memcmp(a.data, b.data, a.size) == 0);
}

void ws_snapshot_clear(struct snapshot *snapshot) {
    free(snapshot->content.data);
    *snapshot = (struct snapshot){ 0 };
}

bool ws_read(const char *path, struct snapshot *result, const char **error) {
    *result = (struct snapshot){ 0 };
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    const char *failure = "cannot open file";
    if (fd < 0)
        goto fail;

    failure = "expected a regular file";
    if (fstat(fd, &result->info) < 0 || !S_ISREG(result->info.st_mode))
        goto fail;

    failure = "file exceeds 16 MiB limit";
    if (result->info.st_size < 0 || (uint64_t)result->info.st_size > MAX_FIELD_SIZE)
        goto fail;

    size_t capacity = (size_t)result->info.st_size;
    result->content.data = malloc(capacity + 1);
    failure = "out of memory";
    if (!result->content.data)
        goto fail;

    char buffer[65536];
    for (;;) {
        ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR)
            continue;

        failure = "file read failed";
        if (count < 0)
            goto fail;
        if (count == 0)
            break;

        failure = "file exceeds 16 MiB limit";
        if ((size_t)count > MAX_FIELD_SIZE - result->content.size)
            goto fail;

        size_t size = result->content.size + (size_t)count;
        if (size > capacity) {
            char *data = realloc(result->content.data, size + 1);
            failure = "out of memory";
            if (!data)
                goto fail;

            result->content.data = data;
            capacity = size;
        }

        memcpy(result->content.data + result->content.size, buffer, (size_t)count);
        result->content.size = size;
    }

    struct stat after;
    failure = "file changed during read; retry";
    if (fstat(fd, &after) < 0 || !same(&result->info, &after))
        goto fail;

    result->content.data[result->content.size] = '\0';
    close(fd);
    return true;

fail:
    if (fd >= 0)
        close(fd);
    ws_snapshot_clear(result);
    return ws_fail(error, failure);
}

static char *parent_directory(const char *path) {
    char *parent = strdup(path);
    if (!parent)
        return NULL;

    char *slash = strrchr(parent, '/');
    if (!slash) {
        free(parent);
        return strdup(".");
    }

    slash[slash == parent ? 1 : 0] = '\0';
    return parent;
}

static bool persist_directory(const char *parent, const char **error) {
    int fd = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0)
        return ws_fail(error, "cannot open directory; save outcome uncertain");

    int result = fsync(fd);
    close(fd);
    return result == 0 || ws_fail(error, "cannot persist directory; save outcome uncertain");
}

static bool write_content(int fd, struct field content, const char **error) {
    size_t offset = 0;
    while (offset < content.size) {
        ssize_t count = write(fd, content.data + offset, content.size - offset);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return ws_fail(error, "file write failed");

        offset += (size_t)count;
    }

    return true;
}

/* A baseline requests conflict checks and preservation of ordinary ownership/mode. */
static bool replace(const char *path, struct field content, unsigned mode, struct snapshot *baseline,
                    const char **error) {
    bool ok = false;
    int fd = -1;
    char *parent = parent_directory(path);
    char *temp = parent ? ws_join(parent, ".janisnvim-XXXXXX") : NULL;
    struct snapshot current = { 0 }, rechecked = { 0 }, updated = { 0 };
    if (!temp) {
        ws_fail(error, "out of memory");
        goto out;
    }

    if (baseline) {
        if (!ws_read(path, &current, error))
            goto out;
        if (!equal(current.content, baseline->content) || !same(&current.info, &baseline->info)) {
            ws_fail(error, "remote conflict: file or metadata changed; local edits retained");
            goto out;
        }

        if (current.info.st_uid != geteuid() || current.info.st_nlink != 1 || (current.info.st_mode & 07000) ||
            listxattr(path, NULL, 0) != 0) {
            ws_fail(error, "safe replacement requires an owned, single-link file without special bits or extended attributes");
            goto out;
        }
        mode = current.info.st_mode & 0777;
    }

    fd = mkstemp(temp);
    if (fd < 0) {
        ws_fail(error, "cannot create temporary file");
        goto out;
    }

    if (baseline && (fchown(fd, current.info.st_uid, current.info.st_gid) < 0 || flistxattr(fd, NULL, 0) != 0)) {
        ws_fail(error, "cannot preserve ownership or replacement inherits unsupported extended metadata");
        goto out;
    }

    if (!write_content(fd, content, error))
        goto out;
    if (fchmod(fd, mode) < 0 || fsync(fd) < 0) {
        ws_fail(error, "cannot persist file");
        goto out;
    }

    if (baseline) {
        if (!ws_read(path, &rechecked, error))
            goto out;
        if (!same(&rechecked.info, &current.info) || !equal(rechecked.content, current.content)) {
            ws_fail(error, "remote conflict during save");
            goto out;
        }
    }

    /* shortcut: unrelated writers can race the final check; cooperating sessions use a sidecar lock. */
    if (rename(temp, path) < 0) {
        ws_fail(error, "cannot replace file");
        goto out;
    }
    if (!persist_directory(parent, error))
        goto out;

    if (baseline) {
        if (!ws_read(path, &updated, error))
            goto out;
        if (!equal(updated.content, content)) {
            ws_fail(error, "remote changed immediately after save");
            goto out;
        }

        ws_snapshot_clear(baseline);
        *baseline = updated;
        updated = (struct snapshot){ 0 };
    }
    ok = true;

out:
    if (fd >= 0) {
        close(fd);
        unlink(temp);
    }
    free(temp);
    free(parent);
    ws_snapshot_clear(&current);
    ws_snapshot_clear(&rechecked);
    ws_snapshot_clear(&updated);
    return ok;
}

bool ws_atomic_write(const char *path, struct field content, unsigned mode, const char **error) {
    return replace(path, content, mode, NULL, error);
}

bool ws_sync(const char *path, struct snapshot *baseline, struct field content, const char **error) {
    return replace(path, content, 0, baseline, error);
}

bool ws_private_directory(const char *path, const char **error) {
    if (mkdir(path, 0700) < 0 && errno != EEXIST)
        return ws_fail(error, "cannot create cache directory");

    struct stat info;
    if (lstat(path, &info) < 0 || !S_ISDIR(info.st_mode) || info.st_uid != geteuid() || (info.st_mode & 077))
        return ws_fail(error, "cache directory must be owned by you with mode 0700 and not a symlink");
    return true;
}

bool ws_absolute_path(const char *path) {
    if (!path || path[0] != '/')
        return false;
    if (!path[1])
        return true;

    const char *part = path + 1;
    while (*part) {
        const char *end = strchr(part, '/');
        size_t size = end ? (size_t)(end - part) : strlen(part);
        if (!size || (size == 1 && part[0] == '.') || (size == 2 && part[0] == '.' && part[1] == '.'))
            return false;
        if (!end)
            return true;
        part = end + 1;
    }
    return false;
}

static bool inside(const char *directory, const char *path) {
    size_t length = strlen(directory);
    return !strncmp(directory, path, length) && (length == 1 || path[length] == '/' || path[length] == '\0');
}

char *ws_cache_entry(const char *root, const char *host, const char *directory, const char *path, const char **error) {
    if (!host[0] || strlen(host) > 128 || !ws_absolute_path(directory) || !ws_absolute_path(path) || !inside(directory, path)) {
        ws_fail(error, "invalid remote identity or file outside workspace");
        return NULL;
    }

    char *identity = NULL;
    if (asprintf(&identity, "%zu:%s%zu:%s%zu:%s", strlen(host), host, strlen(directory), directory, strlen(path), path) < 0)
        identity = NULL;
    char *result = strdup(root);
    if (!identity || !result) {
        ws_fail(error, "out of memory");
        goto fail;
    }
    if (!ws_private_directory(result, error))
        goto fail;

    const char digits[] = "0123456789abcdef";
    size_t length = strlen(identity);
    for (size_t offset = 0; offset < length;) {
        char chunk[101];
        size_t count = length - offset < 50 ? length - offset : 50;
        for (size_t i = 0; i < count; ++i) {
            unsigned char c = (unsigned char)identity[offset + i];
            chunk[2 * i] = digits[c >> 4];
            chunk[2 * i + 1] = digits[c & 15];
        }
        chunk[count * 2] = '\0';
        offset += count;

        char *next = ws_join(result, chunk);
        if (!next) {
            ws_fail(error, "out of memory");
            goto fail;
        }
        free(result);
        result = next;
        if (!ws_private_directory(result, error))
            goto fail;
    }

    char *entry = ws_join(result, "entry");
    if (!entry) {
        ws_fail(error, "out of memory");
        goto fail;
    }
    free(result);
    result = entry;
    if (!ws_private_directory(result, error))
        goto fail;

    free(identity);
    return result;

fail:
    free(identity);
    free(result);
    return NULL;
}

bool ws_prepare_cache(const char *entry, const char *file, struct field remote, const char **error) {
    bool ok = false;
    char *base = ws_join(entry, "base");
    struct snapshot local = { 0 }, previous = { 0 };
    struct stat info;
    if (!base)
        return ws_fail(error, "out of memory");

    if (lstat(file, &info) == 0) {
        if (!ws_read(file, &local, error))
            goto out;
        if (!equal(local.content, remote)) {
            if (!ws_read(base, &previous, error))
                goto out;
            if (equal(remote, previous.content)) {
                ok = true;
                goto out;
            }
            if (!equal(local.content, previous.content)) {
                ws_fail(error, "remote conflict: cached edits retained; manual reconciliation required");
                goto out;
            }
        }
    } else if (errno != ENOENT) {
        ws_fail(error, "cannot inspect cached file");
        goto out;
    }

    ok = ws_atomic_write(base, remote, 0600, error) && ws_atomic_write(file, remote, 0600, error);

out:
    free(base);
    ws_snapshot_clear(&local);
    ws_snapshot_clear(&previous);
    return ok;
}
