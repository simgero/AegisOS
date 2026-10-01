#ifndef AEGIS_PACKAGE_EXECUTOR_H
#define AEGIS_PACKAGE_EXECUTOR_H
#include <stdint.h>
#include "package_store.h"
#include "package_execution_protocol.h"
#include <string>
#include <vector>
namespace aegis {
// Internal mechanical execution plan. Caller authenticates repository/archive
// contents and resolves the exact complete action before fresh AOSP approval.
// Private target is the requester, not the confirming administrator. No client
// FD, host path, command, shell script or option is accepted here.
struct PackageExecution {
    uint32_t requester = 0, serial = 0;
    uint64_t job = 0;
    std::string plan_sha256;
    uint32_t kind = AEGIS_PACKAGE_ARCHIVES;
    // Archives: exact APT-canonical archive basenames in candidate's own cache.
    // The trusted planner resolves these from package/version/architecture:
    // --no-download requires the canonical cache entry even for absolute .deb
    // arguments. A hash-based renaming alone is insufficient for APT.
    // Each archive is separately pinned by preparation's content hash.
    // Remove and metadata-only selection items are package names. Selection
    // has one unchanged installed version and changes only private intent/marks.
    // Mixed items follow review.effects: a
    // nonempty after version selects an archive, an empty version a removal.
    // Updates/private fallbacks are resolved
    // by the trusted planner into exact archives, never an implicit online run.
    std::vector<std::string> items;
    // Sealed expected effects/state; missing review is accepted by the device
    // probe only. The production entry requires a complete bound review.
    aegis_package_execution_review review = {};
};
// Call only after PackageExecutionCheck/PackagePreparationCheck succeeds.
inline bool PackageExecutionHasArchive(const PackageExecution& p, size_t i) {
    return i<p.items.size() && (p.kind==AEGIS_PACKAGE_ARCHIVES
        || ((p.kind==AEGIS_PACKAGE_MIXED || p.kind==AEGIS_PACKAGE_RECONCILE) && p.review.effects[i].after[0]));
}
enum class PackageExecutionOutcome { Unconfirmed, Failed, NeedsValidation, Published };
struct PackageExecutionResult {
    PackageExecutionOutcome outcome = PackageExecutionOutcome::Unconfirmed;
    int status = 0, error = 0;
    PackageGeneration generation = {}; // Only the bound broker transaction may publish.
};
struct PackageExecutor;
int PackageExecutionCheck(const PackageExecution& plan);
// stage is the already authenticated root-owned private staging directory;
// candidate is its exclusively owned detached writable ext4 fsmount. Copying,
// hashing, repository verification and archive preparation precede this gate
// and MUST themselves be lifecycle-owned by the future preparation stage.
// stage/helper provenance, CE/serial and AOSP authorization are caller duties.
// Mutates candidate's ID map. Never pass an active generation or personal HOME.
// Copies all caller FDs, retains every partial resource in *output on failure.
// Register *output before releasing AOSP admission; close originals separately.
// One common absolute CLOCK_MONOTONIC ns deadline, at most10s from now.
int PackageExecutorStart(int groups, int stage, int candidate, int helper,
                         const PackageExecution& plan, uint64_t deadline,
                         PackageExecutor** output);
int PackageExecutorCancel(PackageExecutor* worker);
// Only after actual PID1 reaping, empty/removed cgroup and closure of owned
// stage/mount/channel/config FDs is *worker cleared. Timeout/error retains it.
// NeedsValidation means APT, dpkg consistency and native account/file policy
// checks completed, NOT a hash-bound/published/activated complete image.
// Killed/lost replies stay Unconfirmed; never infer rollback or publication.
int PackageExecutorFinish(PackageExecutor** worker, bool cancel, int timeout_ms,
                          PackageExecutionResult* result);
} // namespace aegis
#endif
