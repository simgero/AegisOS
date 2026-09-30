#ifndef AEGIS_RUNTIME_MEMORY_GROUP_H
#define AEGIS_RUNTIME_MEMORY_GROUP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Internal host-root broker resource, never a client authorization endpoint.
 * The caller serializes AOSP lifecycle and owns an empty, root:root 0700
 * cgroup-v2 parent with memory already enabled in cgroup.subtree_control.
 * No controller is enabled/changed on Android's shared root by this library.
 */
struct aegis_memory_group;

/* Creates only u<user>-s<serial>, exclusively; never adopts existing groups.
 * 1 GiB hard / 768 MiB high / no swap / group OOM. No process enters here.
 * *output starts NULL. If a newly created directory survives a failure,
 * output retains ownership: caller must kill/wait-empty and remove it.
 * Preparation failure never yields a usable clone target.
 */
int aegis_memory_group_create(int parent_fd, uint32_t user, uint32_t serial,
                              struct aegis_memory_group **output);
/* Package workers use a separate canonical leaf for the same identity. This
 * keeps their cancellation/limits independent of an open personal runtime.
 * Never adopts an existing leaf; the broker permits one package job per user. */
int aegis_memory_group_create_package(int parent_fd, uint32_t user, uint32_t serial,
                                      struct aegis_memory_group **output);

/* Claim once for this exact id+serial. Returns a borrowed CLOEXEC directory fd
 * for CLONE_INTO_CGROUP ONLY; never transfer to a client or close it. Rechecks
 * identity/limits and requires an empty group. A successful claim seals the
 * group even if clone3 fails: remove it before another start. A single-threaded
 * owner must keep the object alive across clone3.
 */
int aegis_memory_group_claim(struct aegis_memory_group *group, uint32_t user, uint32_t serial);

/* One trusted network companion after the primary claim, in the SAME budget.
 * Root owner only, exact identity, populated/unchanged limits, one attempt;
 * never available after cancellation. Caller retains and reaps its own pidfd. */
int aegis_memory_group_claim_companion(struct aegis_memory_group *group, uint32_t user, uint32_t serial);

/* Signal this owned cgroup tree, then observe populated=0 within 0..10000ms.
 * Timeout retains all references. This does NOT reap an owned child, close
 * CE references or prove AOSP key removal. Those are separate requirements.
 */
int aegis_memory_group_kill_and_wait(struct aegis_memory_group *group, int timeout_ms);

/* Remove only this same, empty directory, then free/clear *group. On any
 * failure retain it; never infer success from a vanished/replaced directory.
 * There is intentionally no abandon/free-live-group API.
 */
int aegis_memory_group_remove(struct aegis_memory_group **group);

#ifdef __cplusplus
}
#endif
#endif
