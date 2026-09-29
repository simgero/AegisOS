#ifndef AEGIS_BROKER_OWNER_PACKAGE_H
#define AEGIS_BROKER_OWNER_PACKAGE_H
#include "broker_owner.h"
#include "package_publisher.h"
#include "package_executor.h"
#include "package_preparer.h"
namespace aegis {
enum class PublicationState { Prepared, Running, Complete, Sealed, Preparing };

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
// Poll NeedsValidation certifies completed execution only, never activation.
// Register and start slow preparation in the SAME execution slot/job. Poll
// yields Preparing -> Prepared only after child reap and validated mount
// receipt. Prepared pins stage/mount until StartExecution under a NEW fresh
// AOSP approval/session admission, or cancellation/STOP_USER. No grant cached.
// plan.execution.job and *job initially zero. If partial start fails AFTER
// registration, *job is nonzero and remains cancellable/owned even on -1.
// Every source/archive FD and stage is trusted/CE-anchored by the caller.
int BrokerPrepareCandidate(aegis_broker_owner* owner,const PackagePreparation& plan,
                           int groups,int stage,int source,int prepare_helper,int execute_helper,
                           const std::vector<int>& archives,uint64_t deadline,uint64_t* job);
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
