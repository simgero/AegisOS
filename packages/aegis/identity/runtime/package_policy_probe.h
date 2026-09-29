#ifndef AEGIS_PACKAGE_POLICY_PROBE_H
#define AEGIS_PACKAGE_POLICY_PROBE_H
#include <stdint.h>
/* Inert local-device fixture; never a production worker entrypoint. */
struct aegis_package_policy_probe {
    uint32_t magic, user, checks, nnp, securebits, filter;
    uint64_t effective, permitted, inheritable, bounding, ambient;
};
int aegis_probe_package_policy(uint32_t user);
int aegis_probe_package_policy_exec(uint32_t user);
#endif
