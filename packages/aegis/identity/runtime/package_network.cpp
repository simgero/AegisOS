#include "package_network.h"
#include "child_private.h"
#include <android-base/unique_fd.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/memfd.h>
#include <linux/nsfs.h>
#include <linux/sched.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
using android::base::unique_fd;
namespace aegis {
namespace {
int Fail(int e) { errno=e;return -1; }
[[noreturn]] void Dead() { syscall(SYS_exit_group,120);__builtin_unreachable(); }
[[noreturn]] void Exec(const int* fds) {
    for(unsigned i=0;i<5;++i)if(syscall(SYS_dup3,fds[i],3+i,i==4?O_CLOEXEC:0)<0)Dead();
    if(syscall(SYS_close_range,0u,2u,0u)<0||syscall(SYS_close_range,8u,~0u,0u)<0)Dead();
    struct { uint64_t handler,flags,restorer,mask; } action={};uint64_t empty=0;
    for(int n=1;n<=64;++n)if(n!=SIGKILL&&n!=SIGSTOP)
        if(syscall(SYS_rt_sigaction,n,&action,nullptr,sizeof(empty))<0)Dead();
    if(syscall(SYS_rt_sigprocmask,SIG_SETMASK,&empty,nullptr,sizeof(empty))<0
       ||syscall(SYS_prctl,PR_SET_PDEATHSIG,SIGKILL,0,0,0)<0
       ||syscall(SYS_prctl,PR_SET_DUMPABLE,0,0,0,0)<0||syscall(SYS_chdir,"/")<0)Dead();
    pollfd alive={6,POLLIN,0};timespec zero={};
    if(syscall(SYS_ppoll,&alive,1u,&zero,nullptr,sizeof(empty))!=0)Dead();
    rlimit core={0,0};if(syscall(SYS_prlimit64,0,RLIMIT_CORE,&core,nullptr)<0)Dead();
    int oom=syscall(SYS_openat,AT_FDCWD,"/proc/self/oom_score_adj",O_WRONLY|O_CLOEXEC,0);
    if(oom<0||syscall(SYS_write,oom,"0",1)!=1||syscall(SYS_close,oom)<0)Dead();
    char label[]="aegis-package-network",locale[]="LANG=C";
    char* argv[]={label,nullptr};char* env[]={locale,nullptr};
    syscall(SYS_execveat,7,"",argv,env,AT_EMPTY_PATH);Dead();
}
}
int PackageNetworkStart(int netns,int program,aegis_memory_group* group,uint32_t user,uint32_t serial,
                        uint64_t job,int timeout,aegis_child** output) {
    if(!output||*output||user<10||user>=21473||serial>INT32_MAX||!job||job>INT64_MAX||timeout<1||timeout>10000)return Fail(EINVAL);
    struct stat st;Elf64_Ehdr hdr={};int flags=fcntl(program,F_GETFL);
    if(flags<0||fstat(program,&st)<0)return -1;
    if((flags&(O_ACCMODE|O_PATH))!=O_RDONLY||!S_ISREG(st.st_mode)||st.st_uid
       ||(st.st_gid!=0&&st.st_gid!=2000)||(st.st_mode&07022)||(st.st_mode&0555)!=0555)return Fail(EPERM);
    if(pread(program,&hdr,sizeof(hdr),0)!=static_cast<ssize_t>(sizeof(hdr))||memcmp(hdr.e_ident,ELFMAG,SELFMAG)
       ||hdr.e_ident[EI_CLASS]!=ELFCLASS64||hdr.e_ident[EI_DATA]!=ELFDATA2LSB||hdr.e_machine!=EM_AARCH64
       ||(hdr.e_type!=ET_EXEC&&hdr.e_type!=ET_DYN))return Fail(ENOEXEC);
    if(fgetxattr(program,"security.capability",nullptr,0)>=0||(errno!=ENODATA&&errno!=EOPNOTSUPP))return Fail(EPERM);
    if(ioctl(netns,NS_GET_NSTYPE)!=CLONE_NEWNET)return Fail(EPERM);
    unique_fd config(syscall(SYS_memfd_create,"aegis-package-network",MFD_CLOEXEC|MFD_ALLOW_SEALING));
    network::Request request={network::kMagic,user,serial,0,job};
    if(!config.ok()||write(config.get(),&request,sizeof(request))!=static_cast<ssize_t>(sizeof(request))
       ||fcntl(config.get(),F_ADD_SEALS,F_SEAL_WRITE|F_SEAL_GROW|F_SEAL_SHRINK|F_SEAL_SEAL)<0)return -1;
    int pair[2];if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair)<0)return -1;
    unique_fd channel(pair[0]),endpoint(pair[1]),parent(syscall(SYS_pidfd_open,syscall(SYS_getpid),0u));
    if(!parent.ok())return -1;
    int inputs[]={netns,config.get(),endpoint.get(),parent.get(),program},fixed[5];unique_fd owned[5];
    for(unsigned i=0;i<5;++i) { owned[i].reset(fcntl(inputs[i],F_DUPFD_CLOEXEC,128));if(!owned[i].ok())return -1;fixed[i]=owned[i].get(); }
    auto* child=static_cast<aegis_child*>(calloc(1,sizeof(aegis_child)));if(!child)return -1;
    child->owner=syscall(SYS_getpid);child->pidfd=-1;
    int target=aegis_memory_group_claim_companion(group,user,serial);if(target<0) { free(child);return -1; }
    clone_args args={};args.flags=CLONE_PIDFD|CLONE_INTO_CGROUP;args.pidfd=reinterpret_cast<uintptr_t>(&child->pidfd);
    args.cgroup=target;args.exit_signal=SIGCHLD;
    pid_t pid=syscall(SYS_clone3,&args,sizeof(args));if(pid==0)Exec(fixed);
    if(pid<0) { free(child);return -1; }*output=child;endpoint.reset();
    // The dup still held by this parent must close before waiting for peer EOF.
    owned[2].reset();pollfd ready={channel.get(),POLLIN,0};int p=poll(&ready,1,timeout);
    if(p<0)return -1;if(!p)return Fail(ETIMEDOUT);
    network::Ready response={};
    if(recv(channel.get(),&response,sizeof(response),MSG_DONTWAIT|MSG_TRUNC)!=static_cast<ssize_t>(sizeof(response))
       ||response.magic!=network::kMagic||response.error>4095)return Fail(EPROTO);
    return response.error?Fail(response.error):0;
}
}
