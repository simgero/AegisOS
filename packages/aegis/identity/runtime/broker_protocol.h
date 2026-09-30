#ifndef AEGIS_BROKER_PROTOCOL_H
#define AEGIS_BROKER_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Private host-side SEQPACKET connection, system_server -> root runtime owner.
 * Neither an AOSP authentication service nor the namespace-init protocol.
 * No incoming descriptors, passwords, host paths, bearer tokens or CLI identities.
 * EXEC carries bounded program arguments within the already-admitted namespace.
 * Authorize the peer BEFORE parsing; retain one sequence counter per connection.
 */
#define AEGIS_BROKER_MAGIC 0x42524741u
#define AEGIS_BROKER_VERSION 3u
#define AEGIS_BROKER_MAX_PACKET 8192u
#define AEGIS_BROKER_MAX_ARGS 32u
#define AEGIS_BROKER_MAX_ARG_BYTES (AEGIS_BROKER_MAX_PACKET - 40u)
#define AEGIS_BROKER_MAX_WAIT_NS UINT64_C(10000000000)
enum aegis_broker_operation { AEGIS_BROKER_HELLO = 1, AEGIS_BROKER_START = 2,
    AEGIS_BROKER_STOP_USER = 3, AEGIS_BROKER_STATUS = 4,
    AEGIS_BROKER_EXEC = 5, AEGIS_BROKER_RESULT = 6, AEGIS_BROKER_CONTINUE_START = 7,
    AEGIS_BROKER_PACKAGE_BEGIN = 8, AEGIS_BROKER_PACKAGE_PLAN = 9,
    AEGIS_BROKER_PACKAGE_REVIEW = 10, AEGIS_BROKER_PACKAGE_PREPARE = 11,
    AEGIS_BROKER_PACKAGE_STATUS = 12, AEGIS_BROKER_PACKAGE_START = 13,
    AEGIS_BROKER_PACKAGE_CANCEL = 14 };
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
/* EXEC: 32-byte header, uint32 argc, uint32 byte count, exact NUL-ended argv.
 * RESULT: 32-byte header, positive signed-64 broker command ID.
 * CONTINUE_START: header plus positive signed-64 selection job, never creates work.
 * Other lifecycle requests stay 32 bytes. This decoded object is NOT wire
 * layout; it owns its argument bytes and contains no borrowed pointers.
 */
struct aegis_broker_call {
    struct aegis_broker_request request;
    uint64_t command;
    uint32_t argc, payload_bytes;
    char payload[AEGIS_BROKER_MAX_ARG_BYTES];
    uint32_t package_action, package_scope;
    char package_name[129], package_version[129], package_digest[65];
};
/* Both terminal replies are 48 bytes. EXEC success carries exactly one private
 * PTY master; RESULT carries no fd. exited=0 means running, not exit status 0.
 * Error replies contain no command, wait status, exit flag or descriptor.
 */
/* START and CONTINUE_START replies are 40 bytes, no descriptors. EAGAIN
 * carries a positive selection job; READY may carry zero only for a legacy
 * context without selection. Other errors carry zero. This job is correlation,
 * never authentication; every continuation requires fresh AOSP admission. */
struct aegis_broker_start_reply {
    struct aegis_broker_reply header;
    uint64_t job;
};
struct aegis_broker_terminal_reply {
    struct aegis_broker_reply header;
    uint64_t command;
    int32_t wait_status;
    uint32_t exited;
};

/* Additive package operations keep the lifecycle/terminal v3 frames unchanged.
 * BEGIN: header + action/scope/nameBytes/versionBytes (4x uint32), then exact
 * printable ASCII name/version bytes, no NUL/padding. Scope 1=personal, 2=shared.
 * Other package calls: header + positive job; START additionally carries exactly
 * 64 lowercase hex digest bytes from the service's retained native review.
 * Replies: normal 32-byte header, job(uint64), kind(uint32), bytes(uint32), body.
 * kind 0=receipt, 1=status JSON, 2=complete review JSON. At most 64 KiB total.
 * No incoming/outgoing descriptors. BEGIN can return a retained job on error;
 * other responses always echo the same job. No credential or path is transported.
 */
#define AEGIS_BROKER_PACKAGE_MAX_REPLY 65536u
struct aegis_broker_package_reply {
    uint64_t job;
    uint32_t kind, bytes;
    unsigned char data[AEGIS_BROKER_PACKAGE_MAX_REPLY - 48u];
};
int aegis_broker_package_operation(unsigned operation);
int aegis_broker_reply_package(int fd, const struct aegis_broker_request *request,
                                int error, const struct aegis_broker_package_reply *package);

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
/* One nonblocking recvmsg. All ancillary descriptors are closed/rejected;
 * oversized/truncated packets and EOF cannot publish a request. Authenticate
 * the connected peer before calling. EAGAIN means no packet, not disconnect. */
int aegis_broker_receive(int fd, uint64_t previous, uint64_t now_ns,
                         struct aegis_broker_request *request);
int aegis_broker_decode(const void *packet, size_t size, uint64_t previous,
                        uint64_t now_ns, struct aegis_broker_call *call);
int aegis_broker_receive_call(int fd, uint64_t previous, uint64_t now_ns,
                              struct aegis_broker_call *call);
/* Validate owned argument storage; return pointers into call without logging it. */
int aegis_broker_arguments(const struct aegis_broker_call *call,
                           const char *argv[AEGIS_BROKER_MAX_ARGS + 1]);
/* Send one complete reply; timeout/EOF/failed send never confirms completion.
 * Caller handles deadline and retains cleanup ownership on every failure. */
int aegis_broker_reply(int fd, const struct aegis_broker_request *request,
                       int error, enum aegis_broker_state state);
int aegis_broker_reply_start(int fd, const struct aegis_broker_request *request,
                             int error, uint64_t job);
/* Atomic, nonblocking, caller RETAINS ownership of master even after success. */
int aegis_broker_reply_terminal(int fd, const struct aegis_broker_request *request,
                                int error, uint64_t command, int wait_status,
                                int exited, int master);

#ifdef __cplusplus
}
#endif
#endif
