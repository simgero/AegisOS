#ifndef AEGIS_BROKER_OWNER_SELECTION_H
#define AEGIS_BROKER_OWNER_SELECTION_H
#include "broker_owner.h"
#include "package_preparer.h"
namespace aegis {
enum class RuntimeSelectionState { Selecting, Selected, Activated, Failed, Sealed, ReconciliationInputs };
// Internal authenticated requester API. Caller holds fresh AOSP id/serial/CE
// admission and anchors all non-CE inputs. No public wire registration yet.
// Register BEFORE opening personal CE/spawning, share monotone broker job IDs.
// Slow work stays in the lifecycle-owned child. Output job initially zero;
// nonzero on failure means owned partial/failed work, not a successful start.
// Generic variant: personal=-1 ONLY for verified absence; no CLI descriptors.
// CE variant opens fixed /data and distinguishes checked absence from errors.
// At most one runtime selection per user; no replacing an active generation.
// STOP_USER/HELLO/disconnect clear it only after confirmed complete teardown.
int BrokerPrepareRuntimeSelection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
                                  int groups,int shared,int personal,int factory,int helper,
                                  uint64_t deadline,uint64_t* job);
int BrokerPrepareCeRuntimeSelection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
                                    int groups,int shared,int factory,int helper,
                                    uint64_t deadline,uint64_t* job);
// Package jobs select independently of an already activated runtime. Scope is
// retained in the owner: shared jobs must not receive a personal store; a later
// planning handoff cannot change scope or consume a runtime START selection.
// Generic trusted-input variant for internal callers and isolated fixtures.
int BrokerPreparePackageSelection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
                                  bool personal_scope,int groups,int shared,int personal,int factory,int helper,
                                  uint64_t deadline,uint64_t* job);
// Internal three-view selection. ReconciliationInputs is deliberately NOT
// Selected: it cannot be adopted by START or consumed by an ordinary planner.
// Every private/old-common/current-common mount stays in the owner until one
// explicit planning handoff or confirmed STOP. No private FD is exported.
int BrokerPrepareReconciliationSelection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
    int groups,int shared,int personal,int factory,int helper,uint64_t deadline,uint64_t* job);
int BrokerPreparePrivateRemovalSelection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
    int groups,int shared,int personal,int factory,int helper,uint64_t deadline,uint64_t* job);
// Product inputs and CE/serial are opened only after registering the new job.
// This internal entry grants neither package authority nor runtime activation.
int BrokerBeginConfiguredReconciliation(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
    uint64_t deadline,uint64_t* job);
// Product variant uses ONLY startup-pinned factory/helper/state/cgroup inputs.
// The caller holds fresh authenticated requester+serial+AOSP CE admission.
// Personal scope opens fixed checked CE after registration; shared scope never
// opens that private store. No path, FD, hash or administrator identity enters.
int BrokerPrepareConfiguredPackageSelection(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                                             bool personal,uint64_t deadline,uint64_t* job);
// Exact package job only; never cancels a runtime selection or running shell.
// Partial cancellation remains owned until the child is reaped and mount closed.
int BrokerCancelPackageSelection(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                                 uint64_t job,uint64_t deadline);
// Nonblocking metadata only, never exports a private FD. Does not consume the
// selection. Selected means a mount is held, not a live runtime or CE eviction.
// Activated records adoption, not ongoing context liveness; use owner STATUS.
// A subsequent freshly admitted owner START uses exactly the registered mount,
// via a temporary private anchor. Existing contexts retain their generation.
// Failed selections MUST NOT silently use the factory base on START.
int BrokerPollRuntimeSelection(aegis_broker_owner* owner,uint32_t user,uint32_t serial,uint64_t job,
                               RuntimeSelectionState* state,PackagePreparationResult* result);
}
#endif
