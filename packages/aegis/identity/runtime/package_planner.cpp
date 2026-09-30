#include "package_planner.h"
#include "package_network.h"
#include "package_planning_protocol.h"
#include "namespace.h"
#include "memory_group.h"
#include <android-base/unique_fd.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <new>
using android::base::unique_fd;
namespace aegis {
struct PackagePlanner {
    pid_t process=0;
    unique_fd selected,root,metadata,devices,sources,key,ca,channel;
    aegis_namespace* context=nullptr;
    aegis_child* network=nullptr;
    aegis_memory_group* group=nullptr;
    aegis_planning_request request={};
    bool anchored=false,spawned=false,cancelled=false;
};
namespace {
int Fail(int e) { errno=e;return -1; }
uint64_t Now() { timespec t={};if(clock_gettime(CLOCK_MONOTONIC,&t)<0)return 0;return uint64_t(t.tv_sec)*1000000000+t.tv_nsec; }
int Left(uint64_t deadline) { auto t=Now();if(!t)return -1;return deadline<=t?0:int((deadline-t)/1000000); }
bool Owned(PackagePlanner* p) { return p&&p->process==syscall(SYS_getpid)&&aegis_namespace_check_broker()==0 ? true : (Fail(EPERM),false); }
int Encode(const PackagePlanning& p,aegis_planning_request* r) {
    if(p.requester<10||p.requester>=21473||p.serial>INT32_MAX||!p.job||p.job>INT64_MAX)return Fail(EINVAL);
    if(PackageResolverCheck(p.request)<0)return -1;
    if(p.request.package.size()>128||p.request.version.size()>128)return Fail(EINVAL);
    *r={};r->magic=AEGIS_PLANNING_MAGIC;r->version=AEGIS_PLANNING_VERSION;r->user=p.requester;r->serial=p.serial;r->job=p.job;
    r->internet=p.request.internet;r->action=uint32_t(p.request.action);memcpy(r->package,p.request.package.data(),p.request.package.size());
    memcpy(r->version_text,p.request.version.data(),p.request.version.size());return 0;
}
int PolicyFile(int fd,uint64_t maximum) {
    struct stat s;int f=fcntl(fd,F_GETFL);if(f<0||fstat(fd,&s)<0)return -1;
    if((f&(O_ACCMODE|O_PATH))!=O_RDONLY||!S_ISREG(s.st_mode)||s.st_uid||s.st_gid
       ||s.st_nlink!=1||(s.st_mode&07022)||s.st_size<=0||uint64_t(s.st_size)>maximum)return Fail(EPERM);
    return 0;
}
}
int PackagePlanningCheck(const PackagePlanning& p) { aegis_planning_request r;return Encode(p,&r); }
int PackagePlannerStart(int groups,int factory,int selected,int sources,int key,int helper,
                         const PackagePlanning& plan,uint64_t deadline,PackagePlanner** output,int network_helper,int ca_bundle) {
    if(!output||*output)return Fail(EINVAL);
    if(aegis_namespace_check_broker()<0)return -1;
    auto now=Now();if(!now)return -1;
    if(deadline<=now||deadline-now>UINT64_C(10000000000))return Fail(ETIMEDOUT);
    aegis_planning_request r;if(Encode(plan,&r)<0||PolicyFile(sources,16384)<0||PolicyFile(key,1048576)<0)return -1;
    if(plan.request.internet && (network_helper<0||ca_bundle<0))return Fail(EINVAL);
    if(plan.request.internet && PolicyFile(ca_bundle,1048576)<0)return -1;
    auto* p=new(std::nothrow) PackagePlanner;if(!p)return Fail(ENOMEM);
    p->process=syscall(SYS_getpid);p->request=r;*output=p;
    p->selected.reset(fcntl(selected,F_DUPFD_CLOEXEC,4));p->sources.reset(fcntl(sources,F_DUPFD_CLOEXEC,4));p->key.reset(fcntl(key,F_DUPFD_CLOEXEC,4));
    p->ca.reset(fcntl(plan.request.internet?ca_bundle:key,F_DUPFD_CLOEXEC,4));
    if(!p->selected.ok()||!p->sources.ok()||!p->key.ok()||!p->ca.ok())return -1;
    int pair[2];if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair)<0)return -1;
    p->channel.reset(pair[0]);unique_fd child(pair[1]);
    if(aegis_memory_group_create(groups,plan.requester,plan.serial,&p->group)<0
       ||aegis_namespace_create_limited(plan.requester,plan.serial,helper,child.get(),p->group,&p->context)<0)return -1;
    p->spawned=true;child.reset();int left=Left(deadline);if(left<=0)return Fail(ETIMEDOUT);
    if(aegis_namespace_prepare_package_for(p->context,left)<0)return -1;
    if(plan.request.internet) {
        unique_fd netns(aegis_namespace_planner_network(p->context));if(!netns.ok())return -1;
        left=Left(deadline);if(left<=0)return Fail(ETIMEDOUT);
        if(PackageNetworkStart(netns.get(),network_helper,p->group,plan.requester,plan.serial,plan.job,left,&p->network)<0)return -1;
    }
    p->root.reset(aegis_namespace_base_mount(p->context,factory));
    p->devices.reset(aegis_namespace_devices_mount(p->context));if(!p->root.ok()||!p->devices.ok())return -1;
    if(aegis_namespace_temporary_base_begin(p->selected.get())<0)return -1;p->anchored=true;
    p->metadata.reset(aegis_namespace_base_mount(p->context,p->selected.get()));if(!p->metadata.ok())return -1;
    if(aegis_namespace_temporary_base_end(p->selected.get())<0)return -1;p->anchored=false;
    int fds[]={p->root.get(),p->devices.get(),p->metadata.get(),p->sources.get(),p->key.get(),p->ca.get()};
    if(Left(deadline)<=0)return Fail(ETIMEDOUT);
    if(aegis_planning_send(p->channel.get(),&r,sizeof(r),fds,6)<0||aegis_namespace_resume(p->context)<0)return -1;
    // Channel owns queued copies; originals remain registered until reaping.
    return Left(deadline)>0?0:Fail(ETIMEDOUT);
}
int PackagePlannerCancel(PackagePlanner* p) {
    if(!Owned(p))return -1;p->cancelled=true;int e=0;
    if(p->context&&aegis_namespace_stop(p->context)<0)e=errno;
    if(p->network&&aegis_child_request_stop(p->network)<0&&!e)e=errno;
    if(p->group&&aegis_memory_group_kill_and_wait(p->group,0)<0&&errno!=ETIMEDOUT&&!e)e=errno;
    return e?Fail(e):0;
}
int PackagePlannerFinish(PackagePlanner** pointer,bool cancel,int timeout,PackagePlanningResult* result,int* collected) {
    if(!pointer||!*pointer||!result||!collected||*collected!=-1||timeout<0||timeout>10000)return Fail(EINVAL);
    auto* p=*pointer;if(!Owned(p))return -1;auto now=Now();if(!now)return -1;
    auto deadline=now+uint64_t(timeout)*1000000;
    if(cancel)(void)PackagePlannerCancel(p);
    int left=Left(deadline);if(left<0)return -1;aegis_child_exit exited={};
    if(p->context&&aegis_namespace_wait(p->context,left,&exited)<0)return -1;
    left=Left(deadline);if(left<0)return -1;
    if(p->group&&aegis_memory_group_kill_and_wait(p->group,left)<0)return -1;
    aegis_child_exit network_exit={};
    if(p->network) {
        left=Left(deadline);if(left<0)return -1;
        if(aegis_child_wait(p->network,left,&network_exit)<0)return -1;
    }
    if(p->group&&aegis_memory_group_remove(&p->group)<0)return -1;
    if(p->anchored) { if(aegis_namespace_temporary_base_end(p->selected.get())<0)return -1;p->anchored=false; }
    PackagePlanningResult r;r.error=EIO;
    if(p->spawned && exited.code==CLD_EXITED)r.status=exited.status;
    unique_fd directory;
    if(!p->spawned||p->cancelled) { r.outcome=PackagePlanningResult::Outcome::Failed;r.error=ECANCELED; }
    else if(exited.code==CLD_EXITED&&exited.status==0) {
        aegis_planning_reply reply={};int fd=-1;
        // Every well-formed terminal reply includes a scratch directory, even
        // on APT failure. Worker setup failures exit nonzero instead.
        if(aegis_planning_receive(p->channel.get(),&reply,sizeof(reply),&fd,1)==0) {
            directory.reset(fd);struct stat st;
            if(reply.magic==AEGIS_PLANNING_MAGIC&&reply.version==AEGIS_PLANNING_VERSION
               &&reply.user==p->request.user&&reply.serial==p->request.serial&&reply.job==p->request.job
               &&reply.phase<=uint32_t(PackageResolverResult::Phase::Collected)&&reply.status<=255&&reply.error<=4095&&reply.effects<=64
               &&fstat(fd,&st)==0&&S_ISDIR(st.st_mode)) {
                r.phase=static_cast<PackageResolverResult::Phase>(reply.phase);r.status=reply.status;r.error=reply.error;r.effects=reply.effects;
                r.outcome=reply.phase==uint32_t(PackageResolverResult::Phase::Collected)&&!r.status&&!r.error
                    ? PackagePlanningResult::Outcome::Collected : PackagePlanningResult::Outcome::Failed;
                if(r.outcome==PackagePlanningResult::Outcome::Collected
                   &&(planning_wire::Decode(reply.evidence,&r.evidence)<0 || r.evidence.changes.size()!=r.effects)) {
                    r.outcome=PackagePlanningResult::Outcome::Failed;r.error=EPROTO;
                }
            } else r.error=EPROTO;
        } else r.error=errno;
    }
    if(p->network&&!p->cancelled && (network_exit.code!=CLD_KILLED||network_exit.status!=SIGKILL)) {
        r.outcome=PackagePlanningResult::Outcome::Failed;r.error=EIO;
    }
    if(r.outcome==PackagePlanningResult::Outcome::Failed && !p->cancelled && directory.ok()) {
        const char* log=nullptr;
        switch(r.phase) {
            case PackageResolverResult::Phase::Update:log="update.log";break;
            case PackageResolverResult::Phase::Simulate:log="simulate.log";break;
            case PackageResolverResult::Phase::Download:log="download.log";break;
            case PackageResolverResult::Phase::Indexes:log="index-targets";break;
            default:break;
        }
        if(log) {
            unique_fd file(openat(directory.get(),log,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));
            struct stat st;struct statfs fs;
            if(file.ok()&&fstat(file.get(),&st)==0&&fstatfs(file.get(),&fs)==0&&fs.f_type==TMPFS_MAGIC
               &&S_ISREG(st.st_mode)&&st.st_nlink==1&&st.st_uid==p->request.user*100000+5000
               &&st.st_size>=0&&st.st_size<=(int64_t{512}<<20)) {
                char buffer[8192];off_t offset=st.st_size>off_t(sizeof(buffer))?st.st_size-off_t(sizeof(buffer)):0;
                ssize_t n=pread(file.get(),buffer,sizeof(buffer),offset);if(n>0)r.diagnostic.assign(buffer,n);
            }
        }
    }
    if(r.outcome==PackagePlanningResult::Outcome::Collected)*collected=directory.release();
    if(p->network)aegis_child_release(p->network);
    if(p->context)aegis_namespace_release(p->context);
    delete p;*pointer=nullptr;*result=r;return 0;
}
}
