#ifndef AEGIS_RUNTIME_NAMESPACE_PROBE_H
#define AEGIS_RUNTIME_NAMESPACE_PROBE_H

#include <stdint.h>

/* Device-test fixture only. No broker protocol or production readiness claim. */
struct aegis_namespace_probe {
    uint32_t magic, user_id, serial, pid, ppid, uid, gid, groups, death_signal;
    uint32_t extra_fds, setgroups_denied, private_mounts, fixed_environment;
    uint32_t session_id, process_group;
    int32_t oom_score_adj;
    uint32_t uid_rows[3][3], gid_rows[3][3];
    uint64_t namespace_inodes[6]; /* user, pid, mnt, ipc, uts, net */
};

#endif
