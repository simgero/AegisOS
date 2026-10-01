#ifndef AEGIS_PACKAGE_STORE_H
#define AEGIS_PACKAGE_STORE_H

#include <atomic>
#include <mutex>
#include <stdint.h>
#include <string>

// Internal persistence primitive, not an authorization or package installation
// endpoint. The caller supplies a verified store FD after fresh AOSP approval
// and, for personal scope, CE/serial checks. Never accept these from a CLI.
namespace aegis {
struct PackageOwner {
    bool personal;
    uint32_t user_id;
    uint32_t serial;
};
struct PackageGeneration {
    std::string image_sha256;
    std::string shared_base_sha256; // Required for a personal derived generation.
    uint64_t bytes = 0;
};
enum class PackagePublish { Rejected = -1, Confirmed = 0, Unconfirmed = 1 };

class PackageStore {
  public:
    // Root-owned, mode 0700, same-filesystem directory, exclusive nonblocking
    // process lock. Creation requires an empty directory. No path traversal,
    // automatic repair, deletion of old generations or retention policy here.
    static PackageStore* Open(int directory, PackageOwner owner, bool create);
    // Pins and exclusively locks a pristine shared directory without creating
    // metadata. Returns an owned read-only FD; EEXIST means nonempty, never a
    // claim that existing metadata is valid. All Open calls take this same
    // directory lock before initialization and keep it for the store lifetime.
    static int LockEmptyShared(int directory);
    ~PackageStore();
    PackageStore(const PackageStore&) = delete;
    PackageStore& operator=(const PackageStore&) = delete;

    // Returns an independently opened read-only image FD after hash verification.
    // ENOENT means no generation selected; a missing selected image is ESTALE.
    // Returned FDs pin previous contents
    // across later publication. Close all private FDs before AOSP CE eviction.
    int Current(PackageGeneration* generation);

    // Bounded status observation ONLY: validate selected metadata and image
    // type/owner/mode/link-count/size, but never read/hash image contents or
    // return an image FD. This is NOT verification, admission or execution
    // authority. Current() remains mandatory before using a selected image.
    int SelectionMetadata(PackageGeneration* generation);

    // Read an immutable retained SHARED image referenced by a verified private
    // generation's base hash. Never changes current or opens a personal store.
    // The caller owns provenance of the hash; absence is ENOENT, corruption fails.
    // Full content/metadata verification precedes output, including after reopen.
    int RetainedShared(const std::string& hash, PackageGeneration* generation);

    // Copy a complete already validated package image into broker-owned storage.
    // The source is never installed in place. It must contain consistent apt/
    // dpkg/files/config/technical-account state; this primitive does not prove
    // that semantic validation or fresh AOSP authorization.
    // expected_current is null only for the first selection. A stale selection
    // is rejected. cancel is sampled during copy and before the rename point.
    // A confirmed rename selects a whole image; old images remain. Fsync failure
    // after rename is Unconfirmed, never success or a claim that nothing changed.
    PackagePublish Publish(const PackageGeneration* expected_current, int source,
                           const PackageGeneration& candidate,
                           const std::atomic_bool& cancel);

  private:
    PackageStore(int directory, int lock, PackageOwner owner);
    bool Check() const;
    int ReadSelection(PackageGeneration* generation) const;
    int OpenImage(const PackageGeneration& generation) const;
    int directory_, lock_, process_;
    PackageOwner owner_;
    std::mutex mutex_;
};
} // namespace aegis
#endif
