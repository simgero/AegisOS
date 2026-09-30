#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "namespace.h"
#include "ce_private.h"
#include "child_private.h"
#include "uid_layout.h"
#include "mounts_private.h"
#include "memory_group.h"

#include <dirent.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/magic.h>
#include <linux/mount.h>
#include <linux/nsfs.h>
#include <linux/sched.h>
#include <log/log.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>

#if !defined(__aarch64__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "The namespace launcher supports only little-endian AArch64"
#endif

struct aegis_namespace {
    struct aegis_child *child;
    int proc_root, gate, attempted, mapped, userns, counted, package;
    uint32_t user_id, serial;
};

enum { REF_UID, REF_GID, REF_GROUPS, REF_OOM, REF_USERNS, REF_COUNT };

/* One trusted, process-owned mount namespace, established while still in
 * Android init's namespaces. Keep its nsfs fd for this broker's lifetime;
 * an inherited fd in a fork is NOT authority to become another broker.
 * -2 seals a partially failed initialization: callers must terminate. */
static int broker_mounts = -1;
static pid_t broker_mounts_owner;
static unsigned live_contexts;
static int host_namespaces[3] = {-1, -1, -1};
static pid_t host_namespaces_owner;

static int denied(void) { errno = EPERM; return -1; }
static int report_namespace_failure(const char *phase) {
    int saved = errno;
    __android_log_print(ANDROID_LOG_ERROR, "AegisRuntimeNamespace",
                        "AEGIS_RUNTIME_NAMESPACE_FAILED: %s errno=%d", phase, saved);
    errno = saved;
    return -1;
}
static int owner(struct aegis_namespace *context) {
    if (!context) { errno = EINVAL; return -1; }
    return context->child->owner == (pid_t)syscall(SYS_getpid) ? 0 : denied();
}
static void close_gate(struct aegis_namespace *context) {
    if (context->gate >= 0) close(context->gate);
    context->gate = -1;
}

static int text_at(int directory, const char *name, char *text, size_t capacity) {
    int fd = openat(directory, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t length = read(fd, text, capacity - 1);
    int saved = errno;
    close(fd);
    if (length < 0) { errno = saved; return -1; }
    if ((size_t)length == capacity - 1) { errno = EOVERFLOW; return -1; }
    text[length] = '\0';
    return 0;
}

static int same_namespace(int self, int pinned, const char *name) {
    struct stat first, second;
    /* Namespace entries are kernel magic links, intentionally followed. */
    if (fstatat(self, name, &first, 0) < 0 || fstat(pinned, &second) < 0) return -1;
    return first.st_dev == second.st_dev && first.st_ino == second.st_ino ? 0 : denied();
}

static int initial_map(int self, const char *name) {
    char text[128], extra;
    unsigned long long inside, outside, count;
    if (text_at(self, name, text, sizeof(text)) < 0) return -1;
    return sscanf(text, " %llu %llu %llu %c", &inside, &outside, &count, &extra) == 3
            && inside == 0 && outside == 0 && count == UINT32_MAX ? 0 : denied();
}

static int check_process(int proc, pid_t pid, const int pinned[3], pid_t pinned_owner) {
    if (pinned_owner != pid || pinned[0] < 0 || pinned[1] < 0 || pinned[2] < 0) {
        denied(); return report_namespace_failure("host namespace ownership");
    }
    uid_t r, e, s;
    gid_t gr, ge, gs;
    struct statfs fs;
    struct sigaction action;
    if (getresuid(&r, &e, &s) < 0 || getresgid(&gr, &ge, &gs) < 0
            || sigaction(SIGCHLD, NULL, &action) < 0 || fstatfs(proc, &fs) < 0)
        return report_namespace_failure("read process prerequisites");
    if (r || e || s || gr || ge || gs || getgroups(0, NULL) != 0
            || fs.f_type != PROC_SUPER_MAGIC || action.sa_handler == SIG_IGN
            || (action.sa_flags & SA_NOCLDWAIT) || syscall(SYS_gettid) != pid) {
        denied(); return report_namespace_failure("process prerequisites");
    }
    char number[32];
    snprintf(number, sizeof(number), "%ld", (long)pid);
    int self = openat(proc, number, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (self < 0) return report_namespace_failure("open own proc directory");
    const char *phase = "compare pinned user namespace";
    int result = -1;
    if (same_namespace(self, pinned[0], "ns/user") < 0) goto done;
    phase = "compare pinned PID namespace";
    if (same_namespace(self, pinned[1], "ns/pid") < 0) goto done;
    phase = "initial UID map";
    if (initial_map(self, "uid_map") < 0) goto done;
    phase = "initial GID map";
    if (initial_map(self, "gid_map") < 0) goto done;
    phase = "compare mount namespace";
    if (broker_mounts == -1) {
        if (same_namespace(self, pinned[2], "ns/mnt") < 0) goto done;
    } else {
        struct stat expected, current;
        if (broker_mounts < 0 || broker_mounts_owner != pid) { denied(); goto done; }
        if (fstat(broker_mounts, &expected) < 0 || fstatat(self, "ns/mnt", &current, 0) < 0)
            goto done;
        if (expected.st_dev != current.st_dev || expected.st_ino != current.st_ino) {
            denied(); goto done;
        }
    }
    phase = "single thread";
    int task = openat(self, "task", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (task < 0) goto done;
    DIR *threads = fdopendir(task);
    if (!threads) { close(task); goto done; }
    unsigned count = 0;
    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(threads))) if (entry->d_name[0] != '.') count++;
    int read_error = errno;
    closedir(threads);
    if (read_error) errno = read_error;
    else if (count != 1) denied();
    else result = 0;
done:;
    int saved = errno;
    close(self);
    errno = saved;
    if (result < 0) return report_namespace_failure(phase);
    return result;
}

static int check_broker(int proc, pid_t pid) {
    return check_process(proc, pid, host_namespaces, host_namespaces_owner);
}

int aegis_namespace_pin_host(int user_ns, int pid_ns, int mount_ns) {
    pid_t pid = (pid_t)syscall(SYS_getpid);
    if (host_namespaces_owner && host_namespaces_owner != pid) return denied();
    const int inputs[] = {user_ns, pid_ns, mount_ns};
    const int kinds[] = {CLONE_NEWUSER, CLONE_NEWPID, CLONE_NEWNS};
    int copies[] = {-1, -1, -1};
    int proc = -1, result = -1;
    for (unsigned i = 0; i < 3; i++) {
        if (inputs[i] < 3) { errno = EBADF; goto done; }
        copies[i] = fcntl(inputs[i], F_DUPFD_CLOEXEC, 3);
        if (copies[i] < 0) goto done;
        struct statfs fs;
        struct stat candidate, previous;
        int flags = fcntl(copies[i], F_GETFL);
        if (flags < 0 || fstatfs(copies[i], &fs) < 0 || fstat(copies[i], &candidate) < 0)
            goto done;
        if ((flags & (O_ACCMODE | O_PATH)) != O_RDONLY || fs.f_type != NSFS_MAGIC
                || ioctl(copies[i], NS_GET_NSTYPE) != kinds[i]) { denied(); goto done; }
        if (host_namespaces_owner) {
            if (fstat(host_namespaces[i], &previous) < 0) goto done;
            if (candidate.st_dev != previous.st_dev || candidate.st_ino != previous.st_ino) {
                denied(); goto done;
            }
        }
    }
    proc = open("/proc", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (proc < 0 || check_process(proc, pid, copies, pid) < 0) goto done;
    if (!host_namespaces_owner) {
        for (unsigned i = 0; i < 3; i++) {
            host_namespaces[i] = copies[i]; copies[i] = -1;
        }
        host_namespaces_owner = pid;
    }
    result = 0;
done:;
    int saved = errno;
    if (proc >= 0) close(proc);
    for (unsigned i = 0; i < 3; i++) if (copies[i] >= 0) close(copies[i]);
    errno = saved;
    return result;
}

int aegis_namespace_check_broker(void) {
    int proc = open("/proc", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (proc < 0) return -1;
    int result = check_broker(proc, (pid_t)syscall(SYS_getpid)), saved = errno;
    close(proc); errno = saved;
    return result;
}

int aegis_namespace_private_mounts(void) {
    if (aegis_namespace_check_broker() < 0) return -1;
    if (broker_mounts >= 0) return 0;
    if (live_contexts) { errno = EBUSY; return -1; }
    /* No client paths/fds and no namespace received over IPC. Establish the
     * private copy ourselves, before cloning any personal context. */
    broker_mounts = -2;
    if (syscall(SYS_unshare, CLONE_NEWNS) < 0
            || mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) return -1;
    int fd = open("/proc/self/ns/mnt", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    if (ioctl(fd, NS_GET_NSTYPE) != CLONE_NEWNS) {
        close(fd); return denied();
    }
    broker_mounts_owner = (pid_t)syscall(SYS_getpid);
    broker_mounts = fd;
    return aegis_namespace_check_broker();
}

static int temporary_base = -1;
static int same_base_mount(int first,int second) {
    struct statx a = {0}, b = {0};
    if (syscall(SYS_statx,first,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW,STATX_MNT_ID,&a)<0
            || syscall(SYS_statx,second,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW,STATX_MNT_ID,&b)<0) return -1;
    if (!(a.stx_mask&STATX_MNT_ID) || !(b.stx_mask&STATX_MNT_ID)
            || !a.stx_mnt_id || a.stx_mnt_id!=b.stx_mnt_id) { errno=ESTALE;return -1; }
    return 0;
}

int aegis_namespace_attach_base(int source) {
    if (aegis_namespace_check_broker() < 0) return -1;
    if (broker_mounts < 0) return denied();
    if (temporary_base>=0) { errno=EBUSY;return -1; }
    struct stat st;
    struct statvfs flags;
    if (fstat(source, &st) < 0 || fstatvfs(source, &flags) < 0) return -1;
    if (st.st_mode != (S_IFDIR | 0755) || st.st_uid || st.st_gid
            || !(flags.f_flag & ST_RDONLY)) return denied();
    /* /mnt is only a broker-private anchor. No directory is created on any
     * shared Android filesystem. The setup child overlays /mnt with its own
     * staging tmpfs, then pivots away from this inherited Android tree. */
    int target = open("/mnt", O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (target < 0) return -1;
    int result = (int)syscall(SYS_move_mount, source, "", target, "",
                             MOVE_MOUNT_F_EMPTY_PATH | MOVE_MOUNT_T_EMPTY_PATH);
    int saved = errno;
    close(target); errno = saved;
    return result;
}

/* Attach only long enough for open_tree(CLONE). The setup child already
 * exists, so it never inherits this private CE-backed parent anchor. Failure
 * retains the exact mount reference and blocks replacement until cleanup. */
int aegis_namespace_temporary_base_begin(int source) {
    if (aegis_namespace_check_broker()<0) return -1;
    if (temporary_base>=0) { errno=EBUSY;return -1; }
    int copy=fcntl(source,F_DUPFD_CLOEXEC,3);if(copy<0)return -1;
    if(aegis_namespace_attach_base(copy)<0) { int saved=errno;close(copy);errno=saved;return -1; }
    temporary_base=copy;return 0;
}
int aegis_namespace_temporary_base_end(int source) {
    if (aegis_namespace_check_broker()<0) return -1;
    if (temporary_base<0) { errno=EINVAL;return -1; }
    if (same_base_mount(source,temporary_base)<0) return -1;
    int named=open("/mnt",O_PATH|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(named<0)return -1;
    int checked=same_base_mount(named,temporary_base),saved=errno;close(named);errno=saved;
    if(checked<0 || umount2("/mnt",MNT_DETACH)<0)return -1;
    close(temporary_base);temporary_base=-1;return 0;
}

static int check_setup(int fd) {
    struct stat st;
    Elf64_Ehdr header;
    if (fstat(fd, &st) < 0) return -1;
    if (!S_ISREG(st.st_mode) || st.st_uid != 0 || !(st.st_mode & S_IXOTH)
            || (st.st_mode & (S_ISUID | S_ISGID | S_IWGRP | S_IWOTH))) return denied();
    if (pread(fd, &header, sizeof(header), 0) != (ssize_t)sizeof(header)) { errno = ENOEXEC; return -1; }
    if (memcmp(header.e_ident, ELFMAG, SELFMAG) || header.e_ident[EI_CLASS] != ELFCLASS64
            || header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_machine != EM_AARCH64
            || (header.e_type != ET_EXEC && header.e_type != ET_DYN)
            || header.e_phentsize != sizeof(Elf64_Phdr) || !header.e_phnum
            || header.e_phnum > 128 || header.e_phoff > (uint64_t)st.st_size
            || (uint64_t)header.e_phnum * sizeof(Elf64_Phdr)
                    > (uint64_t)st.st_size - header.e_phoff) { errno = ENOEXEC; return -1; }
    for (unsigned i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr segment;
        off_t offset = (off_t)(header.e_phoff + i * sizeof(segment));
        if (pread(fd, &segment, sizeof(segment), offset) != (ssize_t)sizeof(segment)
                || segment.p_type == PT_INTERP) { errno = ENOEXEC; return -1; }
    }
    ssize_t caps = fgetxattr(fd, "security.capability", NULL, 0);
    if (caps >= 0 || (errno != ENODATA && errno != EOPNOTSUPP)) return denied();
    return 0;
}

static int check_channel(int fd) {
    int type;
    socklen_t size = sizeof(type);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) < 0) return -1;
    if (type != SOCK_SEQPACKET) { errno = EPROTOTYPE; return -1; }
    struct sockaddr_un address;
    size = sizeof(address);
    if (getsockname(fd, (struct sockaddr *)&address, &size) < 0) return -1;
    if (address.sun_family != AF_UNIX || size != sizeof(sa_family_t)) return denied();
    size = sizeof(address);
    if (getpeername(fd, (struct sockaddr *)&address, &size) < 0) return -1;
    return address.sun_family == AF_UNIX && size == sizeof(sa_family_t) ? 0 : denied();
}

static _Noreturn void child_failed(void) {
    syscall(SYS_exit_group, 125);
    __builtin_unreachable();
}

/* Raw clone3 bypasses Bionic atfork/TLS initialization. From here to exec use
 * ONLY raw syscalls and stack values: no malloc, stdio, cached getpid or libc
 * set*id wrappers. The helper exec starts a fresh libc instance. */
static _Noreturn void child_exec(int setup, int control, int gate, int parent,
                                  char *user, char *serial) {
    if (syscall(SYS_dup3, control, 3, 0) < 0
            || syscall(SYS_dup3, setup, 4, O_CLOEXEC) < 0
            || syscall(SYS_dup3, gate, 5, O_CLOEXEC) < 0
            || syscall(SYS_dup3, parent, 6, O_CLOEXEC) < 0
            || syscall(SYS_close_range, 0u, 2u, 0u) < 0
            || syscall(SYS_close_range, 7u, UINT_MAX, 0u) < 0) child_failed();
    /* AArch64 kernel sigaction ABI; unlike libc sigset_t, kernel mask is 8B. */
    struct { uint64_t handler, flags, restorer, mask; } action = {0};
    uint64_t empty = 0;
    for (int signal_number = 1; signal_number <= 64; signal_number++) {
        if (signal_number == SIGKILL || signal_number == SIGSTOP) continue;
        if (syscall(SYS_rt_sigaction, signal_number, &action, NULL, sizeof(empty)) < 0)
            child_failed();
    }
    if (syscall(SYS_rt_sigprocmask, SIG_SETMASK, &empty, NULL, sizeof(empty)) < 0)
        child_failed();
    /* Only our trusted, gated clone holds this end of the private socket.
     * Open its OWN proc references, so the parent never traverses another
     * user's hidden /proc/PID or dereferences another process's ns link.
     * Maps are O_PATH references, not open writable map files: the parent
     * must reopen them under its original credentials for multi-ID mapping.
     * Passing an already-open child map would lose that kernel authority. */
    const char *names[] = {"uid_map", "gid_map", "setgroups", "oom_score_adj"};
    int self = (int)syscall(SYS_openat, AT_FDCWD, "/proc/self",
                            O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
    if (self < 0) child_failed();
    union { struct cmsghdr alignment; char bytes[CMSG_SPACE(REF_COUNT * sizeof(int))]; } ancillary = {0};
    char kind = 'F';
    struct iovec vector = {.iov_base = &kind, .iov_len = 1};
    struct msghdr message = {.msg_iov = &vector, .msg_iovlen = 1,
                            .msg_control = ancillary.bytes, .msg_controllen = sizeof(ancillary.bytes)};
    struct cmsghdr *header = CMSG_FIRSTHDR(&message);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(REF_COUNT * sizeof(int));
    int *refs = (int *)CMSG_DATA(header);
    for (unsigned i = 0; i < REF_USERNS; i++) {
        refs[i] = (int)syscall(SYS_openat, self, names[i], O_PATH | O_NOFOLLOW | O_CLOEXEC, 0);
        if (refs[i] < 0) child_failed();
    }
    refs[REF_USERNS] = (int)syscall(SYS_openat, self, "ns/user", O_RDONLY | O_CLOEXEC, 0);
    if (refs[REF_USERNS] < 0
            || syscall(SYS_sendmsg, 5, &message, MSG_NOSIGNAL | MSG_DONTWAIT) != 1)
        child_failed();
    for (unsigned i = 0; i < REF_COUNT; i++) syscall(SYS_close, refs[i]);
    syscall(SYS_close, self);
    struct pollfd ready = {.fd = 5, .events = POLLIN};
    struct timespec limit = {.tv_sec = 10};
    if (syscall(SYS_ppoll, &ready, 1u, &limit, NULL, sizeof(empty)) != 1
            || !(ready.revents & POLLIN)) child_failed();
    char token[2];
    if (syscall(SYS_recvfrom, 5, token, sizeof(token), MSG_DONTWAIT, NULL, NULL) != 1
            || token[0] != 'G' || syscall(SYS_getpid) != 1 || syscall(SYS_getppid) != 0
            || syscall(SYS_getgroups, 0, NULL) != 0
            || syscall(SYS_setsid) < 0
            || syscall(SYS_setresgid, 0u, 0u, 0u) < 0
            || syscall(SYS_setresuid, 0u, 0u, 0u) < 0) child_failed();
    /* Credential changes can clear PDEATHSIG. Arm it AFTER both ID changes,
     * then check the stable parent pidfd to cover a death before arming. */
    if (syscall(SYS_prctl, PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0) < 0
            || syscall(SYS_prctl, PR_SET_DUMPABLE, 0, 0, 0, 0) < 0
            || syscall(SYS_mount, NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0)
        child_failed();
    struct pollfd alive = {.fd = 6, .events = POLLIN};
    struct timespec zero = {0};
    if (syscall(SYS_ppoll, &alive, 1u, &zero, NULL, sizeof(empty)) != 0) child_failed();
    syscall(SYS_close, 5);
    syscall(SYS_close, 6);
    char label[] = "aegis-runtime-setup", path[] = "PATH=/system/bin", locale[] = "LANG=C";
    char *argv[] = {label, user, serial, NULL}, *envp[] = {path, locale, NULL};
    syscall(SYS_execveat, 4, "", argv, envp, AT_EMPTY_PATH);
    child_failed();
}

static int create(uint32_t user_id, uint32_t serial, int setup_fd, int control_fd,
                   struct aegis_memory_group *group, struct aegis_namespace **output) {
    if (!output || *output || user_id < 10 || user_id >= 21473 || serial > INT32_MAX) {
        errno = EINVAL; return -1;
    }
    struct aegis_namespace *context = calloc(1, sizeof(*context));
    struct aegis_child *child = calloc(1, sizeof(*child));
    int setup = -1, control = -1, parent = -1, pair[2] = {-1, -1}, child_gate = -1;
    if (!context || !child) { free(context); free(child); return -1; }
    context->child = child;
    context->gate = context->proc_root = context->userns = child->pidfd = -1;
    context->user_id = user_id;
    context->serial = serial;
    child->owner = (pid_t)syscall(SYS_getpid);
    context->proc_root = open("/proc", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (context->proc_root < 0 || check_broker(context->proc_root, child->owner) < 0) goto fail;
    /* All source fds >=7: child's dup3 to fixed slots can never alias a source. */
    setup = fcntl(setup_fd, F_DUPFD_CLOEXEC, 7);
    if (setup < 0 || check_setup(setup) < 0) goto fail;
    control = fcntl(control_fd, F_DUPFD_CLOEXEC, 7);
    if (control < 0 || check_channel(control) < 0) goto fail;
    int parent_original = (int)syscall(SYS_pidfd_open, child->owner, 0u);
    if (parent_original < 0) goto fail;
    parent = fcntl(parent_original, F_DUPFD_CLOEXEC, 7);
    int saved = errno;
    close(parent_original);
    errno = saved;
    if (parent < 0 || socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair) < 0) goto fail;
    context->gate = fcntl(pair[0], F_DUPFD_CLOEXEC, 7);
    child_gate = fcntl(pair[1], F_DUPFD_CLOEXEC, 7);
    if (context->gate < 0 || child_gate < 0) goto fail;
    close(pair[0]); close(pair[1]); pair[0] = pair[1] = -1;
    char user[16], generation[16];
    snprintf(user, sizeof(user), "%u", user_id);
    snprintf(generation, sizeof(generation), "%u", serial);
    struct clone_args args = {
        .flags = CLONE_PIDFD | CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNS
            | CLONE_NEWIPC | CLONE_NEWUTS | CLONE_NEWNET,
        .pidfd = (uint64_t)(uintptr_t)&child->pidfd,
        .exit_signal = SIGCHLD,
    };
    if (group) {
        int cgroup = aegis_memory_group_claim(group, user_id, serial);
        if (cgroup < 0) goto fail;
        args.flags |= CLONE_INTO_CGROUP;
        args.cgroup = (uint64_t)cgroup;
    }
    uint64_t all = UINT64_MAX, previous;
    if (syscall(SYS_rt_sigprocmask, SIG_SETMASK, &all, &previous, sizeof(all)) < 0) goto fail;
    pid_t pid = (pid_t)syscall(SYS_clone3, &args, sizeof(args));
    if (pid == 0) child_exec(setup, control, child_gate, parent, user, generation);
    saved = errno;
    /* Valid fixed arguments; restoring the mask cannot lose ownership even if
     * the kernel were to reject it. Output ownership is assigned before return. */
    int restored = (int)syscall(SYS_rt_sigprocmask, SIG_SETMASK, &previous, NULL, sizeof(all));
    errno = saved;
    if (pid < 0) goto fail;
    close(setup); close(control); close(parent); close(child_gate);
    if (restored < 0) { context->attempted = 1; close_gate(context); }
    *output = context;
    context->counted = 1;
    live_contexts++;
    return 0;
fail:;
    int error = errno;
    if (setup >= 0) close(setup);
    if (control >= 0) close(control);
    if (parent >= 0) close(parent);
    if (pair[0] >= 0) close(pair[0]);
    if (pair[1] >= 0) close(pair[1]);
    if (child_gate >= 0) close(child_gate);
    aegis_namespace_release(context);
    errno = error;
    return -1;
}

int aegis_namespace_create(uint32_t user_id, uint32_t serial, int setup_fd, int control_fd,
                           struct aegis_namespace **output) {
    return create(user_id, serial, setup_fd, control_fd, NULL, output);
}

int aegis_namespace_create_limited(uint32_t user_id, uint32_t serial, int setup_fd,
                                   int control_fd, struct aegis_memory_group *group,
                                   struct aegis_namespace **output) {
    if (!group) { errno = EINVAL; return -1; }
    return create(user_id, serial, setup_fd, control_fd, group, output);
}

/* Reopen a kernel proc inode under THIS process's credentials. Only our own
 * fd table is traversed. O_NOFOLLOW here would reject the intended fd magic
 * link; the reference itself has already been checked as a proc regular file.
 * Source references are never retained by or accepted from a runtime client. */
static int reopen_reference(int reference, int flags) {
    char name[64];
    snprintf(name, sizeof(name), "/proc/self/fd/%d", reference);
    int fd = open(name, flags | O_CLOEXEC);
    if (fd < 0) return -1;
    struct stat expected, actual;
    if (fstat(reference, &expected) < 0 || fstat(fd, &actual) < 0
            || expected.st_dev != actual.st_dev || expected.st_ino != actual.st_ino) {
        close(fd); return denied();
    }
    return fd;
}

static int write_reference(int reference, const char *text, size_t length) {
    int fd = reopen_reference(reference, O_WRONLY);
    if (fd < 0) return -1;
    /* uid/gid maps require a single write at offset 0. NEVER append or retry a
     * partial write, because the kernel may already have committed the map. */
    ssize_t written = write(fd, text, length);
    int saved = written < 0 ? errno : EIO;
    close(fd);
    if (written != (ssize_t)length) { errno = saved; return -1; }
    return 0;
}

static int read_reference(int reference, char *text, size_t capacity) {
    int fd = reopen_reference(reference, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t length = read(fd, text, capacity - 1);
    int saved = errno;
    close(fd);
    if (length < 0) { errno = saved; return -1; }
    if ((size_t)length == capacity - 1) { errno = EOVERFLOW; return -1; }
    text[length] = '\0';
    return 0;
}

static int mapped(int reference, uint32_t user_id) {
    char text[1024];
    if (read_reference(reference, text, sizeof(text)) < 0) return -1;
    const char *cursor = text;
    for (size_t i = 0; i < sizeof(aegis_uid_extents) / sizeof(aegis_uid_extents[0]); i++) {
        unsigned inside, outside, count;
        int consumed = 0;
        const struct aegis_uid_extent *row = &aegis_uid_extents[i];
        if (sscanf(cursor, " %u %u %u %n", &inside, &outside, &count, &consumed) != 3
                || consumed <= 0 || inside != row->inside || count != row->count
                || outside != user_id * AEGIS_PER_USER_RANGE + row->app_id) return denied();
        cursor += consumed;
    }
    return *cursor == '\0' ? 0 : denied();
}

static int still_waiting(struct aegis_namespace *context) {
    if (owner(context) < 0) return -1;
    if (context->attempted || context->gate < 0) { errno = EALREADY; return -1; }
    if (check_broker(context->proc_root, context->child->owner) < 0) return -1;
    /* Public wait() might already have reaped this child. Never prepare or
     * release its execution gate after somebody consumed the exit status. */
    if (context->child->observed || context->child->observation_error) {
        errno = ECHILD;
        return -1;
    }
    siginfo_t info = {0};
    if (waitid(P_PIDFD, (id_t)context->child->pidfd, &info,
               WEXITED | WNOHANG | WNOWAIT) < 0) return -1;
    if (info.si_pid) { errno = ESRCH; return -1; }
    return 0;
}

static int receive_references(struct aegis_namespace *context, int refs[REF_COUNT], int timeout) {
    struct pollfd ready = {.fd = context->gate, .events = POLLIN};
    int polled = poll(&ready, 1, timeout);
    if (polled < 0) return -1;
    if (!polled) { errno = ETIMEDOUT; return -1; }
    if (!(ready.revents & POLLIN)) { errno = EPIPE; return -1; }
    union { struct cmsghdr alignment; char bytes[CMSG_SPACE(REF_COUNT * sizeof(int))]; } ancillary = {0};
    char kind[2];
    struct iovec vector = {.iov_base = kind, .iov_len = sizeof(kind)};
    struct msghdr message = {.msg_iov = &vector, .msg_iovlen = 1,
                            .msg_control = ancillary.bytes, .msg_controllen = sizeof(ancillary.bytes)};
    ssize_t length = recvmsg(context->gate, &message, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
    if (length < 0) return -1;
    unsigned count = 0, headers = 0;
    int malformed = 0;
    for (struct cmsghdr *header = CMSG_FIRSTHDR(&message); header;
            header = CMSG_NXTHDR(&message, header)) {
        headers++;
        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS
                || header->cmsg_len < CMSG_LEN(0)) { malformed = 1; continue; }
        size_t bytes = header->cmsg_len - CMSG_LEN(0);
        if (bytes % sizeof(int)) malformed = 1;
        for (size_t i = 0; i < bytes / sizeof(int); i++) {
            int fd;
            memcpy(&fd, CMSG_DATA(header) + i * sizeof(fd), sizeof(fd));
            if (count < REF_COUNT) refs[count++] = fd;
            else { close(fd); malformed = 1; }
        }
    }
    if (length != 1 || kind[0] != 'F' || headers != 1 || count != REF_COUNT
            || malformed || (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC))) {
        errno = EPROTO; return -1;
    }
    struct stat stats[REF_COUNT];
    for (unsigned i = 0; i < REF_COUNT; i++) {
        struct statfs fs;
        int flags = fcntl(refs[i], F_GETFL);
        if (flags < 0 || fstatfs(refs[i], &fs) < 0 || fstat(refs[i], &stats[i]) < 0)
            return -1;
        if (!S_ISREG(stats[i].st_mode)) return denied();
        if (i < REF_USERNS) {
            if (!(flags & O_PATH) || fs.f_type != PROC_SUPER_MAGIC) return denied();
        } else if ((flags & (O_ACCMODE | O_PATH)) != O_RDONLY
                || fs.f_type != NSFS_MAGIC || ioctl(refs[i], NS_GET_NSTYPE) != CLONE_NEWUSER) {
            return denied();
        }
        for (unsigned j = 0; j < i; j++) {
            if (stats[i].st_dev == stats[j].st_dev && stats[i].st_ino == stats[j].st_ino)
                return denied();
        }
    }
    return still_waiting(context);
}

static int prepare(struct aegis_namespace *context, int package, int timeout) {
    if (timeout < 1 || timeout > 10000) { errno = EINVAL; return -1; }
    if (owner(context) < 0) return -1;
    if (context->mapped) { errno = EALREADY; return -1; }
    char map[1024], groups[32], oom[32];
    int refs[REF_COUNT] = {-1, -1, -1, -1, -1}, result = -1;
    if (still_waiting(context) < 0) goto done;
    context->mapped = -1;  /* Writing either kernel map is a one-shot action. */
    if (receive_references(context, refs, timeout) < 0) goto done;
    /* Never inherit Android's protected-daemon OOM exemption. Do this while
     * the child is gated, before its maps and later credential changes.
     * memory.oom.group cannot kill a member with oom_score_adj=-1000. */
    if (write_reference(refs[REF_OOM], "0", 1) < 0
            || read_reference(refs[REF_OOM], oom, sizeof(oom)) < 0) goto done;
    if (strcmp(oom, "0\n")) { errno = EPROTO; goto done; }
    size_t length = 0;
    for (size_t i = 0; i < sizeof(aegis_uid_extents) / sizeof(aegis_uid_extents[0]); i++) {
        const struct aegis_uid_extent *row = &aegis_uid_extents[i];
        int n = snprintf(map + length, sizeof(map) - length, "%u %u %u\n", row->inside,
                         context->user_id * AEGIS_PER_USER_RANGE + row->app_id, row->count);
        if (n < 0 || (size_t)n >= sizeof(map) - length) { errno = EOVERFLOW; goto done; }
        length += (size_t)n;
    }
    // Ordinary sessions irreversibly deny supplementary-group changes.
    // A package worker needs APT's technical-group transitions; its privileged
    // parent writes the same bounded map and retains the kernel's allow mode.
    // Only GIDs in this exact user's map are representable. No host group is
    // inherited (checked before clone and again in the gated child).
    if ((!package && write_reference(refs[REF_GROUPS], "deny", 4) < 0)
            || write_reference(refs[REF_UID], map, length) < 0
            || write_reference(refs[REF_GID], map, length) < 0
            || read_reference(refs[REF_GROUPS], groups, sizeof(groups)) < 0
            || mapped(refs[REF_UID], context->user_id) < 0
            || mapped(refs[REF_GID], context->user_id) < 0) goto done;
    if (strcmp(groups, package ? "allow\n" : "deny\n")) { errno = EPERM; goto done; }
    context->package = package;
    context->userns = refs[REF_USERNS];
    refs[REF_USERNS] = -1;
    context->mapped = 1;
    result = 0;
done:;
    int saved = errno;
    for (unsigned i = 0; i < REF_COUNT; i++) if (refs[i] >= 0) close(refs[i]);
    if (result < 0) close_gate(context);
    errno = saved;
    if (result < 0) return report_namespace_failure("private child reference preparation");
    return result;
}

int aegis_namespace_prepare(struct aegis_namespace *context) {
    return prepare(context, 0, 10000);
}

int aegis_namespace_prepare_package(struct aegis_namespace *context) {
    return prepare(context, 1, 10000);
}

int aegis_namespace_prepare_package_for(struct aegis_namespace *context, int timeout_ms) {
    return prepare(context, 1, timeout_ms);
}

int aegis_namespace_base_mount(struct aegis_namespace *context, int verified_source_fd) {
    if (still_waiting(context) < 0) return -1;
    if(temporary_base>=0 && same_base_mount(temporary_base,verified_source_fd)<0) { errno=EBUSY;return -1; }
    if (context->mapped != 1 || context->userns < 0) { errno = EAGAIN; return -1; }
    return aegis_clone_base_mount(verified_source_fd, context->userns, context->user_id);
}

int aegis_namespace_map_candidate(struct aegis_namespace *context, int candidate) {
    if (still_waiting(context) < 0) return -1;
    if (context->mapped != 1 || context->userns < 0) { errno = EAGAIN; return -1; }
    if (!context->package) return denied();
    if (aegis_map_candidate_mount(candidate, context->userns, context->user_id) < 0) return -1;
    return still_waiting(context);
}

int aegis_namespace_home_mount(struct aegis_namespace *context, int create) {
    if (still_waiting(context) < 0) return -1;
    if (context->mapped != 1 || context->userns < 0) { errno = EAGAIN; return -1; }
    if (context->package) return denied(); // no personal HOME in a package worker
    /* check_broker accepts only init's mount namespace or the exact private
     * copy established by this process. /data is never replaced by our base
     * anchor at /mnt. Neither path nor identity is chosen by a client. */
    int data = open("/data", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (data < 0) return -1;
    int home = aegis_ce_open_home(data, context->user_id, context->serial, create);
    int saved = errno;
    if (home >= 0 && create) {
        /* Initialize broker-only package metadata in this same authorized CE
         * lifecycle admission. No package installation or reusable approval.
         * This also gives the pending directory its broker-only SELinux type.
         * No package reference is exported to the ordinary runtime. */
        int packages = aegis_ce_open_packages(data, context->user_id, context->serial, 1);
        saved = errno;
        if (packages < 0) { close(home); home = -1; }
        else close(packages);
    }
    close(data);
    if (home < 0) { errno = saved; return -1; }
    int tree = aegis_ce_clone_home(home, context->user_id);
    saved = errno;
    close(home);
    if (tree < 0) { errno = saved; return -1; }
    /* Filesystem I/O may have outlasted the 10s gate. Do not return a storage
     * view for a child which already exited during preparation. */
    if (still_waiting(context) < 0) {
        saved = errno;
        close(tree);
        errno = saved;
        return -1;
    }
    return tree;
}

int aegis_namespace_devices_mount(struct aegis_namespace *context) {
    if (still_waiting(context) < 0) return -1;
    if (context->mapped != 1 || context->userns < 0) { errno = EAGAIN; return -1; }
    int tree = aegis_create_devices_mount(context->userns, context->user_id);
    if (tree < 0) return -1;
    if (still_waiting(context) < 0) {
        int saved = errno;
        close(tree);
        errno = saved;
        return -1;
    }
    return tree;
}

int aegis_namespace_resume(struct aegis_namespace *context) {
    if (owner(context) < 0) return -1;
    if (context->attempted || context->gate < 0) { errno = EALREADY; return -1; }
    /* Preserve the combined create/resume interface for callers that have no
     * host-side mount work. Preparing maps separately never executes code. */
    if (!context->mapped && aegis_namespace_prepare(context) < 0) return -1;
    int result = -1;
    if (still_waiting(context) < 0) goto done;
    if (context->mapped != 1 || context->userns < 0) { errno = EPROTO; goto done; }
    if (send(context->gate, "G", 1, MSG_NOSIGNAL | MSG_DONTWAIT) != 1) goto done;
    result = 0;
done:;
    int saved = errno;
    context->attempted = 1;
    close_gate(context);
    errno = saved;
    return result;
}

int aegis_namespace_stop(struct aegis_namespace *context) {
    if (owner(context) < 0) return -1;
    context->attempted = 1;
    close_gate(context);
    return aegis_child_request_stop(context->child);
}

int aegis_namespace_wait(struct aegis_namespace *context, int timeout_ms,
                         struct aegis_child_exit *result) {
    if (owner(context) < 0) return -1;
    return aegis_child_wait(context->child, timeout_ms, result);
}

void aegis_namespace_release(struct aegis_namespace *context) {
    if (!context) return;
    if (context->counted && context->child->owner == (pid_t)syscall(SYS_getpid))
        live_contexts--;
    close_gate(context);
    if (context->proc_root >= 0) close(context->proc_root);
    if (context->userns >= 0) close(context->userns);
    aegis_child_release(context->child);
    free(context);
}
