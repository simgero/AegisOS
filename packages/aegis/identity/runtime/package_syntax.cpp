#include "package_resolver.h"
#include <errno.h>
#include "package_plan.h"
namespace aegis {
bool PackagePlanNameValid(const std::string& s) {
    if(s.size()<2 || s.size()>128 || !((s[0]>='a'&&s[0]<='z')||(s[0]>='0'&&s[0]<='9')))return false;
    for(char c:s)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='+'||c=='-'||c=='.'))return false;
    return true;
}
bool PackagePlanVersionValid(const std::string& s) {
    if(s.empty() || s.size()>128)return false;
    size_t start=0,colon=s.find(':');
    if(colon!=std::string::npos) {
        if(!colon)return false;
        for(size_t i=0;i<colon;++i)if(s[i]<'0'||s[i]>'9')return false;
        start=colon+1;
    }
    if(start>=s.size() || s[start]<'0'||s[start]>'9')return false;
    for(size_t i=start;i<s.size();++i) {
        char c=s[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')
             ||c=='.'||c=='+'||c=='-'||c=='~'))return false;
    }
    // The bounded adapter accepts a strict subset of Debian versions; the final
    // hyphen introduces a revision and cannot terminate the version.
    return s.back()!='-';
}
}

namespace aegis {
int PackageResolverCheck(const PackageResolverRequest& r) {
    if(r.reconciliation && r.action!=PackageAction::Update)return (errno=EINVAL,-1);
    if(r.action==PackageAction::Update)return r.package.empty()&&r.version.empty()?0:(errno=EINVAL,-1);
    if(r.action!=PackageAction::Install&&r.action!=PackageAction::Remove)return (errno=EINVAL,-1);
    if(!PackagePlanNameValid(r.package)||r.package.back()=='+'||r.package.back()=='-'||(!r.version.empty()&&!PackagePlanVersionValid(r.version))
       ||(r.action==PackageAction::Remove&&!r.version.empty()))return (errno=EINVAL,-1);
    return 0;
}
}
