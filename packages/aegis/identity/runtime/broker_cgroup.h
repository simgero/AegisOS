#ifndef AEGIS_BROKER_CGROUP_H
#define AEGIS_BROKER_CGROUP_H
#ifdef __cplusplus
extern "C" {
#endif

#define AEGIS_BROKER_DELEGATION "aegis-runtime"
#define AEGIS_BROKER_OWNER "broker"
#define AEGIS_BROKER_CGROUP "contexts"
/* Validate Init's fixed private delegation and this process's exclusive owner
 * leaf before touching stale contexts. Never moves a task or changes a limit.
 * Returns the delegation directory and a separate cgroup.procs reference in
 * *entry (initially -1). Keep BOTH until the broker/context owner is destroyed:
 * clone3 checks this common ancestor's inode without doing a VFS path lookup.
 * The caller supplies the trusted Android cgroup mount root, never a client FD.
 */
int aegis_broker_cgroup_open_delegation(int root, int *entry);
/* Startup-only, while holding the exclusive broker lock and before listening.
 * Validated private delegation FD; never supplied by a client. Creates/validates the
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
