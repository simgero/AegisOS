// Builder: aegis-build. Execution: local Android QEMU only.
// Inert text payloads on /data/local/tmp, not apt images or personal CE stores.
#include "package_store.h"
#include <android-base/unique_fd.h>
#include <gtest/gtest.h>
#include <openssl/sha.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <memory>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using android::base::unique_fd;
using aegis::PackageGeneration;
using aegis::PackageOwner;
using aegis::PackagePublish;
using aegis::PackageStore;
namespace {
const PackageOwner shared{false,0,0};
const PackageOwner personal{true,10,42};
const std::atomic_bool proceed{false};
std::string Digest(const std::string& bytes) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),digest);
    char result[2*SHA256_DIGEST_LENGTH+1];
    for (size_t i=0;i<sizeof(digest);++i) snprintf(result+2*i,3,"%02x",digest[i]);
    return result;
}
std::string Read(int fd) {
    char text[512]; ssize_t size=pread(fd,text,sizeof(text),0);
    EXPECT_GE(size,0);return size<0?"":std::string(text,size);
}
class RuntimePackageStore : public ::testing::Test {
 protected:
    unique_fd parent,root;
    std::unique_ptr<PackageStore> store;
    unsigned files=0;
    void SetUp() override {
        ASSERT_EQ(0u,geteuid());
        char path[]="/data/local/tmp/aegis-package-store-XXXXXX";
        ASSERT_NE(nullptr,mkdtemp(path));
        parent.reset(open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        ASSERT_GE(parent.get(),0);
        ASSERT_EQ(0,mkdirat(parent.get(),"store",0700));
        root.reset(openat(parent.get(),"store",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        ASSERT_GE(root.get(),0);
        // Retain only these tiny synthetic fixtures for failure diagnosis.
        RecordProperty("fixture",path);
    }
    void Start(PackageOwner owner=shared) {
        store.reset(PackageStore::Open(root.get(),owner,true));
        ASSERT_NE(nullptr,store.get()) << strerror(errno);
    }
    unique_fd Source(const std::string& text) {
        const std::string name="source-"+std::to_string(files++);
        unique_fd fd(openat(parent.get(),name.c_str(),O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600));
        EXPECT_GE(fd.get(),0);
        EXPECT_EQ(static_cast<ssize_t>(text.size()),write(fd.get(),text.data(),text.size()));
        EXPECT_EQ(0,fsync(fd.get()));
        return fd;
    }
    PackageGeneration Image(const std::string& text,const std::string& base="") {
        return {Digest(text),base,text.size()};
    }
    void Put(const char* name,const std::string& text,mode_t mode=0400) {
        unique_fd fd(openat(root.get(),name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,mode));
        ASSERT_GE(fd.get(),0);
        ASSERT_EQ(static_cast<ssize_t>(text.size()),write(fd.get(),text.data(),text.size()));
        ASSERT_EQ(0,fsync(fd.get()));
    }
};

TEST_F(RuntimePackageStore, SelectionSwitchPreservesOldOpenImageAndSurvivesReopen) {
    Start();if(!store)return;
    auto first=Image("old complete payload");auto a=Source("old complete payload");
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(nullptr,a.get(),first,proceed));
    PackageGeneration selected;unique_fd old(store->Current(&selected));
    ASSERT_GE(old.get(),0);EXPECT_EQ(first.image_sha256,selected.image_sha256);
    auto second=Image("new complete payload");auto b=Source("new complete payload");
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(&first,b.get(),second,proceed));
    EXPECT_EQ("old complete payload",Read(old.get()));
    store.reset();store.reset(PackageStore::Open(root.get(),shared,false));
    ASSERT_NE(nullptr,store.get());unique_fd now(store->Current(&selected));
    ASSERT_GE(now.get(),0);EXPECT_EQ("new complete payload",Read(now.get()));
    EXPECT_EQ(second.image_sha256,selected.image_sha256);
    EXPECT_EQ(O_RDONLY,fcntl(now.get(),F_GETFL)&O_ACCMODE);
}

TEST_F(RuntimePackageStore, RetainedSharedReopensExactOldImageWithoutChangingSelection) {
    Start();ASSERT_TRUE(store);auto first=Image("old complete payload"),second=Image("new complete payload");
    auto a=Source("old complete payload"),b=Source("new complete payload");
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(nullptr,a.get(),first,proceed));
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(&first,b.get(),second,proceed));
    store.reset();store.reset(PackageStore::Open(root.get(),shared,false));ASSERT_TRUE(store);
    PackageGeneration observed;unique_fd old(store->RetainedShared(first.image_sha256,&observed));
    ASSERT_TRUE(old.ok())<<strerror(errno);EXPECT_EQ("old complete payload",Read(old.get()));
    EXPECT_EQ(first.bytes,observed.bytes);EXPECT_EQ(first.image_sha256,observed.image_sha256);EXPECT_TRUE(observed.shared_base_sha256.empty());
    EXPECT_EQ(O_RDONLY,fcntl(old.get(),F_GETFL)&O_ACCMODE);
    unique_fd current(store->Current(&observed));ASSERT_TRUE(current.ok());EXPECT_EQ(second.image_sha256,observed.image_sha256);
}
TEST_F(RuntimePackageStore, RetainedSharedRejectsPrivateScopeAndInvalidHashesWithoutOutput) {
    Start(personal);ASSERT_TRUE(store);auto image=Image("private",std::string(64,'a'));auto source=Source("private");
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(nullptr,source.get(),image,proceed));
    auto output=Image("sentinel");EXPECT_EQ(-1,store->RetainedShared(image.image_sha256,&output));EXPECT_EQ(EPERM,errno);
    for(const auto& hash:{std::string("../current"),std::string(64,'G'),std::string(65,'a'),std::string()}) {
        EXPECT_EQ(-1,store->RetainedShared(hash,&output));EXPECT_EQ(EINVAL,errno);
    }
    EXPECT_EQ(Digest("sentinel"),output.image_sha256);
}
TEST_F(RuntimePackageStore, RetainedSharedMissingOrAliasedImageNeverChangesOutput) {
    Start();ASSERT_TRUE(store);auto output=Image("sentinel");const auto hash=Digest("old payload");
    EXPECT_EQ(-1,store->RetainedShared(hash,&output));EXPECT_EQ(ENOENT,errno);
    auto source=Source("old payload");const auto name=hash+".image";
    ASSERT_EQ(0,symlinkat("../source-0",root.get(),name.c_str()));
    EXPECT_EQ(-1,store->RetainedShared(hash,&output));EXPECT_EQ(ELOOP,errno);
    EXPECT_EQ(Digest("sentinel"),output.image_sha256);
}
TEST_F(RuntimePackageStore, RetainedSharedRehashesHistoricalContentsAndChecksMetadata) {
    Start();ASSERT_TRUE(store);auto first=Image("old payload"),second=Image("new payload");
    auto a=Source("old payload"),b=Source("new payload");
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(nullptr,a.get(),first,proceed));
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(&first,b.get(),second,proceed));
    const auto name=first.image_sha256+".image";unique_fd altered(openat(root.get(),name.c_str(),O_WRONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(altered.ok());
    ASSERT_EQ(1,pwrite(altered.get(),"X",1,0));ASSERT_EQ(0,fsync(altered.get()));
    auto output=Image("sentinel");EXPECT_EQ(-1,store->RetainedShared(first.image_sha256,&output));EXPECT_EQ(ESTALE,errno);
    ASSERT_EQ(0,fchmod(altered.get(),0644));EXPECT_EQ(-1,store->RetainedShared(first.image_sha256,&output));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(Digest("sentinel"),output.image_sha256);
    unique_fd current(store->Current(&output));ASSERT_TRUE(current.ok());EXPECT_EQ(second.image_sha256,output.image_sha256);
}

TEST_F(RuntimePackageStore, StaleBaseMetadataCannotOverwriteNewPersonalSelection) {
    Start(personal);if(!store)return;
    auto data=Source("same bytes");auto first=Image("same bytes",std::string(64,'a'));
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(nullptr,data.get(),first,proceed));
    auto second=first;second.shared_base_sha256=std::string(64,'b');
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(&first,data.get(),second,proceed));
    auto third=first;third.shared_base_sha256=std::string(64,'c');
    EXPECT_EQ(PackagePublish::Rejected,store->Publish(&first,data.get(),third,proceed));
    EXPECT_EQ(ESTALE,errno);
    PackageGeneration actual;unique_fd image(store->Current(&actual));
    ASSERT_GE(image.get(),0);EXPECT_EQ(second.shared_base_sha256,actual.shared_base_sha256);
}

TEST_F(RuntimePackageStore, WrongScopeOrReusedUserNumberCannotOpenStore) {
    Start(personal);if(!store)return;store.reset();
    for(PackageOwner other : {shared,PackageOwner{true,11,42},PackageOwner{true,10,43}}) {
        std::unique_ptr<PackageStore> invalid(PackageStore::Open(root.get(),other,false));
        EXPECT_EQ(nullptr,invalid.get());EXPECT_EQ(ESTALE,errno);
    }
    store.reset(PackageStore::Open(root.get(),personal,false));ASSERT_NE(nullptr,store.get());
    auto source=Source("private payload");auto missingBase=Image("private payload");
    EXPECT_EQ(PackagePublish::Rejected,store->Publish(nullptr,source.get(),missingBase,proceed));
    EXPECT_EQ(EINVAL,errno);
}

TEST_F(RuntimePackageStore, CancelledOrWrongPayloadLeavesPreviousSelection) {
    Start();if(!store)return;
    auto source=Source("original");auto first=Image("original");
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(nullptr,source.get(),first,proceed));
    auto next=Source("modified");auto candidate=Image("modified");
    const std::atomic_bool cancelled{true};
    EXPECT_EQ(PackagePublish::Rejected,store->Publish(&first,next.get(),candidate,cancelled));
    EXPECT_EQ(ECANCELED,errno);
    candidate.image_sha256=std::string(64,'f');
    EXPECT_EQ(PackagePublish::Rejected,store->Publish(&first,next.get(),candidate,proceed));
    EXPECT_EQ(ESTALE,errno);
    PackageGeneration current;unique_fd image(store->Current(&current));
    ASSERT_GE(image.get(),0);EXPECT_EQ("original",Read(image.get()));
}

TEST_F(RuntimePackageStore, StoreLockAndConcurrentExpectedSelectionSerializeWriters) {
    Start();if(!store)return;
    std::unique_ptr<PackageStore> duplicate(PackageStore::Open(root.get(),shared,false));
    EXPECT_EQ(nullptr,duplicate.get());EXPECT_EQ(EWOULDBLOCK,errno);
    auto a=Source("writer A");auto b=Source("writer B");
    auto first=Image("writer A");auto second=Image("writer B");
    PackagePublish one=PackagePublish::Unconfirmed,two=one;
    std::thread left([&]{one=store->Publish(nullptr,a.get(),first,proceed);});
    std::thread right([&]{two=store->Publish(nullptr,b.get(),second,proceed);});
    left.join();right.join();
    EXPECT_TRUE((one==PackagePublish::Confirmed&&two==PackagePublish::Rejected)
            ||(two==PackagePublish::Confirmed&&one==PackagePublish::Rejected));
    PackageGeneration current;unique_fd image(store->Current(&current));
    ASSERT_GE(image.get(),0);
    EXPECT_EQ(one==PackagePublish::Confirmed?"writer A":"writer B",Read(image.get()));
}

TEST_F(RuntimePackageStore, MalformedSelectionIsNotTreatedAsAnEmptyStore) {
    Start();if(!store)return;Put("current","damaged metadata\n");
    auto source=Source("candidate");auto candidate=Image("candidate");
    EXPECT_EQ(PackagePublish::Rejected,store->Publish(nullptr,source.get(),candidate,proceed));
    EXPECT_EQ(EPROTO,errno);
    unique_fd retained(openat(root.get(),"current",O_RDONLY|O_CLOEXEC));
    EXPECT_EQ("damaged metadata\n",Read(retained.get()));
}

TEST_F(RuntimePackageStore, SymlinkAndHardlinkedSelectedImagesAreNotAccepted) {
    Start();if(!store)return;
    auto source=Source("candidate");auto candidate=Image("candidate");
    ASSERT_EQ(0,symlinkat("../outside",root.get(),"current"));
    EXPECT_EQ(PackagePublish::Rejected,store->Publish(nullptr,source.get(),candidate,proceed));
    EXPECT_EQ(ELOOP,errno);
    ASSERT_EQ(0,unlinkat(root.get(),"current",0)); // This test's own symlink only.
    ASSERT_EQ(PackagePublish::Confirmed,store->Publish(nullptr,source.get(),candidate,proceed));
    const std::string image=candidate.image_sha256+".image";
    ASSERT_EQ(0,linkat(root.get(),image.c_str(),root.get(),"inert-alias",0));
    PackageGeneration current;unique_fd invalid(store->Current(&current));
    EXPECT_EQ(-1,invalid.get());EXPECT_EQ(EPERM,errno);
}

TEST_F(RuntimePackageStore, UnselectedLeftoversAreNotActivatedOrDeletedOnReopen) {
    Start();if(!store)return;
    Put(".pending-inert-interrupted-fixture","unfinished",0600);store.reset();
    store.reset(PackageStore::Open(root.get(),shared,false));ASSERT_NE(nullptr,store.get());
    PackageGeneration current;unique_fd none(store->Current(&current));
    EXPECT_EQ(-1,none.get());EXPECT_EQ(ENOENT,errno);
    unique_fd leftover(openat(root.get(),".pending-inert-interrupted-fixture",O_RDONLY|O_CLOEXEC));
    ASSERT_GE(leftover.get(),0);EXPECT_EQ("unfinished",Read(leftover.get()));
}

TEST_F(RuntimePackageStore, ChildCannotUseAnInheritedStoreHandle) {
    Start();if(!store)return;
    pid_t child=fork();ASSERT_GE(child,0);
    if(child==0) {
        PackageGeneration value;int fd=store->Current(&value);
        _exit(fd<0&&errno==EPERM?0:1);
    }
    int status=0;ASSERT_EQ(child,waitpid(child,&status,0));
    ASSERT_TRUE(WIFEXITED(status));EXPECT_EQ(0,WEXITSTATUS(status));
    PackageGeneration current;unique_fd none(store->Current(&current));
    EXPECT_EQ(-1,none.get());EXPECT_EQ(ENOENT,errno);
}

TEST_F(RuntimePackageStore, ConfirmedSelectionSurvivesPublisherExitWithoutDestructors) {
    auto source=Source("committed before exit");auto candidate=Image("committed before exit");
    pid_t child=fork();ASSERT_GE(child,0);
    if(child==0) {
        auto owned=PackageStore::Open(root.get(),shared,true);
        _exit(owned&&owned->Publish(nullptr,source.get(),candidate,proceed)==PackagePublish::Confirmed?0:1);
    }
    int status=0;ASSERT_EQ(child,waitpid(child,&status,0));
    ASSERT_TRUE(WIFEXITED(status));ASSERT_EQ(0,WEXITSTATUS(status));
    store.reset(PackageStore::Open(root.get(),shared,false));ASSERT_NE(nullptr,store.get());
    PackageGeneration current;unique_fd selected(store->Current(&current));
    ASSERT_GE(selected.get(),0);EXPECT_EQ("committed before exit",Read(selected.get()));
}
} // namespace

TEST_F(RuntimePackageStore, EmptySharedFencePreventsFirstInitializationWithoutWritingMetadata) {
    unique_fd fence(PackageStore::LockEmptyShared(root.get()));ASSERT_TRUE(fence.ok())<<strerror(errno);
    std::unique_ptr<PackageStore> blocked(PackageStore::Open(root.get(),shared,true));EXPECT_FALSE(blocked);EXPECT_EQ(EWOULDBLOCK,errno);
    unique_fd second(PackageStore::LockEmptyShared(root.get()));EXPECT_FALSE(second.ok());EXPECT_EQ(EWOULDBLOCK,errno);
    struct stat st;EXPECT_EQ(-1,fstatat(root.get(),"lock",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,fstatat(root.get(),"owner",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    fence.reset();Start();ASSERT_TRUE(store);
}
TEST_F(RuntimePackageStore, InitializedSharedLockAndPartialMetadataNeverCountAsEmptyFactory) {
    Start();ASSERT_TRUE(store);
    unique_fd held(PackageStore::LockEmptyShared(root.get()));EXPECT_FALSE(held.ok());EXPECT_EQ(EWOULDBLOCK,errno);
    store.reset();unique_fd nonempty(PackageStore::LockEmptyShared(root.get()));EXPECT_FALSE(nonempty.ok());EXPECT_EQ(EEXIST,errno);
    ASSERT_EQ(0,unlinkat(root.get(),"lock",0));
    unique_fd damaged(PackageStore::LockEmptyShared(root.get()));EXPECT_FALSE(damaged.ok());EXPECT_EQ(EEXIST,errno);
    std::unique_ptr<PackageStore> checked(PackageStore::Open(root.get(),shared,false));EXPECT_FALSE(checked);EXPECT_EQ(ENOENT,errno);
}
