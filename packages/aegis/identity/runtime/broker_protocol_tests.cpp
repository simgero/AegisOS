// Build on aegis-build. Execute only in local Android QEMU, never on the host.
#include "broker_protocol.h"
#include <gtest/gtest.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
const unsigned char golden[] = {0x41,0x47,0x52,0x42,1,0,2,0,2,0,0,0,0,0,0,0,
    0x64,0xca,0x9a,0x3b,0,0,0,0,10,0,0,0,0xd2,4,0,0};
struct aegis_broker_request start() {
    struct aegis_broker_request request;
    memcpy(&request, golden, sizeof(request));
    return request;
}
int parse(const struct aegis_broker_request& request, uint64_t previous = 1, uint64_t now = 100) {
    struct aegis_broker_request output;
    return aegis_broker_parse(&request, sizeof(request), previous, now, &output);
}
struct Pair {
    int fd[2] = {-1, -1};
    ~Pair() { for (int value : fd) if (value >= 0) close(value); }
};
int open_fds() {
    DIR* dir = opendir("/proc/self/fd");
    if (!dir) return -1;
    int count = 0;
    while (dirent* entry = readdir(dir)) if (entry->d_name[0] != '.') ++count;
    closedir(dir);
    return count;
}
}

TEST(RuntimeBrokerProtocol, NativeGoldenFrameMatchesJavaEncoding) {
    struct aegis_broker_request output = {};
    ASSERT_EQ(0, aegis_broker_parse(golden, sizeof(golden), 1, 100, &output));
    EXPECT_EQ(2u, output.sequence); EXPECT_EQ(1000000100u, output.deadline_ns);
    EXPECT_EQ(10u, output.user); EXPECT_EQ(1234u, output.serial);
    EXPECT_EQ(AEGIS_BROKER_START, output.operation);
}

TEST(RuntimeBrokerProtocol, TruncatedMalformedAndOversizedRequestsDoNotModifyOutput) {
    struct aegis_broker_request output, before;
    memset(&output, 0xa5, sizeof(output)); before = output;
    unsigned char data[33]; memcpy(data, golden, 32); data[32] = 0;
    for (size_t size : {0u, 31u, 33u}) {
        EXPECT_EQ(-1, aegis_broker_parse(data, size, 1, 100, &output));
        EXPECT_EQ(0, memcmp(&output, &before, sizeof(output)));
    }
    for (size_t offset : {0u, 4u, 6u}) {
        memcpy(data, golden, 32); data[offset] = 0;
        EXPECT_EQ(-1, aegis_broker_parse(data, 32, 1, 100, &output));
    }
}

TEST(RuntimeBrokerProtocol, DeadlineAndSequenceAreBoundedAndMonotonic) {
    auto request = start();
    EXPECT_EQ(-1, parse(request, 2));
    EXPECT_EQ(-1, parse(request, 3));
    EXPECT_EQ(-1, parse(request, 1, request.deadline_ns));
    EXPECT_EQ(-1, parse(request, 1, UINT64_MAX));
    request.deadline_ns = 100 + AEGIS_BROKER_MAX_WAIT_NS + 1;
    EXPECT_EQ(-1, parse(request));
    request = start(); request.sequence = 0; EXPECT_EQ(-1, parse(request));
    request = start(); request.sequence = UINT64_MAX; EXPECT_EQ(-1, parse(request));
}

TEST(RuntimeBrokerProtocol, HelloMustPrecedePersonalOperationsAndCannotBeReplayed) {
    auto request = start();
    EXPECT_EQ(-1, parse(request, 0));
    request.operation = AEGIS_BROKER_HELLO; request.user = request.serial = 0;
    EXPECT_EQ(0, parse(request, 0));
    EXPECT_EQ(-1, parse(request, 1));
    request.user = 10; EXPECT_EQ(-1, parse(request, 0));
}

TEST(RuntimeBrokerProtocol, PersonalIdentityAndAllSerialStopAreUnambiguous) {
    auto request = start();
    for (uint32_t user : {0u, 9u, 21473u, UINT32_MAX}) {
        request.user = user; EXPECT_EQ(-1, parse(request));
    }
    request = start(); request.serial = UINT32_MAX; EXPECT_EQ(-1, parse(request));
    request = start(); request.operation = AEGIS_BROKER_STOP_USER;
    EXPECT_EQ(-1, parse(request));
    request.serial = 0; EXPECT_EQ(0, parse(request));
}

TEST(RuntimeBrokerProtocol, ReplyRequiresExactStateAndEchoesCorrelation) {
    Pair pair;
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair.fd));
    auto request = start();
    EXPECT_EQ(-1, aegis_broker_reply(pair.fd[0], &request, EIO, AEGIS_BROKER_READY));
    EXPECT_EQ(-1, aegis_broker_reply(pair.fd[0], &request, 0, AEGIS_BROKER_ABSENT));
    ASSERT_EQ(0, aegis_broker_reply(pair.fd[0], &request, 0, AEGIS_BROKER_READY));
    struct aegis_broker_reply reply = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(reply)), recv(pair.fd[1], &reply, sizeof(reply), MSG_DONTWAIT));
    EXPECT_EQ(AEGIS_BROKER_MAGIC, reply.magic); EXPECT_EQ(AEGIS_BROKER_VERSION, reply.version);
    EXPECT_EQ(request.operation, reply.operation); EXPECT_EQ(request.sequence, reply.sequence);
    EXPECT_EQ(request.user, reply.user); EXPECT_EQ(request.serial, reply.serial);
    EXPECT_EQ(0, reply.error); EXPECT_EQ(AEGIS_BROKER_READY, reply.state);
    request.operation = AEGIS_BROKER_STOP_USER; request.serial = 0;
    EXPECT_EQ(-1, aegis_broker_reply(pair.fd[0], &request, 0, AEGIS_BROKER_READY));
    EXPECT_EQ(0, aegis_broker_reply(pair.fd[0], &request, EBUSY, AEGIS_BROKER_SEALED));
}

TEST(RuntimeBrokerProtocol, DevelopmentRootIsNotSystemServerAndWrongSocketTypeIsRejected) {
    ASSERT_EQ(0u, getuid()) << "Run only as the dedicated development test process";
    Pair pair, stream;
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair.fd));
    EXPECT_EQ(-1, aegis_broker_check_peer(pair.fd[0])); EXPECT_EQ(EPERM, errno);
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, stream.fd));
    EXPECT_EQ(-1, aegis_broker_check_peer(stream.fd[0])); EXPECT_EQ(EPROTOTYPE, errno);
    EXPECT_EQ(-1, aegis_broker_check_peer(-1));
}

TEST(RuntimeBrokerProtocol, ReceiveRequiresOneWholePacketAndPreservesOutputOnFailure) {
    Pair pair;
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair.fd));
    struct aegis_broker_request output = {}, before = {};
    EXPECT_EQ(-1, aegis_broker_receive(pair.fd[0], 1, 100, &output)); EXPECT_EQ(EAGAIN, errno);
    unsigned char packet[33]; memcpy(packet, golden, 32); packet[32] = 0;
    for (size_t size : {31u, 33u}) {
        ASSERT_EQ(static_cast<ssize_t>(size), send(pair.fd[1], packet, size, MSG_NOSIGNAL));
        EXPECT_EQ(-1, aegis_broker_receive(pair.fd[0], 1, 100, &output)); EXPECT_EQ(EPROTO, errno);
        EXPECT_EQ(0, memcmp(&before, &output, sizeof(output)));
    }
    ASSERT_EQ(32, send(pair.fd[1], golden, sizeof(golden), MSG_NOSIGNAL));
    EXPECT_EQ(0, aegis_broker_receive(pair.fd[0], 1, 100, &output));
    EXPECT_EQ(10u, output.user);
    close(pair.fd[1]); pair.fd[1] = -1;
    EXPECT_EQ(-1, aegis_broker_receive(pair.fd[0], 2, 100, &output)); EXPECT_EQ(EPIPE, errno);
}

TEST(RuntimeBrokerProtocol, RejectedAncillaryDescriptorsIncludingTruncationAreClosed) {
    Pair pair;
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair.fd));
    int original = open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(original, 0);
    int before = open_fds(); ASSERT_GT(before, 0);
    for (size_t count : {1u, 12u}) {
        union { cmsghdr alignment; char bytes[CMSG_SPACE(12 * sizeof(int))]; } control = {};
        iovec io = {const_cast<unsigned char*>(golden), sizeof(golden)};
        msghdr message = {};
        message.msg_iov = &io; message.msg_iovlen = 1;
        message.msg_control = control.bytes; message.msg_controllen = CMSG_SPACE(count * sizeof(int));
        cmsghdr* ancillary = CMSG_FIRSTHDR(&message);
        ancillary->cmsg_level = SOL_SOCKET; ancillary->cmsg_type = SCM_RIGHTS;
        ancillary->cmsg_len = CMSG_LEN(count * sizeof(int));
        for (size_t i = 0; i < count; ++i) memcpy(CMSG_DATA(ancillary) + i * sizeof(int), &original, sizeof(int));
        EXPECT_EQ(32, sendmsg(pair.fd[1], &message, MSG_NOSIGNAL));
        struct aegis_broker_request output = {};
        EXPECT_EQ(-1, aegis_broker_receive(pair.fd[0], 1, 100, &output)); EXPECT_EQ(EPROTO, errno);
        EXPECT_EQ(before, open_fds()); EXPECT_GE(fcntl(original, F_GETFD), 0);
    }
    close(original);
}
