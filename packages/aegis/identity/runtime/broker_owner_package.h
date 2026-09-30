#ifndef AEGIS_BROKER_OWNER_PACKAGE_H
#define AEGIS_BROKER_OWNER_PACKAGE_H
#include "broker_owner.h"
#include "package_publisher.h"
#include "package_executor.h"
#include "package_preparer.h"
#include "package_planner.h"
namespace aegis {
// The only package request fields supplied by an admitted product caller.
// Identity comes from the current AOSP session; no policy, path, descriptor,
// internet flag, creation flag or digest can be selected by this request.
struct PackageIntent {
    PackageAction action=PackageAction::Install;
    std::string package,version;
    bool personal=false;
};
// One session-facing view over the existing owned phases. No second job table,
// external phase selector or reconstructed action is needed by the wire adapter.
// Complete results are consumed by a successful poll; a lost response never
// establishes rollback. An unknown job is never restarted implicitly.
enum class ConfiguredPackagePhase {
    Selecting, Selected, Planning, Collected, Reviewed, Preparing, Prepared,
    Running, AwaitingValidation, Publishing, Complete, Sealed
};
struct ConfiguredPackageStatus {
    ConfiguredPackagePhase phase=ConfiguredPackagePhase::Sealed;
    PackageIntent intent;
    std::string plan_sha256;
    uint64_t valid_until_unix=0;
    PackageExecutionResult result;
};
// Caller must authenticate the original requester/session and hold its AOSP
// admission for inspection. Reaping may advance the already registered job
// into its owned publisher; no new request, grant, copy/hash/APT runs in this
// caller. Output remains unchanged on failure. Internal fixture jobs cannot
// become product jobs.
int BrokerPollConfiguredPackage(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                                 uint64_t job,ConfiguredPackageStatus* output);
// Exact owned identity is enough for cleanup after a session is revoked; it is
// never an install grant. Seals the current phase, retaining incomplete teardown
// in the same owner. Completed published execution returns EALREADY, not rollback.
int BrokerCancelConfiguredPackage(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                                   uint64_t job,uint64_t deadline);
// Start selection with the original intent registered before opening any CE.
// Uses startup-pinned product inputs exclusively. A returned nonzero job on
// failure still belongs to the owner and must be cancelled/reaped normally.
int BrokerBeginConfiguredPackage(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                                  const PackageIntent& intent,uint64_t deadline,uint64_t* job);
// Fresh requester/session/CE admission is required again. The same retained
// intent and job transfer to planning; no replacement request or FD is accepted.
// EAGAIN means selecting, EALREADY means already transferred. Lookup failures
// (including ENOENT) can also be retained failed selections: inspect/poll their
// owned state. Failure never proves quiescence or releases cancellation duties.
int BrokerContinueConfiguredPackagePlanning(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                                              uint64_t job,uint64_t deadline);
enum class PlanningState { Running, Collected, Reviewed, Complete, Sealed };
// Register before snapshot copying/APT. Same requester admission, 16-slot budget,
// monotonically minted IDs and STOP/HELLO/disconnect ownership as execution.
// Collected retains its private evidence directory inside the owner; Poll never
// hands out descriptors or releases the slot. Review and preparation consume only this retained result; approval stays separate.
// No client-supplied factory/selected/policy/helper is accepted by a public API.
int BrokerStartPlanning(aegis_broker_owner* owner,const PackagePlanning& request,
                         int groups,int factory,int selected,int sources,int key,int helper,
                         uint64_t deadline,uint64_t* job,int network_helper=-1,int ca_bundle=-1);
// Production preparation handoff: use the already registered selection job,
// never an externally reopened CE mount. The same job ID survives selection ->
// planning, with the verified source identity retained privately by the owner.
// Still requires fresh requester admission; not fresh install/admin approval.
int BrokerStartPlanningFromSelection(aegis_broker_owner* owner,const PackagePlanning& request,
                                      uint64_t selection_job,int groups,int factory,int sources,int key,int helper,
                                      uint64_t deadline,uint64_t* job,int network_helper=-1,int ca_bundle=-1);
int BrokerPollPlanning(aegis_broker_owner* owner,uint32_t user,uint32_t serial,uint64_t job,
                        PlanningState* state,PackagePlanningResult* result);
// Bounded review from this owned worker and its retained verified selection.
// The caller supplies no hashes/effects/source identity. Rechecks repository
// expiry each time; returns metadata only, never authority or private FDs.
int BrokerReviewPlanning(aegis_broker_owner* owner,uint32_t user,uint32_t serial,uint64_t job,
                         uint64_t deadline,PackageBoundPlan* output);
// Continue the same Reviewed job through candidate preparation. Archives come
// exclusively from the quiescent owned planner directory; copy/hash is async.
// Source/stage/store/helper FDs are internally pinned under fresh admission,
// never CLI inputs; the preparer checks source bytes against retained selection.
// Personal scope uses its CE-anchored stage/store; supplied stage/store must be -1.
// NEW fresh AOSP admin approval is still required before BrokerStartExecution.
int BrokerPreparePlannedTransaction(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
    uint64_t job,const std::string& digest,int groups,int stage,int store,int source,
    int prepare_helper,int execute_helper,int publish_helper,uint64_t deadline);
// Product handoff from its retained Reviewed job. No supplied digest, source,
// stage, store, helper or archive FD. Registers execution before opening CE or
// mutating a target; retained source bytes are checked asynchronously. Only a
// newly admitted AOSP action approval may later start the Prepared execution.
int BrokerPrepareConfiguredTransaction(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                                        uint64_t job,uint64_t deadline);
int BrokerCancelPlanning(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                          uint64_t job,uint64_t deadline);

enum class PublicationState { Prepared, Running, Complete, Sealed, Preparing, AwaitingValidation, Publishing };

// Trusted internal planner API, not a socket/CLI authorization endpoint.
// The authenticated REQUESTER and verified source/store/helper/cgroup FDs are
// supplied under its existing AOSP admission/CE gate. Copies are registered
// with STOP_USER/HELLO/shutdown before this returns. Caller closes originals
// before releasing admission. No copying/hash/APT inside these calls.
// request.job and *job must be zero. IDs are minted and never reused within
// this owner. The resulting preparation belongs to this exact authenticated
// broker connection/lifetime; a reconnect MUST invalidate Java preparations.
// Max16 retained slots, at most one unconsumed job per requester. Completed jobs
// remain until a matching poll consumes the result or quiescence clears them.
int BrokerPreparePublication(aegis_broker_owner* owner,const PackagePublication& request,
                             int groups,int store,int source,int helper,uint64_t deadline,uint64_t* job);

// Called only after fresh AOSP action/admin approval and current requester
// session/CE checks under admission. Exact identity, job AND plan must match.
// A start attempt is consumed; partial starts remain owned on every failure.
// Native code never receives the administrator password or a reusable grant.
int BrokerStartPublication(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                           uint64_t job,const std::string& plan,uint64_t deadline);
// Caller authenticates requester/session, also for read/cancel. A completed
// poll consumes the result, never its ID. ENOENT means unknown, not rollback.
int BrokerPollPublication(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                          uint64_t job,const std::string& plan,PublicationState* state,
                          PackagePublicationResult* result);
int BrokerCancelPublication(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                            uint64_t job,const std::string& plan,uint64_t deadline);
// The same registry owns pre-staged APT jobs. Shares the 16-slot limit and
// monotonic IDs with publication, one unconsumed job per requester across both.
// Inputs must be trusted/lifecycle-owned preparation results, not client FDs.
// This is still an internal API; no public wire/CLI caller is enabled yet.
// Successful APT remains AwaitingValidation under this same owned job. Polling
// neither consumes it nor exposes its private staging FD. STOP/cancel still
// close its CE reference; only a future registered validator may advance it.
// NeedsValidation certifies completed execution only, never activation.
// Register and start slow preparation in the SAME execution slot/job. Poll
// yields Preparing -> Prepared only after child reap and validated mount
// receipt. Prepared pins stage/mount until StartExecution under a NEW fresh
// AOSP approval/session admission, or cancellation/STOP_USER. No grant cached.
// plan.execution.job and *job initially zero. If partial start fails AFTER
// registration, *job is nonzero and remains cancellable/owned even on -1.
// Every source/archive FD and stage is trusted/CE-anchored by the caller.
// Complete owned transaction: target is immutable before NEW fresh admin
// approval at StartExecution. It matches requester/serial/plan, has job=0,
// derive_source_hash=true and an empty candidate hash, with exact image bytes.
// After successful checked APT and teardown, the same slot starts its pinned
// publisher. Hashing/copying stay in that child. PollExecution returns Published
// with generation only after confirmed selection AND complete resource release.
// Cancellation/STOP also cover publishing; a lost reply never claims rollback.
// Generic internal variant requires trusted anchored stage/store FDs. Production
// personal scope must use the CE variant below, with no caller path or store FD.
int BrokerPrepareTransaction(aegis_broker_owner* owner,const PackagePreparation& plan,
                              const PackagePublication& target,int groups,int stage,int store,int source,
                              int prepare_helper,int execute_helper,int publish_helper,
                              const std::vector<int>& archives,uint64_t deadline,uint64_t* job);
int BrokerPreparePersonalTransaction(aegis_broker_owner* owner,const PackagePreparation& plan,
                                      const PackagePublication& target,int groups,int source,
                                      int prepare_helper,int execute_helper,int publish_helper,
                                      const std::vector<int>& archives,uint64_t deadline,uint64_t* job);
int BrokerPrepareCandidate(aegis_broker_owner* owner,const PackagePreparation& plan,
                           int groups,int stage,int source,int prepare_helper,int execute_helper,
                           const std::vector<int>& archives,uint64_t deadline,uint64_t* job);
// CE-anchored variants for private scope. Open only fixed /data beneath
// init-pinned host namespaces, verify AOSP system_ce serial + matching live
// fscrypt-v2 policy, and register BEFORE creating/opening private storage.
// No external stage/store fd or path, and no separate administrator identity.
// Failed CE admission after registration returns a nonzero consumed job;
// the normal matching poll/cancel/STOP_USER paths own that failed result.
// AOSP session/lifecycle admission and fresh approval obligations above remain.
int BrokerPreparePersonalCandidate(aegis_broker_owner* owner,const PackagePreparation& plan,
                                   int groups,int source,int prepare_helper,int execute_helper,
                                   const std::vector<int>& archives,uint64_t deadline,uint64_t* job);
// Requires request.personal=true. Store initialization is still an explicit
// plan.create operation performed by the separately authorized publisher.
int BrokerPreparePersonalPublication(aegis_broker_owner* owner,const PackagePublication& request,
                                     int groups,int source,int helper,uint64_t deadline,uint64_t* job);
int BrokerPrepareExecution(aegis_broker_owner* owner,const PackageExecution& request,
                           int groups,int stage,int candidate,int helper,uint64_t deadline,uint64_t* job);
int BrokerStartExecution(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                         uint64_t job,const std::string& plan,uint64_t deadline);
int BrokerPollExecution(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                        uint64_t job,const std::string& plan,PublicationState* state,
                        PackageExecutionResult* result);
int BrokerCancelExecution(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                          uint64_t job,const std::string& plan,uint64_t deadline);
} // namespace aegis
#endif
