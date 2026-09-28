#ifndef AEGIS_BROKER_PROTOCOL_H
#define AEGIS_BROKER_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Private host-side SEQPACKET connection, system_server -> root runtime owner.
 * Neither an AOSP authentication service nor the namespace-init protocol.
 * No descriptors, passwords, paths, bearer tokens or CLI identities accepted.
 * Authorize the peer BEFORE parsing; retain one sequence counter per connection.
 */
#define AEGIS_BROKER_MAGIC 0x42524741u
#define AEGIS_BROKER_VERSION 1u
#define AEGIS_BROKER_MAX_WAIT_NS UINT64_C(10000000000)
enum aegis_broker_operation { AEGIS_BROKER_HELLO = 1, AEGIS_BROKER_START = 2,
    AEGIS_BROKER_STOP_USER = 3, AEGIS_BROKER_STATUS = 4 };
enum aegis_broker_state { AEGIS_BROKER_ABSENT = 0, AEGIS_BROKER_READY = 1,
    AEGIS_BROKER_SEALED = 2 };
struct aegis_broker_request {
    uint32_t magic;
    uint16_t version, operation;
    uint64_t sequence, deadline_ns;
    uint32_t user, serial;
};
struct aegis_broker_reply {
    uint32_t magic;
    uint16_t version, operation;
    uint64_t sequence;
    uint32_t user, serial;
    int32_t error;
    uint32_t state;
};

/* Kernel-supplied UID/GID 1000 AND exact system_server SELinux socket context.
 * Accepted fd must be connected AF_UNIX/SOCK_SEQPACKET. No name-based trust. */
int aegis_broker_check_peer(int fd);
/* One exact 32-byte frame, strictly increasing positive signed-64 sequence,
 * absolute CLOCK_MONOTONIC deadline > now and at most ten seconds ahead.
 * STOP_USER serial=0 means ALL owned serials, not serial zero's context.
 * HELLO (0,0) is accepted only as the FIRST request; the owner may acknowledge
 * it only after proving every predecessor's resources are gone.
 */
int aegis_broker_parse(const void *packet, size_t size, uint64_t previous,
                       uint64_t now_ns, struct aegis_broker_request *request);
/* Send one complete reply; timeout/EOF/failed send never confirms completion.
 * Caller handles deadline and retains cleanup ownership on every failure. */
int aegis_broker_reply(int fd, const struct aegis_broker_request *request,
                       int error, enum aegis_broker_state state);

#ifdef __cplusplus
}
#endif
#endif
