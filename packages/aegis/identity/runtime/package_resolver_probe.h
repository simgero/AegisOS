#ifndef AEGIS_PACKAGE_RESOLVER_PROBE_H
#define AEGIS_PACKAGE_RESOLVER_PROBE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Device-only fixture transport, never a production or client interface.
struct aegis_resolver_probe_reply { uint32_t magic, phase, status, error, count; };
int aegis_probe_package_resolver(uint32_t user);
#ifdef __cplusplus
}
#endif
#endif
