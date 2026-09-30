#ifndef AEGIS_PACKAGE_POLICY_H
#define AEGIS_PACKAGE_POLICY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Bootstrap only: factory is the already authenticated readonly Debian mount,
 * not a caller descriptor. Fixed Debian 13 sources and archive keys, plus the
 * verified readonly AOSP Conscrypt CA set. Returns three readonly sealed memfds
 * (sources, key, CA); outputs initially -1, unchanged on any failure. No network,
 * user files, host certificate overrides or downloaded trust input is accepted. */
int aegis_package_policy_open(int factory, int output[3]);
/* Internal worker-input validation. Regular root-owned readonly files remain
 * valid for trusted fixtures; unlinked files must be fully sealed memfds.
 * This checks integrity of an input, not its provenance or authorization. */
int aegis_package_policy_file(int fd, uint64_t maximum);
#ifdef __cplusplus
}
#endif
#endif
