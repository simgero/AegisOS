#ifndef AEGIS_PACKAGE_PLAN_H
#define AEGIS_PACKAGE_PLAN_H
#include "package_preparer.h"
#include "package_publisher.h"
namespace aegis {
// Syntax only, no package existence or authorization claim.
bool PackagePlanNameValid(const std::string& value);
bool PackagePlanVersionValid(const std::string& value);
enum class PackageAction : uint32_t { Install=1, Update=2, Remove=3 };
// Evidence supplied by the trusted APT adapter AFTER signature, freshness and
// Release -> Packages -> archive verification. Digests alone do NOT prove trust.
// id identifies a configured index, not a user URL/path. policy_sha256 below
// binds the immutable source configuration, keyrings and resolver settings.
struct PackageRepository {
    std::string id, release_sha256, index_sha256;
    uint64_t valid_until_unix=0;
};
// Unspecified is rejected; an omitted dependency mark must never become manual.
enum class PackageInstallReason : uint32_t { Unspecified=0, Manual=1, Automatic=2 };
struct PackageChange {
    std::string name, architecture, before_version, after_version;
    // Empty after_version means remove (keep conffiles), never purge.
    // Repository ID and archive must both be absent for a removal.
    std::string repository;
    PackageInput archive;
    PackageInstallReason reason=PackageInstallReason::Unspecified;
};
enum class PackageStatePresence : uint32_t { Unspecified=0, Absent=1, Present=2 };
struct PackageResolvedPlan {
    uint32_t requester=0, serial=0;
    bool personal=false, create_store=false, has_previous=false;
    PackageAction action=PackageAction::Install;
    std::string requested_package, requested_version;
    PackageInput source, shared;
    PackageGeneration previous;
    std::string planner_image_sha256, policy_sha256, initial_status_sha256;
    // Snapshot of APT extended_states in the selected source. Absence is distinct
    // from an existing empty file. Never synthesize from only changed packages.
    PackageStatePresence initial_apt_state_presence=PackageStatePresence::Unspecified;
    PackageInput initial_apt_state;
    // Strictly sorted unique IDs/names. Archive FDs use the same change order.
    std::vector<PackageRepository> repositories;
    std::vector<PackageChange> changes;
};
struct PackageBoundPlan {
    PackagePreparation preparation;
    PackagePublication publication;
    // Earliest repository expiry. 0 only for repository-free removal.
    uint64_t valid_until_unix=0;
    // Complete validated review, including dependency marks and initial APT state.
    // Execution must still enforce these expected effects/marks independently;
    // the mechanical preparer/executor does not yet apply them.
    PackageResolvedPlan reviewed;
};
// Validate the resolved description and derive BOTH mechanical stages and their
// common, versioned SHA-256. No caller-selected hash, admin identity, path, FD,
// command, option or job ID enters the plan. A private owner is the requester.
// Output is untouched on failure. Job IDs remain zero for broker registration.
// Bounded CPU-only operation; no image reads, repository IO, credential checks,
// dependency solving, execution or publication. Must NOT receive CLI metadata.
// The adapter must verify repositories/dependencies, source/status provenance,
// exact expected effects and recheck expiry before fresh AOSP confirmation.
// Execution still needs independent exact-effects checking; this binder does
// not infer that APT will perform only the declared changes.
// Mixed install/remove effects are presently EOPNOTSUPP, never silently dropped.
// A changed shared base is ESTALE: private rebase is an explicit later operation.
int PackageBindResolvedPlan(const PackageResolvedPlan& plan,uint64_t now_unix,
                             PackageBoundPlan* output);
} // namespace aegis
#endif
