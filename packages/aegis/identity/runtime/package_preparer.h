#ifndef AEGIS_PACKAGE_PREPARER_H
#define AEGIS_PACKAGE_PREPARER_H
#include "package_executor.h"
#include "package_store.h"
namespace aegis {
struct PackageInput { uint64_t bytes=0; std::string sha256; };
struct PackagePreparation {
    PackageExecution execution;
    PackageInput image;
    // Dense archive-only order, skipping removal effects in execution.items.
    // FDs are trusted pinned
    // archive sources, never CLI-supplied. Hashes do not establish repo trust.
    std::vector<PackageInput> archives;
};
enum class PackagePreparationOutcome { Unconfirmed, Failed, Prepared };
struct PackagePreparationResult {
    PackagePreparationOutcome outcome=PackagePreparationOutcome::Unconfirmed;
    int error=0;
    // Only a runtime selection returns a generation; preparation leaves it empty.
    PackageGeneration generation={};
    enum class Scope { None, Factory, Shared, Personal } scope=Scope::None;
    PackageInput shared; // Verified shared/factory selection preceding personal selection.
    PackageInput previous_shared; // Only reconciliation returns the private generation's verified prior base.
};
struct PackageRuntimeSelection {
    uint32_t requester=0, serial=0;
    uint64_t job=0;
    PackageInput factory; // Pinned immutable system_ext receipt, never a CLI hash.
};
struct PackagePreparer;
int PackagePreparationCheck(const PackagePreparation& plan);
// Bounded registration/start only. ALL slow copy/hash/mount/cache preparation
// runs in the owned child. stage must be a new exclusive empty root:root0700
// directory, verified under the requester's CE/admission gate for private work.
// Never pass a selected generation's directory. The immutable helper and every
// input's provenance must be verified by the broker. No package code executes.
// Partial *worker remains owned even when this returns failure.
int PackagePreparerStart(int groups,int stage,int source,int helper,
                         const std::vector<int>& archives,const PackagePreparation& plan,
                         PackagePreparer** worker);
// Select and verify a complete generation in the SAME owned child machinery.
// Caller anchors stores under admission (personal CE+serial); -1 means verified
// absence, never an error opening/validating a present directory. No directories
// are created. Empty initialized stores fall back personal -> shared -> factory;
// corruption/foreign identity/lock contention never falls back. Personal base
// must equal selected shared/factory hash; otherwise ESTALE requires rebase.
// Returned mount is readonly/nosuid/nodev/noexec, not yet attached/idmapped.
// Slow SHA reads stay outside the broker's short admission. The owner MUST
// register before starting, include the worker/mount in STOP_USER, and close
// caller originals under admission. This internal primitive is not yet wired
// to production START or a public command. Existing mounts pin old images.
int PackageRuntimeSelectionCheck(const PackageRuntimeSelection& request);
int PackageRuntimeSelectionStart(int groups,int shared_store,int personal_store,
                                 int factory,int helper,const PackageRuntimeSelection& request,
                                 PackagePreparer** worker);
// Internal reconciliation input stage. Requires an existing private generation
// whose shared base differs from current shared/factory. Returns three checked
// readonly detached mounts: private, previous shared, current shared. It does
// NOT merge, publish, authorize packages or make a stale runtime start valid.
// Caller anchors personal CE under admission and registers ownership first, as
// above. EALREADY means the private base is current; ENODATA means no private
// generation; unavailable/corrupt old bases never fall back to current/factory.
int PackageReconciliationSelectionStart(int groups,int shared_store,int personal_store,
    int factory,int helper,const PackageRuntimeSelection& request,PackagePreparer** worker);
int PackageReconciliationSelectionFinish(PackagePreparer** worker,bool cancel,int timeout_ms,
    PackagePreparationResult* result,int mounts[3]);
int PackagePreparerCancel(PackagePreparer* worker);
// Actual child reaping + empty/removed group first. Timeout retains *worker.
// On Prepared only, *candidate receives ONE detached nosuid/nodev/noexec ext4
// mount FD (RW for preparation, readonly for selection); it MUST immediately
// enter the same registered broker slot.
// Keeping it or the stage pins private CE. Caller originals remain its duty.
// candidate initially -1, no output change on timeout. Cancellation consumes
// any queued mount without handing it off. No publication or approval occurs.
int PackagePreparerFinish(PackagePreparer** worker,bool cancel,int timeout_ms,
                          PackagePreparationResult* result,int* candidate);
}
#endif
