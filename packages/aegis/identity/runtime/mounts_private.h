#ifndef AEGIS_RUNTIME_MOUNTS_PRIVATE_H
#define AEGIS_RUNTIME_MOUNTS_PRIVATE_H
#include <stdint.h>

/* Implementation-only. namespace.c supplies its own checked user-namespace
 * reference and selected user. Neither value comes from a CLI/client fd. */
int aegis_clone_base_mount(int source, int userns, uint32_t user_id);

/* Fresh bounded tmpfs containing only fixed standard character devices,
 * private-mount placeholders and fixed proc/PTY links. No host /dev bind. */
int aegis_create_devices_mount(int userns, uint32_t user_id);

#endif
