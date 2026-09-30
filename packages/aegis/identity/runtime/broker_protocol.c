#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "broker_protocol.h"
#include "control.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error The AEGIS ARM64 broker protocol requires little-endian encoding
#endif
_Static_assert(sizeof(struct aegis_broker_request) == 32, "Request wire layout");
_Static_assert(sizeof(struct aegis_broker_reply) == 32, "Reply wire layout");
_Static_assert(sizeof(struct aegis_broker_start_reply) == 40, "Start reply wire layout");
_Static_assert(sizeof(struct aegis_broker_terminal_reply) == 48, "Terminal reply wire layout");
_Static_assert(offsetof(struct aegis_broker_request, user) == 24, "Request identity offset");
_Static_assert(offsetof(struct aegis_broker_reply, error) == 24, "Reply error offset");

static int fail(int error) { errno = error; return -1; }

int aegis_broker_check_peer(int fd) {
    int type = 0;
    socklen_t length = sizeof(type);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &length) < 0) return -1;
    if (length != sizeof(type) || type != SOCK_SEQPACKET) return fail(EPROTOTYPE);
    struct sockaddr_un address = {0};
    length = sizeof(address);
    if (getpeername(fd, (struct sockaddr *)&address, &length) < 0) return -1;
    if (length < sizeof(sa_family_t) || address.sun_family != AF_UNIX) return fail(EPROTOTYPE);
    struct ucred peer = {0};
    length = sizeof(peer);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &length) < 0) return -1;
    if (length != sizeof(peer) || peer.pid <= 0 || peer.uid != 1000 || peer.gid != 1000)
        return fail(EPERM);
    char context[128] = {0};
    static const char expected[] = "u:r:system_server:s0";
    length = sizeof(context);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERSEC, context, &length) < 0) return -1;
    /* Linux permits SO_PEERSEC results with or without the trailing NUL. */
    if ((length != sizeof(expected) - 1 && length != sizeof(expected))
            || memcmp(context, expected, sizeof(expected) - 1)
            || (length == sizeof(expected) && context[length - 1] != '\0')) return fail(EPERM);
    return 0;
}

static int parse_header(const void *packet, size_t size, uint64_t previous,
                         uint64_t now_ns, struct aegis_broker_request *request) {
    if (!packet || !request || size != sizeof(*request)) return fail(EPROTO);
    struct aegis_broker_request value;
    memcpy(&value, packet, sizeof(value));
    if (value.magic != AEGIS_BROKER_MAGIC || value.version != AEGIS_BROKER_VERSION
            || value.operation < AEGIS_BROKER_HELLO || value.operation > AEGIS_BROKER_CONTINUE_START
            || !value.sequence || value.sequence > INT64_MAX || value.sequence <= previous
            || now_ns > INT64_MAX || value.deadline_ns > INT64_MAX || value.deadline_ns <= now_ns
            || value.deadline_ns - now_ns > AEGIS_BROKER_MAX_WAIT_NS || value.serial > INT32_MAX)
        return fail(EPROTO);
    if (value.operation == AEGIS_BROKER_HELLO) {
        if (previous || value.user || value.serial) return fail(EPROTO);
    } else if (!previous || value.user < 10 || value.user >= 21473
            || (value.operation == AEGIS_BROKER_STOP_USER && value.serial)) return fail(EPROTO);
    *request = value;
    return 0;
}

int aegis_broker_parse(const void *packet, size_t size, uint64_t previous,
                       uint64_t now_ns, struct aegis_broker_request *request) {
    struct aegis_broker_request checked;
    if (!request) return fail(EINVAL);
    if (parse_header(packet, size, previous, now_ns, &checked) < 0) return -1;
    if (checked.operation > AEGIS_BROKER_STATUS) return fail(EPROTO);
    *request = checked;
    return 0;
}

int aegis_broker_arguments(const struct aegis_broker_call *call,
                           const char *argv[AEGIS_BROKER_MAX_ARGS + 1]) {
    if (!call || !argv || call->request.operation != AEGIS_BROKER_EXEC || call->command
            || !call->argc || call->argc > AEGIS_BROKER_MAX_ARGS
            || !call->payload_bytes || call->payload_bytes > sizeof(call->payload)) return fail(EPROTO);
    const char *checked[AEGIS_BROKER_MAX_ARGS + 1] = {0};
    const char *cursor = call->payload, *end = cursor + call->payload_bytes;
    for (uint32_t i = 0; i < call->argc; i++) {
        if (cursor == end) return fail(EPROTO);
        const char *terminator = memchr(cursor, 0, (size_t)(end - cursor));
        if (!terminator) return fail(EPROTO);
        checked[i] = cursor;
        cursor = terminator + 1;
    }
    if (cursor != end || checked[0][0] != '/') return fail(EPROTO);
    memcpy(argv, checked, sizeof(checked));
    return 0;
}

int aegis_broker_decode(const void *packet, size_t size, uint64_t previous,
                        uint64_t now_ns, struct aegis_broker_call *call) {
    if (!packet || !call || size < 32 || size > AEGIS_BROKER_MAX_PACKET) return fail(EPROTO);
    struct aegis_broker_call checked = {0};
    if (parse_header(packet, 32, previous, now_ns, &checked.request) < 0) return -1;
    const unsigned char *bytes = packet;
    if (checked.request.operation == AEGIS_BROKER_EXEC) {
        if (size < 40) return fail(EPROTO);
        memcpy(&checked.argc, bytes + 32, 4);
        memcpy(&checked.payload_bytes, bytes + 36, 4);
        if (checked.payload_bytes != size - 40) return fail(EPROTO);
        memcpy(checked.payload, bytes + 40, checked.payload_bytes);
        const char *argv[AEGIS_BROKER_MAX_ARGS + 1];
        if (aegis_broker_arguments(&checked, argv) < 0) return -1;
    } else if (checked.request.operation == AEGIS_BROKER_RESULT
            || checked.request.operation == AEGIS_BROKER_CONTINUE_START) {
        if (size != 40) return fail(EPROTO);
        memcpy(&checked.command, bytes + 32, 8);
        if (!checked.command || checked.command > INT64_MAX) return fail(EPROTO);
    } else if (size != 32) {
        return fail(EPROTO);
    }
    *call = checked;
    return 0;
}

int aegis_broker_receive_call(int fd, uint64_t previous, uint64_t now_ns,
                              struct aegis_broker_call *call) {
    unsigned char packet[AEGIS_BROKER_MAX_PACKET];
    int received_fd = -1;
    ssize_t count = aegis_receive(fd, packet, sizeof(packet), &received_fd);
    int saved = errno;
    if (received_fd >= 0) { close(received_fd); return fail(EPROTO); }
    if (count < 0) return fail(saved);
    if (!count) return fail(EPIPE);
    return aegis_broker_decode(packet, (size_t)count, previous, now_ns, call);
}

int aegis_broker_receive(int fd, uint64_t previous, uint64_t now_ns,
                         struct aegis_broker_request *request) {
    unsigned char packet[sizeof(*request)];
    int received_fd = -1;
    ssize_t count = aegis_receive(fd, packet, sizeof(packet), &received_fd);
    int saved = errno;
    if (received_fd >= 0) { close(received_fd); return fail(EPROTO); }
    if (count < 0) return fail(saved);
    if (!count) return fail(EPIPE);
    return aegis_broker_parse(packet, (size_t)count, previous, now_ns, request);
}

int aegis_broker_reply(int fd, const struct aegis_broker_request *request,
                       int error, enum aegis_broker_state state) {
    if (!request || request->magic != AEGIS_BROKER_MAGIC || request->version != AEGIS_BROKER_VERSION
            || request->operation < AEGIS_BROKER_HELLO || request->operation > AEGIS_BROKER_STATUS
            || !request->sequence || request->sequence > INT64_MAX || error < 0 || error > 4095
            || request->serial > INT32_MAX
            || (request->operation == AEGIS_BROKER_HELLO ? request->user || request->serial
                : request->user < 10 || request->user >= 21473)
            || (request->operation == AEGIS_BROKER_STOP_USER && request->serial)
            || (state != AEGIS_BROKER_ABSENT && state != AEGIS_BROKER_READY && state != AEGIS_BROKER_SEALED)
            || (error && state != AEGIS_BROKER_SEALED)
            || (!error && request->operation == AEGIS_BROKER_START && state != AEGIS_BROKER_READY)
            || (!error && (request->operation == AEGIS_BROKER_HELLO
                || request->operation == AEGIS_BROKER_STOP_USER) && state != AEGIS_BROKER_ABSENT))
        return fail(EINVAL);
    struct aegis_broker_reply reply = {
        .magic = AEGIS_BROKER_MAGIC, .version = AEGIS_BROKER_VERSION,
        .operation = request->operation, .sequence = request->sequence,
        .user = request->user, .serial = request->serial, .error = error, .state = state,
    };
    ssize_t sent = send(fd, &reply, sizeof(reply), MSG_DONTWAIT | MSG_NOSIGNAL);
    if (sent < 0) return -1;
    return sent == (ssize_t)sizeof(reply) ? 0 : fail(EIO);
}

int aegis_broker_reply_terminal(int fd, const struct aegis_broker_request *request,
                                int error, uint64_t command, int wait_status,
                                int exited, int master) {
    if (!request || request->magic != AEGIS_BROKER_MAGIC || request->version != AEGIS_BROKER_VERSION
            || (request->operation != AEGIS_BROKER_EXEC && request->operation != AEGIS_BROKER_RESULT)
            || !request->sequence || request->sequence > INT64_MAX
            || request->user < 10 || request->user >= 21473 || request->serial > INT32_MAX
            || error < 0 || error > 4095 || (exited != 0 && exited != 1)
            || (error && (command || wait_status || exited || master != -1))
            || (!error && (!command || command > INT64_MAX))
            || (!error && request->operation == AEGIS_BROKER_EXEC && (master < 0 || wait_status || exited))
            || (request->operation == AEGIS_BROKER_RESULT && master != -1)
            || (!exited && wait_status)
            || (exited && (wait_status < 0 || wait_status > UINT16_MAX
                || (!WIFEXITED(wait_status) && !WIFSIGNALED(wait_status))))) return fail(EINVAL);
    struct aegis_broker_terminal_reply reply = {
        .header = {.magic = AEGIS_BROKER_MAGIC, .version = AEGIS_BROKER_VERSION,
            .operation = request->operation, .sequence = request->sequence,
            .user = request->user, .serial = request->serial, .error = error,
            .state = error ? AEGIS_BROKER_SEALED : AEGIS_BROKER_READY},
        .command = command, .wait_status = wait_status, .exited = (uint32_t)exited,
    };
    struct iovec io = {.iov_base = &reply, .iov_len = sizeof(reply)};
    union { struct cmsghdr alignment; char bytes[CMSG_SPACE(sizeof(int))]; } control = {0};
    struct msghdr message = {.msg_iov = &io, .msg_iovlen = 1};
    if (master >= 0) {
        message.msg_control = control.bytes; message.msg_controllen = sizeof(control.bytes);
        struct cmsghdr *header = CMSG_FIRSTHDR(&message);
        header->cmsg_level = SOL_SOCKET; header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(header), &master, sizeof(master));
    }
    ssize_t sent = sendmsg(fd, &message, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (sent < 0) return -1;
    return sent == (ssize_t)sizeof(reply) ? 0 : fail(EIO);
}

int aegis_broker_reply_start(int fd, const struct aegis_broker_request *request,
                             int error, uint64_t job) {
    if (!request || request->magic != AEGIS_BROKER_MAGIC || request->version != AEGIS_BROKER_VERSION
            || (request->operation != AEGIS_BROKER_START && request->operation != AEGIS_BROKER_CONTINUE_START)
            || !request->sequence || request->sequence > INT64_MAX
            || request->user < 10 || request->user >= 21473 || request->serial > INT32_MAX
            || error < 0 || error > 4095 || job > INT64_MAX
            || (error == EAGAIN ? !job : (error && job))
            || (!error && request->operation == AEGIS_BROKER_CONTINUE_START && !job)) return fail(EINVAL);
    struct aegis_broker_start_reply reply = {
        .header = {.magic=AEGIS_BROKER_MAGIC, .version=AEGIS_BROKER_VERSION,
            .operation=request->operation, .sequence=request->sequence,
            .user=request->user, .serial=request->serial, .error=error,
            .state=error ? AEGIS_BROKER_SEALED : AEGIS_BROKER_READY},
        .job=job,
    };
    ssize_t sent=send(fd,&reply,sizeof(reply),MSG_DONTWAIT|MSG_NOSIGNAL);
    if(sent<0)return -1;
    return sent==(ssize_t)sizeof(reply) ? 0 : fail(EIO);
}
