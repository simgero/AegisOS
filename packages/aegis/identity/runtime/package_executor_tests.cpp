#include "ce_live_test.h"
// Compile on the SSH builder, execute only in local QEMU. Synthetic packages
// in an exclusively copied base; no live AOSP users, passwords or CE mutation.
#include "package_executor.h"
#include "package_candidate_labels.h"
#include <sys/xattr.h>
#include "package_plan.h"
#include "package_execution_protocol.h"
#include "package_apt_fixture.h"
#include "broker_owner_package.h"
#include "broker_owner_selection.h"
#include "context.h"
#include "namespace.h"
#include <gtest/gtest.h>
#include <json/json.h>
#include <android-base/unique_fd.h>
#include <dirent.h>
#include <grp.h>
#include <limits.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/statvfs.h>
#include <sys/inotify.h>
#include <poll.h>
#include <time.h>
#include <memory>
using namespace aegis;
using android::base::unique_fd;
namespace {
std::string InputHash(const std::string& value);
std::string ControlFile(int root,const char* path) {
    unique_fd fd(openat(root,path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
    if(!fd.ok())return "<unavailable>";
    std::string out;char buffer[8192];
    for(;;) {
        ssize_t n=TEMP_FAILURE_RETRY(read(fd.get(),buffer,sizeof(buffer)));
        if(n<0)return "<unavailable>";
        if(!n)return out;
        out.append(buffer,n);if(out.size()>(64u<<20))return "<unavailable>";
    }
}
void InitialReview(int root,aegis_package_execution_review* review) {
    auto status=ControlFile(root,"var/lib/dpkg/status");ASSERT_NE("<unavailable>",status);
    review->present=1;memcpy(review->initial_status,InputHash(status).c_str(),65);
    struct stat st;
    if(fstatat(root,"var/lib/apt/extended_states",&st,AT_SYMLINK_NOFOLLOW)<0) {
        ASSERT_EQ(ENOENT,errno);review->apt_state_presence=1;
    } else {
        auto state=ControlFile(root,"var/lib/apt/extended_states");ASSERT_NE("<unavailable>",state);
        review->apt_state_presence=2;review->apt_state_bytes=state.size();
        memcpy(review->initial_apt_state,InputHash(state).c_str(),65);
    }
}
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
int ReferencesTo(const struct stat& original) {
    DIR* entries=opendir("/proc/self/fd");if(!entries)return -1;
    int count=0;
    while(auto* entry=readdir(entries)) {
        char* end=nullptr;long fd=strtol(entry->d_name,&end,10);
        if(end==entry->d_name || *end || fd<0 || fd>INT_MAX)continue;
        struct stat st;
        if(fstat(fd,&st)==0 && st.st_dev==original.st_dev && st.st_ino==original.st_ino)count++;
    }
    closedir(entries);return count;
}
void Octal(char* out,size_t width,uint64_t value) {
    snprintf(out,width,"%0*llo",static_cast<int>(width-1),static_cast<unsigned long long>(value));
}
void Tar(std::string& archive,const char* name,const std::string& contents,unsigned mode=0644,char type='0') {
    char header[512]={};EXPECT_LT(strlen(name),100u);memcpy(header,name,strlen(name));
    Octal(header+100,8,mode);Octal(header+108,8,0);Octal(header+116,8,0);
    Octal(header+124,12,contents.size());Octal(header+136,12,0);
    memset(header+148,' ',8);header[156]=type;memcpy(header+257,"ustar",5);memcpy(header+263,"00",2);
    unsigned sum=0;for(unsigned char b:header)sum+=b;
    snprintf(header+148,8,"%06o",sum);header[155]=' ';
    archive.append(header,512);archive+=contents;archive.append((512-contents.size()%512)%512,'\0');
}
void Ar(std::string& archive,const char* name,const std::string& data) {
    char h[61];int n=snprintf(h,sizeof(h),"%-16s%-12u%-6u%-6u%-8o%-10zu`\n",name,0,0,0,0100644,data.size());
    EXPECT_EQ(60,n);archive.append(h,60);archive+=data;if(data.size()%2)archive+='\n';
}
std::string Deb(const char* kind,int version,bool waiting=false,const std::string& after="",bool documentation=false) {
    std::string v=std::to_string(version),control,data;
    std::string fields="Package: aegis-exec-"+std::string(kind)+"\nVersion: "+v+
        "\nArchitecture: all\nMaintainer: AEGIS fixture <test@invalid>\nDescription: Local device fixture\n";
    bool app=!strcmp(kind,"app");if(app)fields+="Depends: aegis-exec-lib (= "+v+")\n";
    Tar(control,"./control",fields);
    if(app) {
        Tar(control,"./conffiles","/etc/aegis-exec.conf\n");
        Tar(control,"./postinst","#!/bin/sh\nset -eu\necho postinst-"+v+" >> /var/log/aegis-exec-script\n"
            "mkdir -p /var/lib/aegis-exec-owned\nchown 42:42 /var/lib/aegis-exec-owned\n"
            +std::string(waiting?"echo waiting > /var/log/aegis-exec-waiting\nwhile :; do sleep 1; done\n":"echo done >> /var/log/aegis-exec-script\n")+after,0755);
        Tar(control,"./prerm","#!/bin/sh\necho prerm >> /var/log/aegis-exec-script\n",0755);
        Tar(data,"./usr/bin/aegis-exec-app","#!/bin/sh\necho app-"+v+"\n",0755);
        Tar(data,"./etc/aegis-exec.conf","version="+v+"\n");
        if(documentation) {
            for(const char* name:{"./usr/share/doc/aegis-exec-app/","./usr/share/man/man1/",
                                  "./usr/share/info/","./usr/share/locale/zxx/",
                                  "./usr/share/locale/zxx/LC_MESSAGES/"})Tar(data,name,"",0755,'5');
            Tar(data,"./usr/share/doc/aegis-exec-app/README","docs-"+v+"\n");
            Tar(data,"./usr/share/man/man1/aegis-exec-app.1","manual-"+v+"\n");
            Tar(data,"./usr/share/info/aegis-exec-app.info","info-"+v+"\n");
            Tar(data,"./usr/share/locale/zxx/LC_MESSAGES/aegis-exec-app.mo","messages-"+v+"\n");
        }
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
    void Archives(int version,bool waiting=false,const std::string& after="",bool documentation=false) {
        plan.archives=true;plan.items.clear();
        for(const char* name:{"var/cache","var/cache/apt","var/cache/apt/archives"}) {
            int made=mkdirat(candidate.get(),name,0755);
            ASSERT_TRUE(made==0 || errno==EEXIST) << strerror(errno);
        }
        for(const char* kind:{"lib","app"}) {
            std::string name="aegis-exec-"+std::string(kind)+"_"+std::to_string(version)+"_all.deb";
            std::string path="var/cache/apt/archives/"+name;
            ASSERT_EQ(0,WriteAt(candidate.get(),path.c_str(),Deb(kind,version,waiting,after,documentation))) << strerror(errno);
            plan.items.push_back(name);
        }
    }
    void Review(int before,int after) {
        plan.review={};InitialReview(candidate.get(),&plan.review);ASSERT_FALSE(HasFatalFailure());
        std::sort(plan.items.begin(),plan.items.end());
        for(unsigned i=0;i<2;i++) {
            auto& e=plan.review.effects[i];
            strcpy(e.name,i?"aegis-exec-lib":"aegis-exec-app");strcpy(e.architecture,"all");
            if(before)snprintf(e.before,sizeof(e.before),"%d",before);
            if(after)snprintf(e.after,sizeof(e.after),"%d",after);
            e.reason=i?2:1;
        }
        ASSERT_EQ(0,PackageExecutionCheck(plan));
    }
    void RejectedBeforeScripts(int expected) {
        ASSERT_EQ(0,Start())<<strerror(errno);PackageExecutionResult result;
        ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
        EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);
        EXPECT_EQ(expected,result.error)<<"status="<<result.status;
        EXPECT_EQ("<unavailable>",ControlFile(candidate.get(),"var/log/aegis-exec-script"));
        EXPECT_EQ("<unavailable>",ControlFile(candidate.get(),"usr/bin/aegis-exec-app"));
    }
    int Start() { return PackageExecutorStart(parent.get(),stage.get(),candidate.get(),helper.get(),plan,Deadline(),&worker); }
    void Completed() {
        int before=CountFDs();ASSERT_GT(before,0);
        ASSERT_EQ(0,Start()) << strerror(errno);
        PackageExecutionResult result;ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result)) << strerror(errno);
        EXPECT_EQ(PackageExecutionOutcome::NeedsValidation,result.outcome) << "status=" << result.status << " error=" << result.error
            << AptImageFixture::read(candidate.get(),("var/log/aegis-package-"+std::to_string(plan.job)+".log").c_str())
            << AptImageFixture::read(candidate.get(),("var/log/aegis-package-"+std::to_string(plan.job)+"-check.log").c_str())
            << AptImageFixture::read(candidate.get(),("var/log/aegis-package-"+std::to_string(plan.job)+"-audit.log").c_str())
            << AptImageFixture::read(candidate.get(),("var/log/aegis-package-"+std::to_string(plan.job)+"-verify.log").c_str())
            << AptImageFixture::read(candidate.get(),("var/log/aegis-package-"+std::to_string(plan.job)+"-simulate.log").c_str())
            << AptImageFixture::read(candidate.get(),("var/log/aegis-package-"+std::to_string(plan.job)+"-simulate.json").c_str());
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
TEST_F(RuntimePackageExecutor, ReviewedInstallUpgradeRemovePreservesConffilesAndDependencyMarks) {
    Archives(1);Review(0,1);ASSERT_FALSE(HasFatalFailure());Completed();ASSERT_FALSE(HasFailure());
    auto automatic=ControlFile(candidate.get(),"var/lib/apt/extended_states");
    EXPECT_NE(std::string::npos,automatic.find("Package: aegis-exec-lib\n"));
    EXPECT_EQ(std::string::npos,automatic.find("Package: aegis-exec-app\n"));
    Remount();ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,WriteAt(candidate.get(),"etc/aegis-exec.conf","local=preserved\n",O_TRUNC));
    Archives(2);Review(1,2);ASSERT_FALSE(HasFatalFailure());Completed();ASSERT_FALSE(HasFailure());
    EXPECT_EQ("local=preserved\n",ControlFile(candidate.get(),"etc/aegis-exec.conf"));
    EXPECT_EQ("2\n",ControlFile(candidate.get(),"usr/share/aegis-exec-library"));
    automatic=ControlFile(candidate.get(),"var/lib/apt/extended_states");
    EXPECT_NE(std::string::npos,automatic.find("Package: aegis-exec-lib\n"));
    EXPECT_EQ(std::string::npos,automatic.find("Package: aegis-exec-app\n"));
    Remount();ASSERT_FALSE(HasFatalFailure());plan.archives=false;plan.items={"aegis-exec-app","aegis-exec-lib"};
    Review(2,0);ASSERT_FALSE(HasFatalFailure());Completed();ASSERT_FALSE(HasFailure());
    EXPECT_EQ("<unavailable>",ControlFile(candidate.get(),"usr/bin/aegis-exec-app"));
    EXPECT_EQ("local=preserved\n",ControlFile(candidate.get(),"etc/aegis-exec.conf"));
}
TEST_F(RuntimePackageExecutor, ChangedInitialStatusRejectsBeforeAnyPackageScript) {
    Archives(1);Review(0,1);ASSERT_FALSE(HasFatalFailure());
    plan.review.initial_status[0]=plan.review.initial_status[0]=='a'?'b':'a';
    RejectedBeforeScripts(ESTALE);
}
TEST_F(RuntimePackageExecutor, ChangedInitialAutomaticStateRejectsBeforeAnyPackageScript) {
    Archives(1);Review(0,1);ASSERT_FALSE(HasFatalFailure());
    auto state=ControlFile(candidate.get(),"var/lib/apt/extended_states");
    if(state=="<unavailable>")state="";
    ASSERT_EQ(0,WriteAt(candidate.get(),"var/lib/apt/extended_states",state+"\n",O_CREAT|O_TRUNC));
    RejectedBeforeScripts(ESTALE);
}
TEST_F(RuntimePackageExecutor, DifferentSimulatedVersionRejectsBeforeAnyPackageScript) {
    Archives(1);Review(0,2);ASSERT_FALSE(HasFatalFailure());RejectedBeforeScripts(ESTALE);
}
TEST_F(RuntimePackageExecutor, MaintainerScriptCannotReplaceTrustedHookOrConfiguration) {
    Archives(1,false,
        "if echo changed > /tmp/aegis-trusted/config; then exit 81; fi\n"
        "if echo changed > /tmp/aegis-trusted/hook; then exit 82; fi\n"
        "if rm /tmp/aegis-trusted/config; then exit 83; fi\n"
        "echo readonly-confirmed > /var/log/aegis-trusted-proof\n");
    Review(0,1);ASSERT_FALSE(HasFatalFailure());Completed();ASSERT_FALSE(HasFailure());
    EXPECT_EQ("readonly-confirmed\n",ControlFile(candidate.get(),"var/log/aegis-trusted-proof"));
}
// Each policy violation is written by a real successfully completing Debian
// maintainer script in the exclusive candidate, not by a mocked result flag.
TEST_F(RuntimePackageExecutor, RejectsPersonalAccountIntroducedByMaintainerScript) {
    Archives(1,false,
        "printf 'rogue:x:1001:1001:rogue:/home/rogue:/bin/sh\\n' >> /etc/passwd\n"
        "printf 'rogue:!:0:0:99999:7:::\\n' >> /etc/shadow\n"
        "printf 'rogue:x:1001:\\n' >> /etc/group\n"
        "printf 'rogue:!::\\n' >> /etc/gshadow\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EPERM,result.error);
    EXPECT_EQ(0,result.status);EXPECT_EQ(nullptr,worker);
    EXPECT_NE(std::string::npos,AptImageFixture::read(candidate.get(),"etc/passwd").find("rogue:x:1001"));
}
TEST_F(RuntimePackageExecutor, RejectsUnlockedLinuxCredentialAfterSuccessfulApt) {
    Archives(1,false,"sed -i 's/^runtime:[^:]*:/runtime:active:/' /etc/shadow\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EPERM,result.error);EXPECT_EQ(0,result.status);
}
TEST_F(RuntimePackageExecutor, RejectsAlternateNssIdentityAuthority) {
    Archives(1,false,"sed -i 's/^passwd:.*/passwd: files ldap/' /etc/nsswitch.conf\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EPERM,result.error);EXPECT_EQ(0,result.status);
}
TEST_F(RuntimePackageExecutor, RejectsPersistentFifoInsteadOfHangingWhileOpeningIt) {
    Archives(1,false,"mkfifo /var/lib/aegis-exec-fifo\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EPERM,result.error);EXPECT_EQ(0,result.status);
    ASSERT_EQ(0,unlinkat(candidate.get(),"var/lib/aegis-exec-fifo",0));
}
TEST_F(RuntimePackageExecutor, RejectsPackagedFifoBeforeDpkgCanBlockReadingIt) {
    Archives(1,false,"rm /usr/bin/ls; mkfifo /usr/bin/ls\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EPERM,result.error);
    EXPECT_EQ(0,result.status);EXPECT_EQ(nullptr,worker);
    EXPECT_EQ("<unavailable>",AptImageFixture::read(candidate.get(),"var/log/aegis-package-1-verify.log"));
    struct stat st;ASSERT_EQ(0,fstatat(candidate.get(),"usr/bin/ls",&st,AT_SYMLINK_NOFOLLOW));
    EXPECT_TRUE(S_ISFIFO(st.st_mode));ASSERT_EQ(0,unlinkat(candidate.get(),"usr/bin/ls",0));
}
TEST_F(RuntimePackageExecutor, RejectsSetidProgramCreatedByMaintainerScript) {
    Archives(1,false,"chmod 4755 /usr/bin/aegis-exec-app\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EPERM,result.error);EXPECT_EQ(0,result.status);
}
TEST_F(RuntimePackageExecutor, ReapsObservedBackgroundScriptBeforeConsistencyChecks) {
    Archives(1,false,
        "grep '^CapEff:' /proc/self/status > /var/log/aegis-exec-caps\n"
        "setpriv --reuid=42 --regid=42 --clear-groups /bin/sh -c '"
        "id -u > /var/lib/aegis-exec-owned/background; while :; do sleep 1; done' &\n"
        "while [ ! -s /var/lib/aegis-exec-owned/background ]; do sleep 1; done\n");
    ASSERT_FALSE(HasFailure());Completed();ASSERT_FALSE(HasFailure());
    EXPECT_EQ("42\n",AptImageFixture::read(candidate.get(),"var/lib/aegis-exec-owned/background"));
    EXPECT_NE(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-exec-caps").find("00000000000000db"));
    EXPECT_EQ("",AptImageFixture::read(candidate.get(),"var/log/aegis-package-1-audit.log"));
}
TEST_F(RuntimePackageExecutor, RejectsMissingDpkgControlMetadata) {
    Archives(1,false,"rm /var/lib/dpkg/info/base-files.list\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);
    EXPECT_NE(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-package-1-audit.log").find("base-files"));
}
TEST_F(RuntimePackageExecutor, RejectsNewMissingDocumentationInsteadOfBroadSlimExemption) {
    Archives(1,false,"rm /usr/share/doc/coreutils/copyright\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EBADMSG,result.error);
    EXPECT_NE(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-package-1-verify.log").find("/usr/share/doc/coreutils/copyright"));
}
TEST_F(RuntimePackageExecutor, InstallsCompletePayloadDespiteInheritedPathExclusions) {
    const char* config="etc/dpkg/dpkg.cfg.d/aegis-test-excludes";
    const std::string exclusions="path-exclude=/usr/share/doc/*\npath-exclude=/usr/share/man/*\n"
        "path-exclude=/usr/share/info/*\npath-exclude=/usr/share/locale/*\n";
    ASSERT_EQ(0,WriteAt(candidate.get(),config,exclusions));
    Archives(1,false,"",true);ASSERT_FALSE(HasFailure());
    Review(0,1);ASSERT_FALSE(HasFailure());Completed();ASSERT_FALSE(HasFailure());
    EXPECT_EQ(exclusions,ControlFile(candidate.get(),config));
    EXPECT_EQ("docs-1\n",ControlFile(candidate.get(),"usr/share/doc/aegis-exec-app/README"));
    EXPECT_EQ("manual-1\n",ControlFile(candidate.get(),"usr/share/man/man1/aegis-exec-app.1"));
    EXPECT_EQ("info-1\n",ControlFile(candidate.get(),"usr/share/info/aegis-exec-app.info"));
    EXPECT_EQ("messages-1\n",ControlFile(candidate.get(),"usr/share/locale/zxx/LC_MESSAGES/aegis-exec-app.mo"));
    EXPECT_EQ(std::string::npos,ControlFile(candidate.get(),"var/log/aegis-package-1-verify.log").find("aegis-exec-app"));
}
TEST_F(RuntimePackageExecutor, StillRejectsNewDocumentationRemovedByPackageScript) {
    Archives(1,false,"rm /usr/share/doc/aegis-exec-app/README\n",true);
    ASSERT_FALSE(HasFailure());Review(0,1);ASSERT_FALSE(HasFailure());
    ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EBADMSG,result.error);
    EXPECT_EQ(0,result.status);
    EXPECT_NE(std::string::npos,ControlFile(candidate.get(),"var/log/aegis-package-1-verify.log")
        .find("missing     /usr/share/doc/aegis-exec-app/README"));
}
TEST_F(RuntimePackageExecutor, CannotExtendMissingBaselineByRewritingItsLog) {
    // Upstream keeps this empty directory. Its path qualifies for the narrow
    // slim family, but it was PRESENT during the pre-action verification.
    struct stat original;
    ASSERT_EQ(0,fstatat(candidate.get(),"usr/share/man/man1",&original,AT_SYMLINK_NOFOLLOW));
    ASSERT_TRUE(S_ISDIR(original.st_mode));
    Archives(1,false,"rmdir /usr/share/man/man1\n"
        "printf 'missing     /usr/share/man/man1\\n' >> /var/log/aegis-package-1-verify-before.log\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EBADMSG,result.error);
    EXPECT_NE(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-package-1-verify.log").find("missing     /usr/share/man/man1\n"));
}
TEST_F(RuntimePackageExecutor, MissingProgramInInitialCandidateRejectsBeforePackageScripts) {
    ASSERT_EQ(0,unlinkat(candidate.get(),"usr/bin/ls",0));
    Archives(1);ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EBADMSG,result.error);
    EXPECT_EQ("<unavailable>",AptImageFixture::read(candidate.get(),"var/log/aegis-exec-script"));
    EXPECT_NE(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-package-1-verify-before.log").find("/usr/bin/ls"));
}
TEST_F(RuntimePackageExecutor, RejectsChangedPackagedProgramAfterSuccessfulMaintainerScript) {
    Archives(1,false,"printf changed > /usr/bin/ls\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EBADMSG,result.error);
    EXPECT_NE(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-package-1-verify.log").find("/usr/bin/ls"));
}
TEST_F(RuntimePackageExecutor, RejectsMissingPackagedProgramAfterSuccessfulMaintainerScript) {
    Archives(1,false,"rm /usr/bin/ls\n");
    ASSERT_FALSE(HasFailure());ASSERT_EQ(0,Start());PackageExecutionResult result;
    ASSERT_EQ(0,PackageExecutorFinish(&worker,false,9000,&result));
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EBADMSG,result.error);
    EXPECT_NE(std::string::npos,AptImageFixture::read(candidate.get(),"var/log/aegis-package-1-verify.log").find("/usr/bin/ls"));
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
TEST_F(RuntimePackageExecutor, SuccessfulAptRemainsOwnedUntilValidationOrCancellation) {
    Archives(1);Broker();ASSERT_FALSE(HasFatalFailure());int before=CountFDs();
    struct stat original;ASSERT_EQ(0,fstat(stage.get(),&original));
    uint64_t job=Prepared();ASSERT_FALSE(HasFailure());
    // The caller gives up its stage reference. The broker must retain the exact
    // object beyond actual APT exit, without sending an unowned fd to a client.
    stage.reset();before--;
    ASSERT_EQ(0,BrokerStartExecution(broker,10,42,job,plan.plan_sha256,Deadline())) << strerror(errno);
    PublicationState state=PublicationState::Running;PackageExecutionResult result;
    for(unsigned i=0;i<900 && state==PublicationState::Running;i++) {
        ASSERT_EQ(0,aegis_broker_owner_reap_publications(broker));
        ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.plan_sha256,&state,&result));
        if(state==PublicationState::Running)usleep(10000);
    }
    ASSERT_EQ(PublicationState::AwaitingValidation,state);
    EXPECT_EQ(PackageExecutionOutcome::NeedsValidation,result.outcome);
    EXPECT_EQ(0,result.status);EXPECT_EQ(0,result.error);
    EXPECT_EQ(before+1,CountFDs());EXPECT_EQ(1,ReferencesTo(original));
    EXPECT_EQ(-1,aegis_broker_owner_release(&broker));EXPECT_EQ(EBUSY,errno);
    uint64_t duplicate=0;
    EXPECT_EQ(-1,BrokerPrepareExecution(broker,plan,parent.get(),-1,candidate.get(),helper.get(),Deadline(),&duplicate));
    EXPECT_EQ(EBUSY,errno);EXPECT_EQ(0u,duplicate);
    EXPECT_EQ(-1,BrokerStartExecution(broker,10,42,job,plan.plan_sha256,Deadline()));EXPECT_EQ(EALREADY,errno);
    EXPECT_EQ(-1,BrokerPollExecution(broker,11,42,job,plan.plan_sha256,&state,&result));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,BrokerCancelExecution(broker,10,43,job,plan.plan_sha256,Deadline()));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,BrokerCancelExecution(broker,10,42,job,std::string(64,'b'),Deadline()));EXPECT_EQ(ESTALE,errno);
    for(unsigned i=0;i<16;++i) {
        ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.plan_sha256,&state,&result));
        EXPECT_EQ(PublicationState::AwaitingValidation,state);
    }
    EXPECT_EQ(before+1,CountFDs());EXPECT_EQ(1,ReferencesTo(original));
    ASSERT_EQ(0,BrokerCancelExecution(broker,10,42,job,plan.plan_sha256,Deadline()));
    EXPECT_EQ(before,CountFDs());EXPECT_EQ(0,ReferencesTo(original));
    ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.plan_sha256,&state,&result));
    EXPECT_EQ(PublicationState::Complete,state);EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);
    EXPECT_EQ(ECANCELED,result.error);
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

// End-to-end native owner path. These use exclusively owned ordinary fixtures;
// no fresh AOSP credentials, production SELinux transition or live CE mutation.
class RuntimePackageTransaction : public RuntimePackagePreparation {
 protected:
    unique_fd store,publish_helper;
    PackagePublication target;
    unsigned next_stage=0;
    void SetUp() override {
        RuntimePackagePreparation::SetUp();ASSERT_FALSE(HasFatalFailure());
        ASSERT_EQ(0,mkdirat(root.get(),"store",0700));
        store.reset(openat(root.get(),"store",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(store.ok());
        publish_helper=Helper("aegis-package-publish");ASSERT_TRUE(publish_helper.ok());
        plan.execution.job=0;target.requester=10;target.serial=42;target.plan_sha256=plan.execution.plan_sha256;
        target.create=true;target.derive_source_hash=true;target.candidate.bytes=plan.image.bytes;
        ASSERT_EQ(0,aegis_broker_owner_create(parent.get(),source.get(),execute_helper.get(),execute_helper.get(),&broker));
    }
    int Register() {
        job=0;return BrokerPrepareTransaction(broker,plan,target,parent.get(),stage.get(),store.get(),source.get(),
            prepare_helper.get(),execute_helper.get(),publish_helper.get(),Fds(),Deadline(),&job);
    }
    void Complete(PackageExecutionResult* result) {
        PublicationState state=PublicationState::Running;bool publishing=false;
        for(unsigned i=0;i<900 && state!=PublicationState::Complete;i++) {
            ASSERT_EQ(0,aegis_broker_owner_reap_publications(broker));
            ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,result));
            publishing|=state==PublicationState::Publishing;
            if(state!=PublicationState::Complete)usleep(10000);
        }
        ASSERT_EQ(PublicationState::Complete,state);RecordProperty("observed_publishing",publishing);
    }
    void Run(PackageExecutionResult* result) {
        ASSERT_EQ(0,Register())<<strerror(errno);ASSERT_GT(job,0u);
        AwaitPrepared();ASSERT_FALSE(HasFatalFailure());
        ASSERT_EQ(0,BrokerStartExecution(broker,10,42,job,plan.execution.plan_sha256,Deadline()))<<strerror(errno);
        Complete(result);
    }
    void FreezePublishing() {
        ASSERT_EQ(0,Register());AwaitPrepared();ASSERT_FALSE(HasFatalFailure());
        ASSERT_EQ(0,BrokerStartExecution(broker,10,42,job,plan.execution.plan_sha256,Deadline()));
        PublicationState state=PublicationState::Running;PackageExecutionResult result;
        for(unsigned i=0;i<900 && state==PublicationState::Running;i++) {
            ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));
            if(state==PublicationState::Running)usleep(1000);
        }
        ASSERT_EQ(PublicationState::Publishing,state)<<result.error;
        // Freeze only this observed job's exclusively owned cgroup. No numeric
        // PID signaling or timing-based inference that publication is in flight.
        ASSERT_EQ(0,WriteAt(parent.get(),"p10-s42/cgroup.freeze","1\n",0));
        bool frozen=false;
        for(unsigned i=0;i<1000;i++) {
            auto events=AptImageFixture::read(parent.get(),"p10-s42/cgroup.events");
            if(events.find("populated 1\n")!=std::string::npos && events.find("frozen 1\n")!=std::string::npos) { frozen=true;break; }
            usleep(1000);
        }
        ASSERT_TRUE(frozen);struct stat st;
        ASSERT_EQ(-1,fstatat(store.get(),"current",&st,AT_SYMLINK_NOFOLLOW));ASSERT_EQ(ENOENT,errno);
    }
    void NewStage() {
        std::string name="transaction-"+std::to_string(++next_stage);
        ASSERT_EQ(0,mkdirat(root.get(),name.c_str(),0700));stages.push_back(name);
        stage.reset(openat(root.get(),name.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(stage.ok());
    }
    unique_fd Selection(const PackageExecutionResult& result,bool personal=false) {
        std::unique_ptr<PackageStore> selected(PackageStore::Open(store.get(),{personal,personal?10u:0u,personal?42u:0u},false));
        if(!selected) { ADD_FAILURE()<<strerror(errno);return unique_fd(); }
        PackageGeneration found;unique_fd image(selected->Current(&found));
        EXPECT_TRUE(image.ok())<<strerror(errno);EXPECT_EQ(found.image_sha256,result.generation.image_sha256);
        EXPECT_EQ(found.bytes,plan.image.bytes);EXPECT_EQ(found.shared_base_sha256,result.generation.shared_base_sha256);
        return image;
    }
    void VerifyContents(const PackageExecutionResult& result,int version,bool removed=false,bool personal=false) {
        unique_fd selected=Selection(result,personal);ASSERT_TRUE(selected.ok());
        NewStage();ASSERT_FALSE(HasFatalFailure());
        auto check=plan;check.execution.job=100+next_stage;check.execution.archives=false;
        check.execution.items={"aegis-exec-app"};check.execution.review={};check.archives.clear();
        check.image={result.generation.bytes,result.generation.image_sha256};
        ASSERT_EQ(0,PackagePreparerStart(parent.get(),stage.get(),selected.get(),prepare_helper.get(),{},check,&worker));
        PackagePreparationResult prepared;int fd=-1;
        ASSERT_EQ(0,PackagePreparerFinish(&worker,false,9000,&prepared,&fd));mount.reset(fd);
        ASSERT_EQ(PackagePreparationOutcome::Prepared,prepared.outcome)<<prepared.error;
        EXPECT_EQ(removed?"<unavailable>":"#!/bin/sh\necho app-"+std::to_string(version)+"\n",
            AptImageFixture::read(mount.get(),"usr/bin/aegis-exec-app"));
        EXPECT_EQ("version="+std::to_string(version)+"\n",AptImageFixture::read(mount.get(),"etc/aegis-exec.conf"));
        if(plan.execution.review.present && !removed) {
            auto automatic=ControlFile(mount.get(),"var/lib/apt/extended_states");
            EXPECT_NE(std::string::npos,automatic.find("Package: aegis-exec-lib\n"));
            EXPECT_EQ(std::string::npos,automatic.find("Package: aegis-exec-app\n"));
        }
        mount.reset();NoLoop();
    }
    void Next(const PackageExecutionResult& previous,int version,bool remove=false) {
        source=Selection(previous,target.personal);ASSERT_TRUE(source.ok());
        plan.image={previous.generation.bytes,previous.generation.image_sha256};
        target.create=false;target.has_previous=true;target.previous=previous.generation;
        NewStage();ASSERT_FALSE(HasFatalFailure());
        archives.clear();plan.archives.clear();plan.execution.items.clear();plan.execution.archives=!remove;
        if(remove) { plan.execution.items={"aegis-exec-app","aegis-exec-lib"};return; }
        for(const char* kind:{"lib","app"}) {
            std::string name="aegis-exec-"+std::string(kind)+"_"+std::to_string(version)+"_all.deb";
            auto contents=Deb(kind,version);ASSERT_EQ(0,WriteAt(root.get(),name.c_str(),contents));archive_names.push_back(name);
            archives.emplace_back(openat(root.get(),name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));ASSERT_TRUE(archives.back().ok());
            plan.archives.push_back({contents.size(),InputHash(contents)});plan.execution.items.push_back(name);
        }
    }
    void TearDown() override {
        if(broker) { EXPECT_EQ(0,aegis_broker_owner_stop_all(broker,Deadline()));EXPECT_EQ(0,aegis_broker_owner_release(&broker)); }
        if(!HasFailure() && store.ok()) {
            unique_fd scan(openat(store.get(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
            DIR* entries=fdopendir(scan.release());ASSERT_NE(nullptr,entries);
            while(auto* e=readdir(entries)) {
                if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
                struct stat st;ASSERT_EQ(0,fstatat(store.get(),e->d_name,&st,AT_SYMLINK_NOFOLLOW));
                ASSERT_TRUE(S_ISREG(st.st_mode));ASSERT_EQ(0u,st.st_uid);EXPECT_EQ(0,unlinkat(store.get(),e->d_name,0));
            }
            closedir(entries);store.reset();EXPECT_EQ(0,unlinkat(root.get(),"store",AT_REMOVEDIR));
        }
        RuntimePackagePreparation::TearDown();
    }
};
// The production START integration remains separate. These exercise the actual
// async selector against real transaction output; no synthetic digest can stand
// in for mounting, old-view retention, readonly behavior or cancellation cleanup.
class RuntimePackageSelection : public RuntimePackageTransaction {
 protected:
    unique_fd factory;
    PackageRuntimeSelection selection;
    void SetUp() override {
        RuntimePackageTransaction::SetUp();ASSERT_FALSE(HasFatalFailure());
        factory.reset(fcntl(source.get(),F_DUPFD_CLOEXEC,3));ASSERT_TRUE(factory.ok());
        selection={10,42,700,plan.image};
    }
    int SelectStart(int shared,int personal) {
        return PackageRuntimeSelectionStart(parent.get(),shared,personal,factory.get(),prepare_helper.get(),selection,&worker);
    }
    void Selected(int shared,int personal,PackagePreparationResult* result) {
        ASSERT_EQ(0,SelectStart(shared,personal))<<strerror(errno);int fd=-1;
        ASSERT_EQ(0,PackagePreparerFinish(&worker,false,9000,result,&fd))<<strerror(errno);mount.reset(fd);
    }
    void Readonly() {
        ASSERT_TRUE(mount.ok());struct statvfs flags;ASSERT_EQ(0,fstatvfs(mount.get(),&flags));
        EXPECT_EQ(static_cast<unsigned long>(ST_RDONLY|ST_NOSUID|ST_NODEV|ST_NOEXEC),
            flags.f_flag&(ST_RDONLY|ST_NOSUID|ST_NODEV|ST_NOEXEC));
        unique_fd attempted(openat(mount.get(),"must-not-exist",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600));
        EXPECT_FALSE(attempted.ok());EXPECT_EQ(EROFS,errno);
    }
    std::string App(int fd) { return AptImageFixture::read(fd,"usr/bin/aegis-exec-app"); }
};
class RuntimeSelectionOwner : public RuntimePackageSelection {
 protected:
    uint64_t selected_job=0;
    bool temporary=false;
    void SetUp() override {
        RuntimePackageSelection::SetUp();ASSERT_FALSE(HasFatalFailure());
        ASSERT_EQ(0,aegis_namespace_private_mounts());selection.job=0;
    }
    int Configure() {
        aegis_base_receipt receipt={};receipt.bytes=selection.factory.bytes;
        memcpy(receipt.sha256,selection.factory.sha256.c_str(),65);
        return aegis_broker_owner_enable_selection(broker,factory.get(),&receipt,prepare_helper.get(),stage.get());
    }
    int RegisterSelection(int shared=-1,int personal=-1) {
        selected_job=0;return BrokerPrepareRuntimeSelection(broker,selection,parent.get(),shared,personal,
            factory.get(),prepare_helper.get(),Deadline(),&selected_job);
    }
    void AwaitSelection(PackagePreparationResult* result) {
        RuntimeSelectionState state=RuntimeSelectionState::Selecting;
        for(unsigned i=0;i<900 && state==RuntimeSelectionState::Selecting;++i) {
            ASSERT_EQ(0,aegis_broker_owner_reap_publications(broker));
            ASSERT_EQ(0,BrokerPollRuntimeSelection(broker,selection.requester,selection.serial,selected_job,&state,result));
            if(state==RuntimeSelectionState::Selecting)usleep(10000);
        }
        ASSERT_EQ(RuntimeSelectionState::Selected,state)<<result->error;
        ASSERT_EQ(PackagePreparationOutcome::Prepared,result->outcome);
    }
    int Apply(uint16_t operation,uint32_t user,uint32_t serial,aegis_broker_state* state) {
        aegis_broker_request request={};request.magic=AEGIS_BROKER_MAGIC;request.version=AEGIS_BROKER_VERSION;
        request.operation=operation;request.sequence=operation==AEGIS_BROKER_HELLO?1:2;
        request.user=user;request.serial=serial;request.deadline_ns=Deadline();
        return aegis_broker_owner_apply(broker,&request,state);
    }
    int StartJob(uint32_t user,uint32_t serial,uint64_t expected,uint64_t* job) {
        aegis_broker_call call={};call.request.magic=AEGIS_BROKER_MAGIC;call.request.version=AEGIS_BROKER_VERSION;
        call.request.operation=expected ? AEGIS_BROKER_CONTINUE_START : AEGIS_BROKER_START;
        call.request.sequence=2;call.request.deadline_ns=Deadline();call.request.user=user;call.request.serial=serial;
        call.command=expected;aegis_broker_state state;
        return aegis_broker_owner_start(broker,&call,job,&state);
    }
    void Freeze() {
        auto name="p"+std::to_string(selection.requester)+"-s"+std::to_string(selection.serial);
        ASSERT_EQ(0,WriteAt(parent.get(),(name+"/cgroup.freeze").c_str(),"1\n",0));
        bool frozen=false;
        for(unsigned i=0;i<1000;++i) {
            auto events=AptImageFixture::read(parent.get(),(name+"/cgroup.events").c_str());
            if(events.find("populated 1\n")!=std::string::npos && events.find("frozen 1\n")!=std::string::npos) { frozen=true;break; }
            usleep(1000);
        }
        ASSERT_TRUE(frozen);
    }
    void TearDown() override {
        if(temporary)EXPECT_EQ(0,aegis_namespace_temporary_base_end(mount.get()));
        RuntimePackageSelection::TearDown();
    }
};
TEST_F(RuntimeSelectionOwner, ReadySelectionIsOwnedUntilStopAndPollNeverExportsOrConsumesItsMount) {
    int fds=CountFDs();ASSERT_EQ(0,RegisterSelection());PackagePreparationResult result;AwaitSelection(&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(PackagePreparationResult::Scope::Factory,result.scope);EXPECT_EQ(selection.factory.sha256,result.generation.image_sha256);
    int ready_fds=CountFDs();RuntimeSelectionState state;
    for(int i=0;i<16;++i)ASSERT_EQ(0,BrokerPollRuntimeSelection(broker,10,42,selected_job,&state,&result));
    EXPECT_EQ(ready_fds,CountFDs());EXPECT_EQ(-1,aegis_broker_owner_release(&broker));EXPECT_EQ(EBUSY,errno);
    aegis_broker_state current;ASSERT_EQ(0,Apply(AEGIS_BROKER_STATUS,10,42,&current));EXPECT_EQ(AEGIS_BROKER_SEALED,current);
    ASSERT_EQ(0,Stop());EXPECT_EQ(fds,CountFDs());EXPECT_EQ(-1,BrokerPollRuntimeSelection(broker,10,42,selected_job,&state,&result));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimeSelectionOwner, IdentitySerialAndMonotoneJobBindSelectionWithoutReplacingIt) {
    ASSERT_EQ(0,RegisterSelection());auto first=selected_job;PackagePreparationResult result;AwaitSelection(&result);ASSERT_FALSE(HasFatalFailure());
    RuntimeSelectionState state;EXPECT_EQ(-1,BrokerPollRuntimeSelection(broker,11,42,first,&state,&result));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,BrokerPollRuntimeSelection(broker,10,43,first,&state,&result));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,RegisterSelection());EXPECT_EQ(EALREADY,errno);EXPECT_EQ(0u,selected_job);
    ASSERT_EQ(0,Stop());ASSERT_EQ(0,RegisterSelection());EXPECT_GT(selected_job,first);ASSERT_EQ(0,Stop());
}
TEST_F(RuntimeSelectionOwner, StopKillsObservedFrozenSelectorAndCannotTargetAnotherUserByAccident) {
    int fds=CountFDs();ASSERT_EQ(0,RegisterSelection());Freeze();ASSERT_FALSE(HasFatalFailure());
    aegis_broker_state current;ASSERT_EQ(0,Apply(AEGIS_BROKER_STOP_USER,11,0,&current));
    RuntimeSelectionState state;PackagePreparationResult result;
    ASSERT_EQ(0,BrokerPollRuntimeSelection(broker,10,42,selected_job,&state,&result));EXPECT_EQ(RuntimeSelectionState::Selecting,state);
    ASSERT_EQ(0,Stop());EXPECT_EQ(fds,CountFDs());
    EXPECT_EQ("populated 0\nfrozen 0\n",AptImageFixture::read(parent.get(),"cgroup.events"));
}
TEST_F(RuntimeSelectionOwner, ExpiredDisconnectSealsEverySelectorThenRequiresConfirmedReap) {
    int fds=CountFDs();ASSERT_EQ(0,RegisterSelection());Freeze();ASSERT_FALSE(HasFatalFailure());
    selection.requester=11;selection.serial=99;ASSERT_EQ(0,RegisterSelection());Freeze();ASSERT_FALSE(HasFatalFailure());
    // First call may finish immediately or retain ETIMEDOUT ownership. Neither
    // path may leave the other frozen child unsignalled.
    int stopped=aegis_broker_owner_stop_all(broker,0);if(stopped<0)EXPECT_EQ(ETIMEDOUT,errno);
    ASSERT_EQ(0,aegis_broker_owner_stop_all(broker,Deadline()));EXPECT_EQ(fds,CountFDs());
    EXPECT_EQ("populated 0\nfrozen 0\n",AptImageFixture::read(parent.get(),"cgroup.events"));
}
TEST_F(RuntimeSelectionOwner, HelloClosesReadyMountsForBothUsersBeforeAcknowledging) {
    int fds=CountFDs();PackagePreparationResult result;ASSERT_EQ(0,RegisterSelection());AwaitSelection(&result);ASSERT_FALSE(HasFatalFailure());
    selection.requester=11;selection.serial=99;ASSERT_EQ(0,RegisterSelection());AwaitSelection(&result);ASSERT_FALSE(HasFatalFailure());
    aegis_broker_state state;ASSERT_EQ(0,Apply(AEGIS_BROKER_HELLO,0,0,&state));EXPECT_EQ(AEGIS_BROKER_ABSENT,state);EXPECT_EQ(fds,CountFDs());
}
TEST_F(RuntimeSelectionOwner, MissingCeConsumesARegisteredJobAndStartCannotFallBackToFactory) {
    // This absent numeric identity is checked read-only before the CE opener.
    struct stat st;ASSERT_EQ(-1,lstat("/data/system_ce/21472",&st));ASSERT_EQ(ENOENT,errno);
    selection.requester=21472;selection.serial=1234;
    int fds=CountFDs();
    EXPECT_EQ(-1,BrokerPrepareCeRuntimeSelection(broker,selection,parent.get(),-1,factory.get(),prepare_helper.get(),Deadline(),&selected_job));
    EXPECT_EQ(ENOENT,errno);EXPECT_GT(selected_job,0u);
    RuntimeSelectionState state;PackagePreparationResult result;
    ASSERT_EQ(0,BrokerPollRuntimeSelection(broker,21472,1234,selected_job,&state,&result));
    EXPECT_EQ(RuntimeSelectionState::Failed,state);EXPECT_EQ(ENOENT,result.error);EXPECT_EQ(fds,CountFDs());
    aegis_broker_state current;EXPECT_EQ(-1,Apply(AEGIS_BROKER_START,21472,1234,&current));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(AEGIS_BROKER_SEALED,current);ASSERT_EQ(0,Apply(AEGIS_BROKER_STOP_USER,21472,0,&current));EXPECT_EQ(fds,CountFDs());
}
TEST_F(RuntimeSelectionOwner, FailedSelectorNeverUsesLegacyBaseAndStopAllowsNewPreparation) {
    selection.factory.sha256=std::string(64,'f');ASSERT_EQ(0,RegisterSelection());
    RuntimeSelectionState state=RuntimeSelectionState::Selecting;PackagePreparationResult result;
    for(int i=0;i<900 && state==RuntimeSelectionState::Selecting;++i) {
        ASSERT_EQ(0,BrokerPollRuntimeSelection(broker,10,42,selected_job,&state,&result));if(state==RuntimeSelectionState::Selecting)usleep(10000);
    }
    ASSERT_EQ(RuntimeSelectionState::Failed,state);ASSERT_EQ(ESTALE,result.error);
    aegis_broker_state current;EXPECT_EQ(-1,Apply(AEGIS_BROKER_START,10,42,&current));EXPECT_EQ(ESTALE,errno);
    ASSERT_EQ(0,Stop());selection.factory=plan.image;ASSERT_EQ(0,RegisterSelection());AwaitSelection(&result);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(0,Stop());
}
TEST_F(RuntimeSelectionOwner, PartialContextStartClonesSelectedMountAndDetachesAnchorBeforeCeFailure) {
    struct stat absent;ASSERT_EQ(-1,lstat("/data/system_ce/21472",&absent));ASSERT_EQ(ENOENT,errno);
    selection.requester=21472;selection.serial=1234;
    int fds=CountFDs();unique_fd anchor(open("/mnt",O_PATH|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));ASSERT_TRUE(anchor.ok());
    struct stat before,after;ASSERT_EQ(0,fstat(anchor.get(),&before));
    ASSERT_EQ(0,RegisterSelection());PackagePreparationResult result;AwaitSelection(&result);ASSERT_FALSE(HasFatalFailure());
    aegis_broker_state current;
    EXPECT_EQ(-1,Apply(AEGIS_BROKER_START,21472,1234,&current));EXPECT_EQ(ENOENT,errno);
    // ENOENT comes only after selected-image attachment, ID-map cloning and
    // anchor detachment. No fallback to the intentionally non-mount legacy FD.
    EXPECT_EQ(AEGIS_BROKER_SEALED,current);ASSERT_EQ(0,stat("/mnt",&after));EXPECT_EQ(before.st_dev,after.st_dev);EXPECT_EQ(before.st_ino,after.st_ino);
    EXPECT_EQ(-1,aegis_broker_owner_release(&broker));EXPECT_EQ(EBUSY,errno);
    ASSERT_EQ(0,Apply(AEGIS_BROKER_STOP_USER,21472,0,&current));anchor.reset();EXPECT_EQ(fds,CountFDs());
    EXPECT_EQ("populated 0\nfrozen 0\n",AptImageFixture::read(parent.get(),"cgroup.events"));
}
TEST_F(RuntimeSelectionOwner, TemporaryAnchorCannotBeReplacedOrDetachedThroughAnotherMount) {
    selection.job=777;PackagePreparationResult result;Selected(-1,-1,&result);ASSERT_FALSE(HasFatalFailure());ASSERT_TRUE(mount.ok());
    unique_fd original(open("/mnt",O_PATH|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(original.ok());
    struct stat before,after;ASSERT_EQ(0,fstat(original.get(),&before));
    ASSERT_EQ(0,aegis_namespace_temporary_base_begin(mount.get()));temporary=true;
    EXPECT_EQ(-1,aegis_namespace_temporary_base_begin(mount.get()));EXPECT_EQ(EBUSY,errno);
    EXPECT_EQ(-1,aegis_namespace_attach_base(mount.get()));EXPECT_EQ(EBUSY,errno);
    EXPECT_EQ(-1,aegis_namespace_temporary_base_end(original.get()));EXPECT_EQ(ESTALE,errno);
    ASSERT_EQ(0,aegis_namespace_temporary_base_end(mount.get()));temporary=false;
    ASSERT_EQ(0,stat("/mnt",&after));EXPECT_EQ(before.st_dev,after.st_dev);EXPECT_EQ(before.st_ino,after.st_ino);
    Readonly();
}

TEST_F(RuntimePackageSelection, AbsentStoresSelectVerifiedFactoryWithoutCreatingAStage) {
    int fds=CountFDs();PackagePreparationResult result;Selected(-1,-1,&result);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(PackagePreparationOutcome::Prepared,result.outcome)<<result.error;
    EXPECT_EQ(PackagePreparationResult::Scope::Factory,result.scope);EXPECT_EQ(selection.factory.sha256,result.generation.image_sha256);
    Readonly();EXPECT_EQ("<unavailable>",App(mount.get()));
    struct stat st;EXPECT_EQ(-1,fstatat(stage.get(),"candidate.ext4",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    mount.reset();EXPECT_EQ(fds,CountFDs());
}
TEST_F(RuntimePackageSelection, InitializedEmptyStoreFallsBackButIncompleteStoreDoesNot) {
    PackagePreparationResult result;Selected(store.get(),-1,&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(ENOENT,result.error);EXPECT_FALSE(mount.ok());
    std::unique_ptr<PackageStore> initialized(PackageStore::Open(store.get(),{false,0,0},true));ASSERT_TRUE(initialized);initialized.reset();
    Selected(store.get(),-1,&result);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(PackagePreparationOutcome::Prepared,result.outcome)<<result.error;Readonly();
    EXPECT_EQ(PackagePreparationResult::Scope::Factory,result.scope);EXPECT_EQ(selection.factory.sha256,result.generation.image_sha256);
}
TEST_F(RuntimePackageSelection, MountedOldVersionSurvivesRealUpdateWhileNewSelectionSeesNewVersion) {
    PackageExecutionResult installed;Run(&installed);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(PackageExecutionOutcome::Published,installed.outcome);
    PackagePreparationResult result;Selected(store.get(),-1,&result);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(PackagePreparationOutcome::Prepared,result.outcome)<<result.error;Readonly();
    EXPECT_EQ(PackagePreparationResult::Scope::Shared,result.scope);EXPECT_EQ(installed.generation.image_sha256,result.generation.image_sha256);
    EXPECT_EQ("#!/bin/sh\necho app-1\n",App(mount.get()));unique_fd old=std::move(mount);
    Next(installed,2);ASSERT_FALSE(HasFatalFailure());PackageExecutionResult updated;Run(&updated);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(PackageExecutionOutcome::Published,updated.outcome)<<updated.error;
    Selected(store.get(),-1,&result);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(PackagePreparationOutcome::Prepared,result.outcome)<<result.error;
    Readonly();EXPECT_EQ(updated.generation.image_sha256,result.generation.image_sha256);
    EXPECT_EQ("#!/bin/sh\necho app-1\n",App(old.get()));EXPECT_EQ("#!/bin/sh\necho app-2\n",App(mount.get()));
    // Full store rehash after both mounted views proves no readonly journal replay.
    unique_fd checked=Selection(updated);EXPECT_TRUE(checked.ok());
}
TEST_F(RuntimePackageSelection, PrivateInstallUpdateRemoveStayBoundToRequesterAndPinnedBase) {
    target.personal=true;target.candidate.shared_base_sha256=selection.factory.sha256;
    PackageExecutionResult prior;
    for(int version=1;version<=3;++version) {
        if(version>1) { Next(prior,version,version==3);ASSERT_FALSE(HasFatalFailure()); }
        Run(&prior);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(PackageExecutionOutcome::Published,prior.outcome)<<prior.error;
        PackagePreparationResult result;Selected(-1,store.get(),&result);ASSERT_FALSE(HasFatalFailure());
        ASSERT_EQ(PackagePreparationOutcome::Prepared,result.outcome)<<result.error;Readonly();
        EXPECT_EQ(PackagePreparationResult::Scope::Personal,result.scope);
        EXPECT_EQ(selection.factory.sha256,result.generation.shared_base_sha256);
        EXPECT_EQ(prior.generation.image_sha256,result.generation.image_sha256);
        EXPECT_EQ(version==3?"<unavailable>":"#!/bin/sh\necho app-"+std::to_string(version)+"\n",App(mount.get()));
        mount.reset();
    }
    for(bool wrong_user:{true,false}) {
        selection.requester=wrong_user?11:10;selection.serial=wrong_user?42:43;
        PackagePreparationResult result;Selected(-1,store.get(),&result);ASSERT_FALSE(HasFatalFailure());
        EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(ESTALE,result.error);EXPECT_FALSE(mount.ok());
    }
}
TEST_F(RuntimePackageSelection, PrivateVersionOverridesSharedOnlyForItsOwnerAndExactBase) {
    PackageExecutionResult common;Run(&common);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(PackageExecutionOutcome::Published,common.outcome);
    Next(common,2);ASSERT_FALSE(HasFatalFailure());
    unique_fd shared=std::move(store);
    ASSERT_EQ(0,mkdirat(root.get(),"private-store",0700));
    store.reset(openat(root.get(),"private-store",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));ASSERT_TRUE(store.ok());
    target.personal=true;target.create=true;target.has_previous=false;target.previous={};
    target.candidate.shared_base_sha256=common.generation.image_sha256;
    PackageExecutionResult own;Run(&own);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(PackageExecutionOutcome::Published,own.outcome)<<own.error;
    PackagePreparationResult selected;Selected(shared.get(),store.get(),&selected);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(PackagePreparationOutcome::Prepared,selected.outcome)<<selected.error;
    EXPECT_EQ(PackagePreparationResult::Scope::Personal,selected.scope);EXPECT_EQ("#!/bin/sh\necho app-2\n",App(mount.get()));
    EXPECT_EQ(common.generation.image_sha256,selected.generation.shared_base_sha256);Readonly();mount.reset();
    Selected(shared.get(),-1,&selected);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(PackagePreparationOutcome::Prepared,selected.outcome)<<selected.error;
    EXPECT_EQ(PackagePreparationResult::Scope::Shared,selected.scope);EXPECT_EQ("#!/bin/sh\necho app-1\n",App(mount.get()));mount.reset();
    // Dispose only this successful fixture's private store. On any failure,
    // retain both sets of immutable files for diagnosis.
    if(HasFailure())return;
    unique_fd scan(openat(store.get(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));DIR* entries=fdopendir(scan.release());ASSERT_NE(nullptr,entries);
    while(auto* e=readdir(entries)) {
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        struct stat st;ASSERT_EQ(0,fstatat(store.get(),e->d_name,&st,AT_SYMLINK_NOFOLLOW));ASSERT_TRUE(S_ISREG(st.st_mode));ASSERT_EQ(0u,st.st_uid);
        EXPECT_EQ(0,unlinkat(store.get(),e->d_name,0));
    }
    closedir(entries);store.reset();EXPECT_EQ(0,unlinkat(root.get(),"private-store",AT_REMOVEDIR));store=std::move(shared);
}
TEST_F(RuntimePackageSelection, StalePrivateBaseNeverFallsBackToFactory) {
    target.personal=true;target.candidate.shared_base_sha256=selection.factory.sha256;
    PackageExecutionResult installed;Run(&installed);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(PackageExecutionOutcome::Published,installed.outcome);
    // A different trusted factory receipt represents a changed shared base.
    // Personal store must be rejected before a fallback can hide its packages.
    selection.factory.sha256=std::string(64,'f');PackagePreparationResult result;
    Selected(-1,store.get(),&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(ESTALE,result.error);EXPECT_FALSE(mount.ok());
}
TEST_F(RuntimePackageSelection, MissingSelectedImageIsCorruptionNotAnEmptyStore) {
    PackageExecutionResult installed;Run(&installed);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(PackageExecutionOutcome::Published,installed.outcome);
    auto name=installed.generation.image_sha256+".image";
    // Rename only this disposable test's generation; retain the inode for cleanup.
    ASSERT_EQ(0,renameat(store.get(),name.c_str(),store.get(),"saved.image"));
    PackagePreparationResult result;Selected(store.get(),-1,&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(ESTALE,result.error);EXPECT_FALSE(mount.ok());
    ASSERT_EQ(0,renameat(store.get(),"saved.image",store.get(),name.c_str()));
    unique_fd unchanged=Selection(installed);EXPECT_TRUE(unchanged.ok());
}
TEST_F(RuntimePackageSelection, BusyStoreFailsWithoutFallbackOrWaitingForPublication) {
    std::unique_ptr<PackageStore> held(PackageStore::Open(store.get(),{false,0,0},true));ASSERT_TRUE(held);
    PackagePreparationResult result;Selected(store.get(),-1,&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(EWOULDBLOCK,result.error);EXPECT_FALSE(mount.ok());
}
TEST_F(RuntimePackageSelection, CancelConsumesQueuedMountAndReleasesAllOwnedReferences) {
    int fds=CountFDs();ASSERT_EQ(0,SelectStart(-1,-1));
    bool exited=false;
    for(unsigned i=0;i<900;++i) {
        if(AptImageFixture::read(parent.get(),"p10-s42/cgroup.events").find("populated 0\n")!=std::string::npos) { exited=true;break; }
        usleep(10000);
    }
    ASSERT_TRUE(exited);PackagePreparationResult result;int fd=-1;
    ASSERT_EQ(0,PackagePreparerFinish(&worker,true,9000,&result,&fd));
    EXPECT_EQ(-1,fd);EXPECT_EQ(nullptr,worker);EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(ECANCELED,result.error);
    EXPECT_EQ(PackagePreparationResult::Scope::None,result.scope);EXPECT_TRUE(result.generation.image_sha256.empty());
    EXPECT_EQ(fds,CountFDs());EXPECT_EQ("populated 0\nfrozen 0\n",AptImageFixture::read(parent.get(),"cgroup.events"));
}
TEST_F(RuntimePackageSelection, InvalidIdentityReceiptOrDescriptorsNeverSpawn) {
    int fds=CountFDs();auto valid=selection;
    selection.requester=0;EXPECT_EQ(-1,SelectStart(-1,-1));EXPECT_EQ(EINVAL,errno);EXPECT_EQ(nullptr,worker);
    selection=valid;selection.serial=UINT32_MAX;EXPECT_EQ(-1,SelectStart(-1,-1));EXPECT_EQ(EINVAL,errno);
    selection=valid;selection.job=0;EXPECT_EQ(-1,SelectStart(-1,-1));EXPECT_EQ(EINVAL,errno);
    selection=valid;selection.factory.bytes++;EXPECT_EQ(-1,SelectStart(-1,-1));EXPECT_EQ(EINVAL,errno);
    selection=valid;selection.factory.sha256[0]='z';EXPECT_EQ(-1,SelectStart(-1,-1));EXPECT_EQ(EINVAL,errno);
    selection=valid;EXPECT_EQ(-1,SelectStart(-2,-1));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(-1,SelectStart(factory.get(),-1));EXPECT_EQ(EPERM,errno);EXPECT_EQ(nullptr,worker);EXPECT_EQ(fds,CountFDs());
}

TEST_F(RuntimePackageTransaction, OneOwnedJobPublishesRealInstallUpgradeAndRemoveWithOldImagesRetained) {
    int descriptors=CountFDs();auto original=plan.image;
    PackageExecutionResult installed;Run(&installed);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(PackageExecutionOutcome::Published,installed.outcome)<<installed.error;
    EXPECT_EQ(0,installed.error);EXPECT_NE(original.sha256,installed.generation.image_sha256);
    uint64_t first=job;VerifyContents(installed,1);ASSERT_FALSE(HasFailure());
    unique_fd old=Selection(installed);ASSERT_TRUE(old.ok());
    Next(installed,2);ASSERT_FALSE(HasFatalFailure());PackageExecutionResult updated;Run(&updated);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(PackageExecutionOutcome::Published,updated.outcome)<<updated.error;EXPECT_GT(job,first);
    EXPECT_NE(installed.generation.image_sha256,updated.generation.image_sha256);
    VerifyContents(updated,2);ASSERT_FALSE(HasFailure());
    unique_fd preserved(openat(store.get(),(installed.generation.image_sha256+".image").c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    ASSERT_TRUE(preserved.ok());struct stat before,after;ASSERT_EQ(0,fstat(old.get(),&before));ASSERT_EQ(0,fstat(preserved.get(),&after));
    EXPECT_EQ(before.st_ino,after.st_ino);EXPECT_EQ(before.st_dev,after.st_dev);
    Next(updated,3,true);ASSERT_FALSE(HasFatalFailure());PackageExecutionResult removed;Run(&removed);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(PackageExecutionOutcome::Published,removed.outcome)<<removed.error;
    VerifyContents(removed,2,true);ASSERT_FALSE(HasFailure());
    // Only caller-owned image descriptors remain; owner owns no private worker.
    old.reset();preserved.reset();EXPECT_EQ(descriptors-2,CountFDs()); // removal released two archive FDs
}
TEST_F(RuntimePackageTransaction, PersonalGenerationUsesPinnedRequesterAndBaseDespiteCallerMutation) {
    target.personal=true;target.candidate.shared_base_sha256=plan.image.sha256;
    const auto base=plan.image.sha256;
    ASSERT_EQ(0,Register());AwaitPrepared();ASSERT_FALSE(HasFatalFailure());
    target.requester=11;target.serial=99;target.personal=false;target.candidate.shared_base_sha256.clear();
    ASSERT_EQ(0,BrokerStartExecution(broker,10,42,job,plan.execution.plan_sha256,Deadline()));
    PackageExecutionResult result;Complete(&result);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(PackageExecutionOutcome::Published,result.outcome)<<result.error;
    EXPECT_EQ(base,result.generation.shared_base_sha256);VerifyContents(result,1,false,true);ASSERT_FALSE(HasFailure());
    EXPECT_EQ(nullptr,PackageStore::Open(store.get(),{true,11,99},false));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(nullptr,PackageStore::Open(store.get(),{false,0,0},false));EXPECT_EQ(ESTALE,errno);
}
TEST_F(RuntimePackageTransaction, TargetIdentityDigestSizeAndPreviousAreBoundBeforeAnyPreparation) {
    const auto valid=target;int before=CountFDs();
    for(int which=0;which<6;which++) {
        target=valid;
        switch(which) {
          case 0:target.requester++;break;case 1:target.serial++;break;
          case 2:target.plan_sha256=std::string(64,'a');break;
          case 3:target.candidate.bytes+=4096;break;case 4:target.candidate.image_sha256=std::string(64,'f');break;
          case 5:target.create=false;target.has_previous=true;target.previous={std::string(64,'b'),"",plan.image.bytes};break;
        }
        EXPECT_EQ(-1,Register());EXPECT_EQ(EINVAL,errno);EXPECT_EQ(0u,job);EXPECT_EQ(before,CountFDs());
    }
    struct stat st;EXPECT_EQ(-1,fstatat(stage.get(),"candidate.ext4",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,fstatat(store.get(),"current",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackageTransaction, StopBeforeApprovalClosesStageAndTargetWithoutSelectingAnything) {
    ASSERT_EQ(0,Register());AwaitPrepared();ASSERT_FALSE(HasFatalFailure());
    struct stat s,t;ASSERT_EQ(0,fstat(stage.get(),&s));ASSERT_EQ(0,fstat(store.get(),&t));
    EXPECT_GT(ReferencesTo(s),1);EXPECT_GT(ReferencesTo(t),1);
    ASSERT_EQ(0,Stop());EXPECT_EQ(1,ReferencesTo(s));EXPECT_EQ(1,ReferencesTo(t));NoLoop();
    EXPECT_EQ(-1,fstatat(store.get(),"current",&s,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,BrokerStartExecution(broker,10,42,job,plan.execution.plan_sha256,Deadline()));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackageTransaction, FailedIdentityPolicyNeverCreatesOrSelectsTheTargetStore) {
    auto contents=Deb("app",1,false,"sed -i 's/^runtime:[^:]*:/runtime:active:/' /etc/shadow\n");
    ASSERT_EQ(0,WriteAt(root.get(),archive_names[1].c_str(),contents,O_TRUNC));
    plan.archives[1]={contents.size(),InputHash(contents)};
    PackageExecutionResult result;Run(&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EPERM,result.error);EXPECT_TRUE(result.generation.image_sha256.empty());
    struct stat st;EXPECT_EQ(-1,fstatat(store.get(),"current",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,fstatat(store.get(),"owner",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackageTransaction, ActualStoreConflictRejectsAfterAptWithoutReplacingSelectedGeneration) {
    // Pre-existing selection, while this independent approved plan expects first
    // selection. APT still executes in its own copy; the rename point must fail.
    PackageGeneration prior{plan.image.sha256,"",plan.image.bytes};
    {
        std::unique_ptr<PackageStore> existing(PackageStore::Open(store.get(),{false,0,0},true));ASSERT_NE(nullptr,existing);
        std::atomic_bool cancel{false};ASSERT_EQ(PackagePublish::Confirmed,existing->Publish(nullptr,source.get(),prior,cancel));
    }
    target.create=false;PackageExecutionResult result;Run(&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(ESTALE,result.error);
    std::unique_ptr<PackageStore> existing(PackageStore::Open(store.get(),{false,0,0},false));ASSERT_NE(nullptr,existing);
    PackageGeneration selected;unique_fd current(existing->Current(&selected));ASSERT_TRUE(current.ok());EXPECT_EQ(prior.image_sha256,selected.image_sha256);
}
TEST_F(RuntimePackageTransaction, StopUserReapsObservedPublishingAndClosesSourceAndStore) {
    FreezePublishing();ASSERT_FALSE(HasFatalFailure());
    struct stat original;ASSERT_EQ(0,fstat(store.get(),&original));EXPECT_GT(ReferencesTo(original),1);
    ASSERT_EQ(0,Stop());EXPECT_EQ(1,ReferencesTo(original));
    struct stat st;EXPECT_EQ(-1,fstatat(parent.get(),"p10-s42",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,fstatat(store.get(),"current",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    PublicationState state;PackageExecutionResult result;
    EXPECT_EQ(-1,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackageTransaction, CancelPublishingReturnsUnconfirmedAfterActualReapNeverFalseRollback) {
    FreezePublishing();ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,BrokerCancelExecution(broker,11,42,job,plan.execution.plan_sha256,Deadline()));EXPECT_EQ(ESTALE,errno);
    EXPECT_NE(std::string::npos,AptImageFixture::read(parent.get(),"p10-s42/cgroup.events").find("populated 1\n"));
    ASSERT_EQ(0,BrokerCancelExecution(broker,10,42,job,plan.execution.plan_sha256,Deadline()));
    PublicationState state;PackageExecutionResult result;
    ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));
    EXPECT_EQ(PublicationState::Complete,state);EXPECT_EQ(PackageExecutionOutcome::Unconfirmed,result.outcome);
    EXPECT_TRUE(result.generation.image_sha256.empty());
    struct stat st;EXPECT_EQ(-1,fstatat(parent.get(),"p10-s42",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,fstatat(store.get(),"current",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackageTransaction, MissingPrivateCeConsumesOneBoundJobWithoutStoreOrWorkerLeak) {
    plan.execution.requester=21472;plan.execution.serial=INT32_MAX;target.requester=21472;target.serial=INT32_MAX;
    target.personal=true;target.candidate.shared_base_sha256=plan.image.sha256;
    struct stat st;ASSERT_EQ(-1,lstat("/data/system_ce/21472",&st));ASSERT_EQ(ENOENT,errno);
    int before=CountFDs();
    ASSERT_EQ(-1,BrokerPreparePersonalTransaction(broker,plan,target,parent.get(),source.get(),prepare_helper.get(),
        execute_helper.get(),publish_helper.get(),Fds(),Deadline(),&job));ASSERT_EQ(ENOENT,errno);ASSERT_GT(job,0u);EXPECT_EQ(before,CountFDs());
    PublicationState state;PackageExecutionResult result;
    ASSERT_EQ(0,BrokerPollExecution(broker,21472,INT32_MAX,job,plan.execution.plan_sha256,&state,&result));
    EXPECT_EQ(PublicationState::Complete,state);EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(ENOENT,result.error);
    EXPECT_EQ(-1,lstat("/data/misc_ce/21472",&st));EXPECT_EQ(ENOENT,errno);
}

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
TEST_F(RuntimePackagePreparation, MissingAospCeConsumesPrivateJobWithoutFdsOrChild) {
    // Read-only prerequisite: never synthesize or touch a live AOSP user root.
    struct stat st;ASSERT_EQ(-1,lstat("/data/system_ce/21472",&st));ASSERT_EQ(ENOENT,errno);
    ASSERT_EQ(0,aegis_broker_owner_create(parent.get(),source.get(),execute_helper.get(),execute_helper.get(),&broker));
    plan.execution.requester=21472;plan.execution.serial=INT32_MAX;plan.execution.job=0;
    int before=CountFDs();
    ASSERT_EQ(-1,BrokerPreparePersonalCandidate(broker,plan,parent.get(),source.get(),prepare_helper.get(),
                                               execute_helper.get(),Fds(),Deadline(),&job));
    ASSERT_EQ(ENOENT,errno);ASSERT_GT(job,0u);EXPECT_EQ(before,CountFDs());
    PublicationState state;PackageExecutionResult result;
    EXPECT_EQ(-1,BrokerPollExecution(broker,21472,42,job,plan.execution.plan_sha256,&state,&result));
    EXPECT_EQ(ESTALE,errno);
    ASSERT_EQ(0,BrokerPollExecution(broker,21472,INT32_MAX,job,plan.execution.plan_sha256,&state,&result));
    EXPECT_EQ(PublicationState::Complete,state);EXPECT_EQ(PackageExecutionOutcome::Failed,result.outcome);
    EXPECT_EQ(ENOENT,result.error);EXPECT_EQ(before,CountFDs());
    EXPECT_EQ("populated 0\nfrozen 0\n",AptImageFixture::read(parent.get(),"cgroup.events"));
    EXPECT_EQ(-1,lstat("/data/misc_ce/21472",&st));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackagePreparation, PrivatePublicationRejectsSharedScopeAndMissingAospCe) {
    struct stat st;ASSERT_EQ(-1,lstat("/data/system_ce/21472",&st));ASSERT_EQ(ENOENT,errno);
    ASSERT_EQ(0,aegis_broker_owner_create(parent.get(),source.get(),execute_helper.get(),execute_helper.get(),&broker));
    PackagePublication p;p.requester=21472;p.serial=INT32_MAX;p.plan_sha256=plan.execution.plan_sha256;
    p.create=true;p.candidate={plan.image.sha256,"",plan.image.bytes};
    int before=CountFDs();
    ASSERT_EQ(-1,BrokerPreparePersonalPublication(broker,p,parent.get(),source.get(),execute_helper.get(),Deadline(),&job));
    EXPECT_EQ(EINVAL,errno);EXPECT_EQ(0u,job);EXPECT_EQ(before,CountFDs());
    p.personal=true;p.candidate.shared_base_sha256=plan.image.sha256;
    ASSERT_EQ(-1,BrokerPreparePersonalPublication(broker,p,parent.get(),source.get(),execute_helper.get(),Deadline(),&job));
    ASSERT_EQ(ENOENT,errno);ASSERT_GT(job,0u);EXPECT_EQ(before,CountFDs());
    PublicationState state;PackagePublicationResult result;
    ASSERT_EQ(0,BrokerPollPublication(broker,p.requester,p.serial,job,p.plan_sha256,&state,&result));
    EXPECT_EQ(PublicationState::Complete,state);EXPECT_EQ(PackagePublish::Rejected,result.publication);
    EXPECT_EQ(ENOENT,result.error);EXPECT_EQ(before,CountFDs());
    EXPECT_EQ(-1,lstat("/data/misc_ce/21472",&st));EXPECT_EQ(ENOENT,errno);
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
TEST_F(RuntimePackagePreparation, RejectsStoredForeignRootLabelInsteadOfOverridingIt) {
    Ready();ASSERT_FALSE(HasFatalFailure());
    unique_fd candidate(openat(mount.get(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(candidate.ok());
    ASSERT_EQ(0,aegis_package_candidate_labels(candidate.get(),0));
    const char foreign[]="u:object_r:shell_data_file:s0";
    ASSERT_EQ(0,fsetxattr(candidate.get(),"security.selinux",foreign,sizeof(foreign),0));
    candidate.reset();ReusePreparedImage();ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,Start());PackagePreparationResult result;int fd=-1;
    ASSERT_EQ(0,PackagePreparerFinish(&worker,false,9000,&result,&fd));
    EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(EPERM,result.error);EXPECT_EQ(-1,fd);NoLoop();
}
TEST_F(RuntimePackagePreparation, RejectsStoredForeignSymlinkLabelBeforeArchiveOrWorkerHandoff) {
    Ready();ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,symlinkat("/system/bin/sh",mount.get(),"aegis-foreign-link"));
    const char foreign[]="u:object_r:shell_data_file:s0";
    std::string path="/proc/self/fd/"+std::to_string(mount.get())+"/aegis-foreign-link";
    ASSERT_EQ(0,lsetxattr(path.c_str(),"security.selinux",foreign,sizeof(foreign),0));
    ReusePreparedImage();ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,Start());PackagePreparationResult result;int fd=-1;
    ASSERT_EQ(0,PackagePreparerFinish(&worker,false,9000,&result,&fd));
    EXPECT_EQ(PackagePreparationOutcome::Failed,result.outcome);EXPECT_EQ(EPERM,result.error);EXPECT_EQ(-1,fd);NoLoop();
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
TEST_F(RuntimePackagePreparation, BrokerRetainsSameJobThroughPreparationAptAndValidationWait) {
    Broker();ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,BrokerStartExecution(broker,10,43,job,plan.execution.plan_sha256,Deadline()));EXPECT_EQ(ESTALE,errno);
    AwaitPrepared();ASSERT_FALSE(HasFatalFailure());EXPECT_EQ(1,MatchingLoops());
    struct stat original;ASSERT_EQ(0,fstat(stage.get(),&original));
    ASSERT_EQ(0,BrokerStartExecution(broker,10,42,job,plan.execution.plan_sha256,Deadline()))<<strerror(errno);
    PublicationState state=PublicationState::Running;PackageExecutionResult result;
    for(unsigned i=0;i<900 && state==PublicationState::Running;++i) {
        ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));
        if(state==PublicationState::Running)usleep(10000);
    }
    ASSERT_EQ(PublicationState::AwaitingValidation,state);
    EXPECT_EQ(PackageExecutionOutcome::NeedsValidation,result.outcome);
    EXPECT_EQ(0,result.error);NoLoop();EXPECT_EQ(2,ReferencesTo(original));
    aegis_broker_request request={};request.magic=AEGIS_BROKER_MAGIC;request.version=AEGIS_BROKER_VERSION;
    request.operation=AEGIS_BROKER_STATUS;request.user=10;request.serial=42;request.sequence=2;request.deadline_ns=Deadline();
    aegis_broker_state current=AEGIS_BROKER_ABSENT;
    ASSERT_EQ(0,aegis_broker_owner_apply(broker,&request,&current));
    EXPECT_EQ(AEGIS_BROKER_SEALED,current); // no runtime context, but CE is still owned
    ASSERT_EQ(0,Stop());EXPECT_EQ(1,ReferencesTo(original));NoLoop();
    EXPECT_EQ(-1,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));EXPECT_EQ(ENOENT,errno);
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
    Broker();ASSERT_FALSE(HasFatalFailure());unique_fd own(openat(parent.get(),"p10-s42",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(own.ok());
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

// Opt-in only after the host driver authenticates a newly created AOSP user
// and starts the actual production runtime in a separate paired profile.
using DISABLED_RuntimePackageCe = RuntimePackagePreparation;
TEST_F(DISABLED_RuntimePackageCe, RegisteredPrivatePreparationStopsBeforeAospLogout) {
    aegis_ce_test::Identity identity;ASSERT_TRUE(aegis_ce_test::Read(&identity));
    ASSERT_EQ(0,aegis_broker_owner_create(parent.get(),source.get(),execute_helper.get(),execute_helper.get(),&broker));
    plan.execution.requester=identity.user;plan.execution.serial=identity.serial;plan.execution.job=0;
    int before=CountFDs();
    ASSERT_EQ(0,BrokerPreparePersonalCandidate(broker,plan,parent.get(),source.get(),prepare_helper.get(),
                                              execute_helper.get(),Fds(),Deadline(),&job))<<strerror(errno);
    PublicationState state=PublicationState::Preparing;PackageExecutionResult result;
    for(unsigned i=0;i<2000 && state==PublicationState::Preparing;++i) {
        ASSERT_EQ(0,aegis_broker_owner_reap_publications(broker));
        ASSERT_EQ(0,BrokerPollExecution(broker,identity.user,identity.serial,job,plan.execution.plan_sha256,&state,&result));
        if(state==PublicationState::Preparing)usleep(10000);
    }
    ASSERT_EQ(PublicationState::Prepared,state)<<result.error;
    EXPECT_EQ(before+4,CountFDs()); // registered cgroup/stage/mount/executable
    aegis_broker_request r={};r.magic=AEGIS_BROKER_MAGIC;r.version=AEGIS_BROKER_VERSION;
    r.operation=AEGIS_BROKER_STOP_USER;r.user=identity.user;r.deadline_ns=Deadline();r.sequence=4;
    aegis_broker_state stopped=AEGIS_BROKER_SEALED;
    ASSERT_EQ(0,aegis_broker_owner_apply(broker,&r,&stopped));EXPECT_EQ(AEGIS_BROKER_ABSENT,stopped);
    EXPECT_EQ(before,CountFDs());
    // Actual CE eviction is asserted next through AOSP logout by the host
    // driver. This test does not pretend its direct owner is the live daemon.
}

TEST_F(RuntimeSelectionOwner, StartContinuationCannotRecreateStoppedOrReplacedJob) {
    int fds=CountFDs();uint64_t job=99;
    EXPECT_EQ(-1,StartJob(10,42,17,&job));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,job);EXPECT_EQ(fds,CountFDs());
    ASSERT_EQ(0,RegisterSelection());Freeze();ASSERT_FALSE(HasFatalFailure());auto original=selected_job;
    EXPECT_EQ(-1,StartJob(10,42,0,&job));EXPECT_EQ(EAGAIN,errno);EXPECT_EQ(original,job);
    EXPECT_EQ(-1,StartJob(10,42,original,&job));EXPECT_EQ(EAGAIN,errno);EXPECT_EQ(original,job);
    EXPECT_EQ(-1,StartJob(11,42,original,&job));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,job);
    EXPECT_EQ(-1,StartJob(10,43,original,&job));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,job);
    ASSERT_EQ(0,Stop());EXPECT_EQ(fds,CountFDs());
    EXPECT_EQ(-1,StartJob(10,42,original,&job));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,job);EXPECT_EQ(fds,CountFDs());
    ASSERT_EQ(0,RegisterSelection());Freeze();ASSERT_FALSE(HasFatalFailure());EXPECT_GT(selected_job,original);
    EXPECT_EQ(-1,StartJob(10,42,original,&job));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,job);
    EXPECT_EQ(-1,StartJob(10,42,selected_job,&job));EXPECT_EQ(EAGAIN,errno);EXPECT_EQ(selected_job,job);
    ASSERT_EQ(0,Stop());EXPECT_EQ(fds,CountFDs());
}
TEST_F(RuntimeSelectionOwner, HelloInvalidatesContinuationEvenWithSameIdentity) {
    ASSERT_EQ(0,RegisterSelection());Freeze();ASSERT_FALSE(HasFatalFailure());auto original=selected_job;
    aegis_broker_state state;ASSERT_EQ(0,Apply(AEGIS_BROKER_HELLO,0,0,&state));
    uint64_t job=99;EXPECT_EQ(-1,StartJob(10,42,original,&job));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,job);
    EXPECT_EQ("populated 0\nfrozen 0\n",AptImageFixture::read(parent.get(),"cgroup.events"));
}
TEST_F(RuntimeSelectionOwner, FailedSelectionIsNotPendingAndCannotSilentlyRestart) {
    selection.factory.sha256=std::string(64,'f');ASSERT_EQ(0,RegisterSelection());
    RuntimeSelectionState state=RuntimeSelectionState::Selecting;PackagePreparationResult result;
    for(unsigned i=0;i<900 && state==RuntimeSelectionState::Selecting;++i) {
        ASSERT_EQ(0,BrokerPollRuntimeSelection(broker,10,42,selected_job,&state,&result));
        if(state==RuntimeSelectionState::Selecting)usleep(10000);
    }
    ASSERT_EQ(RuntimeSelectionState::Failed,state);uint64_t job=99;
    EXPECT_EQ(-1,StartJob(10,42,selected_job,&job));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,job);
    EXPECT_EQ(-1,StartJob(10,42,0,&job));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,job);ASSERT_EQ(0,Stop());
}

TEST_F(RuntimeSelectionOwner, BootstrapCopiesInputsOnceAndRejectsInvalidConfigurationWithoutLeaks) {
    int before=CountFDs();aegis_base_receipt receipt={};receipt.bytes=selection.factory.bytes;
    memcpy(receipt.sha256,selection.factory.sha256.c_str(),65);
    EXPECT_EQ(-1,aegis_broker_owner_enable_selection(broker,-1,&receipt,prepare_helper.get(),stage.get()));
    EXPECT_EQ(before,CountFDs());
    EXPECT_EQ(-1,aegis_broker_owner_enable_selection(broker,factory.get(),&receipt,factory.get(),stage.get()));
    EXPECT_EQ(before,CountFDs());
    receipt.bytes+=4096;EXPECT_EQ(-1,aegis_broker_owner_enable_selection(broker,factory.get(),&receipt,prepare_helper.get(),stage.get()));
    EXPECT_EQ(ESTALE,errno);EXPECT_EQ(before,CountFDs());
    ASSERT_EQ(0,Configure());EXPECT_EQ(before+3,CountFDs());
    EXPECT_EQ(-1,Configure());EXPECT_EQ(EALREADY,errno);EXPECT_EQ(before+3,CountFDs());
    ASSERT_EQ(0,aegis_broker_owner_release(&broker));EXPECT_EQ(before-4,CountFDs());
}
TEST_F(RuntimeSelectionOwner, ConfiguredStartRegistersBeforeMissingCeAndCannotUseLegacyBase) {
    struct stat absent;ASSERT_EQ(-1,lstat("/data/system_ce/21472",&absent));ASSERT_EQ(ENOENT,errno);
    ASSERT_EQ(0,Configure());int before=CountFDs();uint64_t job=99;
    EXPECT_EQ(-1,StartJob(21472,1234,0,&job));EXPECT_EQ(ENOENT,errno);EXPECT_EQ(0u,job);
    RuntimeSelectionState state;PackagePreparationResult result;
    ASSERT_EQ(0,BrokerPollRuntimeSelection(broker,21472,1234,1,&state,&result));
    EXPECT_EQ(RuntimeSelectionState::Failed,state);EXPECT_EQ(ENOENT,result.error);EXPECT_EQ(before,CountFDs());
    aegis_broker_state native;
    ASSERT_EQ(0,Apply(AEGIS_BROKER_STOP_USER,21472,0,&native));
    EXPECT_EQ(-1,StartJob(21472,1234,1,&job));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,job);
}
TEST_F(RuntimeSelectionOwner, ConfiguredStartDoesNotTreatSymlinkOrWrongModeSharedStoreAsAbsent) {
    ASSERT_EQ(0,Configure());uint64_t job=99;
    ASSERT_EQ(0,symlinkat("/",stage.get(),"shared-packages"));
    ASSERT_EQ(-1,StartJob(21472,1234,0,&job));const int rejected=errno;
    // O_DIRECTORY|O_NOFOLLOW may reject the final symlink as ENOTDIR before
    // the resolver reports ELOOP. Neither is absence or permits fallback.
    EXPECT_TRUE(rejected==ELOOP || rejected==ENOTDIR);EXPECT_EQ(0u,job);
    RuntimeSelectionState selected;PackagePreparationResult result;
    ASSERT_EQ(0,BrokerPollRuntimeSelection(broker,21472,1234,1,&selected,&result));
    EXPECT_EQ(RuntimeSelectionState::Failed,selected);EXPECT_EQ(rejected,result.error);
    aegis_broker_state state;ASSERT_EQ(0,Apply(AEGIS_BROKER_STOP_USER,21472,0,&state));
    ASSERT_EQ(0,unlinkat(stage.get(),"shared-packages",0));
    ASSERT_EQ(0,mkdirat(stage.get(),"shared-packages",0700));
    unique_fd directory(openat(stage.get(),"shared-packages",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(directory.ok());
    ASSERT_EQ(0,fchmod(directory.get(),0755));
    EXPECT_EQ(-1,StartJob(21472,1234,0,&job));EXPECT_EQ(EPERM,errno);EXPECT_EQ(0u,job);
    ASSERT_EQ(0,Apply(AEGIS_BROKER_STOP_USER,21472,0,&state));
    directory.reset();ASSERT_EQ(0,unlinkat(stage.get(),"shared-packages",AT_REMOVEDIR));
}
TEST_F(RuntimeSelectionOwner, BootstrapCannotChangeFactoryAfterWorkHasBeenRegistered) {
    ASSERT_EQ(0,RegisterSelection());EXPECT_EQ(-1,Configure());EXPECT_EQ(EALREADY,errno);
    ASSERT_EQ(0,Stop());EXPECT_EQ(-1,Configure());EXPECT_EQ(EALREADY,errno);
}

TEST_F(RuntimePackageTransaction, BoundPlanFeedsRealAptAndRejectsDifferentApprovalDigest) {
    PackageResolvedPlan resolved;resolved.initial_apt_state_presence=PackageStatePresence::Absent;resolved.requester=10;resolved.serial=42;resolved.create_store=true;
    resolved.requested_package="aegis-exec-app";resolved.requested_version="1";
    resolved.source=resolved.shared=plan.image;resolved.planner_image_sha256=plan.image.sha256;
    // Direct preparation needs its own positive job ID; the broker assigns
    // the real transaction ID only after the bound plan has been registered.
    plan.execution.job=1;
    Ready();ASSERT_FALSE(HasFatalFailure());plan.execution.job=0;
    aegis_package_execution_review initial={};InitialReview(mount.get(),&initial);ASSERT_FALSE(HasFatalFailure());
    resolved.initial_status_sha256=initial.initial_status;
    resolved.initial_apt_state_presence=static_cast<PackageStatePresence>(initial.apt_state_presence);
    if(initial.apt_state_presence==2)resolved.initial_apt_state={initial.apt_state_bytes,initial.initial_apt_state};
    mount.reset();NoLoop();ASSERT_FALSE(HasFailure());NewStage();ASSERT_FALSE(HasFatalFailure());
    // Repository fixture hashes remain synthetic, not signed metadata evidence.
    resolved.policy_sha256=std::string(64,'a');
    resolved.repositories={{"fixture",std::string(64,'c'),std::string(64,'d'),2000}};
    resolved.changes={{"aegis-exec-app","all","","1","fixture",plan.archives[1],PackageInstallReason::Manual},
                      {"aegis-exec-lib","all","","1","fixture",plan.archives[0],PackageInstallReason::Automatic}};
    PackageBoundPlan bound;ASSERT_EQ(0,PackageBindResolvedPlan(resolved,1000,&bound))<<strerror(errno);
    plan=bound.preparation;target=bound.publication;std::swap(archives[0],archives[1]);
    ASSERT_EQ(0,BrokerPrepareTransaction(broker,plan,target,parent.get(),stage.get(),store.get(),source.get(),
        prepare_helper.get(),execute_helper.get(),publish_helper.get(),Fds(),Deadline(),&job))<<strerror(errno);
    AwaitPrepared();ASSERT_FALSE(HasFatalFailure());
    auto changed=resolved;changed.requested_version.clear();PackageBoundPlan other;
    ASSERT_EQ(0,PackageBindResolvedPlan(changed,1000,&other));
    ASSERT_NE(other.preparation.execution.plan_sha256,plan.execution.plan_sha256);
    EXPECT_EQ(-1,BrokerStartExecution(broker,10,42,job,other.preparation.execution.plan_sha256,Deadline()));EXPECT_EQ(ESTALE,errno);
    // A dependency-mark change also invalidates the same fresh approval target.
    // The accepted worker subsequently applies and verifies the sealed marks.
    changed=resolved;changed.changes[1].reason=PackageInstallReason::Manual;
    ASSERT_EQ(0,PackageBindResolvedPlan(changed,1000,&other));
    EXPECT_EQ(-1,BrokerStartExecution(broker,10,42,job,other.preparation.execution.plan_sha256,Deadline()));EXPECT_EQ(ESTALE,errno);
    PublicationState state;PackageExecutionResult result;
    ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));
    ASSERT_EQ(PublicationState::Prepared,state);
    ASSERT_EQ(0,BrokerStartExecution(broker,10,42,job,plan.execution.plan_sha256,Deadline()))<<strerror(errno);
    for(unsigned i=0;i<1600;i++) {
        ASSERT_EQ(0,aegis_broker_owner_reap_publications(broker));
        ASSERT_EQ(0,BrokerPollExecution(broker,10,42,job,plan.execution.plan_sha256,&state,&result));
        if(state==PublicationState::Complete)break;
        usleep(10000);
    }
    ASSERT_EQ(PublicationState::Complete,state);
    ASSERT_EQ(PackageExecutionOutcome::Published,result.outcome)<<result.error;
    VerifyContents(result,1);ASSERT_FALSE(HasFailure());
}

TEST_F(RuntimeSelectionOwner, PackageSelectionCannotAuthorizeRuntimeStartAndCancellationIsBoundToItsJob) {
    ASSERT_EQ(0,Configure());int fds=CountFDs();
    ASSERT_EQ(0,BrokerPreparePackageSelection(broker,selection,false,parent.get(),-1,-1,
        factory.get(),prepare_helper.get(),Deadline(),&selected_job));
    PackagePreparationResult result;AwaitSelection(&result);ASSERT_FALSE(HasFatalFailure());
    const auto first=selected_job;uint64_t unexpected=0;
    EXPECT_EQ(-1,StartJob(10,42,0,&unexpected));EXPECT_EQ(EBUSY,errno);EXPECT_EQ(0u,unexpected);
    EXPECT_EQ(-1,StartJob(10,42,first,&unexpected));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,unexpected);
    EXPECT_EQ(-1,BrokerCancelPackageSelection(broker,11,42,first,Deadline()));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,BrokerCancelPackageSelection(broker,10,43,first,Deadline()));EXPECT_EQ(ESTALE,errno);
    uint64_t second=0;
    EXPECT_EQ(-1,BrokerPreparePackageSelection(broker,selection,true,parent.get(),-1,-1,
        factory.get(),prepare_helper.get(),Deadline(),&second));EXPECT_EQ(EBUSY,errno);EXPECT_EQ(0u,second);
    ASSERT_EQ(0,BrokerCancelPackageSelection(broker,10,42,first,Deadline()));EXPECT_EQ(fds,CountFDs());
    ASSERT_EQ(0,RegisterSelection());EXPECT_GT(selected_job,first);
    EXPECT_EQ(-1,BrokerCancelPackageSelection(broker,10,42,selected_job,Deadline()));EXPECT_EQ(EPERM,errno);
    ASSERT_EQ(0,Stop());EXPECT_EQ(fds,CountFDs());
}
TEST_F(RuntimeSelectionOwner, ConfiguredPackageScopeUsesPinnedInputsAndOnlyPersonalScopeOpensCe) {
    struct stat absent;ASSERT_EQ(-1,lstat("/data/system_ce/21472",&absent));ASSERT_EQ(ENOENT,errno);
    selection.requester=21472;selection.serial=1234;
    EXPECT_EQ(-1,BrokerPrepareConfiguredPackageSelection(broker,21472,1234,false,Deadline(),&selected_job));
    EXPECT_EQ(ENOTSUP,errno);EXPECT_EQ(0u,selected_job);
    ASSERT_EQ(0,Configure());int fds=CountFDs();
    ASSERT_EQ(0,BrokerPrepareConfiguredPackageSelection(broker,21472,1234,false,Deadline(),&selected_job));
    PackagePreparationResult result;AwaitSelection(&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(PackagePreparationResult::Scope::Factory,result.scope);
    ASSERT_EQ(0,BrokerCancelPackageSelection(broker,21472,1234,selected_job,Deadline()));
    const auto shared_job=selected_job;selected_job=0;
    EXPECT_EQ(-1,BrokerPrepareConfiguredPackageSelection(broker,21472,1234,true,Deadline(),&selected_job));
    EXPECT_EQ(ENOENT,errno);EXPECT_GT(selected_job,shared_job);
    RuntimeSelectionState state;ASSERT_EQ(0,BrokerPollRuntimeSelection(broker,21472,1234,selected_job,&state,&result));
    EXPECT_EQ(RuntimeSelectionState::Failed,state);EXPECT_EQ(ENOENT,result.error);
    ASSERT_EQ(0,BrokerCancelPackageSelection(broker,21472,1234,selected_job,Deadline()));EXPECT_EQ(fds,CountFDs());
}
