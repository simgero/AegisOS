// Copyright 2026 The AegisOS Authors. SPDX-License-Identifier: Apache-2.0
#include "fscrypt_eviction.h"
#include <gtest/gtest.h>
#include <map>
#include <vector>
using aegis::fscrypt_eviction::Confirm;
using aegis::fscrypt_eviction::EvictUser;
namespace {
struct Probe {
    std::vector<uint32_t> states;
    size_t at = 0;
    int removes = 0, query_error = 0, remove_error = 0;
    uint32_t flags = 0;
    bool run() {
        return Confirm([&](uint32_t* state) {
            if (query_error) { errno = query_error; return -1; }
            if (at >= states.size()) { errno = EOVERFLOW; return -1; }
            *state = states[at++]; return 0;
        }, [&](uint32_t* out) {
            ++removes;
            if (remove_error) { errno = remove_error; return -1; }
            *out = flags; return 0;
        });
    }
};
struct Policies { int internal; std::map<std::string,int> adoptable; };
}
TEST(FscryptEviction, AlreadyAbsentRequiresActualQueryAndNoRemove) {
    Probe p{{FSCRYPT_KEY_STATUS_ABSENT}};
    EXPECT_TRUE(p.run()); EXPECT_EQ(p.removes,0);
}
TEST(FscryptEviction, SuccessfulRemoveStillRequiresAbsentReadback) {
    Probe p{{FSCRYPT_KEY_STATUS_PRESENT,FSCRYPT_KEY_STATUS_ABSENT}};
    EXPECT_TRUE(p.run()); EXPECT_EQ(p.at,2u); EXPECT_EQ(p.removes,1);
}
TEST(FscryptEviction, BusyNeverAuthorizesCompletion) {
    Probe p{{FSCRYPT_KEY_STATUS_PRESENT,FSCRYPT_KEY_STATUS_INCOMPLETELY_REMOVED}};
    p.flags=FSCRYPT_KEY_REMOVAL_STATUS_FLAG_FILES_BUSY;
    EXPECT_FALSE(p.run()); EXPECT_EQ(errno,EBUSY);
}
TEST(FscryptEviction, LastCloseAfterBusyCanCompleteWithAbsentReadback) {
    Probe p{{FSCRYPT_KEY_STATUS_INCOMPLETELY_REMOVED,FSCRYPT_KEY_STATUS_ABSENT}};
    p.flags=FSCRYPT_KEY_REMOVAL_STATUS_FLAG_FILES_BUSY;
    EXPECT_TRUE(p.run());
}
TEST(FscryptEviction, OtherOwnersAndUnknownFlagsNeverAuthorizeCompletion) {
    for(uint32_t flag:{uint32_t(FSCRYPT_KEY_REMOVAL_STATUS_FLAG_OTHER_USERS),uint32_t(0x80000000)}) {
        Probe p{{FSCRYPT_KEY_STATUS_PRESENT,FSCRYPT_KEY_STATUS_ABSENT}};p.flags=flag;
        EXPECT_FALSE(p.run()); EXPECT_EQ(p.at,1u);
    }
}
TEST(FscryptEviction, MissingKeyErrorNeedsIndependentAbsence) {
    Probe p{{FSCRYPT_KEY_STATUS_INCOMPLETELY_REMOVED,FSCRYPT_KEY_STATUS_ABSENT}};
    p.remove_error=ENOKEY; EXPECT_TRUE(p.run());
    Probe q{{FSCRYPT_KEY_STATUS_PRESENT,FSCRYPT_KEY_STATUS_PRESENT}};
    q.remove_error=ENOKEY;EXPECT_FALSE(q.run());EXPECT_EQ(errno,EPROTO);
}
TEST(FscryptEviction, QueryAndRemoveErrorsFailClosed) {
    Probe p{{FSCRYPT_KEY_STATUS_PRESENT}};p.query_error=EIO;
    EXPECT_FALSE(p.run());EXPECT_EQ(p.removes,0);EXPECT_EQ(errno,EIO);
    Probe q{{FSCRYPT_KEY_STATUS_PRESENT}};q.remove_error=EPERM;
    EXPECT_FALSE(q.run());EXPECT_EQ(errno,EPERM);
    Probe r{{42}};EXPECT_FALSE(r.run());EXPECT_EQ(errno,EPROTO);
}
TEST(FscryptEviction, PoliciesSurvivePartialEvictionAndRetryUsesEveryVolume) {
    std::map<int,Policies> map{{10,{1,{{"disk",2}}}},{11,{3,{}}}};
    std::vector<int> called;
    EXPECT_FALSE(EvictUser(map,10,[&](const auto&,int policy){called.push_back(policy);errno=EBUSY;return policy==1;}));
    EXPECT_EQ(errno,EBUSY);ASSERT_EQ(map.size(),2u);EXPECT_EQ(map.at(10).internal,1);
    EXPECT_EQ(called,(std::vector<int>{1,2}));called.clear();
    EXPECT_TRUE(EvictUser(map,10,[&](const auto&,int policy){called.push_back(policy);return true;}));
    EXPECT_EQ(called,(std::vector<int>{1,2}));EXPECT_EQ(map.count(10),0u);EXPECT_EQ(map.count(11),1u);
}
TEST(FscryptEviction, HardFailureIsNotHiddenByBusyAndPreservesPolicies) {
    std::map<int,Policies> map{{10,{1,{{"disk",2}}}}};
    EXPECT_FALSE(EvictUser(map,10,[](const auto&,int policy){errno=policy==1?EBUSY:EIO;return false;}));
    EXPECT_EQ(errno,EIO);EXPECT_EQ(map.size(),1u);
}
