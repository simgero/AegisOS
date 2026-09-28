#ifndef AEGIS_RUNTIME_CONTROL_H
#define AEGIS_RUNTIME_CONTROL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Private, little-endian protocol over one inherited, unnamed SOCK_SEQPACKET
 * pair. Only the AOSP-authorized broker may own the other endpoint. This is
 * not an authentication protocol or a public socket/service. No passwords. */
#define AEGIS_RUNTIME_MAGIC 0x49474541u
#define AEGIS_RUNTIME_VERSION 1u
#define AEGIS_RUNTIME_MAX_PACKET 8192u
#define AEGIS_RUNTIME_MAX_ARGS 32u
#define AEGIS_RUNTIME_MAX_SHELLS 32u

enum aegis_runtime_operation { AEGIS_EXEC = 1, AEGIS_STOP = 2 };
enum aegis_runtime_event { AEGIS_READY = 1, AEGIS_STARTED = 2, AEGIS_EXITED = 3, AEGIS_ERROR = 4 };

struct aegis_runtime_request {
    uint32_t magic;
    uint16_t version;
    uint16_t operation;
    uint64_t request_id;
    uint32_t payload_bytes;
    uint32_t argc;
};

/* STARTED carries exactly one newly-created private PTY master, all other
 * replies carry no descriptors. wait_status is the original waitpid status.
 * READY uses request_id=(userId<<32)|serial, an identity echo, not a token.
 * STOP has no success reply: the broker must await the real init process exit. */
struct aegis_runtime_reply {
    uint32_t magic;
    uint16_t version;
    uint16_t event;
    uint64_t request_id;
    int32_t error;
    int32_t pid;
    int32_t wait_status;
    uint32_t reserved;
};

/* argv points into the caller-owned packet. Return 0 or -EPROTO; never execute. */
int aegis_parse_request(void *packet, size_t length, uint64_t previous_id,
                       struct aegis_runtime_request *request,
                       char *argv[AEGIS_RUNTIME_MAX_ARGS + 1]);
/* Nonblocking atomic reply. Caller retains ownership of passed_fd. */
int aegis_send_reply(int socket_fd, uint16_t event, uint64_t request_id,
                     int error, int pid, int wait_status, int passed_fd);
/* One packet, optionally one CLOEXEC descriptor. Close all received descriptors
 * on malformed/truncated ancillary data. Returns bytes, 0 for EOF, or -1/errno.
 * The caller must close a returned descriptor even when rejecting the payload. */
ssize_t aegis_receive(int socket_fd, void *packet, size_t capacity, int *passed_fd);

#ifdef __cplusplus
}
#endif
#endif
