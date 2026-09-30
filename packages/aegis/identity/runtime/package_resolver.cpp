#include "package_resolver.h"
#include "sandbox.h"
#include <android-base/unique_fd.h>
#include <openssl/sha.h>
#include <json/json.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <array>
using android::base::unique_fd;
namespace aegis {
namespace {
const char policy[]=
    "Dir::Etc::parts \"/run/aegis-plan-policy/empty\";\n"
    "Dir::Etc::main \"/run/aegis-plan-policy/empty.conf\";\n"
    "Dir::Etc::sourcelist \"/run/aegis-plan-policy/sources.list\";\n"
    "Dir::Etc::sourceparts \"/run/aegis-plan-policy/empty\";\n"
    "Dir::Etc::trusted \"/run/aegis-plan-policy/absent.gpg\";\n"
    "Dir::Etc::trustedparts \"/run/aegis-plan-policy/empty\";\n"
    "Dir::State::lists \"/tmp/aegis-planner/lists\";\n"
    "Dir::State::status \"/run/aegis-plan-input/status\";\n"
    "Dir::State::extended_states \"/tmp/aegis-planner/extended_states\";\n"
    "Dir::Cache::archives \"/tmp/aegis-planner/archives\";\n"
    "Dir::Cache::pkgcache \"\";\nDir::Cache::srcpkgcache \"\";\n"
    "Dir::Log \"/tmp/aegis-planner\";\nDir::Bin::dpkg \"/usr/bin/false\";\n"
    "APT::Architecture \"arm64\";\nAPT::Architectures { \"arm64\"; };\n"
    "APT::Install-Recommends \"false\";\nAPT::Install-Suggests \"false\";\n"
    "APT::Get::AllowUnauthenticated \"false\";\nAPT::Update::Error-Mode \"any\";\n"
    "Acquire::AllowInsecureRepositories \"false\";\n"
    "Acquire::AllowDowngradeToInsecureRepositories \"false\";\n"
    "Acquire::AllowWeakRepositories \"false\";\nAcquire::Check-Date \"true\";\n"
    "Acquire::Check-Valid-Until \"true\";\nAcquire::Languages \"none\";\n"
    "Acquire::Max-ValidTime \"10368000\";\nAcquire::GzipIndexes \"false\";\n"
    "Acquire::IndexTargets::deb::Packages::KeepCompressed \"false\";\n"
    "Acquire::Retries \"0\";\nAcquire::http::Timeout \"30\";\nAcquire::https::Timeout \"30\";\n"
    "AptCli::Hooks::Install { \"/run/aegis-plan-policy/hook --apt-plan-hook\"; };\n"
    "AptCli::Hooks::Upgrade { \"/run/aegis-plan-policy/hook --apt-plan-hook\"; };\n";
int Fail(int e) { errno=e;return -1; }
std::string Hash(const std::string& s) {
    unsigned char bytes[32];char hex[65];
    if(!SHA256(reinterpret_cast<const unsigned char*>(s.data()),s.size(),bytes))return {};
    for(unsigned i=0;i<32;++i)snprintf(hex+2*i,3,"%02x",bytes[i]);return hex;
}
int Read(const char* path,size_t limit,bool readonly,std::string* out) {
    unique_fd fd(open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));if(!fd.ok())return -1;
    struct stat st;struct statvfs fs;
    if(fstat(fd.get(),&st)<0||fstatvfs(fd.get(),&fs)<0)return -1;
    if(!S_ISREG(st.st_mode)||st.st_uid||st.st_gid||st.st_nlink!=1||(st.st_mode&07022)
       ||st.st_size<0||uint64_t(st.st_size)>limit||(readonly&&!(fs.f_flag&ST_RDONLY)))return Fail(EPERM);
    std::string data(st.st_size,'\0');
    for(size_t at=0;at<data.size();) {
        ssize_t n=pread(fd.get(),data.data()+at,data.size()-at,at);if(n<0&&errno==EINTR)continue;
        if(n<=0)return n<0?-1:Fail(ESTALE);at+=n;
    }
    *out=std::move(data);return 0;
}
int Write(const char* path,const std::string& data) {
    unique_fd fd(open(path,O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW|O_CLOEXEC,0600));if(!fd.ok())return -1;
    for(size_t at=0;at<data.size();) {
        ssize_t n=write(fd.get(),data.data()+at,data.size()-at);if(n<0&&errno==EINTR)continue;
        if(n<=0)return n<0?-1:Fail(EIO);at+=n;
    }
    return fsync(fd.get());
}
int View(const char* path,bool readonly,bool executable) {
    struct stat st;struct statvfs fs;
    if(lstat(path,&st)<0||statvfs(path,&fs)<0)return -1;
    if(st.st_mode!=(S_IFDIR|0755)||st.st_uid||st.st_gid
       ||!(fs.f_flag&ST_NOSUID)||!(fs.f_flag&ST_NODEV)
       ||bool(fs.f_flag&ST_RDONLY)!=readonly||bool(fs.f_flag&ST_NOEXEC)==executable)return Fail(EPERM);
    return 0;
}
int Run(const std::vector<std::string>& arguments,const char* log,int* status) {
    pid_t child=fork();if(child<0)return -1;
    if(!child) {
        // Every broker/channel/source fd is removed before immutable APT runs.
        if(syscall(SYS_close_range,0u,~0u,0u)<0)_exit(126);
        int in=open("/dev/null",O_RDONLY),out=open(log,O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW,0600);
        if(in!=0||out!=1||dup2(out,2)<0)_exit(126);
        rlimit logs={512u<<20,512u<<20};if(setrlimit(RLIMIT_FSIZE,&logs)<0)_exit(126);
        std::vector<char*> argv;for(const auto& s:arguments)argv.push_back(const_cast<char*>(s.c_str()));argv.push_back(nullptr);
        char* env[]={const_cast<char*>("PATH=/usr/sbin:/usr/bin:/sbin:/bin"),const_cast<char*>("LANG=C"),
            const_cast<char*>("LC_ALL=C"),const_cast<char*>("HOME=/root"),
            const_cast<char*>("DEBIAN_FRONTEND=noninteractive"),
            const_cast<char*>("APT_CONFIG=/run/aegis-plan-policy/config"),nullptr};
        execve(argv[0],argv.data(),env);_exit(127);
    }
    int exited;pid_t waited;do { waited=waitpid(child,&exited,0); }while(waited<0&&errno==EINTR);
    int error=waited<0?errno:0;
    if(!error)*status=WIFEXITED(exited)?WEXITSTATUS(exited):128+WTERMSIG(exited);
    if(kill(-1,SIGKILL)<0&&errno!=ESRCH&&!error)error=errno;
    while(waitpid(-1,&exited,0)>0||errno==EINTR) {}
    return error?Fail(error):0;
}
bool Same(const std::vector<PackageAptEffect>& a,const std::vector<PackageAptEffect>& b) {
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i)if(a[i].name!=b[i].name||a[i].architecture!=b[i].architecture
       ||a[i].before_version!=b[i].before_version||a[i].after_version!=b[i].after_version||a[i].automatic!=b[i].automatic)return false;
    return true;
}
}
const char* PackageResolverConfiguration(bool internet) {
    static const std::string online=std::string(policy)+
        "Acquire::http::Proxy \"socks5h://127.0.0.1:1080\";\n"
        "Acquire::https::Proxy \"socks5h://127.0.0.1:1080\";\n";
    return internet?online.c_str():policy;
}
PackageResolverResult PackageResolverRun(uint32_t user,const PackageResolverRequest& r) {
    PackageResolverResult result;
    auto fail=[&](int e) { result.error=e;return result; };
    if(PackageResolverCheck(r)<0)return fail(errno);
    if(getpid()!=1||getppid()||getuid()||getgid())return fail(EPERM);
    if(View("/",true,true)<0||View("/run/aegis-plan-policy",true,true)<0
       ||View("/run/aegis-plan-input",true,false)<0)return fail(errno);
    std::string config,sources,key,status,automatic;
    if(Read("/run/aegis-plan-policy/config",16384,true,&config)<0
       ||Read("/run/aegis-plan-policy/sources.list",16384,true,&sources)<0
       ||Read("/run/aegis-plan-policy/key.asc",1048576,true,&key)<0
       ||Read("/run/aegis-plan-input/status",64u<<20,true,&status)<0)return fail(errno);
    if(config!=PackageResolverConfiguration(r.internet)||sources.empty()||key.empty())return fail(EPERM);
    bool present=Read("/run/aegis-plan-input/extended_states",16u<<20,true,&automatic)==0;
    if(!present&&errno!=ENOENT)return fail(errno);
    if(mkdir("/tmp/aegis-planner",0755)<0)return fail(errno);
    for(const char* path:{"/tmp/aegis-planner/lists","/tmp/aegis-planner/lists/partial",
        "/tmp/aegis-planner/archives","/tmp/aegis-planner/archives/partial"})if(mkdir(path,0755)<0)return fail(errno);
    if(present&&Write("/tmp/aegis-planner/extended_states",automatic)<0)return fail(errno);
    if(aegis_limit_package_supervisor(user)<0)return fail(errno);
    auto command=[&](std::vector<std::string> args,const char* log) {
        if(Run(args,log,&result.status)<0) { result.error=errno;return false; }
        return result.status==0;
    };
    result.phase=PackageResolverResult::Phase::Update;
    if(!command({"/usr/bin/apt-get","-q","update"},"/tmp/aegis-planner/update.log"))return result;
    const std::string action=r.action==PackageAction::Install?"install":r.action==PackageAction::Remove?"remove":"upgrade";
    std::vector<std::string> args={"/usr/bin/apt-get","-q","--simulate",action};
    if(r.action!=PackageAction::Update)args.push_back(r.package+(r.version.empty()?"":"="+r.version));
    result.phase=PackageResolverResult::Phase::Simulate;
    if(!command(args,"/tmp/aegis-planner/simulate.log"))return result;
    std::string json;
    if(Read("/run/aegis-apt-plan.json",262144,false,&json)<0
       ||PackageReadAptPlan(json,r.action,r.package,r.version,&result.effects)<0
       ||Write("/tmp/aegis-planner/simulation.json",json)<0)return fail(errno);
    if(unlink("/run/aegis-apt-plan.json")<0)return fail(errno);
    if(!result.effects.empty()&&r.action!=PackageAction::Remove) {
        result.phase=PackageResolverResult::Phase::Download;
        args={"/usr/bin/apt-get","-q","--download-only","--yes",action};
        if(r.action!=PackageAction::Update)args.push_back(r.package+(r.version.empty()?"":"="+r.version));
        if(!command(args,"/tmp/aegis-planner/download.log"))return result;
        std::vector<PackageAptEffect> downloaded;
        if(Read("/run/aegis-apt-plan.json",262144,false,&json)<0
           ||PackageReadAptPlan(json,r.action,r.package,r.version,&downloaded)<0)return fail(errno);
        if(!Same(result.effects,downloaded))return fail(ESTALE);
        if(Write("/tmp/aegis-planner/download.json",json)<0||unlink("/run/aegis-apt-plan.json")<0)return fail(errno);
    }
    result.phase=PackageResolverResult::Phase::Indexes;
    if(!command({"/usr/bin/apt-get","indextargets"},"/tmp/aegis-planner/index-targets"))return result;
    std::string targets;
    unique_fd collected(open("/tmp/aegis-planner",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
    const auto now=time(nullptr);
    if(!collected.ok()||now<=0||Read("/tmp/aegis-planner/index-targets",4u<<20,false,&targets)<0
       ||PackageCollectAptEvidence(collected.get(),targets,result.effects,uint64_t(now),
                                  &result.repositories,&result.archives)<0)return fail(errno);
    std::string after;
    if(Read("/run/aegis-plan-input/status",64u<<20,true,&after)<0)return fail(errno);
    if(after!=status)return fail(ESTALE);
    bool have=Read("/tmp/aegis-planner/extended_states",16u<<20,false,&after)==0;
    if(!have&&errno!=ENOENT)return fail(errno);
    if(have!=present||(present&&after!=automatic))return fail(ESTALE);
    Json::Value receipt;receipt["schema"]=1;receipt["initial_status_sha256"]=Hash(status);
    receipt["apt_state_present"]=present;receipt["apt_state_bytes"]=Json::UInt64(automatic.size());
    receipt["initial_apt_state_sha256"]=present?Hash(automatic):"";
    receipt["configuration_sha256"]=Hash(config);receipt["sources_sha256"]=Hash(sources);receipt["keyring_sha256"]=Hash(key);
    receipt["effect_count"]=Json::UInt(result.effects.size());
    receipt["policy_sha256"]=Hash("aegis-resolver-policy-v1:"+Hash(config)+Hash(sources)+Hash(key));
    receipt["repositories"]=Json::Value(Json::arrayValue);
    for(const auto& repo:result.repositories) {
        Json::Value row;row["id"]=repo.id;row["release_sha256"]=repo.release_sha256;
        row["index_sha256"]=repo.index_sha256;row["valid_until_unix"]=Json::UInt64(repo.valid_until_unix);
        receipt["repositories"].append(row);
    }
    receipt["archives"]=Json::Value(Json::arrayValue);
    for(const auto& archive:result.archives) {
        Json::Value row;row["name"]=archive.effect.name;row["repository"]=archive.repository;
        row["filename"]=archive.filename;row["bytes"]=Json::UInt64(archive.archive.bytes);
        row["sha256"]=archive.archive.sha256;receipt["archives"].append(row);
    }
    Json::StreamWriterBuilder writer;writer["indentation"]="";
    if(Write("/tmp/aegis-planner/receipt.json",Json::writeString(writer,receipt))<0)return fail(errno);
    auto& proof=result.evidence;
    proof.initial_status_sha256=Hash(status);
    proof.initial_apt_state_presence=present?PackageStatePresence::Present:PackageStatePresence::Absent;
    if(present)proof.initial_apt_state={automatic.size(),Hash(automatic)};
    proof.policy_sha256=Hash("aegis-resolver-policy-v1:"+Hash(config)+Hash(sources)+Hash(key));
    proof.repositories=result.repositories;
    for(const auto& a:result.archives)proof.changes.push_back({a.effect.name,a.effect.architecture,
        a.effect.before_version,a.effect.after_version,a.repository,a.archive,
        a.effect.automatic?PackageInstallReason::Automatic:PackageInstallReason::Manual});
    result.phase=PackageResolverResult::Phase::Collected;return result;
}
}
