#ifndef OS_PROCESS_H
#define OS_PROCESS_H
#include <stdbool.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif

struct child_process { pid_t pid; int status; };
/* argv is NULL-terminated. A successful spawn owns a child until wait/reap. */
bool process_spawn(struct child_process *child, char *const argv[]);
bool process_running(struct child_process *child);
int process_wait(struct child_process *child);
/* Caller frees the returned directory. */
char *process_cwd(const struct child_process *child);

#ifdef __cplusplus
}
#endif
#endif
