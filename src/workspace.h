#ifndef WORKSPACE_H
#define WORKSPACE_H
#include "protocol/proto.h"
#include <sys/stat.h>
#ifdef __cplusplus
extern "C" {
#endif

struct snapshot {
    struct field content;
    struct stat info;
};
/* Read requires an empty output. Errors point to static strings. Returned paths and snapshot contents belong to the
 * caller. */
bool ws_fail(const char** error, const char* message);
char* ws_join(const char* parent, const char* name);
bool ws_read(const char* path, struct snapshot* result, const char** error);
void ws_snapshot_clear(struct snapshot* snapshot);
bool ws_atomic_write(const char* path, struct field content, unsigned mode, const char** error);
bool ws_sync(const char* path, struct snapshot* baseline, struct field content, const char** error);
bool ws_private_directory(const char* path, const char** error);
bool ws_absolute_path(const char* path);
char* ws_cache_entry(const char* root, const char* host, const char* directory, const char* path, const char** error);
bool ws_prepare_cache(const char* entry, const char* file, struct field remote, const char** error);

#ifdef __cplusplus
}
#endif
#endif
