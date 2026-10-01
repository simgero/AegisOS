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
    // Repository ID and archive are absent for removals and same-version private selection.
    std::string repository;
    PackageInput archive;
    PackageInstallReason reason=PackageInstallReason::Unspecified;
};
enum class PackageStatePresence : uint32_t { Unspecified=0, Absent=1, Present=2 };
struct PackageReconciliationEvidence {
    // Canonical sorted manual root names. Their exact versions belong to the
    // complete resulting registry digest; private intent stays unchanged.
    std::string roots, previous_status_sha256, previous_automatic_sha256;
    std::string current_status_sha256, current_automatic_sha256, solver_automatic_sha256;
    std::string result_registry_sha256, result_automatic_sha256;
};
struct PackageResolvedPlan {
    uint32_t requester=0, serial=0;
    bool personal=false, create_store=false, has_previous=false;
    // Reconciliation evidence cannot enter the ordinary transaction binder.
    // A separate binder binds all three generations; activation remains owner-controlled.
    bool reconciliation=false;
    PackageAction action=PackageAction::Install;
    std::string requested_package, requested_version;
    PackageInput source, shared, previous_shared;
    PackageReconciliationEvidence reconciliation_evidence;
    PackageGeneration previous;
    std::string planner_image_sha256, policy_sha256, initial_status_sha256;
    // Exact root-owned manifest copied from the selected readonly generation.
    // Absent only for a shared/factory source; an existing private source needs it.
    std::string initial_private_choices;
    // Snapshot of APT extended_states in the selected source. Absence is distinct
    // from an existing empty file. Never synthesize from only changed packages.
    PackageStatePresence initial_apt_state_presence=PackageStatePresence::Unspecified;
    PackageInput initial_apt_state;
    // Strictly sorted unique IDs/names. Mechanical archive FDs follow this
    // change order with removals omitted (the evidence adapter uses -1 slots).
    std::vector<PackageRepository> repositories;
    std::vector<PackageChange> changes;
};
struct PackageBoundPlan {
    PackagePreparation preparation;
    PackagePublication publication;
    // Earliest repository expiry. 0 for repository-free removal or private selection.
    uint64_t valid_until_unix=0;
    // Complete validated review, including dependency marks and initial APT state.
    // The worker independently checks these effects and applies their marks;
    // this retained review also belongs to the forthcoming product approval UI.
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
// The worker independently checks exact effects; this binder alone does not
// infer that APT will perform only the declared changes.
// Mixed effects are bound in full; dense archive inputs omit removal slots.
// A changed shared base is ESTALE: private rebase is an explicit later operation.
int PackageBindResolvedPlan(const PackageResolvedPlan& plan,uint64_t now_unix,
                             PackageBoundPlan* output);
// Internal trusted adapter only. Requires verified private/old/current image
// provenance and complete resolver evidence. It grants no authorization and
// does not register runtime admission or shared-current publication fencing.
int PackageBindReconciliationPlan(const PackageResolvedPlan&,uint64_t now_unix,PackageBoundPlan*);
} // namespace aegis
#endif
