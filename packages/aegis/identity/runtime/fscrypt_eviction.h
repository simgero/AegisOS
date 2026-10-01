// Copyright 2026 The AegisOS Authors. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cerrno>
#include <cstdint>
#include <linux/fscrypt.h>
#include <string>

namespace aegis::fscrypt_eviction {
// Caller serializes removal/reinstallation of this exact key. The callbacks
// return -1 with errno or fill their output and return zero. No key bytes here.
template <class Query, class Remove>
bool Confirm(Query query, Remove remove) {
    uint32_t status = 0;
    if (query(&status) < 0) return false;
    if (status == FSCRYPT_KEY_STATUS_ABSENT) return true;
    if (status != FSCRYPT_KEY_STATUS_PRESENT &&
        status != FSCRYPT_KEY_STATUS_INCOMPLETELY_REMOVED) {
        errno = EPROTO;
        return false;
    }
    uint32_t flags = 0;
    if (remove(&flags) < 0) {
        // Last close can finish eviction between the two ioctls. ENOKEY alone
        // is insufficient; confirm ABSENT with the same key specifier below.
        if (errno != ENOKEY) return false;
    } else {
        if (flags & FSCRYPT_KEY_REMOVAL_STATUS_FLAG_OTHER_USERS) {
            errno = EPERM;
            return false;
        }
        if (flags & ~(FSCRYPT_KEY_REMOVAL_STATUS_FLAG_FILES_BUSY |
                      FSCRYPT_KEY_REMOVAL_STATUS_FLAG_OTHER_USERS)) {
            errno = EPROTO;
            return false;
        }
    }
    if (query(&status) < 0) return false;
    if (status == FSCRYPT_KEY_STATUS_ABSENT) return true;
    errno = status == FSCRYPT_KEY_STATUS_INCOMPLETELY_REMOVED ? EBUSY : EPROTO;
    return false;
}

// Retain EVERY policy until all volumes acknowledge completion. A retry must
// still check a previously busy key even if other volumes already completed.
template <class PolicyMap, class User, class Evict>
bool EvictUser(PolicyMap& policies, User user, Evict evict) {
    auto it = policies.find(user);
    if (it == policies.end()) return true;
    int error = 0;
    auto attempt = [&](const std::string& volume, const auto& policy) {
        if (!evict(volume, policy)) {
            int current = errno ? errno : EIO;
            if (!error || (error == EBUSY && current != EBUSY)) error = current;
        }
    };
    attempt("", it->second.internal);
    for (const auto& [volume, policy] : it->second.adoptable) attempt(volume, policy);
    if (error) { errno = error; return false; }
    policies.erase(it);
    return true;
}
}  // namespace aegis::fscrypt_eviction
