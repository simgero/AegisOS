#ifndef AEGIS_PACKAGE_APT_ARCHIVES_H
#define AEGIS_PACKAGE_APT_ARCHIVES_H
#include "package_apt_plan.h"
namespace aegis {
struct PackageAptIndex {
    PackageRepository repository;
    int fd=-1; // Pinned, quiescent, readonly full uncompressed Packages index.
};
struct PackageAptArchive {
    PackageAptEffect effect; // Includes automatic/manual state; never discard it.
    std::string repository, filename;
    PackageInput archive; // Empty for a removal, which never downloads an archive.
};
// Run only in the lifecycle-owned worker, never inside short broker admission.
// Repository receipts MUST come from immutable-policy APT signature/freshness
// verification, never from a caller or from hashing unsigned text. This helper
// hashes each complete index against that authenticated receipt and matches the
// exact resolver-selected name/version/architecture. It does not establish the
// signature trust itself, open a network connection or interpret dependencies.
// Readonly, regular index FDs: max256 MiB each /512 MiB total; max16 sources,
// Authenticated empty indexes are valid but cannot supply an archive.
// 64 effects, 64 KiB lines /1 MiB stanzas /256 fields. No seek-offset mutation.
// Duplicate selected identities within one index, conflicting content across
// repositories, missing selected versions and expiry fail closed. Identical
// content in distinct repositories picks the first sorted configured source.
// Filename is a bounded relative repository path, never a URL or host path.
// No output change on failure; the caller retains all input FDs and lifecycle.
int PackageMatchAptArchives(const std::vector<PackageAptEffect>& effects,
    const std::vector<PackageAptIndex>& indexes,uint64_t now_unix,
    std::vector<PackageAptArchive>* output);
// After the owned downloader has exited, independently verify the pinned
// readonly regular archive against its authenticated index size+SHA-256.
// Keeping the fd pinned and bytes quiescent through preparation is caller duty.
// No pathname, package code, user/admin authority or publication is accepted.
int PackageVerifyAptArchive(const PackageAptArchive& expected,int fd);
// Join trusted request/source context to ALL matched resolver effects, retain
// automatic/manual marks, verify each acquired archive and derive the approval
// digest. context.changes MUST be empty: no competing caller effect list.
// FDs align with effects; removal slots MUST be -1 with no archive/path metadata.
// Repositories/context are authenticated owned-worker inputs, never CLI data.
// Signature trust is a prerequisite, NOT conferred by a matching hash.
// Bounds/expiry are checked before any archive IO. Failure leaves output intact.
// Keep input FDs pinned/quiescent through preparation. No execution/authorization.
int PackageBindAptArchives(const PackageResolvedPlan& context,
    const std::vector<PackageAptArchive>& effects,const std::vector<int>& archive_fds,
    uint64_t now_unix,PackageBoundPlan* output);
}
#endif
