#ifndef AEGIS_RUNTIME_CHILD_H
#define AEGIS_RUNTIME_CHILD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Broker-side child ownership, not an authorization or namespace interface.
 * The launcher must obtain a process (not thread) pidfd atomically at creation
 * with CLONE_PIDFD. Only this owner may reap that child. Never reopen a numeric
 * PID from saved state, accept a pidfd from a client, or use SIGCHLD=SIG_IGN /
 * SA_NOCLDWAIT. Calls on one handle must be serialized by its owning process.
 */
struct aegis_child;

struct aegis_child_exit {
    int code;    /* CLD_EXITED, CLD_KILLED or CLD_DUMPED from waitid. */
    int status;  /* Exit code or signal number, NOT a waitpid-encoded status. */
};

/* Duplicate a verified, waitable child pidfd with CLOEXEC. *child must be NULL.
 * Does not reap, signal, close the caller's fd or assume the child is a runtime.
 * The caller retains responsibility for a child if adoption fails.
 */
int aegis_child_watch(int pidfd, struct aegis_child **child);

/* Request forced termination through the stable pidfd. Zero means requested
 * (or already reaped here), NEVER confirmation that a runtime has stopped.
 * The coordinator must first revoke starts and coordinate package operations.
 */
int aegis_child_request_stop(struct aegis_child *child);

/* Wait at most timeout_ms (0..60000) for this exact child's termination and
 * reap it. Only successful waitid produces success; socket EOF, signal success
 * or ESRCH do not. Result is unchanged on failure. ETIMEDOUT retains the handle
 * and can be retried. ECHILD (reaped elsewhere) remains an unverified failure.
 * A successful result is retained for repeat calls. Even a confirmed runtime
 * init exit still requires descriptor/mount teardown and AOSP CE confirmation.
 */
int aegis_child_wait(struct aegis_child *child, int timeout_ms,
                     struct aegis_child_exit *result);

/* Close this local reference only; neither kill nor reap. Releasing a live or
 * unverified handle abandons observation, so MUST NOT advance a context to
 * STOPPED or allow its resources/user ID to be reused. NULL is harmless.
 */
void aegis_child_release(struct aegis_child *child);

#ifdef __cplusplus
}
#endif
#endif
