#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "package_program.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <linux/securebits.h>
#include <stdint.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#define MAX_ARGS 1024u
#define MAX_BYTES 262144u
extern char **environ;
static int command(const char *name) {
    return name && (!strcmp(name,"/usr/bin/apt-get") || !strcmp(name,"/usr/bin/dpkg")
                   || !strcmp(name,"/usr/bin/apt-mark"));
}
static int context(void) {
    if(getpid()<=1 || getppid()!=1 || getsid(0)!=1 || getpgrp()!=1
       || getuid() || geteuid() || getgid() || getegid() || getgroups(0,NULL)!=0
       || prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0)!=1 || prctl(PR_GET_SECCOMP,0,0,0,0)!=2)
        return -1;
    const uint32_t allowed=(1u<<CAP_CHOWN)|(1u<<CAP_DAC_OVERRIDE)|(1u<<CAP_FOWNER)
        |(1u<<CAP_FSETID)|(1u<<CAP_SETUID)|(1u<<CAP_SETGID);
    struct __user_cap_header_struct header={.version=_LINUX_CAPABILITY_VERSION_3};
    struct __user_cap_data_struct caps[2]={{0},{0}};
    if(syscall(SYS_capget,&header,caps)<0 || caps[0].effective!=allowed
       || caps[0].permitted!=allowed || caps[0].inheritable!=allowed
       || caps[1].effective || caps[1].permitted || caps[1].inheritable)return -1;
    uid_t real,effective,saved;gid_t group_real,group_effective,group_saved;
    if(getresuid(&real,&effective,&saved)<0 || real || effective || saved
       || getresgid(&group_real,&group_effective,&group_saved)<0
       || group_real || group_effective || group_saved)return -1;
    const int securebits=SECBIT_NOROOT|SECBIT_NOROOT_LOCKED
        |SECBIT_NO_CAP_AMBIENT_RAISE|SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED;
    if(prctl(PR_GET_SECUREBITS,0,0,0,0)!=securebits)return -1;
    for(unsigned cap=0;cap<64;cap++) {
        int expected=cap<32 && (allowed&(1u<<cap));
        int bound=prctl(PR_CAPBSET_READ,cap,0,0,0);
        if(bound<0 && errno==EINVAL && !expected)continue;
        if(bound!=!!expected || prctl(PR_CAP_AMBIENT,PR_CAP_AMBIENT_IS_SET,cap,0,0)!=!!expected)
            return -1;
    }
#ifndef AEGIS_PACKAGE_PROGRAM_PROBE
    /* Probe binaries are separate, uninstalled test modules. Product code has
     * no switch or environment variable that can skip the SELinux boundary. */
    int fd=open("/proc/self/attr/current",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return -1;
    char label[128]={0};ssize_t count=read(fd,label,sizeof(label)-1);close(fd);
    if(count<=0 || count==(ssize_t)sizeof(label)-1)return -1;
    if(label[count-1]=='\n' || label[count-1]=='\0')label[count-1]=0;
    if(strcmp(label,"u:r:aegis_package_program:s0"))return -1;
#endif
    return 0;
}
int aegis_package_exec_program(char *const arguments[],char *const environment[]) {
    if(!arguments || !command(arguments[0])) { errno=EPERM;return -1; }
    char *next[MAX_ARGS+3];unsigned count=0;size_t bytes=0;
    next[0]=(char*)"aegis-package";next[1]=(char*)"--package-program";
    for(;arguments[count];++count) {
        if(count==MAX_ARGS) { errno=E2BIG;return -1; }
        size_t size=strnlen(arguments[count],MAX_BYTES);
        if(size==MAX_BYTES || size+1>MAX_BYTES-bytes) { errno=E2BIG;return -1; }
        bytes+=size+1;next[count+2]=arguments[count];
    }
    next[count+2]=NULL;
    /* /proc/self/exe still names the immutable system helper after pivot_root;
     * no candidate-controlled path or copied tmpfs hook is the entrypoint. */
    return execve("/proc/self/exe",next,environment);
}
int aegis_package_program_main(int argc,char **argv) {
    if(argc<1 || argc>(int)MAX_ARGS || !argv || !command(argv[0]) || context()<0)return 126;
    /* Exec closed every CLOEXEC reference. Assert that no control channel or
     * Android mount/source descriptor can reach APT even on future changes. */
    if(syscall(SYS_close_range,3u,~0u,0u)<0)return 126;
    execve(argv[0],argv,environ);return 127;
}
