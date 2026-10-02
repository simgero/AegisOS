#include "package_preparer.h"
#include "package_preparation_protocol.h"
#include "child_private.h"
#include "memory_group.h"
#include "namespace.h"
#include <android-base/unique_fd.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/memfd.h>
#include <linux/magic.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
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
#include <array>

using android::base::unique_fd;
namespace aegis {
namespace wire = preparation;
struct PackagePreparer {
    pid_t process = 0;
    unique_fd store, source, channel;
    std::vector<unique_fd> archives;
    bool cancelled=false,selection=false,reconciliation=false,has_shared=false,has_personal=false;
    bool private_removal=false;
    PackageInput factory;
    uint32_t user=0,serial=0;
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
bool Owned(PackagePreparer* p) {
    if (!p || p->process!=syscall(SYS_getpid)) { Fail(EPERM);return false; }
    return aegis_namespace_check_broker()==0;
}
bool Encode(const PackagePreparation& value,wire::Request* out) {
    *out={};out->magic=wire::kMagic;out->version=wire::kVersion;
    const auto& p=value.execution;
    if(PackageExecutionCheck(p)<0 || value.image.sha256.size()!=64)return false;
    auto& e=out->execution;e.magic=AEGIS_PACKAGE_EXEC_MAGIC;e.version=AEGIS_PACKAGE_EXEC_VERSION;
    e.user=p.requester;e.serial=p.serial;e.job=p.job;
    e.kind=p.kind;e.count=p.items.size();
    e.review=p.review;
    memcpy(e.plan,p.plan_sha256.c_str(),65);
    for(size_t i=0;i<p.items.size();++i)memcpy(e.items[i],p.items[i].c_str(),p.items[i].size()+1);
    out->image.bytes=value.image.bytes;memcpy(out->image.hash,value.image.sha256.c_str(),65);
    if(value.archives.size()!=aegis_package_archive_count(&e))return false;
    size_t archive=0;
    for(size_t i=0;i<p.items.size();++i)if(aegis_package_has_archive(&e,i)) {
        const auto& input=value.archives[archive++];
        if(input.sha256.size()!=64)return false;
        out->archives[i].bytes=input.bytes;
        memcpy(out->archives[i].hash,input.sha256.c_str(),65);
    }
    return wire::Valid(*out);
}
bool File(int fd,mode_t type,bool executable) {
    struct stat st={};int flags=fcntl(fd,F_GETFL);
    if(flags<0 || fstat(fd,&st)<0)return false;
    // AOSP installs trusted system executables as root:shell (2000). The
    // caller still pins helper provenance; store/source must stay root:root.
    bool trusted_group=st.st_gid==0 || (executable && st.st_gid==2000);
    if((st.st_mode&S_IFMT)!=type || st.st_uid || !trusted_group
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
[[noreturn]] void Exec(const int* inputs,size_t count,int parent) {
    for(size_t i=0;i<count;++i)
        if(syscall(SYS_dup3,inputs[i],3+i,i==4?O_CLOEXEC:0)<0)Failed();
    if(syscall(SYS_close_range,0u,2u,0u)<0
       || syscall(SYS_close_range,static_cast<unsigned>(3+count),UINT_MAX,0u)<0)Failed();
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
    int oom=syscall(SYS_openat,AT_FDCWD,"/proc/self/oom_score_adj",O_WRONLY|O_CLOEXEC,0);
    if(oom<0 || syscall(SYS_write,oom,"0",1)!=1 || syscall(SYS_close,oom)<0)Failed();
    char label[]="aegis-package-prepare",locale[]="LANG=C";
    char* args[]={label,nullptr};char* env[]={locale,nullptr};
    syscall(SYS_execveat,wire::kExecutable,"",args,env,AT_EMPTY_PATH);Failed();
}
PackagePreparationResult Response(PackagePreparer* p,const aegis_child_exit& exited,int candidates[3]) {
    PackagePreparationResult result{PackagePreparationOutcome::Unconfirmed,EIO};
    if(exited.code!=CLD_EXITED || exited.status!=0)return result;
    wire::Reply reply={};
    alignas(cmsghdr) char ancillary[CMSG_SPACE(16*sizeof(int))]={};
    iovec vector{&reply,sizeof(reply)};msghdr message={};message.msg_iov=&vector;message.msg_iovlen=1;
    message.msg_control=ancillary;message.msg_controllen=sizeof(ancillary);
    ssize_t n=recvmsg(p->channel.get(),&message,MSG_CMSG_CLOEXEC|MSG_DONTWAIT|MSG_TRUNC);
    if(n<0)return result;
    std::array<unique_fd,3> mounts;unsigned count=0;bool bad=false;
    for(cmsghdr* c=CMSG_FIRSTHDR(&message);c;c=CMSG_NXTHDR(&message,c)) {
        auto* end=reinterpret_cast<unsigned char*>(message.msg_control)+message.msg_controllen;
        if(c->cmsg_len<CMSG_LEN(0)
           || c->cmsg_len>static_cast<size_t>(end-reinterpret_cast<unsigned char*>(c))) { bad=true;break; }
        if(c->cmsg_level!=SOL_SOCKET || c->cmsg_type!=SCM_RIGHTS) { bad=true;continue; }
        size_t bytes=c->cmsg_len-CMSG_LEN(0);if(bytes%sizeof(int))bad=true;
        for(size_t i=0;i<bytes/sizeof(int);++i) {
            int fd;memcpy(&fd,CMSG_DATA(c)+i*sizeof(fd),sizeof(fd));
            if(count<mounts.size())mounts[count].reset(fd);else close(fd);++count;
        }
    }
    if(n!=static_cast<ssize_t>(sizeof(reply)) || bad || message.msg_flags&(MSG_TRUNC|MSG_CTRUNC)
       || reply.magic!=wire::kMagic || reply.version!=wire::kVersion
       || reply.user!=p->user || reply.serial!=p->serial || reply.job!=p->job
       || !aegis_package_hash(reply.plan) || p->plan!=reply.plan || reply.error<0 || reply.error>4095
       || count!=(reply.error ? 0u : p->reconciliation ? 3u : 1u))return result;
    if(!p->reconciliation || reply.error) {
        if(reply.previous_shared.bytes || !aegis_package_zero(reply.previous_shared.hash,65))return result;
    }
    if(reply.error) {
        if(reply.scope || reply.selected.bytes || !aegis_package_zero(reply.selected.hash,65)
           || !aegis_package_zero(reply.shared_base,65) || reply.shared.bytes || !aegis_package_zero(reply.shared.hash,65))return result;
        return {PackagePreparationOutcome::Failed,reply.error};
    }
    PackagePreparationResult selected{PackagePreparationOutcome::Prepared,0};
    if(p->selection) {
        if(reply.scope<1 || reply.scope>3 || (reply.scope==2 && !p->has_shared)
           || (reply.scope==3 && !p->has_personal) || !wire::InputValid(reply.selected,uint64_t{32}<<30)
           || reply.selected.bytes%4096)return result;
        if(reply.scope==3 ? !aegis_package_hash(reply.shared_base) : !aegis_package_zero(reply.shared_base,65))return result;
        if(reply.scope==1 && (reply.selected.bytes!=p->factory.bytes || reply.selected.hash!=p->factory.sha256))return result;
        if(!wire::InputValid(reply.shared,uint64_t{32}<<30) || reply.shared.bytes%4096
           || (reply.scope==3 ? (p->reconciliation && !p->private_removal ? strcmp(reply.shared.hash,reply.shared_base)==0
                                                  : strcmp(reply.shared.hash,reply.shared_base)!=0)
                             : reply.shared.bytes!=reply.selected.bytes||strcmp(reply.shared.hash,reply.selected.hash)!=0))return result;
        if(!p->has_shared && (reply.shared.hash!=p->factory.sha256 || reply.shared.bytes!=p->factory.bytes))return result;
        if(p->reconciliation) {
            if(reply.scope!=3 || !wire::InputValid(reply.previous_shared,uint64_t{32}<<30)
               || reply.previous_shared.bytes%4096 || strcmp(reply.previous_shared.hash,reply.shared_base)!=0)return result;
            if(p->private_removal && reply.previous_shared.bytes!=reply.shared.bytes)return result;
            if(reply.previous_shared.hash==p->factory.sha256 && reply.previous_shared.bytes!=p->factory.bytes)return result;
            selected.previous_shared={reply.previous_shared.bytes,reply.previous_shared.hash};
        }
        selected.shared={reply.shared.bytes,reply.shared.hash};
        selected.generation={reply.selected.hash,reply.shared_base,reply.selected.bytes};
        selected.scope=static_cast<PackagePreparationResult::Scope>(reply.scope);
    } else if(reply.scope || reply.selected.bytes || !aegis_package_zero(reply.selected.hash,65)
              || !aegis_package_zero(reply.shared_base,65) || reply.shared.bytes || !aegis_package_zero(reply.shared.hash,65))return result;
    for(unsigned i=0;i<count;++i) {
        const auto& mount=mounts[i];struct stat st;struct statfs fs;struct statvfs flags;
        if(fcntl(mount.get(),F_GETFL)<0 || !(fcntl(mount.get(),F_GETFL)&O_PATH)
           || fstat(mount.get(),&st)<0 || fstatfs(mount.get(),&fs)<0 || fstatvfs(mount.get(),&flags)<0
           || st.st_mode!=(S_IFDIR|0755) || st.st_uid || st.st_gid || fs.f_type!=EXT4_SUPER_MAGIC
           || (flags.f_flag&(ST_RDONLY|ST_NOSUID|ST_NODEV|ST_NOEXEC))
                !=static_cast<unsigned long>(ST_NOSUID|ST_NODEV|ST_NOEXEC|(p->selection?ST_RDONLY:0)))return result;
    }
    for(unsigned i=0;i<count;++i)candidates[i]=mounts[i].release();
    return selected;
}
} // namespace

int PackagePreparationCheck(const PackagePreparation& request) {
    wire::Request message={};return Encode(request,&message) ? 0 : Fail(EINVAL);
}
int PackagePreparerCancel(PackagePreparer* p) {
    if(!Owned(p))return -1;
    p->cancelled=true;
    int error=0;
    if(p->group && aegis_memory_group_kill_and_wait(p->group,0)<0 && errno!=ETIMEDOUT)
        error=errno;
    if(p->child && aegis_child_request_stop(p->child)<0 && !error)error=errno;
    return error ? Fail(error) : 0;
}

static int Start(int groups,int store,int source,int helper,const std::vector<int>& archives,
                 const wire::Request& message,PackagePreparer** output) {
    if(!output || *output)return Fail(EINVAL);
    if(aegis_namespace_check_broker()<0)return -1;
    if(!wire::Valid(message))return Fail(EINVAL);
    bool shared=message.selection && message.has_shared;
    bool personal=message.selection && message.has_personal;
    if(archives.size()!=(message.selection ? size_t(personal)
        : aegis_package_archive_count(&message.execution)))return Fail(EINVAL);
    if(!File(store,(!message.selection||shared)?S_IFDIR:S_IFREG,false)
       || !File(source,S_IFREG,false) || !File(helper,S_IFREG,true))return -1;
    struct stat st;if(fstat(store,&st)<0)return -1;
    if((!message.selection||shared) && st.st_mode!=(S_IFDIR|0700))return Fail(EPERM);
    for(int archive:archives) {
        if(!File(archive,personal?S_IFDIR:S_IFREG,false))return -1;
        if(personal && (fstat(archive,&st)<0 || st.st_mode!=(S_IFDIR|0700)))return Fail(EPERM);
    }
    auto* p=new(std::nothrow) PackagePreparer;
    if(!p)return Fail(ENOMEM);
    p->process=syscall(SYS_getpid);p->job=message.execution.job;p->plan=message.execution.plan;
    p->user=message.execution.user;p->serial=message.execution.serial;
    p->selection=message.selection;p->reconciliation=message.selection>=2;p->private_removal=message.selection==3;
    p->has_shared=shared;p->has_personal=personal;
    p->factory={message.image.bytes,message.image.hash};
    *output=p; // From here onward the caller retains partial ownership on failure.
    p->store.reset(fcntl(store,F_DUPFD_CLOEXEC,128));
    p->source.reset(fcntl(source,F_DUPFD_CLOEXEC,128));
    unique_fd executable(fcntl(helper,F_DUPFD_CLOEXEC,128));
    unique_fd config(syscall(SYS_memfd_create,"aegis-package-preparation",MFD_CLOEXEC|MFD_ALLOW_SEALING));
    if(!p->store.ok() || !p->source.ok() || !executable.ok() || !config.ok())return -1;
    for(int archive:archives) {
        unique_fd owned(fcntl(archive,F_DUPFD_CLOEXEC,128));if(!owned.ok())return -1;
        p->archives.push_back(std::move(owned));
    }
    if(write(config.get(),&message,sizeof(message))!=static_cast<ssize_t>(sizeof(message)))return Fail(EIO);
    if(fcntl(config.get(),F_ADD_SEALS,F_SEAL_WRITE|F_SEAL_GROW|F_SEAL_SHRINK|F_SEAL_SEAL)<0)return -1;
    unique_fd input(fcntl(config.get(),F_DUPFD_CLOEXEC,128));if(!input.ok())return -1;
    int pair[2];if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair)<0)return -1;
    p->channel.reset(pair[0]);unique_fd child_socket(pair[1]);
    unique_fd endpoint(fcntl(child_socket.get(),F_DUPFD_CLOEXEC,128));if(!endpoint.ok())return -1;
    if(aegis_memory_group_create_package(groups,p->user,p->serial,&p->group)<0)return -1;
    int target=aegis_memory_group_claim(p->group,p->user,p->serial);if(target<0)return -1;
    p->child=static_cast<aegis_child*>(calloc(1,sizeof(aegis_child)));
    if(!p->child)return -1;
    p->child->owner=p->process;p->child->pidfd=-1;
    std::vector<int> inputs={p->store.get(),p->source.get(),endpoint.get(),input.get(),executable.get()};
    for(const auto& archive:p->archives)inputs.push_back(archive.get());
    // Capture raw data before clone; no C++/libc work in the child.
    const int* descriptors=inputs.data();size_t count=inputs.size();
    clone_args clone={};clone.flags=CLONE_PIDFD|CLONE_INTO_CGROUP;
    clone.pidfd=reinterpret_cast<uintptr_t>(&p->child->pidfd);clone.cgroup=target;clone.exit_signal=SIGCHLD;
    pid_t child=syscall(SYS_clone3,&clone,sizeof(clone));
    if(child==0)Exec(descriptors,count,p->process);
    if(child<0) { free(p->child);p->child=nullptr;return -1; }
    p->spawned=true;return 0;
}

int PackagePreparerStart(int groups,int stage,int source,int helper,
                         const std::vector<int>& archives,const PackagePreparation& request,PackagePreparer** output) {
    wire::Request message={};if(!Encode(request,&message))return Fail(EINVAL);
    return Start(groups,stage,source,helper,archives,message,output);
}
int PackageRuntimeSelectionCheck(const PackageRuntimeSelection& request) {
    const auto& h=request.factory.sha256;
    return request.requester>=10 && request.requester<21473 && request.serial<=INT32_MAX
        && request.job && request.job<=INT64_MAX && request.factory.bytes
        && request.factory.bytes<=(uint64_t{32}<<30) && request.factory.bytes%4096==0
        && h.size()==64 && h.find_first_not_of("0123456789abcdef")==std::string::npos ? 0 : Fail(EINVAL);
}
static int SelectionStart(int groups,int shared,int personal,int factory,int helper,
                                 const PackageRuntimeSelection& request,PackagePreparer** output,bool reconciliation,
                                 bool private_removal=false) {
    if(shared < -1 || personal < -1 || PackageRuntimeSelectionCheck(request)<0)return Fail(EINVAL);
    wire::Request message={};message.magic=wire::kMagic;message.version=wire::kVersion;
    message.selection=private_removal?3:reconciliation?2:1;message.has_shared=shared>=0;message.has_personal=personal>=0;
    auto& e=message.execution;e.magic=AEGIS_PACKAGE_EXEC_MAGIC;e.version=AEGIS_PACKAGE_EXEC_VERSION;
    e.user=request.requester;e.serial=request.serial;e.job=request.job;
    memcpy(e.plan,request.factory.sha256.c_str(),65);
    message.image.bytes=request.factory.bytes;memcpy(message.image.hash,e.plan,65);
    std::vector<int> stores;if(personal>=0)stores.push_back(personal);
    return Start(groups,shared>=0?shared:factory,factory,helper,stores,message,output);
}

int PackageRuntimeSelectionStart(int groups,int shared,int personal,int factory,int helper,
    const PackageRuntimeSelection& request,PackagePreparer** output) {
    return SelectionStart(groups,shared,personal,factory,helper,request,output,false);
}
int PackageReconciliationSelectionStart(int groups,int shared,int personal,int factory,int helper,
    const PackageRuntimeSelection& request,PackagePreparer** output) {
    if(personal<0)return Fail(EINVAL);
    return SelectionStart(groups,shared,personal,factory,helper,request,output,true);
}
int PackagePrivateRemovalSelectionStart(int groups,int shared,int personal,int factory,int helper,
    const PackageRuntimeSelection& request,PackagePreparer** output) {
    if(personal<0)return Fail(EINVAL);
    return SelectionStart(groups,shared,personal,factory,helper,request,output,true,true);
}

static int Finish(PackagePreparer** pointer,bool cancel,int timeout_ms,
                   PackagePreparationResult* result,int* candidates,bool reconciliation,bool private_removal=false) {
    if(!pointer || !*pointer || !result || !candidates || timeout_ms<0 || timeout_ms>10000)return Fail(EINVAL);
    const unsigned count=reconciliation?3:1;
    for(unsigned i=0;i<count;++i)if(candidates[i]!=-1)return Fail(EINVAL);
    auto* p=*pointer;if(!Owned(p))return -1;
    if(p->reconciliation!=reconciliation||p->private_removal!=private_removal)return Fail(EINVAL);
    int64_t start=Now();
    // A signal failure does not bypass the completion checks or abandon work.
    if(cancel)(void)PackagePreparerCancel(p);
    if(start<0)return -1;
    int64_t deadline=start+timeout_ms;
    aegis_child_exit exit={};
    int left=Left(deadline);if(left<0)return -1;
    if(p->child && aegis_child_wait(p->child,left,&exit)<0)return -1;
    left=Left(deadline);if(left<0)return -1;
    if(p->group && aegis_memory_group_kill_and_wait(p->group,left)<0)return -1;
    if(p->group && aegis_memory_group_remove(&p->group)<0)return -1;
    PackagePreparationResult observed{PackagePreparationOutcome::Failed,ECANCELED};
    int mounts[3]={-1,-1,-1};
    if(p->spawned)observed=Response(p,exit,mounts);
    std::array<unique_fd,3> prepared;
    for(unsigned i=0;i<3;++i)prepared[i].reset(mounts[i]);
    if(p->cancelled) { for(auto& fd:prepared)fd.reset();observed={PackagePreparationOutcome::Failed,ECANCELED}; }
    for(unsigned i=0;i<count;++i)candidates[i]=prepared[i].release();
    if(p->child)aegis_child_release(p->child);
    delete p;*pointer=nullptr; // Mount only transfers to the existing lifecycle owner on Prepared.
    *result=observed;return 0;
}
int PackagePreparerFinish(PackagePreparer** pointer,bool cancel,int timeout_ms,
    PackagePreparationResult* result,int* candidate) {
    return Finish(pointer,cancel,timeout_ms,result,candidate,false);
}
int PackageReconciliationSelectionFinish(PackagePreparer** pointer,bool cancel,int timeout_ms,
    PackagePreparationResult* result,int candidates[3]) {
    return Finish(pointer,cancel,timeout_ms,result,candidates,true);
}
int PackagePrivateRemovalSelectionFinish(PackagePreparer** pointer,bool cancel,int timeout_ms,
    PackagePreparationResult* result,int candidates[3]) {
    return Finish(pointer,cancel,timeout_ms,result,candidates,true,true);
}
} // namespace aegis
