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
int aegis_limit_supervisor(void);
int aegis_limit_shell(void);
int aegis_install_filter(void);

#ifdef __cplusplus
}
#endif

#endif
