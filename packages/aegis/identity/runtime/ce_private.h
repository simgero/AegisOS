#ifndef AEGIS_RUNTIME_CE_PRIVATE_H
#define AEGIS_RUNTIME_CE_PRIVATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Private broker implementation. Never accept an application-supplied data
 * root, identity or authorization boolean. The namespace wrapper checks the
 * initial Android namespaces and owns the immutable id+serial binding.
 * data_fd must be its freshly opened /data; caller serializes AOSP lifecycle.
 * Returns a new CLOEXEC home fd, not an authentication or CE-lock proof.
 */
int aegis_ce_open_home(int data_fd, uint32_t user_id, uint32_t serial, int create);

/* Strictly reads AOSP's existing user.serial; never repairs or creates it. */
int aegis_ce_require_serial(int directory, uint32_t serial);

/* No symlinks, magic links or mount crossings beneath an anchored directory. */
int aegis_ce_open_directory(int parent, const char *relative);

/* Create the initial standard directories in an empty root-owned staging
 * home only. Internal layout operation, not authorization or CE validation;
 * the provisioning caller checks AOSP CE policy before publication. */
int aegis_ce_create_home_layout(int home, uint32_t user_id);

/* Clone a validated personal home without applying a second UID map. */
int aegis_ce_clone_home(int home, uint32_t user_id);

#ifdef __cplusplus
}
#endif
#endif
