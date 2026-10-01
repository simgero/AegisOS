// Compile on aegis-build, run only in local Android QEMU. Private synthetic
// control files exercise the independent native registry parser and comparison.
#include "package_guard.h"
#include <gtest/gtest.h>
#include <json/json.h>
#include <openssl/sha.h>
#include <android-base/unique_fd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
using android::base::unique_fd;
namespace {
std::string Hash(const std::string& s) {
    unsigned char bytes[32];char text[65];SHA256(reinterpret_cast<const unsigned char*>(s.data()),s.size(),bytes);
    for(unsigned i=0;i<32;i++)snprintf(text+2*i,3,"%02x",bytes[i]);return text;
}
const std::string base="Package: base-one\nStatus: hold ok installed\nArchitecture: arm64\nVersion: 1\nDescription: retained\n folded description\n\n";
const std::string added="Package: test-app\nStatus: install ok installed\nArchitecture: all\nVersion: 2\n\n";
const std::string base_auto="Package: base-one\nArchitecture: arm64\nAuto-Installed: 1\n\n";
const std::string app_auto="Package: test-app\nArchitecture: arm64\nAuto-Installed: 1\n\n";
Json::Value SimulationPlan() {
    Json::Value r;r["jsonrpc"]="2.0";r["method"]="org.debian.apt.hooks.install.pre-prompt";
    auto& p=r["params"];p["command"]="install";
    p["search-terms"]=Json::Value(Json::arrayValue);p["unknown-packages"]=Json::Value(Json::arrayValue);
    Json::Value v;v["id"]=1;v["version"]="2";v["architecture"]="all";v["pin"]=500;
    v["origins"]=Json::Value(Json::arrayValue);
    Json::Value item;item["id"]=1;item["name"]="test-app";item["architecture"]="arm64";
    item["mode"]="install";item["automatic"]=false;
    item["versions"]["candidate"]=v;item["versions"]["install"]=v;p["packages"].append(item);return r;
}
class PackageExecutionGuard : public ::testing::Test {
 protected:
    unique_fd root;
    std::string directory;
    aegis_package_execution_request request={};
    aegis_package_guard* guard=nullptr;
    void Write(const char* path,const std::string& data) {
        unique_fd fd(openat(root.get(),path,O_CREAT|O_TRUNC|O_WRONLY|O_NOFOLLOW|O_CLOEXEC,0644));
        ASSERT_TRUE(fd.ok());ASSERT_EQ(static_cast<ssize_t>(data.size()),write(fd.get(),data.data(),data.size()));
    }
    void SetUp() override {
        ASSERT_EQ(0u,getuid());char path[]="/data/local/tmp/aegis-native-guard-XXXXXX";
        ASSERT_NE(nullptr,mkdtemp(path));directory=path;root.reset(open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(root.ok());
        for(const char* d:{"var","var/lib","var/lib/dpkg","var/lib/apt"})ASSERT_EQ(0,mkdirat(root.get(),d,0755));
        Write("var/lib/dpkg/status",base);Write("var/lib/apt/extended_states",base_auto);ASSERT_FALSE(HasFatalFailure());
        request.magic=AEGIS_PACKAGE_EXEC_MAGIC;request.version=AEGIS_PACKAGE_EXEC_VERSION;
        request.user=10;request.serial=42;request.job=1;request.kind=AEGIS_PACKAGE_ARCHIVES;request.count=1;
        strcpy(request.plan,std::string(64,'a').c_str());strcpy(request.items[0],"test-app_2_all.deb");
        auto& r=request.review;r.present=1;r.apt_state_presence=2;r.apt_state_bytes=base_auto.size();
        strcpy(r.initial_status,Hash(base).c_str());strcpy(r.initial_apt_state,Hash(base_auto).c_str());
        strcpy(r.effects[0].name,"test-app");strcpy(r.effects[0].architecture,"all");strcpy(r.effects[0].after,"2");r.effects[0].reason=2;
        ASSERT_TRUE(aegis_package_execution_valid(&request));
    }
    void Begin() { ASSERT_EQ(0,aegis_package_guard_begin(root.get(),&request,&guard))<<strerror(errno);ASSERT_NE(nullptr,guard); }
    void Finish(const std::string& status,const std::string& state,int expected) {
        Write("var/lib/dpkg/status",status);Write("var/lib/apt/extended_states",state);ASSERT_FALSE(HasFatalFailure());
        int rc=aegis_package_guard_finish(guard,root.get());
        EXPECT_EQ(expected? -1:0,rc);if(expected)EXPECT_EQ(expected,errno);
    }
    void CheckSimulation(const Json::Value& plan,int expected) {
        Json::StreamWriterBuilder b;b["indentation"]="";
        Write("simulation.json",Json::writeString(b,plan));ASSERT_FALSE(HasFatalFailure());
        unique_fd fd(openat(root.get(),"simulation.json",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(fd.ok());
        EXPECT_EQ(expected?-1:0,aegis_package_guard_simulation(guard,fd.get()));
        if(expected)EXPECT_EQ(expected,errno);
    }
    void TearDown() override {
        aegis_package_guard_free(guard);
        if(root.ok()) {
            int removed=unlinkat(root.get(),"simulation.json",0);EXPECT_TRUE(removed==0||errno==ENOENT);
            for(const char* p:{"var/lib/aegis/private-choices","var/lib/aegis/private-choices.new"}) {
                int rc=unlinkat(root.get(),p,0);EXPECT_TRUE(rc==0||errno==ENOENT);
            }
            int dir=unlinkat(root.get(),"var/lib/aegis",AT_REMOVEDIR);EXPECT_TRUE(dir==0||errno==ENOENT);
            // Only the fixed files in this exclusively owned test directory.
            for(const char* p:{"var/lib/dpkg/status","var/lib/apt/extended_states"})EXPECT_EQ(0,unlinkat(root.get(),p,0));
            for(const char* p:{"var/lib/dpkg","var/lib/apt","var/lib","var"})EXPECT_EQ(0,unlinkat(root.get(),p,AT_REMOVEDIR));
            root.reset();EXPECT_EQ(0,rmdir(directory.c_str()));
        }
    }
};
TEST_F(PackageExecutionGuard, ExactChangePreservesUnrelatedHoldAndAutomaticState) {
    Begin();ASSERT_FALSE(HasFatalFailure());Finish(base+added,base_auto+app_auto,0);
}
TEST_F(PackageExecutionGuard, SameSizeChangedInitialStatusCannotUseOldApproval) {
    auto changed=base;changed[changed.find("Version: 1")+9]='9';ASSERT_EQ(base.size(),changed.size());
    Write("var/lib/dpkg/status",changed);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis_package_guard_begin(root.get(),&request,&guard));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(nullptr,guard);
}
TEST_F(PackageExecutionGuard, DuplicateCaseInsensitiveSelectedFieldRejectsEvenWithMatchingDigest) {
    auto bad=base;bad.insert(bad.size()-1,"vErSiOn: 1\n");Write("var/lib/dpkg/status",bad);
    strcpy(request.review.initial_status,Hash(bad).c_str());
    EXPECT_EQ(-1,aegis_package_guard_begin(root.get(),&request,&guard));EXPECT_EQ(EBADMSG,errno);EXPECT_EQ(nullptr,guard);
}
TEST_F(PackageExecutionGuard, UnexpectedInstalledPackageRejects) {
    Begin();ASSERT_FALSE(HasFatalFailure());
    Finish(base+added+"Package: extra-package\nStatus: install ok installed\nArchitecture: all\nVersion: 1\n\n",base_auto+app_auto,ESTALE);
}
TEST_F(PackageExecutionGuard, ChangedUnrelatedVersionRejects) {
    Begin();ASSERT_FALSE(HasFatalFailure());auto changed=base;changed[changed.find("Version: 1")+9]='9';
    Finish(changed+added,base_auto+app_auto,ESTALE);
}
TEST_F(PackageExecutionGuard, UnapprovedHoldRemovalRejects) {
    Begin();ASSERT_FALSE(HasFatalFailure());auto changed=base;changed.replace(changed.find("hold ok"),4,"install");
    Finish(changed+added,base_auto+app_auto,ESTALE);
}
TEST_F(PackageExecutionGuard, HalfConfiguredResultRejects) {
    Begin();ASSERT_FALSE(HasFatalFailure());auto changed=added;changed.replace(changed.find("ok installed"),12,"ok half-configured");
    Finish(base+changed,base_auto+app_auto,EBADMSG);
}
TEST_F(PackageExecutionGuard, LostUnrelatedAutomaticMarkRejects) {
    Begin();ASSERT_FALSE(HasFatalFailure());Finish(base+added,app_auto,ESTALE);
}
TEST_F(PackageExecutionGuard, MissingApprovedAutomaticMarkRejects) {
    Begin();ASSERT_FALSE(HasFatalFailure());Finish(base+added,base_auto,ESTALE);
}
TEST_F(PackageExecutionGuard, DuplicateAutomaticRecordRejects) {
    Begin();ASSERT_FALSE(HasFatalFailure());Finish(base+added,base_auto+app_auto+app_auto,EBADMSG);
}
TEST_F(PackageExecutionGuard, SymlinkStatusCannotLeaveOwnedRoot) {
    ASSERT_EQ(0,unlinkat(root.get(),"var/lib/dpkg/status",0));
    ASSERT_EQ(0,symlinkat("/system/build.prop",root.get(),"var/lib/dpkg/status"));
    EXPECT_EQ(-1,aegis_package_guard_begin(root.get(),&request,&guard));EXPECT_EQ(ELOOP,errno);EXPECT_EQ(nullptr,guard);
}
TEST_F(PackageExecutionGuard, FifoStatusRejectsWithoutBlocking) {
    ASSERT_EQ(0,unlinkat(root.get(),"var/lib/dpkg/status",0));ASSERT_EQ(0,mkfifoat(root.get(),"var/lib/dpkg/status",0644));
    EXPECT_EQ(-1,aegis_package_guard_begin(root.get(),&request,&guard));EXPECT_EQ(EPERM,errno);EXPECT_EQ(nullptr,guard);
}
TEST_F(PackageExecutionGuard, ArchiveSimulationUsesEmptySearchTermsAndExactEffects) {
    Begin();ASSERT_FALSE(HasFatalFailure());CheckSimulation(SimulationPlan(),0);
}
TEST_F(PackageExecutionGuard, ArchiveSimulationRejectsUnexpectedSearchTerm) {
    Begin();ASSERT_FALSE(HasFatalFailure());auto p=SimulationPlan();
    p["params"]["search-terms"].append("/var/cache/apt/archives/test-app_2_all.deb");CheckSimulation(p,ESTALE);
}
TEST_F(PackageExecutionGuard, EmptyArchiveTermsStillRequireApprovedVersionAndArchitecture) {
    Begin();ASSERT_FALSE(HasFatalFailure());auto p=SimulationPlan();
    p["params"]["packages"][0]["versions"]["install"]["version"]="3";CheckSimulation(p,ESTALE);
    p=SimulationPlan();p["params"]["packages"][0]["versions"]["install"]["architecture"]="arm64";CheckSimulation(p,ESTALE);
}
TEST_F(PackageExecutionGuard, EmptyArchiveTermsCannotHideMissingOrAdditionalEffects) {
    Begin();ASSERT_FALSE(HasFatalFailure());auto p=SimulationPlan();
    p["params"]["packages"]=Json::Value(Json::arrayValue);CheckSimulation(p,ESTALE);
    p=SimulationPlan();auto extra=p["params"]["packages"][0];extra["id"]=2;extra["name"]="extra-package";
    p["params"]["packages"].append(extra);CheckSimulation(p,ESTALE);
}
TEST_F(PackageExecutionGuard, RemovalKeepsExactPackageSearchTerms) {
    request.kind=AEGIS_PACKAGE_REMOVE;memset(request.items[0],0,sizeof(request.items[0]));
    strcpy(request.items[0],"base-one");auto& e=request.review.effects[0];e={};
    strcpy(e.name,"base-one");strcpy(e.architecture,"arm64");strcpy(e.before,"1");e.reason=2;
    Begin();ASSERT_FALSE(HasFatalFailure());auto p=SimulationPlan();p["params"]["command"]="remove";
    p["params"]["search-terms"].append("base-one");auto& item=p["params"]["packages"][0];
    item["name"]="base-one";item["mode"]="deinstall";auto v=item["versions"]["install"];
    v["architecture"]="arm64";v["version"]="1";item["versions"].removeMember("install");item["versions"]["current"]=v;
    CheckSimulation(p,0);p["params"]["search-terms"][0]="test-app";CheckSimulation(p,ESTALE);
    p["params"]["search-terms"]=Json::Value(Json::arrayValue);CheckSimulation(p,ESTALE);
}

TEST_F(PackageExecutionGuard, MixedSimulationRetainsExplicitRemovalAndChecksBothEffects) {
    request.kind=AEGIS_PACKAGE_MIXED;request.count=2;
    request.review.effects[1]=request.review.effects[0];request.review.effects[0]={};
    strcpy(request.items[1],request.items[0]);memset(request.items[0],0,sizeof(request.items[0]));strcpy(request.items[0],"base-one");
    auto& e=request.review.effects[0];strcpy(e.name,"base-one");strcpy(e.architecture,"arm64");strcpy(e.before,"1");e.reason=2;
    Begin();ASSERT_FALSE(HasFatalFailure());auto p=SimulationPlan();p["params"]["search-terms"].append("base-one-");
    auto removed=p["params"]["packages"][0];removed["id"]=2;removed["name"]="base-one";removed["mode"]="deinstall";
    auto version=removed["versions"]["install"];version["version"]="1";version["architecture"]="arm64";
    removed["versions"]=Json::Value(Json::objectValue);removed["versions"]["current"]=version;
    p["params"]["packages"].append(removed);CheckSimulation(p,0);
    auto bad=p;bad["params"]["search-terms"][0]="base-one";CheckSimulation(bad,ESTALE);
    bad=p;bad["params"]["packages"].resize(1);CheckSimulation(bad,ESTALE);
    bad=p;bad["params"]["packages"][1]["name"]="other-base";CheckSimulation(bad,ESTALE);
    Finish(added,app_auto,0);Finish(base+added,base_auto+app_auto,ESTALE);
}
TEST_F(PackageExecutionGuard, MixedProtocolRejectsMissingReviewWrongRemovalNameAndSingleKind) {
    auto mixed=request;mixed.kind=AEGIS_PACKAGE_MIXED;mixed.count=2;
    mixed.review.effects[1]=mixed.review.effects[0];mixed.review.effects[0]={};
    strcpy(mixed.items[1],mixed.items[0]);memset(mixed.items[0],0,sizeof(mixed.items[0]));strcpy(mixed.items[0],"base-one");
    auto& e=mixed.review.effects[0];strcpy(e.name,"base-one");strcpy(e.architecture,"arm64");strcpy(e.before,"1");e.reason=2;
    ASSERT_TRUE(aegis_package_execution_valid(&mixed));EXPECT_EQ(1u,aegis_package_archive_count(&mixed));
    auto bad=mixed;bad.review={};EXPECT_FALSE(aegis_package_execution_valid(&bad));
    bad=mixed;strcpy(bad.items[0],"base-two");EXPECT_FALSE(aegis_package_execution_valid(&bad));
    bad=mixed;strcpy(bad.items[0],"base-one-");EXPECT_FALSE(aegis_package_execution_valid(&bad));
    bad=request;bad.kind=AEGIS_PACKAGE_MIXED;EXPECT_FALSE(aegis_package_execution_valid(&bad));
    bad=mixed;bad.kind=99;EXPECT_FALSE(aegis_package_execution_valid(&bad));
}

TEST_F(PackageExecutionGuard, CommitsOnlyTheReviewedInstalledChoiceAndReopensDurableBytes) {
    const char* expected="AEGIS-PRIVATE-CHOICES1\ntest-app\tall\t2\n";
    strcpy(request.review.result_choices,expected);Begin();ASSERT_FALSE(HasFatalFailure());
    Finish(base+added,base_auto+app_auto,0);ASSERT_FALSE(HasFailure());
    ASSERT_EQ(0,aegis_package_guard_commit(guard,root.get()))<<strerror(errno);
    unique_fd fd(openat(root.get(),"var/lib/aegis/private-choices",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(fd.ok());
    char data[128]={};ASSERT_EQ(ssize_t(strlen(expected)),read(fd.get(),data,sizeof(data)));EXPECT_STREQ(expected,data);
    struct stat st;ASSERT_EQ(0,fstat(fd.get(),&st));EXPECT_EQ(mode_t(S_IFREG|0600),st.st_mode);EXPECT_EQ(0u,st.st_uid);EXPECT_EQ(1u,st.st_nlink);
    EXPECT_EQ(-1,aegis_package_guard_commit(guard,root.get()));EXPECT_EQ(ESTALE,errno);
}
TEST_F(PackageExecutionGuard, ManifestCannotSelectAVersionOutsideTheCompleteFinalRegistry) {
    strcpy(request.review.result_choices,"AEGIS-PRIVATE-CHOICES1\ntest-app\tall\t9\n");
    EXPECT_EQ(-1,aegis_package_guard_begin(root.get(),&request,&guard));EXPECT_EQ(ESTALE,errno);EXPECT_EQ(nullptr,guard);
}
TEST_F(PackageExecutionGuard, PackageScriptCannotInjectAChoiceBeforeFinalCommit) {
    strcpy(request.review.result_choices,"AEGIS-PRIVATE-CHOICES1\ntest-app\tall\t2\n");Begin();ASSERT_FALSE(HasFatalFailure());
    Finish(base+added,base_auto+app_auto,0);ASSERT_FALSE(HasFailure());
    ASSERT_EQ(0,mkdirat(root.get(),"var/lib/aegis",0700));Write("var/lib/aegis/private-choices","AEGIS-PRIVATE-CHOICES1\nbase-one\tarm64\t1\n");
    EXPECT_EQ(-1,aegis_package_guard_commit(guard,root.get()));EXPECT_EQ(ESTALE,errno);
}
TEST_F(PackageExecutionGuard, ManifestSymlinkCannotReadOutsideTheCandidate) {
    ASSERT_EQ(0,mkdirat(root.get(),"var/lib/aegis",0700));
    ASSERT_EQ(0,symlinkat("/system/build.prop",root.get(),"var/lib/aegis/private-choices"));
    EXPECT_EQ(-1,aegis_package_guard_begin(root.get(),&request,&guard));EXPECT_EQ(ELOOP,errno);EXPECT_EQ(nullptr,guard);
}

}
