// Compile on aegis-build; execute only in local Android QEMU. The peer is a
// bounded protocol fixture, not a personal Linux supervisor or AOSP authority.
#include "exec.h"
#include "control.h"
#include <gtest/gtest.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <grp.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace {
uint64_t deadline(unsigned millis = 2000) {
    timespec now = {};
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return 0;
    return static_cast<uint64_t>(now.tv_sec) * 1000000000 + now.tv_nsec
            + static_cast<uint64_t>(millis) * 1000000;
}
int descriptors() {
    DIR* directory = opendir("/proc/self/fd");
    if (!directory) return -1;
    int count = 0;
    while (dirent* entry = readdir(directory)) if (entry->d_name[0] != '.') ++count;
    closedir(directory);
    return count;
}
bool request(int channel, uint64_t id) {
    pollfd ready = {channel, POLLIN, 0};
    if (poll(&ready, 1, 3000) != 1) return false;
    char bytes[AEGIS_RUNTIME_MAX_PACKET];
    int received = -1;
    ssize_t size = aegis_receive(channel, bytes, sizeof(bytes), &received);
    if (received >= 0) { close(received); return false; }
    if (size <= 0) return false;
    aegis_runtime_request header;
    char* argv[AEGIS_RUNTIME_MAX_ARGS + 1];
    return !aegis_parse_request(bytes, size, id - 1, &header, argv)
            && header.operation == AEGIS_EXEC && header.request_id == id
            && header.argc == 2 && !strcmp(argv[0], "/bin/bash") && !strcmp(argv[1], "--noprofile");
}
int pty(uint32_t user = 10) {
    int master = posix_openpt(O_RDWR | O_CLOEXEC | O_NOCTTY);
    if (master < 0) return -1;
    if (grantpt(master) < 0 || unlockpt(master) < 0) { close(master); return -1; }
    int slave = ioctl(master, TIOCGPTPEER, O_RDWR | O_CLOEXEC | O_NOCTTY);
    if (slave < 0) { close(master); return -1; }
    int result = fchown(slave, user * 100000 + 7500, user * 100000 + 7500);
    close(slave);
    if (result < 0) { close(master); return -1; }
    return master;
}
bool started(int channel, uint64_t id, int pid, uint32_t user = 10) {
    int master = pty(user);
    if (master < 0) return false;
    int result = aegis_send_reply(channel, AEGIS_STARTED, id, 0, pid, 0, master);
    close(master);
    return result == 0;
}
class RuntimeExec : public ::testing::Test {
 protected:
    aegis_exec* owner = nullptr;
    int peers[2] = {-1, -1};
    int master = -1;
    int fixture_done = -1;
    pid_t child = -1;
    const char* argv[2] = {"/bin/bash", "--noprofile"};

    void SetUp() override {
        ASSERT_EQ(0u, getuid()); ASSERT_EQ(0u, getgid());
        ASSERT_EQ(0, setgroups(0, nullptr));
        ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, peers));
        ASSERT_EQ(0, aegis_exec_create(peers[0], 10, &owner));
        ASSERT_GE(fcntl(peers[0], F_GETFD), 0);  // Caller ownership is retained.
        close(peers[0]); peers[0] = -1;
    }
    void TearDown() override {
        if (master >= 0) close(master);
        if (owner) EXPECT_EQ(0, aegis_exec_close(&owner));
        for (int peer : peers) if (peer >= 0) close(peer);
        if (child > 0) {
            int status = 0;
            pid_t result = 0;
            for (unsigned i = 0; i < 300 && result == 0; ++i) {
                result = waitpid(child, &status, WNOHANG);
                if (!result) usleep(10000);
            }
            if (!result) { kill(child, SIGKILL); waitpid(child, &status, 0); }
            EXPECT_EQ(child, result) << "Fixture did not exit after channel close";
            EXPECT_TRUE(WIFEXITED(status)); EXPECT_EQ(0, WEXITSTATUS(status));
        }
        if (fixture_done >= 0) close(fixture_done);
    }
    void peer(std::function<bool(int)> action) {
        int completion[2];
        ASSERT_EQ(0, pipe2(completion, O_CLOEXEC));
        fixture_done = completion[0];
        child = fork();
        if (child < 0) { close(completion[1]); FAIL() << "fork failed"; }
        if (!child) {
            // Keep only the fixture endpoint. Inherited copies of the engine's
            // socket would otherwise conceal peer loss or keep a poisoned fd alive.
            int channel = fcntl(peers[1], F_DUPFD_CLOEXEC, 10);
            int done = fcntl(completion[1], F_DUPFD_CLOEXEC, 10);
            if (channel < 0 || done < 0 || dup3(channel, 3, O_CLOEXEC) < 0
                    || dup3(done, 4, O_CLOEXEC) < 0) _exit(90);
            if (syscall(SYS_close_range, 5u, ~0u, 0) < 0) _exit(91);
            if (!action(3)) _exit(92);
            if (write(4, "1", 1) != 1) _exit(94);
            close(4);
            pollfd wait = {3, POLLIN | POLLRDHUP, 0};
            // Keep a healthy fixture alive until the owner closes it; do not
            // conflate a valid queued reply with supervisor death.
            if (poll(&wait, 1, 5000) < 1) _exit(93);
            _exit(0);
        }
        close(completion[1]);
        close(peers[1]); peers[1] = -1;
    }
    void finished() {
        pollfd ready = {fixture_done, POLLIN, 0};
        ASSERT_EQ(1, poll(&ready, 1, 3000));
        char value = 0;
        ASSERT_EQ(1, read(fixture_done, &value, 1));
        ASSERT_EQ('1', value);
    }
    int start(uint64_t* id, unsigned millis = 2000) {
        return aegis_exec_start(owner, 2, argv, deadline(millis), id, &master);
    }
    int result(uint64_t id, int* status) {
        for (unsigned i = 0; i < 100; ++i) {
            int rc = aegis_exec_result(owner, id, status);
            if (!rc || errno != EAGAIN) return rc;
            usleep(10000);
        }
        errno = ETIMEDOUT; return -1;
    }
};

TEST_F(RuntimeExec, TransfersOwnedPtyAndRetainsOtherCommandsExitResults) {
    peer([](int channel) {
        return request(channel, 1) && started(channel, 1, 41)
                && request(channel, 2)
                && !aegis_send_reply(channel, AEGIS_EXITED, 1, 0, 41, 3 << 8, -1)
                && started(channel, 2, 42)
                && !aegis_send_reply(channel, AEGIS_EXITED, 2, 0, 42, 0, -1);
    });
    ASSERT_FALSE(HasFatalFailure());
    uint64_t first = 0, second = 0;
    ASSERT_EQ(0, start(&first)) << strerror(errno);
    EXPECT_EQ(1u, first); ASSERT_GE(master, 0);
    EXPECT_NE(0, fcntl(master, F_GETFD) & FD_CLOEXEC);
    int status = -1;
    EXPECT_EQ(-1, aegis_exec_result(owner, first, &status)); EXPECT_EQ(EAGAIN, errno);
    EXPECT_EQ(-1, status);
    close(master); master = -1;
    ASSERT_EQ(0, start(&second)) << strerror(errno); EXPECT_EQ(2u, second);
    finished(); ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0, result(second, &status)); EXPECT_EQ(0, status);
    ASSERT_EQ(0, result(first, &status)); EXPECT_EQ(3 << 8, status);
    EXPECT_EQ(-1, aegis_exec_result(owner, first, &status)); EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(0, aegis_exec_healthy(owner));
}

TEST_F(RuntimeExec, AcknowledgedProgramErrorDoesNotPoisonTheControlChannel) {
    peer([](int channel) {
        return request(channel, 1) && !aegis_send_reply(channel, AEGIS_ERROR, 1, ENOENT, 0, 0, -1)
                && request(channel, 2) && !aegis_send_reply(channel, AEGIS_ERROR, 2, EACCES, 0, 0, -1);
    });
    ASSERT_FALSE(HasFatalFailure());
    uint64_t id = 0;
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(0u, id); EXPECT_EQ(-1, master); EXPECT_EQ(0, aegis_exec_healthy(owner));
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(EACCES, errno);
    EXPECT_EQ(0u, id); EXPECT_EQ(-1, master); EXPECT_EQ(0, aegis_exec_healthy(owner));
}

TEST_F(RuntimeExec, WrongUserPtyIsClosedAndCannotBePublished) {
    peer([](int channel) { return request(channel, 1) && started(channel, 1, 41, 11); });
    ASSERT_FALSE(HasFatalFailure());
    int before = descriptors();
    uint64_t id = 0;
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(EPROTO, errno);
    EXPECT_EQ(0u, id); EXPECT_EQ(-1, master);
    EXPECT_EQ(before - 1, descriptors());  // The poisoned engine fd alone was closed.
    EXPECT_EQ(-1, aegis_exec_healthy(owner)); EXPECT_EQ(EPIPE, errno);
}

TEST_F(RuntimeExec, OrdinaryDeviceDescriptorIsRejectedWithoutLeak) {
    peer([](int channel) {
        int fd = open("/dev/null", O_RDWR | O_CLOEXEC);
        if (fd < 0) return false;
        bool ok = request(channel, 1) && !aegis_send_reply(channel, AEGIS_STARTED, 1, 0, 41, 0, fd);
        close(fd); return ok;
    });
    ASSERT_FALSE(HasFatalFailure());
    int before = descriptors();
    uint64_t id = 0;
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(EPROTO, errno);
    EXPECT_EQ(0u, id); EXPECT_EQ(-1, master); EXPECT_EQ(before - 1, descriptors());
}

TEST_F(RuntimeExec, WrongRequestIdentitySealsTheChannel) {
    peer([](int channel) {
        return request(channel, 1) && !aegis_send_reply(channel, AEGIS_ERROR, 2, ENOENT, 0, 0, -1);
    });
    ASSERT_FALSE(HasFatalFailure());
    uint64_t id = 0;
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(EPROTO, errno);
    EXPECT_EQ(-1, aegis_exec_healthy(owner)); EXPECT_EQ(EPIPE, errno);
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(EPIPE, errno);
}

TEST_F(RuntimeExec, MissingPtyCannotClaimSuccessfulStart) {
    peer([](int channel) {
        return request(channel, 1) && !aegis_send_reply(channel, AEGIS_STARTED, 1, 0, 41, 0, -1);
    });
    ASSERT_FALSE(HasFatalFailure());
    uint64_t id = 0;
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(EPROTO, errno);
    EXPECT_EQ(0u, id); EXPECT_EQ(-1, master);
}

TEST_F(RuntimeExec, UncertainStartTimeoutCannotBeRetriedOnTheChannel) {
    peer([](int channel) { return request(channel, 1); });
    ASSERT_FALSE(HasFatalFailure());
    uint64_t id = 0;
    EXPECT_EQ(-1, start(&id, 30)); EXPECT_EQ(ETIMEDOUT, errno);
    EXPECT_EQ(0u, id); EXPECT_EQ(-1, master);
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(EPIPE, errno);
}

TEST_F(RuntimeExec, InvalidRequestsHaveNoWireEffectsAndDoNotConsumeSequence) {
    uint64_t id = 0;
    const char* relative[] = {"bash"};
    EXPECT_EQ(-1, aegis_exec_start(owner, 1, relative, deadline(), &id, &master)); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_exec_start(owner, 2, argv, 0, &id, &master)); EXPECT_EQ(ETIMEDOUT, errno);
    EXPECT_EQ(-1, aegis_exec_start(owner, 2, argv, deadline(20000), &id, &master)); EXPECT_EQ(EINVAL, errno);
    std::string huge(AEGIS_RUNTIME_MAX_PACKET, 'a');
    const char* oversized[] = {"/bin/bash", huge.c_str()};
    EXPECT_EQ(-1, aegis_exec_start(owner, 2, oversized, deadline(), &id, &master)); EXPECT_EQ(E2BIG, errno);
    EXPECT_EQ(0u, id); EXPECT_EQ(-1, master);
    pollfd ready = {peers[1], POLLIN, 0}; EXPECT_EQ(0, poll(&ready, 1, 0));
    EXPECT_EQ(0, aegis_exec_healthy(owner));
    peer([](int channel) {
        return request(channel, 1) && !aegis_send_reply(channel, AEGIS_ERROR, 1, ENOENT, 0, 0, -1);
    });
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(ENOENT, errno);
}

TEST_F(RuntimeExec, DuplicateExitCannotBeConsumedAsAnotherResult) {
    peer([](int channel) {
        return request(channel, 1) && started(channel, 1, 41)
                && !aegis_send_reply(channel, AEGIS_EXITED, 1, 0, 41, 0, -1)
                && !aegis_send_reply(channel, AEGIS_EXITED, 1, 0, 41, 0, -1);
    });
    ASSERT_FALSE(HasFatalFailure());
    uint64_t id = 0;
    ASSERT_EQ(0, start(&id)) << strerror(errno);
    finished(); ASSERT_FALSE(HasFatalFailure());
    int status = -1;
    EXPECT_EQ(-1, aegis_exec_result(owner, id, &status)); EXPECT_EQ(EPROTO, errno);
    EXPECT_EQ(-1, status); EXPECT_EQ(-1, aegis_exec_healthy(owner));
}

TEST_F(RuntimeExec, ActiveProcessNumberCannotBeReassignedToAnotherCommand) {
    peer([](int channel) {
        return request(channel, 1) && started(channel, 1, 41)
                && request(channel, 2) && started(channel, 2, 41);
    });
    ASSERT_FALSE(HasFatalFailure());
    uint64_t id = 0;
    ASSERT_EQ(0, start(&id));
    close(master); master = -1; id = 0;
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(EPROTO, errno);
    EXPECT_EQ(0u, id); EXPECT_EQ(-1, master);
}

TEST_F(RuntimeExec, CompletedResultsKeepTheirSlotsUntilCollected) {
    peer([](int channel) {
        for (unsigned i = 1; i <= AEGIS_RUNTIME_MAX_SHELLS; ++i) {
            if (!request(channel, i) || !started(channel, i, 100 + i)
                    || aegis_send_reply(channel, AEGIS_EXITED, i, 0, 100 + i, i << 8, -1)) return false;
        }
        return request(channel, AEGIS_RUNTIME_MAX_SHELLS + 1)
                && !aegis_send_reply(channel, AEGIS_ERROR, AEGIS_RUNTIME_MAX_SHELLS + 1, ENOENT, 0, 0, -1);
    });
    ASSERT_FALSE(HasFatalFailure());
    uint64_t id = 0;
    for (unsigned i = 1; i <= AEGIS_RUNTIME_MAX_SHELLS; ++i) {
        id = 0;
        ASSERT_EQ(0, start(&id)) << strerror(errno); EXPECT_EQ(i, id);
        close(master); master = -1;
    }
    id = 0;
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(EBUSY, errno); EXPECT_EQ(0u, id);
    int status = -1;
    ASSERT_EQ(0, result(1, &status)); EXPECT_EQ(1 << 8, status);
    EXPECT_EQ(-1, start(&id)); EXPECT_EQ(ENOENT, errno);
    ASSERT_EQ(0, result(AEGIS_RUNTIME_MAX_SHELLS, &status));
    EXPECT_EQ(static_cast<int>(AEGIS_RUNTIME_MAX_SHELLS << 8), status);
}

TEST_F(RuntimeExec, ForkedProcessCannotClaimOrCloseTheOriginalOwner) {
    pid_t observer = fork();
    ASSERT_GE(observer, 0);
    if (!observer) {
        bool denied = aegis_exec_healthy(owner) == -1 && errno == EPERM;
        denied &= aegis_exec_close(&owner) == -1 && errno == EPERM && owner != nullptr;
        _exit(denied ? 0 : 95);
    }
    int status;
    ASSERT_EQ(observer, waitpid(observer, &status, 0));
    ASSERT_TRUE(WIFEXITED(status)); EXPECT_EQ(0, WEXITSTATUS(status));
    EXPECT_EQ(0, aegis_exec_healthy(owner));
}
}  // namespace
