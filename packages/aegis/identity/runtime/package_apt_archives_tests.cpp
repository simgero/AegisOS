#include "package_apt_archives.h"
#include <gtest/gtest.h>
#include <android-base/unique_fd.h>
#include <openssl/sha.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace aegis;
using android::base::unique_fd;
namespace {
std::string Hash(const std::string& bytes) {
    unsigned char h[SHA256_DIGEST_LENGTH];SHA256(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),h);
    char result[65];for(unsigned i=0;i<sizeof(h);++i)snprintf(result+i*2,3,"%02x",h[i]);return result;
}
std::string Entry(std::string name="test-app",std::string version="2",std::string data="archive") {
    return "Package: "+name+"\nVersion: "+version+"\nArchitecture: all\nDescription: first\n continued description\nFilename: pool/t/"+name+"_"+version+"_all.deb\nSize: "+std::to_string(data.size())+"\nSHA256: "+Hash(data)+"\n\n";
}
class PackageAptArchives : public testing::Test {
 protected:
    std::string dir;std::vector<std::string> paths;std::vector<unique_fd> files;
    std::vector<PackageAptEffect> effects={{"test-app","all","","2",false}};
    void SetUp() override { char pattern[]="/data/local/tmp/aegis-apt-index-XXXXXX";char* p=mkdtemp(pattern);ASSERT_NE(nullptr,p);dir=p; }
    int File(const std::string& contents) {
        std::string path=dir+"/"+std::to_string(paths.size());paths.push_back(path);
        unique_fd writable(open(path.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600));if(!writable.ok())return -1;
        size_t at=0;while(at<contents.size()) { auto n=write(writable.get(),contents.data()+at,contents.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;at+=n; }
        writable.reset();files.emplace_back(open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW));return files.back().get();
    }
    PackageAptIndex Index(const std::string& text,std::string name="test-main") {
        return {{name,std::string(64,'a'),Hash(text),2000},File(text)};
    }
    void Reject(const std::string& text,int expected=EBADMSG) {
        std::vector<PackageAptArchive> result(1);result[0].repository="unchanged";
        EXPECT_EQ(-1,PackageMatchAptArchives(effects,{Index(text)},1000,&result));EXPECT_EQ(expected,errno);
        ASSERT_EQ(1u,result.size());EXPECT_EQ("unchanged",result[0].repository);
    }
    PackageResolvedPlan Context(const std::vector<PackageAptIndex>& indexes) {
        PackageResolvedPlan p;p.initial_apt_state_presence=PackageStatePresence::Absent;p.requester=10;p.serial=42;p.personal=true;p.create_store=true;
        p.requested_package="test-app";p.requested_version="2";
        p.source=p.shared={268435456,std::string(64,'a')};
        p.planner_image_sha256=std::string(64,'b');p.policy_sha256=std::string(64,'c');
        p.initial_status_sha256=std::string(64,'d');
        for(const auto& index:indexes)p.repositories.push_back(index.repository);
        return p;
    }
    void TearDown() override { files.clear();for(const auto& p:paths)EXPECT_EQ(0,unlink(p.c_str()));if(!dir.empty())EXPECT_EQ(0,rmdir(dir.c_str())); }
};
TEST_F(PackageAptArchives, ExactVersionAndArchitectureRetainAutomaticDependency) {
    effects.push_back({"test-lib","arm64","1","3",true});
    auto lib=Entry("test-lib","3");lib.replace(lib.find("Architecture: all"),17,"Architecture: arm64");
    auto index=Index(Entry("test-app","1")+Entry()+lib);
    ASSERT_EQ(7,lseek(index.fd,7,SEEK_SET));std::vector<PackageAptArchive> result;
    ASSERT_EQ(0,PackageMatchAptArchives(effects,{index},1000,&result))<<strerror(errno);
    ASSERT_EQ(2u,result.size());EXPECT_EQ("2",result[0].effect.after_version);
    EXPECT_FALSE(result[0].effect.automatic);EXPECT_TRUE(result[1].effect.automatic);
    EXPECT_EQ("pool/t/test-app_2_all.deb",result[0].filename);EXPECT_EQ(Hash("archive"),result[0].archive.sha256);
    EXPECT_EQ(7,lseek(index.fd,0,SEEK_CUR));
}
TEST_F(PackageAptArchives, AuthenticatedEmptyIndexDoesNotHideAnotherSourceOrInventPackage) {
    auto empty=Index("","empty"),full=Index(Entry(),"full");std::vector<PackageAptArchive> result;
    ASSERT_EQ(0,PackageMatchAptArchives(effects,{empty,full},1000,&result));
    ASSERT_EQ(1u,result.size());EXPECT_EQ("full",result[0].repository);
    EXPECT_EQ(-1,PackageMatchAptArchives(effects,{empty},1000,&result));EXPECT_EQ(ENOENT,errno);
    empty.repository.index_sha256=Hash("changed");
    EXPECT_EQ(-1,PackageMatchAptArchives(effects,{empty,full},1000,&result));EXPECT_EQ(EBADMSG,errno);
    EXPECT_EQ("full",result[0].repository);
}
TEST_F(PackageAptArchives, WholeIndexHashMustMatchAuthenticatedReceipt) {
    auto index=Index(Entry());index.repository.index_sha256=Hash(Entry()+"\n");
    std::vector<PackageAptArchive> result;
    EXPECT_EQ(-1,PackageMatchAptArchives(effects,{index},1000,&result));EXPECT_EQ(EBADMSG,errno);EXPECT_TRUE(result.empty());
}
TEST_F(PackageAptArchives, MissingExactVersionNeverSubstitutesRepositoryCandidate) {
    Reject(Entry("test-app","3"),ENOENT);
    auto other=Entry();other.replace(other.find("Architecture: all"),17,"Architecture: arm64");Reject(other,ENOENT);
}
TEST_F(PackageAptArchives, DuplicateCaseInsensitiveFieldsAndFoldedSimpleFieldsFail) {
    auto e=Entry();e.insert(e.find("Version:"),"PACKAGE: test-app\n");Reject(e);
    e=Entry();e.insert(e.find("Architecture:")," unexpected continuation\n");Reject(e);
    e=Entry();e.insert(e.find("Filename:"),"Description: duplicate\n");Reject(e);
}
TEST_F(PackageAptArchives, ArchiveNamesCannotEscapeRepositoryOrInjectUrls) {
    for(const char* path:{"/tmp/p.deb","../p.deb","pool/../p.deb","https://evil/p.deb","pool/%2e%2e/p.deb","pool//p.deb","pool/p.deb?x","pool/p;id.deb"}) {
        auto e=Entry();auto a=e.find("Filename: ")+10,z=e.find('\n',a);e.replace(a,z-a,path);Reject(e);
    }
}
TEST_F(PackageAptArchives, ArchiveSizeAndStrongHashAreMandatoryAndBounded) {
    for(const char* size:{"0","07","+7","7x","2147483649","18446744073709551616"}) {
        auto e=Entry();auto a=e.find("Size: ")+6,z=e.find('\n',a);e.replace(a,z-a,size);Reject(e);
    }
    auto e=Entry();auto a=e.find("SHA256:");e.replace(a,7,"SHA1:  ");Reject(e);
}
TEST_F(PackageAptArchives, DuplicateSelectedStanzaFailsEvenWithIdenticalContent) { Reject(Entry()+Entry(),EEXIST); }
TEST_F(PackageAptArchives, CrossSourceContentMustAgreeAndSelectionIsDeterministic) {
    auto a=Index(Entry(),"first"),b=Index(Entry(),"second");std::vector<PackageAptArchive> result;
    ASSERT_EQ(0,PackageMatchAptArchives(effects,{a,b},1000,&result));EXPECT_EQ("first",result[0].repository);
    auto different=Index(Entry("test-app","2","different"),"second");
    EXPECT_EQ(-1,PackageMatchAptArchives(effects,{a,different},1000,&result));EXPECT_EQ(EEXIST,errno);EXPECT_EQ("first",result[0].repository);
}
TEST_F(PackageAptArchives, IndexDescriptorsMustBeReadonlyOrdinaryAndBounded) {
    auto index=Index(Entry());unique_fd writeable(open(paths.back().c_str(),O_RDWR|O_CLOEXEC));index.fd=writeable.get();
    std::vector<PackageAptArchive> result;EXPECT_EQ(-1,PackageMatchAptArchives(effects,{index},1000,&result));EXPECT_EQ(EINVAL,errno);
    ASSERT_EQ(0,ftruncate(writeable.get(),(off_t{256}<<20)+1));index.fd=files.back().get();
    EXPECT_EQ(-1,PackageMatchAptArchives(effects,{index},1000,&result));EXPECT_EQ(EINVAL,errno);
    unique_fd directory(open(dir.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));index.fd=directory.get();
    EXPECT_EQ(-1,PackageMatchAptArchives(effects,{index},1000,&result));EXPECT_EQ(EINVAL,errno);
}
TEST_F(PackageAptArchives, ControlFramingAndResourceBoundsFailWithoutPartialOutput) {
    Reject(std::string("Package: test-app\0\n",19));Reject("#comment\n"+Entry());
    Reject("Description: "+std::string(65537,'x')+"\n"+Entry(),EFBIG);
    std::string stanza="Description: header\n";for(unsigned i=0;i<17;++i)stanza+=" "+std::string(64000,'x')+"\n";
    Reject(stanza+Entry(),EFBIG);
    std::string fields;for(unsigned i=0;i<257;++i)fields+="Field"+std::to_string(i)+": x\n";Reject(fields+Entry());
}
TEST_F(PackageAptArchives, ExpirySourceIdentityAndCanonicalEffectsAreRequired) {
    auto index=Index(Entry());std::vector<PackageAptArchive> result;
    EXPECT_EQ(-1,PackageMatchAptArchives(effects,{index},2000,&result));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,PackageMatchAptArchives(effects,{index,index},1000,&result));EXPECT_EQ(EINVAL,errno);
    auto duplicate=effects;duplicate.push_back(effects[0]);EXPECT_EQ(-1,PackageMatchAptArchives(duplicate,{index},1000,&result));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(-1,PackageMatchAptArchives({}, {},1000,&result));EXPECT_EQ(EALREADY,errno);
}
TEST_F(PackageAptArchives, RemovalPreservesEffectWithoutInventingArchive) {
    effects[0].before_version="2";effects[0].after_version.clear();effects[0].automatic=true;std::vector<PackageAptArchive> result;
    ASSERT_EQ(0,PackageMatchAptArchives(effects,{},1000,&result));ASSERT_EQ(1u,result.size());
    EXPECT_EQ("2",result[0].effect.before_version);EXPECT_TRUE(result[0].effect.automatic);
    EXPECT_TRUE(result[0].filename.empty());EXPECT_EQ(0u,result[0].archive.bytes);
    EXPECT_EQ(-1,PackageVerifyAptArchive(result[0],File("archive")));EXPECT_EQ(EINVAL,errno);
}
TEST_F(PackageAptArchives, PinnedArchiveHashRejectsSameSizeMutationAndTruncation) {
    std::vector<PackageAptArchive> result;ASSERT_EQ(0,PackageMatchAptArchives(effects,{Index(Entry())},1000,&result));
    int fd=File("archive");ASSERT_EQ(3,lseek(fd,3,SEEK_SET));ASSERT_EQ(0,PackageVerifyAptArchive(result[0],fd));EXPECT_EQ(3,lseek(fd,0,SEEK_CUR));
    EXPECT_EQ(-1,PackageVerifyAptArchive(result[0],File("archivX")));EXPECT_EQ(EBADMSG,errno);
    EXPECT_EQ(-1,PackageVerifyAptArchive(result[0],File("archiv")));EXPECT_EQ(EBADMSG,errno);
}

TEST_F(PackageAptArchives, BindingJoinsAllArchiveBytesAndDependencyMarks) {
    effects.push_back({"test-lib","all","","2",true});
    auto index=Index(Entry()+Entry("test-lib","2","library"));std::vector<PackageAptArchive> result;
    ASSERT_EQ(0,PackageMatchAptArchives(effects,{index},1000,&result));
    int app=File("archive"),lib=File("library");ASSERT_EQ(3,lseek(app,3,SEEK_SET));
    auto context=Context({index});PackageBoundPlan bound;
    ASSERT_EQ(0,PackageBindAptArchives(context,result,{app,lib},1000,&bound));
    EXPECT_EQ(3,lseek(app,0,SEEK_CUR));EXPECT_EQ(10u,bound.publication.requester);
    ASSERT_EQ(2u,bound.reviewed.changes.size());
    EXPECT_EQ(PackageInstallReason::Manual,bound.reviewed.changes[0].reason);
    EXPECT_EQ(PackageInstallReason::Automatic,bound.reviewed.changes[1].reason);
    EXPECT_EQ(Hash("archive"),bound.preparation.archives[0].sha256);
    EXPECT_EQ(Hash("library"),bound.preparation.archives[1].sha256);
    auto changed=result;changed[1].effect.automatic=false;PackageBoundPlan other;
    ASSERT_EQ(0,PackageBindAptArchives(context,changed,{app,lib},1000,&other));
    EXPECT_NE(bound.publication.plan_sha256,other.publication.plan_sha256);
}
TEST_F(PackageAptArchives, BindingRejectsConflictingEffectsAndDescriptorCount) {
    auto index=Index(Entry());std::vector<PackageAptArchive> result;
    ASSERT_EQ(0,PackageMatchAptArchives(effects,{index},1000,&result));
    auto context=Context({index});int fd=File("archive");PackageBoundPlan bound;
    bound.publication.plan_sha256="unchanged";
    EXPECT_EQ(-1,PackageBindAptArchives(context,result,{},1000,&bound));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(-1,PackageBindAptArchives(context,result,{fd,fd},1000,&bound));EXPECT_EQ(EINVAL,errno);
    context.changes.resize(1);
    EXPECT_EQ(-1,PackageBindAptArchives(context,result,{fd},1000,&bound));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ("unchanged",bound.publication.plan_sha256);
}
TEST_F(PackageAptArchives, BindingRejectsChangedOrReorderedFilesWithoutPartialOutput) {
    effects.push_back({"test-lib","all","","2",true});
    auto index=Index(Entry()+Entry("test-lib","2","library"));std::vector<PackageAptArchive> result;
    ASSERT_EQ(0,PackageMatchAptArchives(effects,{index},1000,&result));
    auto context=Context({index});int app=File("archive"),lib=File("library"),changed=File("archivX");
    PackageBoundPlan bound;bound.publication.plan_sha256="unchanged";
    EXPECT_EQ(-1,PackageBindAptArchives(context,result,{changed,lib},1000,&bound));EXPECT_EQ(EBADMSG,errno);
    EXPECT_EQ(-1,PackageBindAptArchives(context,result,{lib,app},1000,&bound));EXPECT_EQ(EBADMSG,errno);
    EXPECT_EQ("unchanged",bound.publication.plan_sha256);EXPECT_TRUE(bound.reviewed.changes.empty());
    EXPECT_EQ(7,lseek(app,0,SEEK_END)); // Caller still owns every FD after rejection.
}
TEST_F(PackageAptArchives, BindingChecksExpiryBeforeTouchingAnyArchive) {
    auto index=Index(Entry());std::vector<PackageAptArchive> result;
    ASSERT_EQ(0,PackageMatchAptArchives(effects,{index},1000,&result));
    PackageBoundPlan bound;bound.publication.plan_sha256="unchanged";
    EXPECT_EQ(-1,PackageBindAptArchives(Context({index}),result,{INT32_MAX},2000,&bound));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ("unchanged",bound.publication.plan_sha256);
}
TEST_F(PackageAptArchives, RemovalBindingRetainsReasonButRejectsArchiveAuthority) {
    effects[0].before_version="2";effects[0].after_version.clear();effects[0].automatic=true;
    std::vector<PackageAptArchive> result;ASSERT_EQ(0,PackageMatchAptArchives(effects,{},1000,&result));
    auto context=Context({});context.action=PackageAction::Remove;context.requested_version.clear();
    PackageBoundPlan bound;ASSERT_EQ(0,PackageBindAptArchives(context,result,{-1},1000,&bound));
    EXPECT_EQ(PackageInstallReason::Automatic,bound.reviewed.changes[0].reason);EXPECT_TRUE(bound.publication.personal);
    EXPECT_TRUE(bound.preparation.archives.empty());EXPECT_EQ(10u,bound.publication.requester);
    EXPECT_EQ(-1,PackageBindAptArchives(context,result,{File("archive")},1000,&bound));EXPECT_EQ(EINVAL,errno);
    result[0].filename="pool/x.deb";
    EXPECT_EQ(-1,PackageBindAptArchives(context,result,{-1},1000,&bound));EXPECT_EQ(EINVAL,errno);
}
}
