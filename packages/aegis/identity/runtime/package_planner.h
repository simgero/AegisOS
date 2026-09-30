#ifndef AEGIS_PACKAGE_PLANNER_H
#define AEGIS_PACKAGE_PLANNER_H
#include "package_resolver.h"
namespace aegis {
struct PackagePlanning {
    uint32_t requester=0, serial=0;
    uint64_t job=0;
    PackageResolverRequest request;
};
struct PackagePlanningResult {
    enum class Outcome { Unconfirmed, Failed, Collected } outcome=Outcome::Unconfirmed;
    PackageResolverResult::Phase phase=PackageResolverResult::Phase::Validate;
    int status=0,error=0;
    uint32_t effects=0;
};
struct PackagePlanner;
int PackagePlanningCheck(const PackagePlanning& request);
// Internal trusted owner API, not CLI input or authorization. Factory is the
// authenticated immutable factory root, already anchored in the broker. Selected
// is a verified detached readonly generation from PackageRuntimeSelectionStart.
// Sources/key are pinned immutable product files, never selected-package config.
// Their descriptors must be readonly root-owned files. Caller registers ownership
// BEFORE calling, closes original CE references under admission, and includes
// the returned partial worker in STOP_USER even when Start returns failure.
// Startup only maps/passes descriptors; metadata copying and APT run in the child.
// No package execution, approval or publication. This initial worker retains the
// private network namespace; productive network admission is a separate step.
int PackagePlannerStart(int groups,int factory,int selected,int sources,int key,int helper,
                         const PackagePlanning& request,uint64_t deadline,PackagePlanner** worker);
int PackagePlannerCancel(PackagePlanner* worker);
// Actual pidfd reap, cgroup emptiness/removal and temporary-anchor detach precede
// success. Timeout retains ownership and all outputs. A Collected result returns
// one private scratch-directory fd, which MUST immediately remain in the same
// broker slot. It is not a public result or an authorization/activation token.
// Cancellation consumes queued result descriptors. Caller originals remain its duty.
int PackagePlannerFinish(PackagePlanner** worker,bool cancel,int timeout_ms,
                          PackagePlanningResult* result,int* collected);
}
#endif
