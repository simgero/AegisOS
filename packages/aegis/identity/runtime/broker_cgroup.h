#ifndef AEGIS_BROKER_CGROUP_H
#define AEGIS_BROKER_CGROUP_H
#ifdef __cplusplus
extern "C" {
#endif

#define AEGIS_BROKER_CGROUP "aegis-runtime"
/* Startup-only, while holding the exclusive broker lock and before listening.
 * Trusted cgroup-v2 root FD; never supplied by a client. Creates/validates the
 * fixed private parent, kills predecessor members, proves populated=0, removes
 * only canonical personal child groups, then configures aggregate limits.
 * Does NOT change controllers at the shared Android root or move the broker.
 * Returns an owned directory FD. Failure never authorizes HELLO/CE eviction.
 * Runtime child pidfd reaping is handled separately by the live context owner.
 */
int aegis_broker_cgroup_prepare(int root, int timeout_ms);

#ifdef __cplusplus
}
#endif
#endif
