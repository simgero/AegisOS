#include "package_request.h"
#include <android-base/unique_fd.h>
#include <gtest/gtest.h>
#include <linux/memfd.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <cstring>
using android::base::unique_fd;
namespace {
constexpr int seals = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
constexpr char payload[] = "owned-package-request";
unique_fd Input(int applied = seals) {
    unique_fd fd(syscall(SYS_memfd_create, "package-request-test", MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (!fd.ok() || write(fd.get(), payload, sizeof(payload)) != static_cast<ssize_t>(sizeof(payload))
            || (applied && fcntl(fd.get(), F_ADD_SEALS, applied) < 0)) return {};
    return fd;
}
}
TEST(PackageRequestDescriptor, SealedReadWriteDupStillNeedsIndependentReadonlyOpen) {
    auto input = Input(); ASSERT_TRUE(input.ok());
    EXPECT_EQ(-1, aegis_package_request_readonly(input.get(), sizeof(payload))); EXPECT_EQ(EPERM, errno);
    unique_fd duplicate(fcntl(input.get(), F_DUPFD_CLOEXEC, 4)); ASSERT_TRUE(duplicate.ok());
    EXPECT_EQ(-1, aegis_package_request_readonly(duplicate.get(), sizeof(payload)));
    unique_fd readonly(aegis_package_reopen_request(input.get(), sizeof(payload))); ASSERT_TRUE(readonly.ok());
    ASSERT_EQ(0, aegis_package_request_readonly(readonly.get(), sizeof(payload)));
    EXPECT_EQ(O_RDONLY, fcntl(readonly.get(), F_GETFL) & O_ACCMODE);
    EXPECT_EQ(O_RDWR, fcntl(input.get(), F_GETFL) & O_ACCMODE);
    EXPECT_NE(0, fcntl(readonly.get(), F_GETFD) & FD_CLOEXEC);
    struct stat original, copied;
    ASSERT_EQ(0, fstat(input.get(), &original)); ASSERT_EQ(0, fstat(readonly.get(), &copied));
    EXPECT_EQ(original.st_dev, copied.st_dev); EXPECT_EQ(original.st_ino, copied.st_ino);
    EXPECT_EQ(-1, pwrite(readonly.get(), "x", 1, 0)); EXPECT_EQ(EBADF, errno);
    EXPECT_EQ(-1, pwrite(input.get(), "x", 1, 0)); EXPECT_EQ(EPERM, errno);
    char value[sizeof(payload)] = {}; ASSERT_EQ(static_cast<ssize_t>(sizeof(value)), pread(readonly.get(), value, sizeof(value), 0));
    EXPECT_EQ(0, memcmp(payload, value, sizeof(value)));
}
TEST(PackageRequestDescriptor, RejectsUnsealedPartiallySealedAndWrongSizeRequests) {
    auto mutable_input = Input(0); ASSERT_TRUE(mutable_input.ok());
    EXPECT_EQ(-1, aegis_package_reopen_request(mutable_input.get(), sizeof(payload))); EXPECT_EQ(EPERM, errno);
    auto partial = Input(F_SEAL_SHRINK); ASSERT_TRUE(partial.ok());
    EXPECT_EQ(-1, aegis_package_reopen_request(partial.get(), sizeof(payload))); EXPECT_EQ(EPERM, errno);
    auto input = Input(); ASSERT_TRUE(input.ok());
    for (size_t bytes : {size_t(0), sizeof(payload)-1, sizeof(payload)+1, size_t(65537)}) {
        EXPECT_EQ(-1, aegis_package_reopen_request(input.get(), bytes)); EXPECT_EQ(EPERM, errno);
    }
}
TEST(PackageRequestDescriptor, RejectsNonRegularAndPathOnlyDescriptors) {
    int pipefd[2]; ASSERT_EQ(0, pipe2(pipefd, O_CLOEXEC));
    unique_fd pipe_in(pipefd[0]), pipe_out(pipefd[1]), device(open("/dev/null", O_RDONLY | O_CLOEXEC));
    EXPECT_EQ(-1, aegis_package_reopen_request(pipe_in.get(), sizeof(payload))); EXPECT_EQ(EPERM, errno);
    ASSERT_TRUE(device.ok()); EXPECT_EQ(-1, aegis_package_reopen_request(device.get(), sizeof(payload)));
    auto input = Input(); ASSERT_TRUE(input.ok());
    char path[64]; snprintf(path, sizeof(path), "/proc/self/fd/%d", input.get());
    unique_fd path_only(open(path, O_PATH | O_CLOEXEC)); ASSERT_TRUE(path_only.ok());
    EXPECT_EQ(-1, aegis_package_reopen_request(path_only.get(), sizeof(payload))); EXPECT_EQ(EPERM, errno);
    EXPECT_EQ(-1, aegis_package_request_readonly(path_only.get(), sizeof(payload))); EXPECT_EQ(EPERM, errno);
}
TEST(PackageRequestDescriptor, ScmRightsKeepsReadonlyAccessAfterOriginalCloses) {
    auto input = Input(); ASSERT_TRUE(input.ok());
    unique_fd readonly(aegis_package_reopen_request(input.get(), sizeof(payload))); ASSERT_TRUE(readonly.ok());
    int pair[2]; ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair));
    unique_fd sender(pair[0]), receiver(pair[1]);
    char byte = 'q'; iovec io = {&byte, 1};
    union { cmsghdr align; char bytes[CMSG_SPACE(sizeof(int))]; } control = {};
    msghdr message = {}; message.msg_iov=&io; message.msg_iovlen=1;
    message.msg_control=control.bytes; message.msg_controllen=sizeof(control.bytes);
    auto* header=CMSG_FIRSTHDR(&message); header->cmsg_level=SOL_SOCKET; header->cmsg_type=SCM_RIGHTS;
    header->cmsg_len=CMSG_LEN(sizeof(int)); int descriptor=readonly.get();
    memcpy(CMSG_DATA(header), &descriptor, sizeof(descriptor));
    ASSERT_EQ(1, sendmsg(sender.get(), &message, MSG_NOSIGNAL));
    input.reset(); readonly.reset(); memset(&control, 0, sizeof(control));
    message.msg_controllen=sizeof(control.bytes);
    ASSERT_EQ(1, recvmsg(receiver.get(), &message, MSG_CMSG_CLOEXEC));
    ASSERT_EQ(0, message.msg_flags & (MSG_TRUNC | MSG_CTRUNC));
    header=CMSG_FIRSTHDR(&message); ASSERT_NE(nullptr, header);
    ASSERT_EQ(SOL_SOCKET, header->cmsg_level); ASSERT_EQ(SCM_RIGHTS, header->cmsg_type);
    ASSERT_EQ(CMSG_LEN(sizeof(int)), header->cmsg_len);
    memcpy(&descriptor, CMSG_DATA(header), sizeof(descriptor)); unique_fd transferred(descriptor);
    ASSERT_EQ(0, aegis_package_request_readonly(transferred.get(), sizeof(payload)));
    EXPECT_EQ(-1, pwrite(transferred.get(), "x", 1, 0)); EXPECT_EQ(EBADF, errno);
    char value[sizeof(payload)] = {}; ASSERT_EQ(static_cast<ssize_t>(sizeof(value)), pread(transferred.get(), value, sizeof(value), 0));
    EXPECT_EQ(0, memcmp(payload, value, sizeof(value)));
}
