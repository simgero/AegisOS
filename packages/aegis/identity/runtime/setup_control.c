#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "setup.h"
#include "control.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

_Static_assert(sizeof(struct aegis_setup_record) == 24, "Unexpected setup layout");

static int channel(int fd) {
    int type;
    socklen_t length = sizeof(type);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &length) < 0) return -1;
    if (type != SOCK_SEQPACKET) { errno = EPROTOTYPE; return -1; }
    struct sockaddr_un address;
    length = sizeof(address);
    if (getsockname(fd, (struct sockaddr *)&address, &length) < 0) return -1;
    if (length != sizeof(sa_family_t) || address.sun_family != AF_UNIX) {
        errno = EPERM; return -1;
    }
    length = sizeof(address);
    if (getpeername(fd, (struct sockaddr *)&address, &length) < 0) return -1;
    if (length != sizeof(sa_family_t) || address.sun_family != AF_UNIX) {
        errno = EPERM; return -1;
    }
    return 0;
}

int aegis_send_setup(int socket_fd, uint32_t user_id, uint32_t serial,
                     const int fds[AEGIS_SETUP_FDS]) {
    if (!fds || user_id < 10 || user_id >= 21473 || serial > INT32_MAX) {
        errno = EINVAL; return -1;
    }
    if (channel(socket_fd) < 0) return -1;
    for (unsigned i = 0; i < AEGIS_SETUP_FDS; i++) if (fcntl(fds[i], F_GETFD) < 0) return -1;
    for (unsigned role = AEGIS_SETUP_BASE; role <= AEGIS_SETUP_COMMIT; role++) {
        struct aegis_setup_record record = {
            .magic = AEGIS_SETUP_MAGIC, .version = AEGIS_SETUP_VERSION,
            .role = role, .user_id = user_id, .serial = serial,
        };
        struct iovec io = {.iov_base = &record, .iov_len = sizeof(record)};
        union { struct cmsghdr alignment; char bytes[CMSG_SPACE(sizeof(int))]; } ancillary = {0};
        struct msghdr message = {.msg_iov = &io, .msg_iovlen = 1};
        if (role != AEGIS_SETUP_COMMIT) {
            message.msg_control = ancillary.bytes;
            message.msg_controllen = sizeof(ancillary.bytes);
            struct cmsghdr *header = CMSG_FIRSTHDR(&message);
            header->cmsg_level = SOL_SOCKET; header->cmsg_type = SCM_RIGHTS;
            header->cmsg_len = CMSG_LEN(sizeof(int));
            memcpy(CMSG_DATA(header), &fds[role - 1], sizeof(int));
        }
        ssize_t sent;
        do { sent = sendmsg(socket_fd, &message, MSG_DONTWAIT | MSG_NOSIGNAL); }
        while (sent < 0 && errno == EINTR);
        if (sent != (ssize_t)sizeof(record)) {
            if (sent >= 0) errno = EIO;
            return -1;
        }
    }
    return 0;
}

static int64_t milliseconds(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) < 0) return -1;
    return (int64_t)value.tv_sec * 1000 + value.tv_nsec / 1000000;
}

int aegis_receive_setup(int socket_fd, uint32_t user_id, uint32_t serial,
                        int timeout_ms, int fds[AEGIS_SETUP_FDS]) {
    if (!fds || user_id < 10 || user_id >= 21473 || serial > INT32_MAX
            || timeout_ms <= 0 || timeout_ms > 5000) { errno = EINVAL; return -1; }
    for (unsigned i = 0; i < AEGIS_SETUP_FDS; i++) if (fds[i] != -1) { errno = EINVAL; return -1; }
    if (channel(socket_fd) < 0) return -1;
    int received[AEGIS_SETUP_FDS] = {-1, -1, -1, -1};
    int64_t start = milliseconds();
    if (start < 0) return -1;
    int64_t deadline = start + timeout_ms;
    for (unsigned role = AEGIS_SETUP_BASE; role <= AEGIS_SETUP_COMMIT;) {
        int64_t now = milliseconds();
        if (now < 0) goto fail;
        if (now >= deadline) { errno = ETIMEDOUT; goto fail; }
        struct pollfd poll_fd = {.fd = socket_fd, .events = POLLIN | POLLRDHUP};
        int ready = poll(&poll_fd, 1, (int)(deadline - now));
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) goto fail;
        if (ready == 0) { errno = ETIMEDOUT; goto fail; }
        /* A dead broker must not authorize execution through queued records. */
        if (poll_fd.revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL)) {
            errno = EPIPE; goto fail;
        }
        if (!(poll_fd.revents & POLLIN)) continue;
        struct aegis_setup_record record;
        int fd = -1;
        ssize_t n = aegis_receive(socket_fd, &record, sizeof(record), &fd);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n < 0) goto fail;
        if (n != (ssize_t)sizeof(record) || record.magic != AEGIS_SETUP_MAGIC
                || record.version != AEGIS_SETUP_VERSION || record.role != role
                || record.user_id != user_id || record.serial != serial
                || record.reserved[0] || record.reserved[1]
                || (role == AEGIS_SETUP_COMMIT ? fd >= 0 : fd < 0)) {
            if (fd >= 0) close(fd);
            errno = EPROTO; goto fail;
        }
        if (fd >= 0) {
            int copy = fcntl(fd, F_DUPFD_CLOEXEC, 4);
            int saved = errno;
            close(fd);
            if (copy < 0) { errno = saved; goto fail; }
            received[role - 1] = copy;
        }
        role++;
    }
    memcpy(fds, received, sizeof(received));
    return 0;
fail:;
    int saved = errno;
    for (unsigned i = 0; i < AEGIS_SETUP_FDS; i++) if (received[i] >= 0) close(received[i]);
    errno = saved;
    return -1;
}
