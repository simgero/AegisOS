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
