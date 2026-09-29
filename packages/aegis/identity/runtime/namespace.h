#ifndef AEGIS_RUNTIME_NAMESPACE_H
#define AEGIS_RUNTIME_NAMESPACE_H

#include "child.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Trusted startup handoff, never a client IPC endpoint. Init preopens its
 * user/PID/mount namespace handles before starting the broker. Copy and pin
 * these exact NSFS descriptors; reject wrong kinds, mismatched namespaces,
 * inherited process ownership or later replacement. No /proc/1 access and no
 * ptrace permission are needed. Caller retains its descriptors. Repeated
 * calls are allowed only with the same namespaces in the original process.
 * Must precede every other namespace entry point. Device fixtures explicitly
 * supply host handles too; there is no automatic privileged fallback. */
int aegis_namespace_pin_host(int user_ns, int pid_ns, int mount_ns);

/* Fixed host-process prerequisites shared with the daemon's startup guard. */
int aegis_namespace_check_broker(void);

/* Startup-only: verify the initial host namespaces and other broker guards,
 * then create our own private mount namespace. Must precede personal contexts.
 * Idempotent only in this process and that exact namespace. Initialization
 * failure is terminal; no retry/fallback after partially changing namespaces.
 * Holds one CLOEXEC namespace fd for the broker's lifetime. */
int aegis_namespace_private_mounts(void);

/* In the above private namespace only: attach the broker-verified readonly
 * base root at /mnt, without modifying Android's mount namespace or creating
 * filesystem entries. Source fd remains owned and valid. Kernel open_tree
 * cloning requires this attachment; a detached fsmount is not a clone source.
 * Source provenance/hash/immutable backing remain the broker's obligation.
 * Mount lifetime is this namespace's lifetime (and any clone/fd references).
 * Production attaches once; tests detach their own inert fixture on cleanup. */
int aegis_namespace_attach_base(int verified_source_fd);

/* Personal namespace preparation uses only references sent by the trusted
 * raw clone over its private inherited gate. It opens its own proc inodes
 * as O_PATH and its own user namespace as an NSFS fd. The parent reopens the
 * proc references through its own fd table, retaining the parent credentials
 * required for the multi-extent maps. No external fd/PID is accepted and no
 * ptrace/readproc exemption is used. Maps and OOM protection are read back
 * before the existing execution gate can be released. */

/* INTERNAL trusted launcher primitive, not an AOSP authorization endpoint.
 * Caller: dedicated SINGLE-THREADED Android host-root broker, initial user/PID
 * namespaces and either the initial mount namespace or its own explicitly
 * established private copy, no supplementary groups, exclusive child reaper, no SIGCHLD
 * auto-reaping. Drop groups during broker initialization, NOT after NEWUSER.
 * The broker must bind every handle to a fresh authorized AOSP id+serial and
 * CE state; user_id alone grants no authority and must not come from a client.
 */
struct aegis_namespace;
struct aegis_memory_group;

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
 * This unrestricted entry is for inert device probes. The production broker
 * must use create_limited() so workload memory is bounded at process creation.
 */
int aegis_namespace_create(uint32_t user_id, uint32_t serial, int setup_fd, int control_fd,
                           struct aegis_namespace **output);

/* Same child/gate contract, atomically born in the caller's prepared memory
 * group via CLONE_INTO_CGROUP. No create-then-migrate window. The group is
 * matched to this exact id+serial and claimed once. A failed attempt can seal
 * it even without a child; the caller still owns and must remove that group.
 * No success implies authorization, READY, pidfd reaping or CE cleanup.
 */
int aegis_namespace_create_limited(uint32_t user_id, uint32_t serial, int setup_fd,
                                   int control_fd, struct aegis_memory_group *group,
                                   struct aegis_namespace **output);

/* One attempt: anchor the child's proc directory, deny setgroups, write and
 * read back BOTH fixed maps, then release the gate. Any failure closes the
 * gate and permanently prevents retry; the child handle remains owned.
 * Zero means the gate was sent, NOT successful exec, mounts or runtime READY.
 * Caller must recheck AOSP authorization immediately before this operation.
 */
int aegis_namespace_resume(struct aegis_namespace *context);

/* Optional separate mapping step for host-side mount preparation. One-shot;
 * resets/readbacks oom_score_adj=0 and performs exactly the map checks of
 * resume(), but KEEPS the exec gate closed.
 * resume() after this step only rechecks the live child/broker and opens the
 * gate. The original 10s child startup deadline is NOT extended.
 */
int aegis_namespace_prepare(struct aegis_namespace *context);

/* Explicit PACKAGE-worker mapping, never ordinary session preparation. Same
 * fixed UID/GID extents, empty inherited groups, OOM readback, one-shot gate and
 * ownership checks; preserves kernel setgroups=allow so APT can switch between
 * mapped technical groups. The kernel rejects every unmapped GID. Ordinary
 * prepare/resume still write deny. Package contexts cannot obtain a HOME view;
 * only these contexts may map an exclusively owned writable candidate.
 * No client endpoint or authorization: caller still needs AOSP approval,
 * lifecycle registration, limited creation and checked worker mount/policy.
 */
int aegis_namespace_prepare_package(struct aegis_namespace *context);

/* After prepare() and before resume(): clone a preopened, broker-verified,
 * readonly shared base root as a DETACHED readonly/nosuid/nodev/private mount,
 * ID-mapped to this exact child's namespace. Returns a new CLOEXEC fd; caller
 * owns it and must close it on cancellation/teardown. Never attaches a mount,
 * changes disk ownership or changes the source mount. The approved clone is
 * executable even if the source is noexec. On failure context
 * remains paused: abort it or correct preparation within the same deadline.
 *
 * The source must already be attached in this broker's mount namespace.
 * Source provenance, generation hash, absence of private data, and backing
 * filesystem immutability are BROKER obligations. Root ownership/read-only
 * flags alone cannot authenticate a base. Not for CE directories: their host
 * ownership is already user-specific and MUST NOT be remapped a second time.
 * Does not provide mount/exec readiness, AOSP authorization or cleanup proof.
 */
int aegis_namespace_base_mount(struct aegis_namespace *context, int verified_source_fd);

/* Internal package staging only, NOT authorization or a production worker.
 * After prepare_package(), before resume(): map an exclusively owned detached writable
 * ext4 fsmount to this exact child. The caller has already copied and validated
 * an inactive candidate OUTSIDE the admission gate. Never pass the active base,
 * a personal home, an attached mount or a client-supplied descriptor.
 * Input remains owned by caller; this MUTATES that mount in place and makes it
 * executable/nosuid/nodev. Kernel rejects attached/already-IDmapped mounts.
 * On any error discard the candidate; no restoration/retry is promised.
 * All image/loop/mount refs must be lifecycle-owned before gate release, and
 * eventually closed before CE locks. Production CE/SELinux wiring is pending.
 */
int aegis_namespace_map_candidate(struct aegis_namespace *context, int candidate);


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

/* After prepare(), before resume(): create a fresh detached, readonly tmpfs
 * with exactly six standard character devices and fixed private /dev layout.
 * Returns a CLOEXEC mount fd mapped to this child's UID namespace; caller owns
 * it and closes it on cancellation/teardown. No host device tree is copied or
 * mounted. The setup helper must still supply private devpts/shm/mqueue and
 * procfs before using the links. SELinux and device-cgroup denials are fatal.
 * No runtime readiness or authorization is implied. Original deadline applies.
 */
int aegis_namespace_devices_mount(struct aegis_namespace *context);

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
