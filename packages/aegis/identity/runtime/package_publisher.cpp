#include "package_publisher.h"
#include "package_publish_protocol.h"
#include "child_private.h"
#include "memory_group.h"
#include "namespace.h"
#include <android-base/unique_fd.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/memfd.h>
#include <linux/sched.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/xattr.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <cstdlib>
#include <new>

using android::base::unique_fd;
namespace aegis {
namespace wire = publication;
struct PackagePublisher {
    pid_t process = 0;
    unique_fd store, source, channel;
    aegis_memory_group* group = nullptr;
    aegis_child* child = nullptr;
    uint64_t job = 0;
    std::string plan;
    bool spawned = false;
};
namespace {
int Fail(int error) { errno=error;return -1; }
int64_t Now() {
    timespec now={}; if(clock_gettime(CLOCK_MONOTONIC,&now)<0)return -1;
    return int64_t{now.tv_sec}*1000+now.tv_nsec/1000000;
}
int Left(int64_t deadline) {
    int64_t now=Now();return now<0 ? -1 : now>=deadline ? 0 : int(deadline-now);
}
bool Owned(PackagePublisher* p) {
    if (!p || p->process!=syscall(SYS_getpid)) { Fail(EPERM);return false; }
    return aegis_namespace_check_broker()==0;
}
bool String(const std::string& value,char (&out)[65]) {
    if(value.size()>64)return false;
    memcpy(out,value.c_str(),value.size()+1);return true;
}
bool Image(const PackageGeneration& value,wire::Image* out) {
    out->bytes=value.bytes;return String(value.image_sha256,out->sha256)
        && String(value.shared_base_sha256,out->base);
}
bool Encode(const PackagePublication& value,wire::Request* out) {
    *out={};out->magic=wire::kMagic;out->version=wire::kVersion;
    out->user=value.requester;out->serial=value.serial;out->job=value.job;
    out->flags=(value.personal?wire::kPersonal:0)|(value.create?wire::kCreate:0)
              |(value.has_previous?wire::kPrevious:0);
    return String(value.plan_sha256,out->plan) && Image(value.candidate,&out->candidate)
        && (value.has_previous ? Image(value.previous,&out->previous)
             : value.previous.image_sha256.empty() && value.previous.shared_base_sha256.empty()
               && value.previous.bytes==0) && wire::Valid(*out);
}
bool File(int fd,mode_t type,bool executable) {
    struct stat st={};int flags=fcntl(fd,F_GETFL);
    if(flags<0 || fstat(fd,&st)<0)return false;
    if((st.st_mode&S_IFMT)!=type || st.st_uid || st.st_gid
       || (st.st_mode&07022) || (flags&(O_ACCMODE|O_PATH))!=O_RDONLY
       || (executable && (st.st_mode&0555)!=0555)) { Fail(EPERM);return false; }
    if(executable) {
        Elf64_Ehdr header={};
        if(pread(fd,&header,sizeof(header),0)!=static_cast<ssize_t>(sizeof(header))
           || memcmp(header.e_ident,ELFMAG,SELFMAG)
           || header.e_ident[EI_CLASS]!=ELFCLASS64 || header.e_ident[EI_DATA]!=ELFDATA2LSB
           || header.e_machine!=EM_AARCH64
           || (header.e_type!=ET_EXEC&&header.e_type!=ET_DYN)) { Fail(ENOEXEC);return false; }
    }
    if(executable) {
        if(fgetxattr(fd,"security.capability",nullptr,0)>=0
           || (errno!=ENODATA && errno!=EOPNOTSUPP)) { Fail(EPERM);return false; }
    }
    return true;
}
[[noreturn]] void Failed() { syscall(SYS_exit_group,122);__builtin_unreachable(); }
// Only raw syscalls between clone3 and exec: Bionic's cached PID/thread state
// still belongs to the parent until exec. No C++/libc allocation in this path.
[[noreturn]] void Exec(const int inputs[5],int parent) {
    for(int i=0;i<5;++i)
        if(syscall(SYS_dup3,inputs[i],3+i,i==4?O_CLOEXEC:0)<0)Failed();
    if(syscall(SYS_close_range,0u,2u,0u)<0
       || syscall(SYS_close_range,8u,UINT_MAX,0u)<0)Failed();
    struct { uint64_t handler,flags,restorer,mask; } action={};uint64_t empty=0;
    for(int number=1;number<=64;++number) {
        if(number==SIGKILL||number==SIGSTOP)continue;
        if(syscall(SYS_rt_sigaction,number,&action,nullptr,sizeof(empty))<0)Failed();
    }
    if(syscall(SYS_rt_sigprocmask,SIG_SETMASK,&empty,nullptr,sizeof(empty))<0
       || syscall(SYS_prctl,PR_SET_PDEATHSIG,SIGKILL,0,0,0)<0
       || syscall(SYS_getppid)!=parent
       || syscall(SYS_prctl,PR_SET_DUMPABLE,0,0,0,0)<0
       || syscall(SYS_chdir,"/")<0)Failed();
    struct rlimit core={0,0};
    if(syscall(SYS_prlimit64,0,RLIMIT_CORE,&core,nullptr)<0)Failed();
    char label[]="aegis-package-publish",locale[]="LANG=C";
    char* args[]={label,nullptr};char* env[]={locale,nullptr};
    syscall(SYS_execveat,wire::kExecutable,"",args,env,AT_EMPTY_PATH);Failed();
}
PackagePublicationResult Response(PackagePublisher* p,const aegis_child_exit& exit) {
    PackagePublicationResult result{PackagePublish::Unconfirmed,EIO};
    if(exit.code!=CLD_EXITED || exit.status!=0)return result;
    wire::Reply reply={};
    alignas(cmsghdr) char ancillary[CMSG_SPACE(16*sizeof(int))]={};
    iovec vector{&reply,sizeof(reply)};
    msghdr message={};message.msg_iov=&vector;message.msg_iovlen=1;
    message.msg_control=ancillary;message.msg_controllen=sizeof(ancillary);
    ssize_t n=recvmsg(p->channel.get(),&message,MSG_CMSG_CLOEXEC|MSG_DONTWAIT|MSG_TRUNC);
    bool control=false;
    for(cmsghdr* c=CMSG_FIRSTHDR(&message);c;c=CMSG_NXTHDR(&message,c)) {
        control=true;
        if(c->cmsg_level==SOL_SOCKET && c->cmsg_type==SCM_RIGHTS && c->cmsg_len>=CMSG_LEN(0)) {
            size_t count=(c->cmsg_len-CMSG_LEN(0))/sizeof(int);
            int* fds=reinterpret_cast<int*>(CMSG_DATA(c));
            for(size_t i=0;i<count;++i)close(fds[i]);
        }
    }
    if(n!=static_cast<ssize_t>(sizeof(reply)) || control || message.msg_flags&(MSG_TRUNC|MSG_CTRUNC)
       || reply.job!=p->job || !wire::Hash(reply.plan) || p->plan!=reply.plan
       || reply.result<-1 || reply.result>1 || reply.error<0 || reply.error>4095
       || (reply.result==0 && reply.error))return result;
    result.publication=static_cast<PackagePublish>(reply.result);result.error=reply.error;
    return result;
}
} // namespace

int PackagePublicationCheck(const PackagePublication& request) {
    wire::Request message={};return Encode(request,&message) ? 0 : Fail(EINVAL);
}
int PackagePublisherCancel(PackagePublisher* p) {
    if(!Owned(p))return -1;
    int error=0;
    if(p->group && aegis_memory_group_kill_and_wait(p->group,0)<0 && errno!=ETIMEDOUT)
        error=errno;
    if(p->child && aegis_child_request_stop(p->child)<0 && !error)error=errno;
    return error ? Fail(error) : 0;
}

int PackagePublisherStart(int groups,int store,int source,int helper,
                          const PackagePublication& request,PackagePublisher** output) {
    if(!output || *output)return Fail(EINVAL);
    if(aegis_namespace_check_broker()<0)return -1;
    wire::Request message={};
    if(!Encode(request,&message))return Fail(EINVAL);
    if(!File(store,S_IFDIR,false) || !File(source,S_IFREG,false) || !File(helper,S_IFREG,true))return -1;
    auto* p=new(std::nothrow) PackagePublisher;
    if(!p)return Fail(ENOMEM);
    p->process=syscall(SYS_getpid);p->job=request.job;p->plan=request.plan_sha256;
    *output=p; // From here onward the caller retains partial ownership on failure.
    p->store.reset(fcntl(store,F_DUPFD_CLOEXEC,10));
    p->source.reset(fcntl(source,F_DUPFD_CLOEXEC,10));
    unique_fd executable(fcntl(helper,F_DUPFD_CLOEXEC,10));
    unique_fd config(syscall(SYS_memfd_create,"aegis-package-publication",MFD_CLOEXEC|MFD_ALLOW_SEALING));
    if(!p->store.ok() || !p->source.ok() || !executable.ok() || !config.ok())return -1;
    if(write(config.get(),&message,sizeof(message))!=static_cast<ssize_t>(sizeof(message)))return Fail(EIO);
    if(fcntl(config.get(),F_ADD_SEALS,F_SEAL_WRITE|F_SEAL_GROW|F_SEAL_SHRINK|F_SEAL_SEAL)<0)return -1;
    unique_fd input(fcntl(config.get(),F_DUPFD_CLOEXEC,10));if(!input.ok())return -1;
    int pair[2];if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair)<0)return -1;
    p->channel.reset(pair[0]);unique_fd child_socket(pair[1]);
    unique_fd endpoint(fcntl(child_socket.get(),F_DUPFD_CLOEXEC,10));if(!endpoint.ok())return -1;
    if(aegis_memory_group_create(groups,request.requester,request.serial,&p->group)<0)return -1;
    int target=aegis_memory_group_claim(p->group,request.requester,request.serial);if(target<0)return -1;
    p->child=static_cast<aegis_child*>(calloc(1,sizeof(aegis_child)));
    if(!p->child)return -1;
    p->child->owner=p->process;p->child->pidfd=-1;
    int inputs[]={p->store.get(),p->source.get(),endpoint.get(),input.get(),executable.get()};
    clone_args clone={};clone.flags=CLONE_PIDFD|CLONE_INTO_CGROUP;
    clone.pidfd=reinterpret_cast<uintptr_t>(&p->child->pidfd);clone.cgroup=target;clone.exit_signal=SIGCHLD;
    pid_t child=syscall(SYS_clone3,&clone,sizeof(clone));
    if(child==0)Exec(inputs,p->process);
    if(child<0) { free(p->child);p->child=nullptr;return -1; }
    p->spawned=true;return 0;
}

int PackagePublisherFinish(PackagePublisher** pointer,bool cancel,int timeout_ms,
                           PackagePublicationResult* result) {
    if(!pointer || !*pointer || !result || timeout_ms<0 || timeout_ms>10000)return Fail(EINVAL);
    auto* p=*pointer;if(!Owned(p))return -1;
    int64_t start=Now();
    // A signal failure does not bypass the completion checks or abandon work.
    if(cancel)(void)PackagePublisherCancel(p);
    if(start<0)return -1;
    int64_t deadline=start+timeout_ms;
    aegis_child_exit exit={};
    int left=Left(deadline);if(left<0)return -1;
    if(p->child && aegis_child_wait(p->child,left,&exit)<0)return -1;
    left=Left(deadline);if(left<0)return -1;
    if(p->group && aegis_memory_group_kill_and_wait(p->group,left)<0)return -1;
    if(p->group && aegis_memory_group_remove(&p->group)<0)return -1;
    PackagePublicationResult observed{PackagePublish::Rejected,ECANCELED};
    if(p->spawned)observed=Response(p,exit);
    if(p->child)aegis_child_release(p->child);
    delete p;*pointer=nullptr; // All broker-side source/store/channel FDs closed.
    *result=observed;return 0;
}
} // namespace aegis
