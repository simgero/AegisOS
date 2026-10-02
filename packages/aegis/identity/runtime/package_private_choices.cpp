#include "package_private_choices.h"
#include "package_registry.h"
#include <errno.h>
namespace aegis {
namespace {
constexpr char Header[]="AEGIS-PRIVATE-CHOICES1\n";
int Fail(int e) { errno=e;return -1; }
bool Valid(const std::string& name,const PackagePrivateChoice& c) {
    return PackagePlanNameValid(name) && (c.architecture=="all" || c.architecture=="arm64")
        && PackagePlanVersionValid(c.version);
}
}
int PackagePrivateChoicesEncode(const PackagePrivateChoices& choices,std::string* output) {
    if(!output)return Fail(EINVAL);if(choices.size()>AEGIS_PACKAGE_EXEC_ITEMS)return Fail(E2BIG);
    std::string text=Header;
    for(const auto& [name,c]:choices) {
        if(!Valid(name,c))return Fail(EINVAL);
        text+=name+"\t"+c.architecture+"\t"+c.version+"\n";
    }
    if(text.size()>=AEGIS_PACKAGE_CHOICES_BYTES)return Fail(E2BIG);
    *output=std::move(text);return 0;
}
int PackagePrivateChoicesDecode(const std::string& text,PackagePrivateChoices* output) {
    if(!output)return Fail(EINVAL);
    if(text.size()>=AEGIS_PACKAGE_CHOICES_BYTES)return Fail(E2BIG);
    PackagePrivateChoices data;
    if(text.empty()) { *output=std::move(data);return 0; }
    if(text.compare(0,sizeof(Header)-1,Header))return Fail(EBADMSG);
    std::string previous;
    for(size_t at=sizeof(Header)-1;at<text.size();) {
        auto end=text.find('\n',at),a=text.find('\t',at);
        if(end==std::string::npos || a==std::string::npos || a>=end)return Fail(EBADMSG);
        auto b=text.find('\t',a+1);if(b==std::string::npos || b>=end)return Fail(EBADMSG);
        std::string name=text.substr(at,a-at);
        PackagePrivateChoice c{text.substr(a+1,b-a-1),text.substr(b+1,end-b-1)};
        if(previous>=name || !Valid(name,c))return Fail(EBADMSG);
        if(data.size()==AEGIS_PACKAGE_EXEC_ITEMS)return Fail(E2BIG);
        previous=name;data.emplace(name,std::move(c));at=end+1;
    }
    *output=std::move(data);return 0;
}
int PackagePrivateChoicesRemove(const std::string& before,const std::string& requested,std::string* after) {
    if(!after || !PackagePlanNameValid(requested))return Fail(EINVAL);
    if(before.empty())return Fail(ENODATA);
    PackagePrivateChoices choices;if(PackagePrivateChoicesDecode(before,&choices)<0)return -1;
    if(!choices.erase(requested))return Fail(ENOENT);
    return PackagePrivateChoicesEncode(choices,after);
}
int PackageSameVersionSelection(const std::string& status,const std::string& automatic,
    const std::string& requested,const std::string& version,PackageChange* output) {
    if(!output || !PackagePlanNameValid(requested) || (!version.empty()&&!PackagePlanVersionValid(version)))return Fail(EINVAL);
    PackageInstalledRegistry installed;std::set<std::string> marks;
    if(PackageReadInstalledRegistry(status,&installed)<0 || PackageReadAutomaticRegistry(automatic,&marks)<0)return -1;
    auto found=installed.find(requested);if(found==installed.end())return Fail(ENOENT);
    const auto& [current,architecture,want]=found->second;
    if(!version.empty()&&version!=current)return Fail(ESTALE);
    *output={requested,architecture,current,current,"",{},PackageInstallReason::Manual};return 0;
}
int PackagePrivateChoicesApply(const std::string& before,PackageAction action,
    const std::string& requested,const std::string& version,const std::vector<PackageChange>& changes,std::string* after) {
    if(!after || (action!=PackageAction::Install && action!=PackageAction::Update && action!=PackageAction::Remove))return Fail(EINVAL);
    if(action==PackageAction::Update ? !requested.empty() || !version.empty()
       : !PackagePlanNameValid(requested) || (!version.empty()&&!PackagePlanVersionValid(version))
         || (action==PackageAction::Remove&&!version.empty()))return Fail(EINVAL);
    PackagePrivateChoices choices;if(PackagePrivateChoicesDecode(before,&choices)<0)return -1;
    bool matched=action==PackageAction::Update;std::string previous;
    for(const auto& c:changes) {
        if(previous>=c.name || !PackagePlanNameValid(c.name) || (c.architecture!="arm64"&&c.architecture!="all")
           || (!c.before_version.empty()&&!PackagePlanVersionValid(c.before_version))
           || (!c.after_version.empty()&&!PackagePlanVersionValid(c.after_version))
           || (c.before_version.empty()&&c.after_version.empty())
           || (c.reason!=PackageInstallReason::Manual&&c.reason!=PackageInstallReason::Automatic))return Fail(EINVAL);
        previous=c.name;auto old=choices.find(c.name);
        if(old!=choices.end() && (old->second.version!=c.before_version || old->second.architecture!=c.architecture))return Fail(ESTALE);
        bool target=c.name==requested;
        if(target) {
            if((action==PackageAction::Remove)!=c.after_version.empty()
               || (!version.empty()&&c.after_version!=version))return Fail(ESTALE);
            matched=true;
        }
        bool select=target || old!=choices.end() || (action==PackageAction::Update && c.reason==PackageInstallReason::Manual);
        if(c.after_version.empty())choices.erase(c.name);
        else if(select)choices[c.name]={c.architecture,c.after_version};
    }
    if(!matched)return Fail(ENOENT);
    return PackagePrivateChoicesEncode(choices,after);
}
}
