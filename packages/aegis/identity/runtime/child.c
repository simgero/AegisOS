#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "child_private.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int owned(struct aegis_child *child) {
    if (!child) { errno = EINVAL; return -1; }
    /* Bionic caches getpid() in TLS. A raw clone3 child initially inherits that
     * cache, so this ownership check must ask the kernel directly. */
    pid_t self = (pid_t)syscall(SYS_getpid);
    if (self <= 0 || child->owner != self) { errno = EPERM; return -1; }
    return 0;
}

static int terminal(const siginfo_t *info) {
    return info->si_signo == SIGCHLD && info->si_pid > 0 && !info->si_errno
            && (info->si_code == CLD_EXITED || info->si_code == CLD_KILLED
                || info->si_code == CLD_DUMPED);
}

int aegis_child_watch(int pidfd, struct aegis_child **output) {
    if (pidfd < 0 || !output || *output) { errno = EINVAL; return -1; }
    pid_t self = (pid_t)syscall(SYS_getpid);
    if (self <= 0) { errno = EPERM; return -1; }
    int duplicate = fcntl(pidfd, F_DUPFD_CLOEXEC, 3);
    if (duplicate < 0) return -1;
    /* WNOWAIT validates the pidfd and parent relationship without consuming
     * an already-exited child. A pipe/proc-directory/unrelated pidfd is refused.
     * EINTR is returned to the owner rather than introducing an unbounded loop.
     */
    siginfo_t info = {0};
    int checked = waitid(P_PIDFD, (id_t)duplicate, &info, WEXITED | WNOHANG | WNOWAIT);
    if (checked < 0 || (info.si_pid && !terminal(&info))) {
        int saved = checked < 0 ? errno : EPROTO;
        close(duplicate);
        errno = saved;
        return -1;
    }
    struct aegis_child *child = calloc(1, sizeof(*child));
    if (!child) { int saved = errno; close(duplicate); errno = saved; return -1; }
    child->pidfd = duplicate;
    child->owner = self;
    *output = child;
    return 0;
}

/* 1 = reaped by this owner, 0 = not observed yet, -1 = observation failed. */
static int inspect(struct aegis_child *child) {
    if (owned(child) < 0) return -1;
    if (child->observed) return 1;
    if (child->observation_error) { errno = child->observation_error; return -1; }
    siginfo_t info = {0};
    if (waitid(P_PIDFD, (id_t)child->pidfd, &info, WEXITED | WNOHANG) < 0) {
        if (errno == EINTR || errno == EAGAIN) return 0;
        child->observation_error = errno;
        return -1;
    }
    if (!info.si_pid) return 0;
    if (!terminal(&info)) {
        child->observation_error = EPROTO;
        errno = EPROTO;
        return -1;
    }
    child->result.code = info.si_code;
    child->result.status = info.si_status;
    child->observed = 1;
    return 1;
}

int aegis_child_request_stop(struct aegis_child *child) {
    int state = inspect(child);
    if (state < 0) return -1;
    if (state > 0) return 0;
    /* flags=0 signals only this process, never a process group or reused PID.
     * ESRCH still requires waitid confirmation, including detecting a stolen
     * exit status. No numeric kill()/waitpid() fallback exists.
     */
    if (syscall(SYS_pidfd_send_signal, child->pidfd, SIGKILL, NULL, 0u) < 0
            && errno != ESRCH) return -1;
    return 0;
}

static int64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

int aegis_child_wait(struct aegis_child *child, int timeout_ms,
                     struct aegis_child_exit *result) {
    if (!result || timeout_ms < 0 || timeout_ms > 60000) { errno = EINVAL; return -1; }
    if (owned(child) < 0) return -1;
    int64_t start = monotonic_ms();
    if (start < 0) return -1;
    const int64_t deadline = start + timeout_ms;
    for (;;) {
        int state = inspect(child);
        if (state < 0) return -1;
        if (state > 0) { *result = child->result; return 0; }
        int64_t now = monotonic_ms();
        if (now < 0) return -1;
        if (now >= deadline) { errno = ETIMEDOUT; return -1; }
        struct pollfd process = {.fd = child->pidfd, .events = POLLIN};
        int ready = poll(&process, 1, (int)(deadline - now));
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) return -1;
        if (process.revents & POLLNVAL) {
            child->observation_error = EBADF;
            errno = EBADF;
            return -1;
        }
        /* POLLIN/HUP/ERR and timeouts all go through actual waitid. A bare
         * readiness notification is never sufficient to report termination.
         */
    }
}

void aegis_child_release(struct aegis_child *child) {
    if (!child) return;
    close(child->pidfd);
    free(child);
}
