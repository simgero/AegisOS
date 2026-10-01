#include "package_private_choices.h"
#include "package_planning_protocol.h"
#include <gtest/gtest.h>
#include <errno.h>
using namespace aegis;
namespace {
const std::string empty="AEGIS-PRIVATE-CHOICES1\n";
PackageChange Effect(const std::string& name,const std::string& before,const std::string& after,bool automatic=false) {
    return {name,"all",before,after,"",{},automatic?PackageInstallReason::Automatic:PackageInstallReason::Manual};
}
TEST(PackagePrivateChoices, CanonicalRoundtripDistinguishesAbsenceAndPresentEmptySelection) {
    PackagePrivateChoices values;std::string text;
    ASSERT_EQ(0,PackagePrivateChoicesEncode(values,&text));EXPECT_EQ(empty,text);
    values["test-app"]={"all","1:2.3~rc1-4"};values["another-app"]={"arm64","9.0"};
    ASSERT_EQ(0,PackagePrivateChoicesEncode(values,&text));
    EXPECT_EQ(empty+"another-app\tarm64\t9.0\ntest-app\tall\t1:2.3~rc1-4\n",text);
    PackagePrivateChoices decoded;ASSERT_EQ(0,PackagePrivateChoicesDecode(text,&decoded));EXPECT_EQ(values,decoded);
    ASSERT_EQ(0,PackagePrivateChoicesDecode("",&decoded));EXPECT_TRUE(decoded.empty());
}
TEST(PackagePrivateChoices, ExplicitSameVersionIsRetainedWithoutInventingDependencyOwnership) {
    std::string text;
    ASSERT_EQ(0,PackagePrivateChoicesApply("",PackageAction::Install,"test-app","1",
        {Effect("test-app","1","1"),Effect("test-lib","","2",true)},&text));
    EXPECT_EQ(empty+"test-app\tall\t1\n",text);
}
TEST(PackagePrivateChoices, UpgradeAndRemovalRespectExistingExplicitChoices) {
    std::string before=empty+"other-app\tall\t8\ntest-app\tall\t1\n",after;
    ASSERT_EQ(0,PackagePrivateChoicesApply(before,PackageAction::Update,"","",
        {Effect("new-root","1","2"),Effect("test-app","1","3"),Effect("test-lib","1","3",true)},&after));
    EXPECT_EQ(empty+"new-root\tall\t2\nother-app\tall\t8\ntest-app\tall\t3\n",after);
    ASSERT_EQ(0,PackagePrivateChoicesApply(after,PackageAction::Remove,"test-app","",
        {Effect("test-app","3",""),Effect("test-lib","3","",true)},&before));
    EXPECT_EQ(empty+"new-root\tall\t2\nother-app\tall\t8\n",before);
}
TEST(PackagePrivateChoices, UnrelatedChoicesSurviveAnInstallationAndApprovedConflictRemovalDropsOnlyThatChoice) {
    std::string before=empty+"old-app\tall\t1\nother-app\tall\t8\n",after;
    ASSERT_EQ(0,PackagePrivateChoicesApply(before,PackageAction::Install,"test-app","2",
        {Effect("old-app","1",""),Effect("test-app","","2")},&after));
    EXPECT_EQ(empty+"other-app\tall\t8\ntest-app\tall\t2\n",after);
}
TEST(PackagePrivateChoices, MalformedOrAmbiguousManifestsLeaveOutputUnchanged) {
    PackagePrivateChoices expected={{"sentinel",{"all","1"}}};
    for(const auto& bad:std::vector<std::string>{"wrong\n",empty+"test-app\tall\t1",empty+"test-app\tall\t1\r\n",
        empty+"test-app\tall\t1\n"+"test-app\tall\t2\n",empty+"zz\tall\t1\naa\tall\t1\n",
        empty+"../app\tall\t1\n",empty+"test-app\tamd64\t1\n",empty+"test-app\tall\t1\t2\n",
        empty+std::string("test-app\tall\t1\0\n",17),empty+"\n"}) {
        SCOPED_TRACE(bad);auto got=expected;EXPECT_EQ(-1,PackagePrivateChoicesDecode(bad,&got));EXPECT_EQ(expected,got);
    }
}
TEST(PackagePrivateChoices, StaleCurrentVersionAndRequestedSubstitutionCannotChangeIntent) {
    std::string out="unchanged";const auto before=empty+"test-app\tall\t1\n";
    EXPECT_EQ(-1,PackagePrivateChoicesApply(before,PackageAction::Install,"test-app","3",{Effect("test-app","2","3")},&out));
    EXPECT_EQ(ESTALE,errno);EXPECT_EQ("unchanged",out);
    EXPECT_EQ(-1,PackagePrivateChoicesApply(before,PackageAction::Install,"test-app","9",{Effect("test-app","1","3")},&out));
    EXPECT_EQ(ESTALE,errno);EXPECT_EQ("unchanged",out);
    EXPECT_EQ(-1,PackagePrivateChoicesApply(before,PackageAction::Remove,"missing","",{Effect("test-app","1","")},&out));
    EXPECT_EQ(ENOENT,errno);EXPECT_EQ("unchanged",out);
}
TEST(PackagePrivateChoices, BoundLimitsRejectWithoutPartialOutput) {
    PackagePrivateChoices values;for(unsigned i=0;i<65;i++)values["pkg"+std::to_string(i)]={"all","1"};
    std::string out="unchanged";EXPECT_EQ(-1,PackagePrivateChoicesEncode(values,&out));EXPECT_EQ(E2BIG,errno);EXPECT_EQ("unchanged",out);
    PackagePrivateChoices parsed;EXPECT_EQ(-1,PackagePrivateChoicesDecode(std::string(AEGIS_PACKAGE_CHOICES_BYTES,'x'),&parsed));EXPECT_EQ(E2BIG,errno);
}
TEST(PackagePrivateChoices, PlanningWireCarriesExactManifestAndRejectsTrailingHiddenBytes) {
    PackageResolvedPlan plan;plan.initial_private_choices=empty+"test-app\tall\t1\n";
    aegis_planning_evidence wire={};ASSERT_EQ(0,planning_wire::Encode(plan,&wire));PackageResolvedPlan decoded;
    ASSERT_EQ(0,planning_wire::Decode(wire,&decoded));EXPECT_EQ(plan.initial_private_choices,decoded.initial_private_choices);
    wire.private_choices[plan.initial_private_choices.size()+1]='x';
    EXPECT_EQ(-1,planning_wire::Decode(wire,&decoded));EXPECT_EQ(EPROTO,errno);
}
}
