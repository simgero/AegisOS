#include "package_reconciliation.h"
#include "package_registry.h"
#include <errno.h>
namespace aegis {
namespace {
int Fail(int e) { errno=e;return -1; }
}
int PackageReconciliationDerive(const PackageReconciliationInput& in,PackageReconciliationGoals* output) {
    if(!output)return Fail(EINVAL);
    if(in.private_choices.empty())return Fail(ENODATA);
    PackageInstalledRegistry own,old,current;std::set<std::string> own_auto,old_auto,current_auto;
    PackagePrivateChoices choices;
    if(PackageReadInstalledRegistry(in.personal_status,&own)<0
       ||PackageReadInstalledRegistry(in.previous_status,&old)<0
       ||PackageReadInstalledRegistry(in.current_status,&current)<0
       ||PackageReadAutomaticRegistry(in.personal_automatic,&own_auto)<0
       ||PackageReadAutomaticRegistry(in.previous_automatic,&old_auto)<0
       ||PackageReadAutomaticRegistry(in.current_automatic,&current_auto)<0
       ||PackagePrivateChoicesDecode(in.private_choices,&choices)<0)return -1;
    for(const auto& [name,c]:choices) {
        auto it=own.find(name);
        if(it==own.end()||std::get<0>(it->second)!=c.version||std::get<1>(it->second)!=c.architecture)return Fail(ESTALE);
    }
    // A manual root absent from the old common base needs explicit provenance.
    // Do not silently reinterpret an unrecorded legacy private installation.
    for(const auto& [name,value]:own)
        if(!own_auto.count(name)&&!old.count(name)&&!choices.count(name))return Fail(ENODATA);
    PackageReconciliationGoals goals;
    for(const auto& [name,value]:current) {
        if(!current_auto.count(name))goals.roots[name]={std::get<1>(value),std::get<0>(value)};
    }
    for(const auto& [name,c]:choices)goals.roots[name]=c;
    if(goals.roots.empty())return Fail(ENODATA);
    if(goals.roots.size()>kPackageReconciliationRoots)return Fail(E2BIG);
    for(const auto& [name,c]:goals.roots)goals.arguments.push_back(name+"="+c.version);
    for(const auto& [name,value]:own)if(!goals.roots.count(name))
        goals.solver_automatic+="Package: "+name+"\nArchitecture: "+std::get<1>(value)+"\nAuto-Installed: 1\n\n";
    goals.solver_preferences="Package: *\nPin: version *\nPin-Priority: -1\n\n";
    auto preference=[&](const std::string& name,const std::string& version,int priority) {
        goals.solver_preferences+="Package: "+name+"\nPin: version "+version+
            "\nPin-Priority: "+std::to_string(priority)+"\n\n";
    };
    for(const auto& [name,value]:current)preference(name,std::get<0>(value),1001);
    for(const auto& [name,value]:own) {
        auto it=current.find(name);
        if(it==current.end()||std::get<0>(it->second)!=std::get<0>(value))
            preference(name,std::get<0>(value),100);
    }
    if(goals.solver_preferences.size()>(4u<<20))return Fail(E2BIG);
    *output=std::move(goals);return 0;
}
int PackageReconciliationCheckEffects(const PackageReconciliationInput& in,
    const PackageReconciliationGoals& goals,const std::vector<PackageAptEffect>& effects) {
    PackageReconciliationGoals expected;
    if(PackageReconciliationDerive(in,&expected)<0)return -1;
    if(goals.roots!=expected.roots||goals.arguments!=expected.arguments
       ||goals.solver_automatic!=expected.solver_automatic
       ||goals.solver_preferences!=expected.solver_preferences)return Fail(ESTALE);
    PackageInstalledRegistry installed;if(PackageReadInstalledRegistry(in.personal_status,&installed)<0)return -1;
    const auto original=installed;PackageInstalledRegistry current;
    if(PackageReadInstalledRegistry(in.current_status,&current)<0)return -1;
    if(effects.size()>AEGIS_PACKAGE_EXEC_ITEMS)return Fail(E2BIG);
    std::string previous;
    for(const auto& e:effects) {
        if(previous>=e.name||!PackagePlanNameValid(e.name)||(e.architecture!="all"&&e.architecture!="arm64")
           ||(!e.before_version.empty()&&!PackagePlanVersionValid(e.before_version))
           ||(!e.after_version.empty()&&!PackagePlanVersionValid(e.after_version))
           ||e.before_version==e.after_version)return Fail(EBADMSG);
        previous=e.name;auto it=installed.find(e.name);
        if(e.before_version.empty() ? it!=installed.end()
           : it==installed.end()||std::get<0>(it->second)!=e.before_version||std::get<1>(it->second)!=e.architecture)return Fail(ESTALE);
        if(it!=installed.end()&&std::get<2>(it->second)=="hold")return Fail(EDEADLK);
        if(!e.after_version.empty()) {
            auto allowed=[&](const PackageInstalledRegistry& registry) {
                auto v=registry.find(e.name);
                return v!=registry.end()&&std::get<0>(v->second)==e.after_version
                    &&std::get<1>(v->second)==e.architecture;
            };
            // The repository may have advanced since the admin published the
            // common image. A rebase cannot authorize those additional versions.
            if(!allowed(original)&&!allowed(current))return Fail(EPERM);
        }
        if(e.after_version.empty())installed.erase(e.name);
        else installed[e.name]={e.after_version,e.architecture,"install"};
    }
    for(const auto& [name,c]:goals.roots) {
        auto it=installed.find(name);
        if(it==installed.end()||std::get<0>(it->second)!=c.version||std::get<1>(it->second)!=c.architecture)return Fail(EDEADLK);
    }
    return 0;
}
int PackageReconciliationRootsRead(const std::string& text,std::set<std::string>* output) {
    if(!output||text.empty()||text.size()>=AEGIS_PACKAGE_RECONCILIATION_ROOT_BYTES)return Fail(EINVAL);
    std::set<std::string> names;std::string previous;
    for(size_t at=0;at<text.size();) {
        auto end=text.find('\n',at);if(end==std::string::npos)return Fail(EBADMSG);
        auto name=text.substr(at,end-at);at=end+1;
        if(!PackagePlanNameValid(name)||name<=previous)return Fail(EBADMSG);
        previous=name;names.insert(name);if(names.size()>kPackageReconciliationRoots)return Fail(E2BIG);
    }
    *output=std::move(names);return 0;
}
int PackageReconciliationProject(const PackageInstalledRegistry& initial,
    const std::vector<PackageAptEffect>& effects,const std::string& roots,
    PackageInstalledRegistry* output,std::set<std::string>* automatic) {
    if(!output||!automatic)return Fail(EINVAL);
    std::set<std::string> names;if(PackageReconciliationRootsRead(roots,&names)<0)return -1;
    if(effects.size()>AEGIS_PACKAGE_EXEC_ITEMS)return Fail(E2BIG);
    auto installed=initial;std::string previous;
    for(const auto& e:effects) {
        if(e.name<=previous||!PackagePlanNameValid(e.name)||(e.architecture!="all"&&e.architecture!="arm64")
           ||(!e.before_version.empty()&&!PackagePlanVersionValid(e.before_version))
           ||(!e.after_version.empty()&&!PackagePlanVersionValid(e.after_version))
           ||e.before_version==e.after_version)return Fail(EBADMSG);
        previous=e.name;auto it=installed.find(e.name);
        if(e.before_version.empty()?it!=installed.end():it==installed.end()
           ||std::get<0>(it->second)!=e.before_version||std::get<1>(it->second)!=e.architecture)return Fail(ESTALE);
        if(it!=installed.end()&&std::get<2>(it->second)=="hold")return Fail(EDEADLK);
        if(e.after_version.empty())installed.erase(e.name);
        else installed[e.name]={e.after_version,e.architecture,"install"};
    }
    for(const auto& name:names)if(!installed.count(name))return Fail(ESTALE);
    std::set<std::string> marks;
    for(const auto& [name,value]:installed)if(!names.count(name))marks.insert(name);
    *output=std::move(installed);*automatic=std::move(marks);return 0;
}

}
