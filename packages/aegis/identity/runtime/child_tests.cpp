// Real kernel process/pidfd tests for the local Android QEMU guest only.
// These do not launch a runtime or claim namespace/mount/CE teardown.
#include "child.h"

#include <gtest/gtest.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/sched.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

int fd_count() {
    DIR* directory = opendir("/proc/self/fd");
    if (!directory) return -1;
    int count = 0;
    while (dirent* entry = readdir(directory)) if (entry->d_name[0] != '.') count++;
    closedir(directory);
    return count;
}

pid_t clone_with_pidfd(int* pidfd) {
    clone_args args = {};
    args.flags = CLONE_PIDFD;
    args.pidfd = reinterpret_cast<uintptr_t>(pidfd);
    args.exit_signal = SIGCHLD;
    return static_cast<pid_t>(syscall(SYS_clone3, &args, sizeof(args)));
}

class RuntimeChild : public ::testing::Test {
 protected:
    int pidfd = -1;
    int command = -1;
    aegis_child* handle = nullptr;

    void SetUp() override {
        struct sigaction disposition = {};
        ASSERT_EQ(0, sigaction(SIGCHLD, nullptr, &disposition));
        ASSERT_NE(SIG_IGN, disposition.sa_handler);
        ASSERT_EQ(0, disposition.sa_flags & SA_NOCLDWAIT);
        int pipe_fds[2];
        ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pipe_fds));
        pid_t child = clone_with_pidfd(&pidfd);
        if (child == 0) {
            close(pipe_fds[1]);
            alarm(10);  // A broken test must not leave a permanent fixture.
            unsigned char exit_code = 111;
            ssize_t n;
            do { n = read(pipe_fds[0], &exit_code, 1); } while (n < 0 && errno == EINTR);
            _exit(n == 1 ? exit_code : 111);
        }
        close(pipe_fds[0]);
        command = pipe_fds[1];
        ASSERT_GT(child, 0);
        ASSERT_GE(pidfd, 0);
        ASSERT_EQ(0, aegis_child_watch(pidfd, &handle));
    }

    void TearDown() override {
        if (command >= 0) close(command);
        if (pidfd >= 0) {
            // Only the fixture's stable kernel reference is used for cleanup.
            // An already-reaped child remains ESRCH/ECHILD, never another PID.
            syscall(SYS_pidfd_send_signal, pidfd, SIGKILL, nullptr, 0u);
            pollfd process = {pidfd, POLLIN, 0};
            if (poll(&process, 1, 3000) > 0) {
                siginfo_t info = {};
                waitid(P_PIDFD, static_cast<id_t>(pidfd), &info, WEXITED | WNOHANG);
            }
            close(pidfd);
        }
        aegis_child_release(handle);
    }

    void finish(unsigned char code) {
        ASSERT_EQ(1, send(command, &code, 1, MSG_NOSIGNAL));
    }
};

TEST_F(RuntimeChild, TimeoutRetainsObservationAndLeavesResultUntouched) {
    aegis_child_exit result = {123, 456};
    EXPECT_EQ(-1, aegis_child_wait(handle, 20, &result));
    EXPECT_EQ(ETIMEDOUT, errno);
    EXPECT_EQ(123, result.code);
    EXPECT_EQ(456, result.status);
    finish(37);
    ASSERT_EQ(0, aegis_child_wait(handle, 5000, &result));
    EXPECT_EQ(CLD_EXITED, result.code);
    EXPECT_EQ(37, result.status);
    EXPECT_EQ(0, aegis_child_wait(handle, 0, &result));
    EXPECT_EQ(37, result.status);
}

TEST_F(RuntimeChild, ForcedStopRequiresAnActualReapedExit) {
    ASSERT_EQ(0, aegis_child_request_stop(handle));
    aegis_child_exit result = {};
    ASSERT_EQ(0, aegis_child_wait(handle, 5000, &result));
    EXPECT_EQ(CLD_KILLED, result.code);
    EXPECT_EQ(SIGKILL, result.status);
    siginfo_t info = {};
    EXPECT_EQ(-1, waitid(P_PIDFD, static_cast<id_t>(pidfd), &info, WEXITED | WNOHANG));
    EXPECT_EQ(ECHILD, errno);
    EXPECT_EQ(0, aegis_child_request_stop(handle));
    EXPECT_EQ(0, aegis_child_wait(handle, 0, &result));
    EXPECT_EQ(SIGKILL, result.status);
}

TEST_F(RuntimeChild, AlreadyExitedChildIsValidatedWithoutConsumingItsStatus) {
    aegis_child_release(handle);
    handle = nullptr;
    finish(53);
    pollfd process = {pidfd, POLLIN, 0};
    ASSERT_EQ(1, poll(&process, 1, 5000));
    ASSERT_EQ(0, aegis_child_watch(pidfd, &handle));
    siginfo_t info = {};
    ASSERT_EQ(0, waitid(P_PIDFD, static_cast<id_t>(pidfd), &info,
                        WEXITED | WNOHANG | WNOWAIT));
    ASSERT_GT(info.si_pid, 0);
    EXPECT_EQ(53, info.si_status);
    aegis_child_exit result = {};
    ASSERT_EQ(0, aegis_child_wait(handle, 0, &result));
    EXPECT_EQ(CLD_EXITED, result.code);
    EXPECT_EQ(53, result.status);
}

TEST_F(RuntimeChild, AnotherReaperDoesNotBecomeASuccessfulObservation) {
    ASSERT_EQ(0, syscall(SYS_pidfd_send_signal, pidfd, SIGKILL, nullptr, 0u));
    pollfd process = {pidfd, POLLIN, 0};
    ASSERT_EQ(1, poll(&process, 1, 5000));
    siginfo_t info = {};
    ASSERT_EQ(0, waitid(P_PIDFD, static_cast<id_t>(pidfd), &info, WEXITED | WNOHANG));
    ASSERT_GT(info.si_pid, 0);
    aegis_child_exit result = {123, 456};
    EXPECT_EQ(-1, aegis_child_wait(handle, 0, &result));
    EXPECT_EQ(ECHILD, errno);
    EXPECT_EQ(-1, aegis_child_request_stop(handle));
    EXPECT_EQ(ECHILD, errno);
    EXPECT_EQ(123, result.code);
    EXPECT_EQ(456, result.status);
    aegis_child* lost = nullptr;
    EXPECT_EQ(-1, aegis_child_watch(pidfd, &lost));
    EXPECT_EQ(ECHILD, errno);
    EXPECT_EQ(nullptr, lost);
}

TEST_F(RuntimeChild, StoppedProcessIsStillLiveUntilItExits) {
    ASSERT_EQ(0, syscall(SYS_pidfd_send_signal, pidfd, SIGSTOP, nullptr, 0u));
    aegis_child_exit result = {};
    EXPECT_EQ(-1, aegis_child_wait(handle, 20, &result));
    EXPECT_EQ(ETIMEDOUT, errno);
    ASSERT_EQ(0, syscall(SYS_pidfd_send_signal, pidfd, SIGCONT, nullptr, 0u));
    finish(17);
    ASSERT_EQ(0, aegis_child_wait(handle, 5000, &result));
    EXPECT_EQ(CLD_EXITED, result.code);
    EXPECT_EQ(17, result.status);
}

TEST_F(RuntimeChild, CallerFdCanCloseWithoutLosingStableChildReference) {
    close(pidfd);
    pidfd = -1;
    ASSERT_EQ(0, aegis_child_request_stop(handle));
    aegis_child_exit result = {};
    ASSERT_EQ(0, aegis_child_wait(handle, 5000, &result));
    EXPECT_EQ(CLD_KILLED, result.code);
}

TEST_F(RuntimeChild, InvalidArgumentsCannotOverwriteOrConsumeLiveHandle) {
    aegis_child* original = handle;
    EXPECT_EQ(-1, aegis_child_watch(pidfd, &handle));
    EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(original, handle);
    aegis_child_exit result = {123, 456};
    for (int timeout : {-1, 60001}) {
        EXPECT_EQ(-1, aegis_child_wait(handle, timeout, &result));
        EXPECT_EQ(EINVAL, errno);
    }
    EXPECT_EQ(-1, aegis_child_wait(handle, 0, nullptr));
    EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_child_request_stop(nullptr));
    EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(123, result.code);
    finish(41);
    ASSERT_EQ(0, aegis_child_wait(handle, 5000, &result));
    EXPECT_EQ(41, result.status);
}

TEST_F(RuntimeChild, InvalidAndNonChildDescriptorsDoNotLeak) {
    int file = open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(file, 0);
    int self = static_cast<int>(syscall(SYS_pidfd_open, getpid(), 0u));
    ASSERT_GE(self, 0);
    int before = fd_count();
    ASSERT_GT(before, 0);
    for (int i = 0; i < 64; i++) {
        aegis_child* bad = nullptr;
        EXPECT_EQ(-1, aegis_child_watch(file, &bad));
        EXPECT_EQ(nullptr, bad);
        EXPECT_EQ(-1, aegis_child_watch(self, &bad));
        EXPECT_EQ(ECHILD, errno);
        EXPECT_EQ(nullptr, bad);
    }
    EXPECT_EQ(before, fd_count());
    close(file);
    close(self);
}

TEST_F(RuntimeChild, InheritedHandleDoesNotAuthorizeChildToSignalSibling) {
    int observer_pidfd = -1;
    pid_t observer = clone_with_pidfd(&observer_pidfd);
    if (observer == 0) {
        bool denied = aegis_child_request_stop(handle) == -1 && errno == EPERM;
        aegis_child_exit result = {};
        denied &= aegis_child_wait(handle, 0, &result) == -1 && errno == EPERM;
        // A raw clone3 child uses only this failure path and _exit, not libc
        // allocator/atfork state. Kernel exit closes its inherited descriptors.
        _exit(denied ? 0 : 1);
    }
    ASSERT_GT(observer, 0);
    ASSERT_GE(observer_pidfd, 0);
    pollfd process = {observer_pidfd, POLLIN, 0};
    int ready = poll(&process, 1, 5000);
    if (ready <= 0) {
        syscall(SYS_pidfd_send_signal, observer_pidfd, SIGKILL, nullptr, 0u);
        poll(&process, 1, 3000);
    }
    siginfo_t info = {};
    int waited = waitid(P_PIDFD, static_cast<id_t>(observer_pidfd), &info, WEXITED | WNOHANG);
    close(observer_pidfd);
    ASSERT_EQ(1, ready);
    ASSERT_EQ(0, waited);
    ASSERT_GT(info.si_pid, 0);
    EXPECT_EQ(CLD_EXITED, info.si_code);
    EXPECT_EQ(0, info.si_status);
    finish(19);
    aegis_child_exit result = {};
    ASSERT_EQ(0, aegis_child_wait(handle, 5000, &result));
    EXPECT_EQ(19, result.status);
}

TEST_F(RuntimeChild, ReleasingAnObserverDoesNotTerminateOrReapChild) {
    aegis_child_release(handle);
    handle = nullptr;
    ASSERT_EQ(0, aegis_child_watch(pidfd, &handle));
    aegis_child_exit result = {};
    EXPECT_EQ(-1, aegis_child_wait(handle, 0, &result));
    EXPECT_EQ(ETIMEDOUT, errno);
    finish(23);
    ASSERT_EQ(0, aegis_child_wait(handle, 5000, &result));
    EXPECT_EQ(23, result.status);
}

}  // namespace
