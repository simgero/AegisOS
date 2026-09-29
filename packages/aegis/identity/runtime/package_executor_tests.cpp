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
        r.operation=AEGIS_BROKER_STOP_USER;r.user=user;r.serial=42;r.sequence=2;r.deadline_ns=Deadline();
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
    p.job=0;EXPECT_EQ(-1,PackageExecutionCheck(p));p.job=1;p.requester=0;EXPECT_EQ(-1,PackageExecutionCheck(p));
}
TEST_F(RuntimePackageExecutor, ExecutesActualInstallUpgradeRemoveAndClosesOwnedReferences) {
    Archives(1);ASSERT_FALSE(HasFatalFailure());Completed();ASSERT_FALSE(HasFailure());
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
TEST_F(RuntimePackageExecutor, PartialStartRetainsCgroupAndDescriptorsUntilFinish) {
    Archives(1);ASSERT_FALSE(HasFatalFailure());int before=CountFDs();helper.reset();before--;
    EXPECT_EQ(-1,Start());ASSERT_NE(nullptr,worker);
    PackageExecutionResult result;ASSERT_EQ(0,PackageExecutorFinish(&worker,true,9000,&result)) << strerror(errno);
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(before,CountFDs());
}
TEST_F(RuntimePackageExecutor, ProductionEntryRejectsDeveloperTestDomain) {
    Archives(1);Helper("aegis-package-execute");ASSERT_FALSE(HasFatalFailure());int before=CountFDs();
    EXPECT_EQ(-1,Start());ASSERT_NE(nullptr,worker);
    PackageExecutionResult result;ASSERT_EQ(0,PackageExecutorFinish(&worker,true,9000,&result)) << strerror(errno);
    EXPECT_EQ(PackageExecutionOutcome::Unconfirmed,result.outcome);EXPECT_EQ(before,CountFDs());
    EXPECT_EQ("<unavailable>",AptImageFixture::read(candidate.get(),"var/log/aegis-exec-script"));
}
TEST_F(RuntimePackageExecutor, BrokerBindsApprovalAndUserStopCancelsActualAptOnlyForRequester) {
    Archives(1,true);Broker();ASSERT_FALSE(HasFatalFailure());uint64_t first=Prepared(),other=Prepared(11);ASSERT_FALSE(HasFailure());
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
} // namespace
