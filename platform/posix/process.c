#include "os/process.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

bool process_spawn(struct child_process *child, char *const argv[]) {
    *child = (struct child_process){ -1, -1 };
    if (!argv || !argv[0] || !argv[0][0]) {
        errno = EINVAL;
        return false;
    }

    child->pid = fork();
    if (child->pid < 0)
        return false;

    if (child->pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }

    return true;
}

static bool reap(struct child_process *child, int options) {
    if (child->pid < 0)
        return false;

    int status;
    pid_t result;
    do {
        result = waitpid(child->pid, &status, options);
    } while (result < 0 && errno == EINTR);

    if (result == 0)
        return true;

    child->status = result > 0 && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    child->pid = -1;
    return false;
}

bool process_running(struct child_process *child) {
    return reap(child, WNOHANG);
}

int process_wait(struct child_process *child) {
    reap(child, 0);
    return child->status;
}

char *process_cwd(const struct child_process *child) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%ld/cwd", (long)child->pid);
    return realpath(path, NULL);
}
