#include "package_apt_release.h"
#include <android-base/unique_fd.h>
#include <openssl/sha.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <cctype>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
using android::base::unique_fd;
namespace aegis {
namespace {
int Fail(int e) { errno=e;return -1; }
std::string Hash(const std::string& s) {
    unsigned char digest[32];char hex[65];
    if(!SHA256(reinterpret_cast<const unsigned char*>(s.data()),s.size(),digest))return {};
    for(unsigned i=0;i<32;++i)snprintf(hex+2*i,3,"%02x",digest[i]);return hex;
}
bool Relative(const std::string& s) {
    if(s.empty()||s.size()>1024||s.front()=='/'||s.back()=='/')return false;
    std::string part;
    for(char c:s+"/") {
        if(c=='/') { if(part.empty()||part=="."||part=="..")return false;part.clear(); }
        else { if(!isalnum(static_cast<unsigned char>(c))&&c!='-'&&c!='_'&&c!='.'&&c!='+'&&c!='~')return false;part+=c; }
    }
    return true;
}
std::string Trim(const std::string& s) {
    auto a=s.find_first_not_of(" \t"),b=s.find_last_not_of(" \t");return a==std::string::npos?"":s.substr(a,b-a+1);
}
// Strict RFC2822 UTC subset used by Debian Release metadata. Roundtrip rejects
// normalized impossible dates and mismatching weekdays, rather than accepting them.
bool Date(const std::string& s,uint64_t* output) {
    auto at=s.rfind(' ');if(at==std::string::npos)return false;
    auto zone=s.substr(at+1);if(zone!="UTC"&&zone!="GMT"&&zone!="+0000")return false;
    std::string body=s.substr(0,at);tm date={};char* end=strptime(body.c_str(),"%a, %d %b %Y %H:%M:%S",&date);
    if(!end||*end||date.tm_year<70||date.tm_year>200||date.tm_sec>59)return false;
    time_t seconds=timegm(&date);char canonical[64];
    if(seconds<0||!strftime(canonical,sizeof(canonical),"%a, %d %b %Y %H:%M:%S",&date)||body!=canonical)return false;
    *output=seconds;return true;
}
using Fields=std::map<std::string,std::string>;
int Records(const std::string& text,bool folded,std::vector<Fields>* out) {
    if(text.empty()||text.size()>(4u<<20)||text.back()!='\n')return Fail(EBADMSG);
    std::istringstream input(text);std::string line,previous;Fields fields;
    while(std::getline(input,line)) {
        if(line.size()>65536)return Fail(EFBIG);
        for(unsigned char c:line)if(!c||c=='\r'||(c<32&&c!='\t')||c>=127)return Fail(EBADMSG);
        if(line.empty()) { if(!fields.empty()) { out->push_back(std::move(fields));fields.clear();if(out->size()>16)return Fail(EFBIG); }previous.clear();continue; }
        if(line[0]==' '||line[0]=='\t') {
            if(!folded||previous.empty())return Fail(EBADMSG);
            fields[previous]+='\n';fields[previous]+=Trim(line);continue;
        }
        auto colon=line.find(':');if(colon==std::string::npos||!colon||colon>128)return Fail(EBADMSG);
        std::string key=line.substr(0,colon);
        for(char& c:key) { if(!isalnum(static_cast<unsigned char>(c))&&c!='-')return Fail(EBADMSG);c=tolower(static_cast<unsigned char>(c)); }
        if(!fields.emplace(key,Trim(line.substr(colon+1))).second||fields.size()>256)return Fail(EBADMSG);previous=key;
    }
    if(!fields.empty())out->push_back(std::move(fields));return out->size()>16?Fail(EFBIG):0;
}
int Payload(const std::string& bytes,std::string* output) {
    if(bytes.empty()||bytes.size()>(4u<<20))return Fail(EBADMSG);
    std::string text;for(size_t i=0;i<bytes.size();++i) {
        if(bytes[i]=='\r') { if(i+1==bytes.size()||bytes[i+1]!='\n')return Fail(EBADMSG);continue; }text+=bytes[i];
    }
    constexpr char begin[]="-----BEGIN PGP SIGNED MESSAGE-----\n";
    if(text.compare(0,sizeof(begin)-1,begin)) { *output=std::move(text);return 0; }
    auto split=text.find("\n\n",sizeof(begin)-1),signature=text.find("\n-----BEGIN PGP SIGNATURE-----\n");
    if(split==std::string::npos||signature==std::string::npos||signature<=split
       ||text.find("-----END PGP SIGNATURE-----\n",signature)==std::string::npos)return Fail(EBADMSG);
    std::istringstream clear(text.substr(split+2,signature-split-1));std::string line,payload;
    while(std::getline(clear,line)) {
        if(line.compare(0,2,"- ")==0)line.erase(0,2);
        else if(!line.empty()&&line[0]=='-')return Fail(EBADMSG);
        payload+=line+'\n';
    }
    *output=std::move(payload);return 0;
}
// APT 3.0.3 URItoFileName for our fixed credential-free http(s)/copy sources.
// Reject URLs outside this explicit subset; never interpret paths from a client.
std::string Filename(const std::string& uri) {
    std::string raw;
    if(uri.compare(0,8,"https://")==0)raw=uri.substr(8);
    else if(uri.compare(0,7,"http://")==0)raw=uri.substr(7);
    else if(uri.compare(0,6,"copy:/")==0)raw=uri.substr(5);
    else return {};
    if(raw.empty()||raw.find_first_of("@?#\\")!=std::string::npos)return {};
    const std::string quote="|{}[]<>\"^~_=!$%&*";std::string name;
    for(unsigned char c:raw) {
        if(c<=32||c>=127)return {};
        if(c=='/')name+='_';
        else if(quote.find(c)!=std::string::npos) { char hex[4];snprintf(hex,sizeof(hex),"%%%02x",c);name+=hex; }
        else name+=c;
    }
    return name.size()<=255?name:"";
}
int Open(int directory,const std::string& name,uint64_t maximum,struct stat* st) {
    if(name.empty()||name=="."||name==".."||name.find('/')!=std::string::npos)return Fail(EINVAL);
    unique_fd fd(openat(directory,name.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));if(!fd.ok())return -1;
    if(fstat(fd.get(),st)<0)return -1;
    if(!S_ISREG(st->st_mode)||st->st_nlink!=1||(st->st_mode&07022)||st->st_size<0||uint64_t(st->st_size)>maximum)return Fail(EPERM);
    return fd.release();
}
int Read(int directory,const std::string& name,uint64_t maximum,std::string* result) {
    struct stat st;unique_fd fd(Open(directory,name,maximum,&st));if(!fd.ok())return -1;
    std::string bytes(st.st_size,'\0');for(size_t at=0;at<bytes.size();) {
        auto n=pread(fd.get(),bytes.data()+at,bytes.size()-at,at);if(n<0&&errno==EINTR)continue;
        if(n<=0)return n<0?-1:Fail(ESTALE);at+=n;
    }
    *result=std::move(bytes);return 0;
}
}
int PackageReadAuthenticatedRelease(const std::string& bytes,const std::string& key,uint64_t now,PackageReleaseIndex* output) {
    if(!output||!Relative(key)||!now||now>INT64_MAX-PackageReleaseMaxAge)return Fail(EINVAL);
    std::string text;if(Payload(bytes,&text)<0)return -1;
    std::vector<Fields> records;if(Records(text,true,&records)<0)return -1;
    if(records.size()!=1)return Fail(EBADMSG);auto& f=records[0];PackageReleaseIndex value;
    if(!Date(f["date"],&value.date)||value.date>now+10)return Fail(EBADMSG);
    value.valid_until=value.date+PackageReleaseMaxAge;
    if(f.count("valid-until")) { uint64_t expiry;if(!Date(f["valid-until"],&expiry)||expiry<=value.date)return Fail(EBADMSG);value.valid_until=std::min(value.valid_until,expiry); }
    if(value.valid_until<=now)return Fail(ESTALE);
    std::istringstream lines(f["sha256"]);std::string line;std::set<std::string> names;
    while(std::getline(lines,line)) {
        if(line.empty())continue;std::istringstream row(line);std::string hash,size,name,extra;
        if(!(row>>hash>>size>>name)||(row>>extra)||hash.size()!=64||!Relative(name)||!names.insert(name).second)return Fail(EBADMSG);
        for(char c:hash)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return Fail(EBADMSG);
        if(size.empty()||size.size()>12||(size.size()>1&&size[0]=='0'))return Fail(EBADMSG);
        uint64_t n=0;for(char c:size) { if(c<'0'||c>'9')return Fail(EBADMSG);n=n*10+c-'0'; }
        if(name==key) { if(n>(256u<<20))return Fail(EFBIG);value.index={n,hash}; }
    }
    if(value.index.sha256.empty())return Fail(ENOENT);*output=std::move(value);return 0;
}
int PackageCollectAptEvidence(int directory,const std::string& targets,const std::vector<PackageAptEffect>& effects,
    uint64_t now,std::vector<PackageRepository>* repositories,std::vector<PackageAptArchive>* archives) {
    if(!repositories||!archives)return Fail(EINVAL);
    std::vector<Fields> records;if(Records(targets,false,&records)<0)return -1;
    unique_fd lists(openat(directory,"lists",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
    unique_fd cache(openat(directory,"archives",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));if(!lists.ok()||!cache.ok())return -1;
    std::vector<unique_fd> pinned;std::vector<PackageAptIndex> indexes;std::set<std::string> ids;
    for(auto& f:records) {
        if(f["created-by"]!="Packages"||f["target-of"]!="deb"||f["trusted"]!="yes"
           ||f["keepcompressed"]!="no"||f["signed-by"]!="/run/aegis-plan-policy/key.asc"
           ||!Relative(f["metakey"])||f["base-uri"].empty()||f["base-uri"].back()!='/'
           ||f["uri"]!=f["base-uri"]+f["metakey"])return Fail(EPERM);
        const auto name=Filename(f["uri"]),prefix=Filename(f["base-uri"]);
        if(name.empty()||prefix.empty()||f["filename"]!="/tmp/aegis-planner/lists/"+name)return Fail(EPERM);
        std::string release;
        if(Read(lists.get(),prefix+"InRelease",4u<<20,&release)<0) {
            if(errno!=ENOENT)return -1;std::string signature;
            if(Read(lists.get(),prefix+"Release",4u<<20,&release)<0
               ||Read(lists.get(),prefix+"Release.gpg",1u<<20,&signature)<0)return -1;
            if(signature.empty())return Fail(EBADMSG);
        }
        PackageReleaseIndex expected;if(PackageReadAuthenticatedRelease(release,f["metakey"],now,&expected)<0)return -1;
        struct stat st;unique_fd index(Open(lists.get(),name,256u<<20,&st));if(!index.ok())return -1;
        if(uint64_t(st.st_size)!=expected.index.bytes)return Fail(EBADMSG);
        PackageRepository repository={"apt-"+Hash(f["uri"]),Hash(release),expected.index.sha256,expected.valid_until};
        if(!ids.insert(repository.id).second)return Fail(EEXIST);
        indexes.push_back({repository,index.get()});pinned.push_back(std::move(index));
    }
    if(indexes.empty())return Fail(ENOENT);
    std::sort(indexes.begin(),indexes.end(),[](const auto& a,const auto& b){return a.repository.id<b.repository.id;});
    std::vector<PackageAptArchive> matched;
    if(PackageMatchAptArchives(effects,indexes,now,&matched)<0)return -1;
    for(const auto& archive:matched) {
        if(archive.effect.after_version.empty())continue;
        std::string version;for(char c:archive.effect.after_version)version+=c==':'?"%3a":std::string(1,c);
        const auto name=archive.effect.name+"_"+version+"_"+archive.effect.architecture+".deb";
        struct stat st;unique_fd fd(Open(cache.get(),name,2ull<<30,&st));
        if(!fd.ok()||PackageVerifyAptArchive(archive,fd.get())<0)return -1;
    }
    std::vector<PackageRepository> repos;for(const auto& index:indexes)repos.push_back(index.repository);
    *repositories=std::move(repos);*archives=std::move(matched);return 0;
}
}
