#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "mounts_private.h"
#include "uid_layout.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/mount.h>
#include <linux/nsfs.h>
#include <linux/sched.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>

/* Fixed kernel interfaces, not device paths supplied by a client. No block,
 * graphics, input, network, Binder, storage or host-terminal devices. */
static const struct { const char *name; unsigned major, minor; } nodes[] = {
    {"null", 1, 3}, {"zero", 1, 5}, {"full", 1, 7},
    {"random", 1, 8}, {"urandom", 1, 9}, {"tty", 5, 0},
};
static const struct { const char *name; mode_t mode; } directories[] = {
    {"pts", 0755}, {"shm", 01777}, {"mqueue", 01777},
};
static const struct { const char *name, *target; } links[] = {
    {"ptmx", "pts/ptmx"}, {"fd", "/proc/self/fd"},
    {"stdin", "/proc/self/fd/0"}, {"stdout", "/proc/self/fd/1"},
    {"stderr", "/proc/self/fd/2"},
};
#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

static int invalid(void) { errno = EPROTO; return -1; }

static int known(const char *name) {
    for (unsigned i = 0; i < COUNT(nodes); i++) if (!strcmp(name, nodes[i].name)) return 1;
    for (unsigned i = 0; i < COUNT(directories); i++) if (!strcmp(name, directories[i].name)) return 1;
    for (unsigned i = 0; i < COUNT(links); i++) if (!strcmp(name, links[i].name)) return 1;
    return 0;
}

static int ownership(const struct stat *st, uint32_t root_id, mode_t mode) {
    return st->st_uid == root_id && st->st_gid == root_id
            && st->st_mode == mode ? 0 : invalid();
}

static int verify(int tree, uint32_t root_id) {
    int fd = openat(tree, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    DIR *directory = fdopendir(fd);
    if (!directory) { int saved = errno; close(fd); errno = saved; return -1; }
    int result = -1;
    struct stat st;
    struct statfs fs;
    struct statvfs flags;
    if (fstat(fd, &st) < 0 || fstatfs(fd, &fs) < 0 || fstatvfs(fd, &flags) < 0) goto done;
    if (ownership(&st, root_id, S_IFDIR | 0755) < 0) goto done;
    if (fs.f_type != TMPFS_MAGIC
            || (flags.f_flag & (ST_RDONLY | ST_NOSUID | ST_NOEXEC))
                    != (ST_RDONLY | ST_NOSUID | ST_NOEXEC)
            || (flags.f_flag & ST_NODEV)) { invalid(); goto done; }
    unsigned count = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(directory))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (!known(entry->d_name)) { invalid(); goto done; }
        count++;
    }
    if (errno) goto done;
    if (count != COUNT(nodes) + COUNT(directories) + COUNT(links)) { invalid(); goto done; }
    for (unsigned i = 0; i < COUNT(nodes); i++) {
        if (fstatat(fd, nodes[i].name, &st, AT_SYMLINK_NOFOLLOW) < 0
                || ownership(&st, root_id, S_IFCHR | 0666) < 0) goto done;
        if (st.st_nlink != 1 || st.st_rdev != makedev(nodes[i].major, nodes[i].minor)) {
            invalid(); goto done;
        }
    }
    for (unsigned i = 0; i < COUNT(directories); i++) {
        if (fstatat(fd, directories[i].name, &st, AT_SYMLINK_NOFOLLOW) < 0
                || ownership(&st, root_id, S_IFDIR | directories[i].mode) < 0) goto done;
    }
    for (unsigned i = 0; i < COUNT(links); i++) {
        char target[64];
        if (fstatat(fd, links[i].name, &st, AT_SYMLINK_NOFOLLOW) < 0
                || ownership(&st, root_id, S_IFLNK | 0777) < 0) goto done;
        ssize_t length = readlinkat(fd, links[i].name, target, sizeof(target));
        if (length < 0) goto done;
        if ((size_t)length != strlen(links[i].target)
                || memcmp(target, links[i].target, (size_t)length)) { invalid(); goto done; }
    }
    result = 0;
done:;
    int saved = errno;
    closedir(directory);
    errno = saved;
    return result;
}

int aegis_create_devices_mount(int userns, uint32_t user_id) {
    if (user_id < 10 || user_id >= 21473) { errno = EINVAL; return -1; }
    if (ioctl(userns, NS_GET_NSTYPE) != CLONE_NEWUSER) { errno = EPERM; return -1; }
    uint32_t root_id = UINT32_MAX;
    for (unsigned i = 0; i < COUNT(aegis_uid_extents); i++) {
        if (aegis_uid_extents[i].inside == 0)
            root_id = user_id * AEGIS_PER_USER_RANGE + aegis_uid_extents[i].app_id;
    }
    if (root_id == UINT32_MAX) { errno = EINVAL; return -1; }
    int fs = (int)syscall(SYS_fsopen, "tmpfs", FSOPEN_CLOEXEC);
    if (fs < 0) return -1;
    int tree = -1, root = -1;
    const char *parameters[][2] = {
        {"size", "1048576"}, {"nr_inodes", "128"}, {"mode", "0755"},
        {"uid", "0"}, {"gid", "0"},
    };
    for (unsigned i = 0; i < COUNT(parameters); i++) {
        if (syscall(SYS_fsconfig, fs, FSCONFIG_SET_STRING,
                    parameters[i][0], parameters[i][1], 0) < 0) goto fail;
    }
    if (syscall(SYS_fsconfig, fs, FSCONFIG_CMD_CREATE, NULL, NULL, 0) < 0) goto fail;
    tree = (int)syscall(SYS_fsmount, fs, FSMOUNT_CLOEXEC,
                        MOUNT_ATTR_NOSUID | MOUNT_ATTR_NOEXEC);
    if (tree < 0) goto fail;
    close(fs); fs = -1;
    /* fsmount returns O_PATH. Do not use it directly with chmod/readdir.
     * This new tree is detached and known only to this single-threaded broker. */
    root = openat(tree, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (root < 0) goto fail;
    for (unsigned i = 0; i < COUNT(nodes); i++) {
        if (mknodat(root, nodes[i].name, S_IFCHR | 0600, makedev(nodes[i].major, nodes[i].minor)) < 0
                || fchmodat(root, nodes[i].name, 0666, 0) < 0) goto fail;
    }
    for (unsigned i = 0; i < COUNT(directories); i++) {
        if (mkdirat(root, directories[i].name, 0700) < 0
                || fchmodat(root, directories[i].name, directories[i].mode, 0) < 0) goto fail;
    }
    for (unsigned i = 0; i < COUNT(links); i++) {
        if (symlinkat(links[i].target, root, links[i].name) < 0) goto fail;
    }
    close(root); root = -1;
    struct mount_attr attr = {
        .attr_set = MOUNT_ATTR_RDONLY | MOUNT_ATTR_NOSUID | MOUNT_ATTR_NOEXEC | MOUNT_ATTR_IDMAP,
        .attr_clr = MOUNT_ATTR_NODEV, /* Only the six fixed character devices. */
        .propagation = MS_PRIVATE,
        .userns_fd = (uint64_t)userns,
    };
    if (syscall(SYS_mount_setattr, tree, "", AT_EMPTY_PATH, &attr, sizeof(attr)) < 0
            || verify(tree, root_id) < 0) goto fail;
    return tree;
fail:;
    int saved = errno;
    if (root >= 0) close(root);
    if (tree >= 0) close(tree);
    if (fs >= 0) close(fs);
    errno = saved;
    return -1;
}
