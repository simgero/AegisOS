#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "sandbox.h"
#include "uid_layout.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/filter.h>
#include <linux/magic.h>
#include <linux/nsfs.h>
#include <linux/sched.h>
#include <linux/seccomp.h>
#include <linux/securebits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <unistd.h>

#if !defined(__aarch64__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "The runtime supervisor supports only little-endian AArch64"
#endif

static int invalid(void) { errno = EPERM; return -1; }

static int read_text(const char *path, char *text, size_t capacity) {
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    size_t used = 0;
    while (used < capacity - 1) {
        ssize_t n = read(fd, text + used, capacity - 1 - used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { int saved = errno; close(fd); errno = saved; return -1; }
        if (n == 0) { close(fd); text[used] = '\0'; return 0; }
        used += (size_t)n;
    }
    close(fd);
    errno = EOVERFLOW;
    return -1;
}

static int check_map(const char *path, uint32_t user_id) {
    char text[1024];
    if (read_text(path, text, sizeof(text)) < 0) return -1;
    const char *cursor = text;
    for (size_t i = 0; i < sizeof(aegis_uid_extents) / sizeof(aegis_uid_extents[0]); i++) {
        unsigned int inside, outside, count;
        int consumed = 0;
        if (sscanf(cursor, " %u %u %u %n", &inside, &outside, &count, &consumed) != 3
                || consumed <= 0) return invalid();
        const struct aegis_uid_extent *row = &aegis_uid_extents[i];
        if (inside != row->inside || outside != user_id * AEGIS_PER_USER_RANGE + row->app_id
                || count != row->count) return invalid();
        cursor += consumed;
    }
    return *cursor == '\0' ? 0 : invalid();
}

static int check_fs(const char *path, uint64_t type, unsigned long flags) {
    struct statfs fs;
    struct statvfs vfs;
    if (statfs(path, &fs) < 0 || statvfs(path, &vfs) < 0) return -1;
    if ((type && fs.f_type != type) || (vfs.f_flag & flags) != flags) return invalid();
    return 0;
}

static int private_directory(const char *path) {
    struct stat st;
    if (lstat(path, &st) < 0) return -1;
    if (!S_ISDIR(st.st_mode) || st.st_uid != 1000 || st.st_gid != 1000
            || (st.st_mode & 07777) != 0700) return invalid();
    return 0;
}

static int setup_namespaces(uint32_t user_id, int package) {
    uid_t r, e, s;
    gid_t gr, ge, gs;
    if (user_id < 10 || user_id >= 21473 || getpid() != 1 || getppid() != 0
            || getsid(0) != 1 || getpgrp() != 1 || syscall(SYS_gettid) != 1)
        return invalid();
    if (getresuid(&r, &e, &s) < 0 || getresgid(&gr, &ge, &gs) < 0) return -1;
    if (r || e || s || gr || ge || gs || getgroups(0, NULL) != 0) return invalid();
    if (check_fs("/proc", PROC_SUPER_MAGIC, 0) < 0
            || check_map("/proc/self/uid_map", user_id) < 0
            || check_map("/proc/self/gid_map", user_id) < 0) return -1;
    char text[128];
    if (read_text("/proc/self/setgroups", text, sizeof(text)) < 0) return -1;
    if (strcmp(text, package ? "allow\n" : "deny\n")) return invalid();
    if (read_text("/sys/fs/selinux/enforce", text, sizeof(text)) < 0) return -1;
    if (strcmp(text, "1") && strcmp(text, "1\n")) return invalid();
    struct stat user;
    if (stat("/proc/self/ns/user", &user) < 0) return -1;
    const struct { const char *name; int type; } namespaces[] = {
        {"pid", CLONE_NEWPID}, {"mnt", CLONE_NEWNS}, {"ipc", CLONE_NEWIPC},
        {"uts", CLONE_NEWUTS}, {"net", CLONE_NEWNET},
    };
    for (unsigned i = 0; i < sizeof(namespaces) / sizeof(namespaces[0]); i++) {
        char self[64];
        snprintf(self, sizeof(self), "/proc/self/ns/%s", namespaces[i].name);
        /* Follow our own kernel magic links; do not try to inspect Android
         * PID1, which the mapped credentials intentionally cannot ptrace. */
        int ns = open(self, O_RDONLY | O_CLOEXEC);
        if (ns < 0) return -1;
        int type = ioctl(ns, NS_GET_NSTYPE);
        int owner = ioctl(ns, NS_GET_USERNS);
        int saved = errno;
        close(ns);
        if (owner < 0) { errno = saved; return -1; }
        struct stat st;
        int result = fstat(owner, &st);
        saved = errno;
        close(owner);
        if (result < 0) { errno = saved; return -1; }
        if (type != namespaces[i].type || user.st_dev != st.st_dev || user.st_ino != st.st_ino)
            return invalid();
    }
    return 0;
}

static int setup_domain(const char *expected) {
    char text[128];
    if (read_text("/proc/self/attr/current", text, sizeof(text)) < 0) return -1;
    size_t n = strlen(text);
    if (n && text[n - 1] == '\n') text[n - 1] = 0;
    return strcmp(text, expected) ? invalid() : 0;
}
int aegis_check_setup_context(uint32_t user_id) {
    if (setup_namespaces(user_id, 0) < 0) return -1;
    return setup_domain("u:r:aegis_runtime_setup:s0");
}
int aegis_check_package_namespaces(uint32_t user_id) {
    return setup_namespaces(user_id, 1);
}
int aegis_check_package_context(uint32_t user_id) {
    if (aegis_check_package_namespaces(user_id) < 0) return -1;
    return setup_domain("u:r:aegis_package_worker:s0");
}

int aegis_check_context(uint32_t user_id) {
    uid_t real_uid, effective_uid, saved_uid;
    gid_t real_gid, effective_gid, saved_gid;
    if (user_id < 10 || user_id >= 21473 || getpid() != 1 || getppid() != 0)
        return invalid();
    if (getresuid(&real_uid, &effective_uid, &saved_uid) < 0
            || getresgid(&real_gid, &effective_gid, &saved_gid) < 0) return -1;
    if (real_uid || effective_uid || saved_uid || real_gid || effective_gid || saved_gid
            || getgroups(0, NULL) != 0) return invalid();
    char text[128];
    ssize_t n = readlink("/proc/self", text, sizeof(text));
    if (n != 1 || text[0] != '1') return invalid();
    if (check_fs("/proc", PROC_SUPER_MAGIC, ST_NOSUID | ST_NODEV | ST_NOEXEC) < 0
            || check_map("/proc/self/uid_map", user_id) < 0
            || check_map("/proc/self/gid_map", user_id) < 0
            || read_text("/proc/self/setgroups", text, sizeof(text)) < 0) return -1;
    if (strcmp(text, "deny\n") != 0) return invalid();
    if (read_text("/proc/self/attr/current", text, sizeof(text)) < 0) return -1;
    if (strcmp(text, "u:r:aegis_runtime_init:s0") != 0
            && strcmp(text, "u:r:aegis_runtime_init:s0\n") != 0) return invalid();
    if (check_fs("/", 0, ST_RDONLY | ST_NOSUID | ST_NODEV) < 0
            || check_fs("/tmp", TMPFS_MAGIC, ST_NOSUID | ST_NODEV) < 0
            || check_fs("/run", TMPFS_MAGIC, ST_NOSUID | ST_NODEV) < 0
            || check_fs("/dev/pts", DEVPTS_SUPER_MAGIC, ST_NOSUID | ST_NOEXEC) < 0
            || private_directory("/home/user") < 0
            || private_directory("/run/user/1000") < 0) return -1;
    return 0;
}

static int capabilities(uint32_t retained) {
    struct __user_cap_header_struct header = { .version = _LINUX_CAPABILITY_VERSION_3 };
    struct __user_cap_data_struct data[2] = {{0}, {0}};
    data[0].effective = retained;
    data[0].permitted = retained;
    return (int)syscall(SYS_capset, &header, data);
}

int aegis_limit_supervisor(void) {
    if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) < 0
            || prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0
            || prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0) < 0
            || prctl(PR_SET_SECUREBITS, SECBIT_NOROOT | SECBIT_NOROOT_LOCKED
                     | SECBIT_NO_CAP_AMBIENT_RAISE | SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED,
                     0, 0, 0) < 0) return -1;
    /* Drop even capabilities newer than the build headers; EINVAL means the
     * running kernel does not implement that bit. SETPCAP stays effective until
     * the final capset, but nothing remains in the bounding/inheritable sets. */
    for (int cap = 0; cap < 64; cap++) {
        if (prctl(PR_CAPBSET_DROP, cap, 0, 0, 0) < 0 && errno != EINVAL) return -1;
    }
    struct rlimit files = {256, 256}, processes = {256, 256}, core = {0, 0};
    if (setrlimit(RLIMIT_NOFILE, &files) < 0 || setrlimit(RLIMIT_NPROC, &processes) < 0
            || setrlimit(RLIMIT_CORE, &core) < 0) return -1;
    return capabilities((1u << CAP_SETUID) | (1u << CAP_SETGID) | (1u << CAP_KILL));
}

int aegis_limit_shell(void) {
    if (setresgid(1000, 1000, 1000) < 0 || setresuid(1000, 1000, 1000) < 0
            || capabilities(0) < 0 || prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) < 0) return -1;
    return 0;
}

static int limit_package(uint32_t user_id,int supervise) {
    uid_t r, e, saved; gid_t gr, ge, gs;
    if (user_id < 10 || user_id >= 21473 || getpid() != 1 || getppid() != 0
            || getsid(0) != 1 || getpgrp() != 1 || syscall(SYS_gettid) != 1)
        return invalid();
    if (getresuid(&r, &e, &saved) < 0 || getresgid(&gr, &ge, &gs) < 0) return -1;
    if (r || e || saved || gr || ge || gs || getgroups(0, NULL)) return invalid();
    if (check_map("/proc/self/uid_map", user_id) < 0
            || check_map("/proc/self/gid_map", user_id) < 0) return -1;
    char text[128];
    if (read_text("/proc/self/setgroups", text, sizeof(text)) < 0) return -1;
    if (strcmp(text, "allow\n")) return invalid();
    ssize_t length = readlink("/proc/self", text, sizeof(text));
    if (length != 1 || text[0] != '1') return invalid();
    // All permitted authority belongs to this mapped user namespace. No
    // SYS_ADMIN, SETPCAP, SYS_CHROOT, MKNOD, SETFCAP or network-admin
    // capability remains. Trusted mounts/SELinux and lifecycle are separate.
    const uint32_t allowed = (1u << CAP_CHOWN) | (1u << CAP_DAC_OVERRIDE)
        | (1u << CAP_FOWNER) | (1u << CAP_FSETID) | (1u << CAP_SETUID) | (1u << CAP_SETGID);
    if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) < 0
            || prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0
            || prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0) < 0
            || prctl(PR_SET_SECUREBITS, SECBIT_NOROOT | SECBIT_NOROOT_LOCKED,
                     0, 0, 0) < 0) return -1;
    for (int cap = 0; cap < 64; cap++) {
        if (cap < 32 && (allowed & (1u << cap))) continue;
        if (prctl(PR_CAPBSET_DROP, cap, 0, 0, 0) < 0 && errno != EINVAL) return -1;
    }
    struct rlimit files = {256, 256}, processes = {256, 256}, core = {0, 0};
    if (setrlimit(RLIMIT_NOFILE, &files) < 0 || setrlimit(RLIMIT_NPROC, &processes) < 0
            || setrlimit(RLIMIT_CORE, &core) < 0) return -1;
    // Ambient inheritance is restricted to this exact set; locked NOROOT
    // prevents exec as UID0 from resurrecting anything else. Ordinary UID
    // changes still drop permitted/effective/ambient authority normally.
    struct __user_cap_header_struct header = {.version = _LINUX_CAPABILITY_VERSION_3};
    struct __user_cap_data_struct data[2] = {{0}, {0}};
    data[0].effective = data[0].permitted = allowed | (1u << CAP_SETPCAP)
        | (supervise ? (1u << CAP_KILL) : 0);
    data[0].inheritable = allowed;
    if (syscall(SYS_capset, &header, data) < 0) return -1;
    for (int cap = 0; cap < 32; cap++) if (allowed & (1u << cap))
        if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) < 0) return -1;
    if (prctl(PR_SET_SECUREBITS, SECBIT_NOROOT | SECBIT_NOROOT_LOCKED
            | SECBIT_NO_CAP_AMBIENT_RAISE | SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED,
            0, 0, 0) < 0) return -1;
    // Only trusted PID1 retains KILL in effective/permitted. It is deliberately
    // absent from bounding, inheritable and ambient sets, so the following exec
    // into APT/dpkg removes it. Package programs keep exactly the same six caps.
    data[0].effective = data[0].permitted = allowed | (supervise ? (1u << CAP_KILL) : 0);
    if (syscall(SYS_capset, &header, data) < 0) return -1;
    return aegis_install_filter();
}

int aegis_limit_package_worker(uint32_t user_id) { return limit_package(user_id,0); }
int aegis_limit_package_supervisor(uint32_t user_id) { return limit_package(user_id,1); }

#define DENY(n) BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_##n, 0, 1), \
                BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM)

int aegis_install_filter(void) {
    /* This is one layer, not a syscall allowlist or proof of complete isolation.
     * Broker-owned private mounts, SELinux and resource limits remain required.
     * clone3 cannot be inspected through seccomp's argument pointer: ENOSYS
     * allows libc to use clone, whose namespace flags are checked below. */
    const unsigned int namespace_flags = CLONE_NEWNS | CLONE_NEWCGROUP | CLONE_NEWUTS
        | CLONE_NEWIPC | CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET;
    struct sock_filter code[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_AARCH64, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_clone3, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | ENOSYS),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_clone, 0, 7),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, args[0]) + 4),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, args[0])),
        BPF_JUMP(BPF_JMP | BPF_JSET | BPF_K, namespace_flags, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
        DENY(unshare), DENY(setns), DENY(mount), DENY(umount2), DENY(pivot_root),
        DENY(open_tree), DENY(move_mount), DENY(mount_setattr), DENY(fsopen),
        DENY(fsconfig), DENY(fsmount), DENY(fspick), DENY(chroot),
        DENY(keyctl), DENY(add_key), DENY(request_key),
        DENY(ptrace), DENY(process_vm_readv), DENY(process_vm_writev), DENY(kcmp),
        DENY(pidfd_getfd), DENY(bpf), DENY(perf_event_open), DENY(userfaultfd),
        DENY(io_uring_setup), DENY(io_uring_enter), DENY(io_uring_register),
        DENY(init_module), DENY(finit_module), DENY(delete_module),
        DENY(kexec_load), DENY(kexec_file_load), DENY(reboot), DENY(swapon), DENY(swapoff),
        DENY(open_by_handle_at), DENY(name_to_handle_at), DENY(acct), DENY(quotactl),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };
    struct sock_fprog program = { .len = sizeof(code) / sizeof(code[0]), .filter = code };
    return (int)syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &program);
}
