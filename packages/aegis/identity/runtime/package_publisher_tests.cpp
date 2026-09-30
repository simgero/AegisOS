// Compile on aegis-build, execute in local QEMU. Real publication children and
// cgroups, inert filesystem payloads; no AOSP users, CE store or APT execution.
#include "package_publisher.h"
#include "package_publish_protocol.h"
#include "namespace.h"
#include <android-base/unique_fd.h>
#include <gtest/gtest.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
using android::base::unique_fd;
using namespace aegis;
namespace {
std::string Digest(const void* bytes,size_t size) {
    unsigned char digest[SHA256_DIGEST_LENGTH];SHA256(static_cast<const unsigned char*>(bytes),size,digest);
    char text[65];for(unsigned i=0;i<32;++i)snprintf(text+2*i,3,"%02x",digest[i]);return text;
}
std::string Get(int directory,const char* path) {
    unique_fd fd(openat(directory,path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
    if(!fd.ok())return "<failed>";
    char text[1024];ssize_t n=read(fd.get(),text,sizeof(text));
    return n<0?"<failed>":std::string(text,n);
}
int Put(int directory,const char* path,const char* text) {
    unique_fd fd(openat(directory,path,O_WRONLY|O_CLOEXEC|O_NOFOLLOW));
    if(!fd.ok())return -1;
    return write(fd.get(),text,strlen(text))==static_cast<ssize_t>(strlen(text)) ? 0 : -1;
}
int Descriptors() {
    DIR* dir=opendir("/proc/self/fd");if(!dir)return -1;
    int count=0;while(auto* e=readdir(dir))if(e->d_name[0]!='.')++count;
    closedir(dir);return count;
}
class RuntimePackagePublisher : public ::testing::Test {
 protected:
    unique_fd cgroups,parent,directory,store,source,helper;
    PackagePublisher* jobs[2]={nullptr,nullptr};
    std::string group_name,path;
    PackagePublication request;
    bool created=false,helper_copy_created=false;
    void SetUp() override {
        ASSERT_EQ(0u,getuid());ASSERT_EQ(0u,getgid());ASSERT_EQ(0,setgroups(0,nullptr));
        struct sigaction action={};action.sa_handler=SIG_DFL;sigemptyset(&action.sa_mask);
        ASSERT_EQ(0,sigaction(SIGCHLD,&action,nullptr));
        unique_fd u(open("/proc/1/ns/user",O_RDONLY|O_CLOEXEC));
        unique_fd p(open("/proc/1/ns/pid",O_RDONLY|O_CLOEXEC));
        unique_fd m(open("/proc/1/ns/mnt",O_RDONLY|O_CLOEXEC));
        ASSERT_EQ(0,aegis_namespace_pin_host(u.get(),p.get(),m.get())) << strerror(errno);
        cgroups.reset(open("/sys/fs/cgroup",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(cgroups.ok());
        group_name="aegis-publisher-test-"+std::to_string(getpid());
        ASSERT_EQ(0,mkdirat(cgroups.get(),group_name.c_str(),0700));created=true;
        parent.reset(openat(cgroups.get(),group_name.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
        ASSERT_TRUE(parent.ok());ASSERT_EQ(0,fchown(parent.get(),0,0));ASSERT_EQ(0,fchmod(parent.get(),0700));
        ASSERT_EQ("",Get(parent.get(),"cgroup.procs"));
        ASSERT_EQ(0,Put(parent.get(),"cgroup.subtree_control","+memory\n"));
        char name[]="/data/local/tmp/aegis-publisher-XXXXXX";
        ASSERT_NE(nullptr,mkdtemp(name));path=name;RecordProperty("fixture",path);
        directory.reset(open(name,O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(directory.ok());
        ASSERT_EQ(0,mkdirat(directory.get(),"store",0700));
        store.reset(openat(directory.get(),"store",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(store.ok());
        char executable[PATH_MAX];ssize_t n=readlink("/proc/self/exe",executable,sizeof(executable)-1);
        ASSERT_GT(n,0);executable[n]=0;
        std::string binary(executable);binary.resize(binary.find_last_of('/')+1);
        binary+="aegis-package-publish";
        helper.reset(open(binary.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(helper.ok());
        request.requester=10;request.serial=42;request.job=1;
        request.plan_sha256=std::string(64,'a');request.create=true;
        Source("complete generation");
    }
    void Source(const std::string& value) {
        unique_fd writefd(openat(directory.get(),"source",O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600));
        ASSERT_TRUE(writefd.ok());ASSERT_EQ(static_cast<ssize_t>(value.size()),write(writefd.get(),value.data(),value.size()));
        ASSERT_EQ(0,fsync(writefd.get()));writefd.reset();
        source.reset(openat(directory.get(),"source",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(source.ok());
        request.candidate={Digest(value.data(),value.size()),"",value.size()};
        if(request.personal)request.candidate.shared_base_sha256=std::string(64,'b');
    }
    int Start(unsigned slot=0) {
        return PackagePublisherStart(parent.get(),store.get(),source.get(),helper.get(),request,&jobs[slot]);
    }
    int Finish(unsigned slot,PackagePublicationResult* result,bool cancel=false,int wait=5000) {
        return PackagePublisherFinish(&jobs[slot],cancel,wait,result);
    }
    void Selection(const std::string& expected,bool personal=false) {
        PackageOwner owner{personal,personal?10u:0u,personal?42u:0u};
        std::unique_ptr<PackageStore> selected(PackageStore::Open(store.get(),owner,false));
        ASSERT_NE(nullptr,selected.get()) << strerror(errno);
        PackageGeneration generation;unique_fd image(selected->Current(&generation));
        ASSERT_TRUE(image.ok()) << strerror(errno);
        char bytes[128];ssize_t n=pread(image.get(),bytes,sizeof(bytes),0);ASSERT_GE(n,0);
        EXPECT_EQ(expected,std::string(bytes,n));
        EXPECT_EQ(request.candidate.image_sha256,generation.image_sha256);
    }
    void TearDown() override {
        for(auto*& job:jobs)if(job) {
            PackagePublicationResult result;
            EXPECT_EQ(0,PackagePublisherFinish(&job,true,5000,&result)) << strerror(errno);
        }
        if(parent.ok())EXPECT_EQ("populated 0\nfrozen 0\n",Get(parent.get(),"cgroup.events"));
        parent.reset();
        if(created)EXPECT_EQ(0,unlinkat(cgroups.get(),group_name.c_str(),AT_REMOVEDIR)) << strerror(errno);
        // Keep failure evidence. On success remove ONLY this fixture's ordinary
        // source and files under its freshly exclusive mkdtemp store.
        if(!HasFailure() && store.ok()) {
            unique_fd scan(openat(store.get(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
            DIR* entries=fdopendir(scan.release());ASSERT_NE(nullptr,entries);
            while(auto* item=readdir(entries)) {
                if(!strcmp(item->d_name,".")||!strcmp(item->d_name,".."))continue;
                struct stat st={};ASSERT_EQ(0,fstatat(store.get(),item->d_name,&st,AT_SYMLINK_NOFOLLOW));
                ASSERT_TRUE(S_ISREG(st.st_mode));ASSERT_EQ(0u,st.st_uid);
                EXPECT_EQ(0,unlinkat(store.get(),item->d_name,0));
            }
            closedir(entries);store.reset();source.reset();helper.reset();
            if(helper_copy_created)EXPECT_EQ(0,unlinkat(directory.get(),"helper-copy",0));
            EXPECT_EQ(0,unlinkat(directory.get(),"source",0));
            EXPECT_EQ(0,unlinkat(directory.get(),"store",AT_REMOVEDIR));
            directory.reset();EXPECT_EQ(0,rmdir(path.c_str()));
        }
    }
};
TEST_F(RuntimePackagePublisher, PublishesWithActualChildAndClosesOwnedDescriptors) {
    int before=Descriptors();ASSERT_GT(before,0);
    ASSERT_EQ(0,Start()) << strerror(errno);ASSERT_NE(nullptr,jobs[0]);
    PackagePublicationResult result;ASSERT_EQ(0,Finish(0,&result)) << strerror(errno);
    EXPECT_EQ(nullptr,jobs[0]);EXPECT_EQ(PackagePublish::Confirmed,result.publication);
    EXPECT_EQ(0,result.error);EXPECT_EQ(before,Descriptors());Selection("complete generation");
}
TEST_F(RuntimePackagePublisher, SystemHelperGroupAllowedButDataAndOtherGroupsRejected) {
    // Mutate only a newly owned copy, never the shared staged executable.
    unique_fd copy(openat(directory.get(),"helper-copy",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0700));
    ASSERT_TRUE(copy.ok());helper_copy_created=true;
    std::array<char,16384> bytes={};off_t offset=0;
    for(;;) {
        ssize_t count=TEMP_FAILURE_RETRY(pread(helper.get(),bytes.data(),bytes.size(),offset));
        ASSERT_GE(count,0);if(count==0)break;
        for(ssize_t written=0;written<count;) {
            ssize_t n=TEMP_FAILURE_RETRY(write(copy.get(),bytes.data()+written,count-written));
            ASSERT_GT(n,0);written+=n;
        }
        offset+=count;
    }
    ASSERT_EQ(0,fchown(copy.get(),0,2000));ASSERT_EQ(0,fchmod(copy.get(),0755));
    ASSERT_EQ(0,fsync(copy.get()));copy.reset();
    helper.reset(openat(directory.get(),"helper-copy",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(helper.ok());
    int before=Descriptors();
    ASSERT_EQ(0,Start()) << strerror(errno);PackagePublicationResult result;
    ASSERT_EQ(0,Finish(0,&result)) << strerror(errno);
    ASSERT_EQ(PackagePublish::Confirmed,result.publication);Selection("complete generation");
    EXPECT_EQ(before,Descriptors());request.job=2;
    ASSERT_EQ(0,fchown(helper.get(),0,2001));
    EXPECT_EQ(-1,Start());EXPECT_EQ(EPERM,errno);EXPECT_EQ(nullptr,jobs[0]);
    ASSERT_EQ(0,fchown(helper.get(),0,2000));
    ASSERT_EQ(0,fchown(source.get(),0,2000));
    EXPECT_EQ(-1,Start());EXPECT_EQ(EPERM,errno);EXPECT_EQ(nullptr,jobs[0]);
    ASSERT_EQ(0,fchown(source.get(),0,0));ASSERT_EQ(0,fchown(store.get(),0,2000));
    EXPECT_EQ(-1,Start());EXPECT_EQ(EPERM,errno);EXPECT_EQ(nullptr,jobs[0]);
    ASSERT_EQ(0,fchown(store.get(),0,0));EXPECT_EQ(before,Descriptors());
}
TEST_F(RuntimePackagePublisher, ChildUsesOwnedCopiesWhenCallerClosesSourceAndDirectory) {
    ASSERT_EQ(0,Start()) << strerror(errno);source.reset();store.reset();
    PackagePublicationResult result;ASSERT_EQ(0,Finish(0,&result));
    ASSERT_EQ(PackagePublish::Confirmed,result.publication);
    store.reset(openat(directory.get(),"store",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    Selection("complete generation");
}
TEST_F(RuntimePackagePublisher, PersonalTargetUsesRequesterAndRejectsAnotherSerial) {
    request.personal=true;Source("private generation");
    ASSERT_EQ(0,Start());PackagePublicationResult result;ASSERT_EQ(0,Finish(0,&result));
    ASSERT_EQ(PackagePublish::Confirmed,result.publication);Selection("private generation",true);
    request.create=false;request.has_previous=true;request.previous=request.candidate;
    request.serial=43;request.job=2;
    ASSERT_EQ(0,Start());ASSERT_EQ(0,Finish(0,&result));
    EXPECT_EQ(PackagePublish::Rejected,result.publication);EXPECT_EQ(ESTALE,result.error);
    Selection("private generation",true);
}
TEST_F(RuntimePackagePublisher, ConflictingStartNeverOwnsOrKillsExistingGroup) {
    ASSERT_EQ(0,Start());request.job=2;
    EXPECT_EQ(-1,Start(1));EXPECT_EQ(EEXIST,errno);ASSERT_NE(nullptr,jobs[1]);
    PackagePublicationResult second;ASSERT_EQ(0,Finish(1,&second,true));
    EXPECT_EQ(PackagePublish::Rejected,second.publication);
    PackagePublicationResult first;ASSERT_EQ(0,Finish(0,&first));
    EXPECT_EQ(PackagePublish::Confirmed,first.publication);Selection("complete generation");
}
TEST_F(RuntimePackagePublisher, HashFailureDoesNotPublishSelection) {
    request.candidate.image_sha256=std::string(64,'f');
    ASSERT_EQ(0,Start());PackagePublicationResult result;ASSERT_EQ(0,Finish(0,&result));
    EXPECT_EQ(PackagePublish::Rejected,result.publication);EXPECT_EQ(ESTALE,result.error);
    struct stat st={};EXPECT_EQ(-1,fstatat(store.get(),"current",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackagePublisher, InvalidPlanAndWritableSourceNeverStart) {
    int before=Descriptors();request.job=0;
    EXPECT_EQ(-1,Start());EXPECT_EQ(EINVAL,errno);EXPECT_EQ(nullptr,jobs[0]);
    request.job=1;request.plan_sha256="invalid";
    EXPECT_EQ(-1,Start());EXPECT_EQ(EINVAL,errno);EXPECT_EQ(nullptr,jobs[0]);
    request.plan_sha256=std::string(64,'a');source.reset(openat(directory.get(),"source",O_RDWR|O_CLOEXEC));
    EXPECT_EQ(-1,Start());EXPECT_EQ(EPERM,errno);EXPECT_EQ(nullptr,jobs[0]);EXPECT_EQ(before,Descriptors());
}
TEST_F(RuntimePackagePublisher, BorrowedHandleInAnotherProcessCannotFinishOwnersJob) {
    ASSERT_EQ(0,Start());pid_t child=fork();ASSERT_GE(child,0);
    if(!child) {
        PackagePublicationResult result;
        int returned=Finish(0,&result,true,0);
        _exit(returned==-1&&errno==EPERM&&jobs[0] ? 0 : 90);
    }
    int status=0;ASSERT_EQ(child,waitpid(child,&status,0));ASSERT_TRUE(WIFEXITED(status));EXPECT_EQ(0,WEXITSTATUS(status));
    PackagePublicationResult result;ASSERT_EQ(0,Finish(0,&result));EXPECT_EQ(PackagePublish::Confirmed,result.publication);
}
TEST_F(RuntimePackagePublisher, CancelDuringObservedCopyReapsChildAndRetainsOldSelection) {
    ASSERT_EQ(0,Start());PackagePublicationResult first;ASSERT_EQ(0,Finish(0,&first));
    ASSERT_EQ(PackagePublish::Confirmed,first.publication);
    auto previous=request.candidate;request.create=false;request.has_previous=true;request.previous=previous;request.job=2;
    constexpr size_t total=512u*1024u*1024u;
    source.reset();unique_fd writefd(openat(directory.get(),"source",O_WRONLY|O_TRUNC|O_CLOEXEC));ASSERT_TRUE(writefd.ok());
    ASSERT_EQ(0,ftruncate(writefd.get(),total));ASSERT_EQ(0,fsync(writefd.get()));writefd.reset();
    source.reset(openat(directory.get(),"source",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(source.ok());
    std::array<unsigned char,128*1024> zeros={};SHA256_CTX sha;SHA256_Init(&sha);
    for(size_t i=0;i<total;i+=zeros.size())SHA256_Update(&sha,zeros.data(),zeros.size());
    unsigned char bytes[32];SHA256_Final(bytes,&sha);char digest[65];
    for(unsigned i=0;i<32;++i)snprintf(digest+2*i,3,"%02x",bytes[i]);
    request.candidate={digest,"",total};
    unique_fd notify(inotify_init1(IN_CLOEXEC|IN_NONBLOCK));ASSERT_TRUE(notify.ok());
    ASSERT_GE(inotify_add_watch(notify.get(),(path+"/store").c_str(),IN_MODIFY),0);
    ASSERT_EQ(0,Start()) << strerror(errno);
    unique_fd group(openat(parent.get(),"u10-s42",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(group.ok());
    pollfd ready{notify.get(),POLLIN,0};ASSERT_EQ(1,poll(&ready,1,5000));
    // Freeze this exclusively owned test cgroup after a real file modification.
    // Verify an incomplete copied file before testing forced cancellation.
    ASSERT_EQ(0,Put(group.get(),"cgroup.freeze","1\n"));
    bool frozen=false;
    for(int i=0;i<5000;++i) {
        if(Get(group.get(),"cgroup.events").find("frozen 1\n")!=std::string::npos) { frozen=true;break; }
        usleep(1000);
    }
    ASSERT_TRUE(frozen);
    unique_fd scan(openat(store.get(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    DIR* entries=fdopendir(scan.release());ASSERT_NE(nullptr,entries);bool partial=false;
    while(auto* item=readdir(entries))if(!strncmp(item->d_name,".pending-",9)) {
        struct stat st={};ASSERT_EQ(0,fstatat(store.get(),item->d_name,&st,AT_SYMLINK_NOFOLLOW));
        if(st.st_size>0&&static_cast<uint64_t>(st.st_size)<total)partial=true;
    }
    closedir(entries);ASSERT_TRUE(partial) << "No observed mid-copy state; do not claim cancellation coverage";
    PackagePublicationResult result{PackagePublish::Confirmed,77};
    EXPECT_EQ(-1,Finish(0,&result,false,0));EXPECT_EQ(ETIMEDOUT,errno);
    ASSERT_NE(nullptr,jobs[0]);EXPECT_EQ(PackagePublish::Confirmed,result.publication);
    EXPECT_EQ(77,result.error);EXPECT_NE(std::string::npos,Get(group.get(),"cgroup.events").find("populated 1\n"));
    ASSERT_EQ(0,Finish(0,&result,true)) << strerror(errno);
    EXPECT_EQ(PackagePublish::Unconfirmed,result.publication);EXPECT_EQ(nullptr,jobs[0]);
    request.candidate=previous;Selection("complete generation");
    // The child and all owned references are gone. Incomplete pending bytes are
    // not activated. This is process cancellation, not a physical power failure.
}
} // namespace

#include "broker_owner_package.h"
namespace {
class RuntimePackageBroker : public RuntimePackagePublisher {
 protected:
    aegis_broker_owner* broker=nullptr;
    unique_fd fault_group;
    bool ownership_fault=false;
    uint64_t Deadline() {
        timespec now={};if(clock_gettime(CLOCK_MONOTONIC,&now)<0)return 0;
        return static_cast<uint64_t>(now.tv_sec)*1000000000+now.tv_nsec+5000000000;
    }
    void SetUp() override {
        RuntimePackagePublisher::SetUp();if(HasFatalFailure())return;
        request.job=0;
        ASSERT_EQ(0,aegis_broker_owner_create(parent.get(),parent.get(),parent.get(),parent.get(),&broker));
    }
    void TearDown() override {
        if(ownership_fault)EXPECT_EQ(0,fchown(fault_group.get(),0,0));
        fault_group.reset();
        if(broker) {
            EXPECT_EQ(0,aegis_broker_owner_stop_all(broker,Deadline())) << strerror(errno);
            EXPECT_EQ(0,aegis_broker_owner_release(&broker)) << strerror(errno);
        }
        RuntimePackagePublisher::TearDown();
    }
    int Prepare(uint64_t* id,uint32_t user=10,int groups=-2) {
        request.requester=user;
        return BrokerPreparePublication(broker,request,groups==-2?parent.get():groups,
            store.get(),source.get(),helper.get(),Deadline(),id);
    }
    int Run(uint64_t id,uint32_t user=10) {
        return BrokerStartPublication(broker,user,42,id,request.plan_sha256,Deadline());
    }
    int Apply(uint16_t operation,uint32_t user,uint32_t serial,aegis_broker_state* state) {
        aegis_broker_request call={};call.magic=AEGIS_BROKER_MAGIC;call.version=AEGIS_BROKER_VERSION;
        call.operation=operation;call.sequence=operation==AEGIS_BROKER_HELLO?1:2;
        call.deadline_ns=Deadline();call.user=user;call.serial=serial;
        return aegis_broker_owner_apply(broker,&call,state);
    }
    void Completed(uint64_t id,uint32_t user,PackagePublicationResult* result) {
        PublicationState state=PublicationState::Running;
        for(int i=0;i<5000;++i) {
            int polled=BrokerPollPublication(broker,user,42,id,request.plan_sha256,&state,result);
            ASSERT_EQ(0,polled) << strerror(errno);
            if(state==PublicationState::Complete)return;
            usleep(1000);
        }
        FAIL() << "Owned publication did not complete";
    }
};
TEST_F(RuntimePackageBroker, StopUserReleasesOnlyItsPreparationsAndIdsNeverRecycle) {
    int baseline=Descriptors();uint64_t a=0,b=0;
    ASSERT_EQ(0,Prepare(&a,10));ASSERT_EQ(0,Prepare(&b,11));EXPECT_GT(b,a);
    EXPECT_EQ(baseline+8,Descriptors());
    EXPECT_EQ(-1,aegis_broker_owner_release(&broker));EXPECT_EQ(EBUSY,errno);
    aegis_broker_state state=AEGIS_BROKER_ABSENT;
    ASSERT_EQ(0,Apply(AEGIS_BROKER_STATUS,10,42,&state));EXPECT_EQ(AEGIS_BROKER_SEALED,state);
    ASSERT_EQ(0,Apply(AEGIS_BROKER_STOP_USER,10,0,&state));EXPECT_EQ(AEGIS_BROKER_ABSENT,state);
    EXPECT_EQ(baseline+4,Descriptors());
    EXPECT_EQ(-1,Run(a,10));EXPECT_EQ(ENOENT,errno);
    PublicationState phase=PublicationState::Complete;PackagePublicationResult result{PackagePublish::Confirmed,77};
    ASSERT_EQ(0,BrokerPollPublication(broker,11,42,b,request.plan_sha256,&phase,&result));
    EXPECT_EQ(PublicationState::Prepared,phase);EXPECT_EQ(77,result.error);
    uint64_t c=0;ASSERT_EQ(0,Prepare(&c,10));EXPECT_GT(c,b);
    ASSERT_EQ(0,aegis_broker_owner_stop_all(broker,0));EXPECT_EQ(baseline,Descriptors());
}
TEST_F(RuntimePackageBroker, ExactRequesterSerialPlanAndSingleStartBindActualPublication) {
    uint64_t id=0;ASSERT_EQ(0,Prepare(&id));
    EXPECT_EQ(-1,Run(id,11));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,BrokerStartPublication(broker,10,43,id,request.plan_sha256,Deadline()));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,BrokerStartPublication(broker,10,42,id,std::string(64,'f'),Deadline()));EXPECT_EQ(ESTALE,errno);
    ASSERT_EQ(0,Run(id));EXPECT_EQ(-1,Run(id));EXPECT_EQ(EALREADY,errno);
    PackagePublicationResult result;Completed(id,10,&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(PackagePublish::Confirmed,result.publication);Selection("complete generation");
    EXPECT_EQ(-1,Run(id));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackageBroker, PreparationOwnsItsFdsAndIdleReapingClosesThem) {
    uint64_t id=0;ASSERT_EQ(0,Prepare(&id));source.reset();store.reset();
    ASSERT_EQ(0,Run(id));
    bool removed=false;
    for(int i=0;i<5000;++i) {
        ASSERT_EQ(0,aegis_broker_owner_reap_publications(broker));
        struct stat st={};
        if(fstatat(parent.get(),"u10-s42",&st,AT_SYMLINK_NOFOLLOW)<0&&errno==ENOENT) { removed=true;break; }
        usleep(1000);
    }
    ASSERT_TRUE(removed);
    store.reset(openat(directory.get(),"store",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    source.reset(openat(directory.get(),"source",O_RDONLY|O_CLOEXEC));
    uint64_t next=0;EXPECT_EQ(-1,Prepare(&next));EXPECT_EQ(EBUSY,errno);EXPECT_EQ(0u,next);
    PackagePublicationResult result;Completed(id,10,&result);
    ASSERT_FALSE(HasFatalFailure());EXPECT_EQ(PackagePublish::Confirmed,result.publication);
    Selection("complete generation");
    ASSERT_EQ(0,Prepare(&next));EXPECT_GT(next,id);
}
TEST_F(RuntimePackageBroker, FailedStartKeepsPartialOwnershipAndBlocksAdmissionUntilStopped) {
    int baseline=Descriptors();uint64_t id=0;ASSERT_EQ(0,Prepare(&id,10,store.get()));
    EXPECT_EQ(-1,Run(id));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(-1,aegis_broker_owner_release(&broker));EXPECT_EQ(EBUSY,errno);
    aegis_broker_state state=AEGIS_BROKER_READY;
    EXPECT_EQ(-1,Apply(AEGIS_BROKER_START,10,42,&state));EXPECT_EQ(EBUSY,errno);EXPECT_EQ(AEGIS_BROKER_SEALED,state);
    ASSERT_EQ(0,Apply(AEGIS_BROKER_STOP_USER,11,0,&state));
    EXPECT_EQ(-1,aegis_broker_owner_release(&broker));EXPECT_EQ(EBUSY,errno);
    ASSERT_EQ(0,Apply(AEGIS_BROKER_STOP_USER,10,0,&state));EXPECT_EQ(AEGIS_BROKER_ABSENT,state);
    EXPECT_EQ(baseline,Descriptors());
}
TEST_F(RuntimePackageBroker, UserStopReapsPublicationWithoutCancellingOtherUsersPreparation) {
    uint64_t a=0,b=0;ASSERT_EQ(0,Prepare(&a,10));ASSERT_EQ(0,Run(a,10));ASSERT_EQ(0,Prepare(&b,11));
    aegis_broker_state state;ASSERT_EQ(0,Apply(AEGIS_BROKER_STOP_USER,10,0,&state));EXPECT_EQ(AEGIS_BROKER_ABSENT,state);
    struct stat st={};EXPECT_EQ(-1,fstatat(parent.get(),"u10-s42",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    PublicationState phase=PublicationState::Complete;PackagePublicationResult result;
    ASSERT_EQ(0,BrokerPollPublication(broker,11,42,b,request.plan_sha256,&phase,&result));
    EXPECT_EQ(PublicationState::Prepared,phase);
    EXPECT_EQ(-1,BrokerCancelPublication(broker,10,42,b,request.plan_sha256,Deadline()));EXPECT_EQ(ESTALE,errno);
    ASSERT_EQ(0,BrokerCancelPublication(broker,11,42,b,request.plan_sha256,Deadline()));
    Completed(b,11,&result);ASSERT_FALSE(HasFatalFailure());EXPECT_EQ(PackagePublish::Rejected,result.publication);
}
TEST_F(RuntimePackageBroker, HelloClearsPreparedAndRunningWorkBeforeAbsence) {
    int baseline=Descriptors();uint64_t a=0,b=0;
    ASSERT_EQ(0,Prepare(&a,10));ASSERT_EQ(0,Run(a,10));ASSERT_EQ(0,Prepare(&b,11));
    aegis_broker_state state;ASSERT_EQ(0,Apply(AEGIS_BROKER_HELLO,0,0,&state));
    EXPECT_EQ(AEGIS_BROKER_ABSENT,state);EXPECT_EQ(baseline,Descriptors());
    EXPECT_EQ(-1,Run(a,10));EXPECT_EQ(ENOENT,errno);EXPECT_EQ(-1,Run(b,11));EXPECT_EQ(ENOENT,errno);
    uint64_t c=0;ASSERT_EQ(0,Prepare(&c));EXPECT_GT(c,b);
}
TEST_F(RuntimePackageBroker, CleanupFaultRetainsItsOwnerButDoesNotAbandonOtherJobs) {
    uint64_t a=0,b=0;ASSERT_EQ(0,Prepare(&a,10));ASSERT_EQ(0,Run(a,10));
    ASSERT_EQ(0,Prepare(&b,11));ASSERT_EQ(0,Run(b,11));
    // cgroup v2 forbids rename. Fault only the first owned test group's GID;
    // its retained inode/pidfd must remain tracked while the second is reaped.
    fault_group.reset(openat(parent.get(),"u10-s42",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
    ASSERT_TRUE(fault_group.ok());struct stat original={};ASSERT_EQ(0,fstat(fault_group.get(),&original));
    ASSERT_EQ(0u,original.st_uid);ASSERT_EQ(0u,original.st_gid);
    ASSERT_EQ(0,fchown(fault_group.get(),0,1)) << strerror(errno);ownership_fault=true;
    EXPECT_EQ(-1,aegis_broker_owner_stop_all(broker,0));
    EXPECT_TRUE(errno==ETIMEDOUT||errno==EPERM) << strerror(errno);
    EXPECT_EQ(-1,aegis_broker_owner_release(&broker));EXPECT_EQ(EBUSY,errno);
    uint64_t replacement=0;EXPECT_EQ(-1,Prepare(&replacement,10));EXPECT_EQ(EBUSY,errno);
    // The global stop may already have consumed b after confirmed teardown.
    // If it retained an in-flight kill, finish that same handle, without a new job.
    for(int i=0;i<5000;++i) {
        struct stat st={};
        if(fstatat(parent.get(),"u11-s42",&st,AT_SYMLINK_NOFOLLOW)<0&&errno==ENOENT)break;
        (void)aegis_broker_owner_reap_publications(broker);
        usleep(1000);
    }
    struct stat st={};EXPECT_EQ(-1,fstatat(parent.get(),"u11-s42",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    ASSERT_EQ(0,fstatat(parent.get(),"u10-s42",&st,AT_SYMLINK_NOFOLLOW));
    EXPECT_EQ(original.st_ino,st.st_ino);EXPECT_EQ(original.st_dev,st.st_dev);EXPECT_EQ(1u,st.st_gid);
    ASSERT_EQ(0,fchown(fault_group.get(),0,0));ownership_fault=false;fault_group.reset();
    ASSERT_EQ(0,aegis_broker_owner_stop_all(broker,Deadline()));
    EXPECT_EQ(-1,fstatat(parent.get(),"u10-s42",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackageBroker, PreparationsAreBoundedAndForkCannotUseTheOwnersJobs) {
    int baseline=Descriptors();uint64_t first=0;
    for(uint32_t user=10;user<26;++user) { uint64_t id=0;ASSERT_EQ(0,Prepare(&id,user));if(user==10)first=id; }
    uint64_t extra=0;EXPECT_EQ(-1,Prepare(&extra,26));EXPECT_EQ(ENOSPC,errno);EXPECT_EQ(0u,extra);
    pid_t child=fork();ASSERT_GE(child,0);
    if(!child) {
        bool denied=aegis_broker_owner_stop_all(broker,0)==-1&&errno==EPERM;
        denied=denied&&aegis_broker_owner_reap_publications(broker)==-1&&errno==EPERM;
        PublicationState state;PackagePublicationResult result;
        denied=denied&&BrokerPollPublication(broker,10,42,first,request.plan_sha256,&state,&result)==-1&&errno==EPERM;
        denied=denied&&BrokerCancelPublication(broker,10,42,first,request.plan_sha256,0)==-1&&errno==EPERM;
        _exit(denied?0:90);
    }
    int status=0;ASSERT_EQ(child,waitpid(child,&status,0));ASSERT_TRUE(WIFEXITED(status));EXPECT_EQ(0,WEXITSTATUS(status));
    ASSERT_EQ(0,aegis_broker_owner_stop_all(broker,0));EXPECT_EQ(baseline,Descriptors());
}
} // namespace

namespace {
class RuntimePublicationReply : public ::testing::Test {
 protected:
    unique_fd sender,receiver;
    const uint64_t job=47;
    const std::string plan=std::string(64,'b');
    void SetUp() override {
        int sockets[2];ASSERT_EQ(0,socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,sockets));
        sender.reset(sockets[0]);receiver.reset(sockets[1]);
    }
    publication::Reply Valid(int result=0,int error=0) {
        publication::Reply reply={};reply.job=job;reply.result=result;reply.error=error;
        memcpy(reply.plan,plan.c_str(),65);
        if(!result) { reply.candidate.bytes=4096;memset(reply.candidate.sha256,'c',64); }
        return reply;
    }
    void Unconfirmed() {
        auto result=publication::ReceiveReply(receiver.get(),job,plan);
        EXPECT_EQ(PackagePublish::Unconfirmed,result.publication);EXPECT_EQ(EIO,result.error);
    }
    void Send(const publication::Reply& reply) {
        ASSERT_EQ(static_cast<ssize_t>(sizeof(reply)),send(sender.get(),&reply,sizeof(reply),MSG_NOSIGNAL));
    }
};
TEST_F(RuntimePublicationReply, MissingReplyIsBoundedForOpenAndClosedPeer) {
    // Test the exact production decoder in an owned child so a regression is
    // reported promptly, rather than wedging the whole test runner/VM.
    for(bool closed:{false,true}) {
        if(closed)sender.reset();
        pid_t child=fork();ASSERT_GE(child,0);
        if(child==0) {
            auto result=publication::ReceiveReply(receiver.get(),job,plan);
            _exit(result.publication==PackagePublish::Unconfirmed && result.error==EIO ? 0 : 90);
        }
        unique_fd process(syscall(SYS_pidfd_open,child,0));
        if(!process.ok()) {
            int saved=errno;kill(child,SIGKILL);waitpid(child,nullptr,0);
            FAIL()<<"pidfd_open: "<<strerror(saved);
        }
        pollfd completed={process.get(),POLLIN,0};
        int observed=poll(&completed,1,2000);
        if(observed!=1)ASSERT_EQ(0,syscall(SYS_pidfd_send_signal,process.get(),SIGKILL,nullptr,0));
        int status=0;ASSERT_EQ(child,waitpid(child,&status,0));
        EXPECT_EQ(1,observed)<<"Private completion decoder exceeded 2 seconds";
        ASSERT_TRUE(WIFEXITED(status));EXPECT_EQ(0,WEXITSTATUS(status));
    }
}
TEST_F(RuntimePublicationReply, OnlyMatchingJobPlanAndConsistentResultAreAccepted) {
    for(int value:{-1,0,1}) {
        auto reply=Valid(value,value ? ESTALE : 0);Send(reply);
        auto result=publication::ReceiveReply(receiver.get(),job,plan);
        EXPECT_EQ(static_cast<PackagePublish>(value),result.publication);EXPECT_EQ(reply.error,result.error);
    }
    for(int which=0;which<10;++which) {
        auto reply=Valid();
        switch(which) {
          case 0:reply.job++;break;
          case 1:reply.plan[0]='a';break;
          case 2:reply.plan[64]='x';break;
          case 3:reply.result=2;break;
          case 4:reply.error=-1;break;
          case 5:reply.error=4096;break;
          case 6:reply.error=EIO;break;
          case 7:reply.candidate.sha256[0]='z';break;
          case 8:reply.candidate.bytes=0;break;
          case 9:reply.result=-1;break;
        }
        Send(reply);Unconfirmed();
    }
}
TEST_F(RuntimePublicationReply, EmptyShortAndOversizedPacketsCannotConfirm) {
    auto reply=Valid();std::array<char,sizeof(reply)+1> bytes={};
    memcpy(bytes.data(),&reply,sizeof(reply));
    for(size_t size:{size_t{0},sizeof(reply)-1,sizeof(reply)+1}) {
        ASSERT_EQ(static_cast<ssize_t>(size),send(sender.get(),bytes.data(),size,MSG_NOSIGNAL));
        Unconfirmed();
    }
}
TEST_F(RuntimePublicationReply, RejectsAndClosesAncillaryReferencesIncludingTruncation) {
    unique_fd original(open("/dev/null",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(original.ok());
    int before=Descriptors();auto reply=Valid();
    for(size_t payload:{size_t{0},sizeof(reply)})for(size_t count:{size_t{1},size_t{16},size_t{32}}) {
        alignas(cmsghdr) char controls[CMSG_SPACE(32*sizeof(int))]={};
        iovec io={&reply,payload};msghdr message={};message.msg_iov=&io;message.msg_iovlen=1;
        message.msg_control=controls;message.msg_controllen=CMSG_SPACE(count*sizeof(int));
        auto* header=CMSG_FIRSTHDR(&message);header->cmsg_level=SOL_SOCKET;header->cmsg_type=SCM_RIGHTS;
        header->cmsg_len=CMSG_LEN(count*sizeof(int));
        int descriptor=original.get();
        for(size_t i=0;i<count;++i)memcpy(CMSG_DATA(header)+i*sizeof(int),&descriptor,sizeof(int));
        ASSERT_EQ(static_cast<ssize_t>(payload),sendmsg(sender.get(),&message,MSG_NOSIGNAL));
        Unconfirmed();EXPECT_EQ(before,Descriptors());EXPECT_GE(fcntl(original.get(),F_GETFD),0);
    }
}
} // namespace
