#include "control.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "The private runtime protocol currently requires little endian"
#endif
_Static_assert(sizeof(struct aegis_runtime_request) == 24, "Unexpected request layout");
_Static_assert(sizeof(struct aegis_runtime_reply) == 32, "Unexpected reply layout");

int aegis_parse_request(void *packet, size_t length, uint64_t previous_id,
                       struct aegis_runtime_request *request,
                       char *argv[AEGIS_RUNTIME_MAX_ARGS + 1]) {
    memset(request, 0, sizeof(*request));
    memset(argv, 0, sizeof(char *) * (AEGIS_RUNTIME_MAX_ARGS + 1));
    if (length < sizeof(*request) || length > AEGIS_RUNTIME_MAX_PACKET) return -EPROTO;
    memcpy(request, packet, sizeof(*request));
    if (request->magic != AEGIS_RUNTIME_MAGIC || request->version != AEGIS_RUNTIME_VERSION
            || request->request_id == 0 || request->request_id <= previous_id
            || request->payload_bytes != length - sizeof(*request)) return -EPROTO;
    if (request->operation == AEGIS_STOP) {
        return request->argc == 0 && request->payload_bytes == 0 ? 0 : -EPROTO;
    }
    if (request->operation != AEGIS_EXEC || request->argc == 0
            || request->argc > AEGIS_RUNTIME_MAX_ARGS) return -EPROTO;
    char *cursor = (char *)packet + sizeof(*request);
    char *end = (char *)packet + length;
    for (uint32_t i = 0; i < request->argc; i++) {
        if (cursor == end) return -EPROTO;
        char *terminator = memchr(cursor, '\0', (size_t)(end - cursor));
        if (!terminator) return -EPROTO;
        argv[i] = cursor;
        cursor = terminator + 1;
    }
    if (cursor != end || argv[0][0] != '/') return -EPROTO;
    return 0;
}

int aegis_send_reply(int socket_fd, uint16_t event, uint64_t request_id,
                     int error, int pid, int wait_status, int passed_fd) {
    struct aegis_runtime_reply reply = {
        .magic = AEGIS_RUNTIME_MAGIC, .version = AEGIS_RUNTIME_VERSION, .event = event,
        .request_id = request_id, .error = error, .pid = pid, .wait_status = wait_status,
        .reserved = 0,
    };
    struct iovec iov = {.iov_base = &reply, .iov_len = sizeof(reply)};
    union { struct cmsghdr alignment; char bytes[CMSG_SPACE(sizeof(int))]; } control = {0};
    struct msghdr message = {.msg_iov = &iov, .msg_iovlen = 1};
    if (passed_fd >= 0) {
        message.msg_control = control.bytes;
        message.msg_controllen = sizeof(control.bytes);
        struct cmsghdr *header = CMSG_FIRSTHDR(&message);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(header), &passed_fd, sizeof(passed_fd));
    }
    ssize_t sent;
    do { sent = sendmsg(socket_fd, &message, MSG_NOSIGNAL | MSG_DONTWAIT); } while (sent < 0 && errno == EINTR);
    if (sent == (ssize_t)sizeof(reply)) return 0;
    if (sent >= 0) errno = EIO;
    return -1;
}

ssize_t aegis_receive(int socket_fd, void *packet, size_t capacity, int *passed_fd) {
    union { struct cmsghdr alignment; char bytes[CMSG_SPACE(8 * sizeof(int))]; } control = {0};
    struct iovec iov = {.iov_base = packet, .iov_len = capacity};
    struct msghdr message = { .msg_iov = &iov, .msg_iovlen = 1,
        .msg_control = control.bytes, .msg_controllen = sizeof(control.bytes) };
    *passed_fd = -1;
    ssize_t n;
    do { n = recvmsg(socket_fd, &message, MSG_CMSG_CLOEXEC | MSG_DONTWAIT); }
    while (n < 0 && errno == EINTR);
    if (n < 0) return -1;
    int malformed = (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0;
    size_t count = 0;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&message); c; c = CMSG_NXTHDR(&message, c)) {
        if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS
                || c->cmsg_len < CMSG_LEN(0)) { malformed = 1; continue; }
        size_t bytes = c->cmsg_len - CMSG_LEN(0);
        if (bytes == 0 || bytes % sizeof(int)) malformed = 1;
        for (size_t offset = 0; offset + sizeof(int) <= bytes; offset += sizeof(int)) {
            int fd;
            memcpy(&fd, (char *)CMSG_DATA(c) + offset, sizeof(fd));
            if (++count == 1) *passed_fd = fd;
            else { close(fd); malformed = 1; }
        }
    }
    if (malformed) {
        if (*passed_fd >= 0) close(*passed_fd);
        *passed_fd = -1;
        errno = EPROTO;
        return -1;
    }
    return n;
}
