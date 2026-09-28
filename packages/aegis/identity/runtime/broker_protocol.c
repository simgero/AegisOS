#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "broker_protocol.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error The AEGIS ARM64 broker protocol requires little-endian encoding
#endif
_Static_assert(sizeof(struct aegis_broker_request) == 32, "Request wire layout");
_Static_assert(sizeof(struct aegis_broker_reply) == 32, "Reply wire layout");
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

int aegis_broker_parse(const void *packet, size_t size, uint64_t previous,
                       uint64_t now_ns, struct aegis_broker_request *request) {
    if (!packet || !request || size != sizeof(*request)) return fail(EPROTO);
    struct aegis_broker_request value;
    memcpy(&value, packet, sizeof(value));
    if (value.magic != AEGIS_BROKER_MAGIC || value.version != AEGIS_BROKER_VERSION
            || value.operation < AEGIS_BROKER_HELLO || value.operation > AEGIS_BROKER_STATUS
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
