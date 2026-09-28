#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "control.h"
#include "sandbox.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CONTROL_FD 3
#define STARTUP_MILLISECONDS 5000

static volatile sig_atomic_t stopping;
static const int managed_signals[] = {SIGCHLD, SIGTERM, SIGINT, SIGHUP};
static const sigset_t *wait_mask;

struct shell { pid_t pid; uint64_t request_id; };

static void on_signal(int signal_number) {
    if (signal_number != SIGCHLD) stopping = 1;
}

static int configure_signals(sigset_t *empty) {
    sigset_t blocked;
    sigemptyset(empty);
    sigemptyset(&blocked);
    struct sigaction action = {.sa_handler = on_signal, .sa_flags = SA_NOCLDSTOP};
    sigemptyset(&action.sa_mask);
    for (size_t i = 0; i < sizeof(managed_signals) / sizeof(managed_signals[0]); i++) {
        sigaddset(&blocked, managed_signals[i]);
        if (sigaction(managed_signals[i], &action, NULL) < 0) return -1;
    }
    if (sigprocmask(SIG_SETMASK, &blocked, NULL) < 0) return -1;
    wait_mask = empty;
    return 0;
}

static int reset_signals(void) {
    struct sigaction action = {.sa_handler = SIG_DFL};
    sigemptyset(&action.sa_mask);
    for (size_t i = 0; i < sizeof(managed_signals) / sizeof(managed_signals[0]); i++) {
        if (sigaction(managed_signals[i], &action, NULL) < 0) return -1;
    }
    sigset_t empty;
    sigemptyset(&empty);
    return sigprocmask(SIG_SETMASK, &empty, NULL);
}

static int decimal(const char *text, uint32_t *value) {
    if (!*text) return -1;
    uint64_t result = 0;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '9') return -1;
        result = result * 10 + (unsigned int)(*p - '0');
        if (result > INT_MAX) return -1;
    }
    *value = (uint32_t)result;
    return 0;
}

static int check_control(void) {
    int type;
    socklen_t size = sizeof(type);
    if (getsockopt(CONTROL_FD, SOL_SOCKET, SO_TYPE, &type, &size) < 0) return -1;
    if (type != SOCK_SEQPACKET) { errno = EPROTOTYPE; return -1; }
    struct sockaddr_un address;
    size = sizeof(address);
    if (getsockname(CONTROL_FD, (struct sockaddr *)&address, &size) < 0) return -1;
    if (address.sun_family != AF_UNIX || size != sizeof(sa_family_t)) {
        errno = EINVAL; return -1;
    }
    size = sizeof(address);
    if (getpeername(CONTROL_FD, (struct sockaddr *)&address, &size) < 0) return -1;
    if (address.sun_family != AF_UNIX || size != sizeof(sa_family_t)) {
        errno = EINVAL; return -1;
    }
    return fcntl(CONTROL_FD, F_SETFD, FD_CLOEXEC);
}

static int remove_inherited_descriptors(void) {
    if (syscall(SYS_close_range, 4u, ~0u, 0) < 0) return -1;
    close(0); close(1); close(2);
    int fd = open("/dev/null", O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    if (fd != 0) { if (fd >= 0) close(fd); errno = EBADF; return -1; }
    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISCHR(st.st_mode) || st.st_rdev != makedev(1, 3)) {
        errno = ENODEV; return -1;
    }
    if (dup2(fd, 1) < 0 || dup2(fd, 2) < 0) return -1;
    return 0;
}

static int64_t milliseconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) return -1;
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int send_startup(int fd, int error, int master) {
    return aegis_send_reply(fd, error ? AEGIS_ERROR : AEGIS_STARTED, 0,
                            error, 0, 0, master);
}

static _Noreturn void child_failed(int error) {
    (void)send_startup(CONTROL_FD, error > 0 ? error : EIO, -1);
    _exit(127);
}

static _Noreturn void shell_child(int startup, char *const argv[]) {
    /* Replace the broker endpoint before anything could execute as the user.
     * Only this CLOEXEC startup channel and /dev/null survive the descriptor purge. */
    if (dup3(startup, CONTROL_FD, O_CLOEXEC) < 0) _exit(127);
    if (syscall(SYS_close_range, 4u, ~0u, 0) < 0) child_failed(errno);
    if (reset_signals() < 0 || aegis_limit_shell() < 0 || setsid() < 0
            || chdir("/home/user") < 0) child_failed(errno);
    int master = open("/dev/pts/ptmx", O_RDWR | O_NOCTTY | O_CLOEXEC | O_NOFOLLOW);
    if (master < 0) child_failed(errno);
    unsigned int number;
    int locked = 0;
    if (ioctl(master, TIOCGPTN, &number) < 0 || ioctl(master, TIOCSPTLCK, &locked) < 0)
        child_failed(errno);
    char path[64];
    int length = snprintf(path, sizeof(path), "/dev/pts/%u", number);
    if (length < 0 || (size_t)length >= sizeof(path)) child_failed(EOVERFLOW);
    int slave = open(path, O_RDWR | O_NOCTTY | O_CLOEXEC | O_NOFOLLOW);
    if (slave < 0) child_failed(errno);
    struct stat st;
    if (fstat(slave, &st) < 0) child_failed(errno);
    if (!S_ISCHR(st.st_mode) || st.st_uid != 1000) child_failed(EPERM);
    if (ioctl(slave, TIOCSCTTY, 0) < 0) child_failed(errno);
    for (int fd = 0; fd < 3; fd++) if (dup2(slave, fd) < 0) child_failed(errno);
    close(slave);
    if (send_startup(CONTROL_FD, 0, master) < 0) _exit(127);
    close(master);
    char *environment[] = {
        "HOME=/home/user", "USER=runtime", "LOGNAME=runtime", "SHELL=/bin/bash",
        "PATH=/usr/local/bin:/usr/bin:/bin", "LANG=C.UTF-8", "TERM=xterm-256color",
        "XDG_RUNTIME_DIR=/run/user/1000", NULL,
    };
    execve(argv[0], argv, environment);
    child_failed(errno);
}

static int await_startup(int fd) {
    int master = -1;
    int64_t now = milliseconds();
    if (now < 0) return -1;
    int64_t deadline = now + STARTUP_MILLISECONDS;
    for (;;) {
        if (stopping) { errno = ECANCELED; break; }
        now = milliseconds();
        if (now < 0) break;
        int64_t remaining = deadline - now;
        if (remaining <= 0) { errno = ETIMEDOUT; break; }
        struct timespec timeout = {remaining / 1000, (remaining % 1000) * 1000000};
        struct pollfd polls[] = {
            {.fd = fd, .events = POLLIN | POLLRDHUP},
            {.fd = CONTROL_FD, .events = POLLRDHUP},
        };
        int ready = ppoll(polls, 2, &timeout, wait_mask);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) break;
        if (polls[1].revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL)) {
            stopping = 1; errno = EPIPE; break;
        }
        if (!polls[0].revents) continue;
        struct aegis_runtime_reply reply;
        int received_fd;
        ssize_t n = aegis_receive(fd, &reply, sizeof(reply), &received_fd);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n < 0) break;
        if (n == 0 && received_fd < 0 && master >= 0) return master;
        if (n != (ssize_t)sizeof(reply) || reply.magic != AEGIS_RUNTIME_MAGIC
                || reply.version != AEGIS_RUNTIME_VERSION || reply.request_id
                || reply.pid || reply.wait_status || reply.reserved) {
            if (received_fd >= 0) close(received_fd);
            errno = EPROTO; break;
        }
        if (reply.event == AEGIS_ERROR && reply.error > 0 && received_fd < 0) {
            errno = reply.error; break;
        }
        if (reply.event != AEGIS_STARTED || reply.error || received_fd < 0 || master >= 0) {
            if (received_fd >= 0) close(received_fd);
            errno = EPROTO; break;
        }
        master = received_fd;
    }
    int saved = errno;
    if (master >= 0) close(master);
    errno = saved;
    return -1;
}

static int start_shell(char *const argv[], pid_t *pid) {
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair) < 0) return -1;
    *pid = fork();
    if (*pid == 0) shell_child(pair[1], argv);
    int saved = errno;
    close(pair[1]);
    if (*pid < 0) { close(pair[0]); errno = saved; return -1; }
    int master = await_startup(pair[0]);
    saved = errno;
    close(pair[0]);
    if (master < 0) {
        /* Do not wait indefinitely for an uninterruptible child. PID1 reaps it
         * in the normal loop; context termination also kills every descendant. */
        if (kill(*pid, SIGKILL) < 0 && errno != ESRCH) stopping = 1;
        errno = saved;
    }
    return master;
}

static int reap(struct shell shells[AEGIS_RUNTIME_MAX_SHELLS]) {
    /* Bounded work keeps the control channel responsive under rapid exits. */
    for (unsigned int count = 0; count < 256; count++) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid == 0 || (pid < 0 && errno == ECHILD)) return 0;
        if (pid < 0 && errno == EINTR) continue;
        if (pid < 0) return -1;
        for (size_t i = 0; i < AEGIS_RUNTIME_MAX_SHELLS; i++) {
            if (shells[i].pid != pid) continue;
            if (aegis_send_reply(CONTROL_FD, AEGIS_EXITED, shells[i].request_id,
                                0, pid, status, -1) < 0) return -1;
            shells[i].pid = 0;
            break;
        }
    }
    return 0;
}

static int supervise(void) {
    struct shell shells[AEGIS_RUNTIME_MAX_SHELLS] = {{0}};
    uint64_t previous_id = 0;
    while (!stopping) {
        if (reap(shells) < 0) return 74;
        struct pollfd control = {.fd = CONTROL_FD, .events = POLLIN | POLLRDHUP};
        struct timespec timeout = {1, 0};
        int ready = ppoll(&control, 1, &timeout, wait_mask);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) return 74;
        if (stopping) break;
        if (control.revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL)) return 74;
        if (!(control.revents & POLLIN)) continue;
        char packet[AEGIS_RUNTIME_MAX_PACKET];
        int passed_fd;
        ssize_t length = aegis_receive(CONTROL_FD, packet, sizeof(packet), &passed_fd);
        if (length < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (passed_fd >= 0) { close(passed_fd); return 74; }
        if (length <= 0) return 74;
        struct aegis_runtime_request request;
        char *argv[AEGIS_RUNTIME_MAX_ARGS + 1];
        if (aegis_parse_request(packet, (size_t)length, previous_id, &request, argv) < 0) return 74;
        previous_id = request.request_id;
        if (request.operation == AEGIS_STOP) return 0;
        size_t slot = 0;
        while (slot < AEGIS_RUNTIME_MAX_SHELLS && shells[slot].pid) slot++;
        if (slot == AEGIS_RUNTIME_MAX_SHELLS) {
            if (aegis_send_reply(CONTROL_FD, AEGIS_ERROR, request.request_id,
                                EBUSY, 0, 0, -1) < 0) return 74;
            continue;
        }
        pid_t pid;
        int master = start_shell(argv, &pid);
        if (stopping) { if (master >= 0) close(master); break; }
        if (master < 0) {
            if (aegis_send_reply(CONTROL_FD, AEGIS_ERROR, request.request_id,
                                errno, 0, 0, -1) < 0) return 74;
            continue;
        }
        shells[slot] = (struct shell){ .pid = pid, .request_id = request.request_id };
        int sent = aegis_send_reply(CONTROL_FD, AEGIS_STARTED, request.request_id,
                                    0, pid, 0, master);
        close(master);
        if (sent < 0) return 74;
    }
    return 0;
}

int main(int argc, char **argv) {
    uint32_t user_id, serial;
    /* Reject a direct invocation in Android's host PID namespace before
     * touching descriptors, capabilities, signals or user IDs. No bypass flag. */
    if (argc != 3 || decimal(argv[1], &user_id) < 0 || decimal(argv[2], &serial) < 0
            || aegis_check_context(user_id) < 0) return 78;
    if (check_control() < 0 || remove_inherited_descriptors() < 0 || clearenv() < 0) return 78;
    umask(077);
    sigset_t empty;
    if (configure_signals(&empty) < 0 || aegis_limit_supervisor() < 0
            || aegis_install_filter() < 0) return 78;
    if (aegis_send_reply(CONTROL_FD, AEGIS_READY, ((uint64_t)user_id << 32) | serial,
                        0, 1, 0, -1) < 0) return 74;
    return supervise();
}
