#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "broker_cgroup.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <time.h>
#include <unistd.h>

static int fail(int error) { errno = error; return -1; }
static int read_at(int directory, const char *name, char *text, size_t size) {
    int fd = openat(directory, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return -1;
    size_t used = 0;
    int result = -1;
    while (used < size - 1) {
        ssize_t count = read(fd, text + used, size - 1 - used);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) goto done;
        if (!count) { text[used] = 0; result = 0; goto done; }
        used += (size_t)count;
    }
    errno = EOVERFLOW;
done:;
    int saved = errno;
    close(fd); errno = saved; return result;
}
static int equal_at(int directory, const char *name, const char *expected) {
    char text[512];
    if (read_at(directory, name, text, sizeof(text)) < 0) return -1;
    return strcmp(text, expected) ? fail(EPROTO) : 0;
}
static int write_at(int directory, const char *name, const char *value) {
    int fd = openat(directory, name, O_WRONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return -1;
    size_t length = strlen(value);
    ssize_t count;
    do { count = write(fd, value, length); } while (count < 0 && errno == EINTR);
    int saved = count < 0 ? errno : EIO;
    close(fd);
    return count == (ssize_t)length ? 0 : fail(saved);
}
static int directory(int fd, int private, dev_t device) {
    struct stat st;
    struct statfs fs;
    if (fstat(fd, &st) < 0 || fstatfs(fd, &fs) < 0) return -1;
    if (!S_ISDIR(st.st_mode) || st.st_uid || st.st_gid || fs.f_type != CGROUP2_SUPER_MAGIC
            || (device && st.st_dev != device) || (st.st_mode & 0022)
            || (private && (st.st_mode & 07777) != 0700)) return fail(EPERM);
    return 0;
}
static int same_name(int parent, const char *name, int held) {
    struct stat a, b;
    if (fstat(held, &a) < 0 || fstatat(parent, name, &b, AT_SYMLINK_NOFOLLOW) < 0) return -1;
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino ? 0 : fail(ESTALE);
}
static int decimal(const char **cursor, uint32_t maximum, uint32_t *output) {
    const char *start = *cursor, *p = start;
    uint32_t value = 0;
    if (*p < '0' || *p > '9') return 0;
    do {
        uint32_t digit = (uint32_t)(*p - '0');
        if (value > (maximum - digit) / 10) return 0;
        value = value * 10 + digit;
        ++p;
    } while (*p >= '0' && *p <= '9');
    if (p - start > 1 && *start == '0') return 0;
    *cursor = p; *output = value;
    return 1;
}
static int canonical(const char *name) {
    uint32_t user, serial;
    const char *p = name;
    if (*p++ != 'u' || !decimal(&p, 21472, &user) || user < 10
            || *p++ != '-' || *p++ != 's' || !decimal(&p, INT32_MAX, &serial)) return 0;
    return !*p;
}
static int64_t now_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
static int empty(int parent) {
    char text[512], *save = NULL;
    if (read_at(parent, "cgroup.events", text, sizeof(text)) < 0) return -1;
    int found = -1;
    for (char *line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "populated", 9)) continue;
        if (found >= 0) return fail(EPROTO);
        if (!strcmp(line, "populated 0")) found = 1;
        else if (!strcmp(line, "populated 1")) found = 0;
        else return fail(EPROTO);
    }
    return found >= 0 ? found : fail(EPROTO);
}
struct child { int fd; char name[48]; };
static const struct { const char *file, *value; } limits[] = {
    {"memory.max", "2147483648\n"}, {"memory.high", "1610612736\n"},
    {"memory.swap.max", "0\n"}, {"memory.oom.group", "0\n"},
    {"cgroup.max.depth", "1\n"}, {"cgroup.max.descendants", "16\n"},
};

int aegis_broker_cgroup_prepare(int root, int timeout_ms) {
    if (timeout_ms < 0 || timeout_ms > 10000) return fail(EINVAL);
    if (getuid() || geteuid() || getgid() || getegid()) return fail(EPERM);
    if (directory(root, 0, 0) < 0) return -1;
    int created = mkdirat(root, AEGIS_BROKER_CGROUP, 0700) == 0;
    if (!created && errno != EEXIST) return -1;
    int parent = openat(root, AEGIS_BROKER_CGROUP, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (parent < 0) return -1;
    struct child children[16];
    for (unsigned i = 0; i < 16; ++i) children[i].fd = -1;
    unsigned count = 0;
    DIR *entries = NULL;
    int result = -1;
    if (created && fchmod(parent, 0700) < 0) goto done;
    struct stat st;
    if (fstat(root, &st) < 0 || directory(parent, 1, st.st_dev) < 0
            || same_name(root, AEGIS_BROKER_CGROUP, parent) < 0
            || equal_at(parent, "cgroup.type", "domain\n") < 0
            || equal_at(parent, "cgroup.procs", "") < 0) goto done;
    // Inventory before killing: foreign names, nested groups or metadata are
    // not an invitation to claim unrelated processes or recursively delete.
    int scan = openat(parent, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (scan < 0) goto done;
    entries = fdopendir(scan);
    if (!entries) { close(scan); goto done; }
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(entries);
        if (!entry) { if (errno) goto done; break; }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        struct stat item;
        if (fstatat(parent, entry->d_name, &item, AT_SYMLINK_NOFOLLOW) < 0) goto done;
        if (S_ISREG(item.st_mode)) continue; // Kernel-owned control files.
        if (!S_ISDIR(item.st_mode) || !canonical(entry->d_name) || count == 16) {
            errno = EPERM; goto done;
        }
        struct child *child = &children[count++];
        strcpy(child->name, entry->d_name); // canonical() bounds both numbers.
        child->fd = openat(parent, child->name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (child->fd < 0 || directory(child->fd, 1, st.st_dev) < 0
                || same_name(parent, child->name, child->fd) < 0
                || equal_at(child->fd, "cgroup.type", "domain\n") < 0
                || equal_at(child->fd, "cgroup.subtree_control", "") < 0) goto done;
        // No descendant contexts are produced by this broker. Inspect them
        // before requesting any process termination or changing limits.
        char statistics[512];
        if (read_at(child->fd, "cgroup.stat", statistics, sizeof(statistics)) < 0) goto done;
        if (strncmp(statistics, "nr_descendants 0\n", 17)) { errno = EPERM; goto done; }
    }
    closedir(entries); entries = NULL;
    int64_t now = now_ms();
    if (now < 0) goto done;
    int64_t deadline = now + timeout_ms;
    if (write_at(parent, "cgroup.kill", "1\n") < 0) goto done;
    for (;;) {
        int state = empty(parent);
        if (state < 0) goto done;
        if (state) break;
        now = now_ms();
        if (now < 0) goto done;
        if (now >= deadline) { errno = ETIMEDOUT; goto done; }
        struct timespec pause = {.tv_nsec = 1000000};
        if (nanosleep(&pause, NULL) < 0 && errno != EINTR) goto done;
    }
    for (unsigned i = 0; i < count; ++i) {
        if (same_name(parent, children[i].name, children[i].fd) < 0
                || unlinkat(parent, children[i].name, AT_REMOVEDIR) < 0) goto done;
    }
    if (same_name(root, AEGIS_BROKER_CGROUP, parent) < 0) goto done;
    for (unsigned i = 0; i < sizeof(limits) / sizeof(limits[0]); ++i) {
        if (write_at(parent, limits[i].file, limits[i].value) < 0
                || equal_at(parent, limits[i].file, limits[i].value) < 0) goto done;
    }
    if (write_at(parent, "cgroup.subtree_control", "+memory\n") < 0
            || equal_at(parent, "cgroup.subtree_control", "memory\n") < 0
            || equal_at(parent, "cgroup.procs", "") < 0) goto done;
    int state = empty(parent);
    if (state < 0) goto done;
    if (!state) { errno = EBUSY; goto done; }
    result = parent;
done:;
    int saved = errno;
    if (entries) closedir(entries);
    for (unsigned i = 0; i < count; ++i) if (children[i].fd >= 0) close(children[i].fd);
    if (result < 0) close(parent);
    errno = saved;
    return result;
}
