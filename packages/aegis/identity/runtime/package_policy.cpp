#include "package_policy.h"
#include <android-base/unique_fd.h>
#include <algorithm>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/memfd.h>
#include <linux/openat2.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>
using android::base::unique_fd;
namespace {
constexpr int seals=F_SEAL_WRITE|F_SEAL_GROW|F_SEAL_SHRINK|F_SEAL_SEAL;
constexpr uint64_t maximum_bundle=1048576;
constexpr char sources[]=
    "deb [signed-by=/run/aegis-plan-policy/key.asc] https://deb.debian.org/debian trixie main\n"
    "deb [signed-by=/run/aegis-plan-policy/key.asc] https://deb.debian.org/debian trixie-updates main\n"
    "deb [signed-by=/run/aegis-plan-policy/key.asc] https://security.debian.org/debian-security trixie-security main\n";
int fail(int e) { errno=e;return -1; }
int at(int directory,const char* name,int flags) {
    open_how how={};how.flags=flags|O_NOFOLLOW|O_CLOEXEC;
    how.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_XDEV;
    return syscall(SYS_openat2,directory,name,&how,sizeof(how));
}
int label(int fd,const char* expected) {
    char text[128]={};ssize_t count=fgetxattr(fd,"security.selinux",text,sizeof(text));
    if(count<0)return -1;
    size_t size=strlen(expected);
    return (count==static_cast<ssize_t>(size) || (count==static_cast<ssize_t>(size+1)&&!text[size]))
        && !memcmp(text,expected,size) ? 0 : fail(EPERM);
}
int directory(int fd,const char* expected) {
    struct stat st;struct statfs fs;struct statvfs flags;
    if(fstat(fd,&st)<0||fstatfs(fd,&fs)<0||fstatvfs(fd,&flags)<0)return -1;
    if(!S_ISDIR(st.st_mode)||st.st_uid||(st.st_gid!=0&&st.st_gid!=2000)||(st.st_mode&07022)
       ||!(flags.f_flag&ST_RDONLY)||(fs.f_type!=EXT4_SUPER_MAGIC&&fs.f_type!=EROFS_SUPER_MAGIC_V1))return fail(EPERM);
    return label(fd,expected);
}
int read_file(int parent,const char* name,uid_t owner,gid_t group,const char* expected,
              size_t maximum,std::string* result) {
    unique_fd fd(at(parent,name,O_RDONLY|O_NONBLOCK));if(!fd.ok())return -1;
    struct stat st;if(fstat(fd.get(),&st)<0)return -1;
    if(!S_ISREG(st.st_mode)||st.st_uid!=owner||st.st_gid!=group||st.st_nlink!=1
       ||(st.st_mode&07022)||st.st_size<=0||uint64_t(st.st_size)>maximum)return fail(EPERM);
    if(label(fd.get(),expected)<0)return -1;
    std::string text(st.st_size,'\0');size_t offset=0;
    while(offset<text.size()) {
        ssize_t count=pread(fd.get(),text.data()+offset,text.size()-offset,offset);
        if(count<0&&errno==EINTR)continue;if(count<=0)return fail(EIO);offset+=count;
    }
    if(text.find('\0')!=std::string::npos)return fail(EPROTO);
    *result=std::move(text);return 0;
}
int sealed(const char* name,const std::string& text) {
    if(text.empty()||text.size()>maximum_bundle)return fail(EFBIG);
    unique_fd write(syscall(SYS_memfd_create,name,MFD_CLOEXEC|MFD_ALLOW_SEALING));if(!write.ok())return -1;
    size_t offset=0;
    while(offset<text.size()) {
        ssize_t count=pwrite(write.get(),text.data()+offset,text.size()-offset,offset);
        if(count<0&&errno==EINTR)continue;if(count<=0)return fail(EIO);offset+=count;
    }
    if(fchmod(write.get(),0400)<0||fcntl(write.get(),F_ADD_SEALS,seals)<0)return -1;
    char path[64];snprintf(path,sizeof(path),"/proc/self/fd/%d",write.get());
    unique_fd readonly(open(path,O_RDONLY|O_CLOEXEC));if(!readonly.ok())return -1;
    if(aegis_package_policy_file(readonly.get(),maximum_bundle)<0)return -1;
    return readonly.release();
}
}
int aegis_package_policy_file(int fd,uint64_t maximum) {
    struct stat st;int flags=fcntl(fd,F_GETFL);if(flags<0||fstat(fd,&st)<0)return -1;
    if((flags&(O_ACCMODE|O_PATH))!=O_RDONLY||!S_ISREG(st.st_mode)||st.st_uid||st.st_gid
       ||(st.st_mode&07022)||st.st_size<=0||uint64_t(st.st_size)>maximum)return fail(EPERM);
    if(st.st_nlink==1)return 0;
    if(st.st_nlink!=0)return fail(EPERM);
    int applied=fcntl(fd,F_GET_SEALS);
    return applied>=0&&(applied&seals)==seals ? 0 : fail(EPERM);
}
int aegis_package_policy_open(int factory,int output[3]) {
    if(!output||output[0]!=-1||output[1]!=-1||output[2]!=-1)return fail(EINVAL);
    if(getuid()||geteuid()||getgid()||getegid()||syscall(SYS_getpid)!=syscall(SYS_gettid))return fail(EPERM);
    constexpr char base_label[]="u:object_r:aegis_runtime_base_file:s0";
    unique_fd root(at(factory,".",O_RDONLY|O_DIRECTORY));if(!root.ok()||directory(root.get(),base_label)<0)return -1;
    std::string keys;
    for(const char* name:{"etc/apt/trusted.gpg.d/debian-archive-trixie-automatic.asc",
                         "etc/apt/trusted.gpg.d/debian-archive-trixie-security-automatic.asc",
                         "etc/apt/trusted.gpg.d/debian-archive-trixie-stable.asc"}) {
        std::string text;if(read_file(root.get(),name,0,0,base_label,65536,&text)<0)return -1;
        if(text.find("-----BEGIN PGP PUBLIC KEY BLOCK-----")!=0
           ||text.find("-----END PGP PUBLIC KEY BLOCK-----")==std::string::npos)return fail(EPROTO);
        keys+=text;keys+='\n';
    }
    // AOSP verifies/mounts this signed APEX. Pin its directory before listing;
    // no system/user-added writable certificate store is consulted.
    constexpr char ca_label[]="u:object_r:system_security_cacerts_file:s0";
    unique_fd certs(open("/apex/com.android.conscrypt/cacerts",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    if(!certs.ok()||directory(certs.get(),ca_label)<0)return -1;
    unique_fd scan(at(certs.get(),".",O_RDONLY|O_DIRECTORY));if(!scan.ok())return -1;
    DIR* entries=fdopendir(scan.get());if(!entries)return -1;(void)scan.release();
    std::vector<std::string> names;int error=0;
    for(;;) {
        errno=0;auto* entry=readdir(entries);if(!entry) { error=errno;break; }
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
        std::string name=entry->d_name;
        if(name.size()!=10||name[8]!='.'||name[9]<'0'||name[9]>'9'
           ||name.substr(0,8).find_first_not_of("0123456789abcdef")!=std::string::npos
           ||names.size()>=512) { error=EPROTO;break; }
        names.push_back(std::move(name));
    }
    closedir(entries);if(error)return fail(error);if(names.empty())return fail(ENOENT);
    std::sort(names.begin(),names.end());std::string ca;
    for(const auto& name:names) {
        std::string text;if(read_file(certs.get(),name.c_str(),1000,1000,ca_label,32768,&text)<0)return -1;
        if(text.find("-----BEGIN CERTIFICATE-----")==std::string::npos
           ||text.find("-----END CERTIFICATE-----")==std::string::npos)return fail(EPROTO);
        if(ca.size()+text.size()+1>maximum_bundle)return fail(EFBIG);
        ca+=text;ca+='\n';
    }
    unique_fd configured_sources(sealed("aegis-package-sources",sources)),
              configured_keys(sealed("aegis-package-keys",keys)),configured_ca(sealed("aegis-package-ca",ca));
    if(!configured_sources.ok()||!configured_keys.ok()||!configured_ca.ok())return -1;
    output[0]=configured_sources.release();output[1]=configured_keys.release();output[2]=configured_ca.release();return 0;
}
