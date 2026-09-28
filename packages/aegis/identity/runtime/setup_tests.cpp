// Compile on aegis-build; execute ONLY in local Android QEMU. These tests do
// not substitute for a real CE/base mount, SELinux transition or Linux session.
#include "setup.h"
#include "sandbox.h"

#include <gtest/gtest.h>
#include <array>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
int descriptors() {
    DIR* directory = opendir("/proc/self/fd");
    if (!directory) return -1;
    int count = 0;
    while (dirent* entry = readdir(directory)) if (entry->d_name[0] != '.') count++;
    closedir(directory);
    return count;
}

class RuntimeSetup : public ::testing::Test {
 protected:
    int sockets[2] = {-1, -1};
    int sources[AEGIS_SETUP_FDS] = {-1, -1, -1, -1};
    int output[AEGIS_SETUP_FDS] = {-1, -1, -1, -1};
    void SetUp() override {
        ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets));
        for (unsigned i = 0; i < AEGIS_SETUP_FDS; i++) {
            int pipe_fds[2];
            ASSERT_EQ(0, pipe2(pipe_fds, O_CLOEXEC));
            sources[i] = pipe_fds[0];
            char value = static_cast<char>('A' + i);
            EXPECT_EQ(1, write(pipe_fds[1], &value, 1));
            close(pipe_fds[1]);
        }
    }
    void TearDown() override {
        for (int fd : sockets) if (fd >= 0) close(fd);
        for (int fd : sources) if (fd >= 0) close(fd);
        for (int fd : output) if (fd >= 0) close(fd);
    }
    void unchanged() { for (int fd : output) EXPECT_EQ(-1, fd); }

    static aegis_setup_record record(unsigned role) {
        aegis_setup_record value = {};
        value.magic = AEGIS_SETUP_MAGIC; value.version = AEGIS_SETUP_VERSION;
        value.role = role; value.user_id = 10; value.serial = 1234;
        return value;
    }
    ssize_t raw(aegis_setup_record value, int fd, size_t count = 1,
                size_t bytes = sizeof(aegis_setup_record)) {
        alignas(cmsghdr) std::array<char, CMSG_SPACE(2 * sizeof(int))> ancillary = {};
        iovec io = {&value, bytes};
        msghdr message = {};
        message.msg_iov = &io; message.msg_iovlen = 1;
        if (fd >= 0) {
            message.msg_control = ancillary.data();
            message.msg_controllen = CMSG_SPACE(count * sizeof(int));
            cmsghdr* header = CMSG_FIRSTHDR(&message);
            header->cmsg_level = SOL_SOCKET; header->cmsg_type = SCM_RIGHTS;
            header->cmsg_len = CMSG_LEN(count * sizeof(int));
            for (size_t i = 0; i < count; i++) memcpy(CMSG_DATA(header) + i * sizeof(int), &fd, sizeof(fd));
        }
        return sendmsg(sockets[0], &message, MSG_NOSIGNAL);
    }
};

TEST_F(RuntimeSetup, TransfersOnlyCommittedOrderedReferencesAndRetainsSenderOwnership) {
    int before = descriptors();
    ASSERT_EQ(0, aegis_send_setup(sockets[0], 10, 1234, sources));
    ASSERT_EQ(0, aegis_receive_setup(sockets[1], 10, 1234, 5000, output));
    EXPECT_EQ(before + 4, descriptors());
    for (unsigned i = 0; i < AEGIS_SETUP_FDS; i++) {
        EXPECT_GE(output[i], 4);
        EXPECT_NE(0, fcntl(output[i], F_GETFD) & FD_CLOEXEC);
        EXPECT_GE(fcntl(sources[i], F_GETFD), 0);
        char value = 0;
        EXPECT_EQ(1, read(output[i], &value, 1));
        EXPECT_EQ(static_cast<char>('A' + i), value);
        close(output[i]); output[i] = -1;
    }
    EXPECT_EQ(before, descriptors());
}

TEST_F(RuntimeSetup, WrongIdentityAndSerialCannotExposeDescriptors) {
    for (int change = 0; change < 2; change++) {
        // Send only one record, ensuring no queued remainder between attempts.
        auto value = record(AEGIS_SETUP_BASE);
        if (change == 0) value.user_id = 11;
        else value.serial++;
        ASSERT_EQ(static_cast<ssize_t>(sizeof(value)), raw(value, sources[0]));
        int before = descriptors();
        EXPECT_EQ(-1, aegis_receive_setup(sockets[1], 10, 1234, 5000, output));
        EXPECT_EQ(EPROTO, errno);
        unchanged(); EXPECT_EQ(before, descriptors());
    }
}

TEST_F(RuntimeSetup, MalformedRecordsAndExtraDescriptorsAreClosed) {
    for (int change = 0; change < 7; change++) {
        auto value = record(AEGIS_SETUP_BASE);
        if (change == 0) value.magic++;
        if (change == 1) value.version++;
        if (change == 2) value.role = AEGIS_SETUP_HOME;
        if (change == 3) value.reserved[0] = 1;
        size_t bytes = change == 4 ? sizeof(value) - 1 : sizeof(value);
        int fd = change == 5 ? -1 : sources[0];
        size_t count = change == 6 ? 2 : 1;
        ASSERT_EQ(static_cast<ssize_t>(bytes), raw(value, fd, count, bytes));
        int before = descriptors();
        EXPECT_EQ(-1, aegis_receive_setup(sockets[1], 10, 1234, 5000, output));
        EXPECT_EQ(EPROTO, errno);
        unchanged(); EXPECT_EQ(before, descriptors());
    }
}

TEST_F(RuntimeSetup, PartialTransferTimesOutAndClosesAlreadyReceivedReferences) {
    ASSERT_EQ(static_cast<ssize_t>(sizeof(aegis_setup_record)),
              raw(record(AEGIS_SETUP_BASE), sources[0]));
    int before = descriptors();
    EXPECT_EQ(-1, aegis_receive_setup(sockets[1], 10, 1234, 30, output));
    EXPECT_EQ(ETIMEDOUT, errno);
    unchanged(); EXPECT_EQ(before, descriptors());
}

TEST_F(RuntimeSetup, RepeatedRoleOrDescriptorBearingCommitCancelsWholeTransfer) {
    for (int change = 0; change < 2; change++) {
        unsigned sent = change == 0 ? 1 : AEGIS_SETUP_FDS;
        for (unsigned i = 0; i < sent; i++) {
            ASSERT_EQ(static_cast<ssize_t>(sizeof(aegis_setup_record)), raw(record(i + 1), sources[i]));
        }
        unsigned wrong_role = change == 0 ? AEGIS_SETUP_BASE : AEGIS_SETUP_COMMIT;
        ASSERT_EQ(static_cast<ssize_t>(sizeof(aegis_setup_record)), raw(record(wrong_role), sources[0]));
        int before = descriptors();
        EXPECT_EQ(-1, aegis_receive_setup(sockets[1], 10, 1234, 5000, output));
        EXPECT_EQ(EPROTO, errno);
        unchanged(); EXPECT_EQ(before, descriptors());
    }
}

TEST_F(RuntimeSetup, DeadPeerCannotCommitQueuedDescriptors) {
    ASSERT_EQ(0, aegis_send_setup(sockets[0], 10, 1234, sources));
    close(sockets[0]); sockets[0] = -1;
    int before = descriptors();
    EXPECT_EQ(-1, aegis_receive_setup(sockets[1], 10, 1234, 5000, output));
    EXPECT_EQ(EPIPE, errno);
    unchanged(); EXPECT_EQ(before, descriptors());
    // Closing the failed channel also drops any unconsumed references queued
    // inside the socket. A failed context/channel must never be reused.
    close(sockets[1]); sockets[1] = -1;
}

TEST_F(RuntimeSetup, BadArgumentsAndWrongSocketTypeDoNotConsumeOrPublishRecords) {
    EXPECT_EQ(-1, aegis_send_setup(sockets[0], 0, 1234, sources)); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_send_setup(sockets[0], 10, UINT32_MAX, sources)); EXPECT_EQ(EINVAL, errno);
    int invalid[AEGIS_SETUP_FDS] = {sources[0], sources[1], -1, sources[3]};
    EXPECT_EQ(-1, aegis_send_setup(sockets[0], 10, 1234, invalid)); EXPECT_EQ(EBADF, errno);
    char byte;
    EXPECT_EQ(-1, recv(sockets[1], &byte, 1, MSG_DONTWAIT)); EXPECT_EQ(EAGAIN, errno);
    EXPECT_EQ(-1, aegis_receive_setup(sockets[1], 10, 1234, 0, output)); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_receive_setup(sockets[1], 10, 1234, 5001, output)); EXPECT_EQ(EINVAL, errno);
    int occupied[AEGIS_SETUP_FDS] = {sources[0], -1, -1, -1};
    EXPECT_EQ(-1, aegis_receive_setup(sockets[1], 10, 1234, 50, occupied)); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(sources[0], occupied[0]); EXPECT_GE(fcntl(sources[0], F_GETFD), 0);
    int stream[2];
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, stream));
    EXPECT_EQ(-1, aegis_send_setup(stream[0], 10, 1234, sources)); EXPECT_EQ(EPROTOTYPE, errno);
    EXPECT_EQ(-1, aegis_receive_setup(stream[1], 10, 1234, 50, output)); EXPECT_EQ(EPROTOTYPE, errno);
    close(stream[0]); close(stream[1]);
    unchanged();
}

TEST(RuntimeSetupEntry, RejectsDirectAndroidHostInvocation) {
    EXPECT_EQ(-1, aegis_check_setup_context(10)); EXPECT_EQ(EPERM, errno);
    char path[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    ASSERT_GT(n, 0); ASSERT_LT(n, static_cast<ssize_t>(sizeof(path) - 1));
    path[n] = '\0';
    std::string executable(path);
    executable.resize(executable.find_last_of('/') + 1);
    executable += "aegis-runtime-setup";
    pid_t child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        execl(executable.c_str(), "aegis-runtime-setup", "10", "1234", nullptr);
        _exit(127);
    }
    int status;
    ASSERT_EQ(child, waitpid(child, &status, 0));
    ASSERT_TRUE(WIFEXITED(status)); EXPECT_EQ(78, WEXITSTATUS(status));
}
}  // namespace
