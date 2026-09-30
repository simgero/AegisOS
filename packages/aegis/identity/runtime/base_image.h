#ifndef AEGIS_BASE_IMAGE_H
#define AEGIS_BASE_IMAGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

struct aegis_base_receipt {
    uint64_t bytes;
    char sha256[65];
};
/* Strict, bounded parsing of our immutable build receipt. This is NOT a
 * signature verifier; only the trusted system_ext copy may authorize a mount. */
int aegis_base_parse_receipt(const char *json, size_t length, struct aegis_base_receipt *output);

/* Fixed system_ext inputs only: no CLI paths, arbitrary manifests or images.
 * Returns the owned O_PATH fsmount fd for a detached,
 * readonly/nosuid/nodev/noexec ext4 mount. Keep that file description alive
 * until attachment; reopening its root does not transfer mount ownership.
 * Source stays unmodified; no mount is attached by this function. The broker
 * attaches it in its own checked private namespace before cloning user views.
 * Requires the dedicated image/base SELinux types and production policy.
 * Caller must still establish trusted boot, daemon ownership, AOSP admission,
 * lifecycle recovery and helpers. Success alone does not start a runtime.
 * Closing the last mount/clone reference releases the autoclearing loop device.
 */
int aegis_base_open(void);
/* Same immutable validation/mount, also retain the EXACT verified source file
 * and receipt for later asynchronous generation selection. image starts -1;
 * outputs change only after complete success. Caller owns both returned FDs. */
int aegis_base_open_selection(int *image, struct aegis_base_receipt *receipt);

#ifdef __cplusplus
}
#endif
#endif
