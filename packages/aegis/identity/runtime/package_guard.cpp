#include "package_guard.h"
#include "package_apt_plan.h"
#include "package_private_choices.h"
#include "package_registry.h"
#include "package_reconciliation.h"
#include <openssl/sha.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/openat2.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include <new>
using Installed=aegis::PackageInstalledRegistry;
struct aegis_package_guard {
    aegis_package_execution_request request={};
    Installed expected;
    std::set<std::string> automatic;
};
namespace {
int Fail(int e) { errno=e;return -1; }
std::string Hash(const std::string& s) {
    unsigned char h[32];if(!SHA256(reinterpret_cast<const unsigned char*>(s.data()),s.size(),h))return {};
    const char hex[]="0123456789abcdef";std::string out;
    for(auto b:h) { out+=hex[b>>4];out+=hex[b&15]; }return out;
}
int ReadFd(int fd,size_t limit,std::string* out) {
    struct stat before,after;if(fstat(fd,&before)<0)return -1;
    if(!S_ISREG(before.st_mode)||before.st_uid||before.st_gid||before.st_nlink!=1
       || (before.st_mode&07022)||before.st_size<0||uint64_t(before.st_size)>limit)return Fail(EPERM);
    std::string data(static_cast<size_t>(before.st_size),'\0');
    for(size_t at=0;at<data.size();) {
        auto n=pread(fd,data.data()+at,data.size()-at,at);if(n<0&&errno==EINTR)continue;
        if(n<0)return -1;if(!n)return Fail(ESTALE);at+=n;
    }
    if(fstat(fd,&after)<0)return -1;
    if(before.st_size!=after.st_size || before.st_mtim.tv_sec!=after.st_mtim.tv_sec
       || before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec || before.st_ctim.tv_sec!=after.st_ctim.tv_sec
       || before.st_ctim.tv_nsec!=after.st_ctim.tv_nsec)return Fail(ESTALE);
    *out=std::move(data);return 0;
}
int Read(int root,const char* path,size_t limit,std::string* out) {
    open_how how={};how.flags=O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW;
    how.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV;
    int fd=syscall(SYS_openat2,root,path,&how,sizeof(how));if(fd<0)return -1;
    int result=ReadFd(fd,limit,out),saved=errno;close(fd);errno=saved;return result;
}
int CheckChoices(int root,const char* expected) {
    std::string actual;int result=Read(root,aegis::kPrivateChoicesPath,AEGIS_PACKAGE_CHOICES_BYTES-1,&actual);
    if(result<0 && errno!=ENOENT)return -1;
    if((result==0)!=bool(*expected) || actual!=expected)return Fail(ESTALE);
    return 0;
}
int MatchChoices(const char* text,const Installed& installed) {
    aegis::PackagePrivateChoices choices;if(aegis::PackagePrivateChoicesDecode(text,&choices)<0)return -1;
    for(const auto& [name,c]:choices) {
        auto it=installed.find(name);
        if(it==installed.end() || std::get<0>(it->second)!=c.version || std::get<1>(it->second)!=c.architecture)return Fail(ESTALE);
    }
    return 0;
}
int Directory(int parent,const char* name) {
    open_how how={};how.flags=O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW;
    how.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV;
    return syscall(SYS_openat2,parent,name,&how,sizeof(how));
}
int StoreChoices(int root,const char* expected,const char* desired) {
    if(CheckChoices(root,expected)<0)return -1;
    if(!*desired)return 0; // A shared generation cannot gain private intent.
    int lib=Directory(root,"var/lib");if(lib<0)return -1;
    struct stat st;int error=0;
    if(fstat(lib,&st)<0)error=errno;
    else if(st.st_uid || st.st_gid || (st.st_mode&07022))error=EPERM;
    if(!error && mkdirat(lib,"aegis",0700)<0 && errno!=EEXIST)error=errno;
    int dir=error?-1:Directory(lib,"aegis");if(dir<0&&!error)error=errno;
    if(!error && fstat(dir,&st)<0)error=errno;
    if(!error && (st.st_uid || st.st_gid || st.st_mode!=(S_IFDIR|0700)))error=EPERM;
    int fd=error?-1:openat(dir,"private-choices.new",O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC|O_WRONLY,0600);
    if(fd<0&&!error)error=errno;
    if(fd>=0) {
        size_t size=strlen(desired);
        for(size_t at=0;at<size&&!error;) {
            ssize_t n=write(fd,desired+at,size-at);if(n<0&&errno==EINTR)continue;
            if(n<=0)error=n<0?errno:EIO;else at+=n;
        }
        if(!error&&fsync(fd)<0)error=errno;
        if(!error&&CheckChoices(root,expected)<0)error=errno;
        if(!error&&renameat(dir,"private-choices.new",dir,"private-choices")<0)error=errno;
        if(error)unlinkat(dir,"private-choices.new",0);
        close(fd);
    }
    if(!error&&fsync(dir)<0)error=errno;
    if(!error&&fsync(lib)<0)error=errno;
    if(dir>=0)close(dir);close(lib);return error?Fail(error):0;
}

}
extern "C" int aegis_package_guard_begin(int root,const aegis_package_execution_request* r,aegis_package_guard** out) {
    if(!out||*out||!r||!aegis_package_execution_valid(r)||r->review.present!=1)return Fail(EINVAL);
    std::string status,state;
    if(Read(root,"var/lib/dpkg/status",64u<<20,&status)<0)return -1;
    if(Hash(status)!=r->review.initial_status)return Fail(ESTALE);
    int read_state=Read(root,"var/lib/apt/extended_states",16u<<20,&state);
    if(r->review.apt_state_presence==1) {
        if(read_state==0)return Fail(ESTALE);if(errno!=ENOENT)return -1;
    } else {
        if(read_state<0)return -1;
        if(state.size()!=r->review.apt_state_bytes||Hash(state)!=r->review.initial_apt_state)return Fail(ESTALE);
    }
    if(CheckChoices(root,r->review.initial_choices)<0)return -1;
    auto* p=new(std::nothrow) aegis_package_guard;if(!p)return Fail(ENOMEM);p->request=*r;
    if(aegis::PackageReadInstalledRegistry(status,&p->expected)<0||aegis::PackageReadAutomaticRegistry(state,&p->automatic)<0) { int e=errno;delete p;return Fail(e); }
    if(MatchChoices(r->review.initial_choices,p->expected)<0) { int e=errno;delete p;return Fail(e); }
    for(auto it=p->automatic.begin();it!=p->automatic.end();) {
        if(!p->expected.count(*it))it=p->automatic.erase(it);else ++it;
    }
    if(r->kind==AEGIS_PACKAGE_RECONCILE) {
        std::vector<aegis::PackageAptEffect> effects;
        for(unsigned i=0;i<r->count;++i) {
            const auto& e=r->review.effects[i];effects.push_back({e.name,e.architecture,e.before,e.after,e.reason==2});
        }
        Installed expected;std::set<std::string> automatic;
        if(aegis::PackageReconciliationProject(p->expected,effects,r->review.reconciliation_roots,&expected,&automatic)<0) {
            int e=errno;delete p;return Fail(e);
        }
        if(Hash(aegis::PackageCanonicalInstalled(expected))!=r->review.result_registry
           ||Hash(aegis::PackageCanonicalAutomatic(automatic))!=r->review.result_automatic) { delete p;return Fail(ESTALE); }
        p->expected=std::move(expected);p->automatic=std::move(automatic);
    }
    for(unsigned i=0;r->kind!=AEGIS_PACKAGE_RECONCILE && i<r->count;++i) {
        const auto& e=r->review.effects[i];auto current=p->expected.find(e.name);
        if(!aegis::PackagePlanNameValid(e.name)||(*e.before&&!aegis::PackagePlanVersionValid(e.before))
           ||(*e.after&&!aegis::PackagePlanVersionValid(e.after))
           ||(*e.before?(current==p->expected.end()||std::get<0>(current->second)!=e.before||std::get<1>(current->second)!=e.architecture):current!=p->expected.end())) {
            delete p;return Fail(ESTALE);
        }
        if(!*e.after) { p->expected.erase(e.name);p->automatic.erase(e.name); }
        else {
            auto want=current==p->expected.end()?"install":std::get<2>(current->second);
            p->expected[e.name]={e.after,e.architecture,want};
            if(e.reason==2)p->automatic.insert(e.name);else p->automatic.erase(e.name);
        }
    }
    if(MatchChoices(r->review.result_choices,p->expected)<0) { int e=errno;delete p;return Fail(e); }
    *out=p;return 0;
}
extern "C" int aegis_package_guard_simulation(aegis_package_guard* p,int fd) {
    if(!p)return Fail(EINVAL);std::string json;
    if(ReadFd(fd,262144,&json)<0)return -1;
    const auto& r=p->request;std::vector<std::string> args;
    // APT 3.0.3 consumes local .deb paths before emitting the hook. With our
    // archive argv, only removal terms remain: bare names for remove, explicit
    // name- terms for mixed install. Every effect is compared to the review.
    for(unsigned i=0;i<r.count;++i)if(!aegis_package_has_archive(&r,i))
        args.push_back(std::string(r.items[i])+((r.kind==AEGIS_PACKAGE_MIXED||r.kind==AEGIS_PACKAGE_RECONCILE)?"-":""));
    std::vector<aegis::PackageAptEffect> actual;
    if(aegis::PackageReadAptOperation(json,r.kind==AEGIS_PACKAGE_REMOVE?"remove":"install",args,&actual)<0)return -1;
    if(actual.size()!=r.count)return Fail(ESTALE);
    for(size_t i=0;i<actual.size();++i) {
        const auto& a=actual[i];const auto& e=r.review.effects[i];
        if(a.name!=e.name||a.architecture!=e.architecture||a.before_version!=e.before||a.after_version!=e.after)return Fail(ESTALE);
    }
    // Explicit offline archive argv makes APT's simulation marks manual. The
    // sealed desired marks are applied after installation, then checked below.
    return 0;
}
extern "C" int aegis_package_guard_finish(aegis_package_guard* p,int root) {
    if(!p)return Fail(EINVAL);std::string status,state;Installed actual;std::set<std::string> automatic;
    if(Read(root,"var/lib/dpkg/status",64u<<20,&status)<0||aegis::PackageReadInstalledRegistry(status,&actual)<0)return -1;
    if(actual!=p->expected)return Fail(ESTALE);
    if(Read(root,"var/lib/apt/extended_states",16u<<20,&state)<0&&errno!=ENOENT)return -1;
    if(aegis::PackageReadAutomaticRegistry(state,&automatic)<0)return -1;
    for(auto it=automatic.begin();it!=automatic.end();) {
        if(!actual.count(*it))it=automatic.erase(it);else ++it;
    }
    if(automatic!=p->automatic)return Fail(ESTALE);
    return CheckChoices(root,p->request.review.initial_choices);
}
extern "C" int aegis_package_guard_reconcile_marks(aegis_package_guard* p,int root) {
    if(!p||p->request.kind!=AEGIS_PACKAGE_RECONCILE)return Fail(EINVAL);
    std::string status,old;Installed actual;std::set<std::string> old_marks;
    if(Read(root,"var/lib/dpkg/status",64u<<20,&status)<0
       ||aegis::PackageReadInstalledRegistry(status,&actual)<0)return -1;
    if(actual!=p->expected)return Fail(ESTALE);
    if(CheckChoices(root,p->request.review.initial_choices)<0)return -1;
    if(Read(root,"var/lib/apt/extended_states",16u<<20,&old)<0&&errno!=ENOENT)return -1;
    if(aegis::PackageReadAutomaticRegistry(old,&old_marks)<0)return -1;
    std::string state;
    for(const auto& name:p->automatic)state+="Package: "+name+"\nArchitecture: "+std::get<1>(actual.at(name))+"\nAuto-Installed: 1\n\n";
    int dir=Directory(root,"var/lib/apt");if(dir<0)return -1;
    struct stat st;int error=0;
    if(fstat(dir,&st)<0)error=errno;
    else if(st.st_uid||st.st_gid||(st.st_mode&07022))error=EPERM;
    int fd=error?-1:openat(dir,"extended_states.aegis-new",O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC|O_WRONLY,0644);
    if(fd<0&&!error)error=errno;
    if(fd>=0) {
        for(size_t at=0;at<state.size()&&!error;) {
            auto n=write(fd,state.data()+at,state.size()-at);if(n<0&&errno==EINTR)continue;
            if(n<=0)error=n<0?errno:EIO;else at+=n;
        }
        if(!error&&fsync(fd)<0)error=errno;
        if(!error&&renameat(dir,"extended_states.aegis-new",dir,"extended_states")<0)error=errno;
        if(error)unlinkat(dir,"extended_states.aegis-new",0);close(fd);
    }
    if(!error&&fsync(dir)<0)error=errno;close(dir);
    if(error)return Fail(error);
    return aegis_package_guard_finish(p,root);
}
extern "C" int aegis_package_guard_commit(aegis_package_guard* p,int root) {
    if(aegis_package_guard_finish(p,root)<0)return -1;
    if(p->request.kind==AEGIS_PACKAGE_RECONCILE)return 0; // retain private intent byte-for-byte
    return StoreChoices(root,p->request.review.initial_choices,p->request.review.result_choices);
}
extern "C" void aegis_package_guard_free(aegis_package_guard* p) { delete p; }
