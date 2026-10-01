#include "package_registry.h"
#include "package_apt_plan.h"
#include <errno.h>
#include <utility>
namespace aegis {
namespace {
int Fail(int e) { errno=e;return -1; }
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
}
int PackageReadInstalledRegistry(const std::string& text,PackageInstalledRegistry* out) {
    if(!out)return Fail(EINVAL);
    PackageInstalledRegistry data;std::set<std::string> names;
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
int PackageReadAutomaticRegistry(const std::string& text,std::set<std::string>* out) {
    if(!out)return Fail(EINVAL);
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
std::string PackageCanonicalInstalled(const PackageInstalledRegistry& registry) {
    std::string out="AEGIS-INSTALLED1\n";
    for(const auto& [name,p]:registry)out+=name+"\t"+std::get<1>(p)+"\t"+std::get<0>(p)+"\t"+std::get<2>(p)+"\n";
    return out;
}
std::string PackageCanonicalAutomatic(const std::set<std::string>& names) {
    std::string out="AEGIS-AUTOMATIC1\n";for(const auto& n:names)out+=n+"\n";return out;
}

}
