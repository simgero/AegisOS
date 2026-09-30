// Compile on aegis-build; execute only in local Android QEMU with the new
// namespace-capable kernel and applicable SELinux policy. Never skip missing
// prerequisites into a passing isolation result. No AOSP users are created.
#include "namespace.h"
#include "namespace_probe.h"
#include "package_policy_probe.h"
#include "package_apt_probe.h"
#include "package_apt_plan.h"
#include "package_apt_fixture.h"
#include "sandbox.h"
#include <linux/capability.h>
#include <linux/securebits.h>
#include <sys/prctl.h>
#include "base_image.h"

#include <gtest/gtest.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <linux/mount.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <set>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
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

class RuntimeNamespace : public ::testing::Test {
 protected:
    int setup = -1, peers[2] = {-1, -1}, source = -1, mounts[2] = {-1, -1};
    bool source_attached = false;
    aegis_namespace* contexts[2] = {nullptr, nullptr};

    void SetUp() override {
        ASSERT_EQ(0u, getuid()) << "Requires the dedicated root device-test process";
        ASSERT_EQ(0u, getgid());
        // Affects only this disposable test process, not Android users/groups.
        ASSERT_EQ(0, setgroups(0, nullptr));
        struct sigaction action = {};
        action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask);
        ASSERT_EQ(0, sigaction(SIGCHLD, &action, nullptr));
        // Match production's explicit trusted startup handoff. Only this
        // developer-root fixture opens /proc/1; the daemon inherits from init.
        int user_ns = open("/proc/1/ns/user", O_RDONLY | O_CLOEXEC);
        int pid_ns = open("/proc/1/ns/pid", O_RDONLY | O_CLOEXEC);
        int mount_ns = open("/proc/1/ns/mnt", O_RDONLY | O_CLOEXEC);
        int pinned = aegis_namespace_pin_host(user_ns, pid_ns, mount_ns);
        int saved = errno;
        if (user_ns >= 0) close(user_ns);
        if (pid_ns >= 0) close(pid_ns);
        if (mount_ns >= 0) close(mount_ns);
        ASSERT_EQ(0, pinned) << strerror(saved);
        ASSERT_EQ(0, aegis_namespace_private_mounts()) << strerror(errno);
        char path[PATH_MAX];
        ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
        ASSERT_GT(n, 0);
        ASSERT_LT(n, static_cast<ssize_t>(sizeof(path) - 1));
        path[n] = '\0';
        std::string binary(path);
        binary.resize(binary.find_last_of('/') + 1);
        binary += "aegis-runtime-namespace-probe";
        setup = open(binary.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        ASSERT_GE(setup, 0);
        struct stat st;
        ASSERT_EQ(0, fstat(setup, &st));
        ASSERT_EQ(0u, st.st_uid) << "Install trusted test artifacts as root:root, mode 0755";
    }

    void TearDown() override {
        for (unsigned i = 0; i < 2; i++) {
            if (contexts[i]) {
                EXPECT_EQ(0, aegis_namespace_stop(contexts[i]));
                aegis_child_exit result = {};
                EXPECT_EQ(0, aegis_namespace_wait(contexts[i], 5000, &result));
                aegis_namespace_release(contexts[i]);
            }
            if (peers[i] >= 0) close(peers[i]);
            if (mounts[i] >= 0) close(mounts[i]);
        }
        if (source_attached) {
            struct stat ours, named;
            if (aegis_namespace_check_broker() == 0
                    && fstat(source, &ours) == 0 && stat("/mnt", &named) == 0
                    && ours.st_dev == named.st_dev && ours.st_ino == named.st_ino) {
                EXPECT_EQ(0, umount2("/mnt", MNT_DETACH));
            } else ADD_FAILURE() << "Refuse to unmount a replaced fixture anchor";
        }
        if (source >= 0) close(source);
        if (setup >= 0) close(setup);
        unsetenv("AEGIS_NAMESPACE_TEST_ONLY");
    }

    int create(unsigned slot, uint32_t user) {
        int pair[2];
        if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair) < 0) return -1;
        int result = aegis_namespace_create(user, 1234, setup, pair[1], &contexts[slot]);
        int saved = errno;
        close(pair[1]);
        if (result < 0) close(pair[0]);
        else peers[slot] = pair[0];
        errno = saved;
        return result;
    }

    ssize_t report(unsigned slot, aegis_namespace_probe* output) {
        pollfd ready = {peers[slot], POLLIN, 0};
        if (poll(&ready, 1, 5000) != 1) return -1;
        return recv(peers[slot], output, sizeof(*output), MSG_DONTWAIT | MSG_TRUNC);
    }

    void check(const aegis_namespace_probe& result, uint32_t user, bool package = false) {
        EXPECT_EQ(0x41454e53u, result.magic);
        EXPECT_EQ(user, result.user_id);
        EXPECT_EQ(1234u, result.serial);
        EXPECT_EQ(1u, result.pid); EXPECT_EQ(0u, result.ppid);
        EXPECT_EQ(1u, result.session_id); EXPECT_EQ(1u, result.process_group);
        EXPECT_EQ(0, result.oom_score_adj);
        EXPECT_EQ(0u, result.uid); EXPECT_EQ(0u, result.gid);
        EXPECT_EQ(0u, result.groups); EXPECT_EQ(static_cast<uint32_t>(SIGKILL), result.death_signal);
        EXPECT_EQ(0u, result.extra_fds); EXPECT_EQ(1u, result.fixed_environment);
        EXPECT_EQ(1u, result.private_mounts); EXPECT_EQ(package ? 0u : 1u, result.setgroups_denied);
        const uint32_t expected[3][3] = {
            {0, user * 100000 + 5000, 1000}, {1000, user * 100000 + 7500, 1},
            {65534, user * 100000 + 7501, 1},
        };
        for (unsigned row = 0; row < 3; row++) for (unsigned col = 0; col < 3; col++) {
            EXPECT_EQ(expected[row][col], result.uid_rows[row][col]);
            EXPECT_EQ(expected[row][col], result.gid_rows[row][col]);
        }
        const char* names[] = {"user", "pid", "mnt", "ipc", "uts", "net"};
        for (unsigned i = 0; i < 6; i++) {
            std::string path = std::string("/proc/self/ns/") + names[i];
            struct stat st;
            ASSERT_EQ(0, stat(path.c_str(), &st));
            EXPECT_NE(static_cast<uint64_t>(st.st_ino), result.namespace_inodes[i]) << names[i];
        }
    }

    void finish(unsigned slot) {
        ASSERT_EQ(1, send(peers[slot], "Q", 1, MSG_NOSIGNAL));
        aegis_child_exit result = {};
        ASSERT_EQ(0, aegis_namespace_wait(contexts[slot], 5000, &result));
        EXPECT_EQ(CLD_EXITED, result.code); EXPECT_EQ(0, result.status);
    }

    int fixture(bool readonly) {
        // Real kernel filesystem operations, ONLY in the local Android guest.
        // Attach only in the checked private broker namespace, just as the
        // production base must be attached before the kernel can clone it.
        // No loop device, Android mount or AOSP user/CE directory is changed.
        int fs = static_cast<int>(syscall(SYS_fsopen, "tmpfs", FSOPEN_CLOEXEC));
        if (fs < 0) return -1;
        if (syscall(SYS_fsconfig, fs, FSCONFIG_SET_STRING, "mode", "0755", 0) < 0
                || syscall(SYS_fsconfig, fs, FSCONFIG_SET_STRING, "size", "1048576", 0) < 0
                || syscall(SYS_fsconfig, fs, FSCONFIG_CMD_CREATE, nullptr, nullptr, 0) < 0) {
            int saved = errno; close(fs); errno = saved; return -1;
        }
        source = static_cast<int>(syscall(SYS_fsmount, fs, FSMOUNT_CLOEXEC, 0u));
        int saved = errno;
        close(fs);
        errno = saved;
        if (source < 0) return -1;
        const char* names[] = {"root.txt", "user.txt", "nobody.txt"};
        const uid_t owners[] = {0, 1000, 65534};
        for (unsigned i = 0; i < 3; i++) {
            int file = openat(source, names[i], O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0644);
            if (file < 0) return -1;
            int result = write(file, "fixture\n", 8) == 8
                    && fchown(file, owners[i], owners[i]) == 0 && fchmod(file, 0644) == 0 ? 0 : -1;
            saved = errno; close(file); errno = saved;
            if (result < 0) return -1;
        }
        if (readonly) return freeze_source();
        return 0;
    }

    int freeze_source(bool anchor = true) {
        mount_attr attributes = {};
        attributes.attr_set = MOUNT_ATTR_RDONLY | MOUNT_ATTR_NOEXEC;
        int result = static_cast<int>(syscall(SYS_mount_setattr, source, "", AT_EMPTY_PATH,
                                              &attributes, sizeof(attributes)));
        if (result < 0) return -1;
        return anchor ? anchor_source() : 0;
    }

    int anchor_source() {
        struct stat android_before, android_after;
        if (stat("/proc/1/root/mnt", &android_before) < 0) return -1;
        if (aegis_namespace_attach_base(source) < 0) return -1;
        source_attached = true;
        if (stat("/proc/1/root/mnt", &android_after) < 0) return -1;
        if (android_before.st_dev != android_after.st_dev
                || android_before.st_ino != android_after.st_ino) { errno = EXDEV; return -1; }
        return 0;
    }

    void mapped_files(int tree, uint32_t user) {
        struct stat root;
        ASSERT_EQ(0, fstat(tree, &root));
        EXPECT_EQ(user * 100000 + 5000, root.st_uid);
        EXPECT_EQ(user * 100000 + 5000, root.st_gid);
        EXPECT_EQ(static_cast<mode_t>(0755), root.st_mode & 07777);
        struct statvfs flags;
        ASSERT_EQ(0, fstatvfs(tree, &flags));
        EXPECT_EQ(static_cast<unsigned long>(ST_RDONLY | ST_NOSUID | ST_NODEV),
                  flags.f_flag & (ST_RDONLY | ST_NOSUID | ST_NODEV));
        EXPECT_EQ(0u, flags.f_flag & ST_NOEXEC);
        int fd_flags = fcntl(tree, F_GETFD);
        ASSERT_GE(fd_flags, 0); EXPECT_NE(0, fd_flags & FD_CLOEXEC);
        ASSERT_EQ(0, fstatvfs(source, &flags));
        EXPECT_NE(0u, flags.f_flag & ST_NOEXEC);
        const char* names[] = {"root.txt", "user.txt", "nobody.txt"};
        const uid_t originals[] = {0, 1000, 65534};
        const uid_t offsets[] = {5000, 7500, 7501};
        for (unsigned i = 0; i < 3; i++) {
            struct stat mapped, original;
            ASSERT_EQ(0, fstatat(tree, names[i], &mapped, AT_SYMLINK_NOFOLLOW));
            ASSERT_EQ(0, fstatat(source, names[i], &original, AT_SYMLINK_NOFOLLOW));
            EXPECT_EQ(user * 100000 + offsets[i], mapped.st_uid);
            EXPECT_EQ(user * 100000 + offsets[i], mapped.st_gid);
            EXPECT_EQ(originals[i], original.st_uid); EXPECT_EQ(originals[i], original.st_gid);
            EXPECT_EQ(original.st_dev, mapped.st_dev); EXPECT_EQ(original.st_ino, mapped.st_ino);
            int file = openat(tree, names[i], O_RDONLY | O_CLOEXEC);
            ASSERT_GE(file, 0);
            char bytes[8] = {};
            EXPECT_EQ(8, read(file, bytes, sizeof(bytes)));
            EXPECT_EQ(std::string("fixture\n"), std::string(bytes, sizeof(bytes)));
            close(file);
        }
    }
};

TEST_F(RuntimeNamespace, OfflineAptInstallsUpgradesAndPurgesCompleteCandidate) {
    AptImageFixture fixture;
    source = fixture.create();
    ASSERT_GE(source, 0) << strerror(errno);
    ASSERT_EQ(0, create(0, 10));
    EXPECT_EQ(-1, aegis_namespace_map_candidate(contexts[0], source));
    EXPECT_EQ(EAGAIN, errno);
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[0]));
    EXPECT_EQ(-1, aegis_namespace_map_candidate(contexts[0], source)); EXPECT_EQ(EPERM, errno);
    struct stat unmapped;struct statvfs unchanged_flags;
    ASSERT_EQ(0, fstat(source, &unmapped));ASSERT_EQ(0, fstatvfs(source, &unchanged_flags));
    EXPECT_EQ(0u, unmapped.st_uid);EXPECT_NE(0u, unchanged_flags.f_flag & ST_NOEXEC);
    ASSERT_EQ(0, aegis_namespace_stop(contexts[0]));
    aegis_child_exit rejected = {};
    ASSERT_EQ(0, aegis_namespace_wait(contexts[0], 5000, &rejected));
    aegis_namespace_release(contexts[0]);contexts[0] = nullptr;
    close(peers[0]);peers[0] = -1;
    ASSERT_EQ(0, create(0, 10));
    ASSERT_EQ(0, aegis_namespace_prepare_package(contexts[0]));
    EXPECT_EQ(-1, aegis_namespace_home_mount(contexts[0], 0)); EXPECT_EQ(EPERM, errno);
    // Wrong object and a second mapping attempt cannot turn the ordinary base
    // or an already mapped candidate into another writable worker root.
    EXPECT_EQ(-1, aegis_namespace_map_candidate(contexts[0], setup));
    ASSERT_EQ(0, aegis_namespace_map_candidate(contexts[0], source)) << strerror(errno);
    EXPECT_EQ(-1, aegis_namespace_map_candidate(contexts[0], source));
    EXPECT_EQ(EPERM, errno);
    mounts[0] = aegis_namespace_devices_mount(contexts[0]);
    ASSERT_GE(mounts[0], 0) << strerror(errno);
    ASSERT_EQ(0, aegis_namespace_resume(contexts[0]));
    aegis_namespace_probe namespace_report = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(namespace_report)), report(0, &namespace_report));
    check(namespace_report, 10, true);
    ASSERT_EQ(1, send(peers[0], "A", 1, MSG_NOSIGNAL));
    uint32_t magic = 0x41505446;
    iovec io = {&magic, sizeof(magic)};
    alignas(cmsghdr) char control[CMSG_SPACE(2 * sizeof(int))] = {};
    msghdr msg = {};
    msg.msg_iov = &io;msg.msg_iovlen = 1;msg.msg_control = control;msg.msg_controllen = sizeof(control);
    cmsghdr* header = CMSG_FIRSTHDR(&msg);
    header->cmsg_level = SOL_SOCKET;header->cmsg_type = SCM_RIGHTS;header->cmsg_len = CMSG_LEN(2 * sizeof(int));
    int fds[] = {source, mounts[0]};memcpy(CMSG_DATA(header), fds, sizeof(fds));
    ASSERT_EQ(static_cast<ssize_t>(sizeof(magic)), sendmsg(peers[0], &msg, MSG_NOSIGNAL));
    pollfd ready = {peers[0], POLLIN, 0};
    ASSERT_EQ(1, poll(&ready, 1, 90000));
    aegis_apt_probe_result result = {};
    ssize_t got = recv(peers[0], &result, sizeof(result), MSG_DONTWAIT | MSG_TRUNC);
    aegis_child_exit exited = {};
    ASSERT_EQ(0, aegis_namespace_wait(contexts[0], 5000, &exited));
    // The child is dead before reading any of its logs/results through our own
    // still-owned mount. Failure evidence stays in this unique fixture image.
    std::string log = AptImageFixture::read(source, "var/log/aegis-package-test.log");
    fprintf(stderr, "APT isolated candidate log:\n%s\n", log.c_str());
    for(const char* name:{"aegis-plan-unsigned.log","aegis-plan-signature.log","aegis-plan-expired.log",
                          "aegis-plan-index.log","aegis-plan-missing.log","aegis-plan-install.json",
                          "aegis-plan-upgrade.json","aegis-plan-remove.json"}) {
        auto evidence=AptImageFixture::read(source,(std::string("var/log/")+name).c_str());
        fprintf(stderr,"APT planner evidence %s:\n%s\n",name,evidence.c_str());
    }
    ASSERT_EQ(static_cast<ssize_t>(sizeof(result)), got);
    ASSERT_EQ(0x41505452u, result.magic);
    ASSERT_EQ(6u, result.phase) << "status=" << result.status << " errno=" << result.error;
    ASSERT_EQ(0u, result.status);ASSERT_EQ(0u, result.error);
    ASSERT_EQ(CLD_EXITED, exited.code);ASSERT_EQ(0, exited.status);
    ASSERT_EQ("APT_INSTALL_UPGRADE_PURGE_OK\n", AptImageFixture::read(source, "var/log/aegis-package-test.complete"));
    EXPECT_EQ("SIGNED_METADATA_RESOLUTION_OK\nUNSIGNED_REJECTED\nSIGNATURE_TAMPER_REJECTED\nEXPIRED_REJECTED\nINDEX_TAMPER_REJECTED\nMISSING_VERSION_REJECTED\n",
        AptImageFixture::read(source,"var/log/aegis-plan-tests.complete"));
    std::vector<aegis::PackageAptEffect> effects;
    auto json=AptImageFixture::read(source,"var/log/aegis-plan-install.json");
    ASSERT_EQ(0,aegis::PackageReadAptPlan(json,aegis::PackageAction::Install,"aegis-probe-app","2",&effects))<<json;
    ASSERT_EQ(2u,effects.size());
    EXPECT_EQ("aegis-probe-app",effects[0].name);EXPECT_EQ("aegis-probe-lib",effects[1].name);
    EXPECT_EQ("2",effects[0].after_version);EXPECT_EQ("2",effects[1].after_version);
    EXPECT_TRUE(effects[0].before_version.empty());EXPECT_TRUE(effects[1].before_version.empty());
    EXPECT_FALSE(effects[0].automatic);EXPECT_TRUE(effects[1].automatic);
    json=AptImageFixture::read(source,"var/log/aegis-plan-upgrade.json");
    ASSERT_EQ(0,aegis::PackageReadAptPlan(json,aegis::PackageAction::Update,"","",&effects))<<json;
    ASSERT_EQ(2u,effects.size());
    for(const auto& effect:effects) { EXPECT_EQ("1",effect.before_version);EXPECT_EQ("2",effect.after_version); }
    json=AptImageFixture::read(source,"var/log/aegis-plan-remove.json");
    ASSERT_EQ(0,aegis::PackageReadAptPlan(json,aegis::PackageAction::Remove,"aegis-probe-lib","",&effects))<<json;
    ASSERT_EQ(2u,effects.size());
    for(const auto& effect:effects) { EXPECT_EQ("2",effect.before_version);EXPECT_TRUE(effect.after_version.empty()); }

    struct stat st;
    ASSERT_EQ(0, fstatat(source, "var/lib/aegis-probe-owned", &st, AT_SYMLINK_NOFOLLOW));
    constexpr uid_t technical_owner = 10u * 100000u + 5000u + 42u;
    EXPECT_EQ(technical_owner, st.st_uid);EXPECT_EQ(technical_owner, st.st_gid);
    EXPECT_EQ(-1, fstatat(source, "usr/bin/aegis-probe-app", &st, AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(-1, fstatat(source, "etc/aegis-probe.conf", &st, AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT, errno);
    aegis_namespace_release(contexts[0]);contexts[0] = nullptr;
    close(peers[0]);peers[0] = -1;
    close(mounts[0]);mounts[0] = -1;
    close(source);source = -1; // last mount ref before autoclear and fixture unlink
    fixture.passed = !HasFailure();
}

TEST_F(RuntimeNamespace, PackageWorkerRightsSurviveExecWithoutHostOrMountAuthority) {
    ASSERT_EQ(0, create(0,10));
    ASSERT_EQ(0, aegis_namespace_prepare_package(contexts[0]));
    EXPECT_EQ(-1, aegis_namespace_home_mount(contexts[0], 0)); EXPECT_EQ(EPERM, errno);
    ASSERT_EQ(0,aegis_namespace_resume(contexts[0]));
    aegis_namespace_probe initial={};ASSERT_EQ(static_cast<ssize_t>(sizeof(initial)),report(0,&initial));check(initial,10,true);
    ASSERT_EQ(1,send(peers[0],"P",1,MSG_NOSIGNAL));
    pollfd ready={peers[0],POLLIN,0};ASSERT_EQ(1,poll(&ready,1,5000));
    aegis_package_policy_probe result={};
    ssize_t received=recv(peers[0],&result,sizeof(result),MSG_TRUNC);
    if (received != static_cast<ssize_t>(sizeof(result))) {
        aegis_child_exit diagnostic={};int waited=aegis_namespace_wait(contexts[0],5000,&diagnostic);
        FAIL() << "Policy fixture reply=" << received << " wait=" << waited
               << " code=" << diagnostic.code << " status=" << diagnostic.status;
    }
    EXPECT_EQ(0x504b4753u,result.magic);EXPECT_EQ(10u,result.user);EXPECT_EQ(127u,result.checks);
    const uint64_t allowed=(1u<<CAP_CHOWN)|(1u<<CAP_DAC_OVERRIDE)|(1u<<CAP_FOWNER)
        |(1u<<CAP_FSETID)|(1u<<CAP_SETUID)|(1u<<CAP_SETGID);
    EXPECT_EQ(allowed,result.effective);EXPECT_EQ(allowed,result.permitted);
    EXPECT_EQ(allowed,result.inheritable);EXPECT_EQ(allowed,result.bounding);EXPECT_EQ(allowed,result.ambient);
    EXPECT_EQ(1u,result.nnp);EXPECT_EQ(2u,result.filter);
    EXPECT_EQ(static_cast<uint32_t>(SECBIT_NOROOT|SECBIT_NOROOT_LOCKED
        |SECBIT_NO_CAP_AMBIENT_RAISE|SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED),result.securebits);
    aegis_child_exit ended={};ASSERT_EQ(0,aegis_namespace_wait(contexts[0],5000,&ended));
    EXPECT_EQ(CLD_EXITED,ended.code);EXPECT_EQ(0,ended.status);
}

TEST(RuntimePackageSandbox, HostCallerRejectedBeforePrivilegeChanges) {
    int bits=prctl(PR_GET_SECUREBITS,0,0,0,0),nnp=prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0);
    EXPECT_EQ(-1,aegis_limit_package_worker(10));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(-1,aegis_limit_package_supervisor(10));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(bits,prctl(PR_GET_SECUREBITS,0,0,0,0));
    EXPECT_EQ(nnp,prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0));
}

TEST_F(RuntimeNamespace, HostHandoffRejectsWrongHandlesWithoutLeakingOrReplacingPins) {
    int user_ns = open("/proc/1/ns/user", O_RDONLY | O_CLOEXEC);
    int pid_ns = open("/proc/1/ns/pid", O_RDONLY | O_CLOEXEC);
    int mount_ns = open("/proc/1/ns/mnt", O_RDONLY | O_CLOEXEC);
    int regular = open("/dev/null", O_RDONLY | O_CLOEXEC);
    int changed = open("/proc/self/ns/mnt", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(user_ns, 0); ASSERT_GE(pid_ns, 0); ASSERT_GE(mount_ns, 0);
    ASSERT_GE(regular, 0); ASSERT_GE(changed, 0);
    int before = descriptors();
    for (unsigned i = 0; i < 8; i++) {
        EXPECT_EQ(-1, aegis_namespace_pin_host(-1, pid_ns, mount_ns));
        EXPECT_EQ(-1, aegis_namespace_pin_host(regular, pid_ns, mount_ns));
        EXPECT_EQ(-1, aegis_namespace_pin_host(pid_ns, user_ns, mount_ns));
        EXPECT_EQ(-1, aegis_namespace_pin_host(user_ns, user_ns, mount_ns));
        EXPECT_EQ(-1, aegis_namespace_pin_host(user_ns, pid_ns, changed));
        EXPECT_EQ(0, aegis_namespace_pin_host(user_ns, pid_ns, mount_ns));
        EXPECT_EQ(before, descriptors());
        EXPECT_EQ(0, aegis_namespace_check_broker());
    }
    close(user_ns); close(pid_ns); close(mount_ns); close(regular); close(changed);
    EXPECT_EQ(0, aegis_namespace_check_broker());
}

TEST_F(RuntimeNamespace, BrokerOwnsAPrivateMountNamespaceAndRefusesOtherNamespaces) {
    struct stat self, init;
    ASSERT_EQ(0, stat("/proc/self/ns/mnt", &self));
    ASSERT_EQ(0, stat("/proc/1/ns/mnt", &init));
    EXPECT_NE(self.st_ino, init.st_ino);
    ASSERT_EQ(0, aegis_namespace_check_broker());
    int own = open("/proc/self/ns/mnt", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(own, 0);
    // Only this disposable test process moves. The production API never takes
    // a namespace fd from a caller. Save/restore the pinned view even on error.
    ASSERT_EQ(0, unshare(CLONE_NEWNS));
    EXPECT_EQ(-1, aegis_namespace_check_broker()); EXPECT_EQ(EPERM, errno);
    EXPECT_EQ(-1, aegis_namespace_private_mounts()); EXPECT_EQ(EPERM, errno);
    int restored = setns(own, CLONE_NEWNS);
    close(own);
    ASSERT_EQ(0, restored);
    EXPECT_EQ(0, aegis_namespace_check_broker());
}

TEST_F(RuntimeNamespace, GatePrecedesExecAndKernelMapsMatchTheSelectedUser) {
    ASSERT_EQ(0, setenv("AEGIS_NAMESPACE_TEST_ONLY", "must-not-reach-helper", 1));
    ASSERT_EQ(0, create(0, 10)) << strerror(errno);
    aegis_namespace* original = contexts[0];
    EXPECT_EQ(-1, aegis_namespace_create(10, 1234, setup, peers[0], &contexts[0]));
    EXPECT_EQ(EINVAL, errno); EXPECT_EQ(original, contexts[0]);
    pollfd ready = {peers[0], POLLIN, 0};
    EXPECT_EQ(0, poll(&ready, 1, 20));
    aegis_child_exit untouched = {123, 456};
    EXPECT_EQ(-1, aegis_namespace_wait(contexts[0], 20, &untouched));
    EXPECT_EQ(ETIMEDOUT, errno); EXPECT_EQ(123, untouched.code); EXPECT_EQ(456, untouched.status);
    ASSERT_EQ(0, aegis_namespace_resume(contexts[0])) << strerror(errno);
    aegis_namespace_probe actual = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(actual)), report(0, &actual));
    check(actual, 10);
    EXPECT_EQ(-1, aegis_namespace_resume(contexts[0])); EXPECT_EQ(EALREADY, errno);
    finish(0);
}

TEST_F(RuntimeNamespace, ConcurrentUsersHaveDistinctKernelNamespacesAndHostIds) {
    ASSERT_EQ(0, create(0, 10)) << strerror(errno);
    ASSERT_EQ(0, create(1, 11)) << strerror(errno);
    ASSERT_EQ(0, aegis_namespace_resume(contexts[0]));
    ASSERT_EQ(0, aegis_namespace_resume(contexts[1]));
    aegis_namespace_probe first = {}, second = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(first)), report(0, &first));
    ASSERT_EQ(static_cast<ssize_t>(sizeof(second)), report(1, &second));
    check(first, 10); check(second, 11);
    for (unsigned i = 0; i < 6; i++) EXPECT_NE(first.namespace_inodes[i], second.namespace_inodes[i]);
    for (unsigned i = 0; i < 3; i++) EXPECT_NE(first.uid_rows[i][1], second.uid_rows[i][1]);
    finish(0); finish(1);
}

TEST_F(RuntimeNamespace, PrivateReferencesResetInheritedOomProtectionBeforeExec) {
    int oom = open("/proc/self/oom_score_adj", O_RDWR | O_CLOEXEC);
    ASSERT_GE(oom, 0);
    char original[32];
    ssize_t length = pread(oom, original, sizeof(original), 0);
    if (length <= 0 || length == static_cast<ssize_t>(sizeof(original))) {
        close(oom); FAIL() << "Cannot preserve fixture OOM adjustment";
    }
    if (pwrite(oom, "-1000", 5, 0) != 5) {
        close(oom); FAIL() << "Cannot model Init's protected broker";
    }
    int created = create(0, 10);
    int saved = errno;
    ssize_t restored = pwrite(oom, original, static_cast<size_t>(length), 0);
    close(oom);
    ASSERT_EQ(length, restored) << "Restore fixture before reporting failure";
    ASSERT_EQ(0, created) << strerror(saved);
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[0])) << strerror(errno);
    pollfd ready = {peers[0], POLLIN, 0};
    EXPECT_EQ(0, poll(&ready, 1, 20)) << "Mapping must not release execution";
    ASSERT_EQ(0, aegis_namespace_resume(contexts[0])) << strerror(errno);
    aegis_namespace_probe actual = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(actual)), report(0, &actual));
    check(actual, 10); // Includes OOM=0 and exact parent-relative UID/GID maps.
    finish(0);
}

TEST_F(RuntimeNamespace, PreparingAndCancellingPrivateReferencesLeavesNoFdLeak) {
    int before = descriptors();
    ASSERT_GT(before, 0);
    for (unsigned attempt = 0; attempt < 12; attempt++) {
        ASSERT_EQ(0, create(0, 10 + attempt % 2)) << strerror(errno);
        ASSERT_EQ(0, aegis_namespace_prepare(contexts[0])) << strerror(errno);
        ASSERT_EQ(0, aegis_namespace_stop(contexts[0]));
        aegis_child_exit result = {};
        ASSERT_EQ(0, aegis_namespace_wait(contexts[0], 5000, &result));
        aegis_namespace_release(contexts[0]); contexts[0] = nullptr;
        close(peers[0]); peers[0] = -1;
        EXPECT_EQ(before, descriptors()) << attempt;
    }
}

TEST_F(RuntimeNamespace, CancellationBeforeMappingNeverExecutesTheProbe) {
    ASSERT_EQ(0, create(0, 10));
    ASSERT_EQ(0, aegis_namespace_stop(contexts[0]));
    aegis_child_exit result = {};
    ASSERT_EQ(0, aegis_namespace_wait(contexts[0], 5000, &result));
    EXPECT_TRUE((result.code == CLD_KILLED && result.status == SIGKILL)
                || (result.code == CLD_EXITED && result.status == 125));
    aegis_namespace_probe unused = {};
    EXPECT_EQ(0, report(0, &unused));
    EXPECT_EQ(-1, aegis_namespace_resume(contexts[0])); EXPECT_EQ(EALREADY, errno);
}

TEST_F(RuntimeNamespace, AlreadyReapedGateTimeoutCannotResume) {
    ASSERT_EQ(0, create(0, 10));
    aegis_child_exit result = {};
    ASSERT_EQ(0, aegis_namespace_wait(contexts[0], 15000, &result));
    EXPECT_EQ(CLD_EXITED, result.code); EXPECT_EQ(125, result.status);
    EXPECT_EQ(-1, aegis_namespace_resume(contexts[0])); EXPECT_EQ(ECHILD, errno);
    aegis_namespace_probe unused = {};
    EXPECT_EQ(0, report(0, &unused));
}

TEST_F(RuntimeNamespace, InvalidInputsLeaveNoChildOrLeakedDescriptors) {
    int seq[2], stream[2];
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, seq));
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, stream));
    int file = open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_GE(file, 0);
    int before = descriptors();
    ASSERT_GT(before, 0);
    for (int i = 0; i < 16; i++) {
        aegis_namespace* bad = nullptr;
        for (uint32_t user : {0u, 9u, 21473u, UINT32_MAX}) {
            EXPECT_EQ(-1, aegis_namespace_create(user, 1234, setup, seq[1], &bad));
            EXPECT_EQ(EINVAL, errno); EXPECT_EQ(nullptr, bad);
        }
        EXPECT_EQ(-1, aegis_namespace_create(10, UINT32_MAX, setup, seq[1], &bad));
        EXPECT_EQ(EINVAL, errno); EXPECT_EQ(nullptr, bad);
        EXPECT_EQ(-1, aegis_namespace_create(10, 1234, file, seq[1], &bad)); EXPECT_EQ(nullptr, bad);
        EXPECT_EQ(-1, aegis_namespace_create(10, 1234, setup, stream[1], &bad));
        EXPECT_EQ(EPROTOTYPE, errno); EXPECT_EQ(nullptr, bad);
        EXPECT_EQ(-1, aegis_namespace_create(10, 1234, -1, seq[1], &bad)); EXPECT_EQ(nullptr, bad);
    }
    EXPECT_EQ(before, descriptors());
    close(file); close(seq[0]); close(seq[1]); close(stream[0]); close(stream[1]);
}

TEST_F(RuntimeNamespace, InheritedObserverCannotReleaseOrSignalAnotherContext) {
    ASSERT_EQ(0, create(0, 10));
    pid_t observer = fork();
    ASSERT_GE(observer, 0);
    if (observer == 0) {
        int user_ns = open("/proc/1/ns/user", O_RDONLY | O_CLOEXEC);
        int pid_ns = open("/proc/1/ns/pid", O_RDONLY | O_CLOEXEC);
        int mount_ns = open("/proc/1/ns/mnt", O_RDONLY | O_CLOEXEC);
        bool ok = user_ns >= 0 && pid_ns >= 0 && mount_ns >= 0;
        ok &= aegis_namespace_pin_host(user_ns, pid_ns, mount_ns) == -1 && errno == EPERM;
        if (user_ns >= 0) close(user_ns);
        if (pid_ns >= 0) close(pid_ns);
        if (mount_ns >= 0) close(mount_ns);
        ok &= aegis_namespace_private_mounts() == -1 && errno == EPERM;
        ok &= aegis_namespace_resume(contexts[0]) == -1 && errno == EPERM;
        ok &= aegis_namespace_prepare(contexts[0]) == -1 && errno == EPERM;
        ok &= aegis_namespace_base_mount(contexts[0], -1) == -1 && errno == EPERM;
        ok &= aegis_namespace_home_mount(contexts[0], 0) == -1 && errno == EPERM;
        ok &= aegis_namespace_devices_mount(contexts[0]) == -1 && errno == EPERM;
        ok &= aegis_namespace_stop(contexts[0]) == -1 && errno == EPERM;
        aegis_child_exit result = {};
        ok &= aegis_namespace_wait(contexts[0], 0, &result) == -1 && errno == EPERM;
        _exit(ok ? 0 : 1);
    }
    int status;
    ASSERT_EQ(observer, waitpid(observer, &status, 0));
    ASSERT_TRUE(WIFEXITED(status)); EXPECT_EQ(0, WEXITSTATUS(status));
    ASSERT_EQ(0, aegis_namespace_resume(contexts[0]));
    aegis_namespace_probe actual = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(actual)), report(0, &actual));
    finish(0);
}

TEST_F(RuntimeNamespace, SupplementaryGroupsAreRejectedWithoutChangingTheCaller) {
    pid_t observer = fork();
    ASSERT_GE(observer, 0);
    if (observer == 0) {
        gid_t group = 0;
        if (setgroups(1, &group) < 0) _exit(2);
        aegis_namespace* bad = nullptr;
        bool ok = aegis_namespace_create(10, 1234, setup, -1, &bad) == -1 && errno == EPERM;
        ok &= bad == nullptr && getgroups(0, nullptr) == 1;
        _exit(ok ? 0 : 1);
    }
    int status;
    ASSERT_EQ(observer, waitpid(observer, &status, 0));
    ASSERT_TRUE(WIFEXITED(status)); EXPECT_EQ(0, WEXITSTATUS(status));
    EXPECT_EQ(0, getgroups(0, nullptr));
}

TEST_F(RuntimeNamespace, AutoReapingIsRejectedBeforeAnyChildIsCreated) {
    for (int option = 0; option < 2; option++) {
        pid_t observer = fork();
        ASSERT_GE(observer, 0);
        if (observer == 0) {
            struct sigaction action = {};
            action.sa_handler = option ? SIG_DFL : SIG_IGN;
            action.sa_flags = option ? SA_NOCLDWAIT : 0;
            if (sigaction(SIGCHLD, &action, nullptr) < 0) _exit(2);
            aegis_namespace* bad = nullptr;
            bool ok = aegis_namespace_create(10, 1234, setup, -1, &bad) == -1 && errno == EPERM;
            _exit(ok && !bad ? 0 : 1);
        }
        int status;
        ASSERT_EQ(observer, waitpid(observer, &status, 0));
        ASSERT_TRUE(WIFEXITED(status)); EXPECT_EQ(0, WEXITSTATUS(status));
    }
}

TEST_F(RuntimeNamespace, MultithreadedCallersAreRejectedBeforeCloning) {
    int pipe_fds[2];
    ASSERT_EQ(0, pipe2(pipe_fds, O_CLOEXEC));
    pthread_t thread;
    auto wait_for_end = [](void* fd) -> void* {
        char value;
        while (read(*static_cast<int*>(fd), &value, 1) < 0 && errno == EINTR) {}
        return nullptr;
    };
    ASSERT_EQ(0, pthread_create(&thread, nullptr, wait_for_end, &pipe_fds[0]));
    aegis_namespace* bad = nullptr;
    EXPECT_EQ(-1, aegis_namespace_create(10, 1234, setup, -1, &bad));
    EXPECT_EQ(EPERM, errno); EXPECT_EQ(nullptr, bad);
    EXPECT_EQ(1, write(pipe_fds[1], "Q", 1));
    EXPECT_EQ(0, pthread_join(thread, nullptr));
    close(pipe_fds[0]); close(pipe_fds[1]);
}

TEST_F(RuntimeNamespace, PreparedMappingsKeepExecBlockedWhileBaseMountIsBuilt) {
    ASSERT_EQ(0, fixture(true)) << strerror(errno);
    ASSERT_EQ(0, create(0, 10)) << strerror(errno);
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[0])) << strerror(errno);
    EXPECT_EQ(-1, aegis_namespace_prepare(contexts[0])); EXPECT_EQ(EALREADY, errno);
    pollfd ready = {peers[0], POLLIN, 0};
    EXPECT_EQ(0, poll(&ready, 1, 20));
    mounts[0] = aegis_namespace_base_mount(contexts[0], source);
    ASSERT_GE(mounts[0], 0) << strerror(errno);
    mapped_files(mounts[0], 10);
    EXPECT_EQ(0, poll(&ready, 1, 20));
    // Root in the initial namespace cannot override the mount's read-only bit.
    int write_fd = openat(mounts[0], "new.txt", O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    EXPECT_EQ(-1, write_fd); EXPECT_EQ(EROFS, errno);
    if (write_fd >= 0) close(write_fd);
    EXPECT_EQ(-1, unlinkat(mounts[0], "root.txt", 0)); EXPECT_EQ(EROFS, errno);
    ASSERT_EQ(0, aegis_namespace_resume(contexts[0]));
    aegis_namespace_probe actual = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(actual)), report(0, &actual));
    check(actual, 10);
    finish(0);
}

TEST_F(RuntimeNamespace, DetachedBaseRequiresThePrivateAnchorBeforeItCanBeCloned) {
    ASSERT_EQ(0, fixture(false));
    ASSERT_EQ(0, freeze_source(false));
    ASSERT_EQ(0, create(0, 10));
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[0]));
    EXPECT_EQ(-1, aegis_namespace_base_mount(contexts[0], source));
    EXPECT_EQ(EINVAL, errno);
    ASSERT_EQ(0, anchor_source()) << strerror(errno);
    mounts[0] = aegis_namespace_base_mount(contexts[0], source);
    ASSERT_GE(mounts[0], 0) << strerror(errno);
    mapped_files(mounts[0], 10);
    pollfd ready = {peers[0], POLLIN, 0};
    EXPECT_EQ(0, poll(&ready, 1, 20));
}

TEST_F(RuntimeNamespace, ReopenedRootDoesNotRetainDetachedMountOwnership) {
    ASSERT_EQ(0, fixture(false));
    ASSERT_EQ(0, freeze_source(false));
    int root = openat(source, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    ASSERT_GE(root, 0) << strerror(errno);
    close(source);
    source = root;
    // Metadata is still readable, but closing the fsmount file description
    // dissolved its anonymous namespace. This reproduces the production bug.
    struct stat st;
    ASSERT_EQ(0, fstat(source, &st));
    EXPECT_EQ(-1, aegis_namespace_attach_base(source));
    EXPECT_EQ(EINVAL, errno);
}

TEST_F(RuntimeNamespace, VerifiedImageRetainsOwnershipThroughAttachmentAndCloning) {
    // Exercise the real producer and the immutable image, including its hash,
    // loop device and SELinux-label checks. An already attached tmpfs fixture
    // cannot catch a prematurely closed fsmount owner in aegis_base_open().
    source = aegis_base_open();
    ASSERT_GE(source, 0) << strerror(errno);
    ASSERT_EQ(0, anchor_source()) << strerror(errno);
    EXPECT_NE(0, fcntl(source, F_GETFL) & O_PATH);
    EXPECT_NE(0, fcntl(source, F_GETFD) & FD_CLOEXEC);
    struct stat original;
    struct statvfs flags;
    ASSERT_EQ(0, fstat(source, &original));
    ASSERT_EQ(0, fstatvfs(source, &flags));
    const unsigned long restricted = ST_RDONLY | ST_NOSUID | ST_NODEV | ST_NOEXEC;
    EXPECT_EQ(restricted, flags.f_flag & restricted);
    ASSERT_EQ(0, create(0, 10));
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[0]));
    mounts[0] = aegis_namespace_base_mount(contexts[0], source);
    ASSERT_GE(mounts[0], 0) << strerror(errno);
    struct stat mapped;
    ASSERT_EQ(0, fstat(mounts[0], &mapped));
    EXPECT_EQ(original.st_dev, mapped.st_dev);
    EXPECT_EQ(original.st_ino, mapped.st_ino);
    EXPECT_EQ(1005000u, mapped.st_uid);
    EXPECT_EQ(1005000u, mapped.st_gid);
    ASSERT_EQ(0, fstatvfs(mounts[0], &flags));
    EXPECT_EQ(static_cast<unsigned long>(ST_RDONLY | ST_NOSUID | ST_NODEV),
              flags.f_flag & restricted);
    int writable = openat(mounts[0], "aegis-must-remain-readonly", O_CREAT | O_EXCL | O_WRONLY, 0600);
    int saved = errno;
    if (writable >= 0) close(writable);
    EXPECT_EQ(-1, writable);
    EXPECT_EQ(EROFS, saved);
    // The probe is a trusted Android fixture, not execution of GNU userspace.
    ASSERT_EQ(0, aegis_namespace_resume(contexts[0]));
    aegis_namespace_probe actual = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(actual)), report(0, &actual));
    check(actual, 10);
    finish(0);
}

TEST_F(RuntimeNamespace, TwoViewsKeepSharedInodesWithSeparateUserOwnership) {
    ASSERT_EQ(0, fixture(true));
    ASSERT_EQ(0, create(0, 10)); ASSERT_EQ(0, create(1, 11));
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[0]));
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[1]));
    mounts[0] = aegis_namespace_base_mount(contexts[0], source);
    mounts[1] = aegis_namespace_base_mount(contexts[1], source);
    ASSERT_GE(mounts[0], 0); ASSERT_GE(mounts[1], 0);
    mapped_files(mounts[0], 10); mapped_files(mounts[1], 11);
    struct stat first, second;
    ASSERT_EQ(0, fstatat(mounts[0], "user.txt", &first, 0));
    ASSERT_EQ(0, fstatat(mounts[1], "user.txt", &second, 0));
    EXPECT_EQ(first.st_dev, second.st_dev); EXPECT_EQ(first.st_ino, second.st_ino);
    EXPECT_NE(first.st_uid, second.st_uid); EXPECT_NE(first.st_gid, second.st_gid);
    ASSERT_EQ(0, aegis_namespace_stop(contexts[0]));
    aegis_child_exit result = {};
    ASSERT_EQ(0, aegis_namespace_wait(contexts[0], 5000, &result));
    close(mounts[0]); mounts[0] = -1;
    // A second context and the original base remain intact after closing one view.
    mapped_files(mounts[1], 11);
    ASSERT_EQ(0, aegis_namespace_resume(contexts[1]));
    aegis_namespace_probe actual = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(actual)), report(1, &actual));
    finish(1);
}

TEST_F(RuntimeNamespace, WritableOrNonDirectorySourcesAreRefusedWithoutMutationOrLeaks) {
    ASSERT_EQ(0, fixture(false));
    EXPECT_EQ(-1, aegis_namespace_attach_base(source)); EXPECT_EQ(EPERM, errno);
    ASSERT_EQ(0, create(0, 10)); ASSERT_EQ(0, aegis_namespace_prepare(contexts[0]));
    int before = descriptors();
    ASSERT_GT(before, 0);
    for (unsigned i = 0; i < 16; i++) {
        EXPECT_EQ(-1, aegis_namespace_base_mount(contexts[0], source)); EXPECT_EQ(EPERM, errno);
        EXPECT_EQ(-1, aegis_namespace_base_mount(contexts[0], setup)); EXPECT_EQ(EPERM, errno);
        EXPECT_EQ(-1, aegis_namespace_base_mount(contexts[0], -1)); EXPECT_EQ(EBADF, errno);
    }
    EXPECT_EQ(before, descriptors());
    struct statvfs flags;
    ASSERT_EQ(0, fstatvfs(source, &flags)); EXPECT_EQ(0u, flags.f_flag & ST_RDONLY);
    struct stat st;
    ASSERT_EQ(0, fstat(source, &st)); EXPECT_EQ(0u, st.st_uid); EXPECT_EQ(0u, st.st_gid);
    ASSERT_EQ(0, freeze_source());
    mounts[0] = aegis_namespace_base_mount(contexts[0], source);
    ASSERT_GE(mounts[0], 0);
    mapped_files(mounts[0], 10);
}

TEST_F(RuntimeNamespace, MissingMappingsAndStoppedChildrenCannotProvideBaseViews) {
    ASSERT_EQ(0, fixture(true));
    ASSERT_EQ(0, create(0, 10));
    EXPECT_EQ(-1, aegis_namespace_base_mount(contexts[0], source)); EXPECT_EQ(EAGAIN, errno);
    EXPECT_EQ(-1, aegis_namespace_home_mount(contexts[0], 0)); EXPECT_EQ(EAGAIN, errno);
    EXPECT_EQ(-1, aegis_namespace_devices_mount(contexts[0])); EXPECT_EQ(EAGAIN, errno);
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[0]));
    ASSERT_EQ(0, aegis_namespace_stop(contexts[0]));
    aegis_child_exit result = {};
    ASSERT_EQ(0, aegis_namespace_wait(contexts[0], 5000, &result));
    int before = descriptors();
    EXPECT_EQ(-1, aegis_namespace_base_mount(contexts[0], source)); EXPECT_EQ(EALREADY, errno);
    EXPECT_EQ(-1, aegis_namespace_home_mount(contexts[0], 0)); EXPECT_EQ(EALREADY, errno);
    EXPECT_EQ(-1, aegis_namespace_devices_mount(contexts[0])); EXPECT_EQ(EALREADY, errno);
    EXPECT_EQ(before, descriptors());
}

TEST_F(RuntimeNamespace, PrivateDevicesAreWhitelistedMappedAndMetadataIsReadonly) {
    ASSERT_EQ(0, create(0, 10));
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[0]));
    mounts[0] = aegis_namespace_devices_mount(contexts[0]);
    ASSERT_GE(mounts[0], 0) << strerror(errno);
    struct stat root;
    ASSERT_EQ(0, fstat(mounts[0], &root));
    EXPECT_EQ(1005000u, root.st_uid); EXPECT_EQ(1005000u, root.st_gid);
    EXPECT_EQ(static_cast<mode_t>(S_IFDIR | 0755), root.st_mode);
    struct statvfs flags;
    ASSERT_EQ(0, fstatvfs(mounts[0], &flags));
    EXPECT_EQ(static_cast<unsigned long>(ST_RDONLY | ST_NOSUID | ST_NOEXEC),
              flags.f_flag & (ST_RDONLY | ST_NOSUID | ST_NOEXEC));
    EXPECT_EQ(0u, flags.f_flag & ST_NODEV);
    EXPECT_NE(0, fcntl(mounts[0], F_GETFD) & FD_CLOEXEC);

    int readable = openat(mounts[0], ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    ASSERT_GE(readable, 0);
    DIR* directory = fdopendir(readable);
    if (!directory) close(readable);
    ASSERT_NE(nullptr, directory);
    std::set<std::string> names;
    errno = 0;
    while (dirent* entry = readdir(directory)) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) names.insert(entry->d_name);
    }
    EXPECT_EQ(0, errno);
    closedir(directory);
    const std::set<std::string> expected = {
        "null", "zero", "full", "random", "urandom", "tty", "pts", "shm", "mqueue",
        "ptmx", "fd", "stdin", "stdout", "stderr",
    };
    EXPECT_EQ(expected, names);
    const struct { const char* name; unsigned major, minor; } devices[] = {
        {"null", 1, 3}, {"zero", 1, 5}, {"full", 1, 7},
        {"random", 1, 8}, {"urandom", 1, 9}, {"tty", 5, 0},
    };
    for (const auto& device : devices) {
        struct stat st;
        ASSERT_EQ(0, fstatat(mounts[0], device.name, &st, AT_SYMLINK_NOFOLLOW));
        EXPECT_EQ(static_cast<mode_t>(S_IFCHR | 0666), st.st_mode);
        EXPECT_EQ(1005000u, st.st_uid); EXPECT_EQ(1005000u, st.st_gid);
        EXPECT_EQ(makedev(device.major, device.minor), st.st_rdev);
    }
    const struct { const char* name; const char* target; } links[] = {
        {"ptmx", "pts/ptmx"}, {"fd", "/proc/self/fd"}, {"stdin", "/proc/self/fd/0"},
        {"stdout", "/proc/self/fd/1"}, {"stderr", "/proc/self/fd/2"},
    };
    for (const auto& link : links) {
        char target[64];
        ssize_t n = readlinkat(mounts[0], link.name, target, sizeof(target));
        ASSERT_GT(n, 0);
        EXPECT_EQ(std::string(link.target), std::string(target, static_cast<size_t>(n)));
    }
    int file = openat(mounts[0], "unexpected", O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    EXPECT_EQ(-1, file); EXPECT_EQ(EROFS, errno);
    if (file >= 0) close(file);
    EXPECT_EQ(-1, unlinkat(mounts[0], "null", 0)); EXPECT_EQ(EROFS, errno);
    EXPECT_EQ(-1, fchmodat(mounts[0], "null", 0600, 0)); EXPECT_EQ(EROFS, errno);
    file = openat(mounts[0], "null", O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    ASSERT_GE(file, 0);
    EXPECT_EQ(7, write(file, "fixture", 7));
    close(file);
    file = openat(mounts[0], "zero", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    ASSERT_GE(file, 0);
    char zero[4] = {1, 1, 1, 1};
    EXPECT_EQ(4, read(file, zero, sizeof(zero)));
    EXPECT_EQ(std::string(4, '\0'), std::string(zero, sizeof(zero)));
    close(file);
    // No probe or user code has executed while mounts were prepared.
    pollfd ready = {peers[0], POLLIN, 0};
    EXPECT_EQ(0, poll(&ready, 1, 20));
}

TEST_F(RuntimeNamespace, EachContextGetsIndependentDeviceStorageAndReferences) {
    ASSERT_EQ(0, create(0, 10)); ASSERT_EQ(0, create(1, 11));
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[0]));
    ASSERT_EQ(0, aegis_namespace_prepare(contexts[1]));
    int before = descriptors();
    mounts[0] = aegis_namespace_devices_mount(contexts[0]);
    mounts[1] = aegis_namespace_devices_mount(contexts[1]);
    ASSERT_GE(mounts[0], 0); ASSERT_GE(mounts[1], 0);
    EXPECT_EQ(before + 2, descriptors());
    struct stat a, b;
    ASSERT_EQ(0, fstat(mounts[0], &a)); ASSERT_EQ(0, fstat(mounts[1], &b));
    EXPECT_EQ(1005000u, a.st_uid); EXPECT_EQ(1105000u, b.st_uid);
    EXPECT_TRUE(a.st_dev != b.st_dev || a.st_ino != b.st_ino);
    ASSERT_EQ(0, aegis_namespace_stop(contexts[0]));
    aegis_child_exit result = {};
    ASSERT_EQ(0, aegis_namespace_wait(contexts[0], 5000, &result));
    int after_stop = descriptors();
    close(mounts[0]); mounts[0] = -1;
    int file = openat(mounts[1], "null", O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    ASSERT_GE(file, 0);
    EXPECT_EQ(7, write(file, "fixture", 7));
    close(file);
    close(mounts[1]); mounts[1] = -1;
    EXPECT_EQ(after_stop - 2, descriptors());
}

}  // namespace
