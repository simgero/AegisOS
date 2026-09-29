#ifndef AEGIS_PACKAGE_EXECUTION_PROTOCOL_H
#define AEGIS_PACKAGE_EXECUTION_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define AEGIS_PACKAGE_EXEC_MAGIC UINT32_C(0x41455045)
#define AEGIS_PACKAGE_EXEC_VERSION 1u
#define AEGIS_PACKAGE_EXEC_ITEMS 64u
#define AEGIS_PACKAGE_EXEC_NAME 160u
#define AEGIS_PACKAGE_EXEC_FDS 3u
#define AEGIS_PACKAGE_ARCHIVES 1u
#define AEGIS_PACKAGE_REMOVE 2u
#define AEGIS_PACKAGE_EXEC_READY 1u
#define AEGIS_PACKAGE_EXEC_DONE 2u
struct aegis_package_execution_request {
    uint32_t magic, version, user, serial;
    uint64_t job;
    uint32_t kind, count;
    char plan[65];
    uint8_t reserved[7];
    char items[AEGIS_PACKAGE_EXEC_ITEMS][AEGIS_PACKAGE_EXEC_NAME];
};
struct aegis_package_execution_reply {
    uint32_t magic, version, user, serial;
    uint64_t job;
    char plan[65];
    uint8_t reserved[3];
    uint32_t phase, status, error;
};
static inline int aegis_package_zero(const void *data, size_t count) {
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < count; i++) if (bytes[i]) return 0;
    return 1;
}
static inline int aegis_package_hash(const char hash[65]) {
    if (hash[64]) return 0;
    for (unsigned i = 0; i < 64; i++)
        if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f'))) return 0;
    return 1;
}
static inline int aegis_package_item(const char name[AEGIS_PACKAGE_EXEC_NAME], uint32_t kind) {
    size_t length = strnlen(name, AEGIS_PACKAGE_EXEC_NAME);
    if (length < 2 || length == AEGIS_PACKAGE_EXEC_NAME
            || !aegis_package_zero(name + length, AEGIS_PACKAGE_EXEC_NAME - length)) return 0;
    if (!(name[0] >= 'a' && name[0] <= 'z') && !(name[0] >= '0' && name[0] <= '9')) return 0;
    for (size_t i = 0; i < length; i++) {
        char c = name[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.') continue;
        if (kind == AEGIS_PACKAGE_ARCHIVES
                && ((c >= 'A' && c <= 'Z') || c == '_' || c == '~' || c == ':' || c == '%')) continue;
        return 0;
    }
    return kind == AEGIS_PACKAGE_REMOVE || (length > 4 && !strcmp(name + length - 4, ".deb"));
}
static inline int aegis_package_execution_valid(const struct aegis_package_execution_request *r) {
    if (r->magic != AEGIS_PACKAGE_EXEC_MAGIC || r->version != AEGIS_PACKAGE_EXEC_VERSION
            || r->user < 10 || r->user >= 21473 || r->serial > INT32_MAX
            || !r->job || r->job > INT64_MAX || !aegis_package_hash(r->plan)
            || !aegis_package_zero(r->reserved, sizeof(r->reserved))
            || (r->kind != AEGIS_PACKAGE_ARCHIVES && r->kind != AEGIS_PACKAGE_REMOVE)
            || !r->count || r->count > AEGIS_PACKAGE_EXEC_ITEMS) return 0;
    for (unsigned i = 0; i < AEGIS_PACKAGE_EXEC_ITEMS; i++) {
        if (i >= r->count) { if (!aegis_package_zero(r->items[i], sizeof(r->items[i]))) return 0;continue; }
        if (!aegis_package_item(r->items[i], r->kind)) return 0;
        for (unsigned j = 0; j < i; j++) if (!strcmp(r->items[i], r->items[j])) return 0;
    }
    return 1;
}
#ifdef __cplusplus
extern "C" {
#endif
/* Private committed transfer: readonly sealed request + candidate/device
 * mounts from the owning broker. Sender retains its FDs. No CLI endpoint. */
int aegis_package_execution_send(int channel, uint32_t user, uint32_t serial, uint64_t job,
                                 const int fds[AEGIS_PACKAGE_EXEC_FDS]);
/* Receiver retains partial received FDs on failure; caller must close them. */
int aegis_package_execution_receive(int channel, uint32_t user, uint32_t serial,
                                    int fds[3], struct aegis_package_execution_request *request);
/* Trusted namespace PID1 core; production entry separately requires the exact
 * package SELinux domain. This never takes an arbitrary command or script. */
int aegis_package_execute(uint32_t user, uint32_t serial);
#ifdef __cplusplus
}
#endif
#endif
