#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "context.h"
#include "control.h"
#include "memory_group.h"
#include "namespace.h"
#include "setup.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

struct aegis_context {
    pid_t owner;
    uint32_t user, serial;
    int channel, ready;
    struct aegis_namespace *namespace;
    struct aegis_memory_group *memory;
};

static int reject(int error) { errno = error; return -1; }

static int owned(struct aegis_context *context) {
    if (!context) return reject(EINVAL);
    if (context->owner != (pid_t)syscall(SYS_getpid)
            || getuid() || geteuid() || getgid() || getegid()) return reject(EPERM);
    return 0;
}

static int64_t now_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int remaining(int64_t deadline) {
    int64_t now = now_ms();
    if (now < 0) return -1;
    return now >= deadline ? 0 : (int)(deadline - now);
}

static void seal(struct aegis_context *context) {
    context->ready = 0;
    if (context->channel >= 0) close(context->channel);
    context->channel = -1;
}

static int await_ready(struct aegis_context *context, int64_t deadline) {
    for (;;) {
        int left = remaining(deadline);
        if (left < 0) return -1;
        if (!left) return reject(ETIMEDOUT);
        struct pollfd p = {.fd = context->channel, .events = POLLIN | POLLRDHUP};
        int result = poll(&p, 1, left);
        if (result < 0 && errno == EINTR) continue;
        if (result < 0) return -1;
        if (!result) return reject(ETIMEDOUT);
        if (p.revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL)) return reject(EPIPE);
        if (!(p.revents & POLLIN)) continue;
        struct aegis_runtime_reply reply;
        int fd = -1;
        ssize_t size = aegis_receive(context->channel, &reply, sizeof(reply), &fd);
        if (size < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        int saved = errno;
        if (fd >= 0) close(fd);
        if (size < 0) return reject(saved);
        if (size != (ssize_t)sizeof(reply) || fd >= 0
                || reply.magic != AEGIS_RUNTIME_MAGIC || reply.version != AEGIS_RUNTIME_VERSION
                || reply.event != AEGIS_READY || reply.error || reply.pid != 1
                || reply.request_id != (((uint64_t)context->user << 32) | context->serial)
                || reply.wait_status || reply.reserved) return reject(EPROTO);
        return 0;
    }
}

int aegis_context_start(uint32_t user, uint32_t serial, int parent_fd, int base_fd,
                        int setup_fd, int init_fd, int create_home,
                        struct aegis_context **output) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    uint64_t deadline = (uint64_t)now.tv_sec * UINT64_C(1000000000)
            + (uint64_t)now.tv_nsec + UINT64_C(10000000000);
    return aegis_context_start_until(user, serial, parent_fd, base_fd, setup_fd, init_fd,
                                     create_home, deadline, output);
}

int aegis_context_start_until(uint32_t user, uint32_t serial, int parent_fd, int base_fd,
                              int setup_fd, int init_fd, int create_home,
                              uint64_t deadline_ns, struct aegis_context **output) {
    if (!output || *output || user < 10 || user >= 21473 || serial > INT32_MAX
            || (create_home != 0 && create_home != 1)) return reject(EINVAL);
    if (getuid() || geteuid() || getgid() || getegid()) return reject(EPERM);
    if (fcntl(parent_fd, F_GETFD) < 0 || fcntl(base_fd, F_GETFD) < 0
            || fcntl(setup_fd, F_GETFD) < 0 || fcntl(init_fd, F_GETFD) < 0) return -1;
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    uint64_t start_ns = (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
    if (deadline_ns > INT64_MAX || deadline_ns <= start_ns
            || deadline_ns - start_ns > UINT64_C(10000000000)) return reject(EINVAL);
    int64_t deadline = (int64_t)(deadline_ns / 1000000); // Round DOWN, never extend the budget.
    if (remaining(deadline) <= 0) return reject(ETIMEDOUT);
    struct aegis_context *context = calloc(1, sizeof(*context));
    if (!context) return -1;
    context->owner = (pid_t)syscall(SYS_getpid);
    context->user = user;
    context->serial = serial;
    context->channel = -1;
    *output = context;
    int pair[2] = {-1, -1};
    int mounts[AEGIS_SETUP_FDS] = {-1, -1, -1, -1};
    if (aegis_memory_group_create(parent_fd, user, serial, &context->memory) < 0) goto fail;
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair) < 0) goto fail;
    context->channel = pair[0];
    pair[0] = -1;
    if (aegis_namespace_create_limited(user, serial, setup_fd, pair[1], context->memory,
                                       &context->namespace) < 0) goto fail;
    close(pair[1]); pair[1] = -1;
    if (aegis_namespace_prepare(context->namespace) < 0) goto fail;
    mounts[0] = aegis_namespace_base_mount(context->namespace, base_fd);
    if (mounts[0] < 0) goto fail;
    mounts[1] = aegis_namespace_home_mount(context->namespace, create_home);
    if (mounts[1] < 0) goto fail;
    mounts[2] = aegis_namespace_devices_mount(context->namespace);
    if (mounts[2] < 0) goto fail;
    mounts[3] = fcntl(init_fd, F_DUPFD_CLOEXEC, 4);
    if (mounts[3] < 0) goto fail;
    if (aegis_send_setup(context->channel, user, serial, mounts) < 0) goto fail;
    // No CE, detached-mount or code descriptors persist in the host context.
    // The queued copies belong to the helper endpoint until consumed/closed.
    for (unsigned i = 0; i < AEGIS_SETUP_FDS; i++) { close(mounts[i]); mounts[i] = -1; }
    if (remaining(deadline) <= 0) { errno = ETIMEDOUT; goto fail; }
    if (aegis_namespace_resume(context->namespace) < 0
            || await_ready(context, deadline) < 0) goto fail;
    context->ready = 1;
    if (aegis_context_channel(context) < 0) goto fail;
    return 0;
fail:;
    int error = errno;
    for (unsigned i = 0; i < AEGIS_SETUP_FDS; i++) if (mounts[i] >= 0) close(mounts[i]);
    for (unsigned i = 0; i < 2; i++) if (pair[i] >= 0) close(pair[i]);
    seal(context);
    if (context->namespace) (void)aegis_namespace_stop(context->namespace);
    return reject(error);
}

int aegis_context_channel(struct aegis_context *context) {
    if (owned(context) < 0) return -1;
    if (!context->ready || context->channel < 0 || !context->namespace) return reject(EAGAIN);
    struct aegis_child_exit ended;
    if (aegis_namespace_wait(context->namespace, 0, &ended) < 0) {
        if (errno == ETIMEDOUT) return context->channel;
        int saved = errno;
        seal(context);
        return reject(saved);
    }
    seal(context);
    return reject(EPIPE);
}

int aegis_context_stop(struct aegis_context **output, int timeout_ms) {
    if (!output || !*output || timeout_ms < 0 || timeout_ms > 10000) return reject(EINVAL);
    struct aegis_context *context = *output;
    if (owned(context) < 0) return -1;
    seal(context);
    // Even if the clock or cgroup operation fails, request child termination.
    // A failed signal is never completion; the pidfd wait below is mandatory.
    if (context->namespace) (void)aegis_namespace_stop(context->namespace);
    int64_t start = now_ms();
    if (start < 0) return -1;
    int64_t deadline = start + timeout_ms;
    int error = 0, left = remaining(deadline);
    if (left < 0) return -1;
    if (context->memory && aegis_memory_group_kill_and_wait(context->memory, left) < 0)
        error = errno;
    left = remaining(deadline);
    if (left < 0) return -1;
    if (context->namespace) {
        struct aegis_child_exit ended;
        if (aegis_namespace_wait(context->namespace, left, &ended) < 0) {
            if (!error) error = errno;
        } else {
            aegis_namespace_release(context->namespace);
            context->namespace = NULL;
        }
    }
    if (error) return reject(error);
    if (context->memory && aegis_memory_group_remove(&context->memory) < 0) return -1;
    free(context);
    *output = NULL;
    return 0;
}
