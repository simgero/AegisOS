#include "package_resolver.h"
#include "package_planning_protocol.h"
#include "sandbox.h"
extern "C" {
#include "package_apt_hook.h"
}
#ifdef AEGIS_PLANNER_PROBE
#include "package_apt_metadata_fixture.h"
#endif
#include <android-base/unique_fd.h>
#include <fcntl.h>
#include <linux/mount.h>
#include <linux/openat2.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string>
#include <stdio.h>
using android::base::unique_fd;
namespace {
int Fail(int e) { errno=e;return -1; }
int Write(int directory,const char* name,const void* data,size_t size,mode_t mode=0444) {
    unique_fd fd(openat(directory,name,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,mode));if(!fd.ok())return -1;
    const char* p=static_cast<const char*>(data);
    while(size) { ssize_t n=write(fd.get(),p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;p+=n;size-=n; }
    return 0;
}
int Text(int dir,const char* name,const char* data,mode_t mode=0444) { return Write(dir,name,data,strlen(data),mode); }
int Copy(int from,const char* name,int to,const char* target,size_t limit,mode_t mode=0444) {
    open_how how={};how.flags=O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK;
    how.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_XDEV;
    unique_fd fd(syscall(SYS_openat2,from,name,&how,sizeof(how)));if(!fd.ok())return -1;
    struct stat st;if(fstat(fd.get(),&st)<0)return -1;
    if(!S_ISREG(st.st_mode)||st.st_uid||st.st_gid||st.st_nlink!=1||(st.st_mode&07022)
       ||st.st_size<0||uint64_t(st.st_size)>limit)return Fail(EPERM);
    std::string bytes(st.st_size,'\0');size_t at=0;
    while(at<bytes.size()) { ssize_t n=pread(fd.get(),bytes.data()+at,bytes.size()-at,at);if(n<0&&errno==EINTR)continue;if(n<=0)return Fail(EIO);at+=n; }
    return Write(to,target,bytes.data(),bytes.size(),mode);
}
int CopyFd(int fd,int target,const char* name,size_t limit) {
    struct stat st;if(fstat(fd,&st)<0)return -1;
    if(!S_ISREG(st.st_mode)||st.st_size<=0||uint64_t(st.st_size)>limit)return Fail(EPERM);
    std::string bytes(st.st_size,'\0');size_t at=0;
    while(at<bytes.size()) { ssize_t n=pread(fd,bytes.data()+at,bytes.size()-at,at);if(n<0&&errno==EINTR)continue;if(n<=0)return Fail(EIO);at+=n; }
    return Write(target,name,bytes.data(),bytes.size());
}
int Freeze(const char* path,bool executable) {
    unique_fd fd(open(path,O_PATH|O_DIRECTORY|O_CLOEXEC));if(!fd.ok())return -1;
    mount_attr a={};a.attr_set=MOUNT_ATTR_RDONLY|MOUNT_ATTR_NOSUID|MOUNT_ATTR_NODEV|(executable?0:MOUNT_ATTR_NOEXEC);
    return syscall(SYS_mount_setattr,fd.get(),"",AT_EMPTY_PATH,&a,sizeof(a));
}
int Setup(int* fds) {
    if(syscall(SYS_move_mount,fds[0],"",AT_FDCWD,"/mnt",MOVE_MOUNT_F_EMPTY_PATH)<0
       ||syscall(SYS_move_mount,fds[1],"",fds[0],"dev",MOVE_MOUNT_F_EMPTY_PATH)<0
       ||mount("proc","/mnt/proc","proc",MS_NOSUID|MS_NODEV|MS_NOEXEC,"hidepid=2,subset=pid")<0
       ||mount("tmpfs","/mnt/tmp","tmpfs",MS_NOSUID|MS_NODEV|MS_NOEXEC,"mode=1777,size=536870912,nr_inodes=32768")<0
       ||mount("tmpfs","/mnt/run","tmpfs",MS_NOSUID|MS_NODEV|MS_NOEXEC,"mode=0755,size=134217728,nr_inodes=4096")<0)return -1;
    for(const char* p:{"/mnt/run/aegis-plan-policy","/mnt/run/aegis-plan-input"})
        if(mkdir(p,0755)<0||mount("tmpfs",p,"tmpfs",MS_NOSUID|MS_NODEV,"mode=0755,size=100663296,nr_inodes=2048")<0)return -1;
    unique_fd policy(open("/mnt/run/aegis-plan-policy",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    unique_fd input(open("/mnt/run/aegis-plan-input",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    unique_fd tmp(open("/mnt/tmp",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    if(!policy.ok()||!input.ok()||!tmp.ok()||mkdirat(policy.get(),"empty",0755)<0
       ||Text(policy.get(),"empty.conf","")<0
       ||Text(policy.get(),"config",aegis::PackageResolverConfiguration())<0
       ||CopyFd(fds[3],policy.get(),"sources.list",16384)<0
       ||CopyFd(fds[4],policy.get(),"key.asc",1048576)<0
       ||Copy(fds[2],"var/lib/dpkg/status",input.get(),"status",64u<<20)<0)return -1;
    if(Copy(fds[2],"var/lib/apt/extended_states",input.get(),"extended_states",16u<<20)<0&&errno!=ENOENT)return -1;
#ifdef AEGIS_PLANNER_PROBE
    // Device-only deterministic repository; never linked into the product helper.
    if(mkdirat(tmp.get(),"aegis-repo",0755)<0)return -1;
    for(const auto& file:aegis_apt_metadata) {
        if(strncmp(file.name,"aegis-repo/",11))continue;
        if(Write(tmp.get(),file.name,file.data,file.size)<0)return -1;
    }
#endif
    unique_fd self(open("/proc/self/exe",O_RDONLY|O_CLOEXEC));if(!self.ok())return -1;
    // Only this already pinned static helper supplies the collector. No selected
    // package executable or configuration is copied into the planner policy.
    struct stat st;if(fstat(self.get(),&st)<0||st.st_size<=0||st.st_size>16*1024*1024)return -1;
    std::string bytes(st.st_size,'\0');size_t at=0;
    while(at<bytes.size()) { ssize_t n=pread(self.get(),bytes.data()+at,bytes.size()-at,at);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;at+=n; }
    if(Write(policy.get(),"hook",bytes.data(),bytes.size(),0555)<0
       ||Freeze("/mnt/run/aegis-plan-policy",true)<0||Freeze("/mnt/run/aegis-plan-input",false)<0
       ||fchdir(fds[0])<0)return -1;
    return 0;
}
}

namespace {
bool Number(const char* s,uint32_t* output) {
    if(!*s||(*s=='0'&&s[1]))return false;uint64_t n=0;
    for(;*s;++s) { if(*s<'0'||*s>'9')return false;n=n*10+*s-'0';if(n>INT32_MAX)return false; }
    *output=n;return true;
}
bool Fixed(const char* s,size_t size) {
    auto n=strnlen(s,size);if(n==size)return false;
    for(size_t i=n;i<size;++i)if(s[i])return false;return true;
}
}
int main(int argc,char** argv) {
    if(argc==2&&!strcmp(argv[1],"--apt-plan-hook"))return aegis_apt_plan_hook();
    uint32_t user,serial;
    if(argc!=3||!Number(argv[1],&user)||!Number(argv[2],&serial))return 78;
#ifdef AEGIS_PLANNER_PROBE
    if(aegis_check_package_namespaces(user)<0)return 78;
#else
    if(aegis_check_package_context(user)<0)return 78;
#endif
    umask(022); // Fixed policy/input directory modes, independent of broker umask.
    int fds[5]={-1,-1,-1,-1,-1};aegis_planning_request r={};
    if(aegis_planning_receive(3,&r,sizeof(r),fds,5)<0)return 79;
    aegis::PackageResolverRequest request;
    bool valid=r.magic==AEGIS_PLANNING_MAGIC&&r.version==AEGIS_PLANNING_VERSION
        &&r.user==user&&r.serial==serial&&r.job&&r.job<=INT64_MAX&&!r.reserved
        &&Fixed(r.package,sizeof(r.package))&&Fixed(r.version_text,sizeof(r.version_text));
    for(auto c:r.padding)if(c)valid=false;
    if(!valid)return 80;
    request.action=static_cast<aegis::PackageAction>(r.action);request.package=r.package;request.version=r.version_text;
    if(aegis::PackageResolverCheck(request)<0||Setup(fds)<0) { perror("aegis planner setup");return 81; }
    for(int fd:fds)close(fd);
    if(syscall(SYS_close_range,4u,~0u,0u)<0||syscall(SYS_pivot_root,".",".")<0
       ||umount2(".",MNT_DETACH)<0||chdir("/")<0)return 82;
    auto result=aegis::PackageResolverRun(user,request);
    unique_fd output(open("/tmp/aegis-planner",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
    if(!output.ok())return 83;
    aegis_planning_reply reply={AEGIS_PLANNING_MAGIC,AEGIS_PLANNING_VERSION,user,serial,r.job,
        uint32_t(result.phase),uint32_t(result.status),uint32_t(result.error),uint32_t(result.effects.size()),{}};
    if(result.phase==aegis::PackageResolverResult::Phase::Collected&&!result.status&&!result.error
       &&aegis::planning_wire::Encode(result.evidence,&reply.evidence)<0)return 85;
    int fd=output.get();return aegis_planning_send(3,&reply,sizeof(reply),&fd,1)==0?0:84;
}
