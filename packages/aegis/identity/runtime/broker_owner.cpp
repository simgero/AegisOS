#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "broker_owner.h"
#include "context.h"
#include "control.h"
#include "broker_owner_package.h"
#include <android-base/unique_fd.h>
#include <array>
#include <memory>
#include <new>
#include <errno.h>
#include <fcntl.h>
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
    std::array<std::unique_ptr<publication_slot>,MAX_PUBLICATIONS> publications;
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
    for(const auto& slot:owner->publications)if(slot && slot->resources() && slot->plan.requester==user) {
        if(slot->plan.serial!=serial)return fail(ESTALE);
        if(slot->state==PublicationState::Sealed)return fail(EBUSY);
    }
    return 0;
}
static bool publication_resources(struct aegis_broker_owner* owner,uint32_t user) {
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
static void seal_publications(struct aegis_broker_owner* owner,uint32_t user) {
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
    // Signal EVERY matching publisher before the first potentially blocking
    // context/child wait. A timeout never leaves later jobs unvisited.
    seal_publications(owner,user);
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
    for(auto& slot:owner->publications) {
        if(!slot || (user && slot->plan.requester!=user))continue;
        int left=remaining_ms(deadline);
        if(left<0) { if(!error)error=errno;left=0; }
        if(reap_publication(*slot,left)<0) { if(!error)error=errno; }
        else slot.reset(); // No resources remain. IDs are never recycled.
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
    empty->user = request->user; empty->serial = request->serial;
    if (aegis_context_start_until(request->user, request->serial, owner->inputs[0], owner->inputs[1],
            owner->inputs[2], owner->inputs[3], 1, request->deadline_ns, &empty->context) < 0) {
        int error = errno;
        // The context API deliberately returns partial ownership on failure.
        if (!empty->context) memset(empty, 0, sizeof(*empty));
        return fail(error);
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
    for(const auto& slot:owner->publications)if(slot && slot->resources())return fail(EBUSY);
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
    for(auto& slot:owner->publications)if(slot && slot->publisher) {
        if(reap_publication(*slot,0)<0 && errno!=ETIMEDOUT) {
            if(!error)error=errno;
            slot->state=PublicationState::Sealed;
            (void)PackagePublisherCancel(slot->publisher);
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
}
int BrokerPreparePublication(aegis_broker_owner* owner,const PackagePublication& request,
                             int groups,int store,int source,int helper,uint64_t deadline,uint64_t* job) {
    if(!job || *job || request.job)return fail(EINVAL);
    if(admission(owner,request.requester,request.serial,deadline)<0)return -1;
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
        prepared->inputs[i].reset(fcntl(fds[i],F_DUPFD_CLOEXEC,3));
        if(!prepared->inputs[i].ok())return -1;
    }
    // No child or persistent changes yet. Failed preparation drops its copies.
    if(remaining_ms(deadline)<=0)return fail(ETIMEDOUT);
    *job=++owner->next_publication;*empty=std::move(prepared);
    return 0;
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
} // namespace aegis
