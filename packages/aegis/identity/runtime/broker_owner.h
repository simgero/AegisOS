#ifndef AEGIS_BROKER_OWNER_H
#define AEGIS_BROKER_OWNER_H
#include "broker_protocol.h"
#include "base_image.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Single-threaded host-root owner behind the authenticated AOSP connection.
 * Owns up to 16 personal context slots and duplicates its trusted input FDs.
 * Owns prepared/running package publications too; not an auth endpoint.
 * Bootstrap MUST verify the
 * immutable generation/helpers and recover stale cgroups before construction.
 * No API accepts a CLI path, password or caller-supplied process identifier.
 */
struct aegis_broker_owner;
int aegis_broker_owner_create(int parent_fd, int base_fd, int setup_fd, int init_fd,
                              struct aegis_broker_owner **output);

/* Startup-only, before any context/job. Bootstrap has verified immutable
 * source/helper provenance and the exclusively locked fixed state directory.
 * Duplicates all three FDs; no personal CE FD persists in this configuration.
 * Once enabled, every first START must register CE+shared generation selection;
 * a lookup/selection error MUST NOT use the legacy factory-mount path. */
int aegis_broker_owner_enable_selection(struct aegis_broker_owner *owner,
                                        int image, const struct aegis_base_receipt *receipt,
                                        int helper, int state_directory);

/* Startup-only fixed Debian/AOSP policy from aegis_package_policy_open, after
 * selection bootstrap and before any work. Duplicates sources/key/CA together;
 * cannot replace live policy. Does not expose a package command or grant. */
int aegis_broker_owner_enable_package_policy(struct aegis_broker_owner *owner,
                                             const int inputs[3]);

/* Caller has already authenticated the system_server peer, validated framing
 * and enforced per-connection sequence ordering. AOSP admission/CE serialization
 * remains held by that peer. Success writes ABSENT/READY/SEALED as appropriate;
 * failure leaves *state=SEALED and retains ALL incomplete cleanup ownership.
 * HELLO stops all predecessor contexts AND package resources before acknowledging
 * a new connection. STOP_USER is absent only after both are fully released.
 * STOP_USER covers every retained serial for the numeric userId.
 */
int aegis_broker_owner_apply(struct aegis_broker_owner *owner,
                             const struct aegis_broker_request *request,
                             enum aegis_broker_state *state);

/* START may create a context from an already registered selection; its EAGAIN
 * reply returns that owned job. CONTINUE_START requires that exact live job,
 * user and serial and MUST NOT re-create work removed by STOP/HELLO. No FD
 * escapes. Other errors clear job, preserving incomplete cleanup ownership.
 * With enabled bootstrap, the first START registers the initial selection. */
int aegis_broker_owner_start(struct aegis_broker_owner *owner,
                             const struct aegis_broker_call *call,
                             uint64_t *job, enum aegis_broker_state *state);

/* The same fresh AOSP user+serial/session/CE admission is required here.
 * EXEC never implicitly starts a missing/sealed context. Returns an owned PTY
 * and a process-lifetime unique command ID, NOT an authentication capability.
 * START/STOP cannot recycle a command ID into a later context of the same user.
 * Outputs begin 0/-1. RESULT consumes one finished status; exited=0 is running.
 * The AOSP caller must separately bind every PTY/command to its admitted CLI
 * session and revoke access on session loss. No personal auth happens here.
 */
int aegis_broker_owner_exec(struct aegis_broker_owner *owner,
                            const struct aegis_broker_call *call,
                            uint64_t *command, int *master);
int aegis_broker_owner_result(struct aegis_broker_owner *owner,
                              const struct aegis_broker_request *request,
                              uint64_t command, int *wait_status, int *exited);

/* Nonblocking housekeeping of owned publication children; never authenticates
 * a caller or confirms CE eviction. Incomplete cleanup remains owned. */
int aegis_broker_owner_reap_publications(struct aegis_broker_owner *owner);

/* On disconnect/shutdown: visit EVERY context/publication, including after a timeout, so
 * all channels seal and termination is requested. One total wait deadline;
 * expired deadline means no waiting, never unconditional success.
 */
int aegis_broker_owner_stop_all(struct aegis_broker_owner *owner, uint64_t deadline_ns);
/* Refuse to free while any context/partial start survives. No implicit stop. */
int aegis_broker_owner_release(struct aegis_broker_owner **owner);

#ifdef __cplusplus
}
#endif
#endif
