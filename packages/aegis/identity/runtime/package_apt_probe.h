#ifndef AEGIS_PACKAGE_APT_PROBE_H
#define AEGIS_PACKAGE_APT_PROBE_H
#include <stdint.h>
struct aegis_apt_probe_result { uint32_t magic, phase, status, error; };
/* Fixed, trusted offline fixture; never a production package entrypoint. */
int aegis_probe_package_apt(uint32_t user);
#endif
