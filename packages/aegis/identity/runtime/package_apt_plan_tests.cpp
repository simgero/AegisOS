#include "package_apt_plan.h"
#include <json/json.h>
#include <gtest/gtest.h>
#include <errno.h>
using namespace aegis;
namespace {
Json::Value Version(const char* text) {
    Json::Value v(Json::objectValue);v["id"]=1;v["version"]=text;v["architecture"]="all";v["pin"]=500;
    v["origins"]=Json::Value(Json::arrayValue);return v;
}
Json::Value Plan() {
    Json::Value r(Json::objectValue);r["jsonrpc"]="2.0";r["method"]="org.debian.apt.hooks.install.pre-prompt";
    auto& p=r["params"];p["command"]="install";p["search-terms"].append("test-app=2");
    p["unknown-packages"]=Json::Value(Json::arrayValue);
    Json::Value item;item["id"]=1;item["name"]="test-app";item["architecture"]="arm64";
    item["mode"]="install";item["automatic"]=false;
    item["versions"]["candidate"]=Version("3");item["versions"]["install"]=Version("2");
    p["packages"].append(item);return r;
}
std::string Encode(const Json::Value& v) { Json::StreamWriterBuilder b;b["indentation"]="";return Json::writeString(b,v); }
void Reject(const std::string& json,int error=EBADMSG) {
    std::vector<PackageAptEffect> out={{"untouched","all","1","2",true}};
    EXPECT_EQ(-1,PackageReadAptPlan(json,PackageAction::Install,"test-app","2",&out));EXPECT_EQ(error,errno);
    ASSERT_EQ(1u,out.size());EXPECT_EQ("untouched",out[0].name);EXPECT_TRUE(out[0].automatic);
}
void Reject(const Json::Value& json,int error=EBADMSG) { Reject(Encode(json),error); }
}
TEST(PackageAptPlan, UsesChosenInstallVersionInsteadOfRepositoryCandidate) {
    std::vector<PackageAptEffect> result;
    ASSERT_EQ(0,PackageReadAptPlan(Encode(Plan()),PackageAction::Install,"test-app","2",&result));
    ASSERT_EQ(1u,result.size());EXPECT_EQ("2",result[0].after_version);EXPECT_TRUE(result[0].before_version.empty());
    EXPECT_EQ("all",result[0].architecture);EXPECT_FALSE(result[0].automatic);
}
TEST(PackageAptPlan, KeepsExactDependencyAndAutomaticStateInCanonicalOrder) {
    auto p=Plan();auto dep=p["params"]["packages"][0];dep["id"]=2;dep["name"]="dependency";dep["automatic"]=true;
    dep["versions"]["install"]=Version("1:5.2~rc1-3");p["params"]["packages"].append(dep);
    std::vector<PackageAptEffect> result;
    ASSERT_EQ(0,PackageReadAptPlan(Encode(p),PackageAction::Install,"test-app","2",&result));
    ASSERT_EQ(2u,result.size());EXPECT_EQ("dependency",result[0].name);EXPECT_TRUE(result[0].automatic);
    EXPECT_EQ("1:5.2~rc1-3",result[0].after_version);EXPECT_EQ("test-app",result[1].name);
}
TEST(PackageAptPlan, PreservesUpgradeDowngradeAndDependentRemovalEffects) {
    for(const char* mode:{"upgrade","downgrade"}) {
        auto p=Plan();auto& item=p["params"]["packages"][0];item["mode"]=mode;
        item["versions"]["current"]=Version(mode[0]=='u'?"1":"3");
        std::vector<PackageAptEffect> result;
        ASSERT_EQ(0,PackageReadAptPlan(Encode(p),PackageAction::Install,"test-app","2",&result));
        EXPECT_EQ(mode[0]=='u'?"1":"3",result[0].before_version);EXPECT_EQ("2",result[0].after_version);
    }
    auto p=Plan();auto dep=p["params"]["packages"][0];dep["id"]=2;dep["name"]="removed-dependency";
    dep["mode"]="deinstall";dep["versions"].removeMember("install");dep["versions"]["current"]=Version("7");
    p["params"]["packages"].append(dep);std::vector<PackageAptEffect> result;
    ASSERT_EQ(0,PackageReadAptPlan(Encode(p),PackageAction::Install,"test-app","2",&result));
    ASSERT_EQ(2u,result.size());EXPECT_EQ("removed-dependency",result[0].name);EXPECT_TRUE(result[0].after_version.empty());
}
TEST(PackageAptPlan, NoOpIsAnEmptyResolutionNotAChangedGeneration) {
    auto p=Plan();p["params"]["packages"]=Json::Value(Json::arrayValue);std::vector<PackageAptEffect> result;
    ASSERT_EQ(0,PackageReadAptPlan(Encode(p),PackageAction::Install,"test-app","2",&result));EXPECT_TRUE(result.empty());
}
TEST(PackageAptPlan, RejectsSameVersionEffectsIncludingArchitectureReplacement) {
    // The real APT no-op hook has an empty packages array. An equal-version
    // effect is not authority to bypass archive verification or package scripts.
    auto p=Plan();p["params"]["packages"][0]["versions"]["current"]=Version("2");
    Reject(p);
    auto bad=p;bad["params"]["packages"][0]["mode"]="reinstall";Reject(bad,EOPNOTSUPP);
    bad=p;bad["params"]["packages"][0]["mode"]="upgrade";Reject(bad);
    bad["params"]["packages"][0]["versions"]["current"]["architecture"]="arm64";Reject(bad);
}

TEST(PackageAptPlan, RejectsChangedRequestVersionUnknownPackagesAndProtocolPhase) {
    auto p=Plan();p["params"]["search-terms"][0]="test-app=3";Reject(p,ESTALE);
    p=Plan();p["params"]["packages"][0]["versions"]["install"]=Version("3");Reject(p,ESTALE);
    p=Plan();p["params"]["unknown-packages"].append("missing");Reject(p);
    p=Plan();p["method"]="org.debian.apt.hooks.install.post";Reject(p);
    p=Plan();p["params"]["command"]="purge";Reject(p);
}
TEST(PackageAptPlan, RejectsDuplicateKeysTrailingDataCommentsAndExcessNesting) {
    auto text=Encode(Plan());Reject(text+"{}");Reject(text+"/*comment*/",EINVAL);
    text.insert(1,"\"jsonrpc\":\"2.0\",");Reject(text);
    Reject(std::string(100,'[')+std::string(100,']'),EINVAL);
    Reject(std::string(262145,' '),EINVAL);Reject(std::string("{}\0",3),EINVAL);
}
TEST(PackageAptPlan, RejectsMissingFieldsDuplicateIdentitiesAndForeignArchitecture) {
    auto p=Plan();auto duplicate=p["params"]["packages"][0];p["params"]["packages"].append(duplicate);Reject(p);
    p=Plan();p["params"]["packages"][0]["architecture"]="amd64";Reject(p);
    p=Plan();p["params"]["packages"][0]["versions"].removeMember("install");Reject(p);
    p=Plan();p["params"]["packages"][0].removeMember("automatic");Reject(p);
    p=Plan();p["params"]["packages"][0]["versions"]["install"]["version"]="2;id";Reject(p);
    p=Plan();p["params"]["packages"][0]["name"]="../test-app";Reject(p);
}
TEST(PackageAptPlan, RefusesUnsupportedOrContradictoryEffectsWithoutOmittingThem) {
    for(const char* mode:{"purge","reinstall"}) { auto p=Plan();p["params"]["packages"][0]["mode"]=mode;Reject(p,EOPNOTSUPP); }
    auto p=Plan();p["params"]["packages"][0]["mode"]="upgrade";Reject(p);
    p=Plan();p["params"]["packages"][0]["versions"]["current"]=Version("1");Reject(p);
    p=Plan();p["params"]["packages"][0]["mode"]="deinstall";Reject(p);
}
TEST(PackageAptPlan, RequiresExplicitRemoveOrUpgradeCommandAndSingleOriginalRequest) {
    auto p=Plan();p["params"]["command"]="remove";p["params"]["search-terms"][0]="test-app";
    auto& item=p["params"]["packages"][0];item["mode"]="deinstall";item["versions"].removeMember("install");
    item["versions"]["current"]=Version("1");std::vector<PackageAptEffect> result;
    ASSERT_EQ(0,PackageReadAptPlan(Encode(p),PackageAction::Remove,"test-app","",&result));
    EXPECT_TRUE(result[0].after_version.empty());EXPECT_EQ("1",result[0].before_version);
    p=Plan();p["params"]["command"]="upgrade";p["params"]["search-terms"]=Json::Value(Json::arrayValue);
    p["params"]["packages"][0]["mode"]="upgrade";p["params"]["packages"][0]["versions"]["current"]=Version("1");
    ASSERT_EQ(0,PackageReadAptPlan(Encode(p),PackageAction::Update,"","",&result));
    p=Plan();p["params"]["search-terms"].append("another-package");Reject(p,ESTALE);
}
TEST(PackageAptPlan, BoundsEffectOriginAndNumericRecords) {
    auto p=Plan();for(unsigned i=0;i<64;i++)p["params"]["packages"].append(p["params"]["packages"][0]);Reject(p);
    p=Plan();p["params"]["packages"][0]["id"]=1.5;Reject(p);
    p=Plan();p["params"]["packages"][0]["automatic"]="true";Reject(p);
    p=Plan();p["params"]["packages"][0]["versions"]["install"]["origins"].append(Json::Value("unknown"));Reject(p);
    p=Plan();p["unexpected"]="x";Reject(p);
}
