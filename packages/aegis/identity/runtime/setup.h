#ifndef AEGIS_RUNTIME_SETUP_H
#define AEGIS_RUNTIME_SETUP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* INTERNAL startup protocol on the broker's private unnamed SEQPACKET pair.
 * Not a user endpoint or authentication mechanism. Send only after fresh AOSP
 * authorization and preparation, keeping the user's lifecycle lock held.
 * The immutable id+serial must match the namespace handle. No paths/passwords.
 * Five strictly ordered records: base, CE home, devices, trusted static init,
 * then commit. Each of the first four carries exactly one descriptor.
 * After any send error the broker MUST abort the context, never retry a prefix.
 * Success means queued references only, not mounts, exec or runtime readiness.
 */
#define AEGIS_SETUP_MAGIC 0x53474541u
#define AEGIS_SETUP_VERSION 1u
#define AEGIS_SETUP_FDS 4u
enum aegis_setup_role { AEGIS_SETUP_BASE = 1, AEGIS_SETUP_HOME = 2,
    AEGIS_SETUP_DEVICES = 3, AEGIS_SETUP_INIT = 4, AEGIS_SETUP_COMMIT = 5 };
struct aegis_setup_record {
    uint32_t magic;
    uint16_t version, role;
    uint32_t user_id, serial;
    uint32_t reserved[2];
};

int aegis_send_setup(int socket_fd, uint32_t user_id, uint32_t serial,
                     const int fds[AEGIS_SETUP_FDS]);

/* Internal helper receiver. Output must be four -1 entries. Absolute total
 * deadline, not renewed per packet, at most 5s. Every returned fd is CLOEXEC
 * and >=4. On failure all received refs are closed and output is unchanged.
 * This validates transport, NOT the objects or the sender's authorization.
 */
int aegis_receive_setup(int socket_fd, uint32_t user_id, uint32_t serial,
                        int timeout_ms, int fds[AEGIS_SETUP_FDS]);

#ifdef __cplusplus
}
#endif
#endif
