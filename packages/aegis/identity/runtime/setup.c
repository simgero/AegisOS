#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "setup.h"
#include "control.h"
#include "sandbox.h"

#include <dirent.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/magic.h>
#include <linux/mount.h>
#include <linux/openat2.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/xattr.h>
#include <unistd.h>

#define CONTROL_FD 3
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
/* ipc/mqueue.c in pinned kernel 50eb8d5d443b: not exported in Bionic's UAPI. */
#define AEGIS_MQUEUE_MAGIC UINT64_C(0x19800202)
static int denied(void) { errno = EPERM; return -1; }

static int decimal(const char *text, uint32_t *value) {
    if (!*text || (text[0] == '0' && text[1])) return -1;
    uint64_t n = 0;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '9') return -1;
        n = n * 10 + (unsigned)(*p - '0');
        if (n > INT32_MAX) return -1;
    }
    *value = (uint32_t)n;
    return 0;
}

static _Noreturn void failed(uint32_t user, uint32_t serial) {
    int error = errno > 0 ? errno : EIO;
    (void)aegis_send_reply(CONTROL_FD, AEGIS_ERROR, ((uint64_t)user << 32) | serial,
                          error, 1, 0, -1);
    /* This is disposable namespace PID 1: kernel teardown removes descendants
     * and all its descriptors. Broker must still wait and release ITS refs. */
    _exit(125);
}
static void expired(int signal_number) { (void)signal_number; _exit(125); }

static int directory(int fd, uint64_t type, uid_t uid, mode_t mode,
                      unsigned long required, unsigned long forbidden) {
    struct stat st;
    struct statfs fs;
    struct statvfs flags;
    if (fstat(fd, &st) < 0 || fstatfs(fd, &fs) < 0 || fstatvfs(fd, &flags) < 0) return -1;
    if (st.st_mode != (S_IFDIR | mode) || st.st_uid != uid || st.st_gid != uid
            || fs.f_type != type || (flags.f_flag & required) != required
            || (flags.f_flag & forbidden)) return denied();
    return 0;
}

static int supervisor(int fd) {
    struct stat st;
    struct statvfs flags;
    Elf64_Ehdr elf;
    if (fstat(fd, &st) < 0 || fstatvfs(fd, &flags) < 0) return -1;
    /* The trusted Android root owner is deliberately unmapped here. Broker
     * authenticates provenance/ownership on the host side; require a readonly
     * executable file and exact SELinux entrypoint, never an interpreter. */
    if (!S_ISREG(st.st_mode) || !(st.st_mode & S_IXOTH)
            || (st.st_mode & (S_ISUID | S_ISGID | S_IWGRP | S_IWOTH))
            || !(flags.f_flag & ST_RDONLY) || (flags.f_flag & ST_NOEXEC)) return denied();
    char label[128];
    const char expected[] = "u:object_r:aegis_runtime_init_exec:s0";
    ssize_t size = fgetxattr(fd, "security.selinux", label, sizeof(label));
    if (size != (ssize_t)sizeof(expected) && size != (ssize_t)sizeof(expected) - 1) return denied();
    if (memcmp(label, expected, (size_t)size)) return denied();
    ssize_t caps = fgetxattr(fd, "security.capability", NULL, 0);
    if (caps >= 0 || (errno != ENODATA && errno != EOPNOTSUPP)) return denied();
    if (pread(fd, &elf, sizeof(elf), 0) != (ssize_t)sizeof(elf)) { errno = ENOEXEC; return -1; }
    if (memcmp(elf.e_ident, ELFMAG, SELFMAG) || elf.e_ident[EI_CLASS] != ELFCLASS64
            || elf.e_ident[EI_DATA] != ELFDATA2LSB || elf.e_machine != EM_AARCH64
            || (elf.e_type != ET_EXEC && elf.e_type != ET_DYN)
            || elf.e_phentsize != sizeof(Elf64_Phdr) || !elf.e_phnum || elf.e_phnum > 128
            || elf.e_phoff > (uint64_t)st.st_size
            || (uint64_t)elf.e_phnum * sizeof(Elf64_Phdr) > (uint64_t)st.st_size - elf.e_phoff) {
        errno = ENOEXEC; return -1;
    }
    for (unsigned i = 0; i < elf.e_phnum; i++) {
        Elf64_Phdr segment;
        if (pread(fd, &segment, sizeof(segment), (off_t)(elf.e_phoff + i * sizeof(segment)))
                != (ssize_t)sizeof(segment) || segment.p_type == PT_INTERP) {
            errno = ENOEXEC; return -1;
        }
    }
    return 0;
}

static int beneath(int parent, const char *name, int flags) {
    struct open_how how = {.flags = (uint64_t)(flags | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW),
        .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
    return (int)syscall(SYS_openat2, parent, name, &how, sizeof(how));
}

static int attach(int tree, int parent, const char *name) {
    int target = beneath(parent, name, O_PATH);
    if (target < 0) return -1;
    int result = (int)syscall(SYS_move_mount, tree, "", target, "",
                             MOVE_MOUNT_F_EMPTY_PATH | MOVE_MOUNT_T_EMPTY_PATH);
    int saved = errno;
    close(target);
    errno = saved;
    return result;
}

struct parameter { const char *name, *value; };
static int fresh(const char *type, const struct parameter *parameters,
                  unsigned count, unsigned flags) {
    int fs = (int)syscall(SYS_fsopen, type, FSOPEN_CLOEXEC);
    if (fs < 0) return -1;
    int result = -1;
    for (unsigned i = 0; i < count; i++) {
        if (syscall(SYS_fsconfig, fs, parameters[i].value ? FSCONFIG_SET_STRING : FSCONFIG_SET_FLAG,
                    parameters[i].name, parameters[i].value, 0) < 0) goto done;
    }
    if (syscall(SYS_fsconfig, fs, FSCONFIG_CMD_CREATE, NULL, NULL, 0) < 0) goto done;
    result = (int)syscall(SYS_fsmount, fs, FSMOUNT_CLOEXEC, flags);
done:;
    int saved = errno;
    close(fs);
    errno = saved;
    return result;
}

static int fresh_at(int parent, const char *name, const char *type,
                     const struct parameter *parameters, unsigned count, unsigned flags) {
    int tree = fresh(type, parameters, count, flags);
    if (tree < 0) return -1;
    int result = attach(tree, parent, name);
    int saved = errno;
    close(tree);
    errno = saved;
    return result;
}

static int tmpfs_at(int parent, const char *name, const char *size,
                     const char *inodes, const char *mode, unsigned extra) {
    const struct parameter parameters[] = {
        {"size", size}, {"nr_inodes", inodes}, {"mode", mode}, {"uid", "0"}, {"gid", "0"},
    };
    return fresh_at(parent, name, "tmpfs", parameters, COUNT(parameters),
                     MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV | extra);
}

static int empty_directory(int parent, const char *name) {
    int fd = beneath(parent, name, O_RDONLY);
    if (fd < 0) return -1;
    DIR *directory = fdopendir(fd);
    if (!directory) { int saved = errno; close(fd); errno = saved; return -1; }
    int result = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) { result = denied(); break; }
    }
    if (errno) result = -1;
    int saved = errno;
    closedir(directory);
    errno = saved;
    return result;
}

static int runtime_directory(int root) {
    int run = beneath(root, "run", O_RDONLY);
    if (run < 0) return -1;
    int result = -1;
    if (mkdirat(run, "user", 0755) < 0 || fchmodat(run, "user", 0755, 0) < 0
            || mkdirat(run, "user/1000", 0700) < 0
            || fchownat(run, "user/1000", 1000, 1000, AT_SYMLINK_NOFOLLOW) < 0
            || fchmodat(run, "user/1000", 0700, 0) < 0) goto done;
    result = 0;
done:;
    int saved = errno;
    close(run);
    errno = saved;
    return result;
}

static int construct(int fds[AEGIS_SETUP_FDS]) {
    const struct parameter staging[] = {
        {"size", "1048576"}, {"nr_inodes", "16"}, {"mode", "0700"},
    };
    int stage = fresh("tmpfs", staging, COUNT(staging),
                      MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV | MOUNT_ATTR_NOEXEC);
    if (stage < 0) return -1;
    int target = open("/mnt", O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    int root = -1, writable = -1, result = -1;
    if (target < 0) goto done;
    if (syscall(SYS_move_mount, stage, "", target, "",
                MOVE_MOUNT_F_EMPTY_PATH | MOVE_MOUNT_T_EMPTY_PATH) < 0) goto done;
    close(target); target = -1;
    writable = openat(stage, ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (writable < 0 || mkdirat(writable, "root", 0700) < 0) goto done;
    close(writable); writable = -1;
    if (attach(fds[0], stage, "root") < 0) goto done;
    root = beneath(stage, "root", O_PATH);
    if (root < 0 || empty_directory(root, "sys") < 0) goto done;
    if (attach(fds[1], root, "home/user") < 0 || attach(fds[2], root, "dev") < 0) goto done;
    const struct parameter proc[] = {{"hidepid", "2"}, {"subset", "pid"}};
    const struct parameter pts[] = {
        {"newinstance", NULL}, {"gid", "1000"}, {"mode", "0600"},
        {"ptmxmode", "0666"}, {"max", "128"},
    };
    const unsigned restricted = MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV | MOUNT_ATTR_NOEXEC;
    if (fresh_at(root, "proc", "proc", proc, COUNT(proc), restricted) < 0
            || fresh_at(root, "dev/pts", "devpts", pts, COUNT(pts),
                        MOUNT_ATTR_NOSUID | MOUNT_ATTR_NOEXEC) < 0
            || fresh_at(root, "dev/mqueue", "mqueue", NULL, 0, restricted) < 0
            || tmpfs_at(root, "dev/shm", "67108864", "8192", "1777", MOUNT_ATTR_NOEXEC) < 0
            || tmpfs_at(root, "tmp", "134217728", "16384", "1777", 0) < 0
            || tmpfs_at(root, "run", "16777216", "4096", "0755", MOUNT_ATTR_NOEXEC) < 0
            || runtime_directory(root) < 0 || sethostname("aegis-runtime", 13) < 0
            || fchdir(root) < 0) goto done;
    result = 0;
done:;
    int saved = errno;
    if (writable >= 0) close(writable);
    if (root >= 0) close(root);
    if (target >= 0) close(target);
    close(stage);
    errno = saved;
    return result;
}

static int mount_inventory(void) {
    const char *expected[] = {"/", "/home/user", "/dev", "/proc", "/dev/pts",
        "/dev/mqueue", "/dev/shm", "/tmp", "/run"};
    unsigned seen = 0;
    FILE *file = fopen("/proc/self/mountinfo", "re");
    if (!file) return -1;
    char line[8192], name[4096];
    int result = -1;
    while (fgets(line, sizeof(line), file)) {
        if (!strchr(line, '\n') || strstr(line, " shared:") || strstr(line, " master:")
                || sscanf(line, "%*u %*u %*s %*s %4095s", name) != 1) { denied(); goto done; }
        unsigned i;
        for (i = 0; i < COUNT(expected); i++) if (!strcmp(name, expected[i])) break;
        if (i == COUNT(expected) || (seen & (1u << i))) { denied(); goto done; }
        seen |= 1u << i;
    }
    if (ferror(file)) goto done;
    if (seen != (1u << COUNT(expected)) - 1) { denied(); goto done; }
    result = 0;
done:;
    int saved = errno;
    fclose(file);
    errno = saved;
    return result;
}

static int readback(uint64_t home_type) {
    const unsigned long restricted = ST_NOSUID | ST_NODEV | ST_NOEXEC;
    const struct {
        const char *name; uint64_t type; unsigned long required, forbidden;
    } views[] = {
        {"/", EXT4_SUPER_MAGIC, ST_RDONLY | ST_NOSUID | ST_NODEV, ST_NOEXEC},
        {"/home/user", home_type, ST_NOSUID | ST_NODEV, ST_RDONLY | ST_NOEXEC},
        {"/dev", TMPFS_MAGIC, ST_RDONLY | ST_NOSUID | ST_NOEXEC, ST_NODEV},
        {"/proc", PROC_SUPER_MAGIC, restricted, ST_RDONLY},
        {"/dev/pts", DEVPTS_SUPER_MAGIC, ST_NOSUID | ST_NOEXEC, ST_RDONLY | ST_NODEV},
        {"/dev/mqueue", AEGIS_MQUEUE_MAGIC, restricted, ST_RDONLY},
        {"/dev/shm", TMPFS_MAGIC, restricted, ST_RDONLY},
        {"/tmp", TMPFS_MAGIC, ST_NOSUID | ST_NODEV, ST_RDONLY | ST_NOEXEC},
        {"/run", TMPFS_MAGIC, restricted, ST_RDONLY},
    };
    for (unsigned i = 0; i < COUNT(views); i++) {
        struct statfs fs;
        struct statvfs flags;
        if (statfs(views[i].name, &fs) < 0 || statvfs(views[i].name, &flags) < 0) return -1;
        if (fs.f_type != views[i].type || (flags.f_flag & views[i].required) != views[i].required
                || (flags.f_flag & views[i].forbidden)) return denied();
    }
    const struct { const char *name; uid_t uid; mode_t mode; } dirs[] = {
        {"/", 0, 0755}, {"/home/user", 1000, 0700}, {"/dev", 0, 0755},
        {"/tmp", 0, 01777}, {"/dev/shm", 0, 01777}, {"/run", 0, 0755},
        {"/run/user", 0, 0755}, {"/run/user/1000", 1000, 0700},
    };
    for (unsigned i = 0; i < COUNT(dirs); i++) {
        struct stat st;
        if (lstat(dirs[i].name, &st) < 0) return -1;
        if (st.st_mode != (S_IFDIR | dirs[i].mode) || st.st_uid != dirs[i].uid
                || st.st_gid != dirs[i].uid) return denied();
    }
    struct stat st;
    if (lstat("/dev/pts/ptmx", &st) < 0) return -1;
    if (st.st_mode != (S_IFCHR | 0666) || st.st_rdev != makedev(5, 2)) return denied();
    char self[32];
    if (readlink("/proc/self", self, sizeof(self)) != 1 || self[0] != '1') return denied();
    if (lstat("/proc/sys", &st) == 0 || errno != ENOENT) return denied();
    struct stat root, parent;
    if (stat("/", &root) < 0 || stat("/..", &parent) < 0) return -1;
    if (root.st_dev != parent.st_dev || root.st_ino != parent.st_ino) return denied();
    return mount_inventory();
}

int main(int argc, char **argv) {
    uint32_t user, serial;
    /* Reject host/direct calls before touching fds, mounts, signals or cwd. */
    if (argc != 3 || decimal(argv[1], &user) < 0 || decimal(argv[2], &serial) < 0
            || aegis_check_setup_context(user) < 0) return 78;
    if (syscall(SYS_close_range, 4u, UINT_MAX, 0) < 0 || clearenv() < 0) failed(user, serial);
    close(0); close(1); close(2);
    umask(077);
    struct sigaction action = {.sa_handler = expired};
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL) < 0) failed(user, serial);
    alarm(10);
    int fds[AEGIS_SETUP_FDS] = {-1, -1, -1, -1};
    if (aegis_receive_setup(CONTROL_FD, user, serial, 5000, fds) < 0) failed(user, serial);
    for (unsigned i = 0; i < 3; i++) {
        int flags = fcntl(fds[i], F_GETFL);
        if (flags < 0 || !(flags & O_PATH)) { errno = EBADF; failed(user, serial); }
    }
    /* AOSP's current /data is F2FS. Shared-base format stays ext4; the
     * broker-verified CE view may be F2FS or ext4, and must retain that exact
     * type across attach/pivot. Identity, fscrypt policy/key checks belong to
     * ce.c before the trusted descriptor is sent; a type alone grants none. */
    struct statfs home_fs;
    if (fstatfs(fds[1], &home_fs) < 0) failed(user, serial);
    if (home_fs.f_type != EXT4_SUPER_MAGIC && home_fs.f_type != F2FS_SUPER_MAGIC) {
        errno = EPERM;
        failed(user, serial);
    }
    if (directory(fds[0], EXT4_SUPER_MAGIC, 0, 0755, ST_RDONLY | ST_NOSUID | ST_NODEV, ST_NOEXEC) < 0
            || directory(fds[1], home_fs.f_type, 1000, 0700, ST_NOSUID | ST_NODEV, ST_RDONLY | ST_NOEXEC) < 0
            || directory(fds[2], TMPFS_MAGIC, 0, 0755, ST_RDONLY | ST_NOSUID | ST_NOEXEC, ST_NODEV) < 0
            || supervisor(fds[3]) < 0 || construct(fds) < 0) failed(user, serial);
    for (unsigned i = 0; i < 3; i++) close(fds[i]);
    if (fds[3] != 4 && dup3(fds[3], 4, O_CLOEXEC) < 0) failed(user, serial);
    if (syscall(SYS_close_range, 5u, UINT_MAX, 0) < 0
            || fcntl(4, F_SETFD, FD_CLOEXEC) < 0
            || syscall(SYS_pivot_root, ".", ".") < 0
            || umount2(".", MNT_DETACH) < 0 || chdir("/") < 0
            || readback(home_fs.f_type) < 0) failed(user, serial);
    /* No directory, mount, cwd or root reference to the inherited Android tree
     * remains. FD4 is only the authenticated readonly static code file and
     * closes on exec. The init entrypoint rechecks its own SELinux domain and
     * context, drops capabilities, installs seccomp, then sends READY. */
    if (fcntl(CONTROL_FD, F_SETFD, 0) < 0) failed(user, serial);
    alarm(0); /* Alarm timers survive exec; do not leak the setup deadline. */
    char *arguments[] = {"aegis-runtime-init", argv[1], argv[2], NULL};
    char *environment[] = {"PATH=/usr/bin:/bin", "LANG=C.UTF-8", NULL};
    syscall(SYS_execveat, 4, "", arguments, environment, AT_EMPTY_PATH);
    failed(user, serial);
}
