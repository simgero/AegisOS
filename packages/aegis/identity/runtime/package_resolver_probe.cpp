// Device fixture only. Production resolver core is tested with an immutable
// verified factory root and separate deliberately hostile selected metadata.
#include "package_resolver_probe.h"
#include "package_resolver.h"
#include "package_apt_metadata_fixture.h"
#include "sandbox.h"
#include <android-base/unique_fd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/mount.h>
#include <linux/openat2.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string>
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
int Freeze(const char* path,bool executable) {
    unique_fd fd(open(path,O_PATH|O_DIRECTORY|O_CLOEXEC));if(!fd.ok())return -1;
    mount_attr a={};a.attr_set=MOUNT_ATTR_RDONLY|MOUNT_ATTR_NOSUID|MOUNT_ATTR_NODEV|(executable?0:MOUNT_ATTR_NOEXEC);
    return syscall(SYS_mount_setattr,fd.get(),"",AT_EMPTY_PATH,&a,sizeof(a));
}
int Setup(int* fds,unsigned mode) {
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
       ||Text(policy.get(),"config",mode==5?"APT::Get::AllowUnauthenticated \"true\";\n":aegis::PackageResolverConfiguration())<0
       ||Text(policy.get(),"sources.list","deb [signed-by=/run/aegis-plan-policy/key.asc] copy:/tmp/aegis-repo ./\n")<0
       ||Copy(fds[2],"var/lib/dpkg/status",input.get(),"status",64u<<20)<0)return -1;
    if(Copy(fds[2],"var/lib/apt/extended_states",input.get(),"extended_states",16u<<20)<0&&errno!=ENOENT)return -1;
    if(mkdirat(tmp.get(),"aegis-repo",0755)<0)return -1;
    for(const auto& file:aegis_apt_metadata) {
        const char* name=file.name;const unsigned char* data=file.data;size_t size=file.size;
        if(!strcmp(name,"aegis-test-key.asc")) {
            if(Write(policy.get(),"key.asc",data,size)<0)return -1;continue;
        }
        if(strncmp(name,"aegis-repo/",11))continue;
        if(mode==1&&!strcmp(name,"aegis-repo/Release.gpg"))continue;
        if(mode==2&&!strcmp(name,"aegis-repo/Release")) { data=aegis_apt_file_0;size=sizeof(aegis_apt_file_0); }
        if(mode==2&&!strcmp(name,"aegis-repo/Release.gpg")) { data=aegis_apt_file_1;size=sizeof(aegis_apt_file_1); }
        std::string changed(reinterpret_cast<const char*>(data),size);
        if((mode==3&&!strcmp(name,"aegis-repo/Packages"))
           ||(mode==4&&!strcmp(name,"aegis-repo/aegis-probe-app_2_all.deb")))changed[0]^=1;
        if(Write(tmp.get(),name,changed.data(),changed.size())<0)return -1;
    }
    unique_fd self(open("/proc/self/exe",O_RDONLY|O_CLOEXEC));if(!self.ok())return -1;
    // Only this already pinned static probe supplies the collector. No selected
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
extern "C" int aegis_probe_package_resolver(uint32_t user) {
    aegis_resolver_probe_reply reply={0x41525052,0,0,0,0};int fds[3]={-1,-1,-1};
    uint32_t request[2]={};alignas(cmsghdr) char controls[CMSG_SPACE(16*sizeof(int))]={};
    iovec io={request,sizeof(request)};msghdr msg={};msg.msg_iov=&io;msg.msg_iovlen=1;msg.msg_control=controls;msg.msg_controllen=sizeof(controls);
    ssize_t n=recvmsg(3,&msg,MSG_CMSG_CLOEXEC|MSG_TRUNC);unsigned count=0;bool bad=false;
    for(cmsghdr* h=CMSG_FIRSTHDR(&msg);h;h=CMSG_NXTHDR(&msg,h)) {
        if(h->cmsg_len<CMSG_LEN(0)) { bad=true;break; }
        if(h->cmsg_level!=SOL_SOCKET||h->cmsg_type!=SCM_RIGHTS) { bad=true;continue; }
        for(size_t i=0;i<(h->cmsg_len-CMSG_LEN(0))/sizeof(int);i++) {
            int fd;memcpy(&fd,CMSG_DATA(h)+i*sizeof(fd),sizeof(fd));if(count<3)fds[count++]=fd;else { close(fd);bad=true; }
        }
    }
    unique_fd output;
    if(n!=static_cast<ssize_t>(sizeof(request))||request[0]!=0x41525051||request[1]>7||count!=3||bad||(msg.msg_flags&(MSG_TRUNC|MSG_CTRUNC)))reply.error=EPROTO;
    else if(aegis_check_package_namespaces(user)<0||Setup(fds,request[1])<0)reply.error=errno;
    else {
        for(int& fd:fds) { close(fd);fd=-1; }
        // Local preparation descriptors leave scope before this point. Only
        // the fixture control socket survives the root transition.
        if(syscall(SYS_close_range,4u,~0u,0u)<0||syscall(SYS_pivot_root,".",".")<0
           ||umount2(".",MNT_DETACH)<0||chdir("/")<0)reply.error=errno;
        else {
            aegis::PackageResolverRequest r;
            if(request[1]==6) { r.action=aegis::PackageAction::Remove;r.package="aegis-probe-lib"; }
            else if(request[1]==7)r.action=aegis::PackageAction::Update;
            else { r.package="aegis-probe-app";r.version="2"; }
            auto result=aegis::PackageResolverRun(user,r);
            reply.phase=static_cast<uint32_t>(result.phase);reply.status=result.status;reply.error=result.error;reply.count=result.effects.size();
            output.reset(open("/tmp/aegis-planner",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        }
    }
    for(int fd:fds)if(fd>=0)close(fd);
    io={&reply,sizeof(reply)};msg={};msg.msg_iov=&io;msg.msg_iovlen=1;
    if(output.ok()) {
        memset(controls,0,sizeof(controls));msg.msg_control=controls;msg.msg_controllen=CMSG_SPACE(sizeof(int));
        auto* h=CMSG_FIRSTHDR(&msg);h->cmsg_level=SOL_SOCKET;h->cmsg_type=SCM_RIGHTS;h->cmsg_len=CMSG_LEN(sizeof(int));
        int fd=output.get();memcpy(CMSG_DATA(h),&fd,sizeof(fd));
    }
    return sendmsg(3,&msg,MSG_NOSIGNAL)==static_cast<ssize_t>(sizeof(reply))?0:125;
}
