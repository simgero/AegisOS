#ifndef AEGIS_PACKAGE_APT_RELEASE_H
#define AEGIS_PACKAGE_APT_RELEASE_H
#include "package_apt_archives.h"
namespace aegis {
// Bound metadata lifetime even when a stable Debian Release omits Valid-Until.
// Keep the immutable APT Acquire::Max-ValidTime setting identical.
constexpr uint64_t PackageReleaseMaxAge=120u*24u*60u*60u;
struct PackageReleaseIndex { uint64_t date=0, valid_until=0; PackageInput index; };
// Parse ONLY bytes already authenticated by immutable-policy APT. This is NOT
// an OpenPGP verifier. Handles clear-signed InRelease and detached Release text.
// Check the full uncompressed Packages hash, date and effective expiry anew.
// No output mutation on failure; bounded to 4 MiB and 64 KiB per line.
int PackageReadAuthenticatedRelease(const std::string& bytes,const std::string& meta_key,
                                    uint64_t now,PackageReleaseIndex* output);
// Owned resolver only, after successful APT update and every child is reaped.
// lists/ and archives/ must be private, quiescent and from that exact run;
// arbitrary directory/indextargets text MUST NOT establish signature trust.
// Reject compressed or unexpected targets, missing signed metadata, stale
// releases, wrong full-index hashes and wrong acquired archive bytes.
int PackageCollectAptEvidence(int directory,const std::string& targets,
    const std::vector<PackageAptEffect>& effects,uint64_t now,
    std::vector<PackageRepository>* repositories,std::vector<PackageAptArchive>* archives);
}
#endif
