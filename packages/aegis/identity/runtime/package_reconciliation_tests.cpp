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
TEST(PackageReconciliationGoals, PreferencesPreferPublishedVersionsAndKeepExistingPrivateAlternatives) {
    auto i=Inputs();i.private_choices+="app\tall\t1\n";PackageReconciliationGoals g;
    ASSERT_EQ(0,PackageReconciliationDerive(i,&g));
    EXPECT_EQ("Package: *\nPin: release *\nPin-Priority: -1\n\n"
        "Package: app\nPin: version 1\nPin-Priority: 2001\n\n"
        "Package: core\nPin: version 1\nPin-Priority: 2001\n\n"
        "Package: app\nPin: version 2\nPin-Priority: 1001\n\n"
        "Package: lib\nPin: version 2\nPin-Priority: 1001\n\n"
        "Package: lib\nPin: version 1\nPin-Priority: 100\n\n",g.solver_preferences);
    EXPECT_EQ(0,PackageReconciliationCheckEffects(i,g,{}));
    auto changed=g;changed.solver_preferences.clear();
    EXPECT_EQ(-1,PackageReconciliationCheckEffects(i,changed,{}));EXPECT_EQ(ESTALE,errno);
}
TEST(PackageReconciliationGoals, RejectsVersionsOutsidePublishedCommonAndExistingPersonalRegistries) {
    auto i=Inputs();PackageReconciliationGoals g;ASSERT_EQ(0,PackageReconciliationDerive(i,&g));
    const std::vector<PackageAptEffect> valid={{"app","all","1","2",false},{"lib","all","1","2",true}};
    EXPECT_EQ(0,PackageReconciliationCheckEffects(i,g,valid));
    auto newer=valid;newer[1].after_version="3";
    EXPECT_EQ(-1,PackageReconciliationCheckEffects(i,g,newer));EXPECT_EQ(EPERM,errno);
    auto unknown=valid;unknown.push_back({"new-library","all","","1",true});
    EXPECT_EQ(-1,PackageReconciliationCheckEffects(i,g,unknown));EXPECT_EQ(EPERM,errno);
    auto changed_arch=valid;changed_arch[1].architecture="arm64";
    EXPECT_EQ(-1,PackageReconciliationCheckEffects(i,g,changed_arch));EXPECT_EQ(ESTALE,errno);
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
PackageResolvedPlan ReconciliationPlan() {
    PackageResolvedPlan p;p.requester=10;p.serial=42;p.personal=p.has_previous=p.reconciliation=true;
    p.action=PackageAction::Update;p.source={268435456,std::string(64,'a')};
    p.shared={268435456,std::string(64,'b')};p.previous_shared={268435456,std::string(64,'c')};
    p.previous={p.source.sha256,p.previous_shared.sha256,p.source.bytes};p.planner_image_sha256=std::string(64,'d');
    p.policy_sha256=std::string(64,'e');p.initial_status_sha256=std::string(64,'f');
    p.initial_private_choices=Header;p.initial_apt_state_presence=PackageStatePresence::Absent;
    auto& re=p.reconciliation_evidence;re.roots="app\ncore\n";
    re.previous_status_sha256=std::string(64,'1');re.previous_automatic_sha256=std::string(64,'2');
    re.current_status_sha256=std::string(64,'3');re.current_automatic_sha256=std::string(64,'4');
    re.solver_automatic_sha256=std::string(64,'5');re.result_registry_sha256=std::string(64,'6');re.result_automatic_sha256=std::string(64,'7');
    p.repositories={{"source",std::string(64,'8'),std::string(64,'9'),2000}};
    p.changes={{"app","all","1","2","source",{1024,std::string(64,'0')},PackageInstallReason::Manual},
               {"lib","all","1","2","source",{1024,std::string(64,'1')},PackageInstallReason::Automatic}};
    return p;
}
TEST(PackageReconciliationBinding, BindsPrivateSourceAndNewBaseWithoutChangingPrivateIntent) {
    auto p=ReconciliationPlan();PackageBoundPlan b;ASSERT_EQ(0,PackageBindReconciliationPlan(p,1000,&b))<<strerror(errno);
    EXPECT_EQ(p.source.sha256,b.preparation.image.sha256);EXPECT_EQ(p.previous.image_sha256,b.publication.previous.image_sha256);
    EXPECT_EQ(p.shared.sha256,b.publication.candidate.shared_base_sha256);EXPECT_TRUE(b.publication.personal);
    EXPECT_TRUE(b.publication.fence_shared_current);EXPECT_EQ(p.shared.sha256,b.publication.expected_shared.image_sha256);
    EXPECT_EQ(p.shared.bytes,b.publication.expected_shared.bytes);EXPECT_TRUE(b.publication.expected_shared.shared_base_sha256.empty());
    EXPECT_FALSE(b.publication.create);EXPECT_EQ(AEGIS_PACKAGE_RECONCILE,b.preparation.execution.kind);
    EXPECT_EQ(Header,std::string(b.preparation.execution.review.initial_choices));
    EXPECT_EQ(Header,std::string(b.preparation.execution.review.result_choices));
    EXPECT_EQ(p.reconciliation_evidence.result_registry_sha256,std::string(b.preparation.execution.review.result_registry));
    EXPECT_EQ(b.preparation.execution.plan_sha256,b.publication.plan_sha256);
}
TEST(PackageReconciliationBinding, EmptyEffectsStillBindAnExplicitBaseTransitionAndFullMarks) {
    auto p=ReconciliationPlan();p.changes.clear();p.repositories.clear();PackageBoundPlan b;
    ASSERT_EQ(0,PackageBindReconciliationPlan(p,1000,&b))<<strerror(errno);
    EXPECT_TRUE(b.preparation.execution.items.empty());EXPECT_TRUE(b.preparation.archives.empty());
    EXPECT_EQ(0u,b.valid_until_unix);EXPECT_EQ(AEGIS_PACKAGE_RECONCILE,b.preparation.execution.kind);
    EXPECT_NE(b.publication.previous.shared_base_sha256,b.publication.candidate.shared_base_sha256);
}
TEST(PackageReconciliationBinding, EverySnapshotAndCompleteResultChangesApprovalDigest) {
    auto p=ReconciliationPlan();PackageBoundPlan first;ASSERT_EQ(0,PackageBindReconciliationPlan(p,1000,&first));
    using Member=std::string PackageReconciliationEvidence::*;
    for(Member member:{&PackageReconciliationEvidence::previous_status_sha256,&PackageReconciliationEvidence::previous_automatic_sha256,
          &PackageReconciliationEvidence::current_status_sha256,&PackageReconciliationEvidence::current_automatic_sha256,
          &PackageReconciliationEvidence::solver_automatic_sha256,&PackageReconciliationEvidence::result_registry_sha256,
          &PackageReconciliationEvidence::result_automatic_sha256}) {
        auto changed=p;(changed.reconciliation_evidence.*member)=std::string(64,'a');PackageBoundPlan b;
        ASSERT_EQ(0,PackageBindReconciliationPlan(changed,1000,&b));EXPECT_NE(first.publication.plan_sha256,b.publication.plan_sha256);
    }
    auto changed=p;changed.previous_shared.bytes*=2;PackageBoundPlan b;
    ASSERT_EQ(0,PackageBindReconciliationPlan(changed,1000,&b));EXPECT_NE(first.publication.plan_sha256,b.publication.plan_sha256);
}
TEST(PackageReconciliationBinding, RejectsWrongScopeBaseCreationAndOrdinaryEndpointWithoutChangingOutput) {
    for(unsigned mode=0;mode<7;++mode) {
        auto p=ReconciliationPlan();
        if(mode==0)p.personal=false;if(mode==1)p.has_previous=false;if(mode==2)p.create_store=true;
        if(mode==3)p.previous_shared.sha256=std::string(64,'d');if(mode==4)p.previous_shared={};
        if(mode==5)p.reconciliation=false;if(mode==6)p.shared=p.previous_shared;
        PackageBoundPlan b;b.publication.plan_sha256="unchanged";
        EXPECT_EQ(-1,PackageBindReconciliationPlan(p,1000,&b));EXPECT_EQ("unchanged",b.publication.plan_sha256);
    }
    PackageBoundPlan b;EXPECT_EQ(-1,PackageBindResolvedPlan(ReconciliationPlan(),1000,&b));EXPECT_EQ(EOPNOTSUPP,errno);
}
TEST(PackageReconciliationBinding, MissingOrNoncanonicalRootAndResultEvidenceCannotBeApproved) {
    for(const char* roots:{"","app","app\napp\n","core\napp\n","--option\n"}) {
        auto p=ReconciliationPlan();p.reconciliation_evidence.roots=roots;PackageBoundPlan b;
        EXPECT_EQ(-1,PackageBindReconciliationPlan(p,1000,&b));
    }
    auto p=ReconciliationPlan();p.reconciliation_evidence.result_registry_sha256.clear();PackageBoundPlan b;
    EXPECT_EQ(-1,PackageBindReconciliationPlan(p,1000,&b));
    p=ReconciliationPlan();p.initial_private_choices=Header+"private\tall\t1\n";
    EXPECT_EQ(-1,PackageBindReconciliationPlan(p,1000,&b));EXPECT_EQ(ESTALE,errno);
    p=ReconciliationPlan();p.changes[1].reason=PackageInstallReason::Manual;
    EXPECT_EQ(-1,PackageBindReconciliationPlan(p,1000,&b));EXPECT_EQ(ESTALE,errno);
}
TEST(PackageReconciliationBinding, CompleteEvidenceRoundTripsAndUnusedTailCannotHideData) {
    auto p=ReconciliationPlan();aegis_planning_evidence wire={};ASSERT_EQ(0,planning_wire::Encode(p,&wire));
    PackageResolvedPlan decoded;ASSERT_EQ(0,planning_wire::Decode(wire,&decoded));
    EXPECT_EQ(p.reconciliation_evidence.roots,decoded.reconciliation_evidence.roots);
    EXPECT_EQ(p.reconciliation_evidence.result_registry_sha256,decoded.reconciliation_evidence.result_registry_sha256);
    wire.roots[100]='x';EXPECT_EQ(-1,planning_wire::Decode(wire,&decoded));EXPECT_EQ(EPROTO,errno);
}
TEST(PackageReconciliationBinding, ProjectionChangesUntouchedMarksAndPreservesHoldAndAllInstalledVersions) {
    PackageInstalledRegistry initial={{"app",{"1","all","install"}},{"core",{"1","all","hold"}},{"lib",{"1","all","install"}}},expected;
    std::set<std::string> marks;
    ASSERT_EQ(0,PackageReconciliationProject(initial,{},"core\n",&expected,&marks));
    EXPECT_EQ(initial,expected);EXPECT_EQ((std::set<std::string>{"app","lib"}),marks);
    EXPECT_EQ("AEGIS-INSTALLED1\napp\tall\t1\tinstall\ncore\tall\t1\thold\nlib\tall\t1\tinstall\n",PackageCanonicalInstalled(expected));
    EXPECT_EQ("AEGIS-AUTOMATIC1\napp\nlib\n",PackageCanonicalAutomatic(marks));
    auto before=expected;EXPECT_EQ(-1,PackageReconciliationProject(initial,{{"core","all","1","2",false}},"core\n",&expected,&marks));
    EXPECT_EQ(EDEADLK,errno);EXPECT_EQ(before,expected);
}

}
