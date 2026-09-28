// Generated from runtime/uid-map.json by scripts/runtime/uid_layout.py.
#ifndef AEGIS_RUNTIME_UID_LAYOUT_H
#define AEGIS_RUNTIME_UID_LAYOUT_H
#include <stdint.h>
#define AEGIS_PER_USER_RANGE 100000u
struct aegis_uid_extent { uint32_t inside, app_id, count; };
static const struct aegis_uid_extent aegis_uid_extents[] = {
    {0u, 5000u, 1000u},
    {1000u, 7500u, 1u},
    {65534u, 7501u, 1u}
};
#endif
