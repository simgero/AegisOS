#ifndef AEGIS_PACKAGE_RESOLVER_H
#define AEGIS_PACKAGE_RESOLVER_H
#include "package_apt_release.h"
namespace aegis {
// Runs only inside an owned, isolated PID1 with the verified immutable factory
// root, fixed readonly /run/aegis-plan-policy and readonly metadata snapshot at
// /run/aegis-plan-input. No selected/user executable or configuration is loaded.
// The owner authenticates these mounts BEFORE entering this function, registers
// cancellation/cgroup/CE ownership and provides bounded writable /tmp storage.
// Network connectivity, source policy provenance and lifecycle are owner duties;
// neither a caller path nor a credential is accepted by this engine.
struct PackageResolverRequest {
    PackageAction action=PackageAction::Install;
    std::string package, version;
    bool internet=false; // Trusted product policy; never a proxy address from a caller.
    bool reconciliation=false; // Internal three-generation mode; only Update, no user operands.
};
struct PackageResolverResult {
    enum class Phase { Validate, Update, Simulate, Download, Indexes, Collected };
    Phase phase=Phase::Validate;
    int status=0, error=0;
    std::vector<PackageAptEffect> effects;
    std::vector<PackageRepository> repositories;
    std::vector<PackageAptArchive> archives;
    PackageResolvedPlan evidence; // Only authenticated resolver fields; no caller identity/source.
};
// Write these exact bytes to the readonly policy/config before any APT starts.
// policy/sources.list and policy/key.asc are immutable product inputs, not
// client strings. A source must specify this exact keyring via signed-by.
// Exact installs can search non-candidate dependencies. Reconciliation instead
// uses published candidates plus installed versions. Neither changes repository
// trust or the independent exact-root checks.
const char* PackageResolverConfiguration(bool internet=false,bool reconciliation=false,
                                         bool exact_version=false);
int PackageResolverCheck(const PackageResolverRequest& request);
// On success /tmp/aegis-planner retains signed APT lists, downloaded archives,
// exact simulation, index-targets, and a native snapshot receipt. The owner MUST
// wait for every child to exit, independently join authenticated index/archive
// bytes with PackageBindAptArchives, recheck expiry, then request fresh AOSP
// admin approval. Collected is not authorization, publication or installation.
// No package scripts/dpkg are run; initial status/state must remain unchanged.
PackageResolverResult PackageResolverRun(uint32_t user,const PackageResolverRequest& request);
}
#endif
