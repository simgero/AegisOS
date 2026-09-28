// Compile on aegis-build; run only in the local Android QEMU guest. These
// fixtures own empty, exclusive cgroups and inert children, never AOSP users.
#include "memory_group.h"
#include "context.h"
#include "broker_owner.h"
#include "namespace.h"
#include "namespace_probe.h"

#include <gtest/gtest.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <linux/sched.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace {

int put(int directory, const char* name, const char* value) {
    int fd = openat(directory, name, O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    size_t size = strlen(value);
    ssize_t result = write(fd, value, size);
    int saved = result < 0 ? errno : EIO;
    close(fd);
    errno = saved;
    return result == static_cast<ssize_t>(size) ? 0 : -1;
}

std::string get(int directory, const char* name) {
    int fd = openat(directory, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return "<open-failed>";
    char text[512];
    ssize_t size = read(fd, text, sizeof(text));
    close(fd);
    return size < 0 ? "<read-failed>" : std::string(text, static_cast<size_t>(size));
}

class RuntimeMemoryGroup : public ::testing::Test {
 protected:
    int root = -1, parent = -1, pidfds[2] = {-1, -1};
    std::string name;
    bool created = false;
    aegis_memory_group* groups[2] = {nullptr, nullptr};
    aegis_namespace* context = nullptr;
    aegis_context* runtime = nullptr;
    aegis_broker_owner* broker = nullptr;
    int peers[2] = {-1, -1}, setup = -1;

    void SetUp() override {
        ASSERT_EQ(0u, getuid()); ASSERT_EQ(0u, getgid());
        ASSERT_EQ(0, setgroups(0, nullptr));
        struct sigaction action = {};
        action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask);
        ASSERT_EQ(0, sigaction(SIGCHLD, &action, nullptr));
        root = open("/sys/fs/cgroup", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        ASSERT_GE(root, 0);
        name = "aegis-memory-test-" + std::to_string(getpid());
        ASSERT_EQ(0, mkdirat(root, name.c_str(), 0700)) << strerror(errno);
        created = true;  // Cleanup only the directory this test just created.
        parent = openat(root, name.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        ASSERT_GE(parent, 0);
        ASSERT_EQ(0, fchown(parent, 0, 0));
        ASSERT_EQ(0, fchmod(parent, 0700));
        ASSERT_EQ("", get(parent, "cgroup.procs"));
        // The real Android root must already delegate memory. Never change
        // its controller settings or move our test process out of its group.
        ASSERT_EQ(0, put(parent, "cgroup.subtree_control", "+memory\n")) << strerror(errno);
    }

    void TearDown() override {
        if (broker) {
            timespec now = {};
            ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &now));
            uint64_t deadline = static_cast<uint64_t>(now.tv_sec) * 1000000000 + now.tv_nsec + 5000000000;
            EXPECT_EQ(0, aegis_broker_owner_stop_all(broker, deadline));
            EXPECT_EQ(0, aegis_broker_owner_release(&broker));
        }
        if (runtime) EXPECT_EQ(0, aegis_context_stop(&runtime, 5000));
        if (context) {
            EXPECT_EQ(0, aegis_namespace_stop(context));
            aegis_child_exit result = {};
            EXPECT_EQ(0, aegis_namespace_wait(context, 5000, &result));
            aegis_namespace_release(context);
        }
        for (int i = 0; i < 2; i++) {
            if (groups[i]) EXPECT_EQ(0, aegis_memory_group_kill_and_wait(groups[i], 5000));
            if (pidfds[i] >= 0) {
                syscall(SYS_pidfd_send_signal, pidfds[i], SIGKILL, nullptr, 0u);
                pollfd ready = {pidfds[i], POLLIN, 0};
                if (poll(&ready, 1, 5000) > 0) {
                    siginfo_t info = {};
                    waitid(P_PIDFD, static_cast<id_t>(pidfds[i]), &info, WEXITED | WNOHANG);
                } else ADD_FAILURE() << "Owned test child did not exit";
                close(pidfds[i]);
            }
            if (groups[i]) EXPECT_EQ(0, aegis_memory_group_remove(&groups[i]));
            if (peers[i] >= 0) close(peers[i]);
        }
        if (setup >= 0) close(setup);
        if (parent >= 0) close(parent);
        if (created) EXPECT_EQ(0, unlinkat(root, name.c_str(), AT_REMOVEDIR)) << strerror(errno);
        if (root >= 0) close(root);
    }

    int make(unsigned slot, uint32_t user = 10) {
        return aegis_memory_group_create(parent, user, 1234, &groups[slot]);
    }

    int open_group(uint32_t user = 10) {
        std::string child = "u" + std::to_string(user) + "-s1234";
        return openat(parent, child.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    }

    int spawn(unsigned slot, uint32_t user) {
        int target = aegis_memory_group_claim(groups[slot], user, 1234);
        if (target < 0) return -1;
        clone_args args = {};
        args.flags = CLONE_PIDFD | CLONE_INTO_CGROUP;
        args.pidfd = reinterpret_cast<uintptr_t>(&pidfds[slot]);
        args.cgroup = static_cast<uint64_t>(target);
        args.exit_signal = SIGCHLD;
        pid_t child = static_cast<pid_t>(syscall(SYS_clone3, &args, sizeof(args)));
        if (child == 0) {
            // Raw clone: use only raw syscalls until exit. Bound the lifetime
            // even if a broken test never kills the fixture.
            timespec delay = {.tv_sec = 10, .tv_nsec = 0};
            syscall(SYS_ppoll, nullptr, 0u, &delay, nullptr, sizeof(uint64_t));
            syscall(SYS_exit_group, 91);
            __builtin_unreachable();
        }
        return child > 0 ? 0 : -1;
    }

    void open_probe() {
        char path[PATH_MAX];
        ssize_t size = readlink("/proc/self/exe", path, sizeof(path) - 1);
        ASSERT_GT(size, 0); ASSERT_LT(size, static_cast<ssize_t>(sizeof(path) - 1));
        path[size] = 0;
        std::string binary(path);
        binary.resize(binary.find_last_of('/') + 1);
        binary += "aegis-runtime-namespace-probe";
        setup = open(binary.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        ASSERT_GE(setup, 0);
        ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, peers));
    }
};

class RuntimeContext : public RuntimeMemoryGroup {};
class RuntimeBrokerOwner : public RuntimeMemoryGroup {};

uint64_t deadline_ns() {
    timespec now = {};
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return 0;
    return static_cast<uint64_t>(now.tv_sec) * 1000000000 + now.tv_nsec + 5000000000;
}

struct aegis_broker_request broker_request(uint16_t operation, uint32_t user, uint32_t serial) {
    struct aegis_broker_request request = {};
    request.magic = AEGIS_BROKER_MAGIC; request.version = AEGIS_BROKER_VERSION;
    request.operation = operation; request.sequence = 2; request.deadline_ns = deadline_ns();
    request.user = user; request.serial = serial;
    return request;
}

int descriptors() {
    DIR* directory = opendir("/proc/self/fd");
    if (!directory) return -1;
    int count = 0;
    while (dirent* entry = readdir(directory)) if (entry->d_name[0] != '.') count++;
    closedir(directory);
    return count;
}

TEST_F(RuntimeContext, RejectedHelperRetainsCleanupAndPreservesCallerDescriptors) {
    int file = open("/dev/null", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    ASSERT_GE(file, 0);
    int before = descriptors();
    ASSERT_GT(before, 0);
    int result = aegis_context_start(10, 1234, parent, file, file, file, 0, &runtime);
    EXPECT_EQ(-1, result); EXPECT_NE(nullptr, runtime);
    EXPECT_GE(fcntl(file, F_GETFD), 0); EXPECT_GE(fcntl(parent, F_GETFD), 0);
    if (runtime) {
        EXPECT_EQ(-1, aegis_context_channel(runtime)); EXPECT_EQ(EAGAIN, errno);
        EXPECT_EQ(-1, aegis_context_stop(&runtime, -1)); EXPECT_EQ(EINVAL, errno);
        EXPECT_NE(nullptr, runtime);
        EXPECT_EQ(0, aegis_context_stop(&runtime, 5000));
        EXPECT_EQ(nullptr, runtime);
    }
    struct stat st;
    EXPECT_EQ(-1, fstatat(parent, "u10-s1234", &st, AT_SYMLINK_NOFOLLOW)); EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(before, descriptors());
    close(file);
}

TEST_F(RuntimeContext, ExpiredOrUnboundedDeadlineCannotAllocateResources) {
    for (uint64_t deadline : {UINT64_C(0), UINT64_MAX, deadline_ns() + UINT64_C(10000000000)}) {
        EXPECT_EQ(-1, aegis_context_start_until(10, 1234, parent, parent, parent, parent,
                                               0, deadline, &runtime));
        EXPECT_EQ(EINVAL, errno); EXPECT_EQ(nullptr, runtime);
        struct stat st;
        EXPECT_EQ(-1, fstatat(parent, "u10-s1234", &st, AT_SYMLINK_NOFOLLOW));
        EXPECT_EQ(ENOENT, errno);
    }
}

TEST_F(RuntimeBrokerOwner, FailedStartRemainsOwnedAndOnlyItsUserStopConfirmsRemoval) {
    int before = descriptors();
    ASSERT_EQ(0, aegis_broker_owner_create(parent, parent, parent, parent, &broker));
    EXPECT_EQ(before + 4, descriptors());
    auto request = broker_request(AEGIS_BROKER_START, 10, 1234);
    aegis_broker_state state = AEGIS_BROKER_READY;
    EXPECT_EQ(-1, aegis_broker_owner_apply(broker, &request, &state));
    EXPECT_EQ(AEGIS_BROKER_SEALED, state);
    EXPECT_EQ(-1, aegis_broker_owner_release(&broker)); EXPECT_EQ(EBUSY, errno);
    request = broker_request(AEGIS_BROKER_STOP_USER, 11, 0);
    ASSERT_EQ(0, aegis_broker_owner_apply(broker, &request, &state));
    EXPECT_EQ(AEGIS_BROKER_ABSENT, state);
    EXPECT_EQ(-1, aegis_broker_owner_release(&broker)); EXPECT_EQ(EBUSY, errno);
    request = broker_request(AEGIS_BROKER_STATUS, 10, 1235);
    EXPECT_EQ(-1, aegis_broker_owner_apply(broker, &request, &state)); EXPECT_EQ(ESTALE, errno);
    request = broker_request(AEGIS_BROKER_STOP_USER, 10, 0);
    ASSERT_EQ(0, aegis_broker_owner_apply(broker, &request, &state));
    EXPECT_EQ(AEGIS_BROKER_ABSENT, state);
    ASSERT_EQ(0, aegis_broker_owner_release(&broker)); EXPECT_EQ(nullptr, broker);
    EXPECT_EQ(before, descriptors());
    EXPECT_GE(fcntl(parent, F_GETFD), 0);
}

TEST_F(RuntimeBrokerOwner, PartialStartsAreBoundedAndGlobalCleanupVisitsEverySlot) {
    ASSERT_EQ(0, aegis_broker_owner_create(parent, parent, parent, parent, &broker));
    aegis_broker_state state = AEGIS_BROKER_ABSENT;
    for (uint32_t user = 10; user < 26; user++) {
        auto request = broker_request(AEGIS_BROKER_START, user, 1234);
        EXPECT_EQ(-1, aegis_broker_owner_apply(broker, &request, &state));
        EXPECT_EQ(AEGIS_BROKER_SEALED, state);
        std::string group = "u" + std::to_string(user) + "-s1234";
        struct stat st;
        EXPECT_EQ(0, fstatat(parent, group.c_str(), &st, AT_SYMLINK_NOFOLLOW));
    }
    auto request = broker_request(AEGIS_BROKER_START, 26, 1234);
    EXPECT_EQ(-1, aegis_broker_owner_apply(broker, &request, &state)); EXPECT_EQ(ENOSPC, errno);
    // Every partial fixture is already empty; no waiting is needed, but every
    // context/group still needs its explicit ownership-based cleanup.
    ASSERT_EQ(0, aegis_broker_owner_stop_all(broker, 0));
    ASSERT_EQ(0, aegis_broker_owner_release(&broker));
    for (uint32_t user = 10; user < 26; user++) {
        std::string group = "u" + std::to_string(user) + "-s1234";
        struct stat st;
        EXPECT_EQ(-1, fstatat(parent, group.c_str(), &st, AT_SYMLINK_NOFOLLOW)); EXPECT_EQ(ENOENT, errno);
    }
}

TEST_F(RuntimeBrokerOwner, RawCloneCannotUseTheParentsResourceRegistry) {
    ASSERT_EQ(0, aegis_broker_owner_create(parent, parent, parent, parent, &broker));
    auto request = broker_request(AEGIS_BROKER_STATUS, 10, 1234);
    clone_args args = {};
    args.flags = CLONE_PIDFD; args.pidfd = reinterpret_cast<uintptr_t>(&pidfds[0]);
    args.exit_signal = SIGCHLD;
    pid_t child = static_cast<pid_t>(syscall(SYS_clone3, &args, sizeof(args)));
    if (!child) {
        aegis_broker_state state;
        bool denied = aegis_broker_owner_apply(broker, &request, &state) == -1 && errno == EPERM;
        denied = denied && aegis_broker_owner_stop_all(broker, 0) == -1 && errno == EPERM;
        denied = denied && aegis_broker_owner_release(&broker) == -1 && errno == EPERM;
        syscall(SYS_exit_group, denied ? 0 : 92);
        __builtin_unreachable();
    }
    ASSERT_GT(child, 0);
    pollfd exited = {pidfds[0], POLLIN, 0};
    ASSERT_EQ(1, poll(&exited, 1, 5000));
    siginfo_t info = {};
    ASSERT_EQ(0, waitid(P_PIDFD, static_cast<id_t>(pidfds[0]), &info, WEXITED | WNOHANG));
    EXPECT_EQ(CLD_EXITED, info.si_code); EXPECT_EQ(0, info.si_status);
    ASSERT_EQ(0, aegis_broker_owner_release(&broker));
}

TEST_F(RuntimeContext, RejectedStartNeverRemovesAnAlreadyOwnedGroup) {
    ASSERT_EQ(0, make(0));
    // Valid descriptors are sufficient to reach exclusive group creation;
    // the duplicate name must stop before helper validation or CE access.
    EXPECT_EQ(-1, aegis_context_start(10, 1234, parent, parent, parent, parent, 0, &runtime));
    EXPECT_EQ(EEXIST, errno); ASSERT_NE(nullptr, runtime);
    ASSERT_EQ(0, aegis_context_stop(&runtime, 5000));
    EXPECT_GE(aegis_memory_group_claim(groups[0], 10, 1234), 0);
}

TEST_F(RuntimeContext, RawCloneCannotReleaseAnInheritedPartialContext) {
    ASSERT_EQ(-1, aegis_context_start(10, 1234, parent, parent, parent, parent, 0, &runtime));
    ASSERT_NE(nullptr, runtime);
    clone_args args = {};
    args.flags = CLONE_PIDFD;
    args.pidfd = reinterpret_cast<uintptr_t>(&pidfds[0]);
    args.exit_signal = SIGCHLD;
    pid_t child = static_cast<pid_t>(syscall(SYS_clone3, &args, sizeof(args)));
    if (!child) {
        bool denied = aegis_context_channel(runtime) == -1 && errno == EPERM;
        denied = denied && aegis_context_stop(&runtime, 0) == -1 && errno == EPERM;
        syscall(SYS_exit_group, denied ? 0 : 92);
        __builtin_unreachable();
    }
    ASSERT_GT(child, 0);
    pollfd exited = {pidfds[0], POLLIN, 0};
    ASSERT_EQ(1, poll(&exited, 1, 5000));
    siginfo_t info = {};
    ASSERT_EQ(0, waitid(P_PIDFD, static_cast<id_t>(pidfds[0]), &info, WEXITED | WNOHANG));
    EXPECT_EQ(CLD_EXITED, info.si_code); EXPECT_EQ(0, info.si_status);
    EXPECT_EQ(0, aegis_context_stop(&runtime, 5000));
}

TEST_F(RuntimeMemoryGroup, ExactLimitsIdentityAndOneUseClaim) {
    ASSERT_EQ(0, make(0)) << strerror(errno);
    int directory = open_group();
    ASSERT_GE(directory, 0);
    EXPECT_EQ("1073741824\n", get(directory, "memory.max"));
    EXPECT_EQ("805306368\n", get(directory, "memory.high"));
    EXPECT_EQ("0\n", get(directory, "memory.swap.max"));
    EXPECT_EQ("1\n", get(directory, "memory.oom.group"));
    EXPECT_EQ("0\n", get(directory, "cgroup.max.depth"));
    EXPECT_EQ("0\n", get(directory, "cgroup.max.descendants"));
    EXPECT_EQ(-1, mkdirat(directory, "unexpected-child", 0700));
    EXPECT_EQ(EAGAIN, errno);
    close(directory);
    EXPECT_EQ(-1, aegis_memory_group_claim(groups[0], 11, 1234)); EXPECT_EQ(ESTALE, errno);
    EXPECT_EQ(-1, aegis_memory_group_claim(groups[0], 10, 1235)); EXPECT_EQ(ESTALE, errno);
    int borrowed = aegis_memory_group_claim(groups[0], 10, 1234);
    ASSERT_GE(borrowed, 0);
    EXPECT_NE(0, fcntl(borrowed, F_GETFD) & FD_CLOEXEC);
    EXPECT_EQ(-1, aegis_memory_group_claim(groups[0], 10, 1234)); EXPECT_EQ(EAGAIN, errno);
    ASSERT_EQ(0, aegis_memory_group_remove(&groups[0]));
    EXPECT_EQ(nullptr, groups[0]);
}

TEST_F(RuntimeMemoryGroup, DuplicateNameIsNeverAdoptedOrChanged) {
    ASSERT_EQ(0, make(0));
    ASSERT_EQ(-1, make(1)); EXPECT_EQ(EEXIST, errno);
    EXPECT_EQ(nullptr, groups[1]);
    EXPECT_GE(aegis_memory_group_claim(groups[0], 10, 1234), 0);
}

TEST_F(RuntimeMemoryGroup, TamperedLimitRejectsAdmissionButStillAllowsCleanup) {
    ASSERT_EQ(0, make(0));
    int directory = open_group();
    ASSERT_GE(directory, 0);
    int changed = put(directory, "memory.max", "max\n");
    close(directory);
    ASSERT_EQ(0, changed);
    EXPECT_EQ(-1, aegis_memory_group_claim(groups[0], 10, 1234)); EXPECT_EQ(EPROTO, errno);
    EXPECT_EQ(0, aegis_memory_group_kill_and_wait(groups[0], 5000));
    EXPECT_EQ(0, aegis_memory_group_remove(&groups[0]));
}

TEST_F(RuntimeMemoryGroup, MissingControllerRetainsCleanupOwnershipWithoutCloneTarget) {
    // Disable delegation only on our empty fixture before it has children.
    ASSERT_EQ(0, put(parent, "cgroup.subtree_control", "-memory\n"));
    ASSERT_EQ(-1, make(0));
    ASSERT_NE(nullptr, groups[0]);
    EXPECT_EQ(-1, aegis_memory_group_claim(groups[0], 10, 1234)); EXPECT_EQ(EAGAIN, errno);
    EXPECT_EQ(0, aegis_memory_group_remove(&groups[0]));
}

TEST_F(RuntimeMemoryGroup, EmptyStopPermanentlySealsAdmissionAndTimeoutInputsDoNot) {
    ASSERT_EQ(0, make(0));
    for (int timeout : {-1, 10001}) {
        EXPECT_EQ(-1, aegis_memory_group_kill_and_wait(groups[0], timeout)); EXPECT_EQ(EINVAL, errno);
    }
    ASSERT_EQ(0, aegis_memory_group_kill_and_wait(groups[0], 0));
    EXPECT_EQ(-1, aegis_memory_group_claim(groups[0], 10, 1234)); EXPECT_EQ(EAGAIN, errno);
}

TEST_F(RuntimeMemoryGroup, KillEmptyObservationIsSeparateFromChildReapingAndAnotherGroup) {
    ASSERT_EQ(0, make(0, 10)); ASSERT_EQ(0, make(1, 11));
    ASSERT_EQ(0, spawn(0, 10)); ASSERT_EQ(0, spawn(1, 11));
    EXPECT_EQ(-1, aegis_memory_group_remove(&groups[0])); EXPECT_EQ(EBUSY, errno);
    ASSERT_EQ(0, aegis_memory_group_kill_and_wait(groups[0], 5000));
    pollfd exited = {pidfds[0], POLLIN, 0};
    ASSERT_EQ(1, poll(&exited, 1, 5000));
    siginfo_t info = {};
    ASSERT_EQ(0, waitid(P_PIDFD, static_cast<id_t>(pidfds[0]), &info,
                        WEXITED | WNOHANG | WNOWAIT));
    EXPECT_GT(info.si_pid, 0); EXPECT_EQ(CLD_KILLED, info.si_code); EXPECT_EQ(SIGKILL, info.si_status);
    // Observing populated=0 did not reap. Explicit pidfd wait consumes exit.
    ASSERT_EQ(0, waitid(P_PIDFD, static_cast<id_t>(pidfds[0]), &info, WEXITED | WNOHANG));
    pollfd other = {pidfds[1], POLLIN, 0};
    EXPECT_EQ(0, poll(&other, 1, 0));
    ASSERT_EQ(0, aegis_memory_group_remove(&groups[0]));
}

TEST_F(RuntimeMemoryGroup, RawCloneCannotUseOrDestroyTheParentsGroupHandle) {
    ASSERT_EQ(0, make(0));
    clone_args args = {};
    args.flags = CLONE_PIDFD;
    args.pidfd = reinterpret_cast<uintptr_t>(&pidfds[0]);
    args.exit_signal = SIGCHLD;
    pid_t child = static_cast<pid_t>(syscall(SYS_clone3, &args, sizeof(args)));
    if (!child) {
        // Bionic's cached PID is inherited here; only a kernel PID check can
        // reject this child. Exercise only the immediate owner-error paths.
        bool denied = aegis_memory_group_claim(groups[0], 10, 1234) == -1 && errno == EPERM;
        denied = denied && aegis_memory_group_kill_and_wait(groups[0], 0) == -1 && errno == EPERM;
        denied = denied && aegis_memory_group_remove(&groups[0]) == -1 && errno == EPERM;
        syscall(SYS_exit_group, denied ? 0 : 92);
        __builtin_unreachable();
    }
    ASSERT_GT(child, 0);
    pollfd exited = {pidfds[0], POLLIN, 0};
    ASSERT_EQ(1, poll(&exited, 1, 5000));
    siginfo_t info = {};
    ASSERT_EQ(0, waitid(P_PIDFD, static_cast<id_t>(pidfds[0]), &info, WEXITED | WNOHANG));
    EXPECT_EQ(CLD_EXITED, info.si_code); EXPECT_EQ(0, info.si_status);
    EXPECT_GE(aegis_memory_group_claim(groups[0], 10, 1234), 0);
}

TEST_F(RuntimeMemoryGroup, InvalidInputsLeaveNoGroupAndNoNamespace) {
    EXPECT_EQ(-1, aegis_memory_group_create(parent, 9, 1, &groups[0])); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_memory_group_create(parent, 21473, 1, &groups[0])); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_memory_group_create(parent, 10, UINT32_MAX, &groups[0])); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_memory_group_create(parent, 10, 1, nullptr)); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_namespace_create_limited(10, 1234, -1, -1, nullptr, &context));
    EXPECT_EQ(EINVAL, errno); EXPECT_EQ(nullptr, context); EXPECT_EQ(nullptr, groups[0]);
    int wrong_fs = open("/data/local/tmp", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    ASSERT_GE(wrong_fs, 0);
    EXPECT_EQ(-1, aegis_memory_group_create(wrong_fs, 10, 1234, &groups[0])); EXPECT_EQ(EPERM, errno);
    close(wrong_fs);
    EXPECT_EQ(nullptr, groups[0]);
}

TEST_F(RuntimeMemoryGroup, NamespaceIsAlreadyBoundedWhileExecGateIsClosed) {
    ASSERT_EQ(0, make(0));
    open_probe(); ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1, aegis_namespace_create_limited(11, 1234, setup, peers[1], groups[0], &context));
    EXPECT_EQ(ESTALE, errno); ASSERT_EQ(nullptr, context);
    // Give the parent the exemption Android daemons may hold. The child must
    // lose it before helper execution; restore this disposable test process
    // even if an assertion fails. No other Android process is modified.
    struct OomGuard {
        int self = open("/proc/self", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        std::string original = self < 0 ? "" : get(self, "oom_score_adj");
        bool changed = false;
        ~OomGuard() {
            if (changed) EXPECT_EQ(0, put(self, "oom_score_adj", original.c_str()));
            if (self >= 0) close(self);
        }
    } protection;
    ASSERT_GE(protection.self, 0);
    ASSERT_FALSE(protection.original.empty()); ASSERT_NE('<', protection.original[0]);
    ASSERT_EQ(0, put(protection.self, "oom_score_adj", "-1000\n"));
    protection.changed = true;
    ASSERT_EQ(0, aegis_namespace_create_limited(10, 1234, setup, peers[1], groups[0], &context))
            << strerror(errno);
    int directory = open_group();
    ASSERT_GE(directory, 0);
    EXPECT_NE(std::string::npos, get(directory, "cgroup.events").find("populated 1\n"));
    close(directory);
    pollfd pending = {peers[0], POLLIN, 0};
    EXPECT_EQ(0, poll(&pending, 1, 0));
    ASSERT_EQ(0, aegis_namespace_resume(context)) << strerror(errno);
    ASSERT_EQ(1, poll(&pending, 1, 5000));
    aegis_namespace_probe report = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(report)),
              recv(peers[0], &report, sizeof(report), MSG_DONTWAIT | MSG_TRUNC));
    EXPECT_EQ(0x41454e53u, report.magic); EXPECT_EQ(10u, report.user_id);
    EXPECT_EQ(1234u, report.serial); EXPECT_EQ(0, report.oom_score_adj);
    ASSERT_EQ(0, aegis_memory_group_kill_and_wait(groups[0], 5000));
    aegis_child_exit result = {};
    ASSERT_EQ(0, aegis_namespace_wait(context, 5000, &result));
    EXPECT_EQ(CLD_KILLED, result.code); EXPECT_EQ(SIGKILL, result.status);
}

}  // namespace
