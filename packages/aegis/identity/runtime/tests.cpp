// Android ARM64 guest tests. Never run on the builder or Linux host CI.
#include "control.h"
#include "sandbox.h"

#include <gtest/gtest.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <linux/sched.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#include <string>
#include <vector>

namespace {
using Packet = std::vector<char>;

static void check_resource_name(const char* name, uid_t app_id) {
    SCOPED_TRACE(name);
    const passwd* user = getpwnam(name);
    ASSERT_NE(nullptr, user);
    EXPECT_STREQ(name, user->pw_name);
    EXPECT_EQ(app_id, user->pw_uid);
    EXPECT_EQ(app_id, user->pw_gid);
    const passwd* reverse_user = getpwuid(app_id);
    ASSERT_NE(nullptr, reverse_user);
    EXPECT_STREQ(name, reverse_user->pw_name);
    const group* resource_group = getgrnam(name);
    ASSERT_NE(nullptr, resource_group);
    EXPECT_STREQ(name, resource_group->gr_name);
    EXPECT_EQ(app_id, resource_group->gr_gid);
    const group* reverse_group = getgrgid(app_id);
    ASSERT_NE(nullptr, reverse_group);
    EXPECT_STREQ(name, reverse_group->gr_name);
}

TEST(RuntimeRegistry, InstalledNamesAndIdsRoundTripThroughBionic) {
    // Inspect the installed partition registries, not generated source text.
    // Canonical reverse names distinguish real entries from OEM fallbacks.
    for (unsigned inside = 0; inside < 1000; inside++) {
        char name[64];
        snprintf(name, sizeof(name), "vendor_aegis_linux_%04u", inside);
        check_resource_name(name, 5000 + inside);
    }
    check_resource_name("system_ext_aegis_runtime_user", 7500);
    check_resource_name("system_ext_aegis_runtime_nobody", 7501);
}

Packet request(uint16_t operation, uint64_t id, const std::vector<std::string>& args) {
    aegis_runtime_request header = {};
    header.magic = AEGIS_RUNTIME_MAGIC;
    header.version = AEGIS_RUNTIME_VERSION;
    header.operation = operation;
    header.request_id = id;
    header.argc = args.size();
    for (const auto& arg : args) header.payload_bytes += arg.size() + 1;
    Packet packet(sizeof(header));
    memcpy(packet.data(), &header, sizeof(header));
    for (const auto& arg : args) packet.insert(packet.end(), arg.c_str(), arg.c_str() + arg.size() + 1);
    return packet;
}

int parse(Packet packet, uint64_t previous = 0) {
    aegis_runtime_request header;
    char* argv[AEGIS_RUNTIME_MAX_ARGS + 1];
    return aegis_parse_request(packet.data(), packet.size(), previous, &header, argv);
}

TEST(RuntimeRequest, PreservesArgumentBoundariesAndEmptyArguments) {
    Packet packet = request(AEGIS_EXEC, 1, {"/bin/bash", "-c", "printf '%s' 'a b'", ""});
    aegis_runtime_request header;
    char* argv[AEGIS_RUNTIME_MAX_ARGS + 1];
    ASSERT_EQ(0, aegis_parse_request(packet.data(), packet.size(), 0, &header, argv));
    EXPECT_EQ(4u, header.argc);
    EXPECT_STREQ("/bin/bash", argv[0]);
    EXPECT_STREQ("printf '%s' 'a b'", argv[2]);
    EXPECT_STREQ("", argv[3]);
    EXPECT_EQ(nullptr, argv[4]);
}

TEST(RuntimeRequest, RejectsZeroAndReplayedIds) {
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_EXEC, 0, {"/bin/bash"})));
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_EXEC, 7, {"/bin/bash"}), 7));
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_EXEC, 6, {"/bin/bash"}), 7));
    EXPECT_EQ(0, parse(request(AEGIS_EXEC, 8, {"/bin/bash"}), 7));
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_STOP, UINT64_MAX, {}), UINT64_MAX));
}

TEST(RuntimeRequest, RequiresExactBoundedFrame) {
    Packet original = request(AEGIS_EXEC, 1, {"/bin/bash", "-l"});
    for (size_t size = 0; size < original.size(); size++) {
        Packet truncated(original.begin(), original.begin() + size);
        EXPECT_EQ(-EPROTO, parse(truncated)) << size;
    }
    Packet extra = original;
    extra.push_back(0);
    EXPECT_EQ(-EPROTO, parse(extra));
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_EXEC, 1, {"/bin/bash", std::string(8192, 'a')})));
    // Include the trailing bytes in payload_bytes: argc still forbids them.
    aegis_runtime_request header;
    memcpy(&header, extra.data(), sizeof(header));
    header.payload_bytes++;
    memcpy(extra.data(), &header, sizeof(header));
    EXPECT_EQ(-EPROTO, parse(extra));
    original.back() = 'x';
    EXPECT_EQ(-EPROTO, parse(original));
}

TEST(RuntimeRequest, RejectsWrongProtocolOperationAndExecutable) {
    for (int field = 0; field < 4; field++) {
        Packet packet = request(AEGIS_EXEC, 1, {"/bin/bash"});
        aegis_runtime_request header;
        memcpy(&header, packet.data(), sizeof(header));
        if (field == 0) header.magic ^= 1;
        if (field == 1) header.version++;
        if (field == 2) header.operation = 99;
        if (field == 3) header.argc++;
        memcpy(packet.data(), &header, sizeof(header));
        EXPECT_EQ(-EPROTO, parse(packet));
    }
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_EXEC, 1, {})));
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_EXEC, 1, {""})));
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_EXEC, 1, {"bash"})));
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_EXEC, 1, {"./bin/bash"})));
    std::vector<std::string> args(AEGIS_RUNTIME_MAX_ARGS, "");
    args[0] = "/bin/bash";
    EXPECT_EQ(0, parse(request(AEGIS_EXEC, 1, args)));
    args.emplace_back("");
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_EXEC, 1, args)));
}

TEST(RuntimeRequest, StopCannotCarryArguments) {
    EXPECT_EQ(0, parse(request(AEGIS_STOP, 1, {})));
    EXPECT_EQ(-EPROTO, parse(request(AEGIS_STOP, 1, {"/bin/bash"})));
}

class RuntimeChannel : public ::testing::Test {
 protected:
    int sockets[2] = {-1, -1};
    void SetUp() override {
        ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets));
    }
    void TearDown() override { for (int fd : sockets) if (fd >= 0) close(fd); }
};

TEST_F(RuntimeChannel, RepliesTransferOneUsableCloseOnExecDescriptor) {
    int pipe_fds[2];
    ASSERT_EQ(0, pipe2(pipe_fds, O_CLOEXEC));
    ASSERT_EQ(0, aegis_send_reply(sockets[0], AEGIS_STARTED, 7, 0, 42, 0, pipe_fds[0]));
    close(pipe_fds[0]);
    aegis_runtime_reply reply;
    int received;
    ASSERT_EQ(static_cast<ssize_t>(sizeof(reply)),
              aegis_receive(sockets[1], &reply, sizeof(reply), &received));
    ASSERT_GE(received, 0);
    EXPECT_EQ(FD_CLOEXEC, fcntl(received, F_GETFD) & FD_CLOEXEC);
    EXPECT_EQ(AEGIS_STARTED, reply.event);
    EXPECT_EQ(7u, reply.request_id);
    EXPECT_EQ(42, reply.pid);
    EXPECT_EQ(0u, reply.reserved);
    EXPECT_EQ(1, write(pipe_fds[1], "x", 1));
    char value = 0;
    EXPECT_EQ(1, read(received, &value, 1));
    EXPECT_EQ('x', value);
    close(received);
    close(pipe_fds[1]);
}

static int descriptors() {
    DIR* directory = opendir("/proc/self/fd");
    if (!directory) return -1;
    int count = 0;
    while (dirent* entry = readdir(directory)) if (entry->d_name[0] != '.') count++;
    closedir(directory);
    return count;
}

static ssize_t send_descriptors(int socket, int fd, size_t count) {
    alignas(cmsghdr) std::array<char, CMSG_SPACE(12 * sizeof(int))> control = {};
    char payload = 'x';
    iovec io = {&payload, 1};
    msghdr message = {};
    message.msg_iov = &io;
    message.msg_iovlen = 1;
    message.msg_control = control.data();
    message.msg_controllen = CMSG_SPACE(count * sizeof(int));
    cmsghdr* c = CMSG_FIRSTHDR(&message);
    c->cmsg_level = SOL_SOCKET;
    c->cmsg_type = SCM_RIGHTS;
    c->cmsg_len = CMSG_LEN(count * sizeof(int));
    for (size_t i = 0; i < count; i++) memcpy(CMSG_DATA(c) + i * sizeof(int), &fd, sizeof(fd));
    return sendmsg(socket, &message, MSG_NOSIGNAL | MSG_DONTWAIT);
}

TEST_F(RuntimeChannel, ExtraAndTruncatedRightsNeverLeakDescriptors) {
    int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(fd, 0);
    int before = descriptors();
    ASSERT_GE(before, 0);
    for (size_t count : {2u, 8u, 12u}) {
        for (int attempt = 0; attempt < 64; attempt++) {
            ASSERT_EQ(1, send_descriptors(sockets[0], fd, count));
            char payload;
            int received;
            EXPECT_EQ(-1, aegis_receive(sockets[1], &payload, 1, &received));
            EXPECT_EQ(EPROTO, errno);
            EXPECT_EQ(-1, received);
        }
        EXPECT_EQ(before, descriptors());
    }
    close(fd);
}

TEST_F(RuntimeChannel, TruncatedPayloadClosesTransferredDescriptor) {
    int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(fd, 0);
    int before = descriptors();
    ASSERT_GE(before, 0);
    ASSERT_EQ(0, aegis_send_reply(sockets[0], AEGIS_STARTED, 1, 0, 2, 0, fd));
    char payload;
    int received;
    EXPECT_EQ(-1, aegis_receive(sockets[1], &payload, 1, &received));
    EXPECT_EQ(EPROTO, errno);
    EXPECT_EQ(-1, received);
    EXPECT_EQ(before, descriptors());
    close(fd);
}

TEST_F(RuntimeChannel, QueueBackpressureAndPeerLossDoNotBlockOrRaiseSigpipe) {
    int size = 4096;
    ASSERT_EQ(0, setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)));
    int sent = 0;
    while (sent < 10000 && aegis_send_reply(sockets[0], AEGIS_EXITED, 1, 0, 2, 0, -1) == 0) sent++;
    ASSERT_LT(sent, 10000);
    EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
    close(sockets[1]);
    sockets[1] = -1;
    EXPECT_EQ(-1, aegis_send_reply(sockets[0], AEGIS_EXITED, 1, 0, 2, 0, -1));
    EXPECT_TRUE(errno == EPIPE || errno == ECONNRESET);
}

TEST_F(RuntimeChannel, EmptyChannelIsNonblockingAndClosedChannelIsEof) {
    char packet[32];
    int received;
    EXPECT_EQ(-1, aegis_receive(sockets[1], packet, sizeof(packet), &received));
    EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
    EXPECT_EQ(-1, received);
    close(sockets[0]);
    sockets[0] = -1;
    EXPECT_EQ(0, aegis_receive(sockets[1], packet, sizeof(packet), &received));
    EXPECT_EQ(-1, received);
}

static int child_status(pid_t child) {
    for (int attempt = 0; attempt < 500; attempt++) {
        int status;
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (result < 0 && errno != EINTR) return -1;
        usleep(10000);
    }
    kill(child, SIGKILL);
    while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {}
    return -1;
}

TEST(RuntimeEntry, RefusesExecutionOutsidePreparedContext) {
    ASSERT_NE(1, getpid());
    uid_t uid = getuid();
    EXPECT_EQ(-1, aegis_check_context(10));
    EXPECT_EQ(EPERM, errno);
    EXPECT_EQ(uid, getuid());
    char executable[4096];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    ASSERT_GT(length, 0);
    executable[length] = '\0';
    std::string binary(executable);
    binary = binary.substr(0, binary.rfind('/') + 1) + "aegis-runtime-init";
    ASSERT_EQ(0, access(binary.c_str(), X_OK));
    pid_t child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) { execl(binary.c_str(), binary.c_str(), "10", "10", nullptr); _exit(125); }
    EXPECT_EQ(78, child_status(child));
}

TEST(RuntimeFilter, OrdinaryForkWorksWhileNamespaceAndKeyOperationsAreRefused) {
    pid_t child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0 || aegis_install_filter() < 0) _exit(10);
        if (syscall(SYS_clone3, nullptr, 0) != -1 || errno != ENOSYS) _exit(11);
        if (syscall(SYS_unshare, CLONE_NEWUSER) != -1 || errno != EPERM) _exit(12);
        if (syscall(SYS_keyctl, 0, 0, 0, 0, 0) != -1 || errno != EPERM) _exit(13);
        pid_t grandchild = fork();
        if (grandchild < 0) _exit(14);
        if (grandchild == 0) {
            bool denied = syscall(SYS_setns, -1, 0) == -1 && errno == EPERM;
            _exit(denied ? 0 : 15);
        }
        _exit(child_status(grandchild) == 0 ? 0 : 16);
    }
    EXPECT_EQ(0, child_status(child));
}
}  // namespace
