#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "broker_owner.h"
#include "context.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define MAX_CONTEXTS 16
struct slot {
    uint32_t user, serial;
    struct aegis_context *context;
};
struct aegis_broker_owner {
    pid_t process;
    int inputs[4];
    struct slot slots[MAX_CONTEXTS];
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
    struct aegis_broker_owner *owner = calloc(1, sizeof(*owner));
    if (!owner) return -1;
    for (unsigned i = 0; i < 4; i++) owner->inputs[i] = -1;
    int sources[] = {parent_fd, base_fd, setup_fd, init_fd};
    for (unsigned i = 0; i < 4; i++) {
        owner->inputs[i] = fcntl(sources[i], F_DUPFD_CLOEXEC, 3);
        if (owner->inputs[i] < 0) {
            int error = errno;
            for (unsigned j = 0; j < 4; j++) if (owner->inputs[j] >= 0) close(owner->inputs[j]);
            free(owner);
            return fail(error);
        }
    }
    owner->process = (pid_t)syscall(SYS_getpid);
    *output = owner;
    return 0;
}

static int stop(struct aegis_broker_owner *owner, uint32_t user, uint64_t deadline) {
    int error = 0;
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
        *state = AEGIS_BROKER_ABSENT;
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
    for (unsigned i = 0; i < 4; i++) close(owner->inputs[i]);
    free(owner); *output = NULL;
    return 0;
}
