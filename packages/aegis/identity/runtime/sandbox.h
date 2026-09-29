#ifndef AEGIS_RUNTIME_SANDBOX_H
#define AEGIS_RUNTIME_SANDBOX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Additional preconditions, not a replacement for the trusted AOSP broker's
 * authentication, CE, namespace, mount, SELinux and resource-limit setup. */
int aegis_check_context(uint32_t user_id);
/* Setup-only guard, before any mount/descriptor changes. Requires the new
 * namespaces but still the inherited Android proc view and exact setup domain. */
int aegis_check_setup_context(uint32_t user_id);
/* Package entry requires allow maps and the exact enforcing package domain.
 * The namespace-only half exists for the separately compiled DEVICE TEST entry;
 * it is not sufficient for production activation. No runtime bypass flag. */
int aegis_check_package_namespaces(uint32_t user_id);
int aegis_check_package_context(uint32_t user_id);
int aegis_limit_supervisor(void);
int aegis_limit_shell(void);
/* Package-worker PID1 only, AFTER trusted setup has detached Android's root,
 * provided an exclusively owned candidate and verified its SELinux domain.
 * Requires explicit package preparation with the exact map and setgroups=allow.
 * Keeps only mapped CHOWN/DAC_OVERRIDE/FOWNER/FSETID/SETUID/SETGID across exec.
 * This grants no host capabilities, AOSP authority, mounts or package approval.
 * Drops all other bounding bits, locks root/ambient escalation, sets NNP and
 * installs the same mount/namespace/kernel-operation filter as ordinary GNU.
 * Any partial failure is fatal to that worker; never retry or execute packages.
 */
int aegis_limit_package_worker(uint32_t user_id);

int aegis_install_filter(void);

#ifdef __cplusplus
}
#endif

#endif
