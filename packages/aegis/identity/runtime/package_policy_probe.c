/* Device test only. Pivot into an exclusively created tmpfs and fresh proc;
 * apply the actual package-worker policy and re-exec this trusted static probe.
 * No AOSP identity, CE, real package, repository, helper policy or host change. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "package_policy_probe.h"
#include "sandbox.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/securebits.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

static int caps(struct aegis_package_policy_probe *r) {
    struct __user_cap_header_struct header = {.version = _LINUX_CAPABILITY_VERSION_3};
    struct __user_cap_data_struct data[2] = {{0}, {0}};
    if (syscall(SYS_capget, &header, data) < 0) return -1;
    r->effective = data[0].effective | (uint64_t)data[1].effective << 32;
    r->permitted = data[0].permitted | (uint64_t)data[1].permitted << 32;
    r->inheritable = data[0].inheritable | (uint64_t)data[1].inheritable << 32;
    r->bounding = r->ambient = 0;
    for (int cap = 0; cap < 64; cap++) {
        int value = prctl(PR_CAPBSET_READ, cap, 0, 0, 0);
        if (value < 0) { if (errno == EINVAL) break; return -1; }
        if (value) r->bounding |= UINT64_C(1) << cap;
        value = prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_IS_SET, cap, 0, 0);
        if (value < 0) return -1;
        if (value) r->ambient |= UINT64_C(1) << cap;
    }
    return 0;
}

int aegis_probe_package_policy(uint32_t user) {
    if (getpid() != 1 || getppid() != 0 || getuid() || getgid()) return 101;
    int executable = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
    if (executable < 0) return 102;
    if (executable != 4) {
        if (dup3(executable, 4, O_CLOEXEC) < 0) return 102;
        close(executable);
    }
    // Only this disposable child's private mount namespace is changed. No
    // mkdir in the Android tree; /mnt is immediately covered by our own tmpfs.
    if (mount("tmpfs", "/mnt", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
              "mode=0755,size=1048576,nr_inodes=32") < 0
            || mkdir("/mnt/proc", 0755) < 0
            || mount("proc", "/mnt/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                     "hidepid=2,subset=pid") < 0
            || chdir("/mnt") < 0 || syscall(SYS_pivot_root, ".", ".") < 0
            || umount2(".", MNT_DETACH) < 0 || chdir("/") < 0
            || syscall(SYS_close_range, 5u, ~0u, 0u) < 0) return 103;
    // A mismatching requester must fail before any irreversible privilege drop.
    struct aegis_package_policy_probe before = {0}, after = {0};
    if (caps(&before) < 0) return 104;
    int bits = prctl(PR_GET_SECUREBITS, 0, 0, 0, 0);
    int nnp = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
    if (aegis_limit_package_worker(user + 1) != -1 || errno != EPERM
            || caps(&after) < 0 || before.effective != after.effective
            || before.permitted != after.permitted || before.inheritable != after.inheritable
            || before.bounding != after.bounding || before.ambient != after.ambient
            || bits != prctl(PR_GET_SECUREBITS, 0, 0, 0, 0)
            || nnp != prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0)) return 104;
    if (aegis_limit_package_worker(user) < 0) return 105;
    char number[24];snprintf(number, sizeof(number), "%u", user);
    char *args[] = {"aegis-package-policy-exec", number, NULL};
    char *env[] = {"LANG=C", NULL};
    syscall(SYS_execveat, 4, "", args, env, AT_EMPTY_PATH);
    return 106;
}

int aegis_probe_package_policy_exec(uint32_t user) {
    if (getpid() != 1 || getppid() != 0 || getuid() || getgid()
            || fcntl(4, F_GETFD) != -1 || errno != EBADF) return 107;
    struct aegis_package_policy_probe r = {.magic = 0x504b4753, .user = user, .checks = 1};
    if (caps(&r) < 0) return 108;
    r.nnp = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
    r.securebits = prctl(PR_GET_SECUREBITS, 0, 0, 0, 0);
    r.filter = prctl(PR_GET_SECCOMP, 0, 0, 0, 0);
    struct stat st;
    if (lstat("/system", &st) == -1 && errno == ENOENT
            && lstat("/data", &st) == -1 && errno == ENOENT
            && lstat("/sys", &st) == -1 && errno == ENOENT
            && lstat("/home", &st) == -1 && errno == ENOENT) r.checks |= 2;
    if (prctl(PR_SET_SECUREBITS, 0, 0, 0, 0) == -1 && errno == EPERM
            && prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_SYS_ADMIN, 0, 0) == -1
            && errno == EPERM) r.checks |= 4;
    if (mount("tmpfs", "/", "tmpfs", 0, NULL) == -1 && errno == EPERM
            && syscall(SYS_unshare, CLONE_NEWUSER) == -1 && errno == EPERM
            && syscall(SYS_clone3, NULL, 0) == -1 && errno == ENOSYS
            && chroot("/") == -1 && errno == EPERM
            && ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1 && errno == EPERM) r.checks |= 8;
    int file = open("/owned", O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
    if (file < 0) return 109;
    if (fchown(file, 42, 42) == 0 && fchmod(file, 0000) == 0 && fstat(file, &st) == 0
            && st.st_uid == 42 && st.st_gid == 42) {
        int reopened = open("/owned", O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
        if (reopened >= 0) {
            if (write(reopened, "owned", 5) == 5) r.checks |= 16;
            close(reopened);
        }
    }
    close(file);
    pid_t child = fork();
    if (child < 0) return 110;
    if (!child) {
        close(3);
        struct aegis_package_policy_probe dropped = {0};
        int ok = setresgid(42, 42, 42) == 0 && setresuid(42, 42, 42) == 0
            && caps(&dropped) == 0 && !dropped.effective && !dropped.permitted && !dropped.ambient
            && setresuid(0, 0, 0) == -1 && errno == EPERM;
        _exit(ok ? 0 : 111);
    }
    int status;
    if (waitpid(child, &status, 0) != child) return 112;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) r.checks |= 32;
    return send(3, &r, sizeof(r), MSG_NOSIGNAL) == (ssize_t)sizeof(r) ? 0 : 113;
}
