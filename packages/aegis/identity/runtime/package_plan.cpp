#include "package_plan.h"
#include "package_execution_protocol.h"
#include <errno.h>
#include <openssl/sha.h>
#include <algorithm>
#include <utility>
namespace aegis {
namespace {
int Fail(int error) { errno=error;return -1; }
constexpr char EmptySha256[]="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
bool Hash(const std::string& s) { return s.size()==64 && aegis_package_hash(s.c_str()); }
bool Empty(const PackageInput& p) { return p.bytes==0 && p.sha256.empty(); }
bool Empty(const PackageGeneration& p) {
    return !p.bytes && p.image_sha256.empty() && p.shared_base_sha256.empty();
}
bool Image(const PackageInput& p) {
    return p.bytes && p.bytes<=(uint64_t{32}<<30) && !(p.bytes%4096) && Hash(p.sha256);
}
bool Same(const PackageInput& a,const PackageInput& b) {
    return a.bytes==b.bytes && a.sha256==b.sha256;
}
std::string Archive(const PackageChange& c) {
    // APT 3.0.3 pkgAcqArchive + QuoteString: epoch ':' is escaped as lowercase
    // %3a. Other accepted package/version characters are already unchanged.
    std::string version;
    for(char b:c.after_version)if(b==':')version+="%3a";else version+=b;
    return c.name+"_"+version+"_"+c.architecture+".deb";
}
struct Encoding {
    std::string data;
    void Number(uint64_t n) { for(int shift=56;shift>=0;shift-=8)data.push_back(char((n>>shift)&255)); }
    void Text(const std::string& s) { Number(s.size());data+=s; }
    void Input(const PackageInput& p) { Number(p.bytes);Text(p.sha256); }
    void Generation(const PackageGeneration& p) { Number(p.bytes);Text(p.image_sha256);Text(p.shared_base_sha256); }
    std::string Digest() const {
        unsigned char bytes[SHA256_DIGEST_LENGTH];
        if(!SHA256(reinterpret_cast<const unsigned char*>(data.data()),data.size(),bytes))return {};
        static constexpr char hex[]="0123456789abcdef";
        std::string result;result.reserve(64);
        for(unsigned char c:bytes) { result+=hex[c>>4];result+=hex[c&15]; }
        return result;
    }
};
}
int PackageBindResolvedPlan(const PackageResolvedPlan& p,uint64_t now,PackageBoundPlan* output) {
    if(!output || !now || now>INT64_MAX || p.requester<10 || p.requester>=21473 || p.serial>INT32_MAX
       || !Image(p.source) || !Image(p.shared) || !Hash(p.planner_image_sha256)
       || !Hash(p.policy_sha256) || !Hash(p.initial_status_sha256)
       || p.repositories.size()>16 || p.changes.size()>AEGIS_PACKAGE_EXEC_ITEMS
       || (p.action!=PackageAction::Install && p.action!=PackageAction::Update && p.action!=PackageAction::Remove))return Fail(EINVAL);
    if(p.initial_apt_state_presence==PackageStatePresence::Present) {
        if(p.initial_apt_state.bytes>(uint64_t{16}<<20) || !Hash(p.initial_apt_state.sha256)
           || (!p.initial_apt_state.bytes && p.initial_apt_state.sha256!=EmptySha256))return Fail(EINVAL);
    } else if(p.initial_apt_state_presence!=PackageStatePresence::Absent || !Empty(p.initial_apt_state))return Fail(EINVAL);
    if(p.action==PackageAction::Update) {
        if(!p.requested_package.empty() || !p.requested_version.empty())return Fail(EINVAL);
    } else if(!PackagePlanNameValid(p.requested_package) || (!p.requested_version.empty() && !PackagePlanVersionValid(p.requested_version))
              || (p.action==PackageAction::Remove && !p.requested_version.empty()))return Fail(EINVAL);
    if(p.has_previous) {
        if(p.create_store || p.previous.bytes!=p.source.bytes || p.previous.image_sha256!=p.source.sha256
           || (p.personal ? !Hash(p.previous.shared_base_sha256) : !p.previous.shared_base_sha256.empty()))return Fail(EINVAL);
        if(p.personal && p.previous.shared_base_sha256!=p.shared.sha256)return Fail(ESTALE);
    } else if(!Empty(p.previous) || !Same(p.source,p.shared))return Fail(EINVAL);
    if(!p.personal && !Same(p.source,p.shared))return Fail(EINVAL);
    if(p.changes.empty())return Fail(EALREADY); // No job/approval for a no-op.
    PackageBoundPlan bound;auto& prep=bound.preparation;auto& exec=prep.execution;auto& pub=bound.publication;
    exec.requester=p.requester;exec.serial=p.serial;
    prep.image=p.source;exec.review.present=1;
    exec.review.apt_state_presence=static_cast<uint32_t>(p.initial_apt_state_presence);
    exec.review.apt_state_bytes=p.initial_apt_state.bytes;
    memcpy(exec.review.initial_status,p.initial_status_sha256.c_str(),65);
    if(p.initial_apt_state_presence==PackageStatePresence::Present)
        memcpy(exec.review.initial_apt_state,p.initial_apt_state.sha256.c_str(),65);
    pub.requester=p.requester;pub.serial=p.serial;pub.personal=p.personal;pub.create=p.create_store;
    pub.has_previous=p.has_previous;pub.previous=p.previous;pub.derive_source_hash=true;
    pub.candidate.bytes=p.source.bytes;if(p.personal)pub.candidate.shared_base_sha256=p.shared.sha256;
    Encoding e;e.Text("org.aegisos.package.resolved-plan");e.Number(2);
    e.Number(p.requester);e.Number(p.serial);e.Number(p.personal);e.Number(p.create_store);e.Number(p.has_previous);
    e.Number(static_cast<uint32_t>(p.action));e.Text(p.requested_package);e.Text(p.requested_version);
    e.Input(p.source);e.Input(p.shared);e.Generation(p.previous);
    e.Text(p.planner_image_sha256);e.Text(p.policy_sha256);e.Text(p.initial_status_sha256);
    e.Number(static_cast<uint32_t>(p.initial_apt_state_presence));e.Input(p.initial_apt_state);
    e.Number(p.repositories.size());std::string previous;
    for(const auto& repo:p.repositories) {
        if(!PackagePlanNameValid(repo.id) || (!previous.empty() && previous>=repo.id) || !Hash(repo.release_sha256)
           || !Hash(repo.index_sha256) || !repo.valid_until_unix || repo.valid_until_unix>INT64_MAX)return Fail(EINVAL);
        if(repo.valid_until_unix<=now)return Fail(ESTALE);
        previous=repo.id;
        if(!bound.valid_until_unix || repo.valid_until_unix<bound.valid_until_unix)bound.valid_until_unix=repo.valid_until_unix;
        e.Text(repo.id);e.Text(repo.release_sha256);e.Text(repo.index_sha256);e.Number(repo.valid_until_unix);
    }
    e.Number(p.changes.size());previous.clear();uint64_t archive_total=0;bool requested=p.action==PackageAction::Update;
    for(const auto& c:p.changes) {
        if(!PackagePlanNameValid(c.name) || (!previous.empty() && previous>=c.name) || (c.architecture!="arm64"&&c.architecture!="all")
           || (!c.before_version.empty() && !PackagePlanVersionValid(c.before_version))
           || (!c.after_version.empty() && !PackagePlanVersionValid(c.after_version))
           || c.before_version==c.after_version
           || (c.reason!=PackageInstallReason::Manual && c.reason!=PackageInstallReason::Automatic))return Fail(EINVAL);
        previous=c.name;
        if(!c.after_version.empty()) {
            if(!c.archive.bytes || c.archive.bytes>(uint64_t{2}<<30) || !Hash(c.archive.sha256))return Fail(EINVAL);
            archive_total+=c.archive.bytes;if(archive_total>(uint64_t{8}<<30))return Fail(EINVAL);
            if(!PackagePlanNameValid(c.repository) || !std::any_of(p.repositories.begin(),p.repositories.end(),
                 [&](const auto& repo){return repo.id==c.repository;}))return Fail(EINVAL);
            exec.items.push_back(Archive(c));prep.archives.push_back(c.archive);
        } else {
            if(!c.repository.empty() || !Empty(c.archive))return Fail(EINVAL);
            exec.items.push_back(c.name);
        }
        if(c.name==p.requested_package) {
            if((p.action==PackageAction::Remove)!=c.after_version.empty())return Fail(EINVAL);
            if(!p.requested_version.empty() && c.after_version!=p.requested_version)return Fail(ESTALE);
            requested=true;
        }
        e.Text(c.name);e.Text(c.architecture);e.Text(c.before_version);e.Text(c.after_version);
        e.Number(static_cast<uint32_t>(c.reason));e.Text(c.repository);e.Input(c.archive);
        auto& expected=exec.review.effects[exec.items.size()-1];
        memcpy(expected.name,c.name.c_str(),c.name.size()+1);
        memcpy(expected.architecture,c.architecture.c_str(),c.architecture.size()+1);
        memcpy(expected.before,c.before_version.c_str(),c.before_version.size()+1);
        memcpy(expected.after,c.after_version.c_str(),c.after_version.size()+1);
        expected.reason=static_cast<uint8_t>(c.reason);
    }
    if(!requested)return Fail(EINVAL);
    exec.kind=prep.archives.empty()?AEGIS_PACKAGE_REMOVE
        :prep.archives.size()==exec.items.size()?AEGIS_PACKAGE_ARCHIVES:AEGIS_PACKAGE_MIXED;
    std::string digest=e.Digest();if(digest.empty())return Fail(EIO);
    exec.plan_sha256=digest;pub.plan_sha256=digest;
    // Reuse the real protocol limits/validation, including total archive bytes,
    // canonical filename length and action-override suffixes on remove names.
    exec.job=1;pub.job=1;
    if(PackagePreparationCheck(prep)<0 || PackagePublicationCheck(pub)<0)return -1;
    exec.job=0;pub.job=0;bound.reviewed=p;*output=std::move(bound);return 0;
}
} // namespace aegis
