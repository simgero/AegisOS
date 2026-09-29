#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "base_image.h"
#include "broker_cgroup.h"
#include "broker_owner.h"
#include "namespace.h"
#include <cutils/sockets.h>
#include <log/log.h>
#include <private/android_filesystem_config.h>
#include <selinux/selinux.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <linux/magic.h>
#include <linux/openat2.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static int fail(int error) { errno = error; return -1; }
/* Init connects daemon stderr to /dev/null. Fixed phase names and errno are
 * useful at boot without logging credentials, user paths or protocol data. */
static void report_failure(const char *phase, int error) {
    __android_log_print(ANDROID_LOG_ERROR, "AegisRuntimeBroker",
                        "AEGIS_RUNTIME_BROKER_FAILED: %s errno=%d", phase, error);
    errno = error;
}
static int inherit_host_namespaces(void) {
    /* Init's Descriptor::Publish sanitizes each non-alphanumeric byte to '_'.
     * android_get_control_file uses realpath, which does not describe NSFS
     * magic links. Parse only these fixed init entries, then validate the
     * actual descriptors in pin_host. No caller-supplied paths or handles. */
    static const char *const names[] = {
        "ANDROID_FILE__proc_1_ns_user", "ANDROID_FILE__proc_1_ns_pid",
        "ANDROID_FILE__proc_1_ns_mnt",
    };
    int descriptors[] = {-1, -1, -1};
    int result = -1;
    for (unsigned i = 0; i < 3; i++) {
        const char *text = getenv(names[i]);
        if (!text || !*text || strlen(text) > 10) { errno = EBADF; goto done; }
        for (const char *p = text; *p; p++)
            if (*p < '0' || *p > '9') { errno = EBADF; goto done; }
        char *end;
        errno = 0;
        long value = strtol(text, &end, 10);
        if (errno || *end || value < 3 || value > INT_MAX) { errno = EBADF; goto done; }
        descriptors[i] = (int)value;
        for (unsigned j = 0; j < i; j++)
            if (descriptors[i] == descriptors[j]) { descriptors[i] = -1; errno = EBADF; goto done; }
    }
    if (getppid() != 1) { errno = EPERM; goto done; }
    result = aegis_namespace_pin_host(descriptors[0], descriptors[1], descriptors[2]);
done:;
    int saved = errno;
    for (unsigned i = 0; i < 3; i++) {
        if (descriptors[i] >= 0) close(descriptors[i]);
        unsetenv(names[i]);
    }
    return result < 0 ? fail(saved) : 0;
}
static uint64_t now_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return 0;
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) + now.tv_nsec;
}
static uint64_t stop_deadline(void) {
    uint64_t now = now_ns();
    return now ? now + AEGIS_BROKER_MAX_WAIT_NS : 0;
}
static int exact_label(int fd, const char *expected) {
    char *label = NULL;
    if (fgetfilecon(fd, &label) < 0) return -1;
    int result = label && !strcmp(label, expected);
    freecon(label);
    return result ? 0 : fail(EPERM);
}
static int helper(const char *name, const char *label) {
    int fd = open(name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return -1;
    struct stat st;
    struct statfs fs;
    struct statvfs flags;
    if (fstat(fd, &st) < 0 || fstatfs(fd, &fs) < 0 || fstatvfs(fd, &flags) < 0) goto error;
    /* AOSP's installed /system/bin executables are root:shell, mode 0755.
     * The group has no write access; EROFS, immutable contents and the exact
     * executable label remain mandatory. Do not normalize image ownership. */
    if (st.st_mode != (S_IFREG | 0755) || st.st_uid != AID_ROOT
            || st.st_gid != AID_SHELL || st.st_nlink != 1
            || st.st_size < 64 || st.st_size > 32 * 1024 * 1024
            || fs.f_type != EROFS_SUPER_MAGIC_V1 || !(flags.f_flag & ST_RDONLY)
            || (flags.f_flag & ST_NOEXEC)) { errno = EPERM; goto error; }
    if (exact_label(fd, label) < 0) goto error;
    return fd;
error:;
    int saved = errno;
    close(fd); return fail(saved);
}
static int exclusive_lock(void) {
    int data = open("/data", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (data < 0) return -1;
    struct open_how how = {
        .flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC,
        .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_XDEV,
    };
    int directory = (int)syscall(SYS_openat2, data, "misc/aegis-runtime", &how, sizeof(how));
    int saved = errno; close(data); errno = saved;
    if (directory < 0) return -1;
    int fd = -1, result = -1;
    struct stat dir, st, named;
    if (fstat(directory, &dir) < 0) goto done;
    if (dir.st_mode != (S_IFDIR | 0700) || dir.st_uid || dir.st_gid) { errno = EPERM; goto done; }
    if (exact_label(directory, "u:object_r:aegis_runtime_state_file:s0") < 0) goto done;
    fd = openat(directory, "broker.lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0 || fstat(fd, &st) < 0) goto done;
    if (st.st_mode != (S_IFREG | 0600) || st.st_uid || st.st_gid || st.st_nlink != 1
            || st.st_dev != dir.st_dev || st.st_size) { errno = EPERM; goto done; }
    if (exact_label(fd, "u:object_r:aegis_runtime_state_file:s0") < 0
            || flock(fd, LOCK_EX | LOCK_NB) < 0
            || fstatat(directory, "broker.lock", &named, AT_SYMLINK_NOFOLLOW) < 0) goto done;
    if (named.st_dev != st.st_dev || named.st_ino != st.st_ino) { errno = ESTALE; goto done; }
    result = fd;
done:;
    saved = errno;
    if (result < 0 && fd >= 0) close(fd);
    close(directory); errno = saved;
    return result;
}
static int inherited_listener(void) {
    int original = android_get_control_socket("aegis_runtime");
    if (original < 0) return fail(EBADF);
    int fd = fcntl(original, F_DUPFD_CLOEXEC, 3);
    if (fd < 0) return -1;
    close(original);
    int type;
    socklen_t length = sizeof(type);
    struct sockaddr_un address = {0};
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &length) < 0) goto error;
    if (length != sizeof(type) || type != SOCK_SEQPACKET) { errno = EPROTOTYPE; goto error; }
    length = sizeof(address);
    if (getsockname(fd, (struct sockaddr *)&address, &length) < 0) goto error;
    static const char path[] = "/dev/socket/aegis_runtime";
    if (address.sun_family != AF_UNIX || length != offsetof(struct sockaddr_un, sun_path) + sizeof(path)
            || memcmp(address.sun_path, path, sizeof(path))) { errno = EPERM; goto error; }
    struct stat st;
    if (lstat(path, &st) < 0) goto error;
    if (st.st_mode != (S_IFSOCK | 0600) || st.st_uid != 1000 || st.st_gid != 1000) {
        errno = EPERM; goto error;
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) goto error;
    return fd;
error:;
    int saved = errno;
    close(fd); return fail(saved);
}
static int reply_until(int socket, int signals, const struct aegis_broker_request *request,
                        int error, enum aegis_broker_state state, uint64_t command,
                        int wait_status, int exited, int master) {
    for (;;) {
        uint64_t now = now_ns();
        if (!now || now >= request->deadline_ns) return fail(ETIMEDOUT);
        int sent = request->operation >= AEGIS_BROKER_EXEC
                ? aegis_broker_reply_terminal(socket, request, error, command, wait_status, exited, master)
                : aegis_broker_reply(socket, request, error, state);
        if (sent == 0) return 0;
        if (errno == EINTR) continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK) return -1;
        struct pollfd wait[] = {{socket, POLLOUT | POLLRDHUP, 0}, {signals, POLLIN, 0}};
        int timeout = (int)((request->deadline_ns - now) / 1000000);
        if (!timeout) return fail(ETIMEDOUT);
        int ready = poll(wait, 2, timeout);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) return -1;
        if (!ready) return fail(ETIMEDOUT);
        if (wait[1].revents) {
            struct signalfd_siginfo info;
            ssize_t count = read(signals, &info, sizeof(info));
            if (count != sizeof(info)) return fail(EIO);
            if (info.ssi_signo != SIGCHLD) return fail(ECANCELED);
            // Do not turn an unrelated child exit into connection loss.
        }
        if (wait[0].revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL))
            return fail(EPIPE);
    }
}
static int serve(int listener, int signals, struct aegis_broker_owner *owner) {
    int peer = -1, result = -1;
    uint64_t sequence = 0, hello_deadline = 0;
    for (;;) {
        struct pollfd events[] = {{signals, POLLIN, 0}, {peer, POLLIN | POLLRDHUP, 0},
                                  {listener, POLLIN, 0}};
        int ready = poll(events, 3, 1000);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) break;
        if (events[0].revents) {
            struct signalfd_siginfo info;
            ssize_t count = read(signals, &info, sizeof(info));
            if (count != sizeof(info)) { errno = EIO; break; }
            if (info.ssi_signo != SIGCHLD) { result = 0; break; }
            // The context owner alone consumes waitid(P_PIDFD) child status.
        }
        // Close completed publication resources even if the original client
        // never polls its result. Cleanup failure takes the common stop path.
        if (aegis_broker_owner_reap_publications(owner) < 0) break;
        uint64_t now = now_ns();
        if (!now) break;
        if (peer >= 0 && ((events[1].revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL))
                || (!sequence && now >= hello_deadline))) goto disconnect;
        if (peer >= 0 && (events[1].revents & POLLIN)) {
            struct aegis_broker_call call;
            if (aegis_broker_receive_call(peer, sequence, now, &call) < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                goto disconnect;
            }
            const struct aegis_broker_request *request = &call.request;
            sequence = request->sequence;
            enum aegis_broker_state state = AEGIS_BROKER_SEALED;
            uint64_t command = 0;
            int master = -1, wait_status = 0, exited = 0, error = 0;
            if (request->operation == AEGIS_BROKER_EXEC) {
                if (aegis_broker_owner_exec(owner, &call, &command, &master) < 0) error = errno;
                else state = AEGIS_BROKER_READY;
            } else if (request->operation == AEGIS_BROKER_RESULT) {
                if (aegis_broker_owner_result(owner, request, call.command, &wait_status, &exited) < 0)
                    error = errno;
                else { command = call.command; state = AEGIS_BROKER_READY; }
            } else if (aegis_broker_owner_apply(owner, request, &state) < 0) error = errno;
            if (error <= 0 && state == AEGIS_BROKER_SEALED && request->operation != AEGIS_BROKER_STATUS)
                error = EIO;
            if (error < 0 || error > 4095) error = EIO;
            int replied = reply_until(peer, signals, request, error, state, command, wait_status, exited, master);
            int saved = errno;
            // SCM_RIGHTS copies the reference. Never retain a second host PTY
            // after publication, or after an uncertain/failed send.
            if (master >= 0) close(master);
            errno = saved;
            if (replied < 0) {
                if (errno == ECANCELED) { result = 0; break; }
                goto disconnect;
            }
        }
        if (events[2].revents & (POLLHUP | POLLERR | POLLNVAL)) { errno = EPIPE; break; }
        if (events[2].revents & POLLIN) {
            int connection = accept4(listener, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
            if (connection < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                break;
            }
            // A second connection cannot steal or stop the current session.
            if (peer >= 0 || aegis_broker_check_peer(connection) < 0) { close(connection); continue; }
            peer = connection; sequence = 0; hello_deadline = stop_deadline();
            if (!hello_deadline) goto disconnect;
        }
        continue;
disconnect:
        close(peer); peer = -1; sequence = 0;
        if (aegis_broker_owner_stop_all(owner, stop_deadline()) < 0) break;
        // Only confirmed cleanup permits another authenticated HELLO.
    }
    int saved = errno;
    if (peer >= 0) close(peer);
    if (aegis_broker_owner_stop_all(owner, stop_deadline()) < 0) return -1;
    errno = saved;
    return result;
}

int main(int argc, char **argv) {
    (void)argv;
    umask(0077);
    char mode[PROP_VALUE_MAX], *context = NULL;
    if (argc != 1) { report_failure("argument count", EINVAL); return 1; }
    if (__system_property_get("ro.aegis.runtime.mode", mode) <= 0 || strcmp(mode, "managed-v1")) {
        report_failure("runtime mode", EPERM); return 1;
    }
    if (is_selinux_enabled() != 1) { report_failure("SELinux enabled", EPERM); return 1; }
    if (security_getenforce() != 1) { report_failure("SELinux enforcing", EPERM); return 1; }
    if (getcon(&context) < 0) { report_failure("read own security context", errno); return 1; }
    if (!context || strcmp(context, "u:r:aegis_runtime_broker:s0")) {
        freecon(context); report_failure("own security context", EPERM);
        return 1;
    }
    freecon(context);
    struct sigaction action = {.sa_handler = SIG_DFL};
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGCHLD, &action, NULL) < 0 || setgroups(0, NULL) < 0) {
        report_failure("initial process state", errno);
        return 1;
    }
    if (inherit_host_namespaces() < 0) {
        report_failure("init namespace handoff", errno);
        return 1;
    }
    if (aegis_namespace_check_broker() < 0) {
        report_failure("initial namespaces", errno);
        return 1;
    }
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM); sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP); sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        report_failure("initial signal mask", errno);
        return 1;
    }
    int signals = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    int lock = -1, root = -1, delegation = -1, entry = -1, parent = -1;
    int base = -1, setup = -1, init = -1, listener = -1;
    struct aegis_broker_owner *owner = NULL;
    const char *phase = "signals";
    int result = 1;
    if (signals < 0) goto done;
    phase = "exclusive lock"; lock = exclusive_lock(); if (lock < 0) goto done;
    phase = "init cgroup delegation";
    root = open("/sys/fs/cgroup", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (root < 0) goto done;
    delegation = aegis_broker_cgroup_open_delegation(root, &entry);
    if (delegation < 0) goto done;
    phase = "cgroup recovery";
    parent = aegis_broker_cgroup_prepare(delegation, 10000); if (parent < 0) goto done;
    phase = "private mount namespace";
    if (aegis_namespace_private_mounts() < 0) goto done;
    phase = "immutable base"; base = aegis_base_open(); if (base < 0) goto done;
    phase = "private base anchor";
    if (aegis_namespace_attach_base(base) < 0) goto done;
    phase = "setup helper";
    setup = helper("/system/bin/aegis-runtime-setup", "u:object_r:aegis_runtime_setup_exec:s0");
    if (setup < 0) goto done;
    phase = "init helper";
    init = helper("/system/bin/aegis-runtime-init", "u:object_r:aegis_runtime_init_exec:s0");
    if (init < 0) goto done;
    phase = "context owner";
    if (aegis_broker_owner_create(parent, base, setup, init, &owner) < 0) goto done;
    phase = "init socket"; listener = inherited_listener(); if (listener < 0) goto done;
    if (clearenv() < 0 || listen(listener, 4) < 0) goto done;
    __android_log_print(ANDROID_LOG_INFO, "AegisRuntimeBroker", "AEGIS_RUNTIME_BROKER_LISTENING");
    phase = "control channel";
    if (serve(listener, signals, owner) < 0) goto done;
    result = 0;
done:;
    int saved = errno;
    if (listener >= 0) close(listener);
    if (owner) {
        if (aegis_broker_owner_stop_all(owner, stop_deadline()) < 0
                || aegis_broker_owner_release(&owner) < 0) {
            if (!result) { saved = errno; phase = "final context cleanup"; }
            result = 1;
        }
    }
    if (result) report_failure(phase, saved);
    else __android_log_print(ANDROID_LOG_INFO, "AegisRuntimeBroker", "AEGIS_RUNTIME_BROKER_STOPPED");
    // On incomplete cleanup no ACK is sent. Process death closes remaining
    // references; the next owner must recover the private group before HELLO.
    int descriptors[] = {init, setup, base, parent, entry, delegation, root, signals, lock};
    for (unsigned i = 0; i < sizeof(descriptors) / sizeof(descriptors[0]); ++i)
        if (descriptors[i] >= 0) close(descriptors[i]);
    return result;
}
