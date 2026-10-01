#include "package_store.h"

#include <android-base/unique_fd.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <linux/fs.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <array>
#include <algorithm>
#include <cstring>
#include <sstream>

using android::base::unique_fd;
namespace aegis {
namespace {
constexpr uint64_t kMaxImage = uint64_t{32} * 1024 * 1024 * 1024;
int Fail(int error) { errno = error; return -1; }
bool Hash(const std::string& text) {
    return text.size() == 64 && text.find_first_not_of("0123456789abcdef") == std::string::npos;
}
bool Owner(PackageOwner value) {
    return value.personal ? value.user_id >= 10 && value.user_id < 21473
            && value.serial <= INT32_MAX : value.user_id == 0 && value.serial == 0;
}
bool Generation(PackageOwner owner, const PackageGeneration& value) {
    return Hash(value.image_sha256) && value.bytes && value.bytes <= kMaxImage
            && (owner.personal ? Hash(value.shared_base_sha256) : value.shared_base_sha256.empty());
}
std::string OwnerText(PackageOwner owner) {
    return "AEGIS-PACKAGE-STORE 1\n" + std::string(owner.personal ? "user " : "all ")
            + std::to_string(owner.user_id) + " " + std::to_string(owner.serial) + "\n";
}
std::string Record(PackageOwner owner, const PackageGeneration& generation) {
    return OwnerText(owner) + generation.image_sha256 + "\n"
            + (owner.personal ? generation.shared_base_sha256 : "-") + "\n"
            + std::to_string(generation.bytes) + "\n";
}
int OpenAt(int directory, const char* name, int flags, mode_t mode = 0) {
    open_how how = {};
    how.flags = flags | O_NOFOLLOW | O_CLOEXEC;
    how.mode = mode;
    how.resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_XDEV;
    return syscall(SYS_openat2, directory, name, &how, sizeof(how));
}
bool Metadata(int fd, mode_t type, mode_t permissions, bool single_link) {
    struct stat st;
    if (fstat(fd, &st) < 0) return false;
    if ((st.st_mode & S_IFMT) != type || (st.st_mode & 07777) != permissions
            || st.st_uid || st.st_gid || !st.st_nlink || (single_link && st.st_nlink != 1)) {
        Fail(EPERM); return false;
    }
    for (const char* name : {"system.posix_acl_access", "system.posix_acl_default"}) {
        if (fgetxattr(fd, name, nullptr, 0) >= 0) { Fail(EPERM); return false; }
        if (errno != ENODATA && errno != EOPNOTSUPP) return false;
    }
    return true;
}
bool Named(int directory, const char* name, int fd) {
    struct stat actual, expected;
    if (fstatat(directory, name, &actual, AT_SYMLINK_NOFOLLOW) < 0 || fstat(fd, &expected) < 0)
        return false;
    if (actual.st_dev != expected.st_dev || actual.st_ino != expected.st_ino) {
        Fail(ESTALE); return false;
    }
    return true;
}
bool Write(int fd, const void* data, size_t size) {
    auto bytes = static_cast<const char*>(data);
    while (size) {
        ssize_t written = write(fd, bytes, size);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { if (!written) Fail(EIO); return false; }
        bytes += written; size -= written;
    }
    return true;
}
bool ReadSmall(int fd, std::string* output) {
    char buffer[512]; ssize_t size;
    do { size = pread(fd, buffer, sizeof(buffer), 0); } while (size < 0 && errno == EINTR);
    if (size < 0) return false;
    if (static_cast<size_t>(size) == sizeof(buffer)) { Fail(EOVERFLOW); return false; }
    *output = std::string(buffer, size);
    return true;
}
std::string Hex(const unsigned char* data, size_t size) {
    const char* alphabet = "0123456789abcdef";
    std::string result(size * 2, '0');
    for (size_t i = 0; i < size; ++i) {
        result[2*i] = alphabet[data[i] >> 4]; result[2*i+1] = alphabet[data[i] & 15];
    }
    return result;
}
bool Contents(int fd, const PackageGeneration& generation, int copy,
              const std::atomic_bool* cancel) {
    struct stat before, after;
    if (fstat(fd, &before) < 0) return false;
    if (!S_ISREG(before.st_mode) || before.st_size < 0
            || static_cast<uint64_t>(before.st_size) != generation.bytes) {
        Fail(EINVAL); return false;
    }
    SHA256_CTX hash;
    if (!SHA256_Init(&hash)) { Fail(EIO); return false; }
    std::array<unsigned char, 128 * 1024> buffer;
    uint64_t offset = 0;
    while (offset < generation.bytes) {
        if (cancel && cancel->load()) { Fail(ECANCELED); return false; }
        size_t count = std::min<uint64_t>(buffer.size(), generation.bytes - offset);
        ssize_t size = pread(fd, buffer.data(), count, offset);
        if (size < 0 && errno == EINTR) continue;
        if (size <= 0) { if (!size) Fail(EIO); return false; }
        if (!SHA256_Update(&hash, buffer.data(), size)) { Fail(EIO); return false; }
        if (copy >= 0 && !Write(copy, buffer.data(), size)) return false;
        offset += size;
    }
    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (!SHA256_Final(digest, &hash)) { Fail(EIO); return false; }
    if (fstat(fd, &after) < 0) return false;
    if (before.st_dev != after.st_dev || before.st_ino != after.st_ino
            || before.st_size != after.st_size
            || before.st_mtim.tv_sec != after.st_mtim.tv_sec
            || before.st_mtim.tv_nsec != after.st_mtim.tv_nsec
            || before.st_ctim.tv_sec != after.st_ctim.tv_sec
            || before.st_ctim.tv_nsec != after.st_ctim.tv_nsec
            || Hex(digest, sizeof(digest)) != generation.image_sha256) {
        Fail(ESTALE); return false;
    }
    return true;
}
bool Empty(int fd) {
    int scan = OpenAt(fd, ".", O_RDONLY | O_DIRECTORY);
    if (scan < 0) return false;
    DIR* dir = fdopendir(scan);
    if (!dir) { int saved=errno;close(scan);Fail(saved);return false; }
    int error = 0; errno = 0;
    while (dirent* item = readdir(dir)) {
        if (strcmp(item->d_name, ".") && strcmp(item->d_name, "..")) { error = EEXIST; break; }
    }
    if (!error) error = errno;
    if (closedir(dir) < 0 && !error) error = errno;
    if (error) { Fail(error); return false; }
    return true;
}
// Only removes this call's temporary inode. Failure never deletes a published
// image/selection. A crash can leave an unselected pending file for later GC.
class Pending {
  public:
    int directory;
    std::string name;
    unique_fd fd;
    bool named = false;
    explicit Pending(int parent) : directory(parent) {
        unsigned char random[16];
        ssize_t count;
        do { count=getrandom(random,sizeof(random),0); } while(count<0&&errno==EINTR);
        if (count < 0 || static_cast<size_t>(count) != sizeof(random)) { if(count>=0)Fail(EIO); return; }
        name = ".pending-" + Hex(random, sizeof(random));
        fd.reset(OpenAt(directory, name.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600));
        named = fd.ok();
    }
    bool Seal(mode_t mode) {
        if (fchmod(fd.get(),mode)<0 || fsync(fd.get())<0) return false;
        unique_fd readonly(OpenAt(directory,name.c_str(),O_RDONLY));
        struct stat first,second;
        if (!readonly.ok() || fstat(fd.get(),&first)<0 || fstat(readonly.get(),&second)<0)
            return false;
        if (first.st_dev!=second.st_dev || first.st_ino!=second.st_ino) {
            Fail(ESTALE);return false;
        }
        fd=std::move(readonly); // Close our only writable FD before publication.
        return true;
    }
    ~Pending() {
        int saved = errno;
        if (named && fd.ok() && Named(directory, name.c_str(), fd.get()))
            unlinkat(directory, name.c_str(), 0);
        errno = saved;
    }
};
} // namespace

PackageStore::PackageStore(int directory, int lock, PackageOwner owner)
    : directory_(directory), lock_(lock), process_(getpid()), owner_(owner) {}
PackageStore::~PackageStore() { close(lock_); close(directory_); }
bool PackageStore::Check() const {
    if (getpid() != process_ || geteuid() || !Owner(owner_)) { Fail(EPERM); return false; }
    return Metadata(directory_, S_IFDIR, 0700, false)
            && Metadata(lock_, S_IFREG, 0600, true) && Named(directory_, "lock", lock_);
}

PackageStore* PackageStore::Open(int directory, PackageOwner owner, bool create) {
    if (geteuid() || !Owner(owner)) { Fail(EINVAL); return nullptr; }
    unique_fd root(OpenAt(directory, ".", O_RDONLY | O_DIRECTORY));
    if (!root.ok() || !Metadata(root.get(), S_IFDIR, 0700, false)) return nullptr;
    if (create && !Empty(root.get())) return nullptr;
    unique_fd lock(OpenAt(root.get(), "lock", create ? O_RDWR | O_CREAT | O_EXCL : O_RDONLY, create ? 0600 : 0));
    if (!lock.ok() || !Metadata(lock.get(), S_IFREG, 0600, true)
            || flock(lock.get(), LOCK_EX | LOCK_NB) < 0 || !Named(root.get(), "lock", lock.get()))
        return nullptr;
    const std::string expected = OwnerText(owner);
    if (create) {
        unique_fd identity(OpenAt(root.get(), "owner", O_WRONLY | O_CREAT | O_EXCL, 0600));
        if (!identity.ok() || !Write(identity.get(), expected.data(), expected.size())
                || fchmod(identity.get(), 0400) < 0 || fsync(identity.get()) < 0
                || fsync(lock.get()) < 0 || fsync(root.get()) < 0) return nullptr;
    }
    unique_fd identity(OpenAt(root.get(), "owner", O_RDONLY));
    std::string actual;
    if (!identity.ok() || !Metadata(identity.get(), S_IFREG, 0400, true)
            || !ReadSmall(identity.get(), &actual)) return nullptr;
    if (actual != expected) { Fail(ESTALE); return nullptr; }
    return new PackageStore(root.release(), lock.release(), owner);
}

int PackageStore::ReadSelection(PackageGeneration* output) const {
    unique_fd fd(OpenAt(directory_, "current", O_RDONLY));
    if (!fd.ok() || !Metadata(fd.get(), S_IFREG, 0400, true)) return -1;
    std::string text;
    if (!ReadSmall(fd.get(), &text)) return -1;
    const std::string prefix = OwnerText(owner_);
    if (text.compare(0, prefix.size(), prefix)) return Fail(EPROTO);
    std::istringstream stream(text.substr(prefix.size()));
    PackageGeneration value;
    std::string base;
    if (!(stream >> value.image_sha256 >> base >> value.bytes)) return Fail(EPROTO);
    value.shared_base_sha256 = base == "-" ? "" : base;
    if (!Generation(owner_, value) || Record(owner_, value) != text) return Fail(EPROTO);
    *output = value;
    return 0;
}
int PackageStore::OpenImage(const PackageGeneration& value) const {
    unique_fd fd(OpenAt(directory_, (value.image_sha256 + ".image").c_str(), O_RDONLY));
    if (!fd.ok() || !Metadata(fd.get(), S_IFREG, 0444, true) || !Contents(fd.get(), value, -1, nullptr))
        return -1;
    return fd.release();
}
int PackageStore::Current(PackageGeneration* output) {
    if (!output) return Fail(EINVAL);
    if (getpid() != process_) return Fail(EPERM);
    std::lock_guard<std::mutex> guard(mutex_);
    if (!Check()) return -1;
    PackageGeneration value;
    if (ReadSelection(&value) < 0) return -1;
    int fd = OpenImage(value);
    if (fd < 0 && errno == ENOENT) return Fail(ESTALE); // Selected image missing, not an empty store.
    if (fd >= 0) *output = value;
    return fd;
}

int PackageStore::RetainedShared(const std::string& hash,PackageGeneration* output) {
    if(!output || !Hash(hash))return Fail(EINVAL);
    if(getpid()!=process_ || owner_.personal)return Fail(EPERM);
    std::lock_guard<std::mutex> guard(mutex_);
    if(!Check())return -1;
    const auto name=hash+".image";
    unique_fd fd(OpenAt(directory_,name.c_str(),O_RDONLY));
    if(!fd.ok() || !Metadata(fd.get(),S_IFREG,0444,true))return -1;
    struct stat st;if(fstat(fd.get(),&st)<0)return -1;
    if(st.st_size<=0 || uint64_t(st.st_size)>kMaxImage)return Fail(EFBIG);
    PackageGeneration value{hash,"",uint64_t(st.st_size)};
    if(!Contents(fd.get(),value,-1,nullptr) || !Named(directory_,name.c_str(),fd.get()))return -1;
    *output=std::move(value);return fd.release();
}

PackagePublish PackageStore::Publish(const PackageGeneration* expected_current, int source,
                                    const PackageGeneration& candidate,
                                    const std::atomic_bool& cancel) {
    auto reject = [](int error) { Fail(error); return PackagePublish::Rejected; };
    if (getpid() != process_) return reject(EPERM);
    std::lock_guard<std::mutex> guard(mutex_);
    if (!Check()) return PackagePublish::Rejected;
    if ((expected_current && !Generation(owner_, *expected_current)) || !Generation(owner_, candidate))
        return reject(EINVAL);
    PackageGeneration current;
    int selected = ReadSelection(&current);
    if (selected < 0 && errno != ENOENT) return PackagePublish::Rejected;
    if ((selected == 0) != (expected_current != nullptr)
            || (expected_current && Record(owner_, current) != Record(owner_, *expected_current)))
        return reject(ESTALE);
    // A damaged existing image never authorizes silently replacing/repairing it.
    if (selected == 0) { unique_fd previous(OpenImage(current)); if (!previous.ok()) return PackagePublish::Rejected; }
    if (cancel.load()) return reject(ECANCELED);
    const std::string image = candidate.image_sha256 + ".image";
    Pending copied(directory_);
    if (!copied.fd.ok() || !Contents(source, candidate, copied.fd.get(), &cancel)
            || !copied.Seal(0444))
        return PackagePublish::Rejected;
    if (cancel.load()) return reject(ECANCELED);
    if (syscall(SYS_renameat2, directory_, copied.name.c_str(), directory_, image.c_str(), RENAME_NOREPLACE) < 0) {
        if (errno != EEXIST) return PackagePublish::Rejected;
        unique_fd prior(OpenImage(candidate));
        if (!prior.ok()) return PackagePublish::Rejected;
    } else copied.named = false;
    if (fsync(directory_) < 0) return PackagePublish::Rejected; // Image not selected yet.
    Pending next(directory_);
    const std::string record = Record(owner_, candidate);
    if (!next.fd.ok() || !Write(next.fd.get(), record.data(), record.size())
            || !next.Seal(0400))
        return PackagePublish::Rejected;
    if (!Check()) return PackagePublish::Rejected;
    if (cancel.load()) return reject(ECANCELED);
    if (renameat(directory_, next.name.c_str(), directory_, "current") < 0)
        return PackagePublish::Rejected;
    next.named = false;
    if (fsync(directory_) < 0) return PackagePublish::Unconfirmed;
    return PackagePublish::Confirmed;
}
} // namespace aegis
