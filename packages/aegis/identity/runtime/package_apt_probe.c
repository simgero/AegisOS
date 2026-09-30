/* Device fixture only: runs the pinned Debian APT/dpkg with synthetic offline
 * packages in an exclusively owned image. No AOSP user, CE, production policy,
 * production repository trust or publication claim. No user-selected path/script/option. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "package_apt_probe.h"
#include "package_apt_metadata_fixture.h"
#include "sandbox.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/mount.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

static const char script[] =
"set -eu\n"
"umask 022\n"
"export DEBIAN_FRONTEND=noninteractive\n"
"export LC_ALL=C\n"
"export PATH=/usr/sbin:/usr/bin:/sbin:/bin\n"
"[ \"$(id -u)\" = 0 ]\n"
"[ ! -e /system ] && [ ! -e /data ] && [ ! -e /sys/fs/cgroup ]\n"
"apt-get --version | head -n 1\n"
"dpkg --version | head -n 1\n"
"for version in 1 2; do\n"
"  for package in lib app; do\n"
"    tree=/tmp/fixture-$package-$version\n"
"    mkdir -p \"$tree/DEBIAN\"\n"
"    cat > \"$tree/DEBIAN/control\" <<EOF\n"
"Package: aegis-probe-$package\n"
"Version: $version\n"
"Architecture: all\n"
"Maintainer: AEGIS local test <test@invalid>\n"
"Description: Disposable offline package fixture\n"
"EOF\n"
"    if [ \"$package\" = lib ]; then\n"
"      mkdir -p \"$tree/usr/share/aegis-probe\"\n"
"      printf '%s\\n' \"$version\" > \"$tree/usr/share/aegis-probe/library\"\n"
"    else\n"
"      printf 'Depends: aegis-probe-lib (= %s)\\n' \"$version\" >> \"$tree/DEBIAN/control\"\n"
"      mkdir -p \"$tree/usr/bin\" \"$tree/etc\"\n"
"      printf '#!/bin/sh\\nprintf \"app-%s\\\\n\"\\n' \"$version\" > \"$tree/usr/bin/aegis-probe-app\"\n"
"      chmod 0755 \"$tree/usr/bin/aegis-probe-app\"\n"
"      printf 'version=%s\\n' \"$version\" > \"$tree/etc/aegis-probe.conf\"\n"
"      printf '/etc/aegis-probe.conf\\n' > \"$tree/DEBIAN/conffiles\"\n"
"      for phase in preinst postinst prerm postrm; do\n"
"        printf '#!/bin/sh\\nset -eu\\nprintf \"%%s\\\\n\" \"%s-%s-$1\" >> /var/log/aegis-probe-scripts.log\\n' \"$phase\" \"$version\" > \"$tree/DEBIAN/$phase\"\n"
"        chmod 0755 \"$tree/DEBIAN/$phase\"\n"
"      done\n"
"      cat >> \"$tree/DEBIAN/postinst\" <<'EOF'\n"
"mkdir -p /var/lib/aegis-probe-owned\n"
"chown 42:42 /var/lib/aegis-probe-owned\n"
"chmod 0700 /var/lib/aegis-probe-owned\n"
"EOF\n"
"    fi\n"
"    dpkg-deb --root-owner-group -Zgzip --build \"$tree\" \"/tmp/aegis-probe-$package-$version.deb\"\n"
"  done\n"
"done\n"
"# Trusted resolver fixture, separate from the execution source configuration.\n"
"mkdir -p /tmp/aegis-plan/lists/partial /tmp/aegis-plan/cache/archives/partial /tmp/aegis-empty.d\n"
": > /tmp/aegis-empty.conf\n"
"cat > /tmp/aegis-plan.sources.list <<'EOF'\n"
"deb [signed-by=/tmp/aegis-test-key.asc] copy:/tmp/aegis-repo ./\n"
"EOF\n"
"cat > /tmp/aegis-plan.conf <<'EOF'\n"
"Dir::Etc::parts \"/tmp/aegis-empty.d\";\n"
"Dir::Etc::main \"/tmp/aegis-empty.conf\";\n"
"Dir::Etc::sourcelist \"/tmp/aegis-plan.sources.list\";\n"
"Dir::Etc::sourceparts \"/tmp/aegis-empty.d\";\n"
"Dir::Etc::trusted \"/tmp/aegis-empty.gpg\";\n"
"Dir::Etc::trustedparts \"/tmp/aegis-empty.d\";\n"
"Dir::State::lists \"/tmp/aegis-plan/lists\";\n"
"Dir::State::status \"/var/lib/dpkg/status\";\n"
"Dir::State::extended_states \"/tmp/aegis-plan/extended_states\";\n"
"Dir::Cache::archives \"/tmp/aegis-plan/cache/archives\";\n"
"Dir::Cache::pkgcache \"\";\n"
"Dir::Cache::srcpkgcache \"\";\n"
"Dir::Log \"/tmp/aegis-plan\";\n"
"Dir::Bin::dpkg \"/usr/bin/false\";\n"
"APT::Architecture \"arm64\";\n"
"APT::Architectures { \"arm64\"; };\n"
"APT::Install-Recommends \"false\";\n"
"APT::Install-Suggests \"false\";\n"
"APT::Get::AllowUnauthenticated \"false\";\n"
"APT::Update::Error-Mode \"any\";\n"
"Acquire::AllowInsecureRepositories \"false\";\n"
"Acquire::AllowDowngradeToInsecureRepositories \"false\";\n"
"Acquire::AllowWeakRepositories \"false\";\n"
"Acquire::Check-Date \"true\";\n"
"Acquire::Check-Valid-Until \"true\";\n"
"AptCli::Hooks::Install { \"/tmp/aegis-plan-hook --apt-plan-hook\"; };\n"
"AptCli::Hooks::Upgrade { \"/tmp/aegis-plan-hook --apt-plan-hook\"; };\n"
"EOF\n"
"planner() { APT_CONFIG=/tmp/aegis-plan.conf apt-get -q \"$@\"; }\n"
"fresh_lists() {\n"
"  rm -rf /tmp/aegis-plan/lists\n"
"  mkdir -p /tmp/aegis-plan/lists/partial\n"
"}\n"
"planner update\n"
"cp /tmp/aegis-repo/Release /tmp/aegis-original-Release\n"
"cp /tmp/aegis-repo/Release.gpg /tmp/aegis-original-Release.gpg\n"
"cp /tmp/aegis-repo/Packages /tmp/aegis-original-Packages\n"
"rm /tmp/aegis-repo/Release.gpg\n"
"fresh_lists\n"
"if planner update > /var/log/aegis-plan-unsigned.log 2>&1; then exit 81; fi\n"
"grep -qi 'not signed' /var/log/aegis-plan-unsigned.log\n"
"cp /tmp/aegis-original-Release.gpg /tmp/aegis-repo/Release.gpg\n"
"sed -i 's/AEGIS TEST ONLY/AEGIS CHANGED/' /tmp/aegis-repo/Release\n"
"fresh_lists\n"
"if planner update > /var/log/aegis-plan-signature.log 2>&1; then exit 82; fi\n"
"cp /tmp/aegis-expired-Release /tmp/aegis-repo/Release\n"
"cp /tmp/aegis-expired-Release.gpg /tmp/aegis-repo/Release.gpg\n"
"fresh_lists\n"
"if planner update > /var/log/aegis-plan-expired.log 2>&1; then exit 83; fi\n"
"grep -qi 'expired' /var/log/aegis-plan-expired.log\n"
"cp /tmp/aegis-original-Release /tmp/aegis-repo/Release\n"
"cp /tmp/aegis-original-Release.gpg /tmp/aegis-repo/Release.gpg\n"
"printf 'tampered\\n' >> /tmp/aegis-repo/Packages\n"
"fresh_lists\n"
"if planner update > /var/log/aegis-plan-index.log 2>&1; then exit 84; fi\n"
"grep -qi 'Hash Sum mismatch' /var/log/aegis-plan-index.log\n"
"cp /tmp/aegis-original-Packages /tmp/aegis-repo/Packages\n"
"fresh_lists\n"
"planner update\n"
"cp /tmp/aegis-repo/Packages /var/log/aegis-plan-Packages\n"
"status_before=$(sha256sum /var/lib/dpkg/status)\n"
"planner --simulate --no-download install aegis-probe-app=2\n"
"cp /run/aegis-apt-plan.json /var/log/aegis-plan-install.json\n"
"rm /run/aegis-apt-plan.json\n"
"[ \"$status_before\" = \"$(sha256sum /var/lib/dpkg/status)\" ]\n"
"[ ! -e /var/log/aegis-probe-scripts.log ]\n"
"# A valid signed index must reject changed archive bytes before installation.\n"
"cp /tmp/aegis-repo/aegis-probe-app_2_all.deb /tmp/aegis-original-app.deb\n"
"printf 'tampered\\n' >> /tmp/aegis-repo/aegis-probe-app_2_all.deb\n"
"if planner --download-only --yes install aegis-probe-app=2 > /var/log/aegis-plan-archive.log 2>&1; then exit 86; fi\n"
"grep -qi 'Hash Sum mismatch' /var/log/aegis-plan-archive.log\n"
"rm -f /run/aegis-apt-plan.json\n"
"cp /tmp/aegis-original-app.deb /tmp/aegis-repo/aegis-probe-app_2_all.deb\n"
"planner --download-only --yes install aegis-probe-app=2\n"
"rm /run/aegis-apt-plan.json\n"
"for name in app lib; do\n"
"  cmp /tmp/aegis-plan/cache/archives/aegis-probe-${name}_2_all.deb /tmp/aegis-repo/aegis-probe-${name}_2_all.deb\n"
"  cp /tmp/aegis-plan/cache/archives/aegis-probe-${name}_2_all.deb /var/log/aegis-plan-${name}.deb\n"
"done\n"
"[ \"$status_before\" = \"$(sha256sum /var/lib/dpkg/status)\" ]\n"
"[ ! -e /var/log/aegis-probe-scripts.log ]\n"
"if planner --simulate --no-download install aegis-probe-app=999 > /var/log/aegis-plan-missing.log 2>&1; then exit 85; fi\n"
"[ ! -e /run/aegis-apt-plan.json ]\n"
"printf 'SIGNED_METADATA_RESOLUTION_OK\\nUNSIGNED_REJECTED\\nSIGNATURE_TAMPER_REJECTED\\nEXPIRED_REJECTED\\nINDEX_TAMPER_REJECTED\\nARCHIVE_TAMPER_REJECTED\\nARCHIVE_DOWNLOAD_VERIFIED\\nMISSING_VERSION_REJECTED\\n' > /var/log/aegis-plan-tests.complete\n"
"mkdir -p /tmp/aegis-sources.d\n"
"touch /tmp/aegis-sources.list\n"
"apt() {\n"
"  apt-get -y --no-download -o Dpkg::Use-Pty=0 -o Dpkg::Options::=--force-confold \\\n"
"    -o Dir::Etc::sourcelist=/tmp/aegis-sources.list -o Dir::Etc::sourceparts=/tmp/aegis-sources.d \"$@\"\n"
"}\n"
"mkdir -p /var/cache/apt/archives\n"
"cp /tmp/aegis-probe-app-1.deb /var/cache/apt/archives/aegis-probe-app_1_all.deb\n"
"cp /tmp/aegis-probe-lib-1.deb /var/cache/apt/archives/aegis-probe-lib_1_all.deb\n"
"apt install /tmp/aegis-probe-app-1.deb /tmp/aegis-probe-lib-1.deb\n"
"[ \"$(aegis-probe-app)\" = app-1 ]\n"
"[ \"$(cat /usr/share/aegis-probe/library)\" = 1 ]\n"
"[ \"$(dpkg-query -W -f='${Version}' aegis-probe-app)\" = 1 ]\n"
"[ \"$(stat -c %u:%g /var/lib/aegis-probe-owned)\" = 42:42 ]\n"
"status_before=$(sha256sum /var/lib/dpkg/status)\n"
"planner --simulate --no-download upgrade\n"
"cp /run/aegis-apt-plan.json /var/log/aegis-plan-upgrade.json\n"
"rm /run/aegis-apt-plan.json\n"
"[ \"$status_before\" = \"$(sha256sum /var/lib/dpkg/status)\" ]\n"
"[ \"$(aegis-probe-app)\" = app-1 ]\n"
"printf 'personal=kept\\n' > /etc/aegis-probe.conf\n"
"mkdir -p /var/cache/apt/archives\n"
"cp /tmp/aegis-probe-app-2.deb /var/cache/apt/archives/aegis-probe-app_2_all.deb\n"
"cp /tmp/aegis-probe-lib-2.deb /var/cache/apt/archives/aegis-probe-lib_2_all.deb\n"
"apt install /tmp/aegis-probe-app-2.deb /tmp/aegis-probe-lib-2.deb\n"
"[ \"$(aegis-probe-app)\" = app-2 ]\n"
"[ \"$(cat /usr/share/aegis-probe/library)\" = 2 ]\n"
"[ \"$(dpkg-query -W -f='${Version}' aegis-probe-app)\" = 2 ]\n"
"[ \"$(dpkg-query -W -f='${Version}' aegis-probe-lib)\" = 2 ]\n"
"[ \"$(cat /etc/aegis-probe.conf)\" = personal=kept ]\n"
"status_before=$(sha256sum /var/lib/dpkg/status)\n"
"planner --simulate --no-download remove aegis-probe-lib\n"
"cp /run/aegis-apt-plan.json /var/log/aegis-plan-remove.json\n"
"rm /run/aegis-apt-plan.json\n"
"[ \"$status_before\" = \"$(sha256sum /var/lib/dpkg/status)\" ]\n"
"[ \"$(aegis-probe-app)\" = app-2 ]\n"
"apt purge aegis-probe-app aegis-probe-lib\n"
"[ ! -e /usr/bin/aegis-probe-app ]\n"
"[ ! -e /usr/share/aegis-probe/library ]\n"
"[ ! -e /etc/aegis-probe.conf ]\n"
"for expected in preinst-1-install postinst-1-configure preinst-2-upgrade postinst-2-configure prerm-2-remove postrm-2-purge; do\n"
"  grep -qx \"$expected\" /var/log/aegis-probe-scripts.log\n"
"done\n"
"test -z \"$(dpkg --audit)\"\n"
"printf 'APT_INSTALL_UPGRADE_PURGE_OK\\n'\n"
"printf 'APT_INSTALL_UPGRADE_PURGE_OK\\n' > /var/log/aegis-package-test.complete\n"
;

static int attach(int tree, int root, const char *name) {
    int target = openat(root, name, O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (target < 0) return -1;
    int result = syscall(SYS_move_mount, tree, "", target, "",
                         MOVE_MOUNT_F_EMPTY_PATH | MOVE_MOUNT_T_EMPTY_PATH);
    int saved = errno;close(target);errno = saved;return result;
}
static int inputs(int fds[2]) {
    union { struct cmsghdr alignment; char bytes[CMSG_SPACE(2 * sizeof(int))]; } extra = {0};
    uint32_t magic = 0;
    struct iovec io = {.iov_base = &magic, .iov_len = sizeof(magic)};
    struct msghdr msg = {.msg_iov = &io, .msg_iovlen = 1, .msg_control = extra.bytes,
                         .msg_controllen = sizeof(extra.bytes)};
    ssize_t n = recvmsg(3, &msg, MSG_CMSG_CLOEXEC);
    unsigned count = 0, headers = 0;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
        headers++;
        if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS || c->cmsg_len < CMSG_LEN(0)) continue;
        size_t bytes = c->cmsg_len - CMSG_LEN(0);
        for (size_t i = 0; i < bytes / sizeof(int); i++) {
            int fd;memcpy(&fd, CMSG_DATA(c) + i * sizeof(fd), sizeof(fd));
            if (count < 2) fds[count++] = fd;else close(fd);
        }
    }
    if (n != (ssize_t)sizeof(magic) || magic != 0x41505446 || headers != 1 || count != 2
            || (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))) { errno = EPROTO;return -1; }
    return 0;
}
static int directory(int fd, uint64_t type, unsigned long required, unsigned long forbidden) {
    struct stat st;struct statfs fs;struct statvfs flags;
    if (fstat(fd, &st) < 0 || fstatfs(fd, &fs) < 0 || fstatvfs(fd, &flags) < 0) return -1;
    if (st.st_mode != (S_IFDIR | 0755) || st.st_uid || st.st_gid || fs.f_type != type
            || (flags.f_flag & required) != required || (flags.f_flag & forbidden)) {
        errno = EPERM;return -1;
    }
    return 0;
}
static int fixture_files(int root) {
    int tmp=openat(root,"tmp",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(tmp<0)return -1;
    int result=-1,in=-1,out=-1;struct stat st;
    if(mkdirat(tmp,"aegis-repo",0755)<0)goto done;
    for(size_t i=0;i<sizeof(aegis_apt_metadata)/sizeof(aegis_apt_metadata[0]);++i) {
        out=openat(tmp,aegis_apt_metadata[i].name,O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW|O_CLOEXEC,0644);
        if(out<0)goto done;
        const unsigned char* data=aegis_apt_metadata[i].data;size_t size=aegis_apt_metadata[i].size,at=0;
        while(at<size) { ssize_t n=write(out,data+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0)goto done;at+=n; }
        if(close(out)<0) { out=-1;goto done; }out=-1;
    }
    in=open("/proc/self/exe",O_RDONLY|O_CLOEXEC);
    out=openat(tmp,"aegis-plan-hook",O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW|O_CLOEXEC,0500);
    if(in<0 || out<0 || fstat(in,&st)<0 || !S_ISREG(st.st_mode) || st.st_size<=0 || st.st_size>8388608)goto done;
    char data[32768];off_t copied=0;
    while(copied<st.st_size) {
        ssize_t n=read(in,data,sizeof(data));if(n<0&&errno==EINTR)continue;if(n<=0)goto done;
        size_t at=0;while(at<(size_t)n) { ssize_t wrote=write(out,data+at,n-at);if(wrote<0&&errno==EINTR)continue;if(wrote<=0)goto done;at+=wrote; }
        copied+=n;
    }
    if(fchmod(out,0500)<0 || fsync(out)<0)goto done;
    result=0;
done:;
    int saved=errno;if(in>=0)close(in);if(out>=0)close(out);close(tmp);errno=saved;return result;
}
int aegis_probe_package_apt(uint32_t user) {
    struct aegis_apt_probe_result result = {.magic = 0x41505452, .phase = 1};
    int fds[2] = {-1, -1};
    if (getpid() != 1 || getppid() || getuid() || getgid()) return 114;
    if (inputs(fds) < 0) goto done;
    result.phase = 2;
    if (directory(fds[0], EXT4_SUPER_MAGIC, ST_NOSUID | ST_NODEV, ST_RDONLY | ST_NOEXEC) < 0
            || directory(fds[1], TMPFS_MAGIC, ST_RDONLY | ST_NOSUID | ST_NOEXEC, ST_NODEV) < 0)
        goto done;
    // All writes are in this exclusively created candidate or fresh tmpfs.
    if (attach(fds[0], AT_FDCWD, "/mnt") < 0 || attach(fds[1], fds[0], "dev") < 0
            || mount("proc", "/mnt/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, "hidepid=2,subset=pid") < 0
            || mount("tmpfs", "/mnt/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777,size=67108864,nr_inodes=8192") < 0
            || mount("tmpfs", "/mnt/run", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "mode=0755,size=4194304,nr_inodes=1024") < 0
            || fixture_files(fds[0]) < 0 || fchdir(fds[0]) < 0) goto done;
    close(fds[0]);close(fds[1]);fds[0] = fds[1] = -1;
    result.phase = 3;
    if (syscall(SYS_close_range, 4u, ~0u, 0u) < 0
            || syscall(SYS_pivot_root, ".", ".") < 0 || umount2(".", MNT_DETACH) < 0
            || chdir("/") < 0 || aegis_limit_package_worker(user) < 0) goto done;
    result.phase = 4;
    pid_t child = fork();
    if (child < 0) goto done;
    if (!child) {
        close(3);
        int in = open("/dev/null", O_RDONLY | O_CLOEXEC);
        int log = open("/var/log/aegis-package-test.log", O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (in < 0 || log < 0 || dup2(in, 0) < 0 || dup2(log, 1) < 0 || dup2(log, 2) < 0) _exit(126);
        // Slots 0/1/2 began closed: open may already return its final slot.
        // dup2(fd, fd) does NOT clear CLOEXEC, unlike a real duplication.
        for (int fd = 0; fd < 3; fd++) if (fcntl(fd, F_SETFD, 0) < 0) _exit(126);
        if (syscall(SYS_close_range, 3u, ~0u, 0u) < 0) _exit(126);
        char *args[] = {"/bin/sh", "-c", (char *)script, NULL};
        char *env[] = {"PATH=/usr/sbin:/usr/bin:/sbin:/bin", "LANG=C", "HOME=/root", NULL};
        execve(args[0], args, env);_exit(127);
    }
    int status;
    if (waitpid(child, &status, 0) != child) goto done;
    result.phase = 5;
    result.status = WIFEXITED(status) ? (uint32_t)WEXITSTATUS(status) : 256u + (uint32_t)WTERMSIG(status);
    if (!result.status) { result.phase = 6;errno = 0; }
done:
    result.error = result.phase >= 5 ? 0 : (uint32_t)errno;
    for (int i = 0; i < 2; i++) if (fds[i] >= 0) close(fds[i]);
    return send(3, &result, sizeof(result), MSG_NOSIGNAL) == (ssize_t)sizeof(result) && result.phase == 6 ? 0 : 115;
}
