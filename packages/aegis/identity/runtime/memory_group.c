#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "memory_group.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/magic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct aegis_memory_group {
    int parent, directory, procs, ready;
    pid_t owner;
    uint32_t user, serial;
    char name[48];
};

static int reject(int error) { errno = error; return -1; }

static int private_directory(int fd) {
    struct stat st;
    struct statfs fs;
    if (fstat(fd, &st) < 0 || fstatfs(fd, &fs) < 0) return -1;
    if (fs.f_type != CGROUP2_SUPER_MAGIC || st.st_mode != (S_IFDIR | 0700)
            || st.st_uid || st.st_gid) return reject(EPERM);
    return 0;
}

static int owned(struct aegis_memory_group *group) {
    if (!group) return reject(EINVAL);
    /* Bionic getpid() caches the parent's value across a raw clone3. */
    if (group->owner != (pid_t)syscall(SYS_getpid)
            || getuid() || geteuid() || getgid() || getegid())
        return reject(EPERM);
    if (private_directory(group->parent) < 0) return -1;
    struct stat held, named;
    struct statfs fs;
    if (fstat(group->directory, &held) < 0
            || fstatfs(group->directory, &fs) < 0
            || fstatat(group->parent, group->name, &named, AT_SYMLINK_NOFOLLOW) < 0) return -1;
    if (fs.f_type != CGROUP2_SUPER_MAGIC || !S_ISDIR(held.st_mode)
            || held.st_uid || held.st_gid) return reject(EPERM);
    if (held.st_dev != named.st_dev || held.st_ino != named.st_ino) return reject(ESTALE);
    // Retain the target's VFS-resolved cgroup.procs inode across clone3's
    // dentry-less permission check. No task is migrated through this file.
    if (group->procs >= 0) {
        if (fstat(group->procs, &held) < 0
                || fstatat(group->directory, "cgroup.procs", &named, AT_SYMLINK_NOFOLLOW) < 0)
            return -1;
        if (!S_ISREG(held.st_mode) || held.st_uid || held.st_gid || (held.st_mode & 0022))
            return reject(EPERM);
        if (held.st_dev != named.st_dev || held.st_ino != named.st_ino) return reject(ESTALE);
    }
    return 0;
}

static int read_value(int directory, const char *name, char *text, size_t capacity) {
    int fd = openat(directory, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    size_t used = 0;
    int result = -1;
    while (used < capacity - 1) {
        ssize_t n = read(fd, text + used, capacity - 1 - used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) goto done;
        if (!n) { text[used] = 0; result = 0; goto done; }
        used += (size_t)n;
    }
    errno = EOVERFLOW;
done:;
    int saved = errno;
    close(fd);
    errno = saved;
    return result;
}

static int write_value(int directory, const char *name, const char *value) {
    int fd = openat(directory, name, O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    size_t length = strlen(value);
    ssize_t written;
    do { written = write(fd, value, length); } while (written < 0 && errno == EINTR);
    int saved = written < 0 ? errno : EIO;
    close(fd);
    return written == (ssize_t)length ? 0 : reject(saved);
}

static int equals(int directory, const char *name, const char *expected) {
    char text[256];
    if (read_value(directory, name, text, sizeof(text)) < 0) return -1;
    return !strcmp(text, expected) ? 0 : reject(EPROTO);
}

static const struct { const char *name, *value; } limits[] = {
    {"memory.max", "1073741824\n"}, {"memory.high", "805306368\n"},
    {"memory.swap.max", "0\n"}, {"memory.oom.group", "1\n"},
    {"cgroup.max.depth", "0\n"}, {"cgroup.max.descendants", "0\n"},
};

static int limits_match(int directory) {
    if (equals(directory, "cgroup.type", "domain\n") < 0) return -1;
    for (unsigned i = 0; i < sizeof(limits) / sizeof(limits[0]); i++) {
        if (equals(directory, limits[i].name, limits[i].value) < 0) return -1;
    }
    return 0;
}

static int populated(int directory) {
    char text[512], *save = NULL;
    if (read_value(directory, "cgroup.events", text, sizeof(text)) < 0) return -1;
    int found = -1;
    for (char *line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "populated", 9)) continue;
        if (found >= 0) return reject(EPROTO);
        if (!strcmp(line, "populated 0")) found = 0;
        else if (!strcmp(line, "populated 1")) found = 1;
        else return reject(EPROTO);
    }
    return found >= 0 ? found : reject(EPROTO);
}

int aegis_memory_group_create(int parent_fd, uint32_t user, uint32_t serial,
                              struct aegis_memory_group **output) {
    if (!output || *output || user < 10 || user >= 21473 || serial > INT32_MAX)
        return reject(EINVAL);
    if (getuid() || geteuid() || getgid() || getegid()) return reject(EPERM);
    if (private_directory(parent_fd) < 0 || equals(parent_fd, "cgroup.type", "domain\n") < 0
            || equals(parent_fd, "cgroup.procs", "") < 0) return -1;
    struct aegis_memory_group *group = calloc(1, sizeof(*group));
    if (!group) return -1;
    group->owner = (pid_t)syscall(SYS_getpid);
    group->user = user;
    group->serial = serial;
    group->directory = group->procs = -1;
    group->parent = fcntl(parent_fd, F_DUPFD_CLOEXEC, 4);
    snprintf(group->name, sizeof(group->name), "u%u-s%u", user, serial);
    if (group->parent < 0 || mkdirat(group->parent, group->name, 0700) < 0) {
        int saved = errno;
        if (group->parent >= 0) close(group->parent);
        free(group);
        return reject(saved);
    }
    /* The caller owns the new group even if configuration fails. No child is
     * admitted until claim() validates every limit. Never silently adopt a
     * pre-existing name or lose a partially configured resource on failure. */
    *output = group;
    group->directory = openat(group->parent, group->name,
                              O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (group->directory < 0) {
        int saved = errno;
        /* No fd/clone target was published, so no caller-owned process could
         * enter this fresh group. Parent ownership/serialization is required.
         * Retain an unrecoverable handle if even removing it fails: callers
         * must report incomplete cleanup instead of admitting another start. */
        if (unlinkat(group->parent, group->name, AT_REMOVEDIR) == 0) {
            close(group->parent);
            free(group);
            *output = NULL;
        }
        return reject(saved);
    }
    if (fchmod(group->directory, 0700) < 0 || owned(group) < 0) return -1;
    group->procs = openat(group->directory, "cgroup.procs", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (group->procs < 0 || owned(group) < 0) return -1;
    int state = populated(group->directory);
    if (state < 0) return -1;
    if (state) return reject(EBUSY);
    for (unsigned i = 0; i < sizeof(limits) / sizeof(limits[0]); i++) {
        if (write_value(group->directory, limits[i].name, limits[i].value) < 0) return -1;
    }
    if (private_directory(group->directory) < 0 || limits_match(group->directory) < 0) return -1;
    group->ready = 1;
    return 0;
}

int aegis_memory_group_claim(struct aegis_memory_group *group, uint32_t user, uint32_t serial) {
    if (owned(group) < 0) return -1;
    if (group->user != user || group->serial != serial) return reject(ESTALE);
    if (!group->ready) return reject(EAGAIN);
    if (private_directory(group->directory) < 0 || limits_match(group->directory) < 0) return -1;
    int state = populated(group->directory);
    if (state < 0) return -1;
    if (state) return reject(EBUSY);
    group->ready = 0;
    return group->directory;
}

static int64_t now_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

int aegis_memory_group_kill_and_wait(struct aegis_memory_group *group, int timeout_ms) {
    if (timeout_ms < 0 || timeout_ms > 10000) return reject(EINVAL);
    if (owned(group) < 0) return -1;
    int64_t start = now_ms();
    if (start < 0) return -1;
    group->ready = 0; /* Stop seals this group; it can never be reused for start. */
    if (write_value(group->directory, "cgroup.kill", "1\n") < 0) return -1;
    for (;;) {
        int state = populated(group->directory);
        if (state <= 0) return state;
        int64_t now = now_ms();
        if (now < 0) return -1;
        if (now - start >= timeout_ms) return reject(ETIMEDOUT);
        struct timespec delay = {.tv_nsec = 1000000};
        /* Check the absolute deadline on EINTR as well. */
        if (nanosleep(&delay, NULL) < 0 && errno != EINTR) return -1;
    }
}

int aegis_memory_group_remove(struct aegis_memory_group **pointer) {
    if (!pointer || !*pointer) return reject(EINVAL);
    struct aegis_memory_group *group = *pointer;
    if (owned(group) < 0) return -1;
    int state = populated(group->directory);
    if (state < 0) return -1;
    if (state) return reject(EBUSY);
    if (unlinkat(group->parent, group->name, AT_REMOVEDIR) < 0) return -1;
    close(group->directory);
    if (group->procs >= 0) close(group->procs);
    close(group->parent);
    free(group);
    *pointer = NULL;
    return 0;
}
