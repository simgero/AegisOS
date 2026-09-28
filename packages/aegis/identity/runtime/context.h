#ifndef AEGIS_RUNTIME_CONTEXT_H
#define AEGIS_RUNTIME_CONTEXT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Internal resource owner for the single-threaded, host-root AOSP broker.
 * NOT an authentication endpoint. Hold fresh AOSP id+serial/CE authorization
 * and lifecycle serialization across start and publication. Inputs are trusted
 * preopened system objects, never descriptors/paths supplied by CLI clients.
 */
struct aegis_context;

/* base_fd is an authenticated, immutable readonly ext4 generation root.
 * setup_fd/init_fd are authenticated static system ELFs. parent_fd is the
 * broker's private memory-cgroup parent. create_home is strictly 0/1.
 * No caller descriptor is consumed. Private cgroup/namespace references are
 * retained; temporary host code/CE/mount descriptor copies close before return.
 *
 * Success requires the exact private init READY reply. It is only a process
 * startup result, not AOSP authorization or an ongoing liveness guarantee.
 * *output starts NULL. Once allocated, it retains ALL cleanup ownership on
 * failure, even if no child was created. Failure closes temporary mounts and
 * sockets and requests termination; caller MUST finish stop() and cannot
 * admit another start for that identity until cleanup is confirmed.
 */
int aegis_context_start(uint32_t user, uint32_t serial, int parent_fd, int base_fd,
                        int setup_fd, int init_fd, int create_home,
                        struct aegis_context **output);

/* Borrowed private control fd, usable only by the owner after READY. Never
 * give it to a CLI client or close it. Caller owns protocol sequence numbers,
 * reply/PTY handling and fresh AOSP/session checks for every operation.
 * Refuses known-exited children; EOF/error still requires stop().
 */
int aegis_context_channel(struct aegis_context *context);

/* Seal, close the control channel, kill/wait-empty the memory group and reap
 * the child via its stable pidfd within ONE total 0..10000ms wait budget.
 * Only after both observations release namespaces and remove the group.
 * On success free/clear *context; on failure retain cleanup ownership, never
 * reopen admission. Retrying stop is allowed. No free-live-context operation.
 * This closes this owner's CE references, not unrelated broker/client refs,
 * package transactions or AOSP's CE key. They remain separate obligations.
 */
int aegis_context_stop(struct aegis_context **context, int timeout_ms);

#ifdef __cplusplus
}
#endif
#endif
