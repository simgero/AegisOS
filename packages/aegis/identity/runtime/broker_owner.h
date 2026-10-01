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

/* Startup-only after fixed policy. Bootstrap verifies both fixed immutable
 * executable paths, EROFS/readonly mount and exact SELinux labels first.
 * Retains planner/network helpers together, before any context or job. Internal
 * fixture callers may provide their trusted probe executables. No wire setter. */
int aegis_broker_owner_enable_package_planner(struct aegis_broker_owner *owner,
                                              int planner, int network);

/* Same startup-only contract, after planner configuration. Pins immutable
 * execute/publish helpers; the existing selection helper prepares candidates. */
int aegis_broker_owner_enable_package_installation(struct aegis_broker_owner *owner,
                                                   int execute, int publish);

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

/* START retains one public job across initial selection, optional configured
 * reconciliation, confirmed publication, reselection and activation. Internal
 * child IDs are never accepted as public continuations. EAGAIN means retained
 * pending work; CONTINUE_START requires the exact public job, user and serial
 * and MUST NOT re-create work removed by STOP/HELLO. No FD escapes. Other errors
 * clear the reply job and latch failure until STOP, retaining cleanup ownership.
 * With enabled bootstrap, the first START registers the fixed CE/shared selection.
 * Only configured stale selections may reconcile, at most once per start. */
int aegis_broker_owner_start(struct aegis_broker_owner *owner,
                             const struct aegis_broker_call *call,
                             uint64_t *job, enum aegis_broker_state *state);

/* Bounded read-only selection metadata, under the same fresh AOSP admission.
 * No image hashing, worker, persistent mutation or descriptor escapes. Unknown
 * observation never changes a running context or grants execution authority. */
int aegis_broker_owner_activation(struct aegis_broker_owner* owner,
    const struct aegis_broker_request* request,enum aegis_broker_state* state,
    struct aegis_activation_status* activation);

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

/* Package frames from the already authenticated and decoded system_server
 * connection. Caller holds fresh AOSP requester/session/CE admission; START
 * additionally requires fresh AOSP admin approval for its retained review.
 * Partial registration returns an owned job even on failure. No FD escapes.
 */
int aegis_broker_owner_package(struct aegis_broker_owner *owner,
                                const struct aegis_broker_call *call,
                                struct aegis_broker_package_reply *output);

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
