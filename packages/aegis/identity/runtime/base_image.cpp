#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "base_image.h"
#include <android-base/unique_fd.h>
#include <json/json.h>
#include <log/log.h>
#include <openssl/sha.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <linux/magic.h>
#include <linux/mount.h>
#include <linux/openat2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <memory>
#include <set>
#include <string>

namespace {
using android::base::unique_fd;
constexpr uint64_t kImageBytes = 268435456;
constexpr size_t kMaxReceipt = 16384;
constexpr char kImageLabel[] = "u:object_r:aegis_runtime_image_file:s0";
constexpr char kBaseLabel[] = "u:object_r:aegis_runtime_base_file:s0";
int fail(int error) { errno = error; return -1; }

// Fixed startup phases only: never log credentials, paths supplied by a caller,
// image contents or protocol payloads. Preserve the original failure errno.
struct FailureTrace {
    const char* phase;
    bool complete = false;
    ~FailureTrace() {
        int saved = errno;
        if (!complete) __android_log_print(ANDROID_LOG_ERROR, "AegisRuntimeBase",
                "AEGIS_RUNTIME_BASE_FAILED: %s errno=%d", phase, saved);
        errno = saved;
    }
};

int64_t monotonic_ns() {
    timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    return static_cast<int64_t>(now.tv_sec) * 1000000000 + now.tv_nsec;
}

int open_loop_until(const char* path, int64_t deadline) {
    // LOOP_CTL_GET_FREE can allocate a kernel device before ueventd has created
    // its node. Wait only for that exact node, within one shared deadline for
    // all allocation attempts. Never create a node or accept a fallback path.
    for (;;) {
        int64_t now = monotonic_ns();
        if (now < 0) return -1;
        if (now >= deadline) return fail(ETIMEDOUT);
        int fd = open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
        if (fd >= 0) return fd;
        if (errno != ENOENT && errno != EINTR) return -1;
        timespec pause = {.tv_sec = 0, .tv_nsec = static_cast<long>(
                std::min<int64_t>(10000000, deadline - now))};
        if (nanosleep(&pause, nullptr) < 0 && errno != EINTR) return -1;
    }
}

bool bounded_nesting(const char* text, size_t length) {
    unsigned depth = 0;
    bool string = false, escaped = false;
    for (size_t i = 0; i < length; ++i) {
        char c = text[i];
        if (!c) return false;
        if (string) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') string = false;
        } else if (c == '"') string = true;
        // JsonCpp's object parser can skip comments after values even when
        // allowComments=false. JSON has no slash token outside a string.
        else if (c == '/') return false;
        else if (c == '{' || c == '[') { if (++depth > 8) return false; }
        else if (c == '}' || c == ']') { if (!depth) return false; --depth; }
    }
    return !depth && !string;
}

bool hex(const Json::Value& value) {
    if (!value.isString()) return false;
    std::string text = value.asString();
    return text.size() == 64 && text.find_first_not_of("0123456789abcdef") == std::string::npos;
}

int label(int fd, const char* expected) {
    char text[128];
    ssize_t count = fgetxattr(fd, "security.selinux", text, sizeof(text));
    if (count < 0) return -1;
    size_t size = strlen(expected);
    return (count == static_cast<ssize_t>(size) || count == static_cast<ssize_t>(size + 1))
            && !memcmp(text, expected, size)
            && (count == static_cast<ssize_t>(size) || !text[size]) ? 0 : fail(EPERM);
}

int system_file(int parent, const char* path, const struct stat& anchor) {
    open_how how = {};
    how.flags = O_RDONLY | O_CLOEXEC | O_NOFOLLOW;
    how.resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_XDEV;
    unique_fd fd(static_cast<int>(syscall(SYS_openat2, parent, path, &how, sizeof(how))));
    if (fd.get() < 0) return -1;
    struct stat st;
    if (fstat(fd.get(), &st) < 0) return -1;
    if (!S_ISREG(st.st_mode) || st.st_dev != anchor.st_dev || st.st_uid || st.st_gid
            || (st.st_mode & 07133) || !(st.st_mode & S_IRUSR) || st.st_nlink != 1)
        return fail(EPERM);
    if (label(fd.get(), kImageLabel) < 0) return -1;
    return fd.release();
}

int verified_image(int fd, const aegis_base_receipt& receipt, struct stat* output) {
    if (fstat(fd, output) < 0) return -1;
    if (output->st_size != static_cast<off_t>(receipt.bytes)) return fail(ESTALE);
    SHA256_CTX hash;
    if (!SHA256_Init(&hash)) return fail(EIO);
    std::array<unsigned char, 65536> buffer;
    uint64_t offset = 0;
    while (offset < receipt.bytes) {
        size_t wanted = static_cast<size_t>(std::min<uint64_t>(buffer.size(), receipt.bytes - offset));
        ssize_t count = pread(fd, buffer.data(), wanted, static_cast<off_t>(offset));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return count < 0 ? -1 : fail(EIO);
        if (!SHA256_Update(&hash, buffer.data(), static_cast<size_t>(count))) return fail(EIO);
        offset += static_cast<uint64_t>(count);
    }
    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (!SHA256_Final(digest, &hash)) return fail(EIO);
    char actual[65];
    for (size_t i = 0; i < sizeof(digest); ++i) snprintf(actual + i * 2, 3, "%02x", digest[i]);
    struct stat after;
    if (fstat(fd, &after) < 0) return -1;
    if (strcmp(actual, receipt.sha256) || after.st_dev != output->st_dev
            || after.st_ino != output->st_ino || after.st_size != output->st_size
            || after.st_mtim.tv_sec != output->st_mtim.tv_sec
            || after.st_mtim.tv_nsec != output->st_mtim.tv_nsec) return fail(ESTALE);
    return 0;
}

int loop_for_image(int image, const struct stat& backing) {
    FailureTrace trace{"open loop control"};
    unique_fd control(open("/dev/loop-control", O_RDWR | O_CLOEXEC | O_NOFOLLOW));
    if (control.get() < 0) return -1;
    trace.phase = "validate loop control";
    struct stat st;
    if (fstat(control.get(), &st) < 0) return -1;
    if (!S_ISCHR(st.st_mode) || major(st.st_rdev) != 10 || minor(st.st_rdev) != 237
            || st.st_uid || (st.st_mode & 0022)) return fail(EPERM);
    int64_t now = monotonic_ns();
    if (now < 0) return -1;
    const int64_t deadline = now + 2000000000;
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        trace.phase = "allocate loop device";
        int number = ioctl(control.get(), LOOP_CTL_GET_FREE);
        if (number < 0) return -1;
        if (number > 1048575) return fail(EOVERFLOW);
        char path[64];
        snprintf(path, sizeof(path), "/dev/block/loop%d", number);
        trace.phase = "wait for loop node";
        unique_fd loop(open_loop_until(path, deadline));
        if (loop.get() < 0) return -1;
        trace.phase = "validate loop node";
        if (fstat(loop.get(), &st) < 0) return -1;
        if (!S_ISBLK(st.st_mode) || major(st.st_rdev) != 7
                || minor(st.st_rdev) != static_cast<unsigned>(number)
                || st.st_uid || (st.st_mode & 0022)) return fail(EPERM);
        loop_config config = {};
        config.fd = static_cast<uint32_t>(image);
        config.info.lo_flags = LO_FLAGS_READ_ONLY | LO_FLAGS_AUTOCLEAR;
        trace.phase = "configure loop image";
        if (ioctl(loop.get(), LOOP_CONFIGURE, &config) < 0) {
            if (errno == EBUSY) continue; // Never clear or adopt someone else's device.
            return -1;
        }
        loop_info64 actual = {};
        uint64_t bytes = 0;
        int readonly = 0;
        trace.phase = "verify configured loop image";
        if (ioctl(loop.get(), LOOP_GET_STATUS64, &actual) < 0
                || ioctl(loop.get(), BLKGETSIZE64, &bytes) < 0
                || ioctl(loop.get(), BLKROGET, &readonly) < 0) return -1;
        if (actual.lo_device != static_cast<uint64_t>(backing.st_dev)
                || actual.lo_inode != backing.st_ino || actual.lo_offset || actual.lo_sizelimit
                || actual.lo_number != static_cast<unsigned>(number)
                || actual.lo_flags != (LO_FLAGS_READ_ONLY | LO_FLAGS_AUTOCLEAR)
                || actual.lo_encrypt_type || actual.lo_encrypt_key_size
                || bytes != kImageBytes || readonly != 1) return fail(EPROTO);
        trace.complete = true;
        return loop.release();
    }
    return fail(EBUSY);
}
} // namespace

int aegis_base_parse_receipt(const char* json, size_t length, aegis_base_receipt* output) {
    if (!json || !output || !length || length > kMaxReceipt) return fail(EINVAL);
    if (!bounded_nesting(json, length)) return fail(EPROTO);
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    builder["allowComments"] = false;
    builder["allowTrailingCommas"] = false;
    builder["strictRoot"] = true;
    builder["failIfExtra"] = true;
    builder["rejectDupKeys"] = true;
    builder["allowSpecialFloats"] = false;
    builder["stackLimit"] = 16;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value value;
    std::string errors;
    if (!reader->parse(json, json + length, &value, &errors) || !value.isObject()) return fail(EPROTO);
    const auto names = value.getMemberNames();
    const std::set<std::string> expected = {"schema", "status", "generation", "recipe_input_sha256",
        "plan_sha256", "tool_sha256", "uuid", "image_sha256", "image_bytes",
        "repeated_build_identical", "scope"};
    if (std::set<std::string>(names.begin(), names.end()) != expected
            || (value["schema"].type() != Json::intValue && value["schema"].type() != Json::uintValue)
            || !value["schema"].isUInt() || value["schema"].asUInt() != 1
            || value["status"] != "BUILT_VERIFIED_NOT_MOUNTED"
            || (value["image_bytes"].type() != Json::intValue && value["image_bytes"].type() != Json::uintValue)
            || !value["image_bytes"].isUInt64() || value["image_bytes"].asUInt64() != kImageBytes
            || !hex(value["image_sha256"]) || value["generation"] != value["image_sha256"]
            || !hex(value["recipe_input_sha256"]) || !hex(value["plan_sha256"])
            || !value["repeated_build_identical"].isBool() || !value["repeated_build_identical"].asBool()
            || !value["uuid"].isString() || value["uuid"].asString().size() != 36
            || !value["scope"].isString() || !value["tool_sha256"].isObject()) return fail(EPROTO);
    const auto tools = value["tool_sha256"].getMemberNames();
    const std::set<std::string> expected_tools = {"mkuserimg_mke2fs", "mke2fs", "e2fsdroid", "e2fsck", "debugfs"};
    if (std::set<std::string>(tools.begin(), tools.end()) != expected_tools) return fail(EPROTO);
    for (const auto& name : tools) if (!hex(value["tool_sha256"][name])) return fail(EPROTO);
    std::string input = value["recipe_input_sha256"].asString();
    std::string uuid = input.substr(0, 8) + "-" + input.substr(8, 4) + "-" + input.substr(12, 4)
            + "-" + input.substr(16, 4) + "-" + input.substr(20, 12);
    if (value["uuid"].asString() != uuid) return fail(EPROTO);
    aegis_base_receipt checked = {};
    checked.bytes = kImageBytes;
    memcpy(checked.sha256, value["image_sha256"].asCString(), sizeof(checked.sha256));
    *output = checked;
    return 0;
}

int aegis_base_open_selection(int* verified_source,aegis_base_receipt* output) {
    if(!verified_source || *verified_source!=-1 || !output)return fail(EINVAL);
    FailureTrace trace{"initial credentials"};
    if (getuid() || geteuid() || getgid() || getegid()) return fail(EPERM);
    trace.phase = "open immutable system_ext";
    unique_fd anchor(open("/system_ext", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (anchor.get() < 0) return -1;
    struct stat st;
    struct statfs fs;
    struct statvfs flags;
    trace.phase = "validate immutable system_ext";
    if (fstat(anchor.get(), &st) < 0 || fstatfs(anchor.get(), &fs) < 0
            || fstatvfs(anchor.get(), &flags) < 0) return -1;
    // This target deliberately supports only the product's readonly EROFS
    // system_ext. Root/SELinux policy and verified system images are the trust
    // boundary; a self-consistent digest in a writable manifest is not enough.
    if (st.st_uid || st.st_gid || (st.st_mode & 0022) || fs.f_type != EROFS_SUPER_MAGIC_V1
            || !(flags.f_flag & ST_RDONLY)) return fail(EPERM);
    trace.phase = "open verified receipt";
    unique_fd receipt(system_file(anchor.get(), "etc/aegis/runtime/generation.json", st));
    if (receipt.get() < 0) return -1;
    trace.phase = "open verified base image";
    unique_fd image(system_file(anchor.get(), "etc/aegis/runtime/base.ext4", st));
    if (image.get() < 0) return -1;
    trace.phase = "read receipt";
    struct stat receipt_stat;
    if (fstat(receipt.get(), &receipt_stat) < 0) return -1;
    if (receipt_stat.st_size <= 0 || receipt_stat.st_size > static_cast<off_t>(kMaxReceipt))
        return fail(EOVERFLOW);
    std::array<char, kMaxReceipt> json;
    size_t used = 0, wanted = static_cast<size_t>(receipt_stat.st_size);
    while (used < wanted) {
        ssize_t count = pread(receipt.get(), json.data() + used, wanted - used, static_cast<off_t>(used));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return count < 0 ? -1 : fail(EIO);
        used += static_cast<size_t>(count);
    }
    aegis_base_receipt checked = {};
    struct stat backing;
    trace.phase = "parse receipt";
    if (aegis_base_parse_receipt(json.data(), used, &checked) < 0) return -1;
    trace.phase = "verify base bytes";
    if (verified_image(image.get(), checked, &backing) < 0) return -1;
    trace.phase = "readonly loop image";
    unique_fd loop(loop_for_image(image.get(), backing));
    if (loop.get() < 0) return -1;
    trace.phase = "open ext4 context";
    unique_fd context(static_cast<int>(syscall(SYS_fsopen, "ext4", FSOPEN_CLOEXEC)));
    if (context.get() < 0) return -1;
    char source[64];
    snprintf(source, sizeof(source), "/proc/self/fd/%d", loop.get());
    trace.phase = "configure readonly ext4 context";
    if (syscall(SYS_fsconfig, context.get(), FSCONFIG_SET_STRING, "source", source, 0) < 0
            || syscall(SYS_fsconfig, context.get(), FSCONFIG_SET_FLAG, "ro", nullptr, 0) < 0
            || syscall(SYS_fsconfig, context.get(), FSCONFIG_SET_FLAG, "noload", nullptr, 0) < 0
            || syscall(SYS_fsconfig, context.get(), FSCONFIG_SET_STRING, "context", kBaseLabel, 0) < 0)
        return -1;
    trace.phase = "create readonly ext4 superblock";
    if (syscall(SYS_fsconfig, context.get(), FSCONFIG_CMD_CREATE, nullptr, nullptr, 0) < 0) return -1;
    trace.phase = "detach readonly base mount";
    unique_fd mount(static_cast<int>(syscall(SYS_fsmount, context.get(), FSMOUNT_CLOEXEC,
        MOUNT_ATTR_RDONLY | MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV | MOUNT_ATTR_NOEXEC)));
    if (mount.get() < 0) return -1;
    struct stat loop_stat;
    trace.phase = "validate detached base mount";
    if (fstat(mount.get(), &st) < 0 || fstat(loop.get(), &loop_stat) < 0
            || fstatfs(mount.get(), &fs) < 0 || fstatvfs(mount.get(), &flags) < 0) return -1;
    constexpr unsigned long required = ST_RDONLY | ST_NOSUID | ST_NODEV | ST_NOEXEC;
    if (st.st_mode != (S_IFDIR | 0755) || st.st_uid || st.st_gid
            || st.st_dev != loop_stat.st_rdev || fs.f_type != EXT4_SUPER_MAGIC
            || (flags.f_flag & required) != required) return fail(EPROTO);
    // fsmount's file description owns the anonymous mount namespace. A newly
    // opened root only retains a path: closing the original fsmount description
    // dissolves that namespace, so a later move_mount would fail with EINVAL.
    // Use a readable descriptor only for xattrs; return the original owner.
    trace.phase = "open labeled detached root";
    unique_fd root(openat(mount.get(), ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (root.get() < 0 || label(root.get(), kBaseLabel) < 0) return -1;
    *verified_source=image.release();*output=checked;
    trace.complete = true;
    return mount.release();
}

int aegis_base_open(void) {
    int image=-1;aegis_base_receipt receipt={};
    int mount=aegis_base_open_selection(&image,&receipt),saved=errno;
    if(image>=0)close(image);
    errno=saved;return mount;
}
