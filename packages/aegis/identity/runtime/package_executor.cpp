#include "package_executor.h"
#include "package_execution_protocol.h"
#include "namespace.h"
#include "memory_group.h"
#include <android-base/unique_fd.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/memfd.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <new>
using android::base::unique_fd;
namespace aegis {
struct PackageExecutor {
    pid_t process = 0;
    unique_fd stage, candidate, devices, channel, config;
    aegis_memory_group* group = nullptr;
    aegis_namespace* context = nullptr;
    aegis_package_execution_request request = {};
    bool ready = false, spawned = false;
};
namespace {
int Fail(int error) { errno = error;return -1; }
uint64_t Now() {
    timespec now = {};if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return 0;
    return static_cast<uint64_t>(now.tv_sec) * UINT64_C(1000000000) + now.tv_nsec;
}
int Left(uint64_t deadline) {
    uint64_t now = Now();if (!now) return -1;
    if (deadline <= now) return 0;
    return static_cast<int>((deadline - now) / UINT64_C(1000000));
}
bool Owned(PackageExecutor* p) {
    if (!p || p->process != syscall(SYS_getpid)) { Fail(EPERM);return false; }
    return aegis_namespace_check_broker() == 0;
}
bool Encode(const PackageExecution& plan, aegis_package_execution_request* r) {
    *r = {};r->magic = AEGIS_PACKAGE_EXEC_MAGIC;r->version = AEGIS_PACKAGE_EXEC_VERSION;
    r->user = plan.requester;r->serial = plan.serial;r->job = plan.job;
    r->kind = plan.archives ? AEGIS_PACKAGE_ARCHIVES : AEGIS_PACKAGE_REMOVE;
    if (plan.plan_sha256.size() != 64 || plan.items.empty() || plan.items.size() > AEGIS_PACKAGE_EXEC_ITEMS) return false;
    memcpy(r->plan, plan.plan_sha256.data(), 64);r->count = plan.items.size();
    for (size_t i = 0; i < plan.items.size(); i++) {
        if (plan.items[i].size() >= AEGIS_PACKAGE_EXEC_NAME || plan.items[i].find('\0') != std::string::npos) return false;
        memcpy(r->items[i], plan.items[i].data(), plan.items[i].size());
    }
    return aegis_package_execution_valid(r);
}
int Reply(PackageExecutor* p, uint32_t phase, int timeout, aegis_package_execution_reply* out) {
    pollfd ready = {p->channel.get(), POLLIN, 0};
    int polled = poll(&ready, 1, timeout);
    if (polled < 0) return -1;
    if (!polled) return Fail(ETIMEDOUT);
    if (!(ready.revents & POLLIN)) return Fail(EPIPE);
    alignas(cmsghdr) char controls[CMSG_SPACE(16 * sizeof(int))] = {};
    iovec io = {out, sizeof(*out)};
    msghdr msg = {};msg.msg_iov = &io;msg.msg_iovlen = 1;
    msg.msg_control = controls;msg.msg_controllen = sizeof(controls);
    ssize_t n = recvmsg(p->channel.get(), &msg, MSG_DONTWAIT | MSG_TRUNC | MSG_CMSG_CLOEXEC);
    bool extra = false;
    for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
        extra = true;
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS && c->cmsg_len >= CMSG_LEN(0)) {
            for (size_t i = 0; i < (c->cmsg_len - CMSG_LEN(0)) / sizeof(int); i++) {
                int fd;memcpy(&fd, CMSG_DATA(c) + i * sizeof(fd), sizeof(fd));close(fd);
            }
        }
    }
    if (n != static_cast<ssize_t>(sizeof(*out)) || extra || (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))
            || out->magic != AEGIS_PACKAGE_EXEC_MAGIC || out->version != AEGIS_PACKAGE_EXEC_VERSION
            || out->user != p->request.user || out->serial != p->request.serial || out->job != p->request.job
            || memcmp(out->plan, p->request.plan, sizeof(out->plan)) || out->phase != phase
            || !aegis_package_zero(out->reserved, sizeof(out->reserved)) || out->status > 255
            || out->error > 4095 || (phase == AEGIS_PACKAGE_EXEC_READY && out->status)) return Fail(EPROTO);
    return 0;
}
}
int PackageExecutionCheck(const PackageExecution& plan) {
    aegis_package_execution_request r;return Encode(plan, &r) ? 0 : Fail(EINVAL);
}
int PackageExecutorCancel(PackageExecutor* p) {
    if (!Owned(p)) return -1;
    int error = 0;
    if (p->context && aegis_namespace_stop(p->context) < 0) error = errno;
    if (p->group && aegis_memory_group_kill_and_wait(p->group, 0) < 0 && errno != ETIMEDOUT && !error) error = errno;
    return error ? Fail(error) : 0;
}
int PackageExecutorStart(int groups, int stage, int candidate, int helper,
                         const PackageExecution& plan, uint64_t deadline, PackageExecutor** output) {
    if (!output || *output) return Fail(EINVAL);
    if (aegis_namespace_check_broker() < 0) return -1;
    uint64_t now = Now();if (!now) return -1;
    if (deadline <= now || deadline - now > UINT64_C(10000000000)) return Fail(ETIMEDOUT);
    aegis_package_execution_request request;
    if (!Encode(plan, &request)) return Fail(EINVAL);
    struct stat st;int flags = fcntl(stage, F_GETFL);
    if (flags < 0 || fstat(stage, &st) < 0) return -1;
    if (st.st_mode != (S_IFDIR | 0700) || st.st_uid || st.st_gid
            || (flags & (O_ACCMODE | O_PATH)) != O_RDONLY) return Fail(EPERM);
    auto* p = new(std::nothrow) PackageExecutor;if (!p) return Fail(ENOMEM);
    p->process = syscall(SYS_getpid);p->request = request;
    *output = p; // No fallible operation may abandon partial ownership below.
    p->stage.reset(fcntl(stage, F_DUPFD_CLOEXEC, 4));
    p->candidate.reset(fcntl(candidate, F_DUPFD_CLOEXEC, 4));
    if (!p->stage.ok() || !p->candidate.ok()) return -1;
    int pair[2];if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair) < 0) return -1;
    p->channel.reset(pair[0]);unique_fd child(pair[1]);
    if (aegis_memory_group_create(groups, plan.requester, plan.serial, &p->group) < 0
            || aegis_namespace_create_limited(plan.requester, plan.serial, helper, child.get(), p->group, &p->context) < 0) return -1;
    p->spawned = true;
    int left = Left(deadline);if (left <= 0) return Fail(ETIMEDOUT);
    if (aegis_namespace_prepare_package_for(p->context, left) < 0
            || aegis_namespace_map_candidate(p->context, p->candidate.get()) < 0) return -1;
    p->devices.reset(aegis_namespace_devices_mount(p->context));
    p->config.reset(syscall(SYS_memfd_create, "aegis-package-execution", MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (!p->devices.ok() || !p->config.ok()) return -1;
    if (write(p->config.get(), &request, sizeof(request)) != static_cast<ssize_t>(sizeof(request))) return Fail(EIO);
    if (fcntl(p->config.get(), F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0) return -1;
    int fds[] = {p->candidate.get(), p->devices.get(), p->config.get()};
    if (Left(deadline) <= 0) return Fail(ETIMEDOUT);
    if (aegis_package_execution_send(p->channel.get(), plan.requester, plan.serial, plan.job, fds) < 0
            || aegis_namespace_resume(p->context) < 0) return -1;
    left = Left(deadline);if (left <= 0) return Fail(ETIMEDOUT);
    aegis_package_execution_reply reply;
    if (Reply(p, AEGIS_PACKAGE_EXEC_READY, left, &reply) < 0) return -1;
    if (reply.error) return Fail(reply.error);
    if (Left(deadline) <= 0) return Fail(ETIMEDOUT);
    p->ready = true;return 0;
}
int PackageExecutorFinish(PackageExecutor** pointer, bool cancel, int timeout,
                          PackageExecutionResult* result) {
    if (!pointer || !*pointer || !result || timeout < 0 || timeout > 10000) return Fail(EINVAL);
    auto* p = *pointer;if (!Owned(p)) return -1;
    uint64_t now = Now();if (!now) return -1;
    uint64_t deadline = now + static_cast<uint64_t>(timeout) * UINT64_C(1000000);
    if (cancel) (void)PackageExecutorCancel(p);
    int left = Left(deadline);if (left < 0) return -1;
    aegis_child_exit exited = {};
    if (p->context && aegis_namespace_wait(p->context, left, &exited) < 0) return -1;
    left = Left(deadline);if (left < 0) return -1;
    if (p->group && aegis_memory_group_kill_and_wait(p->group, left) < 0) return -1;
    if (p->group && aegis_memory_group_remove(&p->group) < 0) return -1;
    PackageExecutionResult observed;observed.error = EIO;
    if (!p->spawned) { observed.outcome = PackageExecutionOutcome::Failed;observed.error = ECANCELED; }
    else if (p->ready && exited.code == CLD_EXITED && exited.status == 0) {
        aegis_package_execution_reply reply;
        if (Reply(p, AEGIS_PACKAGE_EXEC_DONE, 0, &reply) == 0) {
            observed.status = reply.status;observed.error = reply.error;
            observed.outcome = reply.status || reply.error ? PackageExecutionOutcome::Failed : PackageExecutionOutcome::NeedsValidation;
        } else observed.error = errno;
    }
    if (p->context) aegis_namespace_release(p->context);
    delete p;*pointer = nullptr;*result = observed;return 0;
}
} // namespace aegis
