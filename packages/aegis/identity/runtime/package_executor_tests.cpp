// Compile on the SSH builder, execute only in local QEMU. Synthetic packages
// in an exclusively copied base; no live AOSP users, passwords or CE mutation.
#include "package_executor.h"
#include "package_execution_protocol.h"
#include "package_apt_fixture.h"
#include "broker_owner_package.h"
#include "namespace.h"
#include <gtest/gtest.h>
#include <android-base/unique_fd.h>
#include <dirent.h>
#include <grp.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <poll.h>
#include <time.h>
#include <memory>
using namespace aegis;
using android::base::unique_fd;
namespace {
uint64_t Deadline() {
    timespec t;EXPECT_EQ(0,clock_gettime(CLOCK_MONOTONIC,&t));
    return uint64_t(t.tv_sec)*1000000000+t.tv_nsec+UINT64_C(9000000000);
}
int WriteAt(int root,const char* name,const std::string& data,int flags=O_CREAT|O_EXCL) {
    unique_fd fd(openat(root,name,O_WRONLY|O_NOFOLLOW|O_CLOEXEC|flags,0644));
    if(!fd.ok())return -1;
    for(size_t at=0;at<data.size();) {
        ssize_t n=TEMP_FAILURE_RETRY(write(fd.get(),data.data()+at,data.size()-at));
        if(n<=0)return -1;at+=n;
    }
    return flags ? fsync(fd.get()) : 0;
}
int CountFDs() {
    DIR* d=opendir("/proc/self/fd");if(!d)return -1;
    int n=0;while(auto* entry=readdir(d))if(entry->d_name[0]!='.')n++;
    closedir(d);return n;
}
void Octal(char* out,size_t width,uint64_t value) {
    snprintf(out,width,"%0*llo",static_cast<int>(width-1),static_cast<unsigned long long>(value));
}
void Tar(std::string& archive,const char* name,const std::string& contents,unsigned mode=0644) {
    char header[512]={};EXPECT_LT(strlen(name),100u);memcpy(header,name,strlen(name));
    Octal(header+100,8,mode);Octal(header+108,8,0);Octal(header+116,8,0);
    Octal(header+124,12,contents.size());Octal(header+136,12,0);
    memset(header+148,' ',8);header[156]='0';memcpy(header+257,"ustar",5);memcpy(header+263,"00",2);
    unsigned sum=0;for(unsigned char b:header)sum+=b;
    snprintf(header+148,8,"%06o",sum);header[155]=' ';
    archive.append(header,512);archive+=contents;archive.append((512-contents.size()%512)%512,'\0');
}
void Ar(std::string& archive,const char* name,const std::string& data) {
    char h[61];int n=snprintf(h,sizeof(h),"%-16s%-12u%-6u%-6u%-8o%-10zu`\n",name,0,0,0,0100644,data.size());
    EXPECT_EQ(60,n);archive.append(h,60);archive+=data;if(data.size()%2)archive+='\n';
}
std::string Deb(const char* kind,int version,bool waiting=false) {
    std::string v=std::to_string(version),control,data;
    std::string fields="Package: aegis-exec-"+std::string(kind)+"\nVersion: "+v+
        "\nArchitecture: all\nMaintainer: AEGIS fixture <test@invalid>\nDescription: Local device fixture\n";
    bool app=!strcmp(kind,"app");if(app)fields+="Depends: aegis-exec-lib (= "+v+")\n";
    Tar(control,"./control",fields);
    if(app) {
        Tar(control,"./conffiles","/etc/aegis-exec.conf\n");
        Tar(control,"./postinst","#!/bin/sh\nset -eu\necho postinst-"+v+" >> /var/log/aegis-exec-script\n"
            "mkdir -p /var/lib/aegis-exec-owned\nchown 42:42 /var/lib/aegis-exec-owned\n"
            +std::string(waiting?"echo waiting > /var/log/aegis-exec-waiting\nwhile :; do sleep 1; done\n":"echo done >> /var/log/aegis-exec-script\n"),0755);
        Tar(control,"./prerm","#!/bin/sh\necho prerm >> /var/log/aegis-exec-script\n",0755);
        Tar(data,"./usr/bin/aegis-exec-app","#!/bin/sh\necho app-"+v+"\n",0755);
        Tar(data,"./etc/aegis-exec.conf","version="+v+"\n");
    } else Tar(data,"./usr/share/aegis-exec-library",v+"\n");
    control.append(1024,'\0');data.append(1024,'\0');std::string deb="!<arch>\n";
    Ar(deb,"debian-binary","2.0\n");Ar(deb,"control.tar",control);Ar(deb,"data.tar",data);return deb;
}
class RuntimePackageExecutor : public ::testing::Test {
 protected:
    std::unique_ptr<AptImageFixture> image;
    unique_fd candidate,stage,cgroups,parent,helper;
    PackageExecutor* worker=nullptr;
    aegis_broker_owner* broker=nullptr;
    PackageExecution plan;
    std::string group;
    bool group_created=false;
    void SetUp() override {
        ASSERT_EQ(0u,getuid());ASSERT_EQ(0,setgroups(0,nullptr));
        struct sigaction a={};a.sa_handler=SIG_DFL;sigemptyset(&a.sa_mask);
        ASSERT_EQ(0,sigaction(SIGCHLD,&a,nullptr));
        unique_fd u(open("/proc/1/ns/user",O_RDONLY|O_CLOEXEC));
        unique_fd p(open("/proc/1/ns/pid",O_RDONLY|O_CLOEXEC));
        unique_fd m(open("/proc/1/ns/mnt",O_RDONLY|O_CLOEXEC));
        ASSERT_EQ(0,aegis_namespace_pin_host(u.get(),p.get(),m.get())) << strerror(errno);
        cgroups.reset(open("/sys/fs/cgroup",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(cgroups.ok());
        group="aegis-executor-test-"+std::to_string(getpid());
        ASSERT_EQ(0,mkdirat(cgroups.get(),group.c_str(),0700));group_created=true;
        parent.reset(openat(cgroups.get(),group.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
        ASSERT_TRUE(parent.ok());ASSERT_EQ(0,fchown(parent.get(),0,0));ASSERT_EQ(0,fchmod(parent.get(),0700));
        ASSERT_EQ(0,WriteAt(parent.get(),"cgroup.subtree_control","+memory\n",0));
        image=std::make_unique<AptImageFixture>();candidate.reset(image->create());ASSERT_TRUE(candidate.ok())<<strerror(errno);
        stage.reset(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(stage.ok());
        Helper("aegis-package-execute-probe");ASSERT_FALSE(HasFatalFailure());
        plan.requester=10;plan.serial=42;plan.job=1;plan.plan_sha256=std::string(64,'a');
    }
    void Helper(const char* name) {
        char path[4096];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);ASSERT_GT(n,0);path[n]=0;
        std::string executable(path);executable.resize(executable.find_last_of('/')+1);executable+=name;
        helper.reset(open(executable.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(helper.ok());
    }
    void Archives(int version,bool waiting=false) {
        plan.archives=true;plan.items.clear();
        for(const char* name:{"var/cache","var/cache/apt","var/cache/apt/archives"}) {
            int made=mkdirat(candidate.get(),name,0755);
            ASSERT_TRUE(made==0 || errno==EEXIST) << strerror(errno);
        }
        for(const char* kind:{"lib","app"}) {
            std::string name="aegis-exec-"+std::string(kind)+"_"+std::to_string(version)+"_all.deb";
            std::string path="var/cache/apt/archives/"+name;
            ASSERT_EQ(0,WriteAt(candidate.get(),path.c_str(),Deb(kind,version,waiting))) << strerror(errno);
            plan.items.push_back(name);
        }
    }
    int Start() { return PackageExecutorStart(parent.get(),stage.get(),candidate.get(),helper.get(),plan,Deadline(),&worker); }
    void Completed() {
        int before=CountFDs();ASSERT_GT(before,0);
        ASSERT_EQ(0,Start()) << strerror(errno);
        PackageExecutionResult result;ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result)) << strerror(errno);
        EXPECT_EQ(PackageExecutionOutcome::NeedsValidation,result.outcome) << AptImageFixture::read(candidate.get(),("var/log/aegis-package-"+std::to_string(plan.job)+".log").c_str());
        EXPECT_EQ(0,result.status);EXPECT_EQ(0,result.error);EXPECT_EQ(nullptr,worker);EXPECT_EQ(before,CountFDs());
    }
    void Remount() { candidate.reset();candidate.reset(image->remount());ASSERT_TRUE(candidate.ok()) << strerror(errno);plan.job++; }
    bool Waiting() {
        for(unsigned i=0;i<500;i++) {
            if(AptImageFixture::read(candidate.get(),"var/log/aegis-exec-waiting")=="waiting\n")return true;
            usleep(10000);
        }
        return false;
    }
    void Broker() {
        ASSERT_EQ(0,aegis_broker_owner_create(parent.get(),candidate.get(),helper.get(),helper.get(),&broker)) << strerror(errno);
        plan.job=0;
    }
    uint64_t Prepared(uint32_t user=10) {
        plan.requester=user;uint64_t job=0;
        EXPECT_EQ(0,BrokerPrepareExecution(broker,plan,parent.get(),stage.get(),candidate.get(),helper.get(),Deadline(),&job))<<strerror(errno);
        return job;
    }
    int StopUser(uint32_t user) {
        aegis_broker_request r={};r.magic=AEGIS_BROKER_MAGIC;r.version=AEGIS_BROKER_VERSION;
        r.operation=AEGIS_BROKER_STOP_USER;r.user=user;r.serial=0;r.sequence=2;r.deadline_ns=Deadline();
        aegis_broker_state state=AEGIS_BROKER_SEALED;
        int result=aegis_broker_owner_apply(broker,&r,&state);
        if(!result)EXPECT_EQ(AEGIS_BROKER_ABSENT,state);return result;
    }
    void TearDown() override {
        if(broker) {
            EXPECT_EQ(0,aegis_broker_owner_stop_all(broker,Deadline())) << strerror(errno);
            EXPECT_EQ(0,aegis_broker_owner_release(&broker)) << strerror(errno);
        }
        if(worker) { PackageExecutionResult result;EXPECT_EQ(0,PackageExecutorFinish(&worker,true,9000,&result)) << strerror(errno); }
        if(parent.ok())EXPECT_EQ("populated 0\nfrozen 0\n",AptImageFixture::read(parent.get(),"cgroup.events"));
        parent.reset();if(group_created)EXPECT_EQ(0,unlinkat(cgroups.get(),group.c_str(),AT_REMOVEDIR)) << strerror(errno);
        // Close fixture references too, before autoclear; no force unmounts.
        candidate.reset();stage.reset();helper.reset();
        if(image)image->passed=!HasFailure();image.reset();
    }
};
TEST(PackageExecutionPlan, RejectsOptionsPathsNulDuplicateAndUnboundedInputs) {
    PackageExecution p;p.requester=10;p.serial=42;p.job=1;p.plan_sha256=std::string(64,'a');p.items={"app_1_all.deb"};
    ASSERT_EQ(0,PackageExecutionCheck(p));
    for(const char* value:{"-oAPT::Foo=1","../app.deb","/app.deb","app;id.deb","app.deb\nx","app"}) {
        p.items={value};EXPECT_EQ(-1,PackageExecutionCheck(p));
    }
    p.items={std::string("app.deb\0",8)};EXPECT_EQ(-1,PackageExecutionCheck(p));
    p.items={"app.deb","app.deb"};EXPECT_EQ(-1,PackageExecutionCheck(p));
    p.items={"app"};p.archives=false;EXPECT_EQ(0,PackageExecutionCheck(p));
    for(const char* name:{"app+","app-"}) { p.items={name};EXPECT_EQ(-1,PackageExecutionCheck(p)); }
    p.items={"app"};
    p.job=0;EXPECT_EQ(-1,PackageExecutionCheck(p));p.job=1;p.requester=0;EXPECT_EQ(-1,PackageExecutionCheck(p));
}
TEST_F(RuntimePackageExecutor, ExecutesActualInstallUpgradeRemoveAndClosesOwnedReferences) {
    Archives(1);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,WriteAt(candidate.get(),"var/log/aegis-package-1.log","previous-generation-log-must-not-survive\n"));
    Completed();ASSERT_FALSE(HasFailure());
    EXPECT_EQ(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-package-1.log").find("previous-generation-log"));
    EXPECT_EQ("1\n",AptImageFixture::read(candidate.get(),"usr/share/aegis-exec-library"));
    struct stat st;ASSERT_EQ(0,fstatat(candidate.get(),"var/lib/aegis-exec-owned",&st,AT_SYMLINK_NOFOLLOW));
    EXPECT_EQ(10u*100000u+5000u+42u,st.st_uid);EXPECT_EQ(st.st_uid,st.st_gid);
    Remount();ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,WriteAt(candidate.get(),"etc/aegis-exec.conf","personal=kept\n",O_TRUNC));
    Archives(2);ASSERT_FALSE(HasFatalFailure());Completed();ASSERT_FALSE(HasFailure());
    EXPECT_EQ("2\n",AptImageFixture::read(candidate.get(),"usr/share/aegis-exec-library"));
    EXPECT_EQ("#!/bin/sh\necho app-2\n",AptImageFixture::read(candidate.get(),"usr/bin/aegis-exec-app"));
    EXPECT_EQ("personal=kept\n",AptImageFixture::read(candidate.get(),"etc/aegis-exec.conf"));
    Remount();ASSERT_FALSE(HasFatalFailure());plan.archives=false;plan.items={"aegis-exec-app","aegis-exec-lib"};
    Completed();ASSERT_FALSE(HasFailure());
    EXPECT_EQ(-1,fstatat(candidate.get(),"usr/bin/aegis-exec-app",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,fstatat(candidate.get(),"usr/share/aegis-exec-library",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    // Defined remove action keeps the conffile; purge is a distinct policy.
    EXPECT_EQ("personal=kept\n",AptImageFixture::read(candidate.get(),"etc/aegis-exec.conf"));
    EXPECT_NE(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-exec-script").find("prerm\n"));
}
TEST_F(RuntimePackageExecutor, CancelsObservedMaintainerScriptAndReapsWholeGroup) {
    Archives(1,true);ASSERT_FALSE(HasFatalFailure());int before=CountFDs();
    ASSERT_EQ(0,Start()) << strerror(errno);ASSERT_TRUE(Waiting());
    PackageExecutionResult result;EXPECT_EQ(-1,PackageExecutorFinish(&worker,false,0,&result));EXPECT_EQ(ETIMEDOUT,errno);
    ASSERT_NE(nullptr,worker);ASSERT_EQ(0,PackageExecutorFinish(&worker,true,9000,&result)) << strerror(errno);
    EXPECT_EQ(PackageExecutionOutcome::Unconfirmed,result.outcome);EXPECT_EQ(nullptr,worker);EXPECT_EQ(before,CountFDs());
    EXPECT_EQ(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-exec-script").find("done\n"));
}
TEST_F(RuntimePackageExecutor, CancellationAfterNaturalExitNeverReturnsCandidateForValidation) {
    Archives(1);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(0,Start()) << strerror(errno);
    bool exited=false;
    for(unsigned i=0;i<900;i++) {
        if(AptImageFixture::read(parent.get(),"cgroup.events").find("populated 0\n")!=std::string::npos) { exited=true;break; }
        usleep(10000);
    }
    ASSERT_TRUE(exited);ASSERT_NE(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-exec-script").find("done\n"));
    ASSERT_EQ(0,PackageExecutorCancel(worker));
    PackageExecutionResult result;ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(ECANCELED,result.error);
}
TEST_F(RuntimePackageExecutor, PartialStartRetainsCgroupAndDescriptorsUntilFinish) {
    Archives(1);ASSERT_FALSE(HasFatalFailure());int before=CountFDs();helper.reset();before--;
    EXPECT_EQ(-1,Start());ASSERT_NE(nullptr,worker);
    PackageExecutionResult result;ASSERT_EQ(0,PackageExecutorFinish(&worker,true,9000,&result)) << strerror(errno);
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(before,CountFDs());
}
TEST_F(RuntimePackageExecutor, ProductionEntryRejectsDeveloperTestDomain) {
    Archives(1);Helper("aegis-package-execute");ASSERT_FALSE(HasFatalFailure());int before=CountFDs();
    uint64_t before_start=Deadline();
    EXPECT_EQ(-1,Start());ASSERT_NE(nullptr,worker);
    EXPECT_LT(Deadline()-before_start,UINT64_C(4000000000));
    PackageExecutionResult result;ASSERT_EQ(0,PackageExecutorFinish(&worker,true,9000,&result)) << strerror(errno);
    EXPECT_EQ(PackageExecutionOutcome::Unconfirmed,result.outcome);EXPECT_EQ(before,CountFDs());
    EXPECT_EQ("<unavailable>",AptImageFixture::read(candidate.get(),"var/log/aegis-exec-script"));
}
TEST_F(RuntimePackageExecutor, BrokerBindsApprovalAndUserStopCancelsActualAptOnlyForRequester) {
    Archives(1,true);Broker();ASSERT_FALSE(HasFatalFailure());uint64_t first=Prepared(),other=Prepared(11);ASSERT_FALSE(HasFailure());
    PackagePublication duplicate;duplicate.requester=10;duplicate.serial=42;duplicate.plan_sha256=plan.plan_sha256;
    uint64_t duplicate_id=0;
    EXPECT_EQ(-1,BrokerPreparePublication(broker,duplicate,parent.get(),stage.get(),candidate.get(),helper.get(),Deadline(),&duplicate_id));
    EXPECT_EQ(EBUSY,errno);EXPECT_EQ(0u,duplicate_id);
    EXPECT_EQ(-1,BrokerStartExecution(broker,10,43,first,plan.plan_sha256,Deadline()));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,BrokerStartExecution(broker,10,42,first,std::string(64,'b'),Deadline()));EXPECT_EQ(ESTALE,errno);
    ASSERT_EQ(0,BrokerStartExecution(broker,10,42,first,plan.plan_sha256,Deadline())) << strerror(errno);
    EXPECT_EQ(-1,BrokerStartExecution(broker,10,42,first,plan.plan_sha256,Deadline()));EXPECT_EQ(EALREADY,errno);
    ASSERT_TRUE(Waiting());EXPECT_EQ(-1,aegis_broker_owner_release(&broker));EXPECT_EQ(EBUSY,errno);
    ASSERT_EQ(0,StopUser(10)) << strerror(errno);
    PublicationState state=PublicationState::Complete;PackageExecutionResult result;
    EXPECT_EQ(-1,BrokerPollExecution(broker,10,42,first,plan.plan_sha256,&state,&result));EXPECT_EQ(ENOENT,errno);
    ASSERT_EQ(0,BrokerPollExecution(broker,11,42,other,plan.plan_sha256,&state,&result));EXPECT_EQ(PublicationState::Prepared,state);
    EXPECT_EQ(-1,BrokerCancelExecution(broker,10,42,other,plan.plan_sha256,Deadline()));EXPECT_EQ(ESTALE,errno);
    ASSERT_EQ(0,BrokerCancelExecution(broker,11,42,other,plan.plan_sha256,Deadline()));
}
TEST_F(RuntimePackageExecutor, BrokerReapsCompletedAptAndReturnsNeedsValidationOnce) {
    Archives(1);Broker();ASSERT_FALSE(HasFatalFailure());uint64_t job=Prepared();ASSERT_FALSE(HasFailure());
    ASSERT_EQ(0,BrokerStartExecution(broker,10,42,job,plan.plan_sha256,Deadline())) << strerror(errno);
    PublicationState state=PublicationState::Running;PackageExecutionResult result;
    for(unsigned i=0;i<900 && state!=PublicationState::Complete;i++) {
        ASSERT_EQ(0,aegis_broker_owner_reap_publications(broker));
        ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.plan_sha256,&state,&result));
        if(state!=PublicationState::Complete)usleep(10000);
    }
    EXPECT_EQ(PublicationState::Complete,state);EXPECT_EQ(PackageExecutionOutcome::NeedsValidation,result.outcome);
    EXPECT_EQ(0,result.status);EXPECT_EQ(0,result.error);
    EXPECT_EQ(-1,BrokerPollExecution(broker,10,42,job,plan.plan_sha256,&state,&result));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackageExecutor, BrokerCancellationRevokesAlreadyCollectedWorkerCompletion) {
    Archives(1);Broker();ASSERT_FALSE(HasFatalFailure());uint64_t job=Prepared();ASSERT_FALSE(HasFailure());
    ASSERT_EQ(0,BrokerStartExecution(broker,10,42,job,plan.plan_sha256,Deadline())) << strerror(errno);
    bool exited=false;
    for(unsigned i=0;i<900;i++) {
        if(AptImageFixture::read(parent.get(),"cgroup.events").find("populated 0\n")!=std::string::npos) { exited=true;break; }
        usleep(10000);
    }
    ASSERT_TRUE(exited);ASSERT_EQ(0,aegis_broker_owner_reap_publications(broker));
    ASSERT_EQ(0,BrokerCancelExecution(broker,10,42,job,plan.plan_sha256,Deadline()));
    PublicationState state=PublicationState::Running;PackageExecutionResult result;
    ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.plan_sha256,&state,&result));
    EXPECT_EQ(PublicationState::Complete,state);EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);
    EXPECT_EQ(ECANCELED,result.error);
}

} // namespace

namespace {
std::string InputHash(const std::string& value) {
    unsigned char sha[32];char hex[65];SHA256(reinterpret_cast<const unsigned char*>(value.data()),value.size(),sha);
    for(unsigned i=0;i<32;++i)snprintf(hex+i*2,3,"%02x",sha[i]);return hex;
}
class RuntimePackagePreparation : public ::testing::Test {
 protected:
    unique_fd root,stage,source,prepare_helper,execute_helper,cgroups,parent,mount;
    std::vector<unique_fd> archives;
    std::vector<std::string> archive_names,stages;
    PackagePreparation plan;
    PackagePreparer* worker=nullptr;
    aegis_broker_owner* broker=nullptr;
    PackageExecutor* executor=nullptr;
    std::string directory,group;
    bool group_created=false,source_created=false;
    uint64_t job=0;
    unique_fd Helper(const char* name) {
        char path[4096];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);
        if(n<0)return unique_fd();path[n]=0;std::string text(path);text.resize(text.find_last_of('/')+1);text+=name;
        return unique_fd(open(text.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    }
    void SetUp() override {
        ASSERT_EQ(0u,getuid());ASSERT_EQ(0,setgroups(0,nullptr));
        struct sigaction a={};a.sa_handler=SIG_DFL;sigemptyset(&a.sa_mask);ASSERT_EQ(0,sigaction(SIGCHLD,&a,nullptr));
        unique_fd u(open("/proc/1/ns/user",O_RDONLY|O_CLOEXEC)),p(open("/proc/1/ns/pid",O_RDONLY|O_CLOEXEC)),m(open("/proc/1/ns/mnt",O_RDONLY|O_CLOEXEC));
        ASSERT_EQ(0,aegis_namespace_pin_host(u.get(),p.get(),m.get()));
        cgroups.reset(open("/sys/fs/cgroup",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(cgroups.ok());
        group="aegis-preparation-test-"+std::to_string(getpid());ASSERT_EQ(0,mkdirat(cgroups.get(),group.c_str(),0700));group_created=true;
        parent.reset(openat(cgroups.get(),group.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(parent.ok());
        ASSERT_EQ(0,fchown(parent.get(),0,0));ASSERT_EQ(0,fchmod(parent.get(),0700));
        ASSERT_EQ(0,WriteAt(parent.get(),"cgroup.subtree_control","+memory\n",0));
        char path[]="/data/local/tmp/aegis-preparation-XXXXXX";ASSERT_NE(nullptr,mkdtemp(path));directory=path;
        root.reset(open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(root.ok());
        ASSERT_EQ(0,mkdirat(root.get(),"stage",0700));stages.push_back("stage");stage.reset(openat(root.get(),"stage",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(stage.ok());
        prepare_helper=Helper("aegis-package-prepare");execute_helper=Helper("aegis-package-execute-probe");
        ASSERT_TRUE(prepare_helper.ok());ASSERT_TRUE(execute_helper.ok());
        source.reset(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(source.ok());
        unique_fd receipt(open("/system_ext/etc/aegis/runtime/generation.json",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(receipt.ok());
        std::array<char,16385> data;ssize_t n=read(receipt.get(),data.data(),data.size());ASSERT_GT(n,0);
        aegis_base_receipt expected={};ASSERT_EQ(0,aegis_base_parse_receipt(data.data(),n,&expected));
        plan.image={expected.bytes,expected.sha256};plan.execution.requester=10;plan.execution.serial=42;
        plan.execution.job=1;plan.execution.plan_sha256=std::string(64,'d');
        for(const char* kind:{"lib","app"}) {
            auto deb=Deb(kind,1);auto hash=InputHash(deb);std::string name="aegis-exec-"+std::string(kind)+"_1_all.deb";
            ASSERT_EQ(0,WriteAt(root.get(),name.c_str(),deb));archive_names.push_back(name);
            archives.emplace_back(openat(root.get(),name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));ASSERT_TRUE(archives.back().ok());
            plan.archives.push_back({deb.size(),hash});plan.execution.items.push_back(name);
        }
    }
    std::vector<int> Fds() { std::vector<int> out;for(const auto& fd:archives)out.push_back(fd.get());return out; }
    int Start() { return PackagePreparerStart(parent.get(),stage.get(),source.get(),prepare_helper.get(),Fds(),plan,&worker); }
    void Ready() {
        ASSERT_EQ(0,Start())<<strerror(errno);PackagePreparationResult result;int fd=-1;
        ASSERT_EQ(0,PackagePreparerFinish(&worker,false,9000,&result,&fd))<<strerror(errno);mount.reset(fd);
        ASSERT_EQ(PackagePreparationOutcome::Prepared,result.outcome)<<result.error;ASSERT_TRUE(mount.ok());
    }
    void Broker() {
        ASSERT_EQ(0,aegis_broker_owner_create(parent.get(),source.get(),execute_helper.get(),execute_helper.get(),&broker));
        plan.execution.job=0;
        ASSERT_EQ(0,BrokerPrepareCandidate(broker,plan,parent.get(),stage.get(),source.get(),prepare_helper.get(),
                                          execute_helper.get(),Fds(),Deadline(),&job))<<strerror(errno);
        ASSERT_GT(job,0u);
    }
    void AwaitPrepared() {
        PublicationState state=PublicationState::Preparing;PackageExecutionResult result;
        for(unsigned i=0;i<900 && state==PublicationState::Preparing;++i) {
            ASSERT_EQ(0,aegis_broker_owner_reap_publications(broker));
            ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));
            if(state==PublicationState::Preparing)usleep(10000);
        }
        ASSERT_EQ(PublicationState::Prepared,state)<<result.error;
    }
    void ReusePreparedImage() {
        unique_fd image_root(openat(mount.get(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(image_root.ok());
        ASSERT_EQ(0,syncfs(image_root.get()));image_root.reset();mount.reset();NoLoop();ASSERT_FALSE(HasFailure());
        source.reset(openat(stage.get(),"candidate.ext4",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(source.ok());
        struct stat st;ASSERT_EQ(0,fstat(source.get(),&st));SHA256_CTX hash;ASSERT_EQ(1,SHA256_Init(&hash));
        std::array<unsigned char,128*1024> buffer;
        for(off_t at=0;at<st.st_size;) {
            ssize_t n=TEMP_FAILURE_RETRY(pread(source.get(),buffer.data(),buffer.size(),at));ASSERT_GT(n,0);
            ASSERT_EQ(1,SHA256_Update(&hash,buffer.data(),n));at+=n;
        }
        unsigned char digest[32];char hex[65];ASSERT_EQ(1,SHA256_Final(digest,&hash));
        for(unsigned i=0;i<32;++i)snprintf(hex+2*i,3,"%02x",digest[i]);plan.image={static_cast<uint64_t>(st.st_size),hex};
        ASSERT_EQ(0,mkdirat(root.get(),"stage-again",0700));stages.push_back("stage-again");
        stage.reset(openat(root.get(),"stage-again",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(stage.ok());plan.execution.job++;
    }
    int Stop() {
        aegis_broker_request r={};r.magic=AEGIS_BROKER_MAGIC;r.version=AEGIS_BROKER_VERSION;
        r.operation=AEGIS_BROKER_STOP_USER;r.user=10;r.deadline_ns=Deadline();r.sequence=3;
        aegis_broker_state state=AEGIS_BROKER_SEALED;int value=aegis_broker_owner_apply(broker,&r,&state);
        if(!value)EXPECT_EQ(AEGIS_BROKER_ABSENT,state);return value;
    }
    int MatchingLoops() {
        struct stat target;if(fstatat(stage.get(),"candidate.ext4",&target,AT_SYMLINK_NOFOLLOW)<0)return -1;
        DIR* entries=opendir("/sys/block");if(!entries)return -1;int count=0;
        while(auto* entry=readdir(entries)) {
            if(strncmp(entry->d_name,"loop",4))continue;
            std::string path="/dev/block/"+std::string(entry->d_name);unique_fd loop(open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
            if(!loop.ok())continue;loop_info64 info={};
            if(ioctl(loop.get(),LOOP_GET_STATUS64,&info)==0 && info.lo_device==static_cast<uint64_t>(target.st_dev)
               && info.lo_inode==target.st_ino)++count;
        }
        closedir(entries);return count;
    }
    void NoLoop() {
        int found=-1;for(unsigned i=0;i<200;++i) { found=MatchingLoops();if(found==0)break;usleep(10000); }
        EXPECT_EQ(0,found);
    }
    void TearDown() override {
        if(broker) { EXPECT_EQ(0,aegis_broker_owner_stop_all(broker,Deadline()));EXPECT_EQ(0,aegis_broker_owner_release(&broker)); }
        if(executor) { PackageExecutionResult result;EXPECT_EQ(0,PackageExecutorFinish(&executor,true,9000,&result)); }
        if(worker) { PackagePreparationResult result;int fd=-1;EXPECT_EQ(0,PackagePreparerFinish(&worker,true,9000,&result,&fd));if(fd>=0)close(fd); }
        mount.reset();
        if(parent.ok())EXPECT_EQ("populated 0\nfrozen 0\n",AptImageFixture::read(parent.get(),"cgroup.events"));
        parent.reset();if(group_created)EXPECT_EQ(0,unlinkat(cgroups.get(),group.c_str(),AT_REMOVEDIR));
        archives.clear();source.reset();
        if(!HasFailure() && stage.ok()) {
            // Only this test's newly exclusive stage; preserve all failures.
            stage.reset();
            for(const auto& stage_name:stages) {
              stage.reset(openat(root.get(),stage_name.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(stage.ok());
              for(const char* name:{"candidate.ext4","request","marker"}) {
                struct stat st;if(fstatat(stage.get(),name,&st,AT_SYMLINK_NOFOLLOW)<0) { EXPECT_EQ(ENOENT,errno);continue; }
                ASSERT_TRUE(S_ISREG(st.st_mode));ASSERT_EQ(0u,st.st_uid);ASSERT_EQ(1u,st.st_nlink);
                EXPECT_EQ(0,unlinkat(stage.get(),name,0));
            }
              stage.reset();EXPECT_EQ(0,unlinkat(root.get(),stage_name.c_str(),AT_REMOVEDIR));
            }
            for(const auto& name:archive_names)EXPECT_EQ(0,unlinkat(root.get(),name.c_str(),0));
            if(source_created)EXPECT_EQ(0,unlinkat(root.get(),"source",0));
            root.reset();EXPECT_EQ(0,rmdir(directory.c_str()));
        } else if(!directory.empty())fprintf(stderr,"Preparation fixture preserved: %s\n",directory.c_str());
    }
};
TEST(PackagePreparationPlan, RequiresBoundedImageAndOneDigestPerExactArchive) {
    PackagePreparation p;p.execution={10,42,1,std::string(64,'a'),true,{"a.deb"}};
    p.image={4096,std::string(64,'b')};p.archives={{128,std::string(64,'c')}};
    ASSERT_EQ(0,PackagePreparationCheck(p));p.archives.clear();EXPECT_EQ(-1,PackagePreparationCheck(p));
    p.archives={{128,std::string(64,'c')}};p.image.bytes=4097;EXPECT_EQ(-1,PackagePreparationCheck(p));
    p.image.bytes=uint64_t{32}*1024*1024*1024+4096;EXPECT_EQ(-1,PackagePreparationCheck(p));
    p.image.bytes=4096;p.archives[0].sha256="invalid";EXPECT_EQ(-1,PackagePreparationCheck(p));
    p.execution.archives=false;p.execution.items={"app"};EXPECT_EQ(-1,PackagePreparationCheck(p));
    p.archives.clear();EXPECT_EQ(0,PackagePreparationCheck(p));
}
TEST_F(RuntimePackagePreparation, CopiedAndVerifiedArchivesExecuteThroughRealApt) {
    int before=CountFDs();Ready();ASSERT_FALSE(HasFatalFailure());EXPECT_EQ(before+1,CountFDs());
    for(size_t i=0;i<archives.size();++i) {
        auto data=AptImageFixture::read(mount.get(),("var/cache/apt/archives/"+archive_names[i]).c_str());
        EXPECT_EQ(plan.archives[i].sha256,InputHash(data));
    }
    ASSERT_EQ(0,PackageExecutorStart(parent.get(),stage.get(),mount.get(),execute_helper.get(),plan.execution,Deadline(),&executor))<<strerror(errno);
    PackageExecutionResult result;ASSERT_EQ(0,PackageExecutorFinish(&executor,false,9000,&result));
    ASSERT_EQ(PackageExecutionOutcome::NeedsValidation,result.outcome)<<"status="<<result.status<<" errno="<<result.error
        <<AptImageFixture::read(mount.get(),("var/log/aegis-package-"+std::to_string(plan.execution.job)+".log").c_str());
    EXPECT_EQ("1\n",AptImageFixture::read(mount.get(),"usr/share/aegis-exec-library"));
    EXPECT_EQ("#!/bin/sh\necho app-1\n",AptImageFixture::read(mount.get(),"usr/bin/aegis-exec-app"));
    EXPECT_NE(std::string::npos,AptImageFixture::read(mount.get(),"var/log/aegis-exec-script").find("postinst-1"));
    struct stat st;ASSERT_EQ(0,fstatat(mount.get(),"var/lib/aegis-exec-owned",&st,AT_SYMLINK_NOFOLLOW));
    EXPECT_EQ(1005042u,st.st_uid);mount.reset();EXPECT_EQ(before,CountFDs());NoLoop();
}
TEST_F(RuntimePackagePreparation, BrokerPrivateUmaskDoesNotBreakCandidatePermissions) {
    mode_t original=umask(0077);int started=Start();mode_t immediate=umask(original);
    EXPECT_EQ(0077u,immediate);ASSERT_EQ(0,started)<<strerror(errno);
    PackagePreparationResult result;int candidate=-1;
    ASSERT_EQ(0,PackagePreparerFinish(&worker,false,9000,&result,&candidate));mount.reset(candidate);
    ASSERT_EQ(PackagePreparationOutcome::Prepared,result.outcome)<<result.error;
    EXPECT_EQ(original,umask(original));
    struct stat st;
    ASSERT_EQ(0,fstatat(mount.get(),"var/cache/apt/archives",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(0755u,st.st_mode&07777);
    ASSERT_EQ(0,fstatat(mount.get(),("var/cache/apt/archives/"+archive_names[0]).c_str(),&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(0644u,st.st_mode&07777);
    ASSERT_EQ(0,fstatat(stage.get(),"candidate.ext4",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(0600u,st.st_mode&07777);
    ASSERT_EQ(0,fstatat(stage.get(),"request",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(0400u,st.st_mode&07777);
}
TEST_F(RuntimePackagePreparation, BrokerRetainsSameJobThroughPreparationAndApt) {
    Broker();ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,BrokerStartExecution(broker,10,43,job,plan.execution.plan_sha256,Deadline()));EXPECT_EQ(ESTALE,errno);
    AwaitPrepared();ASSERT_FALSE(HasFatalFailure());EXPECT_EQ(1,MatchingLoops());
    ASSERT_EQ(0,BrokerStartExecution(broker,10,42,job,plan.execution.plan_sha256,Deadline()))<<strerror(errno);
    PublicationState state=PublicationState::Running;PackageExecutionResult result;
    for(unsigned i=0;i<900 && state!=PublicationState::Complete;++i) {
        ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));
        if(state!=PublicationState::Complete)usleep(10000);
    }
    EXPECT_EQ(PublicationState::Complete,state);EXPECT_EQ(PackageExecutionOutcome::NeedsValidation,result.outcome);
    EXPECT_EQ(0,result.error);NoLoop();
}
TEST_F(RuntimePackagePreparation, ExistingVerifiedArchivesCanBeUsedAgain) {
    Ready();ASSERT_FALSE(HasFatalFailure());ReusePreparedImage();ASSERT_FALSE(HasFatalFailure());
    Ready();ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,PackageExecutorStart(parent.get(),stage.get(),mount.get(),execute_helper.get(),plan.execution,Deadline(),&executor));
    PackageExecutionResult result;ASSERT_EQ(0,PackageExecutorFinish(&executor,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::NeedsValidation,result.outcome);EXPECT_EQ(0,result.error);
    EXPECT_EQ("1\n",AptImageFixture::read(mount.get(),"usr/share/aegis-exec-library"));
}
TEST_F(RuntimePackagePreparation, ChangedCachedArchiveIsRejectedWithoutReplacement) {
    Ready();ASSERT_FALSE(HasFatalFailure());
    std::string path="var/cache/apt/archives/"+archive_names[0];
    ASSERT_EQ(0,WriteAt(mount.get(),path.c_str(),"changed-cache\n",O_TRUNC));
    ReusePreparedImage();ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,Start());PackagePreparationResult result;int candidate=-1;
    ASSERT_EQ(0,PackagePreparerFinish(&worker,false,9000,&result,&candidate));
    EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(ESTALE,result.error);EXPECT_EQ(-1,candidate);NoLoop();
}
TEST_F(RuntimePackagePreparation, WrongImageHashCannotProduceMount) {
    plan.image.sha256=std::string(64,'0');ASSERT_EQ(0,Start());PackagePreparationResult result;int candidate=-1;
    ASSERT_EQ(0,PackagePreparerFinish(&worker,false,9000,&result,&candidate));
    EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(ESTALE,result.error);EXPECT_EQ(-1,candidate);NoLoop();
}
TEST_F(RuntimePackagePreparation, WrongArchiveHashCannotProduceMount) {
    plan.archives[0].sha256=std::string(64,'0');ASSERT_EQ(0,Start());PackagePreparationResult result;int candidate=-1;
    ASSERT_EQ(0,PackagePreparerFinish(&worker,false,9000,&result,&candidate));
    EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(ESTALE,result.error);EXPECT_EQ(-1,candidate);NoLoop();
}
TEST_F(RuntimePackagePreparation, ExistingStageIsNeverAdoptedOrOverwritten) {
    ASSERT_EQ(0,WriteAt(stage.get(),"marker","preserved\n"));ASSERT_EQ(0,Start());PackagePreparationResult result;int candidate=-1;
    ASSERT_EQ(0,PackagePreparerFinish(&worker,false,9000,&result,&candidate));
    EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(EEXIST,result.error);EXPECT_EQ(-1,candidate);
    EXPECT_EQ("preserved\n",AptImageFixture::read(stage.get(),"marker"));
    struct stat st;EXPECT_EQ(-1,fstatat(stage.get(),"candidate.ext4",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackagePreparation, StopClosesQueuedMountBeforeAnyHandoff) {
    Broker();ASSERT_FALSE(HasFatalFailure());bool done=false;
    for(unsigned i=0;i<900;++i) {
        if(AptImageFixture::read(parent.get(),"cgroup.events").find("populated 0\n")!=std::string::npos) { done=true;break; }
        usleep(10000);
    }
    ASSERT_TRUE(done);ASSERT_EQ(1,MatchingLoops());ASSERT_EQ(0,Stop());NoLoop();
    PublicationState state;PackageExecutionResult result;
    EXPECT_EQ(-1,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackagePreparation, PreparedMountStaysOwnedUntilUserStop) {
    Broker();ASSERT_FALSE(HasFatalFailure());AwaitPrepared();ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(1,MatchingLoops());EXPECT_EQ(-1,aegis_broker_owner_release(&broker));EXPECT_EQ(EBUSY,errno);
    ASSERT_EQ(0,Stop());NoLoop();
}
TEST_F(RuntimePackagePreparation, StopDuringObservedCopyReapsWorkerAndRetainsPartialEvidence) {
    constexpr size_t total=512u*1024u*1024u;
    unique_fd writable(openat(root.get(),"source",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600));ASSERT_TRUE(writable.ok());source_created=true;
    ASSERT_EQ(0,ftruncate(writable.get(),total));ASSERT_EQ(0,fsync(writable.get()));writable.reset();
    source.reset(openat(root.get(),"source",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(source.ok());
    SHA256_CTX hash;ASSERT_EQ(1,SHA256_Init(&hash));std::array<unsigned char,128*1024> zero={};
    for(size_t i=0;i<total;i+=zero.size())ASSERT_EQ(1,SHA256_Update(&hash,zero.data(),zero.size()));
    unsigned char bytes[32];char digest[65];ASSERT_EQ(1,SHA256_Final(bytes,&hash));
    for(unsigned i=0;i<32;++i)snprintf(digest+2*i,3,"%02x",bytes[i]);plan.image={total,digest};
    unique_fd notify(inotify_init1(IN_CLOEXEC|IN_NONBLOCK));ASSERT_TRUE(notify.ok());
    ASSERT_GE(inotify_add_watch(notify.get(),(directory+"/stage").c_str(),IN_MODIFY),0);
    Broker();ASSERT_FALSE(HasFatalFailure());unique_fd own(openat(parent.get(),"u10-s42",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(own.ok());
    bool modified=false;
    for(unsigned i=0;i<100 && !modified;++i) {
        pollfd ready={notify.get(),POLLIN,0};ASSERT_GE(poll(&ready,1,50),0);
        alignas(inotify_event) char events[4096];ssize_t n=read(notify.get(),events,sizeof(events));
        if(n<0) { ASSERT_EQ(EAGAIN,errno);continue; }
        for(size_t at=0;at+sizeof(inotify_event)<=static_cast<size_t>(n);) {
            auto* event=reinterpret_cast<inotify_event*>(events+at);
            if(event->len && !strcmp(event->name,"candidate.ext4"))modified=true;
            at+=sizeof(*event)+event->len;
        }
    }
    ASSERT_TRUE(modified);ASSERT_EQ(0,WriteAt(own.get(),"cgroup.freeze","1\n",0));bool frozen=false;
    for(unsigned i=0;i<500;++i) { if(AptImageFixture::read(own.get(),"cgroup.events").find("frozen 1\n")!=std::string::npos) { frozen=true;break; }usleep(10000); }
    ASSERT_TRUE(frozen);struct stat partial;ASSERT_EQ(0,fstatat(stage.get(),"candidate.ext4",&partial,AT_SYMLINK_NOFOLLOW));
    ASSERT_GT(partial.st_size,0);ASSERT_LT(static_cast<uint64_t>(partial.st_size),total);
    PublicationState state;PackageExecutionResult result;
    ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));EXPECT_EQ(PublicationState::Preparing,state);
    ASSERT_EQ(0,Stop())<<strerror(errno);EXPECT_EQ(0,MatchingLoops());
    struct stat after;ASSERT_EQ(0,fstatat(stage.get(),"candidate.ext4",&after,AT_SYMLINK_NOFOLLOW));
    EXPECT_EQ(partial.st_size,after.st_size);
}
} // namespace
