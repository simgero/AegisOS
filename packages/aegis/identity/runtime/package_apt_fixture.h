// Test-only host preparation. Copies only the verified system_ext base into a
// new root-owned fixture. No selected generation, user home or CE path is used.
// Long copy/hash and RW mount happen BEFORE the 10-second namespace gate.
#ifndef AEGIS_PACKAGE_APT_FIXTURE_H
#define AEGIS_PACKAGE_APT_FIXTURE_H
#include "base_image.h"
#include <gtest/gtest.h>
#include <android-base/unique_fd.h>
#include <openssl/sha.h>
#include <array>
#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <linux/mount.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>

class AptImageFixture {
 public:
    android::base::unique_fd loop;
    std::string directory;
    bool passed = false;
    ~AptImageFixture() {
        // Caller must first close every returned mount/child ref. Never force
        // LOOP_CLR_FD or act on an unowned mount. Autoclear releases our loop.
        loop.reset();
        if (passed && !directory.empty()) {
            EXPECT_EQ(0, unlink((directory + "/candidate.ext4").c_str()));
            EXPECT_EQ(0, rmdir(directory.c_str()));
        } else if (!directory.empty()) {
            fprintf(stderr, "APT fixture preserved: %s\n", directory.c_str());
        }
    }
    int create() {
        using android::base::unique_fd;
        char path[] = "/data/local/tmp/aegis-apt-image-XXXXXX";
        char* created = mkdtemp(path);
        if (!created) return -1;
        directory = created;
        unique_fd receipt(open("/system_ext/etc/aegis/runtime/generation.json", O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
        unique_fd base(open("/system_ext/etc/aegis/runtime/base.ext4", O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
        unique_fd image(open((directory + "/candidate.ext4").c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600));
        if (receipt.get() < 0 || base.get() < 0 || image.get() < 0) return -1;
        std::array<char, 16385> json;
        ssize_t count = ::read(receipt.get(), json.data(), json.size());
        aegis_base_receipt expected = {};
        if (count <= 0 || aegis_base_parse_receipt(json.data(), count, &expected) < 0) return -1;
        struct stat original, backing;
        if (fstat(base.get(), &original) < 0 || original.st_size != static_cast<off_t>(expected.bytes)
                || original.st_uid || original.st_gid || !S_ISREG(original.st_mode)) return -1;
        SHA256_CTX hash;
        if (!SHA256_Init(&hash)) return -1;
        std::array<unsigned char, 65536> buffer;
        for (uint64_t offset = 0; offset < expected.bytes;) {
            size_t wanted = std::min<uint64_t>(buffer.size(), expected.bytes - offset);
            ssize_t got = pread(base.get(), buffer.data(), wanted, offset);
            if (got < 0 && errno == EINTR) continue;
            if (got <= 0 || !SHA256_Update(&hash, buffer.data(), got)) return -1;
            for (ssize_t copied = 0; copied < got;) {
                ssize_t wrote = pwrite(image.get(), buffer.data() + copied, got - copied, offset + copied);
                if (wrote < 0 && errno == EINTR) continue;
                if (wrote <= 0) return -1;
                copied += wrote;
            }
            offset += got;
        }
        unsigned char digest[SHA256_DIGEST_LENGTH];char actual[65];
        if (!SHA256_Final(digest, &hash)) return -1;
        for (size_t i = 0; i < sizeof(digest); ++i) snprintf(actual + i * 2, 3, "%02x", digest[i]);
        if (strcmp(actual, expected.sha256) || fsync(image.get()) < 0 || fstat(image.get(), &backing) < 0) return -1;
        unique_fd control(open("/dev/loop-control", O_RDWR | O_CLOEXEC | O_NOFOLLOW));
        if (control.get() < 0) return -1;
        for (unsigned attempt = 0; attempt < 16; ++attempt) {
            int number = ioctl(control.get(), LOOP_CTL_GET_FREE);
            if (number < 0 || number > 1048575) return -1;
            std::string name = "/dev/block/loop" + std::to_string(number);
            for (unsigned wait = 0; wait < 200; ++wait) {
                loop.reset(open(name.c_str(), O_RDWR | O_CLOEXEC | O_NOFOLLOW));
                if (loop.get() >= 0 || errno != ENOENT) break;
                usleep(10000);
            }
            if (loop.get() < 0) return -1;
            struct stat device;
            if (fstat(loop.get(), &device) < 0 || !S_ISBLK(device.st_mode)
                    || major(device.st_rdev) != 7 || minor(device.st_rdev) != static_cast<unsigned>(number)) return -1;
            loop_config config = {};
            config.fd = image.get();config.info.lo_flags = LO_FLAGS_AUTOCLEAR;
            if (ioctl(loop.get(), LOOP_CONFIGURE, &config) < 0) {
                if (errno == EBUSY) { loop.reset();continue; }
                return -1;
            }
            loop_info64 info = {};uint64_t bytes = 0;int readonly = -1;
            if (ioctl(loop.get(), LOOP_GET_STATUS64, &info) < 0 || ioctl(loop.get(), BLKGETSIZE64, &bytes) < 0
                    || ioctl(loop.get(), BLKROGET, &readonly) < 0
                    || info.lo_device != static_cast<uint64_t>(backing.st_dev) || info.lo_inode != backing.st_ino
                    || info.lo_flags != LO_FLAGS_AUTOCLEAR || info.lo_offset || info.lo_sizelimit
                    || bytes != expected.bytes || readonly) return -1;
            unique_fd fs(static_cast<int>(syscall(SYS_fsopen, "ext4", FSOPEN_CLOEXEC)));
            if (fs.get() < 0) return -1;
            std::string source = "/proc/self/fd/" + std::to_string(loop.get());
            if (syscall(SYS_fsconfig, fs.get(), FSCONFIG_SET_STRING, "source", source.c_str(), 0) < 0
                    || syscall(SYS_fsconfig, fs.get(), FSCONFIG_SET_STRING, "context", "u:object_r:aegis_runtime_base_file:s0", 0) < 0
                    || syscall(SYS_fsconfig, fs.get(), FSCONFIG_CMD_CREATE, nullptr, nullptr, 0) < 0) return -1;
            return static_cast<int>(syscall(SYS_fsmount, fs.get(), FSMOUNT_CLOEXEC,
                                           MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV | MOUNT_ATTR_NOEXEC));
        }
        errno = EBUSY;return -1;
    }
    static std::string read(int root, const char* name) {
        android::base::unique_fd fd(openat(root, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
        if (fd.get() < 0) return "<unavailable>";
        std::string result;
        char buffer[4096];
        for (;;) {
            ssize_t n = ::read(fd.get(), buffer, sizeof(buffer));
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return result;
            result.append(buffer, n);
            if (result.size() > 32768) return result + "<truncated>";
        }
    }
};
#endif
