#ifndef AEGIS_RUNTIME_NAMESPACE_H
#define AEGIS_RUNTIME_NAMESPACE_H

#include "child.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* INTERNAL trusted launcher primitive, not an AOSP authorization endpoint.
 * Caller: dedicated SINGLE-THREADED Android host-root broker, initial user/PID
 * namespaces, no supplementary groups, exclusive child reaper, no SIGCHLD
 * auto-reaping. Drop groups during broker initialization, NOT after NEWUSER.
 * The broker must bind every handle to a fresh authorized AOSP id+serial and
 * CE state; user_id alone grants no authority and must not come from a client.
 */
struct aegis_namespace;

/* setup_fd: already authenticated, preopened static ARM64 ELF from trusted
 * system code (NOT a user's executable). control_fd: one end of a private,
 * unnamed connected Unix SEQPACKET socket pair. Neither caller fd is consumed.
 * The setup helper gets only FD 3, argv [aegis-runtime-setup, USER_ID], and a
 * fixed environment. It still sees the inherited Android root until it sets
 * up trusted mounts: it MUST NOT execute user code before pivot/detach, resource
 * limits and SELinux setup. aegis-runtime-init is NOT yet a usable setup helper.
 *
 * Creates USER/PID/MOUNT/IPC/UTS/NET namespaces and a pidfd atomically. The child
 * waits up to 10s, without executing the helper, for complete maps. Success
 * means ONLY a child was created. All fallible parent allocations precede
 * clone; *output must initially be NULL. Failure owns no new child and leaves
 * that output unchanged (including rejection of an already populated output).
 */
int aegis_namespace_create(uint32_t user_id, int setup_fd, int control_fd,
                           struct aegis_namespace **output);

/* One attempt: anchor the child's proc directory, deny setgroups, write and
 * read back BOTH fixed maps, then release the gate. Any failure closes the
 * gate and permanently prevents retry; the child handle remains owned.
 * Zero means the gate was sent, NOT successful exec, mounts or runtime READY.
 * Caller must recheck AOSP authorization immediately before this operation.
 */
int aegis_namespace_resume(struct aegis_namespace *context);

/* Stable pidfd termination/observation, with the same semantics as child.h.
 * stop also closes the gate. wait confirms ONLY this child's actual exit.
 */
int aegis_namespace_stop(struct aegis_namespace *context);
int aegis_namespace_wait(struct aegis_namespace *context, int timeout_ms,
                         struct aegis_child_exit *result);

/* Close references/gate only, NOT kill/reap or certify teardown. Normally
 * release only AFTER a successful wait and separate mount/resource cleanup.
 */
void aegis_namespace_release(struct aegis_namespace *context);

#ifdef __cplusplus
}
#endif
#endif
