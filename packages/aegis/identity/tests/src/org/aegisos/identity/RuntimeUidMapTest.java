package org.aegisos.identity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.os.Process;
import android.os.UserHandle;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import java.util.HashSet;
import java.util.Set;

import org.junit.Test;
import org.junit.runner.RunWith;

/** Tests the layout against platform UID helpers, not live namespaces or authentication. */
@RunWith(AndroidJUnit4.class)
public final class RuntimeUidMapTest {
    private static RuntimeUidMap mapping(int id, int serial) {
        return new RuntimeUidMap(new AospIdentityBackend.UserKey(id, serial));
    }

    private static void rejected(Runnable action) {
        try {
            action.run();
            fail("Expected invalid mapping to be rejected");
        } catch (IllegalArgumentException expected) { }
    }

    @Test public void allMappedIdsStayInTheirOwnAospUserAndAvoidSystemAndAppIds() {
        Set<Integer> first = new HashSet<>();
        Set<Integer> second = new HashSet<>();
        RuntimeUidMap a = mapping(10, 10), b = mapping(11, 11);
        for (int[] range : RuntimeUidLayout.ranges()) {
            for (int offset = 0; offset < range[2]; offset++) {
                int inside = range[0] + offset;
                int aHost = a.hostId(inside), bHost = b.hostId(inside);
                assertEquals(10, UserHandle.getUserId(aHost));
                assertEquals(11, UserHandle.getUserId(bHost));
                assertTrue(first.add(aHost));
                assertTrue(second.add(bHost));
                int appId = UserHandle.getAppId(aHost);
                assertTrue((appId >= 5000 && appId <= 5999) || appId == 7500 || appId == 7501);
                assertNotEquals(Process.SYSTEM_UID, aHost);
                assertNotEquals(Process.ROOT_UID, aHost);
                assertNotEquals(Process.SHELL_UID, aHost);
            }
        }
        first.retainAll(second);
        assertTrue("Different people must never share a mapped host ID", first.isEmpty());
        assertEquals(UserHandle.getUid(10, 7500), a.hostId(RuntimeUidMap.NORMAL_ID));
        assertNotEquals(a.hostId(0), a.hostId(RuntimeUidMap.NORMAL_ID));
    }

    @Test public void reusedUserNumberKeepsItsNewSerialButCannotClaimTheOldContext() {
        RuntimeUidMap before = mapping(10, 10), after = mapping(10, 99);
        // Host-ID reuse is deliberate; lifecycle cleanup must precede reuse.
        // The map is not a lease authorizing that reuse.
        assertEquals(before.hostId(1000), after.hostId(1000));
        assertFalse(before.owner().equals(after.owner()));
        assertEquals("u10-s10", before.storageKey());
        assertEquals("u10-s99", after.storageKey());
    }

    @Test public void platformBoundsAndUnmappedLinuxIdsAreRejected() {
        rejected(() -> mapping(0, 0));
        rejected(() -> mapping(UserHandle.MIN_SECONDARY_USER_ID - 1, 1));
        rejected(() -> mapping(UserHandle.MAX_SECONDARY_USER_ID, 1));
        rejected(() -> mapping(Integer.MAX_VALUE, 1));
        rejected(() -> mapping(10, -1));
        RuntimeUidMap valid = mapping(10, 10);
        for (int id : new int[]{-1, 1001, 5000, 64055, 65535, Integer.MAX_VALUE}) {
            rejected(() -> valid.hostId(id));
        }
        RuntimeUidMap last = mapping(UserHandle.MAX_SECONDARY_USER_ID - 1, Integer.MAX_VALUE);
        assertTrue(last.hostId(65534) > 0);
        assertEquals(UserHandle.MAX_SECONDARY_USER_ID - 1,
                UserHandle.getUserId(last.hostId(65534)));
    }

    @Test public void procMapContainsOnlyTheReservedExtents() {
        assertEquals("0 1005000 1000\n1000 1007500 1\n65534 1007501 1\n",
                mapping(10, 10).namespaceMap());
    }

    @Test public void anotherCallerCannotMutateTheSharedLayout() {
        RuntimeUidMap owner = mapping(10, 10);
        String before = owner.namespaceMap();
        int[][] attemptedChange = RuntimeUidLayout.ranges();
        attemptedChange[0][1] = Process.ROOT_UID;
        assertEquals(before, owner.namespaceMap());
    }
}
