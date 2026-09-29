#ifndef AEGIS_PACKAGE_PREPARER_H
#define AEGIS_PACKAGE_PREPARER_H
#include "package_executor.h"
namespace aegis {
struct PackageInput { uint64_t bytes=0; std::string sha256; };
struct PackagePreparation {
    PackageExecution execution;
    PackageInput image;
    // In execution.items order; empty for removal. FDs are trusted pinned
    // archive sources, never CLI-supplied. Hashes do not establish repo trust.
    std::vector<PackageInput> archives;
};
enum class PackagePreparationOutcome { Unconfirmed, Failed, Prepared };
struct PackagePreparationResult {
    PackagePreparationOutcome outcome=PackagePreparationOutcome::Unconfirmed;
    int error=0;
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
int PackagePreparerCancel(PackagePreparer* worker);
// Actual child reaping + empty/removed group first. Timeout retains *worker.
// On Prepared only, *candidate receives ONE detached RW nosuid/nodev/noexec
// ext4 mount FD; it MUST immediately enter the same registered broker slot.
// Keeping it or the stage pins private CE. Caller originals remain its duty.
// candidate initially -1, no output change on timeout. Cancellation consumes
// any queued mount without handing it off. No publication or approval occurs.
int PackagePreparerFinish(PackagePreparer** worker,bool cancel,int timeout_ms,
                          PackagePreparationResult* result,int* candidate);
}
#endif
