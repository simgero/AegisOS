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

/* Broker-owned package storage, sibling of HOME, using the same authoritative
 * AOSP serial/policy/present-key and immutable owner checks. Packages, store
 * and staging are root:root0700; never mount these into an ordinary session.
 * No caller-chosen paths. Interrupted creation is preserved and rejected.
 * The caller MUST hold lifecycle admission, register returned references with
 * STOP_USER, and close every copy/mount/loop before releasing CE. These APIs
 * do not authenticate, issue keys or validate package semantics/approval. */
int aegis_ce_open_packages(int data_fd, uint32_t user_id, uint32_t serial, int create);
/* packages comes only from the above function, in the SAME admission. */
int aegis_ce_package_store(int packages, uint32_t user_id, uint32_t serial);
/* Read-only lookup under the same fresh admission/namespace requirements.
 * Verifies authoritative AOSP serial/policy/key before accepting absent AEGIS
 * storage or its newly provisioned pristine empty store. Success may leave
 * *store=-1; missing AOSP roots, locked CE and partial storage are errors.
 * Never infer absence from a generic ENOENT, never create/repair a directory.
 * output initially -1, stays unchanged on failure. Register BEFORE calling. */
int aegis_ce_find_package_store(int data_fd,uint32_t user_id,uint32_t serial,int* store);
/* Creates a new empty directory; no reuse, repair or cleanup of old jobs. */
int aegis_ce_new_package_stage(int packages, uint32_t user_id, uint32_t serial, uint64_t job);

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
