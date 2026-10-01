#include "package_apt_plan.h"
#include <json/json.h>
#include <errno.h>
#include <algorithm>
#include <memory>
#include <set>
namespace aegis {
namespace {
int Fail(int error) { errno=error;return -1; }
bool Bounded(const std::string& text) {
    if(text.empty() || text.size()>262144)return false;
    unsigned depth=0;bool quoted=false,escaped=false;
    for(unsigned char c:text) {
        if(!c)return false;
        if(quoted) { if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false; }
        else if(c=='"')quoted=true;
        else if(c=='/')return false; // JsonCpp can otherwise skip trailing comments.
        else if(c=='['||c=='{') { if(++depth>12)return false; }
        else if(c==']'||c=='}') { if(!depth)return false;--depth; }
    }
    return !quoted && !depth;
}
bool Members(const Json::Value& v,std::initializer_list<const char*> allowed,
             std::initializer_list<const char*> required) {
    if(!v.isObject())return false;
    for(const auto& name:v.getMemberNames()) {
        bool found=false;for(const char* a:allowed)if(name==a)found=true;
        if(!found)return false;
    }
    for(const char* name:required)if(!v.isMember(name))return false;
    return true;
}
bool Text(const Json::Value& v,size_t limit) {
    if(!v.isString())return false;const auto s=v.asString();if(s.size()>limit)return false;
    for(unsigned char c:s)if(c<32 || c==127)return false;
    return true;
}
bool Number(const Json::Value& v) { return (v.type()==Json::uintValue || v.type()==Json::intValue)&&v.isUInt(); }
bool Version(const Json::Value& v,std::string* version,std::string* architecture) {
    if(!Members(v,{"id","version","architecture","pin","origins"},{"id","version","architecture","pin","origins"})
       || !Number(v["id"]) || !Text(v["version"],128) || !PackagePlanVersionValid(v["version"].asString())
       || !v["architecture"].isString() || (v["architecture"]!="all"&&v["architecture"]!="arm64")
       || (v["pin"].type()!=Json::intValue&&v["pin"].type()!=Json::uintValue) || !v["pin"].isInt()
       || !v["origins"].isArray() || v["origins"].size()>16)return false;
    for(const auto& origin:v["origins"]) {
        if(!Members(origin,{"archive","codename","version","origin","label","site"},{}))return false;
        for(const auto& name:origin.getMemberNames())if(!Text(origin[name],512))return false;
    }
    *version=v["version"].asString();*architecture=v["architecture"].asString();return true;
}
}
int PackageReadAptOperation(const std::string& json,const std::string& command,
                            const std::vector<std::string>& arguments,std::vector<PackageAptEffect>* output) {
    if(!output || !Bounded(json) || (command!="install" && command!="remove" && command!="upgrade")
       || arguments.size()>64)return Fail(EINVAL);
    for(const auto& arg:arguments) {
        if(arg.empty() || arg.size()>256)return Fail(EINVAL);
        for(unsigned char c:arg)if(c<33 || c>126)return Fail(EINVAL);
    }
    Json::CharReaderBuilder settings;settings["collectComments"]=false;settings["allowComments"]=false;
    settings["strictRoot"]=true;settings["failIfExtra"]=true;settings["rejectDupKeys"]=true;
    settings["allowTrailingCommas"]=false;settings["allowSpecialFloats"]=false;settings["skipBom"]=false;
    settings["stackLimit"]=32;
    std::unique_ptr<Json::CharReader> reader(settings.newCharReader());Json::Value root;std::string errors;
    if(!reader->parse(json.data(),json.data()+json.size(),&root,&errors)
       || !Members(root,{"jsonrpc","method","params"},{"jsonrpc","method","params"})
       || root["jsonrpc"]!="2.0" || root["method"]!="org.debian.apt.hooks.install.pre-prompt")return Fail(EBADMSG);
    const auto& p=root["params"];
    if(!Members(p,{"command","search-terms","unknown-packages","packages"},{"command","search-terms","unknown-packages","packages"})
       || p["command"]!=command || !p["search-terms"].isArray() || !p["unknown-packages"].isArray()
       || !p["unknown-packages"].empty() || !p["packages"].isArray() || p["packages"].size()>64)return Fail(EBADMSG);
    if(p["search-terms"].size()!=arguments.size())return Fail(ESTALE);
    for(size_t i=0;i<arguments.size();++i)if(p["search-terms"][static_cast<Json::ArrayIndex>(i)]!=arguments[i])return Fail(ESTALE);
    std::vector<PackageAptEffect> result;std::set<std::string> names;std::set<unsigned> ids;
    for(const auto& item:p["packages"]) {
        if(!Members(item,{"id","name","architecture","mode","automatic","versions"},{"id","name","architecture","mode","automatic","versions"})
           || !Number(item["id"]) || !ids.insert(item["id"].asUInt()).second || !Text(item["name"],128)
           || !PackagePlanNameValid(item["name"].asString()) || !names.insert(item["name"].asString()).second
           || (item["architecture"]!="arm64"&&item["architecture"]!="all") || !item["automatic"].isBool()
           || !item["mode"].isString() || !Members(item["versions"],{"current","candidate","install"},{}))return Fail(EBADMSG);
        auto mode=item["mode"].asString();const auto& versions=item["versions"];
        if(mode=="purge"||mode=="reinstall")return Fail(EOPNOTSUPP);
        if(mode!="install"&&mode!="upgrade"&&mode!="downgrade"&&mode!="deinstall")return Fail(EBADMSG);
        PackageAptEffect effect;effect.name=item["name"].asString();effect.automatic=item["automatic"].asBool();
        std::string current_arch,ignored_version,ignored_arch;
        if(versions.isMember("current") && !Version(versions["current"],&effect.before_version,&current_arch))return Fail(EBADMSG);
        if(versions.isMember("candidate") && !Version(versions["candidate"],&ignored_version,&ignored_arch))return Fail(EBADMSG);
        if(mode=="deinstall") {
            if(effect.before_version.empty() || versions.isMember("install"))return Fail(EBADMSG);
            effect.architecture=current_arch;
        } else {
            if(!versions.isMember("install") || !Version(versions["install"],&effect.after_version,&effect.architecture))return Fail(EBADMSG);
            // APT 3.0.3 reports an explicit already-installed selection as
            // mode=install with equal current/install versions (not reinstall).
            // Preserve this evidence. Only the resolver may classify it as a
            // metadata-only choice after checking the frozen installed registry.
            const bool retained=mode=="install" && !effect.before_version.empty()
                && effect.before_version==effect.after_version && current_arch==effect.architecture;
            if(!retained && (effect.before_version==effect.after_version
               || ((mode=="install")!=effect.before_version.empty())))return Fail(EBADMSG);
        }
        if(item["architecture"]=="all" && effect.architecture!="all")return Fail(EBADMSG);
        result.push_back(std::move(effect));
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.name<b.name;});
    *output=std::move(result);return 0;
}
int PackageReadAptPlan(const std::string& json,PackageAction action,const std::string& package,
                       const std::string& version,std::vector<PackageAptEffect>* output) {
    if(!output || !Bounded(json))return Fail(EINVAL);
    std::string command;
    if(action==PackageAction::Install)command="install";
    else if(action==PackageAction::Remove)command="remove";
    else if(action==PackageAction::Update)command="upgrade";
    else return Fail(EINVAL);
    if(action==PackageAction::Update ? !package.empty()||!version.empty()
       : !PackagePlanNameValid(package) || (!version.empty()&&!PackagePlanVersionValid(version))
         || (action==PackageAction::Remove&&!version.empty()))return Fail(EINVAL);
    std::vector<std::string> arguments;
    if(action!=PackageAction::Update)arguments.push_back(package+(version.empty()?"":"="+version));
    std::vector<PackageAptEffect> result;
    if(PackageReadAptOperation(json,command,arguments,&result)<0)return -1;
    bool requested=action==PackageAction::Update || result.empty();
    for(const auto& effect:result)if(effect.name==package) {
        if((action==PackageAction::Remove)!=effect.after_version.empty()
           || (!version.empty()&&effect.after_version!=version))return Fail(ESTALE);
        requested=true;
    }
    if(!requested)return Fail(ESTALE);
    *output=std::move(result);return 0;
}
}
