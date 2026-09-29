// Compile ONLY on aegis-build; execute ONLY in local Android QEMU.
// Detached, unencrypted tmpfs fixtures test rejection paths and private layout
// construction, NOT successful AOSP provisioning, authentication, encryption,
// key removal or CE isolation.
#include "ce_private.h"

#include <gtest/gtest.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/mount.h>
#include <private/android_filesystem_config.h>
#include <set>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
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

class RuntimeCe : public ::testing::Test {
 protected:
    int root = -1;
    void SetUp() override {
        ASSERT_EQ(0u, getuid());
        int fs = static_cast<int>(syscall(SYS_fsopen, "tmpfs", FSOPEN_CLOEXEC));
        ASSERT_GE(fs, 0);
        int result = static_cast<int>(syscall(SYS_fsconfig, fs, FSCONFIG_SET_STRING, "mode", "0700", 0));
        if (result == 0) result = static_cast<int>(syscall(SYS_fsconfig, fs, FSCONFIG_SET_STRING, "size", "1048576", 0));
        if (result == 0) result = static_cast<int>(syscall(SYS_fsconfig, fs, FSCONFIG_CMD_CREATE, nullptr, nullptr, 0));
        int tree = -1;
        if (result == 0) tree = static_cast<int>(syscall(SYS_fsmount, fs, FSMOUNT_CLOEXEC, 0u));
        if (tree >= 0) {
            root = aegis_ce_open_directory(tree, ".");
            close(tree);
        }
        close(fs);
        ASSERT_EQ(0, result);
        ASSERT_GE(root, 0);
    }
    void TearDown() override { if (root >= 0) close(root); }
};

TEST_F(RuntimeCe, NewPrivateHomeHasExactOwnedDirectoriesAndIsNotReinitialized) {
    const std::set<std::string> expected = {
        "Desktop", "Documents", "Downloads", "Pictures", "Videos", "Music", "Books",
        ".config", ".local", ".cache",
    };
    int before = fd_count();
    ASSERT_GT(before, 0);
    ASSERT_EQ(0, aegis_ce_create_home_layout(root, 10));
    EXPECT_EQ(before, fd_count());
    int scan = aegis_ce_open_directory(root, ".");
    ASSERT_GE(scan, 0);
    DIR* directory = fdopendir(scan);
    ASSERT_NE(nullptr, directory);
    std::set<std::string> actual;
    while (dirent* entry = readdir(directory)) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        actual.insert(name);
        struct stat st;
        ASSERT_EQ(0, fstatat(root, name.c_str(), &st, AT_SYMLINK_NOFOLLOW));
        EXPECT_EQ(static_cast<mode_t>(S_IFDIR | 0700), st.st_mode);
        EXPECT_EQ(1007500u, st.st_uid);
        EXPECT_EQ(1007500u, st.st_gid);
    }
    EXPECT_EQ(0, closedir(directory));
    EXPECT_EQ(expected, actual);
    // Simulate a user's later choice. Reinitialization must not repair it.
    ASSERT_EQ(0, fchmodat(root, "Books", 0750, 0));
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(-1, aegis_ce_create_home_layout(root, 10)); EXPECT_EQ(EEXIST, errno);
    }
    struct stat st;
    ASSERT_EQ(0, fstatat(root, "Books", &st, AT_SYMLINK_NOFOLLOW));
    EXPECT_EQ(static_cast<mode_t>(S_IFDIR | 0750), st.st_mode);
    EXPECT_EQ(before, fd_count());
}

TEST_F(RuntimeCe, HomeLayoutRejectsExistingFileAndSymlinkWithoutFollowingOrOverwriting) {
    int sentinel = openat(root, "keep", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    ASSERT_GE(sentinel, 0);
    const char payload[] = "existing private user bytes";
    ASSERT_EQ(static_cast<ssize_t>(sizeof(payload)), write(sentinel, payload, sizeof(payload)));
    ASSERT_EQ(0, symlinkat("keep", root, "Books"));
    int before = fd_count();
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(-1, aegis_ce_create_home_layout(root, 10)); EXPECT_EQ(EEXIST, errno);
    }
    struct stat st;
    ASSERT_EQ(0, fstatat(root, "Books", &st, AT_SYMLINK_NOFOLLOW));
    EXPECT_TRUE(S_ISLNK(st.st_mode));
    char data[sizeof(payload)] = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(data)), pread(sentinel, data, sizeof(data), 0));
    EXPECT_EQ(0, memcmp(payload, data, sizeof(payload)));
    EXPECT_EQ(-1, fstatat(root, "Desktop", &st, AT_SYMLINK_NOFOLLOW)); EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(before, fd_count());
    close(sentinel);
}

TEST_F(RuntimeCe, HomeLayoutRejectsInvalidIdentityAndNonPrivateStagingWithoutMutation) {
    int before = fd_count();
    for (uint32_t user : {0u, 9u, 21473u, UINT32_MAX}) {
        EXPECT_EQ(-1, aegis_ce_create_home_layout(root, user)); EXPECT_EQ(EINVAL, errno);
    }
    ASSERT_EQ(0, fchmod(root, 0755));
    EXPECT_EQ(-1, aegis_ce_create_home_layout(root, 10)); EXPECT_EQ(EPERM, errno);
    ASSERT_EQ(0, fchmod(root, 0700));
    ASSERT_EQ(0, fchown(root, 1007500, 1007500));
    EXPECT_EQ(-1, aegis_ce_create_home_layout(root, 10)); EXPECT_EQ(EPERM, errno);
    struct stat st;
    EXPECT_EQ(-1, fstatat(root, "Books", &st, AT_SYMLINK_NOFOLLOW)); EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(before, fd_count());
}

TEST_F(RuntimeCe, HomeLayoutUsesTheActualMappedOwnerForEachPersonalUser) {
    int before = fd_count();
    for (uint32_t user : {10u, 11u, 21472u}) {
        const std::string name = std::to_string(user);
        ASSERT_EQ(0, mkdirat(root, name.c_str(), 0700));
        int home = aegis_ce_open_directory(root, name.c_str());
        ASSERT_GE(home, 0);
        ASSERT_EQ(0, aegis_ce_create_home_layout(home, user));
        struct stat st;
        ASSERT_EQ(0, fstatat(home, "Documents", &st, AT_SYMLINK_NOFOLLOW));
        EXPECT_EQ(user * 100000u + 7500u, st.st_uid);
        EXPECT_EQ(st.st_uid, st.st_gid);
        close(home);
    }
    EXPECT_EQ(before, fd_count());
}

TEST_F(RuntimeCe, MissingOrNonCanonicalSerialIsNeverRepairedOrAccepted) {
    EXPECT_EQ(-1, aegis_ce_require_serial(root, 1)); EXPECT_EQ(ENODATA, errno);
    EXPECT_EQ(-1, fgetxattr(root, "user.serial", nullptr, 0)); EXPECT_EQ(ENODATA, errno);
    const std::string bad[] = {"", "01", "-1", "+1", "1\n", " 1", std::string("1\0", 2),
                               "2147483648", std::string(100, '1')};
    for (const auto& value : bad) {
        ASSERT_EQ(0, fsetxattr(root, "user.serial", value.data(), value.size(), 0));
        EXPECT_EQ(-1, aegis_ce_require_serial(root, 1));
        char actual[128];
        ssize_t n = fgetxattr(root, "user.serial", actual, sizeof(actual));
        ASSERT_EQ(static_cast<ssize_t>(value.size()), n);
        EXPECT_EQ(value, std::string(actual, static_cast<size_t>(n)));
    }
    for (uint32_t serial : {0u, 1u, static_cast<uint32_t>(INT32_MAX)}) {
        std::string text = std::to_string(serial);
        ASSERT_EQ(0, fsetxattr(root, "user.serial", text.data(), text.size(), 0));
        EXPECT_EQ(0, aegis_ce_require_serial(root, serial));
    }
    EXPECT_EQ(-1, aegis_ce_require_serial(root, UINT32_MAX)); EXPECT_EQ(EINVAL, errno);
}

TEST_F(RuntimeCe, ReusedNumberWithAnotherSerialIsRejectedWithoutMutation) {
    ASSERT_EQ(0, fsetxattr(root, "user.serial", "120", 3, XATTR_CREATE));
    EXPECT_EQ(-1, aegis_ce_require_serial(root, 121)); EXPECT_EQ(ESTALE, errno);
    EXPECT_EQ(0, aegis_ce_require_serial(root, 120));
    EXPECT_EQ(-1, aegis_ce_require_serial(root, 12)); EXPECT_EQ(ESTALE, errno);
}

TEST_F(RuntimeCe, DirectoryWalkRejectsSymlinksEscapesAndNonDirectories) {
    ASSERT_EQ(0, mkdirat(root, "real", 0700));
    ASSERT_EQ(0, symlinkat("real", root, "link"));
    ASSERT_EQ(0, symlinkat("/data", root, "outside"));
    ASSERT_EQ(0, mkfifoat(root, "pipe", 0600));
    int before = fd_count();
    ASSERT_GT(before, 0);
    for (int i = 0; i < 16; i++) {
        for (const char* path : {"link", "link/child", "outside", "../data", "/data", "pipe"}) {
            int fd = aegis_ce_open_directory(root, path);
            EXPECT_EQ(-1, fd) << path;
            if (fd >= 0) close(fd);
        }
    }
    int real = aegis_ce_open_directory(root, "real");
    ASSERT_GE(real, 0);
    EXPECT_NE(0, fcntl(real, F_GETFD) & FD_CLOEXEC);
    close(real);
    EXPECT_EQ(before, fd_count());
}

TEST_F(RuntimeCe, PlausibleDirectoryNamesAndSerialDoNotSubstituteForEncryption) {
    ASSERT_EQ(0, mkdirat(root, "system_ce", 0700));
    ASSERT_EQ(0, mkdirat(root, "misc_ce", 0700));
    ASSERT_EQ(0, mkdirat(root, "system_ce/10", 0700));
    ASSERT_EQ(0, mkdirat(root, "misc_ce/10", 0700));
    int system = aegis_ce_open_directory(root, "system_ce/10");
    int misc = aegis_ce_open_directory(root, "misc_ce/10");
    ASSERT_GE(system, 0); ASSERT_GE(misc, 0);
    EXPECT_EQ(0, fchown(system, AID_SYSTEM, AID_SYSTEM));
    EXPECT_EQ(0, fchmod(system, 0770));
    EXPECT_EQ(0, fsetxattr(system, "user.serial", "1234", 4, XATTR_CREATE));
    EXPECT_EQ(0, fchown(misc, AID_SYSTEM, AID_MISC));
    EXPECT_EQ(0, fchmod(misc, 01771));
    int before = fd_count();
    for (int i = 0; i < 16; i++) {
        int home = aegis_ce_open_home(root, 10, 1234, 1);
        EXPECT_EQ(-1, home);
        if (home >= 0) close(home);
    }
    struct stat st;
    EXPECT_EQ(-1, fstatat(misc, "aegis", &st, AT_SYMLINK_NOFOLLOW)); EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(-1, fstatat(misc, ".aegis-preparing", &st, AT_SYMLINK_NOFOLLOW)); EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(before, fd_count());
    close(system); close(misc);
}

TEST_F(RuntimeCe, InvalidIdentitiesAndUnencryptedMountSourcesCannotLeakViews) {
    // Plausible mapped ordinary ownership must not make an unencrypted
    // directory acceptable as a CE mount source (user 10, app ID 7500).
    ASSERT_EQ(0, fchown(root, 1007500, 1007500));
    int before = fd_count();
    ASSERT_GT(before, 0);
    struct statvfs original, after;
    ASSERT_EQ(0, fstatvfs(root, &original));
    for (int i = 0; i < 16; i++) {
        for (uint32_t user : {0u, 9u, 21473u, UINT32_MAX}) {
            EXPECT_EQ(-1, aegis_ce_open_home(root, user, 1234, 1)); EXPECT_EQ(EINVAL, errno);
        }
        EXPECT_EQ(-1, aegis_ce_open_home(root, 10, UINT32_MAX, 1)); EXPECT_EQ(EINVAL, errno);
        EXPECT_EQ(-1, aegis_ce_open_home(root, 10, 1234, 2)); EXPECT_EQ(EINVAL, errno);
        int tree = aegis_ce_clone_home(root, 10);
        EXPECT_EQ(-1, tree);
        if (tree >= 0) close(tree);
    }
    EXPECT_EQ(0, fstatvfs(root, &after));
    EXPECT_EQ(original.f_flag, after.f_flag);
    EXPECT_EQ(before, fd_count());
}

}  // namespace
