#ifndef AEGIS_RUNTIME_EXEC_H
#define AEGIS_RUNTIME_EXEC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Private command channel AFTER the namespace supervisor's authenticated READY.
 * The context owner retains AOSP admission, user/serial/CE checks and lifetime
 * ownership. Never accept this socket from a CLI or expose it to one.
 * Owns a CLOEXEC duplicate, sequence and up to 32 uncollected command results.
 * All calls belong to the creating single-threaded host-root process.
 */
struct aegis_exec;
int aegis_exec_create(int channel, uint32_t user, struct aegis_exec **output);
int aegis_exec_healthy(struct aegis_exec *owner);
int aegis_exec_close(struct aegis_exec **owner);

/* Absolute CLOCK_MONOTONIC deadline, at most ten seconds away. Success hands
 * the caller a CLOEXEC private PTY master and nonzero command identity; it does
 * not mean the program has completed. Caller owns the returned PTY and must
 * coordinate its lifetime with the personal session. Outputs must start 0/-1.
 * A malformed/uncertain exchange seals this channel; never retry on it.
 * A well-formed supervisor error leaves it usable. Arguments are never logged.
 */
int aegis_exec_start(struct aegis_exec *owner, size_t argc, const char *const *argv,
                     uint64_t deadline_ns, uint64_t *command, int *master);

/* Nonblocking: collect exactly one completed waitpid status. EAGAIN means the
 * known command is still running; ENOENT means unknown/already collected.
 * Drains and retains other commands' exit reports without discarding them.
 * A terminal channel error is NOT a program exit or a runtime cleanup proof.
 */
int aegis_exec_result(struct aegis_exec *owner, uint64_t command, int *wait_status);

#ifdef __cplusplus
}
#endif
#endif
