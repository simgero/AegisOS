#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "mounts_private.h"
#include "uid_layout.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/mount.h>
#include <linux/nsfs.h>
#include <linux/sched.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <unistd.h>

static int denied(void) { errno = EPERM; return -1; }

int aegis_clone_base_mount(int source, int userns, uint32_t user_id) {
    if (user_id < 10 || user_id >= 21473) { errno = EINVAL; return -1; }
    struct stat before;
    struct statvfs source_flags;
    struct statfs source_fs;
    if (fstat(source, &before) < 0 || fstatvfs(source, &source_flags) < 0
            || fstatfs(source, &source_fs) < 0) return -1;
    /* This is a manifest-approved base directory, not a CE home or arbitrary
     * app-selected path. A supported filesystem is also checked by the kernel
     * when applying the ID map. Never fall back to recursive chown/bind/chroot.
     */
    if (!S_ISDIR(before.st_mode) || before.st_uid != 0 || before.st_gid != 0
            || (before.st_mode & 07777) != 0755 || !(source_flags.f_flag & ST_RDONLY))
        return denied();
    if (ioctl(userns, NS_GET_NSTYPE) != CLONE_NEWUSER) return denied();

    /* Intentionally nonrecursive: an incidental host mount BELOW the base
     * must not hitch a ride. Clone only the underlying base filesystem view. */
    int tree = (int)syscall(SYS_open_tree, source, "",
                            OPEN_TREE_CLONE | OPEN_TREE_CLOEXEC | AT_EMPTY_PATH);
    if (tree < 0) return -1;
    struct mount_attr attributes = {
        .attr_set = MOUNT_ATTR_RDONLY | MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV | MOUNT_ATTR_IDMAP,
        .attr_clr = MOUNT_ATTR_NOEXEC, /* Only the approved software view is executable. */
        .propagation = MS_PRIVATE,
        .userns_fd = (uint64_t)userns,
    };
    if (syscall(SYS_mount_setattr, tree, "", AT_EMPTY_PATH, &attributes, sizeof(attributes)) < 0)
        goto fail;
    struct stat after, unchanged;
    struct statvfs flags, source_after;
    struct statfs fs;
    if (fstat(tree, &after) < 0 || fstatvfs(tree, &flags) < 0 || fstatfs(tree, &fs) < 0
            || fstat(source, &unchanged) < 0 || fstatvfs(source, &source_after) < 0) goto fail;
    uint32_t root_id = UINT32_MAX;
    for (unsigned i = 0; i < sizeof(aegis_uid_extents) / sizeof(aegis_uid_extents[0]); i++) {
        if (aegis_uid_extents[i].inside == 0)
            root_id = user_id * AEGIS_PER_USER_RANGE + aegis_uid_extents[i].app_id;
    }
    if (root_id == UINT32_MAX || after.st_uid != root_id || after.st_gid != root_id
            || after.st_dev != before.st_dev || after.st_ino != before.st_ino
            || after.st_mode != before.st_mode || fs.f_type != source_fs.f_type
            || (flags.f_flag & (ST_RDONLY | ST_NOSUID | ST_NODEV))
                    != (ST_RDONLY | ST_NOSUID | ST_NODEV)
            || (flags.f_flag & ST_NOEXEC) || source_after.f_flag != source_flags.f_flag
            || unchanged.st_uid != before.st_uid || unchanged.st_gid != before.st_gid
            || unchanged.st_mode != before.st_mode || unchanged.st_dev != before.st_dev
            || unchanged.st_ino != before.st_ino) { errno = EPROTO; goto fail; }
    return tree;
fail:;
    int saved = errno;
    /* An unattached open_tree clone is destroyed when its last fd closes.
     * There is no path-based unmount and no mount in the broker's namespace. */
    close(tree);
    errno = saved;
    return -1;
}
