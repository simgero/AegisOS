#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "package_execution_protocol.h"
#include "package_request.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
_Static_assert(sizeof(struct aegis_package_execution_request) <= AEGIS_PACKAGE_REQUEST_MAX_BYTES, "bounded request ABI");
_Static_assert(sizeof(struct aegis_package_execution_reply) == 104, "reply ABI");
struct transfer { uint32_t magic, version, user, serial; uint64_t job; };
_Static_assert(sizeof(struct transfer) == 24, "transfer ABI");
static int fail(int error) { errno = error; return -1; }
static int channel(int fd) {
    int type = 0; socklen_t n = sizeof(type);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &n) < 0) return -1;
    if (type != SOCK_SEQPACKET) return fail(EPROTOTYPE);
    struct sockaddr_un address;
    n = sizeof(address);
    if (getsockname(fd, (struct sockaddr *)&address, &n) < 0) return -1;
    if (n != sizeof(sa_family_t) || address.sun_family != AF_UNIX) return fail(EPERM);
    n = sizeof(address);
    if (getpeername(fd, (struct sockaddr *)&address, &n) < 0) return -1;
    return n == sizeof(sa_family_t) && address.sun_family == AF_UNIX ? 0 : fail(EPERM);
}
int aegis_package_execution_send(int fd, uint32_t user, uint32_t serial, uint64_t job,
                                 const int fds[AEGIS_PACKAGE_EXEC_FDS]) {
    if (!fds || user < 10 || user >= 21473 || serial > INT32_MAX || !job || job > INT64_MAX)
        return fail(EINVAL);
    if (channel(fd) < 0) return -1;
    struct transfer data = {AEGIS_PACKAGE_EXEC_MAGIC, AEGIS_PACKAGE_EXEC_VERSION, user, serial, job};
    struct iovec io = {&data, sizeof(data)};
    union { struct cmsghdr alignment; char bytes[CMSG_SPACE(3 * sizeof(int))]; } extra = {0};
    struct msghdr msg = {.msg_iov = &io, .msg_iovlen = 1,
        .msg_control = extra.bytes, .msg_controllen = sizeof(extra.bytes)};
    struct cmsghdr *c = CMSG_FIRSTHDR(&msg);
    c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(3 * sizeof(int));
    memcpy(CMSG_DATA(c), fds, 3 * sizeof(int));
    ssize_t n;
    do { n = sendmsg(fd, &msg, MSG_DONTWAIT | MSG_NOSIGNAL); } while (n < 0 && errno == EINTR);
    return n == (ssize_t)sizeof(data) ? 0 : (n < 0 ? -1 : fail(EIO));
}
int aegis_package_execution_receive(int fd, uint32_t user, uint32_t serial,
                                    int fds[3], struct aegis_package_execution_request *request) {
    if (channel(fd) < 0) return -1;
    struct pollfd ready = {.fd = fd, .events = POLLIN | POLLRDHUP};
    int polled = poll(&ready, 1, 5000);
    if (polled < 0) return -1;
    if (!polled) return fail(ETIMEDOUT);
    // Queued authority is invalid after the owning broker disconnects.
    if (ready.revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL)) return fail(EPIPE);
    struct transfer data = {0};
    union { struct cmsghdr alignment; char bytes[CMSG_SPACE(16 * sizeof(int))]; } extra = {0};
    struct iovec io = {&data, sizeof(data)};
    struct msghdr msg = {.msg_iov = &io, .msg_iovlen = 1,
        .msg_control = extra.bytes, .msg_controllen = sizeof(extra.bytes)};
    ssize_t n = recvmsg(fd, &msg, MSG_DONTWAIT | MSG_TRUNC | MSG_CMSG_CLOEXEC);
    if (n < 0) return -1;
    int count = 0, headers = 0, bad = 0;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
        headers++;
        unsigned char *end = (unsigned char *)msg.msg_control + msg.msg_controllen;
        if (c->cmsg_len < CMSG_LEN(0) || c->cmsg_len > (size_t)(end - (unsigned char *)c)) {
            bad = 1;break;
        }
        if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS || c->cmsg_len < CMSG_LEN(0)) {
            bad = 1;continue;
        }
        size_t bytes = c->cmsg_len - CMSG_LEN(0);
        if (bytes % sizeof(int)) bad = 1;
        for (size_t i = 0; i < bytes / sizeof(int); i++) {
            int received; memcpy(&received, CMSG_DATA(c) + i * sizeof(int), sizeof(int));
            if (count < 3) fds[count++] = received; else { close(received); bad = 1; }
        }
    }
    if (n != (ssize_t)sizeof(data) || bad || headers != 1 || count != 3
            || (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))
            || data.magic != AEGIS_PACKAGE_EXEC_MAGIC || data.version != AEGIS_PACKAGE_EXEC_VERSION
            || data.user != user || data.serial != serial || !data.job || data.job > INT64_MAX)
        return fail(EPROTO);
    for (int i = 0; i < 3; i++) {
        int copy = fcntl(fds[i], F_DUPFD_CLOEXEC, 4);
        int saved = errno; close(fds[i]); fds[i] = copy;
        if (copy < 0) { errno = saved;return -1; }
    }
    if (aegis_package_request_readonly(fds[2], sizeof(*request)) < 0) return -1;
    struct stat st;
    if (fstat(fds[2], &st) < 0) return -1;
    if (!S_ISREG(st.st_mode) || st.st_size != (off_t)sizeof(*request)
            || fcntl(fds[2], F_GET_SEALS) != (F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL))
        return fail(EPERM);
    if (pread(fds[2], request, sizeof(*request), 0) != (ssize_t)sizeof(*request)) return fail(EIO);
    if (!aegis_package_execution_valid(request) || request->user != user
            || request->serial != serial || request->job != data.job) return fail(EPROTO);
    return 0;
}
