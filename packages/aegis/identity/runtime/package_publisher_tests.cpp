// Compile on aegis-build, execute in local QEMU. Real publication children and
// cgroups, inert filesystem payloads; no AOSP users, CE store or APT execution.
#include "package_publisher.h"
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
    bool created=false;
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
            closedir(entries);store.reset();source.reset();
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
