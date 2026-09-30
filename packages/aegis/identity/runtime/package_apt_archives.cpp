#include "package_apt_archives.h"
#include "package_execution_protocol.h"
#include <openssl/sha.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <array>
#include <algorithm>
#include <utility>
#include <functional>
#include <map>
#include <set>
namespace aegis {
namespace {
int Fail(int error) { errno=error;return -1; }
bool Hash(const std::string& s) { return s.size()==64 && aegis_package_hash(s.c_str()); }
std::string Hex(const unsigned char* bytes) {
    const char hex[]="0123456789abcdef";std::string result;
    for(unsigned i=0;i<SHA256_DIGEST_LENGTH;++i) { result+=hex[bytes[i]>>4];result+=hex[bytes[i]&15]; }return result;
}
bool Stable(const struct stat& a,const struct stat& b) {
    return a.st_dev==b.st_dev && a.st_ino==b.st_ino && a.st_size==b.st_size && a.st_mode==b.st_mode
      && a.st_mtim.tv_sec==b.st_mtim.tv_sec && a.st_mtim.tv_nsec==b.st_mtim.tv_nsec
      && a.st_ctim.tv_sec==b.st_ctim.tv_sec && a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
}
int Describe(int fd,uint64_t maximum,struct stat* st) {
    int flags=fcntl(fd,F_GETFL);if(flags<0 || fstat(fd,st)<0)return -1;
    if((flags&O_ACCMODE)!=O_RDONLY || (flags&O_PATH) || !S_ISREG(st->st_mode)
       || st->st_size<0 || uint64_t(st->st_size)>maximum)return Fail(EINVAL);
    return 0;
}
int Stream(int fd,const struct stat& before,const std::string& expected,
           const std::function<int(const char*,size_t)>& consume) {
    SHA256_CTX context;if(!SHA256_Init(&context))return Fail(EIO);
    std::array<char,65536> buffer;
    for(off_t offset=0;offset<before.st_size;) {
        size_t wanted=std::min<off_t>(buffer.size(),before.st_size-offset);
        ssize_t got=pread(fd,buffer.data(),wanted,offset);if(got<0&&errno==EINTR)continue;
        if(got<0)return -1;if(!got)return Fail(ESTALE);
        if(!SHA256_Update(&context,buffer.data(),got))return Fail(EIO);
        if(consume(buffer.data(),got)<0)return -1;offset+=got;
    }
    unsigned char hash[SHA256_DIGEST_LENGTH];if(!SHA256_Final(hash,&context))return Fail(EIO);
    struct stat after;if(fstat(fd,&after)<0)return -1;
    if(!Stable(before,after))return Fail(ESTALE);
    if(Hex(hash)!=expected)return Fail(EBADMSG);
    return 0;
}
bool Relative(const std::string& path) {
    if(path.empty()||path.size()>1024||path.front()=='/'||path.back()=='/'||path.size()<5
       ||path.compare(path.size()-4,4,".deb"))return false;
    std::string component;
    for(char c:path+"/") {
        if(c=='/') { if(component.empty()||component=="."||component=="..")return false;component.clear(); }
        else {
            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')
                 ||c=='.'||c=='+'||c=='-'||c=='_'||c=='~')) {
                return false;
            }
            component+=c;
        }
    }
    return true;
}
bool Size(const std::string& text,uint64_t* result) {
    if(text.empty()||text.size()>10||text.front()=='0')return false;
    uint64_t value=0;for(char c:text) { if(c<'0'||c>'9')return false;value=value*10+c-'0'; }
    if(!value||value>(uint64_t{2}<<30))return false;*result=value;return true;
}
std::string Trim(const std::string& s) {
    auto a=s.find_first_not_of(" \t"),z=s.find_last_not_of(" \t");
    return a==std::string::npos?"":s.substr(a,z-a+1);
}
struct IndexReader {
    const std::vector<PackageAptEffect>& effects;
    std::vector<PackageAptArchive>& found;
    const std::map<std::string,size_t>& names;
    const std::string& repository;
    std::set<std::string> fields;std::map<std::string,std::string> selected;
    std::set<size_t> matches;std::string line,previous;size_t stanza_size=0;
    bool selected_field=false;
    bool Relevant(const std::string& key) {
        return key=="package"||key=="version"||key=="architecture"||key=="filename"||key=="size"||key=="sha256";
    }
    int Finish() {
        if(fields.empty())return 0;
        for(const char* key:{"package","version","architecture"})if(!selected.count(key))return Fail(EBADMSG);
        auto named=names.find(selected["package"]);
        if(named!=names.end()) {
            size_t at=named->second;const auto& e=effects[at];
            if(!e.after_version.empty() && selected["version"]==e.after_version && selected["architecture"]==e.architecture) {
                if(!matches.insert(at).second)return Fail(EEXIST);
                uint64_t size=0;
                if(!Relative(selected["filename"]) || !Size(selected["size"],&size) || !Hash(selected["sha256"]))return Fail(EBADMSG);
                auto& archive=found[at];
                if(!archive.repository.empty()) {
                    if(archive.archive.bytes!=size || archive.archive.sha256!=selected["sha256"])return Fail(EEXIST);
                } else { archive.repository=repository;archive.filename=selected["filename"];archive.archive={size,selected["sha256"]}; }
            }
        }
        fields.clear();selected.clear();previous.clear();selected_field=false;stanza_size=0;return 0;
    }
    int Line() {
        if(line.empty())return Finish();
        stanza_size+=line.size()+1;if(stanza_size>1048576)return Fail(EFBIG);
        if(line[0]==' '||line[0]=='\t') {
            if(previous.empty()||selected_field||Trim(line).empty())return Fail(EBADMSG);
            return 0;
        }
        auto colon=line.find(':');if(colon==std::string::npos||!colon||colon>128||line[0]=='#'||line[0]=='-')return Fail(EBADMSG);
        std::string key=line.substr(0,colon);
        for(char& c:key) { if(c<=32||c>=127)return Fail(EBADMSG);if(c>='A'&&c<='Z')c+=32; }
        if(!fields.insert(key).second || fields.size()>256)return Fail(EBADMSG);
        auto value=Trim(line.substr(colon+1));selected_field=Relevant(key);previous=key;
        if(selected_field) { if(value.empty()||value.size()>1024)return Fail(EBADMSG);selected.emplace(key,std::move(value)); }
        return 0;
    }
    int Bytes(const char* data,size_t size) {
        for(size_t i=0;i<size;++i) {
            unsigned char c=data[i];if(!c||c=='\r'||(c<32&&c!='\n'&&c!='\t')||c==127)return Fail(EBADMSG);
            if(c=='\n') { if(Line()<0)return -1;line.clear(); }
            else { if(line.size()>=65536)return Fail(EFBIG);line+=char(c); }
        }
        return 0;
    }
    int End() { if(!line.empty()) { if(Line()<0)return -1;line.clear(); }return Finish(); }
};
}
int PackageMatchAptArchives(const std::vector<PackageAptEffect>& effects,
    const std::vector<PackageAptIndex>& indexes,uint64_t now,std::vector<PackageAptArchive>* output) {
    if(!output||!now||now>INT64_MAX||effects.size()>64||indexes.size()>16)return Fail(EINVAL);
    if(effects.empty())return Fail(EALREADY);
    std::string previous;std::map<std::string,size_t> names;std::vector<PackageAptArchive> found;
    for(const auto& e:effects) {
        if(!PackagePlanNameValid(e.name)||(!previous.empty()&&previous>=e.name)
           ||(e.architecture!="all"&&e.architecture!="arm64")||e.before_version==e.after_version
           ||(!e.before_version.empty()&&!PackagePlanVersionValid(e.before_version))
           ||(!e.after_version.empty()&&!PackagePlanVersionValid(e.after_version)))return Fail(EINVAL);
        previous=e.name;names.emplace(e.name,found.size());found.push_back({e,"","",{}});
    }
    previous.clear();uint64_t total=0;
    for(const auto& index:indexes) {
        const auto& repo=index.repository;
        if(!PackagePlanNameValid(repo.id)||(!previous.empty()&&previous>=repo.id)||!Hash(repo.release_sha256)
           ||!Hash(repo.index_sha256)||!repo.valid_until_unix||repo.valid_until_unix>INT64_MAX)return Fail(EINVAL);
        if(repo.valid_until_unix<=now)return Fail(ESTALE);previous=repo.id;
        struct stat before;if(Describe(index.fd,uint64_t{256}<<20,&before)<0)return -1;
        total+=before.st_size;if(total>(uint64_t{512}<<20))return Fail(EFBIG);
        IndexReader reader{effects,found,names,repo.id,{},{},{},{},{},0,false};
        if(Stream(index.fd,before,repo.index_sha256,[&](const char* p,size_t n){return reader.Bytes(p,n);})<0 || reader.End()<0)return -1;
    }
    uint64_t archives=0;
    for(const auto& a:found)if(!a.effect.after_version.empty()) {
        if(a.repository.empty())return Fail(ENOENT);
        archives+=a.archive.bytes;if(archives>(uint64_t{8}<<30))return Fail(EFBIG);
    }
    *output=std::move(found);return 0;
}
int PackageVerifyAptArchive(const PackageAptArchive& expected,int fd) {
    if(expected.effect.after_version.empty()||!PackagePlanNameValid(expected.effect.name)
       ||!PackagePlanVersionValid(expected.effect.after_version)
       ||(expected.effect.architecture!="all"&&expected.effect.architecture!="arm64")
       ||!PackagePlanNameValid(expected.repository)||!Relative(expected.filename)
       ||!expected.archive.bytes||expected.archive.bytes>(uint64_t{2}<<30)||!Hash(expected.archive.sha256))return Fail(EINVAL);
    struct stat before;if(Describe(fd,uint64_t{2}<<30,&before)<0)return -1;
    if(uint64_t(before.st_size)!=expected.archive.bytes)return Fail(EBADMSG);
    return Stream(fd,before,expected.archive.sha256,[](const char*,size_t){return 0;});
}
int PackageBindAptArchives(const PackageResolvedPlan& context,
    const std::vector<PackageAptArchive>& effects,const std::vector<int>& fds,
    uint64_t now,PackageBoundPlan* output) {
    if(!output || !context.changes.empty() || effects.size()>64 || effects.size()!=fds.size())return Fail(EINVAL);
    PackageResolvedPlan resolved=context;
    for(size_t i=0;i<effects.size();++i) {
        const auto& archive=effects[i];const auto& effect=archive.effect;
        if(effect.after_version.empty()) {
            if(fds[i]!=-1 || !archive.filename.empty() || !archive.repository.empty()
               || archive.archive.bytes || !archive.archive.sha256.empty())return Fail(EINVAL);
        } else if(fds[i]<0 || !Relative(archive.filename))return Fail(EINVAL);
        resolved.changes.push_back({effect.name,effect.architecture,effect.before_version,
            effect.after_version,archive.repository,archive.archive,
            effect.automatic?PackageInstallReason::Automatic:PackageInstallReason::Manual});
    }
    PackageBoundPlan bound;
    if(PackageBindResolvedPlan(resolved,now,&bound)<0)return -1;
    for(size_t i=0;i<effects.size();++i) {
        if(!effects[i].effect.after_version.empty() && PackageVerifyAptArchive(effects[i],fds[i])<0)return -1;
    }
    *output=std::move(bound);return 0;
}
}
