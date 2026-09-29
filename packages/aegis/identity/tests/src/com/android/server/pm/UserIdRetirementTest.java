/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.server.pm;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.PropertyInvalidatedCache;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.UserInfo;
import android.os.Looper;

import androidx.test.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.server.LocalServices;

import java.io.File;
import java.util.UUID;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Adapted from AOSP's UserManagerServiceIdRecyclingTest fixture. Exercises the
 * integrated services.core allocator in this instrumentation process, never
 * the live system-server's user registry, creation, credentials or storage.
 * The cache directory is unique per test and retained for diagnosis.
 */
@RunWith(AndroidJUnit4.class)
public final class UserIdRetirementTest {
    private UserManagerService users;

    @Before public void setUp() {
        if (Looper.myLooper() == null) Looper.prepare();
        PropertyInvalidatedCache.disableForTestMode();
        Context base = InstrumentationRegistry.getContext();
        File fixture = new File(base.getCacheDir(), "aegis-user-ids-" + UUID.randomUUID());
        assertTrue(fixture.mkdir());
        Context isolated = new ContextWrapper(base) {
            @Override public File getCacheDir() { return fixture; }
        };
        // These LocalServices entries belong only to the test APK process.
        LocalServices.removeServiceForTest(UserManagerInternal.class);
        users = new UserManagerService(isolated);
    }

    @After public void tearDown() {
        LocalServices.removeServiceForTest(UserManagerInternal.class);
    }

    @Test public void unusedHoleStillAllocatesWhileRemovedIdsRemainReserved() {
        int retired = UserManagerService.MIN_USER_ID;
        int available = retired + 7;
        users.addRemovingUserId(retired);
        for (int id = retired + 1; id < UserManagerService.MAX_USER_ID; id++) {
            if (id != available) occupy(id);
        }
        assertEquals(available, users.getNextAvailableId());
        occupy(available);
        assertExhausted();
    }

    @Test public void exhaustionNeverRecyclesIdsOlderThanTheRecentQueue() {
        for (int id = UserManagerService.MIN_USER_ID;
                id < UserManagerService.MAX_USER_ID; id++) occupy(id);
        assertExhausted();
        // Upstream would discard the oldest retirement once this queue overflows.
        int end = UserManagerService.MIN_USER_ID
                + UserManagerService.MAX_RECENTLY_REMOVED_IDS_SIZE + 1;
        assertTrue(end < UserManagerService.MAX_USER_ID);
        for (int id = UserManagerService.MIN_USER_ID; id < end; id++) retire(id);
        for (int attempt = 0; attempt < 3; attempt++) assertExhausted();
    }

    @Test public void removingEveryIdentityDoesNotReopenTheNumberSpace() {
        for (int id = UserManagerService.MIN_USER_ID;
                id < UserManagerService.MAX_USER_ID; id++) {
            occupy(id);
            retire(id);
        }
        // There are no personal UserData objects left, but their IDs are retired.
        for (int attempt = 0; attempt < 3; attempt++) assertExhausted();
    }

    private void occupy(int id) {
        users.putUserInfo(new UserInfo(id, "Allocator fixture " + id, 0));
    }

    private void retire(int id) {
        users.removeUserInfo(id);
        users.addRemovingUserId(id);
    }

    private void assertExhausted() {
        try {
            int reused = users.getNextAvailableId();
            fail("Allocator reused a retired or occupied ID: " + reused);
        } catch (IllegalStateException expected) {
            // Failure is required; no identity may be allocated from this state.
        }
    }
}
