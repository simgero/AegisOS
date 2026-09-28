#ifndef AEGIS_RUNTIME_CHILD_PRIVATE_H
#define AEGIS_RUNTIME_CHILD_PRIVATE_H

#include "child.h"
#include <sys/types.h>

/* Library implementation only. The launcher allocates this BEFORE clone3 so
 * acquiring ownership of a newly created child cannot fail due to allocation
 * or fd duplication. Do not expose this representation to broker clients. */
struct aegis_child {
    int pidfd;
    pid_t owner;
    int observed;
    int observation_error;
    struct aegis_child_exit result;
};

#endif
