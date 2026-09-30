#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "broker_owner.h"
#include "context.h"
#include "control.h"
#include "broker_owner_package.h"
#include "broker_owner_selection.h"
#include "package_policy.h"
#include "ce_private.h"
#include "namespace.h"
#include "uid_layout.h"
#include <sys/vfs.h>
#include <linux/magic.h>
#include <android-base/unique_fd.h>
#include <array>
#include <memory>
#include <new>
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define MAX_CONTEXTS 16
#define MAX_PUBLICATIONS 16
using android::base::unique_fd;
using namespace aegis;
struct publication_slot {
    PackagePublication plan;
    std::array<unique_fd,4> inputs;
    PackagePublisher* publisher = nullptr;
    PublicationState state = PublicationState::Prepared;
    PackagePublicationResult result;
    void close_inputs() { for(auto& fd:inputs)fd.reset(); }
    bool resources() const { return state==PublicationState::Prepared || publisher; }
};
struct execution_slot {
    PackageExecution plan;
    uint64_t valid_until_unix=0; // Carried from the owned authenticated planning result.
    std::array<unique_fd,4> inputs;
    // Stay lifecycle-owned after APT exits. Never reopen by a caller pathname
    // or export this CE reference while the same transaction awaits validation.
    unique_fd validation_stage;
    PackagePublisher* publisher = nullptr;
    bool has_target = false;
    PackagePublication target;
    std::array<unique_fd,3> target_inputs; // groups, store, publisher helper
    void close_target() { for(auto& fd:target_inputs)fd.reset(); }
    PackageExecutor* executor = nullptr;
    PackagePreparer* preparer = nullptr;
    PublicationState state = PublicationState::Prepared;
    PackageExecutionResult result;
    void close_inputs() { for(auto& fd:inputs)fd.reset(); }
    bool resources() const {
        return state==PublicationState::Prepared || preparer || executor || publisher || validation_stage.ok()
            || target_inputs[0].ok() || target_inputs[1].ok() || target_inputs[2].ok();
    }
};
struct planning_slot {
    PackagePlanning plan;
    PackagePreparationResult selected_source;
    PackageInput factory;
    PackagePlanner* worker=nullptr;
    unique_fd evidence;
    PackageBoundPlan bound;
    PlanningState state=PlanningState::Running;
    PackagePlanningResult result;
    bool resources() const { return worker || evidence.ok(); }
};
enum class SelectionPurpose { Runtime, SharedPackage, PersonalPackage };
struct runtime_selection_slot {
    SelectionPurpose purpose=SelectionPurpose::Runtime;
    PackageRuntimeSelection plan;
    PackagePreparer* worker=nullptr;
    unique_fd mount;
    RuntimeSelectionState state=RuntimeSelectionState::Selecting;
    PackagePreparationResult result;
    bool resources() const { return worker || mount.ok(); }
};
struct slot {
    uint32_t user, serial;
    struct aegis_context *context;
    struct { uint64_t public_id, local_id; } commands[AEGIS_RUNTIME_MAX_SHELLS];
};
struct aegis_broker_owner {
    pid_t process;
    int inputs[4];
    struct slot slots[MAX_CONTEXTS];
    uint64_t next_command, next_publication;
    bool selection_enabled=false,package_policy_enabled=false;
    std::array<unique_fd,3> package_policy;
    unique_fd selection_image,selection_helper,selection_directory;
    PackageInput selection_factory;
    std::array<std::unique_ptr<planning_slot>,MAX_PUBLICATIONS> planners;
    std::array<std::unique_ptr<publication_slot>,MAX_PUBLICATIONS> publications;
    std::array<std::unique_ptr<execution_slot>,MAX_PUBLICATIONS> executions;
    std::array<std::unique_ptr<runtime_selection_slot>,MAX_CONTEXTS+MAX_PUBLICATIONS> selections;
};

static int fail(int error) { errno = error; return -1; }
static int root_main(void) {
    uid_t real, effective, saved;
    gid_t greal, geffective, gsaved;
    if (getresuid(&real, &effective, &saved) < 0
            || getresgid(&greal, &geffective, &gsaved) < 0) return -1;
    if (real || effective || saved || greal || geffective || gsaved
            || getgroups(0, NULL) != 0 || syscall(SYS_getpid) != syscall(SYS_gettid))
        return fail(EPERM);
    return 0;
}
static int owned(struct aegis_broker_owner *owner) {
    if (!owner) return fail(EINVAL);
    if (owner->process != (pid_t)syscall(SYS_getpid)) return fail(EPERM);
    return root_main();
}
static int now_ns(uint64_t *output) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    *output = (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
    return 0;
}
static int remaining_ms(uint64_t deadline) {
    uint64_t now;
    if (now_ns(&now) < 0) return -1;
    if (deadline <= now) return 0;
    if (deadline > INT64_MAX || deadline - now > AEGIS_BROKER_MAX_WAIT_NS) return fail(EINVAL);
    return (int)((deadline - now) / 1000000); // Never extend the caller's budget.
}

int aegis_broker_owner_enable_selection(aegis_broker_owner* owner,int image,
        const aegis_base_receipt* receipt,int helper,int directory) {
    if(owned(owner)<0)return -1;
    if(owner->selection_enabled || owner->next_command || owner->next_publication)return fail(EALREADY);
    for(const auto& slot:owner->slots)if(slot.context)return fail(EBUSY);
    if(!receipt || receipt->sha256[64])return fail(EINVAL);
    PackageRuntimeSelection check;check.requester=10;check.serial=0;check.job=1;
    check.factory={receipt->bytes,std::string(receipt->sha256,64)};
    if(PackageRuntimeSelectionCheck(check)<0)return -1;
    unique_fd source(fcntl(image,F_DUPFD_CLOEXEC,3)),program(fcntl(helper,F_DUPFD_CLOEXEC,3)),
              state(fcntl(directory,F_DUPFD_CLOEXEC,3));
    if(!source.ok() || !program.ok() || !state.ok())return -1;
    struct stat st;
    for(int fd:{source.get(),program.get(),state.get()}) {
        int flags=fcntl(fd,F_GETFL);
        if(flags<0 || fstat(fd,&st)<0)return -1;
        if((flags&(O_ACCMODE|O_PATH))!=O_RDONLY || st.st_uid || (st.st_mode&07022))return fail(EPERM);
        if(fd==state.get()) {
            if(st.st_mode!=(S_IFDIR|0700) || st.st_gid)return fail(EPERM);
        } else if(!S_ISREG(st.st_mode) || st.st_nlink!=1)return fail(EPERM);
        if(fd==source.get() && (st.st_gid || st.st_size!=static_cast<off_t>(receipt->bytes)))return fail(ESTALE);
        if(fd==program.get() && ((st.st_gid!=0 && st.st_gid!=2000) || (st.st_mode&0555)!=0555))return fail(EPERM);
    }
    // Receipt/helper/system labels and immutable EROFS trust are established
    // by bootstrap, never by a CLI path or hash. Commit after all duplication.
    owner->selection_factory=check.factory;
    owner->selection_image=std::move(source);owner->selection_helper=std::move(program);
    owner->selection_directory=std::move(state);owner->selection_enabled=true;
    return 0;
}

int aegis_broker_owner_enable_package_policy(aegis_broker_owner* owner,const int inputs[3]) {
    if(owned(owner)<0)return -1;
    if(!owner->selection_enabled || owner->package_policy_enabled
       ||owner->next_command||owner->next_publication)return fail(EALREADY);
    if(!inputs)return fail(EINVAL);
    for(const auto& slot:owner->slots)if(slot.context)return fail(EBUSY);
    std::array<unique_fd,3> pinned;
    for(unsigned i=0;i<3;++i) {
        if(aegis_package_policy_file(inputs[i],i==0?16384:1048576)<0)return -1;
        pinned[i].reset(fcntl(inputs[i],F_DUPFD_CLOEXEC,3));if(!pinned[i].ok())return -1;
    }
    owner->package_policy=std::move(pinned);owner->package_policy_enabled=true;return 0;
}

int aegis_broker_owner_create(int parent_fd, int base_fd, int setup_fd, int init_fd,
                              struct aegis_broker_owner **output) {
    if (!output || *output) return fail(EINVAL);
    if (root_main() < 0) return -1;
    struct aegis_broker_owner *owner = new(std::nothrow) aegis_broker_owner{};
    if (!owner) return -1;
    for (unsigned i = 0; i < 4; i++) owner->inputs[i] = -1;
    int sources[] = {parent_fd, base_fd, setup_fd, init_fd};
    for (unsigned i = 0; i < 4; i++) {
        owner->inputs[i] = fcntl(sources[i], F_DUPFD_CLOEXEC, 3);
        if (owner->inputs[i] < 0) {
            int error = errno;
            for (unsigned j = 0; j < 4; j++) if (owner->inputs[j] >= 0) close(owner->inputs[j]);
            delete owner;
            return fail(error);
        }
    }
    owner->process = (pid_t)syscall(SYS_getpid);
    *output = owner;
    return 0;
}

// No public wire package command calls these APIs yet. Once registered here,
// however, package resources participate in the SAME production STOP/HELLO/
// disconnect path as runtime contexts. Only exact completed teardown is absent.
static int publication_guard(struct aegis_broker_owner* owner,uint32_t user,uint32_t serial) {
    for(const auto& slot:owner->planners)if(slot && slot->plan.requester==user) {
        if(slot->plan.serial!=serial)return fail(ESTALE);
        if(slot->state==PlanningState::Sealed)return fail(EBUSY);
    }
    for(const auto& slot:owner->selections)if(slot && slot->plan.requester==user) {
        if(slot->plan.serial!=serial)return fail(ESTALE);
        if(slot->state==RuntimeSelectionState::Sealed)return fail(EBUSY);
    }
    for(const auto& slot:owner->executions)if(slot && slot->resources() && slot->plan.requester==user) {
        if(slot->plan.serial!=serial)return fail(ESTALE);
        if(slot->state==PublicationState::Sealed)return fail(EBUSY);
    }
    for(const auto& slot:owner->publications)if(slot && slot->resources() && slot->plan.requester==user) {
        if(slot->plan.serial!=serial)return fail(ESTALE);
        if(slot->state==PublicationState::Sealed)return fail(EBUSY);
    }
    return 0;
}
static bool publication_resources(struct aegis_broker_owner* owner,uint32_t user) {
    for(const auto& slot:owner->planners)
        if(slot && slot->resources() && slot->plan.requester==user)return true;
    for(const auto& slot:owner->selections)
        if(slot && slot->resources() && slot->plan.requester==user)return true;
    for(const auto& slot:owner->executions)
        if(slot && slot->resources() && slot->plan.requester==user)return true;
    for(const auto& slot:owner->publications)
        if(slot && slot->resources() && slot->plan.requester==user)return true;
    return false;
}
static int reap_publication(publication_slot& slot,int wait) {
    if(!slot.publisher)return 0;
    PackagePublicationResult result;
    if(PackagePublisherFinish(&slot.publisher,slot.state==PublicationState::Sealed,wait,&result)<0)
        return -1;
    slot.close_inputs();slot.result=result;slot.state=PublicationState::Complete;
    return 0;
}
static int publish_execution(execution_slot& slot) {
    // Caller paths, source FDs and post-approval target changes cannot enter
    // this transition. Only the retained, checked job stage is used.
    open_how how={};how.flags=O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW;
    how.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_XDEV;
    unique_fd image(syscall(SYS_openat2,slot.validation_stage.get(),"candidate.ext4",&how,sizeof(how)));
    int error=image.ok()?0:errno;struct stat st;
    if(!error && fstat(image.get(),&st)<0)error=errno;
    if(!error && (st.st_mode!=(S_IFREG|0600) || st.st_uid || st.st_gid || st.st_nlink!=1
        || st.st_size<=0 || static_cast<uint64_t>(st.st_size)!=slot.target.candidate.bytes))error=EPERM;
    slot.state=PublicationState::Publishing;
    if(!error && PackagePublisherStart(slot.target_inputs[0].get(),slot.target_inputs[1].get(),
            image.get(),slot.target_inputs[2].get(),slot.target,&slot.publisher)<0)error=errno;
    // The child owner now holds the source and store or retains partial
    // ownership. No returned FD escapes the original registered transaction.
    image.reset();slot.validation_stage.reset();slot.close_target();
    if(error) {
        slot.result={PackageExecutionOutcome::Failed,0,error};
        if(slot.publisher) { slot.state=PublicationState::Sealed;(void)PackagePublisherCancel(slot.publisher); }
        else slot.state=PublicationState::Complete;
    }
    return 0;
}
static int reap_execution(execution_slot& slot,int wait) {
    if(slot.publisher) {
        PackagePublicationResult result;
        if(PackagePublisherFinish(&slot.publisher,slot.state==PublicationState::Sealed,wait,&result)<0)return -1;
        slot.close_inputs();slot.close_target();slot.validation_stage.reset();slot.state=PublicationState::Complete;
        auto outcome=result.publication==PackagePublish::Confirmed ? PackageExecutionOutcome::Published
            : result.publication==PackagePublish::Rejected ? PackageExecutionOutcome::Failed : PackageExecutionOutcome::Unconfirmed;
        slot.result={outcome,0,result.error,result.generation};return 0;
    }
    if(slot.preparer) {
        PackagePreparationResult result;int candidate=-1;
        if(PackagePreparerFinish(&slot.preparer,slot.state==PublicationState::Sealed,wait,&result,&candidate)<0)return -1;
        // The already registered slot owns the mount immediately, with no
        // externally visible FD or gap in requester quiescence responsibility.
        slot.inputs[2].reset(candidate);
        if(result.outcome==PackagePreparationOutcome::Prepared) {
            slot.state=PublicationState::Prepared;return 0;
        }
        slot.close_inputs();slot.close_target();slot.state=PublicationState::Complete;
        slot.result={result.outcome==PackagePreparationOutcome::Unconfirmed ? PackageExecutionOutcome::Unconfirmed
                     : PackageExecutionOutcome::Failed,0,result.error};
        return 0;
    }
    if(!slot.executor)return 0;
    PackageExecutionResult result;
    if(PackageExecutorFinish(&slot.executor,slot.state==PublicationState::Sealed,wait,&result)<0)return -1;
    slot.close_inputs();slot.result=result;
    if(result.outcome==PackageExecutionOutcome::NeedsValidation
            && slot.state!=PublicationState::Sealed && slot.validation_stage.ok()) {
        slot.state=PublicationState::AwaitingValidation;
        if(slot.has_target)return publish_execution(slot);
    } else {
        slot.validation_stage.reset();slot.close_target();slot.state=PublicationState::Complete;
        // Never publish an unowned candidate as eligible for validation.
        if(slot.result.outcome==PackageExecutionOutcome::NeedsValidation)
            slot.result={PackageExecutionOutcome::Failed,0,ECANCELED};
    }
    return 0;
}

static int reap_selection(runtime_selection_slot& slot,int wait) {
    if(!slot.worker)return 0;
    PackagePreparationResult result;int mount=-1;
    if(PackagePreparerFinish(&slot.worker,slot.state==RuntimeSelectionState::Sealed,wait,&result,&mount)<0)return -1;
    slot.mount.reset(mount);slot.result=result;
    slot.state=result.outcome==PackagePreparationOutcome::Prepared ? RuntimeSelectionState::Selected : RuntimeSelectionState::Failed;
    return 0;
}
static int reap_planning(planning_slot& slot,int wait) {
    if(!slot.worker)return 0;
    int fd=-1;PackagePlanningResult result;
    if(PackagePlannerFinish(&slot.worker,slot.state==PlanningState::Sealed,wait,&result,&fd)<0)return -1;
    slot.evidence.reset(fd);slot.result=result;
    slot.state=result.outcome==PackagePlanningResult::Outcome::Collected ? PlanningState::Collected : PlanningState::Complete;
    return 0;
}
static void seal_planning(planning_slot& slot) {
    slot.state=PlanningState::Sealed;
    if(slot.worker)(void)PackagePlannerCancel(slot.worker);
    else {
        slot.evidence.reset();slot.result.outcome=PackagePlanningResult::Outcome::Failed;
        slot.result.error=ECANCELED;slot.state=PlanningState::Complete;
    }
}
static void seal_publications(struct aegis_broker_owner* owner,uint32_t user) {
    for(auto& slot:owner->planners)if(slot && (!user || slot->plan.requester==user))seal_planning(*slot);
    for(auto& slot:owner->selections)if(slot && (!user || slot->plan.requester==user)) {
        slot->state=RuntimeSelectionState::Sealed;
        if(slot->worker)(void)PackagePreparerCancel(slot->worker);
    }
    for(auto& slot:owner->executions) {
        if(!slot || (user && slot->plan.requester!=user))continue;
        if(slot->publisher) {
            slot->state=PublicationState::Sealed;(void)PackagePublisherCancel(slot->publisher);
        } else if(slot->preparer) {
            slot->state=PublicationState::Sealed;(void)PackagePreparerCancel(slot->preparer);
        } else if(slot->state==PublicationState::Prepared || slot->state==PublicationState::AwaitingValidation) {
            slot->close_inputs();slot->close_target();slot->validation_stage.reset();
            slot->result={PackageExecutionOutcome::Failed,0,ECANCELED};
            slot->state=PublicationState::Complete;
        } else if(slot->executor) {
            slot->state=PublicationState::Sealed;(void)PackageExecutorCancel(slot->executor);
        }
    }
    for(auto& slot:owner->publications) {
        if(!slot || (user && slot->plan.requester!=user))continue;
        if(slot->state==PublicationState::Prepared) {
            slot->close_inputs();slot->result={PackagePublish::Rejected,ECANCELED};
            slot->state=PublicationState::Complete;
        } else if(slot->publisher) {
            slot->state=PublicationState::Sealed;
            (void)PackagePublisherCancel(slot->publisher);
        }
    }
}

static int stop(struct aegis_broker_owner *owner, uint32_t user, uint64_t deadline) {
    int error = 0;
    // Signal EVERY matching package worker before the first potentially blocking
    // context/child wait. A timeout never leaves later jobs unvisited.
    seal_publications(owner,user);
    for(auto& slot:owner->planners) {
        if(!slot || (user && slot->plan.requester!=user))continue;
        int left=remaining_ms(deadline);if(left<0) { if(!error)error=errno;left=0; }
        if(reap_planning(*slot,left)<0) { if(!error)error=errno; }
        else slot.reset();
    }
    for (unsigned i = 0; i < MAX_CONTEXTS; i++) {
        struct slot *slot = &owner->slots[i];
        if (!slot->context || (user && slot->user != user)) continue;
        int left = remaining_ms(deadline);
        if (left < 0) { if (!error) error = errno; left = 0; }
        // Even with no remaining budget, stop(0) seals and requests termination.
        // Keep visiting other slots after failure; never free a live context.
        if (aegis_context_stop(&slot->context, left) < 0) {
            if (!error) error = errno;
        } else {
            memset(slot, 0, sizeof(*slot));
        }
    }
    for(auto& slot:owner->selections) {
        if(!slot || (user && slot->plan.requester!=user))continue;
        int left=remaining_ms(deadline);
        if(left<0) { if(!error)error=errno;left=0; }
        if(reap_selection(*slot,left)<0) { if(!error)error=errno; }
        else slot.reset(); // Only after worker reap; closes any selected CE mount.
    }
    for(auto& slot:owner->publications) {
        if(!slot || (user && slot->plan.requester!=user))continue;
        int left=remaining_ms(deadline);
        if(left<0) { if(!error)error=errno;left=0; }
        if(reap_publication(*slot,left)<0) { if(!error)error=errno; }
        else slot.reset(); // No resources remain. IDs are never recycled.
    }
    for(auto& slot:owner->executions) {
        if(!slot || (user && slot->plan.requester!=user))continue;
        int left=remaining_ms(deadline);
        if(left<0) { if(!error)error=errno;left=0; }
        if(reap_execution(*slot,left)<0) { if(!error)error=errno; }
        else slot.reset();
    }
    return error ? fail(error) : 0;
}

int aegis_broker_owner_stop_all(struct aegis_broker_owner *owner, uint64_t deadline_ns) {
    if (owned(owner) < 0) return -1;
    // A clock/deadline error still causes stop() to visit and seal all slots.
    int initial = remaining_ms(deadline_ns), error = initial < 0 ? errno : 0;
    int stopped = stop(owner, 0, error ? 0 : deadline_ns);
    if (error) return fail(error);
    return stopped;
}

int aegis_broker_owner_apply(struct aegis_broker_owner *owner,
                             const struct aegis_broker_request *request,
                             enum aegis_broker_state *state) {
    if (!state) return fail(EINVAL);
    *state = AEGIS_BROKER_SEALED;
    if (owned(owner) < 0) return -1;
    if (!request) return fail(EINVAL);
    uint64_t now;
    if (now_ns(&now) < 0) return -1;
    struct aegis_broker_request checked;
    // Frame/identity/deadline defense in depth. The actual connection sequence
    // belongs to the transport, not this resource registry.
    uint64_t previous = request->operation == AEGIS_BROKER_HELLO ? 0
            : (request->sequence > 1 ? request->sequence - 1 : 0);
    if (aegis_broker_parse(request, sizeof(*request), previous, now, &checked) < 0) return -1;
    if (request->operation == AEGIS_BROKER_HELLO || request->operation == AEGIS_BROKER_STOP_USER) {
        if (stop(owner, request->user, request->deadline_ns) < 0) return -1;
        *state = AEGIS_BROKER_ABSENT;
        return 0;
    }
    if(publication_guard(owner,request->user,request->serial)<0)return -1;
    struct slot *empty = NULL;
    for (unsigned i = 0; i < MAX_CONTEXTS; i++) {
        struct slot *slot = &owner->slots[i];
        if (!slot->context) { if (!empty) empty = slot; continue; }
        if (slot->user != request->user) continue;
        if (slot->serial != request->serial) return fail(ESTALE);
        if (aegis_context_channel(slot->context) < 0) {
            if (request->operation == AEGIS_BROKER_STATUS) return 0; // SEALED, cleanup still owned.
            return fail(EBUSY); // Explicit confirmed STOP precedes another start.
        }
        *state = AEGIS_BROKER_READY;
        return 0;
    }
    if (request->operation == AEGIS_BROKER_STATUS) {
        *state = publication_resources(owner,request->user) ? AEGIS_BROKER_SEALED : AEGIS_BROKER_ABSENT;
        return 0;
    }
    if (!empty) return fail(ENOSPC);
    runtime_selection_slot* selected=nullptr;
    for(auto& candidate:owner->selections)if(candidate && candidate->purpose==SelectionPurpose::Runtime && candidate->plan.requester==request->user) {
        selected=candidate.get();break;
    }
    if(!selected)for(const auto& candidate:owner->selections)
        if(candidate && candidate->plan.requester==request->user
                && candidate->purpose!=SelectionPurpose::Runtime)return fail(EBUSY);
    if(selected) {
        if(reap_selection(*selected,0)<0)return errno==ETIMEDOUT ? fail(EAGAIN) : -1;
        if(selected->state!=RuntimeSelectionState::Selected || !selected->mount.ok())
            return fail(selected->result.error ? selected->result.error : EBUSY);
    }
    empty->user = request->user; empty->serial = request->serial;
    int started=selected ? aegis_context_start_selected_until(request->user,request->serial,owner->inputs[0],
            selected->mount.get(),owner->inputs[2],owner->inputs[3],1,request->deadline_ns,&empty->context)
        : aegis_context_start_until(request->user,request->serial,owner->inputs[0],owner->inputs[1],
            owner->inputs[2],owner->inputs[3],1,request->deadline_ns,&empty->context);
    if (started < 0) {
        int error = errno;
        // The context API deliberately returns partial ownership on failure.
        if(selected)selected->state=RuntimeSelectionState::Sealed;
        if (!empty->context) memset(empty, 0, sizeof(*empty));
        return fail(error);
    }
    if(selected) {
        // Context owns its ID-mapped view. No selected CE mount copy stays in
        // the host registry after successful adoption; metadata remains bound.
        selected->mount.reset();selected->state=RuntimeSelectionState::Activated;
    }
    if (remaining_ms(request->deadline_ns) <= 0) {
        // Do not publish READY after the caller's authority/lease has expired.
        // Best-effort stop retains the slot until actual cleanup is confirmed.
        (void)stop(owner, request->user, 0);
        return fail(ETIMEDOUT);
    }
    *state = AEGIS_BROKER_READY;
    return 0;
}

int aegis_broker_owner_release(struct aegis_broker_owner **output) {
    if (!output || !*output) return fail(EINVAL);
    struct aegis_broker_owner *owner = *output;
    if (owned(owner) < 0) return -1;
    for (unsigned i = 0; i < MAX_CONTEXTS; i++) if (owner->slots[i].context) return fail(EBUSY);
    for(const auto& slot:owner->planners)if(slot && slot->resources())return fail(EBUSY);
    for(const auto& slot:owner->publications)if(slot && slot->resources())return fail(EBUSY);
    for(const auto& slot:owner->executions)if(slot && slot->resources())return fail(EBUSY);
    for(const auto& slot:owner->selections)if(slot && slot->resources())return fail(EBUSY);
    for (unsigned i = 0; i < 4; i++) close(owner->inputs[i]);
    delete owner; *output = NULL;
    return 0;
}

static struct slot *terminal_slot(struct aegis_broker_owner *owner,
                                  const struct aegis_broker_request *request, uint16_t operation) {
    if (owned(owner) < 0) return NULL;
    if (!request || request->operation != operation) { errno = EINVAL; return NULL; }
    uint64_t now;
    if (now_ns(&now) < 0) return NULL;
    // Recheck the existing lifecycle envelope without accepting a terminal
    // operation as a 32-byte wire request. Actual sequencing stays in serve().
    struct aegis_broker_request header = *request, checked;
    header.operation = AEGIS_BROKER_START;
    uint64_t previous = header.sequence > 1 ? header.sequence - 1 : 0;
    if (aegis_broker_parse(&header, sizeof(header), previous, now, &checked) < 0) return NULL;
    if(publication_guard(owner,request->user,request->serial)<0)return NULL;
    for (unsigned i = 0; i < MAX_CONTEXTS; i++) {
        struct slot *slot = &owner->slots[i];
        if (!slot->context || slot->user != request->user) continue;
        if (slot->serial != request->serial) { errno = ESTALE; return NULL; }
        if (aegis_context_channel(slot->context) < 0) return NULL;
        return slot;
    }
    errno = ENOENT;
    return NULL;
}

int aegis_broker_owner_exec(struct aegis_broker_owner *owner,
                            const struct aegis_broker_call *call,
                            uint64_t *command, int *master) {
    if (!call || !command || *command || !master || *master != -1) return fail(EINVAL);
    struct slot *slot = terminal_slot(owner, &call->request, AEGIS_BROKER_EXEC);
    if (!slot) return -1;
    const char *argv[AEGIS_BROKER_MAX_ARGS + 1];
    if (aegis_broker_arguments(call, argv) < 0) return -1;
    unsigned position;
    for (position = 0; position < AEGIS_RUNTIME_MAX_SHELLS; position++)
        if (!slot->commands[position].public_id) break;
    if (position == AEGIS_RUNTIME_MAX_SHELLS) return fail(ENOSPC);
    if (owner->next_command == INT64_MAX) return fail(EOVERFLOW);
    // Burn IDs even on uncertain execution: never reassign them on this owner.
    uint64_t public_id = ++owner->next_command, local_id = 0;
    int descriptor = -1;
    if (aegis_context_exec(slot->context, call->argc, argv, call->request.deadline_ns,
            &local_id, &descriptor) < 0) return -1;
    slot->commands[position].public_id = public_id;
    slot->commands[position].local_id = local_id;
    if (remaining_ms(call->request.deadline_ns) <= 0) {
        close(descriptor);
        (void)stop(owner, call->request.user, 0);
        return fail(ETIMEDOUT);
    }
    *command = public_id; *master = descriptor;
    return 0;
}

int aegis_broker_owner_result(struct aegis_broker_owner *owner,
                              const struct aegis_broker_request *request,
                              uint64_t command, int *wait_status, int *exited) {
    if (!command || command > INT64_MAX || !wait_status || !exited) return fail(EINVAL);
    struct slot *slot = terminal_slot(owner, request, AEGIS_BROKER_RESULT);
    if (!slot) return -1;
    for (unsigned i = 0; i < AEGIS_RUNTIME_MAX_SHELLS; i++) {
        if (slot->commands[i].public_id != command) continue;
        int status;
        if (aegis_context_result(slot->context, slot->commands[i].local_id, &status) < 0) {
            if (errno != EAGAIN) return -1;
            *wait_status = 0; *exited = 0;
            return 0;
        }
        memset(&slot->commands[i], 0, sizeof(slot->commands[i]));
        *wait_status = status; *exited = 1;
        return 0;
    }
    return fail(ENOENT);
}

int aegis_broker_owner_reap_publications(struct aegis_broker_owner* owner) {
    if(owned(owner)<0)return -1;
    int error=0;
    for(auto& slot:owner->planners)if(slot && slot->worker) {
        if(reap_planning(*slot,0)<0 && errno!=ETIMEDOUT) {
            if(!error)error=errno;seal_planning(*slot);
        }
    }
    for(auto& slot:owner->selections)if(slot && slot->worker) {
        if(reap_selection(*slot,0)<0 && errno!=ETIMEDOUT) {
            if(!error)error=errno;
            slot->state=RuntimeSelectionState::Sealed;(void)PackagePreparerCancel(slot->worker);
        }
    }
    for(auto& slot:owner->publications)if(slot && slot->publisher) {
        if(reap_publication(*slot,0)<0 && errno!=ETIMEDOUT) {
            if(!error)error=errno;
            slot->state=PublicationState::Sealed;
            (void)PackagePublisherCancel(slot->publisher);
        }
    }
    for(auto& slot:owner->executions)if(slot && (slot->executor || slot->preparer || slot->publisher)) {
        if(reap_execution(*slot,0)<0 && errno!=ETIMEDOUT) {
            if(!error)error=errno;
            slot->state=PublicationState::Sealed;
            if(slot->preparer)(void)PackagePreparerCancel(slot->preparer);
            if(slot->executor)(void)PackageExecutorCancel(slot->executor);
            if(slot->publisher)(void)PackagePublisherCancel(slot->publisher);
        }
    }
    return error ? fail(error) : 0;
}

namespace aegis {
namespace {
int admission(aegis_broker_owner* owner,uint32_t user,uint32_t serial,uint64_t deadline) {
    if(owned(owner)<0)return -1;
    if(user<10 || user>=21473 || serial>INT32_MAX)return fail(EINVAL);
    int left=remaining_ms(deadline);if(left<0)return -1;if(!left)return fail(ETIMEDOUT);
    if(publication_guard(owner,user,serial)<0)return -1;
    for(const auto& context:owner->slots)if(context.context && context.user==user) {
        if(context.serial!=serial)return fail(ESTALE);
        if(aegis_context_channel(context.context)<0)return fail(EBUSY);
    }
    return 0;
}
int capacity(aegis_broker_owner* owner,uint32_t user,const runtime_selection_slot* transferring=nullptr, const planning_slot* planning=nullptr) {
    unsigned count=0;
    for(const auto& slot:owner->planners)if(slot && slot.get()!=planning) {
        count++;if(slot->plan.requester==user)return fail(EBUSY);
    }
    for(const auto& slot:owner->selections)if(slot && slot.get()!=transferring
            && !(slot->purpose==SelectionPurpose::Runtime && slot->state==RuntimeSelectionState::Activated)) {
        count++;if(slot->plan.requester==user)return fail(EBUSY);
    }
    for(const auto& slot:owner->publications)if(slot) {
        count++;if(slot->plan.requester==user)return fail(EBUSY);
    }
    for(const auto& slot:owner->executions)if(slot) {
        count++;if(slot->plan.requester==user)return fail(EBUSY);
    }
    return count>=MAX_PUBLICATIONS ? fail(ENOSPC) : 0;
}
publication_slot* find(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                       uint64_t job,const std::string& plan) {
    if(owned(owner)<0)return nullptr;
    for(auto& slot:owner->publications)if(slot && slot->plan.job==job) {
        if(slot->plan.requester!=user || slot->plan.serial!=serial || slot->plan.plan_sha256!=plan) {
            fail(ESTALE);return nullptr;
        }
        return slot.get();
    }
    fail(ENOENT);return nullptr;
}
execution_slot* find_execution(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                       uint64_t job,const std::string& plan) {
    if(owned(owner)<0)return nullptr;
    for(auto& slot:owner->executions)if(slot && slot->plan.job==job) {
        if(slot->plan.requester!=user || slot->plan.serial!=serial || slot->plan.plan_sha256!=plan) {
            fail(ESTALE);return nullptr;
        }
        return slot.get();
    }
    fail(ENOENT);return nullptr;
}
}
static int start_planning(aegis_broker_owner* owner,const PackagePlanning& request,
                         int groups,int factory,int selected,int sources,int key,int helper,
                         uint64_t deadline,uint64_t* job,runtime_selection_slot* transferring,int network_helper,int ca_bundle) {
    if(!job||*job||request.job)return fail(EINVAL);
    if(admission(owner,request.requester,request.serial,deadline)<0||capacity(owner,request.requester,transferring)<0)return -1;
    if(!transferring&&owner->next_publication==INT64_MAX)return fail(EOVERFLOW);
    if(transferring && !request.personal && transferring->result.scope==PackagePreparationResult::Scope::Personal)return fail(EINVAL);
    PackagePlanning plan=request;plan.job=transferring?transferring->plan.job:owner->next_publication+1;
    if(PackagePlanningCheck(plan)<0)return -1;
    std::unique_ptr<planning_slot>* empty=nullptr;
    for(auto& slot:owner->planners)if(!slot) { empty=&slot;break; }
    if(!empty)return fail(ENOSPC);
    auto slot=std::unique_ptr<planning_slot>(new(std::nothrow) planning_slot);if(!slot)return fail(ENOMEM);
    slot->plan=plan;
    if(transferring) { slot->selected_source=transferring->result;slot->factory=transferring->plan.factory; }
    *empty=std::move(slot);if(!transferring)owner->next_publication=plan.job;*job=plan.job;
    auto& registered=**empty;
    int started=PackagePlannerStart(groups,factory,selected,sources,key,helper,plan,deadline,&registered.worker,network_helper,ca_bundle);
    int start_error=errno;
    // The original selection remains owned until Start has either duplicated
    // its mount into the new registered worker or failed. No FD leaves the owner.
    if(transferring)for(auto& old:owner->selections)if(old.get()==transferring) { old.reset();break; }
    if(started<0) {
        int e=start_error;registered.result.outcome=PackagePlanningResult::Outcome::Failed;registered.result.error=e;
        if(registered.worker)seal_planning(registered);else registered.state=PlanningState::Complete;
        return fail(e);
    }
    return 0;
}
int BrokerStartPlanning(aegis_broker_owner* owner,const PackagePlanning& request,
                         int groups,int factory,int selected,int sources,int key,int helper,
                         uint64_t deadline,uint64_t* job,int network_helper,int ca_bundle) {
    return start_planning(owner,request,groups,factory,selected,sources,key,helper,deadline,job,nullptr,network_helper,ca_bundle);
}
int BrokerStartPlanningFromSelection(aegis_broker_owner* owner,const PackagePlanning& request,
                                      uint64_t selection_job,int groups,int factory,int sources,int key,int helper,
                                      uint64_t deadline,uint64_t* job,int network_helper,int ca_bundle) {
    if(!selection_job)return fail(EINVAL);
    if(admission(owner,request.requester,request.serial,deadline)<0)return -1;
    for(auto& selected:owner->selections)if(selected && selected->plan.job==selection_job) {
        if(selected->plan.requester!=request.requester||selected->plan.serial!=request.serial)return fail(ESTALE);
        if(selected->purpose==SelectionPurpose::Runtime)return fail(EPERM);
        if((selected->purpose==SelectionPurpose::PersonalPackage)!=request.personal)return fail(ESTALE);
        if(reap_selection(*selected,0)<0)return errno==ETIMEDOUT?fail(EAGAIN):-1;
        if(selected->state!=RuntimeSelectionState::Selected||!selected->mount.ok())return fail(EBUSY);
        return start_planning(owner,request,groups,factory,selected->mount.get(),sources,key,helper,deadline,job,selected.get(),network_helper,ca_bundle);
    }
    return fail(ENOENT);
}
static planning_slot* find_planning(aegis_broker_owner* owner,uint32_t user,uint32_t serial,uint64_t job) {
    if(!job||job>INT64_MAX) { fail(EINVAL);return nullptr; }
    if(owned(owner)<0)return nullptr;
    for(auto& slot:owner->planners)if(slot && slot->plan.job==job) {
        if(slot->plan.requester!=user||slot->plan.serial!=serial) { fail(ESTALE);return nullptr; }
        return slot.get();
    }
    fail(ENOENT);return nullptr;
}
int BrokerPollPlanning(aegis_broker_owner* owner,uint32_t user,uint32_t serial,uint64_t job,
                        PlanningState* state,PackagePlanningResult* result) {
    if(!state||!result)return fail(EINVAL);
    auto* slot=find_planning(owner,user,serial,job);if(!slot)return -1;
    if(reap_planning(*slot,0)<0&&errno!=ETIMEDOUT)return -1;
    *state=slot->state;*result=slot->result;
    if(slot->state==PlanningState::Complete)for(auto& item:owner->planners)if(item.get()==slot) { item.reset();break; }
    return 0;
}
int BrokerReviewPlanning(aegis_broker_owner* owner,uint32_t user,uint32_t serial,uint64_t job,
                           uint64_t deadline,PackageBoundPlan* output) {
    if(!output)return fail(EINVAL);
    if(admission(owner,user,serial,deadline)<0)return -1;
    auto* slot=find_planning(owner,user,serial,job);if(!slot)return -1;
    if(reap_planning(*slot,0)<0)return errno==ETIMEDOUT?fail(EAGAIN):-1;
    if(slot->state!=PlanningState::Collected && slot->state!=PlanningState::Reviewed)return fail(EBUSY);
    if(slot->selected_source.outcome!=PackagePreparationOutcome::Prepared
       ||slot->selected_source.scope==PackagePreparationResult::Scope::None)return fail(EPERM);
    const auto& selected=slot->selected_source;
    auto plan=slot->result.evidence;
    plan.requester=user;plan.serial=serial;plan.personal=slot->plan.personal;plan.create_store=slot->plan.create_store;
    plan.action=slot->plan.request.action;plan.requested_package=slot->plan.request.package;plan.requested_version=slot->plan.request.version;
    plan.source={selected.generation.bytes,selected.generation.image_sha256};plan.shared=selected.shared;
    plan.planner_image_sha256=slot->factory.sha256;
    plan.has_previous=plan.personal ? selected.scope==PackagePreparationResult::Scope::Personal
                                   : selected.scope==PackagePreparationResult::Scope::Shared;
    if(plan.has_previous)plan.previous=selected.generation;
    auto now=time(nullptr);if(now<=0)return fail(EIO);
    PackageBoundPlan bound;if(PackageBindResolvedPlan(plan,uint64_t(now),&bound)<0)return -1;
    if(slot->state==PlanningState::Reviewed && slot->bound.preparation.execution.plan_sha256!=bound.preparation.execution.plan_sha256)return fail(ESTALE);
    slot->bound=bound;slot->state=PlanningState::Reviewed;*output=std::move(bound);return 0;
}
int BrokerCancelPlanning(aegis_broker_owner* owner,uint32_t user,uint32_t serial,uint64_t job,uint64_t deadline) {
    auto* slot=find_planning(owner,user,serial,job);if(!slot)return -1;
    seal_planning(*slot);int left=remaining_ms(deadline);if(left<0)return -1;
    if(reap_planning(*slot,left)<0)return -1;
    for(auto& item:owner->planners)if(item.get()==slot) { item.reset();break; }
    return 0;
}

static int optional_shared_store(int state,int* output) {
    open_how how={};how.flags=O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW;
    how.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_XDEV;
    unique_fd store(syscall(SYS_openat2,state,"shared-packages",&how,sizeof(how)));
    if(!store.ok())return errno==ENOENT ? 0 : -1;
    struct stat st,named,parent;
    if(fstat(store.get(),&st)<0 || fstat(state,&parent)<0
            || fstatat(state,"shared-packages",&named,AT_SYMLINK_NOFOLLOW)<0)return -1;
    if(st.st_mode!=(S_IFDIR|0700) || st.st_uid || st.st_gid || st.st_dev!=parent.st_dev
            || st.st_dev!=named.st_dev || st.st_ino!=named.st_ino)return fail(EPERM);
    char label[128]={};constexpr char expected[]="u:object_r:aegis_package_shared_file:s0";
    ssize_t count=fgetxattr(store.get(),"security.selinux",label,sizeof(label));
    if(count<0)return -1;
    if((count!=static_cast<ssize_t>(sizeof(expected)) && count!=static_cast<ssize_t>(sizeof(expected))-1)
            || memcmp(label,expected,sizeof(expected)-1)
            || (count==static_cast<ssize_t>(sizeof(expected)) && label[count-1]))return fail(EPERM);
    // Presence, even empty/corrupt, is passed to the strict read-only selector.
    // Only an absent fixed child is fallback. No directory creation or repair.
    *output=store.release();return 0;
}

static int prepare_runtime_selection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
                                     int groups,int shared,int personal,int factory,int helper,
                                     uint64_t deadline,uint64_t* job,bool ce,int state_root=-1,
                                     SelectionPurpose purpose=SelectionPurpose::Runtime) {
    if(!job || *job || request.job || shared < -1 || personal < -1)return fail(EINVAL);
    if(admission(owner,request.requester,request.serial,deadline)<0)return -1;
    if(purpose==SelectionPurpose::SharedPackage && (ce || personal!=-1))return fail(EINVAL);
    if(purpose==SelectionPurpose::Runtime) {
        for(const auto& slot:owner->slots)if(slot.context && slot.user==request.requester)return fail(EALREADY);
        for(const auto& slot:owner->selections)if(slot && slot->purpose==purpose
                && slot->plan.requester==request.requester)return fail(EALREADY);
    }
    if(capacity(owner,request.requester)<0)return -1;
    if(owner->next_publication==INT64_MAX)return fail(EOVERFLOW);
    auto plan=request;plan.job=owner->next_publication+1;
    if(PackageRuntimeSelectionCheck(plan)<0)return -1;
    std::unique_ptr<runtime_selection_slot>* empty=nullptr;
    for(auto& slot:owner->selections)if(!slot) { empty=&slot;break; }
    if(!empty)return fail(ENOSPC);
    auto slot=std::unique_ptr<runtime_selection_slot>(new(std::nothrow) runtime_selection_slot);
    if(!slot)return fail(ENOMEM);slot->plan=plan;slot->purpose=purpose;
    *job=++owner->next_publication;*empty=std::move(slot);
    auto& registered=**empty; // Own BEFORE opening CE, without deferred CLI FDs.
    unique_fd private_store,shared_store;int error=0;
    if(state_root>=0) {
        int fd=-1;if(optional_shared_store(state_root,&fd)<0)error=errno;
        shared_store.reset(fd);shared=fd;
    }
    if(!error && ce) {
        if(aegis_namespace_check_broker()<0)error=errno;
        else {
            unique_fd data(open("/data",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
            int fd=-1;
            if(!data.ok() || aegis_ce_find_package_store(data.get(),plan.requester,plan.serial,&fd)<0)error=errno;
            private_store.reset(fd);personal=fd;
        }
    }
    if(!error && remaining_ms(deadline)<=0)error=ETIMEDOUT;
    if(!error && PackageRuntimeSelectionStart(groups,shared,personal,factory,helper,plan,&registered.worker)<0)error=errno;
    if(!error && remaining_ms(deadline)<=0)error=ETIMEDOUT;
    if(error) {
        registered.result={PackagePreparationOutcome::Failed,error};
        if(registered.worker) { registered.state=RuntimeSelectionState::Sealed;(void)PackagePreparerCancel(registered.worker); }
        else registered.state=RuntimeSelectionState::Failed;
        return fail(error);
    }
    return 0;
}
int BrokerPrepareRuntimeSelection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
                                  int groups,int shared,int personal,int factory,int helper,
                                  uint64_t deadline,uint64_t* job) {
    return prepare_runtime_selection(owner,request,groups,shared,personal,factory,helper,deadline,job,false);
}
int BrokerPrepareCeRuntimeSelection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
                                    int groups,int shared,int factory,int helper,uint64_t deadline,uint64_t* job) {
    return prepare_runtime_selection(owner,request,groups,shared,-1,factory,helper,deadline,job,true);
}
int BrokerPreparePackageSelection(aegis_broker_owner* owner,const PackageRuntimeSelection& request,
                                  bool personal_scope,int groups,int shared,int personal,int factory,int helper,
                                  uint64_t deadline,uint64_t* job) {
    return prepare_runtime_selection(owner,request,groups,shared,personal,factory,helper,deadline,job,false,-1,
        personal_scope ? SelectionPurpose::PersonalPackage : SelectionPurpose::SharedPackage);
}
int BrokerPrepareConfiguredPackageSelection(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                                             bool personal,uint64_t deadline,uint64_t* job) {
    if(owned(owner)<0)return -1;
    if(!owner->selection_enabled)return fail(ENOTSUP);
    PackageRuntimeSelection plan;plan.requester=user;plan.serial=serial;plan.factory=owner->selection_factory;
    return prepare_runtime_selection(owner,plan,owner->inputs[0],-1,-1,
        owner->selection_image.get(),owner->selection_helper.get(),deadline,job,personal,
        owner->selection_directory.get(),personal ? SelectionPurpose::PersonalPackage : SelectionPurpose::SharedPackage);
}
int BrokerCancelPackageSelection(aegis_broker_owner* owner,uint32_t user,uint32_t serial,uint64_t job,uint64_t deadline) {
    if(owned(owner)<0)return -1;
    if(!job || job>INT64_MAX)return fail(EINVAL);
    for(auto& slot:owner->selections)if(slot && slot->plan.job==job) {
        if(slot->plan.requester!=user || slot->plan.serial!=serial)return fail(ESTALE);
        if(slot->purpose==SelectionPurpose::Runtime)return fail(EPERM);
        slot->state=RuntimeSelectionState::Sealed;
        if(slot->worker)(void)PackagePreparerCancel(slot->worker);
        int left=remaining_ms(deadline);if(left<0)return -1;
        if(reap_selection(*slot,left)<0)return -1;
        slot.reset();return 0;
    }
    return fail(ENOENT);
}
int BrokerPollRuntimeSelection(aegis_broker_owner* owner,uint32_t user,uint32_t serial,uint64_t job,
                               RuntimeSelectionState* state,PackagePreparationResult* result) {
    if(!state || !result || !job)return fail(EINVAL);
    if(owned(owner)<0)return -1;
    for(auto& slot:owner->selections)if(slot && slot->plan.job==job) {
        if(slot->plan.requester!=user || slot->plan.serial!=serial)return fail(ESTALE);
        if(reap_selection(*slot,0)<0 && errno!=ETIMEDOUT)return -1;
        *state=slot->state;*result=slot->result;return 0;
    }
    return fail(ENOENT);
}

static int personal_area(uint32_t user,uint32_t serial) {
    // Namespace provenance is pinned by init, never selected by a CLI. The
    // authenticated id+serial is the REQUESTER, never the approving admin.
    if(aegis_namespace_check_broker()<0)return -1;
    unique_fd data(open("/data",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    if(!data.ok())return -1;
    return aegis_ce_open_packages(data.get(),user,serial,1);
}
static int prepare_publication(aegis_broker_owner* owner,const PackagePublication& request,
                               int groups,int store,int source,int helper,uint64_t deadline,
                               uint64_t* job,bool personal_ce) {
    if(!job || *job || request.job || (personal_ce && !request.personal))return fail(EINVAL);
    if(admission(owner,request.requester,request.serial,deadline)<0)return -1;
    if(capacity(owner,request.requester)<0)return -1;
    if(owner->next_publication==INT64_MAX)return fail(EOVERFLOW);
    PackagePublication plan=request;plan.job=owner->next_publication+1;
    if(PackagePublicationCheck(plan)<0)return -1;
    std::unique_ptr<publication_slot>* empty=nullptr;
    for(auto& slot:owner->publications) {
        if(!slot) { if(!empty)empty=&slot;continue; }
        // Uncollected results count too: one requester must not fill every
        // global slot with completed jobs and starve the other users.
        if(slot->plan.requester==request.requester)return fail(EBUSY);
    }
    if(!empty)return fail(ENOSPC);
    auto prepared=std::unique_ptr<publication_slot>(new(std::nothrow) publication_slot);
    if(!prepared)return fail(ENOMEM);
    prepared->plan=std::move(plan);
    int fds[]={groups,store,source,helper};
    for(unsigned i=0;i<4;++i) {
        if(personal_ce && i==1)continue;
        prepared->inputs[i].reset(fcntl(fds[i],F_DUPFD_CLOEXEC,3));
        if(!prepared->inputs[i].ok())return -1;
    }
    if(remaining_ms(deadline)<=0)return fail(ETIMEDOUT);
    *job=++owner->next_publication;*empty=std::move(prepared);
    auto& slot=**empty;
    // Register before opening any CE reference or creating package metadata.
    // Every error consumes this job and retains a collectable failed result.
    if(personal_ce) {
        int error=0;
        {
            unique_fd area(personal_area(request.requester,request.serial));
            if(!area.ok())error=errno;
            else {
                slot.inputs[1].reset(aegis_ce_package_store(area.get(),request.requester,request.serial));
                if(!slot.inputs[1].ok())error=errno;
            }
        }
        if(!error && remaining_ms(deadline)<=0)error=ETIMEDOUT;
        if(error) {
            slot.close_inputs();slot.state=PublicationState::Complete;
            slot.result={PackagePublish::Rejected,error};return fail(error);
        }
    }
    return 0;
}
int BrokerPreparePublication(aegis_broker_owner* owner,const PackagePublication& request,
                             int groups,int store,int source,int helper,uint64_t deadline,uint64_t* job) {
    return prepare_publication(owner,request,groups,store,source,helper,deadline,job,false);
}
int BrokerPreparePersonalPublication(aegis_broker_owner* owner,const PackagePublication& request,
                                     int groups,int source,int helper,uint64_t deadline,uint64_t* job) {
    return prepare_publication(owner,request,groups,-1,source,helper,deadline,job,true);
}
int BrokerStartPublication(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                           uint64_t job,const std::string& plan,uint64_t deadline) {
    auto* slot=find(owner,user,serial,job,plan);if(!slot)return -1;
    if(slot->state!=PublicationState::Prepared)return fail(EALREADY);
    if(admission(owner,user,serial,deadline)<0)return -1;
    // This consumes the preparation even on failure. Every partial child/FD
    // remains in the pre-registered slot before caller admission is released.
    slot->state=PublicationState::Running;
    int result=PackagePublisherStart(slot->inputs[0].get(),slot->inputs[1].get(),
        slot->inputs[2].get(),slot->inputs[3].get(),slot->plan,&slot->publisher);
    int error=errno;slot->close_inputs();
    if(result<0) {
        if(slot->publisher) { slot->state=PublicationState::Sealed;(void)PackagePublisherCancel(slot->publisher); }
        else { slot->state=PublicationState::Complete;slot->result={PackagePublish::Rejected,error}; }
        return fail(error);
    }
    if(remaining_ms(deadline)<=0) {
        slot->state=PublicationState::Sealed;(void)PackagePublisherCancel(slot->publisher);
        return fail(ETIMEDOUT);
    }
    return 0;
}
int BrokerPollPublication(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                          uint64_t job,const std::string& plan,PublicationState* state,
                          PackagePublicationResult* result) {
    if(!state || !result)return fail(EINVAL);
    auto* slot=find(owner,user,serial,job,plan);if(!slot)return -1;
    if(slot->publisher && reap_publication(*slot,0)<0 && errno!=ETIMEDOUT)return -1;
    *state=slot->state;
    if(slot->state==PublicationState::Complete) {
        *result=slot->result;
        for(auto& entry:owner->publications)if(entry.get()==slot) { entry.reset();break; }
    }
    return 0;
}
int BrokerCancelPublication(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                            uint64_t job,const std::string& plan,uint64_t deadline) {
    auto* slot=find(owner,user,serial,job,plan);if(!slot)return -1;
    int left=remaining_ms(deadline),error=left<0?errno:0;
    // Invalid/expired time still seals and requests this exact owned job's stop.
    if(slot->state==PublicationState::Prepared) {
        slot->close_inputs();slot->state=PublicationState::Complete;
        slot->result={PackagePublish::Rejected,ECANCELED};
    } else if(slot->publisher) {
        slot->state=PublicationState::Sealed;(void)PackagePublisherCancel(slot->publisher);
        if(reap_publication(*slot,left<0?0:left)<0)return -1;
    }
    return error ? fail(error) : 0;
}
static int prepare_candidate(aegis_broker_owner* owner,const PackagePreparation& request,
                              int groups,int stage,int source,int prepare_helper,int execute_helper,
                              const std::vector<int>& archives,uint64_t deadline,uint64_t* job,bool personal_ce,
                              const PackagePublication* target=nullptr,int store=-1,int publish_helper=-1,planning_slot* transferring=nullptr) {
    if(!job || *job || request.execution.job)return fail(EINVAL);
    const auto& identity=request.execution;
    if(admission(owner,identity.requester,identity.serial,deadline)<0 || capacity(owner,identity.requester,nullptr,transferring)<0)return -1;
    if(!transferring && owner->next_publication==INT64_MAX)return fail(EOVERFLOW);
    PackagePreparation preparation=request;preparation.execution.job=transferring?transferring->plan.job:owner->next_publication+1;
    if(PackagePreparationCheck(preparation)<0 || archives.size()!=preparation.archives.size())return fail(EINVAL);
    std::unique_ptr<execution_slot>* empty=nullptr;
    for(auto& slot:owner->executions)if(!slot) { empty=&slot;break; }
    if(!empty)return fail(ENOSPC);
    auto prepared=std::unique_ptr<execution_slot>(new(std::nothrow) execution_slot);
    if(!prepared)return fail(ENOMEM);
    if(target) {
        prepared->has_target=true;prepared->target=*target;prepared->target.job=preparation.execution.job;
        prepared->target_inputs[0].reset(fcntl(groups,F_DUPFD_CLOEXEC,3));
        if(!personal_ce)prepared->target_inputs[1].reset(fcntl(store,F_DUPFD_CLOEXEC,3));
        prepared->target_inputs[2].reset(fcntl(publish_helper,F_DUPFD_CLOEXEC,3));
        if(!prepared->target_inputs[0].ok() || (!personal_ce && !prepared->target_inputs[1].ok())
           || !prepared->target_inputs[2].ok())return -1;
    }
    prepared->plan=preparation.execution;prepared->state=PublicationState::Preparing;
    prepared->inputs[0].reset(fcntl(groups,F_DUPFD_CLOEXEC,3));
    if(!personal_ce)prepared->inputs[1].reset(fcntl(stage,F_DUPFD_CLOEXEC,3));
    prepared->inputs[3].reset(fcntl(execute_helper,F_DUPFD_CLOEXEC,3));
    if(!prepared->inputs[0].ok() || (!personal_ce && !prepared->inputs[1].ok()) || !prepared->inputs[3].ok())return -1;
    if(remaining_ms(deadline)<=0)return fail(ETIMEDOUT);
    if(transferring)prepared->valid_until_unix=transferring->bound.valid_until_unix;
    *job=preparation.execution.job;if(!transferring)owner->next_publication=*job;
    *empty=std::move(prepared);auto& slot=**empty;
    // The same job is now registered in its execution phase. Pinned local
    // archive FDs remain owned by the handoff until PreparerStart duplicates them.
    if(transferring)for(auto& old:owner->planners)if(old.get()==transferring) { old.reset();break; }
    // Register BEFORE any private CE reference, disk mutation or child.
    int error=0;
    if(personal_ce) {
        unique_fd area(personal_area(identity.requester,identity.serial));
        if(!area.ok())error=errno;
        else {
            slot.inputs[1].reset(aegis_ce_new_package_stage(area.get(),identity.requester,identity.serial,*job));
            if(!slot.inputs[1].ok())error=errno;
            if(!error && target) {
                slot.target_inputs[1].reset(aegis_ce_package_store(area.get(),identity.requester,identity.serial));
                if(!slot.target_inputs[1].ok())error=errno;
            }
        }
    }
    if(!error && remaining_ms(deadline)<=0)error=ETIMEDOUT;
    if(!error && PackagePreparerStart(groups,slot.inputs[1].get(),source,prepare_helper,archives,
                                     preparation,&slot.preparer)<0)error=errno;
    if(!error && remaining_ms(deadline)<=0)error=ETIMEDOUT;
    if(error) {
        if(slot.preparer) { slot.state=PublicationState::Sealed;(void)PackagePreparerCancel(slot.preparer); }
        else { slot.close_inputs();slot.close_target();slot.state=PublicationState::Complete;
               slot.result={PackageExecutionOutcome::Failed,0,error}; }
        return fail(error);
    }
    return 0;
}
static int transaction_target(const PackagePreparation& preparation,const PackagePublication& target) {
    const auto& p=preparation.execution;
    if(target.job || p.job || target.requester!=p.requester || target.serial!=p.serial
       || target.plan_sha256!=p.plan_sha256 || !target.derive_source_hash
       || !target.candidate.image_sha256.empty() || target.candidate.bytes!=preparation.image.bytes
       || (target.has_previous && (target.previous.image_sha256!=preparation.image.sha256
                                  || target.previous.bytes!=preparation.image.bytes))
       || (target.personal && !target.has_previous && target.candidate.shared_base_sha256!=preparation.image.sha256))return fail(EINVAL);
    auto checked=target;checked.job=1;return PackagePublicationCheck(checked);
}
int BrokerPrepareTransaction(aegis_broker_owner* owner,const PackagePreparation& plan,
                              const PackagePublication& target,int groups,int stage,int store,int source,
                              int prepare_helper,int execute_helper,int publish_helper,
                              const std::vector<int>& archives,uint64_t deadline,uint64_t* job) {
    if(transaction_target(plan,target)<0)return -1;
    return prepare_candidate(owner,plan,groups,stage,source,prepare_helper,execute_helper,
                             archives,deadline,job,false,&target,store,publish_helper);
}
int BrokerPreparePlannedTransaction(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
    uint64_t job,const std::string& digest,int groups,int stage,int store,int source,
    int prepare_helper,int execute_helper,int publish_helper,uint64_t deadline) {
    auto* slot=find_planning(owner,user,serial,job);if(!slot)return -1;
    if(slot->state!=PlanningState::Reviewed)return fail(EBUSY);
    if(slot->bound.preparation.execution.plan_sha256!=digest)return fail(ESTALE);
    if(slot->plan.personal && (stage!=-1 || store!=-1))return fail(EINVAL);
    PackageBoundPlan bound;if(BrokerReviewPlanning(owner,user,serial,job,deadline,&bound)<0)return -1;
    if(transaction_target(bound.preparation,bound.publication)<0)return -1;
    struct stat directory;struct statfs fs;
    if(fstat(slot->evidence.get(),&directory)<0||fstatfs(slot->evidence.get(),&fs)<0)return -1;
    const uid_t root=user*AEGIS_PER_USER_RANGE+aegis_uid_extents[0].app_id;
    if(!S_ISDIR(directory.st_mode)||directory.st_uid!=root||directory.st_gid!=root||fs.f_type!=TMPFS_MAGIC)return fail(EPERM);
    std::vector<unique_fd> pinned;std::vector<int> archives;
    for(size_t i=0;i<bound.preparation.archives.size();++i) {
        open_how how={};how.flags=O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW;
        how.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_XDEV;
        std::string name="archives/"+bound.preparation.execution.items[i];
        unique_fd fd(syscall(SYS_openat2,slot->evidence.get(),name.c_str(),&how,sizeof(how)));
        if(!fd.ok())return -1;struct stat st;if(fstat(fd.get(),&st)<0)return -1;
        if(!S_ISREG(st.st_mode)||st.st_nlink!=1||(st.st_mode&07022)||st.st_dev!=directory.st_dev
           ||((st.st_uid!=root||st.st_gid!=root)&&(st.st_uid||st.st_gid))
           ||st.st_size<=0||uint64_t(st.st_size)!=bound.preparation.archives[i].bytes)return fail(EPERM);
        if(fchown(fd.get(),0,0)<0||fchmod(fd.get(),0444)<0)return -1;
        archives.push_back(fd.get());pinned.push_back(std::move(fd));
    }
    uint64_t same=0;const bool personal=slot->plan.personal;
    return prepare_candidate(owner,bound.preparation,groups,stage,source,prepare_helper,execute_helper,
        archives,deadline,&same,personal,&bound.publication,store,publish_helper,slot);
}
int BrokerPreparePersonalTransaction(aegis_broker_owner* owner,const PackagePreparation& plan,
                                      const PackagePublication& target,int groups,int source,
                                      int prepare_helper,int execute_helper,int publish_helper,
                                      const std::vector<int>& archives,uint64_t deadline,uint64_t* job) {
    if(!target.personal)return fail(EINVAL);
    if(transaction_target(plan,target)<0)return -1;
    return prepare_candidate(owner,plan,groups,-1,source,prepare_helper,execute_helper,
                             archives,deadline,job,true,&target,-1,publish_helper);
}
int BrokerPrepareCandidate(aegis_broker_owner* owner,const PackagePreparation& request,
                           int groups,int stage,int source,int prepare_helper,int execute_helper,
                           const std::vector<int>& archives,uint64_t deadline,uint64_t* job) {
    return prepare_candidate(owner,request,groups,stage,source,prepare_helper,execute_helper,
                             archives,deadline,job,false);
}
int BrokerPreparePersonalCandidate(aegis_broker_owner* owner,const PackagePreparation& request,
                                   int groups,int source,int prepare_helper,int execute_helper,
                                   const std::vector<int>& archives,uint64_t deadline,uint64_t* job) {
    return prepare_candidate(owner,request,groups,-1,source,prepare_helper,execute_helper,
                             archives,deadline,job,true);
}
int BrokerPrepareExecution(aegis_broker_owner* owner,const PackageExecution& request,
                             int groups,int stage,int candidate,int helper,uint64_t deadline,uint64_t* job) {
    if(!job || *job || request.job)return fail(EINVAL);
    if(admission(owner,request.requester,request.serial,deadline)<0)return -1;
    if(capacity(owner,request.requester)<0)return -1;
    if(owner->next_publication==INT64_MAX)return fail(EOVERFLOW);
    PackageExecution plan=request;plan.job=owner->next_publication+1;
    if(PackageExecutionCheck(plan)<0)return -1;
    std::unique_ptr<execution_slot>* empty=nullptr;
    for(auto& slot:owner->executions) {
        if(!slot) { if(!empty)empty=&slot;continue; }
        // Uncollected results count too: one requester must not fill every
        // global slot with completed jobs and starve the other users.
        if(slot->plan.requester==request.requester)return fail(EBUSY);
    }
    if(!empty)return fail(ENOSPC);
    auto prepared=std::unique_ptr<execution_slot>(new(std::nothrow) execution_slot);
    if(!prepared)return fail(ENOMEM);
    prepared->plan=std::move(plan);
    int fds[]={groups,stage,candidate,helper};
    for(unsigned i=0;i<4;++i) {
        prepared->inputs[i].reset(fcntl(fds[i],F_DUPFD_CLOEXEC,3));
        if(!prepared->inputs[i].ok())return -1;
    }
    // No child or persistent changes yet. Failed preparation drops its copies.
    if(remaining_ms(deadline)<=0)return fail(ETIMEDOUT);
    *job=++owner->next_publication;*empty=std::move(prepared);
    return 0;
}
int BrokerStartExecution(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                           uint64_t job,const std::string& plan,uint64_t deadline) {
    auto* slot=find_execution(owner,user,serial,job,plan);if(!slot)return -1;
    if(slot->state!=PublicationState::Prepared)return fail(EALREADY);
    if(admission(owner,user,serial,deadline)<0)return -1;
    const auto wall=time(nullptr);
    if(slot->valid_until_unix && (wall<=0 || uint64_t(wall)>=slot->valid_until_unix)) {
        slot->close_inputs();slot->close_target();slot->validation_stage.reset();
        slot->result={PackageExecutionOutcome::Failed,0,ESTALE};slot->state=PublicationState::Complete;
        return fail(ESTALE);
    }
    // This consumes the preparation even on failure. Every partial child/FD
    // remains in the pre-registered slot before caller admission is released.
    slot->state=PublicationState::Running;
    int result=PackageExecutorStart(slot->inputs[0].get(),slot->inputs[1].get(),
        slot->inputs[2].get(),slot->inputs[3].get(),slot->plan,deadline,&slot->executor);
    int error=errno;
    if(result==0)slot->validation_stage=std::move(slot->inputs[1]);
    slot->close_inputs();
    if(result<0) {
        slot->close_target();
        if(slot->executor) { slot->state=PublicationState::Sealed;(void)PackageExecutorCancel(slot->executor); }
        else { slot->state=PublicationState::Complete;slot->result={PackageExecutionOutcome::Failed,0,error}; }
        return fail(error);
    }
    if(remaining_ms(deadline)<=0) {
        slot->state=PublicationState::Sealed;(void)PackageExecutorCancel(slot->executor);
        return fail(ETIMEDOUT);
    }
    return 0;
}
int BrokerPollExecution(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                          uint64_t job,const std::string& plan,PublicationState* state,
                          PackageExecutionResult* result) {
    if(!state || !result)return fail(EINVAL);
    auto* slot=find_execution(owner,user,serial,job,plan);if(!slot)return -1;
    if((slot->executor || slot->preparer || slot->publisher) && reap_execution(*slot,0)<0 && errno!=ETIMEDOUT)return -1;
    *state=slot->state;
    if(slot->state==PublicationState::AwaitingValidation)*result=slot->result;
    if(slot->state==PublicationState::Complete) {
        *result=slot->result;
        for(auto& entry:owner->executions)if(entry.get()==slot) { entry.reset();break; }
    }
    return 0;
}
int BrokerCancelExecution(aegis_broker_owner* owner,uint32_t user,uint32_t serial,
                            uint64_t job,const std::string& plan,uint64_t deadline) {
    auto* slot=find_execution(owner,user,serial,job,plan);if(!slot)return -1;
    int left=remaining_ms(deadline),error=left<0?errno:0;
    // Invalid/expired time still seals and requests this exact owned job's stop.
    if(slot->state==PublicationState::Complete) {
        if(slot->result.outcome==PackageExecutionOutcome::Published)return fail(EALREADY);
        // A completed failure is still an uncollected job. Cancellation is
        // terminal regardless of whether that earlier failure was polled.
        slot->result.outcome=PackageExecutionOutcome::Failed;slot->result.error=ECANCELED;
    } else if(slot->publisher) {
        slot->state=PublicationState::Sealed;(void)PackagePublisherCancel(slot->publisher);
        if(reap_execution(*slot,left<0?0:left)<0)return -1;
    } else if(slot->preparer) {
        slot->state=PublicationState::Sealed;(void)PackagePreparerCancel(slot->preparer);
        if(reap_execution(*slot,left<0?0:left)<0)return -1;
    } else if(slot->state==PublicationState::Prepared || slot->state==PublicationState::AwaitingValidation) {
        slot->close_inputs();slot->close_target();slot->validation_stage.reset();slot->state=PublicationState::Complete;
        slot->result={PackageExecutionOutcome::Failed,0,ECANCELED};
    } else if(slot->executor) {
        slot->state=PublicationState::Sealed;(void)PackageExecutorCancel(slot->executor);
        if(reap_execution(*slot,left<0?0:left)<0)return -1;
    }
    return error ? fail(error) : 0;
}
} // namespace aegis

int aegis_broker_owner_start(aegis_broker_owner* owner,const aegis_broker_call* call,
                             uint64_t* job,aegis_broker_state* state) {
    if(!job || !state)return fail(EINVAL);
    *job=0;*state=AEGIS_BROKER_SEALED;
    if(owned(owner)<0)return -1;
    if(!call || call->argc || call->payload_bytes)return fail(EINVAL);
    const auto& request=call->request;
    bool continuation=request.operation==AEGIS_BROKER_CONTINUE_START;
    if((!continuation && request.operation!=AEGIS_BROKER_START)
            || (continuation ? !call->command || call->command>INT64_MAX : call->command!=0))
        return fail(EINVAL);
    // Validate framing/deadline before looking up any resource. The normalized
    // START is internal only; absence on continuation is rejected before apply.
    auto start=request;start.operation=AEGIS_BROKER_START;
    uint64_t now;if(now_ns(&now)<0)return -1;
    aegis_broker_request checked;
    if(aegis_broker_parse(&start,sizeof(start),start.sequence>1?start.sequence-1:0,now,&checked)<0)return -1;
    runtime_selection_slot* selected=nullptr;
    for(auto& slot:owner->selections)if(slot && slot->purpose==SelectionPurpose::Runtime && slot->plan.requester==request.user) {
        selected=slot.get();break;
    }
    if(selected && selected->plan.serial!=request.serial)return fail(ESTALE);
    if(continuation && (!selected || selected->plan.job!=call->command))return fail(ESTALE);
    if(!selected && owner->selection_enabled) {
        // Existing contexts must already own their registered generation. No
        // post-bootstrap fallback to the factory-mount compatibility path.
        for(const auto& slot:owner->slots)if(slot.context && slot.user==request.user)return fail(ESTALE);
        PackageRuntimeSelection plan;plan.requester=request.user;plan.serial=request.serial;
        plan.factory=owner->selection_factory;uint64_t registered=0;
        if(prepare_runtime_selection(owner,plan,owner->inputs[0],-1,-1,
                owner->selection_image.get(),owner->selection_helper.get(),request.deadline_ns,
                &registered,true,owner->selection_directory.get())<0)return -1;
        for(auto& slot:owner->selections)if(slot && slot->plan.job==registered) { selected=slot.get();break; }
        if(!selected)return fail(EIO);
    }
    uint64_t selected_job=selected ? selected->plan.job : 0;
    int result=aegis_broker_owner_apply(owner,&start,state),error=errno;
    if(result==0 || (error==EAGAIN && selected_job)) *job=selected_job;
    // Only a still-owned selector may ask the service to wait. An unrelated
    // EAGAIN (e.g. setup I/O failure) must never turn into a new implicit start.
    if(result<0 && error==EAGAIN && (!selected || !selected->worker
            || selected->state!=RuntimeSelectionState::Selecting)) {
        *job=0;return fail(EIO);
    }
    return result<0 ? fail(error) : 0;
}
