// Trusted bounded-input preparation only: copy/hash/mount/cache, never APT or
// package scripts. Production SELinux/bootstrap/CE anchoring belongs to broker.
#include "package_preparation_protocol.h"
#include <android-base/unique_fd.h>
#include <openssl/sha.h>
#include <algorithm>
#include <array>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <linux/mount.h>
#include <linux/openat2.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>
using android::base::unique_fd;
namespace wire=aegis::preparation;
namespace {
int Fail(int error) { errno=error;return -1; }
int At(int root,const char* path,int flags,mode_t mode=0) {
    open_how how={};how.flags=flags|O_CLOEXEC|O_NOFOLLOW;how.mode=mode;
    how.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_XDEV;
    return syscall(SYS_openat2,root,path,&how,sizeof(how));
}
bool Plain(int fd,mode_t type,mode_t mode) {
    struct stat st;int flags=fcntl(fd,F_GETFL);
    if(flags<0 || fstat(fd,&st)<0)return false;
    if((flags&(O_ACCMODE|O_PATH))!=O_RDONLY || (st.st_mode&S_IFMT)!=type
       || (mode ? (st.st_mode&07777)!=mode : bool(st.st_mode&07022))
       || st.st_uid || st.st_gid || !st.st_nlink || (type==S_IFREG && st.st_nlink!=1)) {
        Fail(EPERM);return false;
    }
    for(const char* name:{"system.posix_acl_access","system.posix_acl_default"}) {
        if(fgetxattr(fd,name,nullptr,0)>=0 || (errno!=ENODATA && errno!=EOPNOTSUPP)) { Fail(EPERM);return false; }
    }
    return true;
}
int Empty(int root) {
    unique_fd scan(At(root,".",O_RDONLY|O_DIRECTORY));if(!scan.ok())return -1;
    DIR* entries=fdopendir(scan.release());if(!entries)return -1;
    int error=0;errno=0;
    while(auto* entry=readdir(entries))
        if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) { error=EEXIST;break; }
    if(!error)error=errno;closedir(entries);return error ? Fail(error) : 0;
}
int Write(int fd,const void* data,size_t size) {
    auto* p=static_cast<const char*>(data);
    while(size) {
        ssize_t n=TEMP_FAILURE_RETRY(write(fd,p,size));if(n<=0)return n<0 ? -1 : Fail(EIO);
        size-=n;p+=n;
    }
    return 0;
}
int Copy(int source,int output,const wire::Input& expected) {
    if(!Plain(source,S_IFREG,0))return -1;
    struct stat before,after;
    if(fstat(source,&before)<0)return -1;
    if(before.st_size!=static_cast<off_t>(expected.bytes))return Fail(ESTALE);
    SHA256_CTX hash;if(!SHA256_Init(&hash))return Fail(EIO);
    std::array<unsigned char,128*1024> buffer;
    for(uint64_t offset=0;offset<expected.bytes;) {
        size_t size=std::min<uint64_t>(buffer.size(),expected.bytes-offset);
        ssize_t n=TEMP_FAILURE_RETRY(pread(source,buffer.data(),size,offset));
        if(n<=0)return n<0 ? -1 : Fail(EIO);
        if(!SHA256_Update(&hash,buffer.data(),n))return Fail(EIO);
        if(output>=0 && Write(output,buffer.data(),n)<0)return -1;offset+=n;
    }
    unsigned char digest[32];char hex[65];
    if(!SHA256_Final(digest,&hash))return Fail(EIO);
    for(unsigned i=0;i<32;++i)snprintf(hex+2*i,3,"%02x",digest[i]);
    if(fstat(source,&after)<0)return -1;
    if(strcmp(hex,expected.hash) || before.st_dev!=after.st_dev || before.st_ino!=after.st_ino
       || before.st_size!=after.st_size || before.st_mtim.tv_sec!=after.st_mtim.tv_sec
       || before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec || before.st_ctim.tv_sec!=after.st_ctim.tv_sec
       || before.st_ctim.tv_nsec!=after.st_ctim.tv_nsec)return Fail(ESTALE);
    return output>=0 ? fsync(output) : 0;
}
int64_t Now() {
    timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)<0)return -1;
    return int64_t(t.tv_sec)*1000000000+t.tv_nsec;
}
int Mount(int image,uint64_t bytes) {
    struct stat backing;if(fstat(image,&backing)<0)return -1;
    unique_fd control(open("/dev/loop-control",O_RDWR|O_CLOEXEC|O_NOFOLLOW));if(!control.ok())return -1;
    struct stat st;if(fstat(control.get(),&st)<0)return -1;
    if(!S_ISCHR(st.st_mode) || major(st.st_rdev)!=10 || minor(st.st_rdev)!=237
       || st.st_uid || (st.st_mode&0022))return Fail(EPERM);
    int64_t now=Now();if(now<0)return -1;int64_t deadline=now+2000000000;
    for(unsigned attempt=0;attempt<16;++attempt) {
        int number=ioctl(control.get(),LOOP_CTL_GET_FREE);
        if(number<0)return -1;if(number>1048575)return Fail(EOVERFLOW);
        std::string path="/dev/block/loop"+std::to_string(number);unique_fd loop;
        for(;;) {
            now=Now();if(now<0)return -1;if(now>=deadline)return Fail(ETIMEDOUT);
            loop.reset(open(path.c_str(),O_RDWR|O_CLOEXEC|O_NOFOLLOW));
            if(loop.ok())break;if(errno!=ENOENT && errno!=EINTR)return -1;usleep(10000);
        }
        if(fstat(loop.get(),&st)<0)return -1;
        if(!S_ISBLK(st.st_mode) || major(st.st_rdev)!=7 || minor(st.st_rdev)!=static_cast<unsigned>(number)
           || st.st_uid || (st.st_mode&0022))return Fail(EPERM);
        loop_config config={};config.fd=image;config.info.lo_flags=LO_FLAGS_AUTOCLEAR;
        if(ioctl(loop.get(),LOOP_CONFIGURE,&config)<0) { if(errno==EBUSY)continue;return -1; }
        loop_info64 info={};uint64_t actual=0;int readonly=-1;
        if(ioctl(loop.get(),LOOP_GET_STATUS64,&info)<0 || ioctl(loop.get(),BLKGETSIZE64,&actual)<0
           || ioctl(loop.get(),BLKROGET,&readonly)<0)return -1;
        if(info.lo_device!=static_cast<uint64_t>(backing.st_dev) || info.lo_inode!=backing.st_ino
           || info.lo_number!=static_cast<unsigned>(number) || info.lo_offset || info.lo_sizelimit
           || info.lo_flags!=LO_FLAGS_AUTOCLEAR || info.lo_encrypt_type || info.lo_encrypt_key_size
           || actual!=bytes || readonly)return Fail(EPROTO);
        unique_fd fs(syscall(SYS_fsopen,"ext4",FSOPEN_CLOEXEC));if(!fs.ok())return -1;
        std::string device="/proc/self/fd/"+std::to_string(loop.get());
        if(syscall(SYS_fsconfig,fs.get(),FSCONFIG_SET_STRING,"source",device.c_str(),0)<0
           || syscall(SYS_fsconfig,fs.get(),FSCONFIG_SET_STRING,"context","u:object_r:aegis_runtime_base_file:s0",0)<0
           || syscall(SYS_fsconfig,fs.get(),FSCONFIG_CMD_CREATE,nullptr,nullptr,0)<0)return -1;
        // Detached mount owns the autoclearing loop; never attach or force-clear.
        return syscall(SYS_fsmount,fs.get(),FSMOUNT_CLOEXEC,MOUNT_ATTR_NOSUID|MOUNT_ATTR_NODEV|MOUNT_ATTR_NOEXEC);
    }
    return Fail(EBUSY);
}
int Prepare(const wire::Request& request) {
    if(!Plain(wire::kStage,S_IFDIR,0700) || flock(wire::kStage,LOCK_EX|LOCK_NB)<0
       || Empty(wire::kStage)<0)return -1;
    // The stage is exclusively owned and all children below are O_EXCL. No
    // retry, deletion, selection mutation or adoption of prior incomplete work.
    unique_fd identity(At(wire::kStage,"request",O_WRONLY|O_CREAT|O_EXCL,0600));
    if(!identity.ok() || Write(identity.get(),&request,sizeof(request))<0
       || fchmod(identity.get(),0400)<0 || fsync(identity.get())<0)return -1;
    identity.reset();
    unique_fd image(At(wire::kStage,"candidate.ext4",O_RDWR|O_CREAT|O_EXCL,0600));
    if(!image.ok() || Copy(wire::kSource,image.get(),request.image)<0)return -1;
    unique_fd mount(Mount(image.get(),request.image.bytes));if(!mount.ok())return -1;
    if(request.execution.kind==AEGIS_PACKAGE_ARCHIVES) {
        unique_fd cache(At(mount.get(),"var",O_RDONLY|O_DIRECTORY));if(!cache.ok())return -1;
        for(const char* part:{"cache","apt","archives"}) {
            if(mkdirat(cache.get(),part,0755)<0 && errno!=EEXIST)return -1;
            unique_fd next(At(cache.get(),part,O_RDONLY|O_DIRECTORY));
            if(!next.ok() || !Plain(next.get(),S_IFDIR,0755))return -1;
            cache=std::move(next);
        }
        for(unsigned i=0;i<request.execution.count;++i) {
            // A previous complete generation may already contain this exact
            // archive. Verify BOTH pinned input and cached contents; never
            // truncate, follow a link, or silently reuse an unverified file.
            unique_fd output(At(cache.get(),request.execution.items[i],O_WRONLY|O_CREAT|O_EXCL,0644));
            if(!output.ok()) {
                if(errno!=EEXIST)return -1;
                unique_fd cached(At(cache.get(),request.execution.items[i],O_RDONLY));
                if(!cached.ok() || Copy(wire::kArchive+i,-1,request.archives[i])<0
                   || Copy(cached.get(),-1,request.archives[i])<0)return -1;
            } else if(Copy(wire::kArchive+i,output.get(),request.archives[i])<0)return -1;
        }
        if(fsync(cache.get())<0)return -1;
    }
    unique_fd root(At(mount.get(),".",O_RDONLY|O_DIRECTORY));
    if(!root.ok() || syncfs(root.get())<0 || fsync(image.get())<0 || fsync(wire::kStage)<0)return -1;
    return mount.release();
}
}
int main(int argc,char**) {
    uid_t r,e,s;gid_t gr,ge,gs;struct stat st={};wire::Request request={};
    int death=0,type=0;socklen_t length=sizeof(type);
    if(argc!=1 || getresuid(&r,&e,&s)<0 || getresgid(&gr,&ge,&gs)<0 || r||e||s||gr||ge||gs
       || getgroups(0,nullptr)!=0 || prctl(PR_GET_PDEATHSIG,&death)<0 || death!=SIGKILL
       || prctl(PR_SET_DUMPABLE,0)<0 || fstat(wire::kRequest,&st)<0 || !S_ISREG(st.st_mode)
       || st.st_uid || st.st_gid || st.st_size!=static_cast<off_t>(sizeof(request))
       || fcntl(wire::kRequest,F_GET_SEALS)!=(F_SEAL_WRITE|F_SEAL_GROW|F_SEAL_SHRINK|F_SEAL_SEAL)
       || pread(wire::kRequest,&request,sizeof(request),0)!=static_cast<ssize_t>(sizeof(request))
       || !wire::Valid(request) || getsockopt(wire::kReply,SOL_SOCKET,SO_TYPE,&type,&length)<0
       || type!=SOCK_SEQPACKET)return 120;
    close(wire::kRequest);
    // Broker uses0077. Only this separate process changes its mask: ordinary
    // candidate directories/cache files need0755/0644, while stage inputs are
    // explicitly0600 and the retained request is explicitly sealed0400.
    umask(0022);
    unique_fd mount(Prepare(request));int error=mount.ok() ? 0 : errno;
    // Successful transfer closes source/stage/archive references before reply;
    // the queued detached mount is still owned by the registered parent socket.
    close(wire::kStage);close(wire::kSource);
    if(request.execution.kind==AEGIS_PACKAGE_ARCHIVES)
        for(unsigned i=0;i<request.execution.count;++i)close(wire::kArchive+i);
    wire::Reply reply={};reply.magic=wire::kMagic;reply.version=wire::kVersion;
    reply.user=request.execution.user;reply.serial=request.execution.serial;reply.job=request.execution.job;
    reply.error=error ? error : 0;memcpy(reply.plan,request.execution.plan,65);
    alignas(cmsghdr) char ancillary[CMSG_SPACE(sizeof(int))]={};iovec io={&reply,sizeof(reply)};
    msghdr message={};message.msg_iov=&io;message.msg_iovlen=1;
    if(mount.ok()) {
        message.msg_control=ancillary;message.msg_controllen=sizeof(ancillary);
        auto* c=CMSG_FIRSTHDR(&message);c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;c->cmsg_len=CMSG_LEN(sizeof(int));
        int fd=mount.get();memcpy(CMSG_DATA(c),&fd,sizeof(fd));
    }
    ssize_t sent=TEMP_FAILURE_RETRY(sendmsg(wire::kReply,&message,MSG_NOSIGNAL|MSG_DONTWAIT));
    return sent==static_cast<ssize_t>(sizeof(reply)) ? 0 : 121;
}
