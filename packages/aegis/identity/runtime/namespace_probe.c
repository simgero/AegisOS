/* Trusted static fixture. Runs ONLY as a local-QEMU device test, never as a
 * production setup helper. It deliberately does NOT launch any user program. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "namespace_probe.h"

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

extern char **environ;

static int read_map(const char *name, uint32_t rows[3][3]) {
    FILE *file = fopen(name, "re");
    if (!file) return -1;
    int result = 0;
    for (int i = 0; i < 3; i++) {
        if (fscanf(file, "%u %u %u", &rows[i][0], &rows[i][1], &rows[i][2]) != 3)
            result = -1;
    }
    char extra;
    if (fscanf(file, " %c", &extra) != EOF || ferror(file)) result = -1;
    fclose(file);
    return result;
}

int main(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[0], "aegis-runtime-setup") || getpid() != 1) return 91;
    struct aegis_namespace_probe report = {.magic = 0x41454e53};
    char *end;
    unsigned long user = strtoul(argv[1], &end, 10);
    if (*end || user < 10 || user >= 21473) return 92;
    report.user_id = (uint32_t)user;
    unsigned long serial = strtoul(argv[2], &end, 10);
    if (*end || serial > INT32_MAX) return 92;
    report.serial = (uint32_t)serial;
    report.pid = getpid(); report.ppid = getppid();
    report.session_id = getsid(0); report.process_group = getpgrp();
    FILE *oom = fopen("/proc/self/oom_score_adj", "re");
    if (!oom) return 93;
    int parsed = fscanf(oom, "%d", &report.oom_score_adj);
    fclose(oom);
    if (parsed != 1) return 93;
    uid_t r, e, s;
    gid_t gr, ge, gs;
    if (getresuid(&r, &e, &s) < 0 || getresgid(&gr, &ge, &gs) < 0
            || r != e || e != s || gr != ge || ge != gs) return 93;
    report.uid = r; report.gid = gr; report.groups = getgroups(0, NULL);
    int death_signal;
    if (prctl(PR_GET_PDEATHSIG, &death_signal, 0, 0, 0) < 0) return 94;
    report.death_signal = death_signal;
    for (int fd = 0; fd < 256; fd++) {
        if (fd != 3 && fcntl(fd, F_GETFD) >= 0) report.extra_fds++;
    }
    report.setgroups_denied = setgroups(0, NULL) == -1 && errno == EPERM;
    report.fixed_environment = environ[0] && environ[1] && !environ[2]
        && !strcmp(environ[0], "PATH=/system/bin") && !strcmp(environ[1], "LANG=C");
    if (read_map("/proc/self/uid_map", report.uid_rows) < 0
            || read_map("/proc/self/gid_map", report.gid_rows) < 0) return 95;
    const char *names[] = {"user", "pid", "mnt", "ipc", "uts", "net"};
    for (unsigned i = 0; i < 6; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/proc/self/ns/%s", names[i]);
        struct stat st;
        if (stat(path, &st) < 0) return 96;
        report.namespace_inodes[i] = st.st_ino;
    }
    FILE *mounts = fopen("/proc/self/mountinfo", "re");
    if (!mounts) return 97;
    char *line = NULL;
    size_t capacity = 0;
    report.private_mounts = 1;
    while (getline(&line, &capacity, mounts) >= 0) {
        char *separator = strstr(line, " - ");
        if (!separator) { report.private_mounts = 0; break; }
        *separator = '\0';
        if (strstr(line, " shared:") || strstr(line, " master:")) report.private_mounts = 0;
    }
    if (ferror(mounts)) report.private_mounts = 0;
    free(line); fclose(mounts);
    if (send(3, &report, sizeof(report), MSG_NOSIGNAL) != (ssize_t)sizeof(report)) return 98;
    /* Bound fixture lifetime even if a broken test leaves its channel open. */
    struct timeval timeout = {.tv_sec = 5};
    if (setsockopt(3, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) return 99;
    char command;
    return recv(3, &command, 1, 0) == 1 && command == 'Q' ? 0 : 100;
}
