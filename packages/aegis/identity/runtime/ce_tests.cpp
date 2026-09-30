// Compile ONLY on aegis-build; execute ONLY in local Android QEMU.
// Detached, unencrypted tmpfs fixtures test rejection paths and private layout
// construction, NOT successful AOSP provisioning, authentication, encryption,
// key removal or CE isolation.
#include "ce_private.h"
#include "ce_live_test.h"
#include "base_image.h"
#include "package_store.h"
#include <memory>

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

TEST_F(RuntimeCe, OptionalStoreLookupDoesNotTreatMissingAospRootsAsAnEmptyPrivateStore) {
    int before=fd_count(),store=-1;
    EXPECT_EQ(-1,aegis_ce_find_package_store(root,10,42,&store));EXPECT_EQ(ENOENT,errno);EXPECT_EQ(-1,store);
    EXPECT_EQ(-1,aegis_ce_find_package_store(root,0,42,&store));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(-1,aegis_ce_find_package_store(root,10,UINT32_MAX,&store));EXPECT_EQ(EINVAL,errno);
    store=root;EXPECT_EQ(-1,aegis_ce_find_package_store(root,10,42,&store));EXPECT_EQ(EINVAL,errno);EXPECT_EQ(root,store);
    struct stat st;EXPECT_EQ(-1,fstatat(root,"system_ce",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(before,fd_count());
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
        int packages = aegis_ce_open_packages(root, 10, 1234, 1);
        EXPECT_EQ(-1, packages);
        if (packages >= 0) close(packages);
        int selected=-1;EXPECT_EQ(-1,aegis_ce_find_package_store(root,10,1234,&selected));EXPECT_EQ(-1,selected);
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

TEST_F(RuntimeCe, PackageOwnerMetadataCannotSubstituteForEncryptedStorage) {
    ASSERT_EQ(0, fsetxattr(root, "user.aegis.owner", "1:10:42", 7, XATTR_CREATE));
    ASSERT_EQ(0, mkdirat(root, "store", 0700));
    ASSERT_EQ(0, mkdirat(root, "staging", 0700));
    int store = aegis_ce_open_directory(root, "store");
    int staging = aegis_ce_open_directory(root, "staging");
    ASSERT_GE(store, 0); ASSERT_GE(staging, 0);
    ASSERT_EQ(0, fsetxattr(store, "user.aegis.owner", "1:10:42", 7, XATTR_CREATE));
    ASSERT_EQ(0, fsetxattr(staging, "user.aegis.owner", "1:10:42", 7, XATTR_CREATE));
    int marker = openat(store, "keep", O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC, 0600);
    ASSERT_GE(marker, 0);
    ASSERT_EQ(4, write(marker, "keep", 4));
    int before = fd_count();
    for (unsigned i = 0; i < 16; i++) {
        int opened = aegis_ce_package_store(root, 10, 42);
        EXPECT_EQ(-1, opened); if (opened >= 0) close(opened);
        int job = aegis_ce_new_package_stage(root, 10, 42, i + 1);
        EXPECT_EQ(-1, job); if (job >= 0) close(job);
    }
    char data[4]; ASSERT_EQ(4, pread(marker, data, 4, 0)); EXPECT_EQ(0, memcmp(data, "keep", 4));
    int scan = aegis_ce_open_directory(staging, "."); ASSERT_GE(scan, 0);
    DIR* entries = fdopendir(scan); ASSERT_NE(nullptr, entries);
    while (auto* entry = readdir(entries))
        EXPECT_TRUE(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."));
    EXPECT_EQ(0, closedir(entries));
    EXPECT_EQ(before, fd_count());
    close(marker); close(store); close(staging);
}

TEST_F(RuntimeCe, InvalidPackageIdentitiesAndJobsAreRejectedBeforeCreatingStorage) {
    int before = fd_count();
    for (uint32_t user : {0u, 9u, 21473u, UINT32_MAX}) {
        EXPECT_EQ(-1, aegis_ce_open_packages(root, user, 42, 1)); EXPECT_EQ(EINVAL, errno);
        EXPECT_EQ(-1, aegis_ce_package_store(root, user, 42)); EXPECT_EQ(EINVAL, errno);
        EXPECT_EQ(-1, aegis_ce_new_package_stage(root, user, 42, 1)); EXPECT_EQ(EINVAL, errno);
    }
    EXPECT_EQ(-1, aegis_ce_open_packages(root, 10, UINT32_MAX, 1)); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_ce_open_packages(root, 10, 42, 2)); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_ce_package_store(root, 10, UINT32_MAX)); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_ce_new_package_stage(root, 10, 42, 0)); EXPECT_EQ(EINVAL, errno);
    EXPECT_EQ(-1, aegis_ce_new_package_stage(root, 10, 42, UINT64_MAX)); EXPECT_EQ(EINVAL, errno);
    struct stat st;
    EXPECT_EQ(-1, fstatat(root, "packages", &st, AT_SYMLINK_NOFOLLOW)); EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(-1, fstatat(root, "staging", &st, AT_SYMLINK_NOFOLLOW)); EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(before, fd_count());
}

// Explicit integration suite; not run in the empty-user component fixture.
// The host driver pins a fresh full-image profile, obtains the identities from
// AOSP, authenticates through the CLI and starts the production runtime first.
// This native process never changes credentials, CE policy or AOSP user state.
class DISABLED_RuntimeCeAosp : public ::testing::Test {
 protected:
    aegis_ce_test::Identity identity;
    android::base::unique_fd data,area;
    aegis::PackageGeneration generation;
    void SetUp() override {
        ASSERT_TRUE(aegis_ce_test::Read(&identity));
        data.reset(open("/data",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));ASSERT_TRUE(data.ok());
    }
    void Area() {
        area.reset(aegis_ce_open_packages(data.get(),identity.user,identity.serial,0));
        ASSERT_TRUE(area.ok())<<strerror(errno);
        char label[128]={};ssize_t n=fgetxattr(area.get(),"security.selinux",label,sizeof(label));
        ASSERT_GT(n,0);ASSERT_LT(n,static_cast<ssize_t>(sizeof(label)));
        if(label[n-1]==0)--n;
        ASSERT_EQ("u:object_r:aegis_package_private_file:s0",std::string(label,n));
    }
    void Base() {
        android::base::unique_fd receipt(open("/system_ext/etc/aegis/runtime/generation.json",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
        ASSERT_TRUE(receipt.ok());std::array<char,16385> text;
        ssize_t n=read(receipt.get(),text.data(),text.size());ASSERT_GT(n,0);
        aegis_base_receipt expected={};ASSERT_EQ(0,aegis_base_parse_receipt(text.data(),n,&expected));
        generation={expected.sha256,expected.sha256,expected.bytes};
    }
    void Selection(aegis::PackageStore* store) {
        aegis::PackageGeneration found;
        android::base::unique_fd image(store->Current(&found));ASSERT_TRUE(image.ok())<<strerror(errno);
        EXPECT_EQ(generation.image_sha256,found.image_sha256);
        EXPECT_EQ(generation.shared_base_sha256,found.shared_base_sha256);
        EXPECT_EQ(generation.bytes,found.bytes);
    }
};
TEST_F(DISABLED_RuntimeCeAosp, PublishesCompleteBaseInAlreadyProvisionedAospCe) {
    Area();ASSERT_FALSE(HasFatalFailure());Base();ASSERT_FALSE(HasFatalFailure());
    android::base::unique_fd store_fd(aegis_ce_package_store(area.get(),identity.user,identity.serial));
    ASSERT_TRUE(store_fd.ok());
    aegis::PackageOwner owner={true,identity.user,identity.serial};
    std::unique_ptr<aegis::PackageStore> store(aegis::PackageStore::Open(store_fd.get(),owner,true));
    ASSERT_NE(nullptr,store.get())<<strerror(errno);
    android::base::unique_fd source(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    ASSERT_TRUE(source.ok());std::atomic_bool cancel{false};
    ASSERT_EQ(aegis::PackagePublish::Confirmed,store->Publish(nullptr,source.get(),generation,cancel))<<strerror(errno);
    Selection(store.get());ASSERT_FALSE(HasFatalFailure());
    // The store's identity check is independent of the directory opener.
    store.reset();
    for(auto other:{aegis::PackageOwner{true,identity.user,identity.serial+1},
                    aegis::PackageOwner{false,0,0}}) {
        std::unique_ptr<aegis::PackageStore> wrong(aegis::PackageStore::Open(store_fd.get(),other,false));
        EXPECT_EQ(nullptr,wrong.get());EXPECT_EQ(ESTALE,errno);
    }
}
TEST_F(DISABLED_RuntimeCeAosp, ReopensExactGenerationAndRejectsAnotherSerial) {
    Area();ASSERT_FALSE(HasFatalFailure());Base();ASSERT_FALSE(HasFatalFailure());
    android::base::unique_fd store_fd(aegis_ce_package_store(area.get(),identity.user,identity.serial));
    ASSERT_TRUE(store_fd.ok());
    std::unique_ptr<aegis::PackageStore> store(aegis::PackageStore::Open(store_fd.get(),{true,identity.user,identity.serial},false));
    ASSERT_NE(nullptr,store.get())<<strerror(errno);Selection(store.get());ASSERT_FALSE(HasFatalFailure());
    int before=fd_count();
    for(unsigned i=0;i<16;++i) {
        EXPECT_EQ(-1,aegis_ce_open_packages(data.get(),identity.user,identity.serial+1,0));EXPECT_EQ(ESTALE,errno);
        EXPECT_EQ(-1,aegis_ce_package_store(area.get(),identity.user,identity.serial+1));EXPECT_EQ(ESTALE,errno);
        EXPECT_EQ(-1,aegis_ce_new_package_stage(area.get(),identity.user,identity.serial+1,1));EXPECT_EQ(ESTALE,errno);
    }
    EXPECT_EQ(before,fd_count());
}
TEST_F(DISABLED_RuntimeCeAosp, LockedAospKeyCannotOpenOrProvisionPackageStorage) {
    int before=fd_count();
    for(unsigned i=0;i<16;++i) {
        EXPECT_EQ(-1,aegis_ce_open_packages(data.get(),identity.user,identity.serial,0));EXPECT_EQ(ENOKEY,errno);
        EXPECT_EQ(-1,aegis_ce_open_packages(data.get(),identity.user,identity.serial,1));EXPECT_EQ(ENOKEY,errno);
        int store=-1;EXPECT_EQ(-1,aegis_ce_find_package_store(data.get(),identity.user,identity.serial,&store));
        EXPECT_EQ(ENOKEY,errno);EXPECT_EQ(-1,store);
    }
    EXPECT_EQ(before,fd_count());
}

}  // namespace
