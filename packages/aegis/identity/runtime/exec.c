#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "exec.h"
#include "control.h"
#include "uid_layout.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_WAIT_NS UINT64_C(10000000000)
struct command { uint64_t id; int pid, exited, status; };
struct aegis_exec {
    pid_t process;
    uint32_t user;
    int channel;
    uint64_t sequence;
    struct command commands[AEGIS_RUNTIME_MAX_SHELLS];
};

static int fail(int error) { errno = error; return -1; }
static int root_main(void) {
    uid_t real, effective, saved;
    gid_t greal, geffective, gsaved;
    if (getresuid(&real, &effective, &saved) < 0
            || getresgid(&greal, &geffective, &gsaved) < 0) return -1;
    if (real || effective || saved || greal || geffective || gsaved
            || getgroups(0, NULL) != 0 || syscall(SYS_getpid) != syscall(SYS_gettid))
        return fail(EPERM);
    return 0;
}
static int owned(struct aegis_exec *owner) {
    if (!owner) return fail(EINVAL);
    if (owner->process != (pid_t)syscall(SYS_getpid)) return fail(EPERM);
    return root_main();
}
static int poison(struct aegis_exec *owner, int error) {
    if (owner->channel >= 0) close(owner->channel);
    owner->channel = -1;
    return fail(error);
}
static int time_left(uint64_t deadline) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) < 0) return -1;
    uint64_t now = (uint64_t)value.tv_sec * UINT64_C(1000000000) + value.tv_nsec;
    if (deadline > INT64_MAX || (deadline > now && deadline - now > MAX_WAIT_NS))
        return fail(EINVAL);
    if (deadline <= now) return fail(ETIMEDOUT);
    // Round down; no wait may extend the caller's admission budget.
    int millis = (int)((deadline - now) / 1000000);
    return millis ? millis : fail(ETIMEDOUT);
}
static int await(struct aegis_exec *owner, short events, uint64_t deadline) {
    for (;;) {
        int millis = time_left(deadline);
        if (millis < 0) return -1;
        struct pollfd wait = {owner->channel, events | POLLRDHUP, 0};
        int ready = poll(&wait, 1, millis);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) return -1;
        if (!ready) return fail(ETIMEDOUT);
        if (wait.revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL)) return fail(EPIPE);
        if (wait.revents & events) return 0;
    }
}

int aegis_exec_create(int channel, uint32_t user, struct aegis_exec **output) {
    if (!output || *output || user < 10 || user >= 21473) return fail(EINVAL);
    if (root_main() < 0) return -1;
    int type;
    socklen_t length = sizeof(type);
    if (getsockopt(channel, SOL_SOCKET, SO_TYPE, &type, &length) < 0) return -1;
    if (length != sizeof(type) || type != SOCK_SEQPACKET) return fail(EPROTOTYPE);
    struct sockaddr_un address;
    length = sizeof(address);
    if (getsockname(channel, (struct sockaddr *)&address, &length) < 0) return -1;
    if (address.sun_family != AF_UNIX || length != sizeof(sa_family_t)) return fail(EPERM);
    length = sizeof(address);
    if (getpeername(channel, (struct sockaddr *)&address, &length) < 0) return -1;
    if (address.sun_family != AF_UNIX || length != sizeof(sa_family_t)) return fail(EPERM);
    struct aegis_exec *owner = calloc(1, sizeof(*owner));
    if (!owner) return -1;
    owner->channel = fcntl(channel, F_DUPFD_CLOEXEC, 3);
    if (owner->channel < 0) { int saved = errno; free(owner); return fail(saved); }
    owner->process = (pid_t)syscall(SYS_getpid);
    owner->user = user;
    *output = owner;
    return 0;
}

int aegis_exec_healthy(struct aegis_exec *owner) {
    if (owned(owner) < 0) return -1;
    return owner->channel >= 0 ? 0 : fail(EPIPE);
}

int aegis_exec_close(struct aegis_exec **output) {
    if (!output || !*output) return fail(EINVAL);
    struct aegis_exec *owner = *output;
    if (owned(owner) < 0) return -1;
    if (owner->channel >= 0) close(owner->channel);
    free(owner); *output = NULL;
    return 0;
}

static int receive(struct aegis_exec *owner, struct aegis_runtime_reply *reply, int *fd) {
    ssize_t length = aegis_receive(owner->channel, reply, sizeof(*reply), fd);
    if (length < 0) return -1;
    if (!length) return fail(EPIPE);
    if (length != (ssize_t)sizeof(*reply) || reply->magic != AEGIS_RUNTIME_MAGIC
            || reply->version != AEGIS_RUNTIME_VERSION || !reply->request_id || reply->reserved)
        return fail(EPROTO);
    return 0;
}
static int exited(struct aegis_exec *owner, const struct aegis_runtime_reply *reply, int fd) {
    if (fd >= 0 || reply->event != AEGIS_EXITED || reply->error || reply->pid <= 1
            || (reply->wait_status & ~0xffff)
            || (!WIFEXITED(reply->wait_status) && !WIFSIGNALED(reply->wait_status)))
        return fail(EPROTO);
    for (unsigned i = 0; i < AEGIS_RUNTIME_MAX_SHELLS; ++i) {
        struct command *entry = &owner->commands[i];
        if (entry->id != reply->request_id) continue;
        if (entry->pid != reply->pid || entry->exited) return fail(EPROTO);
        entry->exited = 1; entry->status = reply->wait_status;
        return 0;
    }
    return fail(EPROTO);
}
static int drain(struct aegis_exec *owner) {
    // At most one exit per occupied slot is valid. Extra traffic is a protocol failure.
    for (unsigned i = 0; i <= AEGIS_RUNTIME_MAX_SHELLS; ++i) {
        struct aegis_runtime_reply reply;
        int fd = -1;
        int result = receive(owner, &reply, &fd), error = errno;
        if (result < 0 && (error == EAGAIN || error == EWOULDBLOCK)) return 0;
        if (result == 0) { result = exited(owner, &reply, fd); error = errno; }
        if (fd >= 0) close(fd);
        if (result < 0) return poison(owner, error);
    }
    return poison(owner, EPROTO);
}
static int private_pty(int fd, uint32_t user) {
    struct stat st;
    struct statfs fs;
    unsigned int number;
    if (fd < 0) return fail(EPROTO);
    if (fstat(fd, &st) < 0 || fstatfs(fd, &fs) < 0) return -1;
    int flags = fcntl(fd, F_GETFL), descriptor_flags = fcntl(fd, F_GETFD);
    if (flags < 0 || descriptor_flags < 0) return -1;
    if (!S_ISCHR(st.st_mode) || st.st_rdev != makedev(5, 2)
            || fs.f_type != DEVPTS_SUPER_MAGIC
            || (flags & O_ACCMODE) != O_RDWR
            || !(descriptor_flags & FD_CLOEXEC)) return fail(EPROTO);
    if (ioctl(fd, TIOCGPTN, &number) < 0) return -1;
    int slave = ioctl(fd, TIOCGPTPEER, O_RDONLY | O_NOCTTY | O_CLOEXEC | O_NONBLOCK);
    if (slave < 0) return -1;
    int result = fstat(slave, &st), saved = errno;
    close(slave);
    if (result < 0) return fail(saved);
    uint32_t uid = UINT32_MAX;
    for (unsigned i = 0; i < sizeof(aegis_uid_extents) / sizeof(aegis_uid_extents[0]); ++i) {
        const struct aegis_uid_extent *row = &aegis_uid_extents[i];
        if (1000u >= row->inside && 1000u - row->inside < row->count)
            uid = user * AEGIS_PER_USER_RANGE + row->app_id + 1000u - row->inside;
    }
    if (uid == UINT32_MAX) return fail(EPROTO);
    return S_ISCHR(st.st_mode) && st.st_uid == uid && st.st_gid == uid ? 0 : fail(EPROTO);
}

int aegis_exec_start(struct aegis_exec *owner, size_t argc, const char *const *argv,
                     uint64_t deadline, uint64_t *command, int *master) {
    if (!command || *command || !master || *master != -1 || !argv
            || !argc || argc > AEGIS_RUNTIME_MAX_ARGS) return fail(EINVAL);
    if (aegis_exec_healthy(owner) < 0 || time_left(deadline) < 0) return -1;
    char packet[AEGIS_RUNTIME_MAX_PACKET];
    size_t length = sizeof(struct aegis_runtime_request);
    for (size_t i = 0; i < argc; ++i) {
        if (!argv[i] || (!i && argv[i][0] != '/')) return fail(EINVAL);
        size_t bytes = strnlen(argv[i], sizeof(packet) - length);
        if (bytes == sizeof(packet) - length) return fail(E2BIG);
        memcpy(packet + length, argv[i], bytes + 1); length += bytes + 1;
    }
    if (drain(owner) < 0) return -1;
    unsigned slot = 0;
    while (slot < AEGIS_RUNTIME_MAX_SHELLS && owner->commands[slot].id) ++slot;
    if (slot == AEGIS_RUNTIME_MAX_SHELLS) return fail(EBUSY);
    if (owner->sequence == UINT64_MAX) return fail(EOVERFLOW);
    uint64_t id = ++owner->sequence;
    struct aegis_runtime_request request = {
        .magic = AEGIS_RUNTIME_MAGIC, .version = AEGIS_RUNTIME_VERSION,
        .operation = AEGIS_EXEC, .request_id = id,
        .payload_bytes = (uint32_t)(length - sizeof(request)), .argc = (uint32_t)argc,
    };
    memcpy(packet, &request, sizeof(request));
    for (;;) {
        if (await(owner, POLLOUT, deadline) < 0) return poison(owner, errno);
        ssize_t sent = send(owner->channel, packet, length, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (sent == (ssize_t)length) break;
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) continue;
        return poison(owner, sent < 0 ? errno : EIO);
    }
    for (;;) {
        if (await(owner, POLLIN, deadline) < 0) return poison(owner, errno);
        struct aegis_runtime_reply reply;
        int fd = -1;
        int result = receive(owner, &reply, &fd), error = errno;
        if (result < 0 && (error == EAGAIN || error == EWOULDBLOCK)) continue;
        if (result == 0 && reply.event == AEGIS_EXITED) {
            result = exited(owner, &reply, fd); error = errno;
            if (fd >= 0) close(fd);
            if (result < 0) return poison(owner, error);
            continue;
        }
        if (result == 0 && reply.request_id == id) {
            if (reply.event == AEGIS_ERROR && fd < 0 && reply.error > 0
                    && reply.error <= 4095 && !reply.pid && !reply.wait_status) return fail(reply.error);
            if (reply.event == AEGIS_STARTED && !reply.error && reply.pid > 1 && !reply.wait_status) {
                result = private_pty(fd, owner->user); error = errno;
                if (result == 0) { result = time_left(deadline); error = errno; }
                for (unsigned i = 0; result >= 0 && i < AEGIS_RUNTIME_MAX_SHELLS; ++i) {
                    if (owner->commands[i].id && !owner->commands[i].exited
                            && owner->commands[i].pid == reply.pid) { result = -1; error = EPROTO; }
                }
                if (result >= 0) {
                    owner->commands[slot] = (struct command){.id = id, .pid = reply.pid};
                    *master = fd; *command = id;
                    return 0;
                }
            } else { result = -1; error = EPROTO; }
        } else if (result == 0) { result = -1; error = EPROTO; }
        if (fd >= 0) close(fd);
        return poison(owner, error);
    }
}

int aegis_exec_result(struct aegis_exec *owner, uint64_t id, int *status) {
    if (!id || !status) return fail(EINVAL);
    if (aegis_exec_healthy(owner) < 0 || drain(owner) < 0) return -1;
    for (unsigned i = 0; i < AEGIS_RUNTIME_MAX_SHELLS; ++i) {
        struct command *entry = &owner->commands[i];
        if (entry->id != id) continue;
        if (!entry->exited) return fail(EAGAIN);
        *status = entry->status;
        memset(entry, 0, sizeof(*entry));
        return 0;
    }
    return fail(ENOENT);
}
