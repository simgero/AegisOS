#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "ce_private.h"
#include "uid_layout.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fs.h>
#include <linux/fscrypt.h>
#include <linux/mount.h>
#include <linux/openat2.h>
#include <private/android_filesystem_config.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>

static const char anchor_name[] = "aegis";
static const char pending_name[] = ".aegis-preparing";
static const char owner_attribute[] = "user.aegis.owner";

static int reject(int error) { errno = error; return -1; }

int aegis_ce_open_directory(int parent, const char *relative) {
    struct open_how how = {
        .flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
        .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_XDEV,
    };
    return (int)syscall(SYS_openat2, parent, relative, &how, sizeof(how));
}

static int exact_attribute(int fd, const char *name, const char *expected) {
    char actual[64];
    ssize_t size = fgetxattr(fd, name, actual, sizeof(actual));
    if (size < 0) return -1;
    return (size_t)size == strlen(expected) && !memcmp(actual, expected, (size_t)size)
            ? 0 : reject(ESTALE);
}

int aegis_ce_require_serial(int directory, uint32_t serial) {
    if (serial > INT32_MAX) return reject(EINVAL);
    char expected[16];
    snprintf(expected, sizeof(expected), "%u", serial);
    return exact_attribute(directory, "user.serial", expected);
}

static int metadata(int fd, uid_t uid, gid_t gid, mode_t mode) {
    struct stat st;
    if (fstat(fd, &st) < 0) return -1;
    return S_ISDIR(st.st_mode) && st.st_uid == uid && st.st_gid == gid
            && (st.st_mode & 07777) == mode ? 0 : reject(EPERM);
}

static int no_acl(int fd) {
    const char *names[] = {"system.posix_acl_access", "system.posix_acl_default"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (fgetxattr(fd, names[i], NULL, 0) >= 0) return reject(EPERM);
        if (errno != ENODATA && errno != EOPNOTSUPP) return -1;
    }
    return 0;
}

static int policy(int fd, struct fscrypt_policy_v2 *output) {
    struct fscrypt_get_policy_ex_arg arg = {.policy_size = sizeof(arg.policy)};
    if (ioctl(fd, FS_IOC_GET_ENCRYPTION_POLICY_EX, &arg) < 0) return -1;
    if (arg.policy_size != sizeof(*output) || arg.policy.version != FSCRYPT_POLICY_V2)
        return reject(EOPNOTSUPP);
    const unsigned char zero[3] = {0};
    if (memcmp(arg.policy.v2.__reserved, zero, sizeof(zero))) return reject(EPROTO);
    *output = arg.policy.v2;
    return 0;
}

static int matching_policy(int fd, const struct fscrypt_policy_v2 *expected) {
    struct fscrypt_policy_v2 actual;
    if (policy(fd, &actual) < 0) return -1;
    return !memcmp(&actual, expected, sizeof(actual)) ? 0 : reject(ESTALE);
}

static int key_present(int fd, const struct fscrypt_policy_v2 *expected) {
    struct fscrypt_get_key_status_arg arg = {0};
    arg.key_spec.type = FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER;
    memcpy(arg.key_spec.u.identifier, expected->master_key_identifier,
           sizeof(arg.key_spec.u.identifier));
    if (ioctl(fd, FS_IOC_GET_ENCRYPTION_KEY_STATUS, &arg) < 0) return -1;
    /* INCOMPLETELY_REMOVED is NOT PRESENT. This reads public key metadata,
     * never key material, and is NOT evidence of fresh AOSP authentication. */
    return arg.status == FSCRYPT_KEY_STATUS_PRESENT ? 0 : reject(ENOKEY);
}

static int same_directory(int first, int second) {
    struct stat a, b;
    if (fstat(first, &a) < 0 || fstat(second, &b) < 0) return -1;
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino ? 0 : reject(ESTALE);
}

static int still_named(int parent, const char *name, int fd) {
    int current = aegis_ce_open_directory(parent, name);
    if (current < 0) return -1;
    int result = same_directory(current, fd), saved = errno;
    close(current);
    errno = saved;
    return result;
}

static int absent(int parent, const char *name) {
    struct stat st;
    if (fstatat(parent, name, &st, AT_SYMLINK_NOFOLLOW) == 0) return reject(EEXIST);
    return errno == ENOENT ? 0 : -1;
}

static uint32_t home_uid(uint32_t user_id) {
    for (unsigned i = 0; i < sizeof(aegis_uid_extents) / sizeof(aegis_uid_extents[0]); i++) {
        const struct aegis_uid_extent *row = &aegis_uid_extents[i];
        if (1000u >= row->inside && 1000u - row->inside < row->count)
            return user_id * AEGIS_PER_USER_RANGE + row->app_id + 1000u - row->inside;
    }
    return UINT32_MAX;
}

static const char *const home_directories[] = {
    "Desktop", "Documents", "Downloads", "Pictures", "Videos", "Music", "Books",
    ".config", ".local", ".cache",
};

int aegis_ce_create_home_layout(int home, uint32_t user_id) {
    if (user_id < 10 || user_id >= 21473) return reject(EINVAL);
    uint32_t uid = home_uid(user_id);
    if (uid == UINT32_MAX) return reject(EINVAL);
    /* Only an empty, still root-owned unpublished staging home is eligible.
     * Never fill in, repair or overwrite a previously published user's HOME. */
    if (metadata(home, 0, 0, 0700) < 0 || no_acl(home) < 0) return -1;
    int scan = aegis_ce_open_directory(home, ".");
    if (scan < 0) return -1;
    DIR *directory = fdopendir(scan);
    if (!directory) { int saved = errno; close(scan); return reject(saved); }
    int error = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            error = EEXIST;
            break;
        }
    }
    if (!error) error = errno;
    if (closedir(directory) < 0 && !error) error = errno;
    if (error) return reject(error);
    for (unsigned i = 0; i < sizeof(home_directories) / sizeof(home_directories[0]); i++) {
        const char *name = home_directories[i];
        if (mkdirat(home, name, 0700) < 0) return -1;
        int child = aegis_ce_open_directory(home, name);
        if (child < 0) return -1;
        int result = -1;
        if (metadata(child, 0, 0, 0700) == 0 && no_acl(child) == 0
                && fchown(child, uid, uid) == 0 && fchmod(child, 0700) == 0
                && metadata(child, uid, uid, 0700) == 0 && no_acl(child) == 0
                && fsync(child) == 0 && still_named(home, name, child) == 0) result = 0;
        int saved = errno;
        close(child);
        if (result < 0) return reject(saved);
    }
    return fsync(home);
}

static int check_home(int fd, uint32_t uid, const struct fscrypt_policy_v2 *expected) {
    if (metadata(fd, uid, uid, 0700) < 0 || no_acl(fd) < 0) return -1;
    return matching_policy(fd, expected);
}

static int check_anchor(int fd, const char *owner, const struct fscrypt_policy_v2 *expected) {
    if (metadata(fd, 0, 0, 0700) < 0 || no_acl(fd) < 0
            || exact_attribute(fd, owner_attribute, owner) < 0) return -1;
    return matching_policy(fd, expected);
}

static int provision(int misc, uint32_t user_id, uint32_t uid, const char *owner,
                     const struct fscrypt_policy_v2 *expected) {
    /* Caller holds the per-identity lifecycle lock. A leftover staging tree is
     * never adopted, overwritten, repaired or removed automatically. It can
     * require explicit recovery after an interrupted first provisioning. */
    if (mkdirat(misc, pending_name, 0700) < 0) return -1;
    int pending = aegis_ce_open_directory(misc, pending_name), home = -1, result = -1;
    if (pending < 0) return -1;
    if (metadata(pending, 0, 0, 0700) < 0 || no_acl(pending) < 0
            || matching_policy(pending, expected) < 0
            || fsetxattr(pending, owner_attribute, owner, strlen(owner), XATTR_CREATE) < 0
            || mkdirat(pending, "home", 0700) < 0) goto done;
    home = aegis_ce_open_directory(pending, "home");
    if (home < 0 || metadata(home, 0, 0, 0700) < 0 || no_acl(home) < 0
            || matching_policy(home, expected) < 0
            || aegis_ce_create_home_layout(home, user_id) < 0) goto done;
    /* Each new directory must inherit the exact AOSP CE policy before the
     * staging anchor is published. Layout creation itself is not CE proof. */
    for (unsigned i = 0; i < sizeof(home_directories) / sizeof(home_directories[0]); i++) {
        int child = aegis_ce_open_directory(home, home_directories[i]);
        if (child < 0) goto done;
        int checked = matching_policy(child, expected), saved = errno;
        close(child);
        if (checked < 0) { errno = saved; goto done; }
    }
    if (fchown(home, uid, uid) < 0 || fchmod(home, 0700) < 0
            || check_home(home, uid, expected) < 0 || check_anchor(pending, owner, expected) < 0
            || fsync(home) < 0 || fsync(pending) < 0
            || key_present(misc, expected) < 0
            || still_named(misc, pending_name, pending) < 0) goto done;
    /* Only a complete validated tree becomes visible under the final name.
     * NOREPLACE also refuses a racing symlink/file, without following it. */
    if (syscall(SYS_renameat2, misc, pending_name, misc, anchor_name, RENAME_NOREPLACE) < 0
            || fsync(misc) < 0) goto done;
    result = 0;
done:;
    int saved = errno;
    if (home >= 0) close(home);
    close(pending);
    errno = saved;
    return result;
}

int aegis_ce_open_home(int data, uint32_t user_id, uint32_t serial, int create) {
    if (user_id < 10 || user_id >= 21473 || serial > INT32_MAX || (create != 0 && create != 1))
        return reject(EINVAL);
    uint32_t uid = home_uid(user_id);
    if (uid == UINT32_MAX) return reject(EINVAL);
    char system_path[48], misc_path[48], owner[64];
    snprintf(system_path, sizeof(system_path), "system_ce/%u", user_id);
    snprintf(misc_path, sizeof(misc_path), "misc_ce/%u", user_id);
    snprintf(owner, sizeof(owner), "1:%u:%u", user_id, serial);
    int system = -1, misc = -1, anchor = -1, home = -1, result = -1;
    struct fscrypt_policy_v2 expected;
    system = aegis_ce_open_directory(data, system_path);
    if (system < 0 || metadata(system, AID_SYSTEM, AID_SYSTEM, 0770) < 0
            || aegis_ce_require_serial(system, serial) < 0 || policy(system, &expected) < 0
            || key_present(system, &expected) < 0) goto done;
    misc = aegis_ce_open_directory(data, misc_path);
    if (misc < 0 || metadata(misc, AID_SYSTEM, AID_MISC, 01771) < 0
            || matching_policy(misc, &expected) < 0 || key_present(misc, &expected) < 0
            || absent(misc, pending_name) < 0) goto done;
    /* system_ce is the serial-number authority. AOSP does NOT stamp misc_ce
     * with user.serial, but vold gives both the SAME internal-volume CE policy.
     * Never set AOSP's serial or fscrypt policy, nor create missing AOSP roots. */
    anchor = aegis_ce_open_directory(misc, anchor_name);
    if (anchor < 0 && errno == ENOENT && create) {
        if (provision(misc, user_id, uid, owner, &expected) < 0) goto done;
        anchor = aegis_ce_open_directory(misc, anchor_name);
    }
    if (anchor < 0 || check_anchor(anchor, owner, &expected) < 0) goto done;
    home = aegis_ce_open_directory(anchor, "home");
    if (home < 0 || check_home(home, uid, &expected) < 0
            || still_named(data, system_path, system) < 0 || still_named(data, misc_path, misc) < 0
            || still_named(misc, anchor_name, anchor) < 0 || still_named(anchor, "home", home) < 0
            || aegis_ce_require_serial(system, serial) < 0
            || matching_policy(system, &expected) < 0 || key_present(home, &expected) < 0) goto done;
    result = home;
    home = -1;
done:;
    int saved = errno;
    if (system >= 0) close(system);
    if (misc >= 0) close(misc);
    if (anchor >= 0) close(anchor);
    if (home >= 0) close(home);
    errno = saved;
    return result;
}

int aegis_ce_clone_home(int home, uint32_t user_id) {
    if (user_id < 10 || user_id >= 21473) return reject(EINVAL);
    uint32_t uid = home_uid(user_id);
    if (uid == UINT32_MAX) return reject(EINVAL);
    struct stat before, after;
    struct statvfs source, source_after, flags;
    struct fscrypt_policy_v2 expected;
    if (metadata(home, uid, uid, 0700) < 0 || no_acl(home) < 0
            || fstat(home, &before) < 0 || fstatvfs(home, &source) < 0
            || policy(home, &expected) < 0 || key_present(home, &expected) < 0) return -1;
    if (!S_ISDIR(before.st_mode) || (source.f_flag & ST_RDONLY)) return reject(EPERM);
    int tree = (int)syscall(SYS_open_tree, home, "",
                            OPEN_TREE_CLONE | OPEN_TREE_CLOEXEC | AT_EMPTY_PATH);
    if (tree < 0) return -1;
    int view = -1;
    /* Already host-owned by this AOSP user's mapped ordinary UID. NO IDMAP.
     * Executable home supports user-built Linux programs; set-ID/device use
     * remains prohibited. SELinux, process capabilities and seccomp still
     * require the separate runtime sandbox before any program can execute. */
    struct mount_attr attr = {
        .attr_set = MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV,
        .attr_clr = MOUNT_ATTR_RDONLY | MOUNT_ATTR_NOEXEC,
        .propagation = MS_PRIVATE,
    };
    if (syscall(SYS_mount_setattr, tree, "", AT_EMPTY_PATH, &attr, sizeof(attr)) < 0) goto fail;
    /* open_tree returns O_PATH, which cannot service fscrypt ioctls. Open a
     * readable reference in the same detached mount for the readback checks. */
    view = aegis_ce_open_directory(tree, ".");
    if (view < 0 || fstat(view, &after) < 0 || fstatvfs(view, &flags) < 0
            || fstatvfs(home, &source_after) < 0 || check_home(view, uid, &expected) < 0
            || key_present(view, &expected) < 0) goto fail;
    if (after.st_dev != before.st_dev || after.st_ino != before.st_ino
            || after.st_uid != before.st_uid || after.st_gid != before.st_gid
            || after.st_mode != before.st_mode || source_after.f_flag != source.f_flag
            || (flags.f_flag & (ST_NOSUID | ST_NODEV)) != (ST_NOSUID | ST_NODEV)
            || (flags.f_flag & (ST_RDONLY | ST_NOEXEC))) { errno = EPROTO; goto fail; }
    close(view);
    return tree;
fail:;
    int saved = errno;
    if (view >= 0) close(view);
    close(tree);
    errno = saved;
    return -1;
}
