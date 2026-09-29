#ifndef AEGIS_BROKER_OWNER_PACKAGE_H
#define AEGIS_BROKER_OWNER_PACKAGE_H
#include "broker_owner.h"
#include "package_publisher.h"
namespace aegis {
enum class PublicationState { Prepared, Running, Complete, Sealed };

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
} // namespace aegis
#endif
