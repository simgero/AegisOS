#include "package_program.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "package_execution_protocol.h"
#include "package_validate.h"
#include "package_candidate_labels.h"
#include "package_guard.h"
#include "sandbox.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/magic.h>
#include <linux/mount.h>
#include <linux/openat2.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <unistd.h>
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define AEGIS_MQUEUE_MAGIC UINT64_C(0x19800202)
static int denied(void) { errno = EPERM; return -1; }
static void expired(int sig) { (void)sig; _exit(125); }
static int directory(int fd, uint64_t type, unsigned long required, unsigned long forbidden) {
    struct stat st;struct statfs fs;struct statvfs flags;
    if (fstat(fd, &st) < 0 || fstatfs(fd, &fs) < 0 || fstatvfs(fd, &flags) < 0) return -1;
    int mode = fcntl(fd, F_GETFL);if (mode < 0) return -1;
    if (!(mode & O_PATH) || st.st_mode != (S_IFDIR | 0755) || st.st_uid || st.st_gid
            || fs.f_type != type || (flags.f_flag & required) != required || (flags.f_flag & forbidden))
        return denied();
    return 0;
}
static int beneath(int parent, const char *name, int flags) {
    struct open_how how = {.flags = (uint64_t)(flags | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW),
        .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
    return (int)syscall(SYS_openat2, parent, name, &how, sizeof(how));
}

static int attach(int tree, int parent, const char *name) {
    int target = beneath(parent, name, O_PATH);
    if (target < 0) return -1;
    int result = (int)syscall(SYS_move_mount, tree, "", target, "",
                             MOVE_MOUNT_F_EMPTY_PATH | MOVE_MOUNT_T_EMPTY_PATH);
    int saved = errno;
    close(target);
    errno = saved;
    return result;
}

struct parameter { const char *name, *value; };
static int fresh(const char *type, const struct parameter *parameters,
                  unsigned count, unsigned flags) {
    int fs = (int)syscall(SYS_fsopen, type, FSOPEN_CLOEXEC);
    if (fs < 0) return -1;
    int result = -1;
    for (unsigned i = 0; i < count; i++) {
        if (syscall(SYS_fsconfig, fs, parameters[i].value ? FSCONFIG_SET_STRING : FSCONFIG_SET_FLAG,
                    parameters[i].name, parameters[i].value, 0) < 0) goto done;
    }
    if (syscall(SYS_fsconfig, fs, FSCONFIG_CMD_CREATE, NULL, NULL, 0) < 0) goto done;
    result = (int)syscall(SYS_fsmount, fs, FSMOUNT_CLOEXEC, flags);
done:;
    int saved = errno;
    close(fs);
    errno = saved;
    return result;
}

static int fresh_at(int parent, const char *name, const char *type,
                     const struct parameter *parameters, unsigned count, unsigned flags) {
    int tree = fresh(type, parameters, count, flags);
    if (tree < 0) return -1;
    int result = attach(tree, parent, name);
    int saved = errno;
    close(tree);
    errno = saved;
    return result;
}

static int tmpfs_at(int parent, const char *name, const char *size,
                     const char *inodes, const char *mode, unsigned extra) {
    const struct parameter parameters[] = {
        {"size", size}, {"nr_inodes", inodes}, {"mode", mode}, {"uid", "0"}, {"gid", "0"},
    };
    return fresh_at(parent, name, "tmpfs", parameters, COUNT(parameters),
                     MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV | extra);
}

static int empty_directory(int parent, const char *name) {
    int fd = beneath(parent, name, O_RDONLY);
    if (fd < 0) return -1;
    DIR *directory = fdopendir(fd);
    if (!directory) { int saved = errno; close(fd); errno = saved; return -1; }
    int result = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) { result = denied(); break; }
    }
    if (errno) result = -1;
    int saved = errno;
    closedir(directory);
    errno = saved;
    return result;
}


static int write_all(int fd,const void *bytes,size_t size) {
    const char *p=bytes;
    while(size) { ssize_t n=write(fd,p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;p+=n;size-=n; }
    return 0;
}
static int trusted_tools(int root) {
    // Copy only our already pinned static executable, before package privileges
    // are dropped. Seal its separate mount readonly before any Debian process.
    int executable=open("/proc/self/exe",O_RDONLY|O_CLOEXEC);
    struct stat st;if(executable<0)return -1;
    if(fstat(executable,&st)<0 || !S_ISREG(st.st_mode) || st.st_size<=0 || st.st_size>(16<<20)) {
        close(executable);return denied();
    }
    const struct parameter parameters[]={{"size","33554432"},{"nr_inodes","32"},{"mode","0755"}};
    int tree=fresh("tmpfs",parameters,COUNT(parameters),MOUNT_ATTR_NOSUID|MOUNT_ATTR_NODEV);
    if(tree<0) { close(executable);return -1; }
    int result=-1,hook=openat(tree,"hook",O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0555),config=-1;
    if(hook<0)goto done;
    char buffer[65536];off_t offset=0;
    while(offset<st.st_size) {
        ssize_t n=pread(executable,buffer,sizeof(buffer),offset);if(n<0&&errno==EINTR)continue;
        if(n<=0||write_all(hook,buffer,n)<0)goto done;
        offset+=n;
    }
    if(fchmod(hook,0555)<0)goto done;
    close(hook);hook=-1;
    config=openat(tree,"config",O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0444);if(config<0)goto done;
    static const char policy[]=
        "Dir::Etc::parts \"/run/aegis-empty.d\";\n"
        "Dir::Etc::main \"/run/aegis-empty.list\";\n"
        "Dir::Etc::sourcelist \"/run/aegis-empty.list\";\n"
        "Dir::Etc::sourceparts \"/run/aegis-empty.d\";\n"
        "Dir::State::lists \"/run/aegis-empty.d\";\n"
        "Dir::Cache::pkgcache \"\";\nDir::Cache::srcpkgcache \"\";\n"
        "APT::Architecture \"arm64\";\nAPT::Architectures { \"arm64\"; };\n"
        "APT::Install-Recommends \"false\";\nAPT::Install-Suggests \"false\";\n"
        "APT::Get::AllowUnauthenticated \"false\";\n"
        "Dpkg::Use-Pty \"false\";\nDpkg::Options { \"--force-confold\"; };\n";
    if(write_all(config,policy,sizeof(policy)-1)<0)goto done;
    close(config);config=-1;
    struct mount_attr attributes={.attr_set=MOUNT_ATTR_RDONLY|MOUNT_ATTR_NOSUID|MOUNT_ATTR_NODEV};
    if(syscall(SYS_mount_setattr,tree,"",AT_EMPTY_PATH,&attributes,sizeof(attributes))<0
       || mkdirat(root,"tmp/aegis-trusted",0755)<0 || attach(tree,root,"tmp/aegis-trusted")<0)goto done;
    result=0;
done:;
    int saved=errno;if(hook>=0)close(hook);if(config>=0)close(config);close(tree);close(executable);errno=saved;return result;
}
static int construct(int root, int devices) {
    // The initial Android root is trusted. Anchor it explicitly: the strict
    // BENEATH helper intentionally rejects absolute paths, even /mnt.
    int host = open("/", O_PATH | O_DIRECTORY | O_CLOEXEC);
    if (host < 0) return -1;
    int attached = attach(root, host, "mnt");
    int saved = errno;close(host);errno = saved;
    if (attached < 0 || empty_directory(root, "sys") < 0
            || attach(devices, root, "dev") < 0) return -1;
    const struct parameter proc[] = {{"hidepid", "2"}, {"subset", "pid"}};
    const struct parameter pts[] = {{"newinstance", NULL}, {"gid", "5"}, {"mode", "0620"},
                                    {"ptmxmode", "0666"}, {"max", "128"}};
    const unsigned restricted = MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV | MOUNT_ATTR_NOEXEC;
    if (fresh_at(root, "proc", "proc", proc, COUNT(proc), restricted) < 0
            || fresh_at(root, "dev/pts", "devpts", pts, COUNT(pts), MOUNT_ATTR_NOSUID | MOUNT_ATTR_NOEXEC) < 0
            || fresh_at(root, "dev/mqueue", "mqueue", NULL, 0, restricted) < 0
            || tmpfs_at(root, "dev/shm", "67108864", "8192", "1777", MOUNT_ATTR_NOEXEC) < 0
            || tmpfs_at(root, "tmp", "134217728", "16384", "1777", 0) < 0
            || tmpfs_at(root, "run", "16777216", "4096", "0755", MOUNT_ATTR_NOEXEC) < 0
            || sethostname("aegis-package", 13) < 0 || fchdir(root) < 0) return -1;
    return trusted_tools(root);
}
static int mount_inventory(void) {
    const char *expected[] = {"/", "/dev", "/proc", "/dev/pts",
        "/dev/mqueue", "/dev/shm", "/tmp", "/run", "/tmp/aegis-trusted"};
    unsigned seen = 0;
    FILE *file = fopen("/proc/self/mountinfo", "re");
    if (!file) return -1;
    char line[8192], name[4096];
    int result = -1;
    while (fgets(line, sizeof(line), file)) {
        if (!strchr(line, '\n') || strstr(line, " shared:") || strstr(line, " master:")
                || sscanf(line, "%*u %*u %*s %*s %4095s", name) != 1) { denied(); goto done; }
        unsigned i;
        for (i = 0; i < COUNT(expected); i++) if (!strcmp(name, expected[i])) break;
        if (i == COUNT(expected) || (seen & (1u << i))) { denied(); goto done; }
        seen |= 1u << i;
    }
    if (ferror(file)) goto done;
    if (seen != (1u << COUNT(expected)) - 1) { denied(); goto done; }
    result = 0;
done:;
    int saved = errno;
    fclose(file);
    errno = saved;
    return result;
}

static int readback(void) {
    const unsigned long restricted = ST_NOSUID | ST_NODEV | ST_NOEXEC;
    const struct {
        const char *name; uint64_t type; unsigned long required, forbidden;
    } views[] = {
        {"/", EXT4_SUPER_MAGIC, ST_NOSUID | ST_NODEV, ST_RDONLY | ST_NOEXEC},
        {"/dev", TMPFS_MAGIC, ST_RDONLY | ST_NOSUID | ST_NOEXEC, ST_NODEV},
        {"/proc", PROC_SUPER_MAGIC, restricted, ST_RDONLY},
        {"/dev/pts", DEVPTS_SUPER_MAGIC, ST_NOSUID | ST_NOEXEC, ST_RDONLY | ST_NODEV},
        {"/dev/mqueue", AEGIS_MQUEUE_MAGIC, restricted, ST_RDONLY},
        {"/dev/shm", TMPFS_MAGIC, restricted, ST_RDONLY},
        {"/tmp", TMPFS_MAGIC, ST_NOSUID | ST_NODEV, ST_RDONLY | ST_NOEXEC},
        {"/run", TMPFS_MAGIC, restricted, ST_RDONLY},
        {"/tmp/aegis-trusted", TMPFS_MAGIC, ST_RDONLY | ST_NOSUID | ST_NODEV, ST_NOEXEC},
    };
    for (unsigned i = 0; i < COUNT(views); i++) {
        struct statfs fs;
        struct statvfs flags;
        if (statfs(views[i].name, &fs) < 0 || statvfs(views[i].name, &flags) < 0) return -1;
        if (fs.f_type != views[i].type || (flags.f_flag & views[i].required) != views[i].required
                || (flags.f_flag & views[i].forbidden)) return denied();
    }
    const struct { const char *name; uid_t uid; mode_t mode; } dirs[] = {
        {"/", 0, 0755}, {"/dev", 0, 0755},
        {"/tmp", 0, 01777}, {"/dev/shm", 0, 01777}, {"/run", 0, 0755},
    };
    for (unsigned i = 0; i < COUNT(dirs); i++) {
        struct stat st;
        if (lstat(dirs[i].name, &st) < 0) return -1;
        if (st.st_mode != (S_IFDIR | dirs[i].mode) || st.st_uid != dirs[i].uid
                || st.st_gid != dirs[i].uid) return denied();
    }
    struct stat st;
    if (lstat("/dev/pts/ptmx", &st) < 0) return -1;
    if (st.st_mode != (S_IFCHR | 0666) || st.st_rdev != makedev(5, 2)) return denied();
    char self[32];
    if (readlink("/proc/self", self, sizeof(self)) != 1 || self[0] != '1') return denied();
    if (lstat("/proc/sys", &st) == 0 || errno != ENOENT) return denied();
    struct stat root, parent;
    if (stat("/", &root) < 0 || stat("/..", &parent) < 0) return -1;
    if (root.st_dev != parent.st_dev || root.st_ino != parent.st_ino) return denied();
    return mount_inventory();
}

static int reply(const struct aegis_package_execution_request *request, uint32_t phase,
                 uint32_t status, uint32_t error) {
    struct aegis_package_execution_reply r = {.magic = AEGIS_PACKAGE_EXEC_MAGIC,
        .version = AEGIS_PACKAGE_EXEC_VERSION, .user = request->user, .serial = request->serial,
        .job = request->job, .phase = phase, .status = status, .error = error};
    memcpy(r.plan, request->plan, sizeof(r.plan));
    ssize_t n;
    do { n = send(3, &r, sizeof(r), MSG_NOSIGNAL | MSG_DONTWAIT); } while (n < 0 && errno == EINTR);
    return n == (ssize_t)sizeof(r) ? 0 : -1;
}
static int regular_at(int root, const char *name, int flags, unsigned mode) {
    struct open_how how = {.flags = (uint64_t)(flags | O_NOFOLLOW | O_CLOEXEC), .mode = mode,
        .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_XDEV};
    int fd = syscall(SYS_openat2, root, name, &how, sizeof(how));
    if (fd < 0) return -1;
    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_uid || st.st_gid
            || st.st_nlink != 1 || (st.st_mode & (S_ISUID | S_ISGID | S_IWGRP | S_IWOTH))) {
        close(fd);errno = EPERM;return -1;
    }
    return fd;
}
enum package_command { PACKAGE_ACTION, PACKAGE_CHECK, PACKAGE_AUDIT, PACKAGE_VERIFY, PACKAGE_VERIFY_BASELINE, PACKAGE_SIMULATE, PACKAGE_MARK_AUTO, PACKAGE_MARK_MANUAL };
static void log_name(char name[128], uint64_t job, enum package_command command) {
    const char *suffix=command==PACKAGE_SIMULATE ? "-simulate" : command==PACKAGE_MARK_AUTO ? "-auto" : command==PACKAGE_MARK_MANUAL ? "-manual" : command==PACKAGE_VERIFY_BASELINE ? "-verify-before" : command==PACKAGE_CHECK ? "-check" : command==PACKAGE_AUDIT ? "-audit" : command==PACKAGE_VERIFY ? "-verify" : "";
    snprintf(name,128,"var/log/aegis-package-%llu%s.log",(unsigned long long)job,suffix);
}
static _Noreturn void apt(const struct aegis_package_execution_request *r, enum package_command command) {
    // Drop the private broker socket and every inherited descriptor BEFORE
    // giving control to Debian. Fresh opens cannot refer outside the pivot.
    if (syscall(SYS_close_range, 0u, UINT_MAX, 0u) < 0) _exit(126);
    int in = open("/dev/null", O_RDONLY | O_CLOEXEC);
    int root = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    char name[128];log_name(name,r->job,command);
    int log = root < 0 ? -1 : regular_at(root, name, O_CREAT | O_WRONLY, 0600);
    if (in != 0 || root < 0 || log < 0) _exit(126);
    // IDs may start again after a broker reboot. Only this inactive copy is
    // writable; reject links/foreign ownership BEFORE replacing an old log.
    if (fchmod(log, 0600) < 0 || ftruncate(log, 0) < 0) _exit(126);
    // Close root before replacing stdio; it can occupy slot 1.
    close(root);
    if (dup2(log, 1) < 0 || dup2(log, 2) < 0) _exit(126);
    for (int fd = 0; fd < 3; fd++) if (fcntl(fd, F_SETFD, 0) < 0) _exit(126);
    if (syscall(SYS_close_range, 3u, UINT_MAX, 0u) < 0) _exit(126);
    char paths[AEGIS_PACKAGE_EXEC_ITEMS][AEGIS_PACKAGE_EXEC_NAME + 32];
    char *args[AEGIS_PACKAGE_EXEC_ITEMS + 24];unsigned n = 0;
    args[n++] = "/usr/bin/apt-get";args[n++] = "-y";args[n++] = "--no-download";
    args[n++] = "-o";args[n++] = "Dpkg::Use-Pty=0";
    args[n++] = "-o";args[n++] = "Dpkg::Options::=--force-confold";
    // The pinned slim base excludes documentation/locales through dpkg.cfg.
    // New packages must be complete: the final matching command-line filter
    // includes their full payload, while verification still rejects new loss.
    args[n++] = "-o";args[n++] = "Dpkg::Options::=--path-include=/*";
    args[n++] = "-o";args[n++] = "Dir::Etc::sourcelist=/run/aegis-empty.list";
    args[n++] = "-o";args[n++] = "Dir::Etc::sourceparts=/run/aegis-empty.d";
    if(command==PACKAGE_SIMULATE) {
        args[n++]="--simulate";args[n++]="-o";
        args[n++]="AptCli::Hooks::Install::=/tmp/aegis-trusted/hook --apt-plan-hook";
    }
    args[n++] = command==PACKAGE_CHECK ? "check" : r->kind == AEGIS_PACKAGE_REMOVE ? "remove" : "install";
    for (unsigned i = 0; (command==PACKAGE_ACTION || command==PACKAGE_SIMULATE) && i < r->count; i++) {
        if (aegis_package_has_archive(r,i)) {
            snprintf(paths[i], sizeof(paths[i]), "/var/cache/apt/archives/%s", r->items[i]);
            args[n++] = paths[i];
        } else if (r->kind == AEGIS_PACKAGE_MIXED) {
            // Validated bare package name; only this trusted code adds APT's
            // explicit removal suffix to an install operation.
            snprintf(paths[i], sizeof(paths[i]), "%s-", r->items[i]);
            args[n++] = paths[i];
        } else args[n++] = (char *)r->items[i];
    }
    if (command==PACKAGE_AUDIT || command==PACKAGE_VERIFY || command==PACKAGE_VERIFY_BASELINE) {
        n=0;args[n++]="/usr/bin/dpkg";
        args[n++]=command==PACKAGE_AUDIT ? "--audit" : "--verify";
        if (command==PACKAGE_VERIFY || command==PACKAGE_VERIFY_BASELINE) args[n++]="--verify-format=rpm";
    }
    if(command==PACKAGE_MARK_AUTO || command==PACKAGE_MARK_MANUAL) {
        n=0;args[n++]="/usr/bin/apt-mark";args[n++]=command==PACKAGE_MARK_AUTO?"auto":"manual";
        for(unsigned i=0;i<r->count;++i)if(*r->review.effects[i].after
                && r->review.effects[i].reason==(command==PACKAGE_MARK_AUTO?2:1))args[n++]=(char*)r->review.effects[i].name;
        if(n==2)_exit(0);
    }
    args[n] = NULL;
    char *env[] = {"PATH=/usr/sbin:/usr/bin:/sbin:/bin", "LANG=C", "LC_ALL=C", "HOME=/root",
                   "DEBIAN_FRONTEND=noninteractive", "APT_CONFIG=/tmp/aegis-trusted/config", NULL};
    aegis_package_exec_program(args,env);_exit(127);
}
// Preserve the bounded structured simulation in the owned candidate. /run is
// ephemeral, so a rejected plan must remain diagnosable after PID1 exits.
// This log is evidence only; the guard still reads the original pinned FD.
static int save_simulation(int root,int input,uint64_t job) {
    struct stat st;
    if(fstat(input,&st)<0)return -1;
    if(!S_ISREG(st.st_mode)||st.st_size<0||st.st_size>262144) { errno=EFBIG;return -1; }
    char name[128];
    snprintf(name,sizeof(name),"var/log/aegis-package-%llu-simulate.json",(unsigned long long)job);
    int output=regular_at(root,name,O_CREAT|O_WRONLY,0600);if(output<0)return -1;
    int error=0;
    if(fchmod(output,0600)<0||ftruncate(output,0)<0)error=errno;
    char buffer[4096];
    for(off_t at=0;!error&&at<st.st_size;) {
        size_t want=(size_t)(st.st_size-at);if(want>sizeof(buffer))want=sizeof(buffer);
        ssize_t n=pread(input,buffer,want,at);if(n<0&&errno==EINTR)continue;
        if(n<=0) { error=n<0?errno:ESTALE;break; }
        if(write_all(output,buffer,(size_t)n)<0) { error=errno;break; }
        at+=n;
    }
    if(close(output)<0&&!error)error=errno;
    if(error) { errno=error;return -1; }return 0;
}
// Only our independently verified PID namespace is visible here. PID1 cannot
// leave a background maintainer script mutating metadata during validation.
static int reap_descendants(void) {
    if (getpid()!=1) return denied();
    if (kill(-1,SIGKILL)<0 && errno!=ESRCH) return -1;
    for (;;) {
        int status;pid_t child=waitpid(-1,&status,0);
        if (child>0 || (child<0 && errno==EINTR)) continue;
        return child<0 && errno==ECHILD ? 0 : -1;
    }
}
static uint32_t run_command(const struct aegis_package_execution_request *request,
                            enum package_command command,uint32_t *result) {
    pid_t child=fork();if (!child) apt(request,command);
    uint32_t error=child<0 ? (uint32_t)errno : 0;int status;
    if (child>0) {
        pid_t waited;do { waited=waitpid(child,&status,0); } while (waited<0&&errno==EINTR);
        if (waited<0) error=errno;
        else *result=WIFEXITED(status) ? (uint32_t)WEXITSTATUS(status) : 128u+(uint32_t)WTERMSIG(status);
    }
    if (reap_descendants()<0 && !error) error=errno;
    return error;
}
static int audit_empty(int root,uint64_t job) {
    char name[128];log_name(name,job,PACKAGE_AUDIT);
    int fd=regular_at(root,name,O_RDONLY,0);if (fd<0) return -1;
    struct stat st;int result=fstat(fd,&st),saved=errno;close(fd);
    if (result<0) { errno=saved;return -1; }
    // dpkg may report warnings on stdout/stderr even when the audit command
    // itself exited successfully. Any diagnostic keeps this candidate inactive.
    return st.st_size==0 ? 0 : (errno=EBADMSG,-1);
}
// The pinned upstream slim image deliberately removed documentary files after
// dpkg recorded them. Capture only those pre-existing missing rows BEFORE APT
// in PID1 memory, never in a candidate-controlled allow-list or reread logfile.
// Source image/hash admission remains the caller's obligation. This is dpkg
// consistency, not repository authentication or a security digest proof.
struct verification_baseline { char **rows;size_t count; };
static int compare_rows(const void *a,const void *b) {
    return strcmp(*(const char *const *)a,*(const char *const *)b);
}
static void baseline_free(struct verification_baseline *baseline) {
    for (size_t i=0;i<baseline->count;i++) free(baseline->rows[i]);
    free(baseline->rows);baseline->rows=NULL;baseline->count=0;
}
static int canonical_path(const char *path) {
    if (*path++!='/') return 0;
    while (*path) {
        const char *start=path;
        while (*path && *path!='/') { if ((unsigned char)*path<=32) return 0;path++; }
        size_t n=(size_t)(path-start);
        if (!n || (n==1 && *start=='.') || (n==2 && start[0]=='.' && start[1]=='.')) return 0;
        if (*path && !*++path) return 0;
    }
    return 1;
}
static int slim_missing_path(const char *path) {
    // A bounded policy for the pinned upstream base, not globs supplied by a
    // package. In particular, no executable/library/account/DB path qualifies.
    const char *prefixes[]={"/usr/share/doc/","/usr/share/info/","/usr/share/man/",
        "/usr/share/locale/","/usr/share/lintian/overrides/"};
    if (!canonical_path(path)) return 0;
    for (unsigned i=0;i<COUNT(prefixes);i++) {
        size_t n=strlen(prefixes[i]);
        if (!strncmp(path,prefixes[i],n) && path[n]) {
            // Upstream explicitly retains copyright files.
            const char *last=strrchr(path,'/');
            return strcmp(last+1,"copyright")!=0;
        }
    }
    return !strcmp(path,"/var/cache/apt/archives") || !strcmp(path,"/var/cache/apt/archives/partial")
        || !strcmp(path,"/var/lib/apt/lists/partial");
}
static int verify_output(int root,uint64_t job,uint32_t status,
                          struct verification_baseline *baseline,int before) {
    if (status>1) return (errno=EBADMSG,-1);
    char name[128];log_name(name,job,before ? PACKAGE_VERIFY_BASELINE : PACKAGE_VERIFY);
    int fd=regular_at(root,name,O_RDONLY,0);if (fd<0) return -1;
    struct stat st;
    if (fstat(fd,&st)<0) { int saved=errno;close(fd);errno=saved;return -1; }
    if (st.st_size>1024*1024 || (status && !st.st_size)) { close(fd);errno=EBADMSG;return -1; }
    FILE *stream=fdopen(fd,"r");if (!stream) { int saved=errno;close(fd);errno=saved;return -1; }
    char *row=NULL;size_t allocated=0;ssize_t n;int error=0;
    while ((n=getline(&row,&allocated,stream))>=0) {
        if (n<14 || n>PATH_MAX+32 || memchr(row,0,(size_t)n) || row[n-1]!='\n') { error=EBADMSG;break; }
        row[n-1]=0;
        // --force-confold deliberately preserves locally changed conffiles.
        if (!strncmp(row,"??5?????? c /",13) && canonical_path(row+12)) continue;
        if (strncmp(row,"missing     /",13) || !slim_missing_path(row+12)) { error=EBADMSG;break; }
        if (before) {
            if (baseline->count==16384) { error=E2BIG;break; }
            char **rows=realloc(baseline->rows,(baseline->count+1)*sizeof(*rows));
            if (!rows) { error=ENOMEM;break; }baseline->rows=rows;
            char *copy=strdup(row);if (!copy) { error=ENOMEM;break; }
            baseline->rows[baseline->count++]=copy;
        } else if (!baseline->count || !bsearch(&row,baseline->rows,baseline->count,sizeof(char *),compare_rows)) {
            error=EBADMSG;break;
        }
    }
    if (!error && ferror(stream)) error=errno ? errno : EIO;
    free(row);fclose(stream);
    if (!error && before && baseline->count) qsort(baseline->rows,baseline->count,sizeof(char *),compare_rows);
    if (error) { errno=error;return -1; }return 0;
}
static int setup_failed(const struct aegis_package_execution_request *request) {
    int error = errno > 0 && errno <= 4095 ? errno : EIO;
    if (aegis_package_execution_valid(request))
        (void)reply(request, AEGIS_PACKAGE_EXEC_READY, 0, (uint32_t)error);
    return 125;
}
int aegis_package_execute(uint32_t user, uint32_t serial, int permit_unbound_fixture) {
    if (aegis_check_package_namespaces(user) < 0) return 78;
    int fds[3] = {-1, -1, -1};
    struct aegis_package_execution_request request = {0};
    if (syscall(SYS_close_range, 4u, UINT_MAX, 0u) < 0 || clearenv() < 0) return setup_failed(&request);
    close(0);close(1);close(2);umask(022);
    struct sigaction action = {.sa_handler = expired};sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL) < 0) return setup_failed(&request);
    alarm(10);
    if (aegis_package_execution_receive(3, user, serial, fds, &request) < 0
            || directory(fds[0], EXT4_SUPER_MAGIC, ST_NOSUID | ST_NODEV, ST_RDONLY | ST_NOEXEC) < 0
            || directory(fds[1], TMPFS_MAGIC, ST_RDONLY | ST_NOSUID | ST_NOEXEC, ST_NODEV) < 0
            || construct(fds[0], fds[1]) < 0) return setup_failed(&request);
    if(!request.review.present && !permit_unbound_fixture) { errno=EPERM;return setup_failed(&request); }
    for (int i = 0; i < 3; i++) close(fds[i]);
    if (syscall(SYS_close_range, 4u, UINT_MAX, 0u) < 0
            || syscall(SYS_pivot_root, ".", ".") < 0 || umount2(".", MNT_DETACH) < 0
            || chdir("/") < 0 || readback() < 0) return setup_failed(&request);
    int root = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (root < 0 || aegis_package_candidate_labels(root,1)<0) return setup_failed(&request);
    for (unsigned i = 0; i < request.count; i++) if(aegis_package_has_archive(&request,i)) {
        char path[AEGIS_PACKAGE_EXEC_NAME + 32];
        snprintf(path, sizeof(path), "var/cache/apt/archives/%s", request.items[i]);
        int archive = regular_at(root, path, O_RDONLY, 0);
        if (archive < 0) return setup_failed(&request);
        close(archive);
    }
    int empty = open("/run/aegis-empty.list", O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (empty < 0 || mkdir("/run/aegis-empty.d", 0755) < 0) return setup_failed(&request);
    close(empty);
    if (aegis_limit_package_supervisor(user) < 0 || reply(&request, AEGIS_PACKAGE_EXEC_READY, 0, 0) < 0) return setup_failed(&request);
    alarm(0);
    uint32_t result=0;
    struct verification_baseline baseline={0};
    struct aegis_package_guard *guard=NULL;
    uint32_t error=0;
    if(request.review.present && aegis_package_guard_begin(root,&request,&guard)<0)error=errno;
    if(!error)error=run_command(&request,PACKAGE_VERIFY_BASELINE,&result);
    if (!error) {
        if (verify_output(root,request.job,result,&baseline,1)<0) error=errno;
        else result=0;
    }
    if(!error && !result && guard) {
        error=run_command(&request,PACKAGE_SIMULATE,&result);
        if(!error && !result) {
            int fd=open("/run/aegis-apt-plan.json",O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
            if(fd<0)error=errno;
            else { if(save_simulation(root,fd,request.job)<0 || aegis_package_guard_simulation(guard,fd)<0)error=errno;close(fd); }
        }
    }
    if (!error && !result) error=run_command(&request,PACKAGE_ACTION,&result);
    // Scripts may replace a packaged regular file with a FIFO. Reject special
    // nodes and invalid account metadata BEFORE dpkg opens candidate files for
    // verification, then validate again after the consistency commands finish.
    if (!error && !result && aegis_package_validate(root)<0) error=errno;
    if(!error && !result && guard)error=run_command(&request,PACKAGE_MARK_AUTO,&result);
    if(!error && !result && guard)error=run_command(&request,PACKAGE_MARK_MANUAL,&result);
    if(!error && !result && guard && aegis_package_guard_finish(guard,root)<0)error=errno;
    if (!error && !result) error=run_command(&request,PACKAGE_CHECK,&result);
    if (!error && !result) error=run_command(&request,PACKAGE_AUDIT,&result);
    if (!error && !result && audit_empty(root,request.job)<0) error=errno;
    if (!error && !result) {
        error=run_command(&request,PACKAGE_VERIFY,&result);
        if (!error) {
            if (verify_output(root,request.job,result,&baseline,0)<0) error=errno;
            else result=0; // only intentional conffile differences were reported
        }
    }
    baseline_free(&baseline);
    if (!error && !result && aegis_package_validate(root)<0) error=errno;
    if(!error && !result && guard && aegis_package_guard_finish(guard,root)<0)error=errno;
    aegis_package_guard_free(guard);
    if (syncfs(root) < 0 && !error) error = errno;
    close(root);
    // PID1 exit tears down any script descendants; owning broker still must
    // reap it, kill/wait the complete cgroup and close all mount/backing refs.
    return reply(&request, AEGIS_PACKAGE_EXEC_DONE, result, error) == 0 ? 0 : 125;
}
