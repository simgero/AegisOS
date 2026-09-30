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
#include <sys/random.h>
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

/* These broker-only directories are siblings of HOME, never exported into
 * an ordinary runtime. AOSP owns the containing fscrypt policy and key. */
static const char package_name[] = "packages";
static const char package_pending[] = ".packages-preparing";
static const char *const package_children[] = {"store", "staging"};

static int package_layout(int fd, const char *owner,
                           const struct fscrypt_policy_v2 *expected) {
    if (check_anchor(fd, owner, expected) < 0) return -1;
    for (unsigned i = 0; i < sizeof(package_children) / sizeof(package_children[0]); i++) {
        int child = aegis_ce_open_directory(fd, package_children[i]);
        if (child < 0) return -1;
        int ok = check_anchor(child, owner, expected), saved = errno;
        close(child);
        if (ok < 0) return reject(saved);
    }
    return 0;
}

static int provision_packages(int anchor, const char *owner,
                               const struct fscrypt_policy_v2 *expected) {
    if (mkdirat(anchor, package_pending, 0700) < 0) return -1;
    int pending = aegis_ce_open_directory(anchor, package_pending);
    if (pending < 0) return -1;
    int result = -1;
    if (metadata(pending, 0, 0, 0700) < 0 || no_acl(pending) < 0
            || matching_policy(pending, expected) < 0
            || fsetxattr(pending, owner_attribute, owner, strlen(owner), XATTR_CREATE) < 0)
        goto done;
    for (unsigned i = 0; i < sizeof(package_children) / sizeof(package_children[0]); i++) {
        const char *name = package_children[i];
        if (mkdirat(pending, name, 0700) < 0) goto done;
        int child = aegis_ce_open_directory(pending, name);
        if (child < 0) goto done;
        int ok = metadata(child, 0, 0, 0700) == 0 && no_acl(child) == 0
                && matching_policy(child, expected) == 0
                && fsetxattr(child, owner_attribute, owner, strlen(owner), XATTR_CREATE) == 0
                && check_anchor(child, owner, expected) == 0
                && fsync(child) == 0 && still_named(pending, name, child) == 0;
        int saved = errno;
        close(child);
        if (!ok) { errno = saved; goto done; }
    }
    if (package_layout(pending, owner, expected) < 0 || fsync(pending) < 0
            || key_present(pending, expected) < 0
            || still_named(anchor, package_pending, pending) < 0
            || syscall(SYS_renameat2, anchor, package_pending, anchor, package_name,
                       RENAME_NOREPLACE) < 0 || fsync(anchor) < 0) goto done;
    result = 0;
done:;
    int saved = errno;
    close(pending);
    errno = saved;
    /* Never adopt, repair or delete an interrupted preparation. */
    return result;
}

static int open_private(int data, uint32_t user_id, uint32_t serial, int create, int packages) {
    if (user_id < 10 || user_id >= 21473 || serial > INT32_MAX || (create != 0 && create != 1))
        return reject(EINVAL);
    uint32_t uid = home_uid(user_id);
    if (uid == UINT32_MAX) return reject(EINVAL);
    char system_path[48], misc_path[48], owner[64];
    snprintf(system_path, sizeof(system_path), "system_ce/%u", user_id);
    snprintf(misc_path, sizeof(misc_path), "misc_ce/%u", user_id);
    snprintf(owner, sizeof(owner), "1:%u:%u", user_id, serial);
    int system = -1, misc = -1, anchor = -1, home = -1, area = -1, result = -1;
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
    if(anchor<0 && errno==ENOENT && packages==2 && !create) {
        if(still_named(data,system_path,system)<0 || still_named(data,misc_path,misc)<0
           || aegis_ce_require_serial(system,serial)<0 || matching_policy(system,&expected)<0
           || key_present(misc,&expected)<0 || absent(misc,anchor_name)<0)goto done;
        result=-2;goto done;
    }
    if (anchor < 0 || check_anchor(anchor, owner, &expected) < 0) goto done;
    home = aegis_ce_open_directory(anchor, "home");
    if (home < 0 || check_home(home, uid, &expected) < 0
            || still_named(data, system_path, system) < 0 || still_named(data, misc_path, misc) < 0
            || still_named(misc, anchor_name, anchor) < 0 || still_named(anchor, "home", home) < 0
            || aegis_ce_require_serial(system, serial) < 0
            || matching_policy(system, &expected) < 0 || key_present(home, &expected) < 0) goto done;
    if (packages) {
        if (absent(anchor, package_pending) < 0) goto done;
        area = aegis_ce_open_directory(anchor, package_name);
        if (area < 0 && errno == ENOENT && create) {
            if (provision_packages(anchor, owner, &expected) < 0) goto done;
            area = aegis_ce_open_directory(anchor, package_name);
        }
        if(area<0 && errno==ENOENT && packages==2 && !create) {
            if(still_named(data,system_path,system)<0 || still_named(data,misc_path,misc)<0
               || still_named(misc,anchor_name,anchor)<0 || aegis_ce_require_serial(system,serial)<0
               || matching_policy(system,&expected)<0 || key_present(home,&expected)<0
               || absent(anchor,package_name)<0)goto done;
            result=-2;goto done;
        }
        if (area < 0 || package_layout(area, owner, &expected) < 0
                || still_named(anchor, package_name, area) < 0
                || still_named(data, system_path, system) < 0
                || still_named(data, misc_path, misc) < 0
                || still_named(misc, anchor_name, anchor) < 0
                || aegis_ce_require_serial(system, serial) < 0
                || matching_policy(system, &expected) < 0 || key_present(area, &expected) < 0)
            goto done;
        result = area;
        area = -1;
    } else {
        result = home;
        home = -1;
    }
done:;
    int saved = errno;
    if (system >= 0) close(system);
    if (misc >= 0) close(misc);
    if (anchor >= 0) close(anchor);
    if (home >= 0) close(home);
    if (area >= 0) close(area);
    errno = saved;
    return result;
}

int aegis_ce_open_home(int data, uint32_t user_id, uint32_t serial, int create) {
    return open_private(data, user_id, serial, create, 0);
}

int aegis_ce_open_packages(int data, uint32_t user_id, uint32_t serial, int create) {
    return open_private(data, user_id, serial, create, 1);
}

/* Only accept an already anchored package root within the SAME lifecycle
 * admission. This check cannot establish provenance of an arbitrary fd. */
static int package_owner(int packages, uint32_t user_id, uint32_t serial,
                          char owner[64], struct fscrypt_policy_v2 *expected) {
    if (user_id < 10 || user_id >= 21473 || serial > INT32_MAX) return reject(EINVAL);
    snprintf(owner, 64, "1:%u:%u", user_id, serial);
    if (policy(packages, expected) < 0 || package_layout(packages, owner, expected) < 0)
        return -1;
    return key_present(packages, expected);
}

int aegis_ce_package_store(int packages, uint32_t user_id, uint32_t serial) {
    char owner[64];
    struct fscrypt_policy_v2 expected;
    if (package_owner(packages, user_id, serial, owner, &expected) < 0) return -1;
    int store = aegis_ce_open_directory(packages, "store");
    if (store < 0) return -1;
    if (check_anchor(store, owner, &expected) < 0
            || still_named(packages, "store", store) < 0 || key_present(store, &expected) < 0) {
        int saved = errno; close(store); return reject(saved);
    }
    return store;
}

int aegis_ce_find_package_store(int data,uint32_t user,uint32_t serial,int* output) {
    if(!output || *output!=-1)return reject(EINVAL);
    int area=open_private(data,user,serial,0,2);
    if(area==-2)return 0; // Verified CE identity, absent AEGIS package area.
    if(area<0)return -1;
    int store=aegis_ce_package_store(area,user,serial),saved=errno;close(area);errno=saved;
    if(store<0)return -1;
    int scan=aegis_ce_open_directory(store,".");
    if(scan<0) { saved=errno;close(store);return reject(saved); }
    DIR* entries=fdopendir(scan);
    if(!entries) { saved=errno;close(scan);close(store);return reject(saved); }
    int empty=1,error=0;struct dirent* entry;
    errno=0;
    while((entry=readdir(entries)))
        if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) { empty=0;break; }
    if(!entry)error=errno;
    if(closedir(entries)<0 && !error)error=errno;
    if(error) { close(store);return reject(error); }
    // Provisioned CE layout initially has a pristine empty store. No metadata
    // is initialized here. A partially initialized/nonempty store goes to the
    // strict selector, which must reject missing owner/lock/current data.
    if(empty)close(store);else *output=store;
    return 0;
}

int aegis_ce_create_package_stage(int packages, uint32_t user_id, uint32_t serial, uint64_t job,
                                  struct aegis_package_stage *output) {
    if (!output || output->parent != -1 || output->directory != -1 || output->name[0]
            || output->inode || output->removed || !job || job > INT64_MAX) return reject(EINVAL);
    char owner[64];
    struct fscrypt_policy_v2 expected;
    if (package_owner(packages, user_id, serial, owner, &expected) < 0) return -1;
    int staging = aegis_ce_open_directory(packages, "staging");
    if (staging < 0) return -1;
    int stage = -1, result = -1;
    unsigned char random[16];
    char hex[33], name[80];
    if (check_anchor(staging, owner, &expected) < 0
            || still_named(packages, "staging", staging) < 0) goto done;
    /* Jobs are monotone only within one owner. A kernel-generated nonce also
     * prevents adopting a directory left by an earlier broker lifetime. */
    ssize_t n;
    do { n = getrandom(random, sizeof(random), GRND_NONBLOCK); } while (n < 0 && errno == EINTR);
    if (n != (ssize_t)sizeof(random)) { if (n >= 0) errno = EIO; goto done; }
    for (unsigned i = 0; i < sizeof(random); i++) snprintf(hex + i * 2, 3, "%02x", random[i]);
    snprintf(name, sizeof(name), "job-%llu-%s", (unsigned long long)job, hex);
    if (aegis_package_stage_create(staging, name, output) < 0) goto done;
    stage = output->directory;
    if (stage < 0 || metadata(stage, 0, 0, 0700) < 0 || no_acl(stage) < 0
            || matching_policy(stage, &expected) < 0
            || fsetxattr(stage, owner_attribute, owner, strlen(owner), XATTR_CREATE) < 0
            || check_anchor(stage, owner, &expected) < 0 || fsync(stage) < 0
            || fsync(staging) < 0 || still_named(staging, name, stage) < 0
            || still_named(packages, "staging", staging) < 0 || key_present(stage, &expected) < 0)
        goto done;
    result = 0;
done:;
    int saved = errno;
    close(staging);
    errno = saved;
    return result;
}

int aegis_ce_new_package_stage(int packages,uint32_t user,uint32_t serial,uint64_t job) {
    struct aegis_package_stage owned=AEGIS_PACKAGE_STAGE_INIT;
    int result=aegis_ce_create_package_stage(packages,user,serial,job,&owned),saved=errno;
    if(result==0) { result=owned.directory;owned.directory=-1; }
    aegis_package_stage_close(&owned);errno=saved;return result;
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
