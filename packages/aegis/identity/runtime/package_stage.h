#ifndef AEGIS_PACKAGE_STAGE_H
#define AEGIS_PACKAGE_STAGE_H
#include <stdint.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Internal authority for one newly created root-owned directory, never a
 * caller path or an adopted prior job. Register this object before Create.
 * Even failure can leave an owned directory: retain it through Cleanup.
 * Cleanup is allowed only after every worker, mount and loop has quiesced.
 * It inspects at most the two fixed outer files; never follows links/recurses.
 * Failure retains ownership and must block completion/CE release. */
struct aegis_package_stage {
    int parent, directory;
    dev_t device;
    ino_t inode;
    unsigned removed;
    char name[96];
};
#define AEGIS_PACKAGE_STAGE_INIT {-1, -1, 0, 0, 0, {0}}
int aegis_package_stage_create(int parent, const char *name, struct aegis_package_stage *stage);
int aegis_package_stage_cleanup(struct aegis_package_stage *stage, uint64_t image_bytes,
                                uint64_t request_bytes);
/* Close references only, not files. For explicit ownership transfer/last
 * process teardown; normal broker completion must first succeed at Cleanup. */
void aegis_package_stage_close(struct aegis_package_stage *stage);
#ifdef __cplusplus
}
#endif
#endif
