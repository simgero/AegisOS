#include "package_guard.h"
#include "package_apt_plan.h"
#include <openssl/sha.h>
#include <errno.h>
#include <fcntl.h>
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
using Installed=std::map<std::string,std::tuple<std::string,std::string,std::string>>;
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
using Fields=std::map<std::string,std::string>;
// Debian status/extended_states, not terminal text. Unknown folded fields are
// bounded and ignored; selected identity/state fields are single-line only.
template<class Consumer> int Records(const std::string& text,Consumer consume) {
    Fields fields;std::set<std::string> seen;std::string previous;size_t stanza=0,count=0;
    auto finish=[&]() {
        if(seen.empty())return 0;
        if(++count>32768)return Fail(E2BIG);
        int result=consume(fields);fields.clear();seen.clear();previous.clear();stanza=0;return result;
    };
    for(size_t at=0;at<text.size();) {
        auto end=text.find('\n',at);if(end==std::string::npos)return Fail(EBADMSG);
        auto line=text.substr(at,end-at);at=end+1;
        if(line.size()>65536 || (stanza+=line.size()+1)>1048576)return Fail(EFBIG);
        for(unsigned char c:line)if(!c||c=='\r'||(c<32&&c!='\t')||c==127)return Fail(EBADMSG);
        if(line.empty()) { if(finish()<0)return -1;continue; }
        if(line[0]==' '||line[0]=='\t') {
            if(previous.empty()||fields.count(previous))return Fail(EBADMSG);
            continue;
        }
        auto colon=line.find(':');if(colon==std::string::npos||!colon||colon>128)return Fail(EBADMSG);
        auto key=line.substr(0,colon);
        for(char& c:key) { if(c<=32||c>=127)return Fail(EBADMSG);if(c>='A'&&c<='Z')c+=32; }
        if(!seen.insert(key).second||seen.size()>256)return Fail(EBADMSG);
        previous=key;
        if(key=="package"||key=="architecture"||key=="version"||key=="status"||key=="auto-installed") {
            auto value=line.substr(colon+1);auto a=value.find_first_not_of(" \t"),z=value.find_last_not_of(" \t");
            if(a==std::string::npos)return Fail(EBADMSG);value=value.substr(a,z-a+1);
            if(value.size()>128)return Fail(EBADMSG);fields.emplace(key,std::move(value));
        }
    }
    return finish();
}
int Status(const std::string& text,Installed* out) {
    Installed data;std::set<std::string> names;
    int result=Records(text,[&](const Fields& f) {
        auto name=f.find("package"),status=f.find("status"),version=f.find("version"),arch=f.find("architecture");
        if(name==f.end()||status==f.end()||!aegis::PackagePlanNameValid(name->second)
           ||!names.insert(name->second).second)return Fail(EBADMSG);
        auto first=status->second.find(' '),last=status->second.rfind(' ');
        if(first==std::string::npos||first==last||status->second.substr(first+1,last-first-1)!="ok")return Fail(EBADMSG);
        auto want=status->second.substr(0,first),state=status->second.substr(last+1);
        if(want!="install"&&want!="deinstall"&&want!="purge"&&want!="hold"&&want!="unknown")return Fail(EBADMSG);
        if(state=="config-files"||state=="not-installed")return 0;
        if(state!="installed"||version==f.end()||arch==f.end()
           ||!aegis::PackagePlanVersionValid(version->second)||(arch->second!="all"&&arch->second!="arm64"))return Fail(EBADMSG);
        data.emplace(name->second,std::make_tuple(version->second,arch->second,want));return 0;
    });
    if(result<0)return -1;*out=std::move(data);return 0;
}
int Auto(const std::string& text,std::set<std::string>* out) {
    std::set<std::string> names,data;
    int result=Records(text,[&](const Fields& f) {
        auto name=f.find("package"),arch=f.find("architecture"),flag=f.find("auto-installed");
        if(name==f.end()||!aegis::PackagePlanNameValid(name->second)||!names.insert(name->second).second
           ||(arch!=f.end()&&arch->second!="all"&&arch->second!="arm64")
           ||flag==f.end()||(flag->second!="0"&&flag->second!="1"))return Fail(EBADMSG);
        if(flag->second=="1")data.insert(name->second);return 0;
    });
    if(result<0)return -1;*out=std::move(data);return 0;
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
    auto* p=new(std::nothrow) aegis_package_guard;if(!p)return Fail(ENOMEM);p->request=*r;
    if(Status(status,&p->expected)<0||Auto(state,&p->automatic)<0) { int e=errno;delete p;return Fail(e); }
    for(auto it=p->automatic.begin();it!=p->automatic.end();) {
        if(!p->expected.count(*it))it=p->automatic.erase(it);else ++it;
    }
    for(unsigned i=0;i<r->count;++i) {
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
    *out=p;return 0;
}
extern "C" int aegis_package_guard_simulation(aegis_package_guard* p,int fd) {
    if(!p)return Fail(EINVAL);std::string json;
    if(ReadFd(fd,262144,&json)<0)return -1;
    const auto& r=p->request;std::vector<std::string> args;
    for(unsigned i=0;i<r.count;++i)args.push_back(r.kind==AEGIS_PACKAGE_ARCHIVES?std::string("/var/cache/apt/archives/")+r.items[i]:r.items[i]);
    std::vector<aegis::PackageAptEffect> actual;
    if(aegis::PackageReadAptOperation(json,r.kind==AEGIS_PACKAGE_ARCHIVES?"install":"remove",args,&actual)<0)return -1;
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
    if(Read(root,"var/lib/dpkg/status",64u<<20,&status)<0||Status(status,&actual)<0)return -1;
    if(actual!=p->expected)return Fail(ESTALE);
    if(Read(root,"var/lib/apt/extended_states",16u<<20,&state)<0&&errno!=ENOENT)return -1;
    if(Auto(state,&automatic)<0)return -1;
    for(auto it=automatic.begin();it!=automatic.end();) {
        if(!actual.count(*it))it=automatic.erase(it);else ++it;
    }
    return automatic==p->automatic?0:Fail(ESTALE);
}
extern "C" void aegis_package_guard_free(aegis_package_guard* p) { delete p; }
