#include "package_reconciliation.h"
#include "package_registry.h"
#include "package_resolver.h"
#include "package_planning_protocol.h"
#include <gtest/gtest.h>
#include <errno.h>
using namespace aegis;
namespace {
std::string S(const std::string& name,const std::string& version,const std::string& want="install") {
    return "Package: "+name+"\nStatus: "+want+" ok installed\nArchitecture: all\nVersion: "+version+"\n\n";
}
std::string A(const std::string& name) { return "Package: "+name+"\nArchitecture: all\nAuto-Installed: 1\n\n"; }
const std::string Header="AEGIS-PRIVATE-CHOICES1\n";
PackageReconciliationInput Inputs() {
    PackageReconciliationInput i;i.personal_status=i.previous_status=S("core","1")+S("app","1")+S("lib","1");
    i.current_status=S("core","1")+S("app","2")+S("lib","2");
    i.personal_automatic=i.previous_automatic=i.current_automatic=A("lib");i.private_choices=Header;return i;
}
TEST(PackageReconciliationGoals, SharedManualRootsAndExactPrivateChoicesLeaveDependenciesToApt) {
    auto i=Inputs();i.private_choices+="app\tall\t1\n";PackageReconciliationGoals g;
    ASSERT_EQ(0,PackageReconciliationDerive(i,&g));
    EXPECT_EQ((std::vector<std::string>{"app=1","core=1"}),g.arguments);
    EXPECT_FALSE(g.roots.count("lib"));EXPECT_EQ(A("lib"),g.solver_automatic);
}
TEST(PackageReconciliationGoals, SharedUpdateAndRemovalAlterOnlyTheSolverProjection) {
    auto i=Inputs();auto original=i.personal_automatic;PackageReconciliationGoals g;
    ASSERT_EQ(0,PackageReconciliationDerive(i,&g));EXPECT_EQ("2",g.roots.at("app").version);
    i.current_status=S("core","1");i.current_automatic.clear();
    ASSERT_EQ(0,PackageReconciliationDerive(i,&g));EXPECT_EQ((std::vector<std::string>{"core=1"}),g.arguments);
    EXPECT_EQ(A("app")+A("lib"),g.solver_automatic);EXPECT_EQ(original,i.personal_automatic);
}
TEST(PackageReconciliationGoals, MissingManifestAndUnrecordedPersonalRootNeverGuessIntent) {
    auto i=Inputs();PackageReconciliationGoals g;g.arguments={"unchanged"};i.private_choices.clear();
    EXPECT_EQ(-1,PackageReconciliationDerive(i,&g));EXPECT_EQ(ENODATA,errno);
    i.private_choices=Header;i.personal_status+=S("secret-app","1");
    EXPECT_EQ(-1,PackageReconciliationDerive(i,&g));EXPECT_EQ(ENODATA,errno);
    EXPECT_EQ((std::vector<std::string>{"unchanged"}),g.arguments);
}
TEST(PackageReconciliationGoals, PrivateVersionArchitectureAndPresenceMustMatchInitialRegistry) {
    for(const char* choice:{"app\tall\t9\n","app\tarm64\t1\n","absent\tall\t1\n"}) {
        auto i=Inputs();i.private_choices+=choice;PackageReconciliationGoals g;
        EXPECT_EQ(-1,PackageReconciliationDerive(i,&g));EXPECT_EQ(ESTALE,errno);
    }
}
TEST(PackageReconciliationGoals, MalformedHistoricalRegistryIsNotIgnored) {
    auto i=Inputs();i.previous_status+="invalid";PackageReconciliationGoals g;
    EXPECT_EQ(-1,PackageReconciliationDerive(i,&g));EXPECT_EQ(EBADMSG,errno);
    i=Inputs();i.current_automatic="Package: lib\nAuto-Installed: 7\n\n";
    EXPECT_EQ(-1,PackageReconciliationDerive(i,&g));EXPECT_EQ(EBADMSG,errno);
}
TEST(PackageReconciliationGoals, RootArgumentsHaveAnIndependentBoundBeyondTheEffectLimit) {
    auto i=Inputs();i.current_status.clear();i.current_automatic.clear();PackageReconciliationGoals g;
    for(size_t n=0;n<kPackageReconciliationRoots;++n)i.current_status+=S("root"+std::to_string(n),"1");
    ASSERT_EQ(0,PackageReconciliationDerive(i,&g));EXPECT_EQ(kPackageReconciliationRoots,g.arguments.size());
    i.current_status+=S("one-more","1");EXPECT_EQ(-1,PackageReconciliationDerive(i,&g));EXPECT_EQ(E2BIG,errno);
    EXPECT_EQ(kPackageReconciliationRoots,g.arguments.size());
}
TEST(PackageReconciliationGoals, ValidatesAllExactRootsIncludingUntouchedPrivateVersions) {
    auto i=Inputs();PackageReconciliationGoals g;ASSERT_EQ(0,PackageReconciliationDerive(i,&g));
    std::vector<PackageAptEffect> changes={{"app","all","1","2",false},{"lib","all","1","2",true}};
    EXPECT_EQ(0,PackageReconciliationCheckEffects(i,g,changes));
    EXPECT_EQ(-1,PackageReconciliationCheckEffects(i,g,{}));EXPECT_EQ(EDEADLK,errno);
    i.private_choices+="app\tall\t1\n";ASSERT_EQ(0,PackageReconciliationDerive(i,&g));
    EXPECT_EQ(0,PackageReconciliationCheckEffects(i,g,{}));
    EXPECT_EQ(-1,PackageReconciliationCheckEffects(i,g,changes));EXPECT_EQ(EDEADLK,errno);
}
TEST(PackageReconciliationGoals, SolverCanRetainAnObsoleteRootWhenItIsStillADependency) {
    auto i=Inputs();i.current_status=S("core","1");i.current_automatic.clear();PackageReconciliationGoals g;
    ASSERT_EQ(0,PackageReconciliationDerive(i,&g));
    EXPECT_EQ(0,PackageReconciliationCheckEffects(i,g,{}));
    EXPECT_EQ(0,PackageReconciliationCheckEffects(i,g,{{"app","all","1","",true},{"lib","all","1","",true}}));
}
TEST(PackageReconciliationGoals, ChangedGoalsOldVersionsAndHeldPackagesAreRejected) {
    auto i=Inputs();PackageReconciliationGoals g;ASSERT_EQ(0,PackageReconciliationDerive(i,&g));
    auto changed=g;changed.arguments.push_back("--allow-unauthenticated");
    EXPECT_EQ(-1,PackageReconciliationCheckEffects(i,changed,{}));EXPECT_EQ(ESTALE,errno);
    std::vector<PackageAptEffect> effects={{"app","all","9","2",false}};
    EXPECT_EQ(-1,PackageReconciliationCheckEffects(i,g,effects));EXPECT_EQ(ESTALE,errno);
    i.personal_status=S("core","1")+S("app","1","hold")+S("lib","1");
    ASSERT_EQ(0,PackageReconciliationDerive(i,&g));effects[0].before_version="1";
    EXPECT_EQ(-1,PackageReconciliationCheckEffects(i,g,effects));EXPECT_EQ(EDEADLK,errno);
}
TEST(PackageReconciliationGoals, InternalEvidenceCannotBeMistakenForAnOrdinaryApprovedUpdate) {
    PackageResolvedPlan p;p.reconciliation=true;aegis_planning_evidence wire={};
    ASSERT_EQ(0,planning_wire::Encode(p,&wire));PackageResolvedPlan decoded;
    ASSERT_EQ(0,planning_wire::Decode(wire,&decoded));ASSERT_TRUE(decoded.reconciliation);
    PackageBoundPlan output;EXPECT_EQ(-1,PackageBindResolvedPlan(decoded,1,&output));EXPECT_EQ(EOPNOTSUPP,errno);
    PackageResolverRequest request;request.reconciliation=true;request.package="app";
    EXPECT_EQ(-1,PackageResolverCheck(request));request.package.clear();request.action=PackageAction::Update;
    EXPECT_EQ(0,PackageResolverCheck(request));
}
}
