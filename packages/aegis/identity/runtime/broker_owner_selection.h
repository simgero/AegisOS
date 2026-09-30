#ifndef AEGIS_BROKER_OWNER_SELECTION_H
#define AEGIS_BROKER_OWNER_SELECTION_H
#include "broker_owner.h"
#include "package_preparer.h"
namespace aegis {
enum class RuntimeSelectionState { Selecting, Selected, Activated, Failed, Sealed };
// Internal authenticated requester API. Caller holds fresh AOSP id/serial/CE
// admission and anchors all non-CE inputs. No public wire registration yet.
// Register BEFORE opening personal CE/spawning, share monotone broker job IDs.
// Slow work stays in the lifecycle-owned child. Output job initially zero;
// nonzero on failure means owned partial/failed work, not a successful start.
// Generic variant: personal=-1 ONLY for verified absence; no CLI descriptors.
// CE variant opens fixed /data and distinguishes checked absence from errors.
// At most one selection per user; no replacing a prepared/active generation.
// STOP_USER/HELLO/disconnect clear it only after confirmed complete teardown.
int BrokerPrepareRuntimeSelection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
                                  int groups,int shared,int personal,int factory,int helper,
                                  uint64_t deadline,uint64_t* job);
int BrokerPrepareCeRuntimeSelection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
                                    int groups,int shared,int factory,int helper,
                                    uint64_t deadline,uint64_t* job);
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
