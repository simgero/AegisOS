#include "package_plan.h"
#include <gtest/gtest.h>
#include <errno.h>
#include <functional>
#include <set>
using namespace aegis;
namespace {
std::string H(char c) { return std::string(64,c); }
PackageResolvedPlan Plan() {
    PackageResolvedPlan p;p.requester=10;p.serial=42;p.personal=true;p.create_store=true;
    p.requested_package="test-app";p.requested_version="2:1.0~rc1-1";
    p.source=p.shared={268435456,H('a')};p.planner_image_sha256=H('b');
    p.policy_sha256=H('c');p.initial_status_sha256=H('d');
    p.initial_apt_state_presence=PackageStatePresence::Present;p.initial_apt_state={7,H('5')};
    p.repositories={{"debian-main",H('e'),H('f'),2000}};
    p.changes={{"test-app","arm64","","2:1.0~rc1-1","debian-main",{4096,H('1')},PackageInstallReason::Manual},
               {"test-lib","all","","1.2-1","debian-main",{2048,H('2')},PackageInstallReason::Automatic}};
    return p;
}
PackageResolvedPlan Removal() {
    auto p=Plan();p.action=PackageAction::Remove;p.requested_version.clear();p.repositories.clear();
    for(auto& c:p.changes) { c.before_version=c.after_version;c.after_version.clear();c.repository.clear();c.archive={}; }
    return p;
}
std::string Digest(const PackageResolvedPlan& p) {
    PackageBoundPlan b;EXPECT_EQ(0,PackageBindResolvedPlan(p,1000,&b))<<strerror(errno);
    return b.preparation.execution.plan_sha256;
}
void Reject(const PackageResolvedPlan& p,int expected=EINVAL,uint64_t now=1000) {
    PackageBoundPlan b;b.valid_until_unix=77;b.preparation.execution.plan_sha256="unchanged";
    b.publication.plan_sha256="unchanged";
    ASSERT_EQ(-1,PackageBindResolvedPlan(p,now,&b));EXPECT_EQ(expected,errno);
    EXPECT_EQ(77u,b.valid_until_unix);EXPECT_EQ("unchanged",b.preparation.execution.plan_sha256);
    EXPECT_EQ("unchanged",b.publication.plan_sha256);
}
}
TEST(PackageResolvedPlan, BindsCanonicalEpochArchivesAndRequesterPrivateOwner) {
    PackageBoundPlan b;ASSERT_EQ(0,PackageBindResolvedPlan(Plan(),1000,&b));
    const auto& prep=b.preparation;const auto& pub=b.publication;
    EXPECT_EQ((std::vector<std::string>{"test-app_2%3a1.0~rc1-1_arm64.deb","test-lib_1.2-1_all.deb"}),prep.execution.items);
    ASSERT_EQ(2u,prep.archives.size());EXPECT_EQ(H('1'),prep.archives[0].sha256);EXPECT_EQ(H('2'),prep.archives[1].sha256);
    EXPECT_EQ(10u,pub.requester);EXPECT_EQ(42u,pub.serial);EXPECT_TRUE(pub.personal);EXPECT_TRUE(pub.create);
    EXPECT_EQ(0u,prep.execution.job);EXPECT_EQ(0u,pub.job);EXPECT_TRUE(pub.derive_source_hash);
    EXPECT_TRUE(pub.candidate.image_sha256.empty());EXPECT_EQ(H('a'),pub.candidate.shared_base_sha256);
    EXPECT_EQ(268435456u,pub.candidate.bytes);EXPECT_EQ(2000u,b.valid_until_unix);
    EXPECT_EQ(prep.execution.plan_sha256,pub.plan_sha256);
}
TEST(PackageResolvedPlan, IndependentVersionSixDigestVectorAndClockStableBinding) {
    auto p=Plan();EXPECT_EQ("7aa6d03f6513fe6c20ce79848c0d362a3f7c34a8b0236b72a220f24e703010f2",Digest(p));
    PackageBoundPlan b;ASSERT_EQ(0,PackageBindResolvedPlan(p,1999,&b));
    EXPECT_EQ(Digest(p),b.preparation.execution.plan_sha256);
}
TEST(PackageResolvedPlan, SameVersionPrivateSelectionBindsNoArchiveAndDurableIntent) {
    auto p=Plan();p.changes.resize(1);auto& c=p.changes[0];
    c.before_version=c.after_version;c.archive={};c.repository.clear();p.repositories.clear();
    PackageBoundPlan b;ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&b))<<strerror(errno);
    EXPECT_EQ(AEGIS_PACKAGE_SELECTION,b.preparation.execution.kind);EXPECT_TRUE(b.preparation.archives.empty());
    EXPECT_EQ((std::vector<std::string>{"test-app"}),b.preparation.execution.items);EXPECT_EQ(0u,b.valid_until_unix);
    EXPECT_EQ("AEGIS-PRIVATE-CHOICES1\ntest-app\tarm64\t2:1.0~rc1-1\n",std::string(b.preparation.execution.review.result_choices));
    EXPECT_TRUE(b.publication.personal);EXPECT_EQ(p.requester,b.publication.requester);
    EXPECT_EQ(b.preparation.execution.plan_sha256,b.publication.plan_sha256);
    const auto original=Digest(p);p.requested_version.clear();EXPECT_NE(original,Digest(p));
}
TEST(PackageResolvedPlan, SameVersionSelectionRejectsWrongActionScopeArchiveAndAutomaticIntent) {
    auto good=Plan();good.changes.resize(1);good.changes[0].before_version=good.changes[0].after_version;
    good.changes[0].repository.clear();good.changes[0].archive={};good.repositories.clear();
    auto p=good;p.personal=false;Reject(p,EALREADY);
    p=good;p.action=PackageAction::Update;p.requested_package.clear();p.requested_version.clear();Reject(p);
    p=good;p.action=PackageAction::Remove;p.requested_version.clear();Reject(p);
    p=good;p.changes[0].reason=PackageInstallReason::Automatic;Reject(p);
    p=good;p.changes[0].archive={1,H('a')};Reject(p);
    p=good;p.changes[0].repository="debian-main";Reject(p);
    p=good;p.requested_version="9";Reject(p,ESTALE);
}
TEST(PackageResolvedPlan, RepeatedPrivateSelectionIsNoOpAndLegacyIntentIsNeverInvented) {
    auto p=Plan();p.changes.resize(1);p.changes[0].before_version=p.changes[0].after_version;
    p.changes[0].archive={};p.changes[0].repository.clear();
    p.has_previous=true;p.create_store=false;p.previous={p.source.sha256,p.shared.sha256,p.source.bytes};
    Reject(p,ENODATA);p.initial_private_choices="AEGIS-PRIVATE-CHOICES1\n";
    PackageBoundPlan b;ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&b));
    p.initial_private_choices=b.preparation.execution.review.result_choices;Reject(p,EALREADY);
    p.shared.sha256=H('4');Reject(p,ESTALE);
}

TEST(PackageResolvedPlan, EverySecurityRelevantChangeInvalidatesTheBinding) {
    std::set<std::string> digests{Digest(Plan())};
    const std::vector<std::function<void(PackageResolvedPlan&)>> changes={
        [](auto& p){p.requester=11;},[](auto& p){p.serial=43;},[](auto& p){p.personal=false;},
        [](auto& p){p.create_store=false;},[](auto& p){p.has_previous=true;p.create_store=false;p.previous={H('a'),H('a'),268435456};p.initial_private_choices="AEGIS-PRIVATE-CHOICES1\n";},
        [](auto& p){p.source.sha256=p.shared.sha256=H('3');},
        [](auto& p){p.source.bytes=p.shared.bytes=536870912;},
        [](auto& p){p.requested_version.clear();},[](auto& p){p.requested_package="test-lib";p.requested_version.clear();},
        [](auto& p){p.action=PackageAction::Update;p.requested_package.clear();p.requested_version.clear();},
        [](auto& p){p.planner_image_sha256=H('3');},[](auto& p){p.policy_sha256=H('3');},
        [](auto& p){p.initial_status_sha256=H('3');},
        [](auto& p){p.initial_apt_state.sha256=H('3');},[](auto& p){p.initial_apt_state.bytes++;},
        [](auto& p){p.initial_apt_state_presence=PackageStatePresence::Absent;p.initial_apt_state={};},
        [](auto& p){p.changes[1].reason=PackageInstallReason::Manual;},[](auto& p){p.repositories[0].release_sha256=H('3');},
        [](auto& p){p.repositories[0].index_sha256=H('3');},[](auto& p){p.repositories[0].valid_until_unix++;},
        [](auto& p){p.repositories[0].id="debian-other";for(auto& c:p.changes)c.repository="debian-other";},
        [](auto& p){p.changes[1].name="test-other";},[](auto& p){p.changes[1].architecture="arm64";},
        [](auto& p){p.changes[1].before_version="1.0";},[](auto& p){p.changes[1].after_version="1.3-1";},
        [](auto& p){p.changes[1].archive.bytes++;},[](auto& p){p.changes[1].archive.sha256=H('3');},
        [](auto& p){p.changes.pop_back();},
    };
    for(size_t i=0;i<changes.size();++i) { SCOPED_TRACE(i);auto p=Plan();changes[i](p);EXPECT_TRUE(digests.insert(Digest(p)).second); }
}
TEST(PackageResolvedPlan, PrivatePreviousGenerationIsRetainedAndBoundToSharedBase) {
    auto p=Plan();p.create_store=false;p.has_previous=true;p.source.sha256=H('3');
    p.previous={H('3'),H('a'),p.source.bytes};p.initial_private_choices="AEGIS-PRIVATE-CHOICES1\n";PackageBoundPlan b;
    ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&b));
    EXPECT_EQ(H('3'),b.publication.previous.image_sha256);EXPECT_EQ(H('a'),b.publication.candidate.shared_base_sha256);
    p.shared.sha256=H('4');Reject(p,ESTALE);
}
TEST(PackageResolvedPlan, SharedChangeNeverCreatesAPrivateTarget) {
    auto p=Plan();p.personal=false;p.create_store=false;p.has_previous=true;p.previous={H('a'),"",p.source.bytes};
    PackageBoundPlan b;ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&b));
    EXPECT_FALSE(b.publication.personal);EXPECT_EQ(10u,b.publication.requester);
    EXPECT_TRUE(b.publication.candidate.shared_base_sha256.empty());
    p.previous.shared_base_sha256=H('a');Reject(p);
}
TEST(PackageResolvedPlan, RejectsChangedOrInventedPreviousSelection) {
    auto p=Plan();p.previous={H('a'),H('a'),p.source.bytes};Reject(p);
    p.has_previous=true;Reject(p);p.create_store=false;p.previous.image_sha256=H('b');Reject(p);
    p.previous.image_sha256=H('a');p.previous.bytes+=4096;Reject(p);
    p=Plan();p.source.sha256=H('3');Reject(p);
    p=Plan();p.personal=false;p.shared.bytes+=4096;Reject(p);
}
TEST(PackageResolvedPlan, NoOpDoesNotCreateApprovalAndOutputIsUntouched) {
    auto p=Plan();p.changes.clear();Reject(p,EALREADY);
    p=Plan();p.changes[0].before_version=p.changes[0].after_version;Reject(p);
    p=Plan();p.changes[0].after_version.clear();Reject(p);
}
TEST(PackageResolvedPlan, ExplicitRequestMustBeRepresentedWithoutVersionSubstitution) {
    auto p=Plan();p.requested_version="99";Reject(p,ESTALE);
    p=Plan();p.requested_package="missing";Reject(p);
    p=Plan();p.action=PackageAction::Update;Reject(p);
    p=Plan();p.action=static_cast<PackageAction>(0);Reject(p);
    p=Removal();p.requested_version="1";Reject(p);
}
TEST(PackageResolvedPlan, ExpiredOrMissingRepositoryEvidenceIsRejected) {
    auto p=Plan();Reject(p,ESTALE,2000);Reject(p,ESTALE,2001);
    p.repositories[0].valid_until_unix=0;Reject(p);
    p=Plan();p.repositories.clear();Reject(p);
    p=Plan();p.changes[0].repository="unknown";Reject(p);
    p=Plan();p.repositories[0].index_sha256=std::string(64,'G');Reject(p);
    p=Plan();p.repositories[0].release_sha256.clear();Reject(p);
}
TEST(PackageResolvedPlan, EarliestRepositoryExpiryIsCarriedToConfirmation) {
    auto p=Plan();p.repositories.push_back({"security-main",H('3'),H('4'),1500});
    p.changes[1].repository="security-main";PackageBoundPlan b;
    ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&b));EXPECT_EQ(1500u,b.valid_until_unix);Reject(p,ESTALE,1500);
}
TEST(PackageResolvedPlan, RejectsDuplicateOrNoncanonicalOrdersAndForeignArchitecture) {
    auto p=Plan();std::swap(p.changes[0],p.changes[1]);Reject(p);
    p=Plan();p.changes[1].name=p.changes[0].name;Reject(p);
    p=Plan();p.changes[1].architecture="amd64";Reject(p);
    p=Plan();p.repositories.push_back(p.repositories[0]);Reject(p);
    p=Plan();p.repositories.push_back({"aaa",H('3'),H('4'),2000});Reject(p);
}
TEST(PackageResolvedPlan, RejectsOptionsPathsControlCharactersAndVersionExpressions) {
    for(const auto& text:{std::string("/tmp/app"),std::string("--assume-yes"),std::string("app;id"),
                          std::string("test-app\n"),std::string("test-app\0x",10)}) {
        auto p=Plan();p.requested_package=text;Reject(p);
        p=Plan();p.changes[0].name=text;Reject(p);
    }
    for(const char* text:{"1*","1/2","1;id","-1","v1","1:",":1","1:2:3","1_2","1%2","1 ","1-"}) {
        auto p=Plan();p.requested_version=text;Reject(p);
        p=Plan();p.changes[0].before_version=text;Reject(p);
    }
}
TEST(PackageResolvedPlan, RemovalKeepsConffilesAndHasNoArchivesOrAdminOwner) {
    PackageBoundPlan b;ASSERT_EQ(0,PackageBindResolvedPlan(Removal(),1000,&b));
    EXPECT_EQ(AEGIS_PACKAGE_REMOVE,b.preparation.execution.kind);EXPECT_TRUE(b.preparation.archives.empty());
    EXPECT_EQ((std::vector<std::string>{"test-app","test-lib"}),b.preparation.execution.items);
    EXPECT_EQ(10u,b.publication.requester);EXPECT_TRUE(b.publication.personal);EXPECT_EQ(0u,b.valid_until_unix);
}
TEST(PackageResolvedPlan, RemovalRejectsArchiveSmugglingAndAptActionOverrides) {
    auto p=Removal();p.changes[0].archive={1,H('a')};Reject(p);
    p=Removal();p.changes[0].repository="debian-main";Reject(p);
    for(const char* name:{"test-app+","test-app-"}) { p=Removal();p.requested_package=name;p.changes[0].name=name;Reject(p); }
}
TEST(PackageResolvedPlan, MixedInstallBindsRemovalAndOnlyItsOneArchive) {
    auto p=Plan();p.changes[1].before_version="1";p.changes[1].after_version.clear();
    p.changes[1].repository.clear();p.changes[1].archive={};
    PackageBoundPlan b;ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&b));
    EXPECT_EQ(AEGIS_PACKAGE_MIXED,b.preparation.execution.kind);
    EXPECT_EQ((std::vector<std::string>{"test-app_2%3a1.0~rc1-1_arm64.deb","test-lib"}),b.preparation.execution.items);
    ASSERT_EQ(1u,b.preparation.archives.size());EXPECT_EQ(H('1'),b.preparation.archives[0].sha256);
    auto changed=p;changed.changes.pop_back();EXPECT_NE(Digest(p),Digest(changed));
    changed=p;changed.changes[1].before_version="9";EXPECT_NE(Digest(p),Digest(changed));
}
TEST(PackageResolvedPlan, MixedRemovalMapsLaterArchiveAndRejectsUnreviewedOrWrongTargetEffects) {
    auto p=Removal();p.repositories=Plan().repositories;p.changes[1]=Plan().changes[1];
    PackageBoundPlan b;ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&b));
    EXPECT_EQ(AEGIS_PACKAGE_MIXED,b.preparation.execution.kind);
    EXPECT_EQ((std::vector<std::string>{"test-app","test-lib_1.2-1_all.deb"}),b.preparation.execution.items);
    ASSERT_EQ(1u,b.preparation.archives.size());EXPECT_EQ(H('2'),b.preparation.archives[0].sha256);
    auto prep=b.preparation;prep.execution.job=1;ASSERT_EQ(0,PackagePreparationCheck(prep));
    prep.archives.insert(prep.archives.begin(),{1,H('0')});EXPECT_EQ(-1,PackagePreparationCheck(prep));
    prep=b.preparation;prep.execution.job=1;prep.execution.review={};EXPECT_EQ(-1,PackagePreparationCheck(prep));
    prep=b.preparation;prep.execution.job=1;prep.execution.items[0]="another-app";EXPECT_EQ(-1,PackagePreparationCheck(prep));
    prep=b.preparation;prep.execution.job=1;prep.execution.kind=AEGIS_PACKAGE_ARCHIVES;EXPECT_EQ(-1,PackagePreparationCheck(prep));
    p.requested_package="test-lib";Reject(p);
    p=Removal();p.action=PackageAction::Install;Reject(p);
}
TEST(PackageResolvedPlan, EnforcesRealMechanicalImageArchiveAndCountLimits) {
    auto p=Plan();p.source.bytes=p.shared.bytes=4097;Reject(p);
    p=Plan();p.source.bytes=p.shared.bytes=(uint64_t{32}<<30)+4096;Reject(p);
    p=Plan();p.changes[0].archive.bytes=0;Reject(p);
    p=Plan();p.changes[0].archive.bytes=(uint64_t{2}<<30)+1;Reject(p);
    p=Plan();p.changes[0].archive.sha256="invalid";Reject(p);
    p=Plan();p.changes.resize(65);Reject(p);
    p=Plan();p.repositories.resize(17);Reject(p);
    p=Plan();p.requested_package=std::string(128,'a');p.changes[0].name=p.requested_package;
    p.requested_version=std::string(100,'1');p.changes[0].after_version=p.requested_version;Reject(p);
}
TEST(PackageResolvedPlan, EnforcesAggregateArchiveBudget) {
    auto p=Plan();p.requested_package="test-00";p.requested_version="1";p.changes.clear();
    for(unsigned i=0;i<5;i++)p.changes.push_back({"test-0"+std::to_string(i),"all","","1","debian-main",{uint64_t{2}<<30,H('1')},PackageInstallReason::Manual});
    Reject(p);p.changes.pop_back();EXPECT_EQ(64u,Digest(p).size());
}
TEST(PackageResolvedPlan, IdentityTimeAndRequiredHashesCannotBeOmitted) {
    auto p=Plan();p.requester=0;Reject(p);p=Plan();p.requester=21473;Reject(p);
    p=Plan();p.serial=UINT32_MAX;Reject(p);p=Plan();Reject(p,EINVAL,0);
    p.policy_sha256.clear();Reject(p);p=Plan();p.planner_image_sha256.clear();Reject(p);
    p=Plan();p.initial_status_sha256.clear();Reject(p);
    EXPECT_EQ(-1,PackageBindResolvedPlan(Plan(),1000,nullptr));EXPECT_EQ(EINVAL,errno);
}

TEST(PackageResolvedPlan, OmittedOrUnknownInstallReasonCannotBecomeManual) {
    auto p=Plan();p.changes[1].reason=PackageInstallReason::Unspecified;Reject(p);
    p=Plan();p.changes[1].reason=static_cast<PackageInstallReason>(3);Reject(p);
    p=Removal();p.changes[0].reason=PackageInstallReason::Unspecified;Reject(p);
}
TEST(PackageResolvedPlan, AbsentAndEmptyAptStateHaveDifferentBindings) {
    auto absent=Plan();absent.initial_apt_state_presence=PackageStatePresence::Absent;absent.initial_apt_state={};
    auto empty=absent;empty.initial_apt_state_presence=PackageStatePresence::Present;
    empty.initial_apt_state.sha256="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    EXPECT_NE(Digest(absent),Digest(empty));EXPECT_NE(Digest(Plan()),Digest(empty));
}
TEST(PackageResolvedPlan, AptStateRequiresCanonicalPresenceHashAndResourceBound) {
    auto p=Plan();p.initial_apt_state_presence=PackageStatePresence::Unspecified;Reject(p);
    p=Plan();p.initial_apt_state_presence=PackageStatePresence::Absent;Reject(p);
    p=Plan();p.initial_apt_state.sha256.clear();Reject(p);
    p=Plan();p.initial_apt_state.bytes=0;Reject(p);
    p=Plan();p.initial_apt_state.bytes=(uint64_t{16}<<20)+1;Reject(p);
    p.initial_apt_state.bytes=uint64_t{16}<<20;EXPECT_EQ(64u,Digest(p).size());
}
TEST(PackageResolvedPlan, ReviewRetainsExactAutomaticMarksAndInitialState) {
    for(const auto& p:{Plan(),Removal()}) {
        PackageBoundPlan b;ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&b));
        EXPECT_EQ(p.requester,b.reviewed.requester);EXPECT_EQ(p.serial,b.reviewed.serial);
        EXPECT_EQ(p.personal,b.reviewed.personal);EXPECT_EQ(p.action,b.reviewed.action);
        EXPECT_EQ(p.initial_status_sha256,b.reviewed.initial_status_sha256);
        EXPECT_EQ(PackageStatePresence::Present,b.reviewed.initial_apt_state_presence);EXPECT_EQ(7u,b.reviewed.initial_apt_state.bytes);
        EXPECT_EQ(H('5'),b.reviewed.initial_apt_state.sha256);
        ASSERT_EQ(2u,b.reviewed.changes.size());EXPECT_EQ(PackageInstallReason::Manual,b.reviewed.changes[0].reason);
        EXPECT_EQ(PackageInstallReason::Automatic,b.reviewed.changes[1].reason);
        EXPECT_EQ(p.changes[1].after_version,b.reviewed.changes[1].after_version);
        EXPECT_EQ(p.changes[1].archive.sha256,b.reviewed.changes[1].archive.sha256);
        EXPECT_EQ(b.preparation.execution.plan_sha256,Digest(b.reviewed));
    }
}

TEST(PackageResolvedPlan, PrivateChoicesAreBoundAndLegacyMissingManifestIsNotInferred) {
    auto p=Plan();p.create_store=false;p.has_previous=true;p.previous={H('a'),H('a'),p.source.bytes};
    Reject(p,ENODATA);p.initial_private_choices="AEGIS-PRIVATE-CHOICES1\n";
    auto first=Digest(p);p.initial_private_choices+="another-app\tall\t1\n";
    EXPECT_NE(first,Digest(p));PackageBoundPlan bound;ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&bound));
    EXPECT_EQ(p.initial_private_choices,std::string(bound.preparation.execution.review.initial_choices));
    EXPECT_EQ(p.initial_private_choices+"test-app\tarm64\t2:1.0~rc1-1\n",std::string(bound.preparation.execution.review.result_choices));
    p.initial_private_choices="broken";Reject(p,EBADMSG);
}

TEST(PackageResolvedPlan, EveryPrivateActionFencesSharedAndOnlyPinnedFactoryAllowsAbsence) {
    for(auto action:{PackageAction::Install,PackageAction::Update,PackageAction::Remove}) {
        auto p=action==PackageAction::Remove?Removal():Plan();p.action=action;
        if(action==PackageAction::Update) { p.requested_package.clear();p.requested_version.clear(); }
        PackageBoundPlan shared;ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&shared));
        EXPECT_TRUE(shared.publication.fence_shared_current);EXPECT_FALSE(shared.publication.allow_factory_shared);
        EXPECT_EQ(p.shared.sha256,shared.publication.expected_shared.image_sha256);
        EXPECT_EQ(p.shared.bytes,shared.publication.expected_shared.bytes);
        p.planner_image_sha256=p.shared.sha256;PackageBoundPlan factory;
        ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&factory));EXPECT_TRUE(factory.publication.fence_shared_current);
        EXPECT_TRUE(factory.publication.allow_factory_shared);EXPECT_NE(shared.publication.plan_sha256,factory.publication.plan_sha256);
        p.personal=false;PackageBoundPlan common;ASSERT_EQ(0,PackageBindResolvedPlan(p,1000,&common));
        EXPECT_FALSE(common.publication.fence_shared_current);EXPECT_FALSE(common.publication.allow_factory_shared);
        EXPECT_TRUE(common.publication.expected_shared.image_sha256.empty());
    }
}
