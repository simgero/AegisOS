// Build on aegis-build. Execute only in local Android QEMU, never on the host.
#include "broker_protocol.h"
#include "control.h"
#include <gtest/gtest.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <vector>
#include <unistd.h>

namespace {
const unsigned char golden[] = {0x41,0x47,0x52,0x42,2,0,2,0,2,0,0,0,0,0,0,0,
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

namespace {
std::vector<unsigned char> exec_packet() {
    // Same literal fixture as Java: /bin/printf, %s, empty argument.
    std::vector<unsigned char> packet = {0x41,0x47,0x52,0x42,2,0,5,0,2,0,0,0,0,0,0,0,
        0x64,0xca,0x9a,0x3b,0,0,0,0,10,0,0,0,0xd2,4,0,0,3,0,0,0,16,0,0,0};
    const unsigned char args[] = {'/','b','i','n','/','p','r','i','n','t','f',0,'%','s',0,0};
    packet.insert(packet.end(), args, args + sizeof(args));
    return packet;
}
}

TEST(RuntimeBrokerTerminal, ExactArgumentPacketPreservesEmptyAndLiteralArguments) {
    auto packet = exec_packet();
    aegis_broker_call call = {};
    ASSERT_EQ(0, aegis_broker_decode(packet.data(), packet.size(), 1, 100, &call));
    const char *argv[AEGIS_BROKER_MAX_ARGS + 1];
    ASSERT_EQ(0, aegis_broker_arguments(&call, argv));
    EXPECT_EQ(3u, call.argc); EXPECT_STREQ("/bin/printf", argv[0]);
    EXPECT_STREQ("%s", argv[1]); EXPECT_STREQ("", argv[2]); EXPECT_EQ(nullptr, argv[3]);
    // The decoded call owns bytes independently of the input packet.
    memset(packet.data(), 0xff, packet.size());
    EXPECT_STREQ("/bin/printf", argv[0]);
}

TEST(RuntimeBrokerTerminal, BadArgumentBoundariesNeverPublishPartialOutput) {
    const auto good = exec_packet();
    aegis_broker_call output, before;
    memset(&output, 0xa5, sizeof(output)); before = output;
    for (unsigned variant = 0; variant < 8; variant++) {
        auto packet = good;
        if (variant == 0) packet[32] = 0;
        if (variant == 1) packet[32] = 33;
        if (variant == 2) packet[32] = 2;
        if (variant == 3) packet[36] = 15;
        if (variant == 4) packet[40] = 'b';
        if (variant == 5) packet.back() = 'x';
        if (variant == 6) packet.push_back(0);
        if (variant == 7) packet.resize(39);
        EXPECT_EQ(-1, aegis_broker_decode(packet.data(), packet.size(), 1, 100, &output));
        EXPECT_EQ(EPROTO, errno); EXPECT_EQ(0, memcmp(&output, &before, sizeof(output)));
    }
}

TEST(RuntimeBrokerTerminal, CommandResultRequiresPositiveSignedIdentityAndExactFrame) {
    auto request = start(); request.operation = AEGIS_BROKER_RESULT;
    unsigned char packet[41] = {}; memcpy(packet, &request, sizeof(request));
    aegis_broker_call call = {};
    for (uint64_t command : {UINT64_C(0), UINT64_MAX, UINT64_C(1) << 63}) {
        memcpy(packet + 32, &command, 8);
        EXPECT_EQ(-1, aegis_broker_decode(packet, 40, 1, 100, &call));
    }
    uint64_t command = 123; memcpy(packet + 32, &command, 8);
    ASSERT_EQ(0, aegis_broker_decode(packet, 40, 1, 100, &call)); EXPECT_EQ(command, call.command);
    for (size_t size : {32u, 39u, 41u}) EXPECT_EQ(-1, aegis_broker_decode(packet, size, 1, 100, &call));
    request.operation = AEGIS_BROKER_START; memcpy(packet, &request, sizeof(request));
    EXPECT_EQ(-1, aegis_broker_decode(packet, 40, 1, 100, &call));
    EXPECT_EQ(0, aegis_broker_decode(packet, 32, 1, 100, &call));
}

TEST(RuntimeBrokerTerminal, TerminalPayloadCannotBypassHandshakeSequenceOrDeadline) {
    auto packet = exec_packet();
    aegis_broker_call call = {};
    EXPECT_EQ(-1, aegis_broker_decode(packet.data(), packet.size(), 0, 100, &call));
    EXPECT_EQ(-1, aegis_broker_decode(packet.data(), packet.size(), 2, 100, &call));
    EXPECT_EQ(-1, aegis_broker_decode(packet.data(), packet.size(), 1, 1000000100, &call));
    aegis_broker_request header;
    EXPECT_EQ(-1, aegis_broker_parse(packet.data(), 32, 1, 100, &header));
    packet[4] = 1; // Protocol v1 must not silently accept a terminal extension.
    EXPECT_EQ(-1, aegis_broker_decode(packet.data(), packet.size(), 1, 100, &call));
}

TEST(RuntimeBrokerTerminal, TerminalReplyTransfersOneDescriptorWithoutConsumingOwnersCopy) {
    Pair pair;
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair.fd));
    // Transport-only fixture. PTY validation is separately tested by RuntimeExec.
    int original = open("/dev/null", O_RDONLY | O_CLOEXEC); ASSERT_GE(original, 0);
    auto request = start(); request.operation = AEGIS_BROKER_EXEC;
    EXPECT_EQ(-1, aegis_broker_reply_terminal(pair.fd[0], &request, 0, 44, 0, 0, -1));
    EXPECT_EQ(-1, aegis_broker_reply_terminal(pair.fd[0], &request, EIO, 44, 0, 0, original));
    ASSERT_EQ(0, aegis_broker_reply_terminal(pair.fd[0], &request, 0, 44, 0, 0, original));
    struct aegis_broker_terminal_reply reply = {};
    int received = -1;
    ASSERT_EQ(48, aegis_receive(pair.fd[1], &reply, sizeof(reply), &received));
    EXPECT_EQ(44u, reply.command); EXPECT_EQ(0u, reply.exited);
    ASSERT_GE(received, 0); EXPECT_NE(original, received);
    EXPECT_NE(0, fcntl(received, F_GETFD) & FD_CLOEXEC); EXPECT_GE(fcntl(original, F_GETFD), 0);
    close(received); close(original);
}

TEST(RuntimeBrokerTerminal, RunningExitedAndErrorRepliesCannotBeConfused) {
    Pair pair;
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair.fd));
    auto request = start(); request.operation = AEGIS_BROKER_RESULT;
    EXPECT_EQ(-1, aegis_broker_reply_terminal(pair.fd[0], &request, 0, 44, 256, 0, -1));
    EXPECT_EQ(-1, aegis_broker_reply_terminal(pair.fd[0], &request, 0, 44, 0x7f, 1, -1));
    EXPECT_EQ(-1, aegis_broker_reply_terminal(pair.fd[0], &request, ENOENT, 44, 0, 0, -1));
    for (int exited : {0, 1}) {
        ASSERT_EQ(0, aegis_broker_reply_terminal(pair.fd[0], &request, 0, 44, 0, exited, -1));
        struct aegis_broker_terminal_reply reply = {}; int fd = -1;
        ASSERT_EQ(48, aegis_receive(pair.fd[1], &reply, sizeof(reply), &fd));
        EXPECT_EQ(-1, fd); EXPECT_EQ(static_cast<uint32_t>(exited), reply.exited);
        EXPECT_EQ(0, reply.wait_status); EXPECT_EQ(AEGIS_BROKER_READY, reply.header.state);
    }
    ASSERT_EQ(0, aegis_broker_reply_terminal(pair.fd[0], &request, ENOENT, 0, 0, 0, -1));
    struct aegis_broker_terminal_reply reply = {}; int fd = -1;
    ASSERT_EQ(48, aegis_receive(pair.fd[1], &reply, sizeof(reply), &fd));
    EXPECT_EQ(ENOENT, reply.header.error); EXPECT_EQ(0u, reply.command); EXPECT_EQ(-1, fd);
}

TEST(RuntimeBrokerTerminal, OversizedTerminalPacketsAndIncomingDescriptorsAreRejectedWithoutLeaks) {
    Pair pair;
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair.fd));
    auto packet = exec_packet();
    int original = open("/dev/null", O_RDONLY | O_CLOEXEC); ASSERT_GE(original, 0);
    int before = open_fds();
    union { cmsghdr alignment; char bytes[CMSG_SPACE(sizeof(int))]; } control = {};
    iovec io = {packet.data(), packet.size()}; msghdr message = {};
    message.msg_iov = &io; message.msg_iovlen = 1;
    message.msg_control = control.bytes; message.msg_controllen = sizeof(control.bytes);
    cmsghdr *ancillary = CMSG_FIRSTHDR(&message);
    ancillary->cmsg_level = SOL_SOCKET; ancillary->cmsg_type = SCM_RIGHTS;
    ancillary->cmsg_len = CMSG_LEN(sizeof(int)); memcpy(CMSG_DATA(ancillary), &original, sizeof(int));
    ASSERT_EQ(static_cast<ssize_t>(packet.size()), sendmsg(pair.fd[1], &message, MSG_NOSIGNAL));
    aegis_broker_call call = {};
    EXPECT_EQ(-1, aegis_broker_receive_call(pair.fd[0], 1, 100, &call)); EXPECT_EQ(EPROTO, errno);
    EXPECT_EQ(before, open_fds()); close(original);
    packet.resize(AEGIS_BROKER_MAX_PACKET + 1);
    ASSERT_EQ(static_cast<ssize_t>(packet.size()), send(pair.fd[1], packet.data(), packet.size(), MSG_NOSIGNAL));
    EXPECT_EQ(-1, aegis_broker_receive_call(pair.fd[0], 1, 100, &call)); EXPECT_EQ(EPROTO, errno);
}

TEST(RuntimeBrokerTerminal, ExactMaximumPayloadAndArgumentCountAreAcceptedWithoutTruncation) {
    auto packet = exec_packet();
    packet.resize(AEGIS_BROKER_MAX_PACKET, 'x');
    uint32_t argc = 32, size = AEGIS_BROKER_MAX_ARG_BYTES;
    memcpy(packet.data() + 32, &argc, 4); memcpy(packet.data() + 36, &size, 4);
    memset(packet.data() + 40, 'x', size); packet[40] = '/';
    // One absolute program followed by 31 empty arguments, all at the boundary.
    memset(packet.data() + packet.size() - argc, 0, argc);
    aegis_broker_call call = {};
    ASSERT_EQ(0, aegis_broker_decode(packet.data(), packet.size(), 1, 100, &call));
    const char *argv[AEGIS_BROKER_MAX_ARGS + 1];
    ASSERT_EQ(0, aegis_broker_arguments(&call, argv));
    EXPECT_EQ(static_cast<size_t>(size - argc), strlen(argv[0]));
    EXPECT_STREQ("", argv[31]); EXPECT_EQ(nullptr, argv[32]);
}
