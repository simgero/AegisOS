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
 * The setup helper gets only FD 3, argv [aegis-runtime-setup, USER_ID, SERIAL], and a
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
int aegis_namespace_create(uint32_t user_id, uint32_t serial, int setup_fd, int control_fd,
                           struct aegis_namespace **output);

/* One attempt: anchor the child's proc directory, deny setgroups, write and
 * read back BOTH fixed maps, then release the gate. Any failure closes the
 * gate and permanently prevents retry; the child handle remains owned.
 * Zero means the gate was sent, NOT successful exec, mounts or runtime READY.
 * Caller must recheck AOSP authorization immediately before this operation.
 */
int aegis_namespace_resume(struct aegis_namespace *context);

/* Optional separate mapping step for host-side mount preparation. One-shot;
 * performs exactly the map checks of resume(), but KEEPS the exec gate closed.
 * resume() after this step only rechecks the live child/broker and opens the
 * gate. The original 10s child startup deadline is NOT extended.
 */
int aegis_namespace_prepare(struct aegis_namespace *context);

/* After prepare() and before resume(): clone a preopened, broker-verified,
 * readonly shared base root as a DETACHED readonly/nosuid/nodev/private mount,
 * ID-mapped to this exact child's namespace. Returns a new CLOEXEC fd; caller
 * owns it and must close it on cancellation/teardown. Never attaches a mount,
 * changes disk ownership or changes the source mount. The approved clone is
 * executable even if the source is noexec. On failure context
 * remains paused: abort it or correct preparation within the same deadline.
 *
 * Source provenance, generation hash, absence of private data, and backing
 * filesystem immutability are BROKER obligations. Root ownership/read-only
 * flags alone cannot authenticate a base. Not for CE directories: their host
 * ownership is already user-specific and MUST NOT be remapped a second time.
 * Does not provide mount/exec readiness, AOSP authorization or cleanup proof.
 */
int aegis_namespace_base_mount(struct aegis_namespace *context, int verified_source_fd);

/* After prepare(), before resume(): resolve this context's immutable id+serial
 * against AOSP's existing internal-volume CE roots, optionally provision its
 * private home, and return a detached writable/nosuid/nodev/private mount fd.
 * No second ID map: files already carry the mapped ordinary host UID/GID.
 * No caller paths, repair of AOSP metadata, or key mutation/export. create is
 * strictly 0/1; existing mismatches and interrupted provisioning are errors.
 *
 * Requires fresh AOSP authentication, CE-unlocked confirmation and a lifecycle
 * lock spanning preparation and resume; those remain BROKER obligations.
 * Kernel key presence alone is NOT authorization. Caller owns the returned fd
 * and must close ALL CE mounts/fds before AOSP key removal. Success is neither
 * attached storage nor runtime readiness. The original child deadline applies.
 */
int aegis_namespace_home_mount(struct aegis_namespace *context, int create);

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
