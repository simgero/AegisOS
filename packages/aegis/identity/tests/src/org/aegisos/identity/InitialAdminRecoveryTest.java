package org.aegisos.identity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.os.RemoteException;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.junit.Test;
import org.junit.runner.RunWith;

/** Device-side coordinator tests with a fake platform. Never changes a real AOSP user. */
@RunWith(AndroidJUnit4.class)
public final class InitialAdminRecoveryTest {
    private static final String PENDING = "created:10:42";
    private static final String DONE = "complete:10:42";
    private static final AospIdentityBackend.UserKey KEY = new AospIdentityBackend.UserKey(10, 42);

    private static final class Platform implements InitialAdminRecovery.Platform {
        String marker = PENDING;
        AospIdentityBackend.UserKey user = KEY;
        String name = "Fixture";
        boolean enabled, admin, partial, running, foreground, ceUnlocked;
        boolean authorized = true, otherUsers, passwordAccepted = true;
        boolean lockLeavesRunning, lockLeavesUnlocked, lockLeavesForeground;
        String failAfter, revokeAfter, changeRecordAfter, ineffective;
        InitialAdminRecovery.Credential credential = InitialAdminRecovery.Credential.PASSWORD;
        final List<String> mutations = new ArrayList<>();
        final SecurityException authenticationFailure = new SecurityException("Fixture AOSP rejection");

        private void after(String operation) throws RemoteException {
            mutations.add(operation);
            if (operation.equals(revokeAfter)) authorized = false;
            if (operation.equals(changeRecordAfter)) marker = "created:11:99";
            if (operation.equals(failAfter)) {
                failAfter = null;
                throw new RemoteException("Fixture interruption after " + operation);
            }
        }

        @Override public void guard() {
            if (!authorized || otherUsers) throw new SecurityException("Fixture authority revoked");
        }
        @Override public String readRecord() { return marker; }
        @Override public InitialAdminRecovery.Snapshot inspect() {
            return new InitialAdminRecovery.Snapshot(user, name, enabled, admin, partial,
                    running, foreground, ceUnlocked, credential);
        }
        @Override public void enrollPassword() throws RemoteException {
            if (credential != InitialAdminRecovery.Credential.NONE) {
                throw new AssertionError("Existing credentials must never be overwritten");
            }
            credential = InitialAdminRecovery.Credential.PASSWORD;
            ceUnlocked = true;
            after("enroll");
        }
        @Override public void verifyPassword() throws RemoteException {
            if (!passwordAccepted) throw authenticationFailure;
            ceUnlocked = true; // Matches LSS's side effect even for a stopped disabled user.
            if (enabled) running = true;
            after("verify");
        }
        @Override public void stopAndLock() throws RemoteException {
            running = lockLeavesRunning;
            ceUnlocked = lockLeavesUnlocked;
            foreground = lockLeavesForeground;
            after("lock");
        }
        @Override public void grantAdmin() {
            if (running || ceUnlocked || foreground || enabled) {
                throw new AssertionError("Premature admin grant");
            }
            if (!"admin".equals(ineffective)) admin = true;
            afterUnchecked("admin");
        }
        @Override public void enable() {
            if (!admin || running || ceUnlocked || foreground) {
                throw new AssertionError("Premature enable");
            }
            if (!"enable".equals(ineffective)) enabled = true;
            afterUnchecked("enable");
        }
        @Override public void writeComplete(String value) {
            if (!enabled || !admin || running || ceUnlocked || foreground) {
                throw new AssertionError("False successful completion");
            }
            if (!"complete".equals(ineffective)) marker = value;
            afterUnchecked("complete");
        }
        private void afterUnchecked(String operation) {
            try { after(operation); }
            catch (RemoteException interruption) { throw new IllegalStateException(interruption); }
        }
    }

    private static InitialAdminRecovery.Snapshot resume(Platform platform) throws RemoteException {
        return InitialAdminRecovery.resume(InitialAdminRecovery.Record.parse(platform.marker),
                "Fixture", platform);
    }

    private static void fails(Platform platform) throws Exception {
        try {
            resume(platform);
            fail("Recovery unexpectedly succeeded");
        } catch (SecurityException | IllegalStateException | RemoteException expected) { }
        assertFalse(platform.mutations.contains("complete"));
    }

    @Test public void recordsRequireCanonicalBoundedAospIdentity() {
        assertEquals(InitialAdminRecovery.Phase.AVAILABLE, InitialAdminRecovery.Record.parse(null).phase);
        assertEquals(InitialAdminRecovery.Phase.RESERVED, InitialAdminRecovery.Record.parse("reserved").phase);
        assertEquals(KEY, InitialAdminRecovery.Record.parse(PENDING).user);
        assertEquals(InitialAdminRecovery.Phase.COMPLETE, InitialAdminRecovery.Record.parse(DONE).phase);
        for (String value : Arrays.asList("", "created:0:42", "created:10:-1", "created:10:042",
                "created:010:42", "created:2147483648:42", "complete:10:2147483648",
                "created:10:42:extra", "complete:10:42\n", "reserved:10:42")) {
            try { InitialAdminRecovery.Record.parse(value); fail("Invalid marker accepted: " + value); }
            catch (IllegalStateException expected) { }
        }
    }

    @Test public void unfinishedEnrollmentSetsPasswordBeforeEnabling() throws Exception {
        Platform p = new Platform();
        p.credential = InitialAdminRecovery.Credential.NONE;
        p.ceUnlocked = true;
        InitialAdminRecovery.Snapshot result = resume(p);
        assertEquals(Arrays.asList("enroll", "lock", "admin", "enable", "complete"), p.mutations);
        assertEquals(DONE, p.marker);
        assertTrue(result.enabled && result.admin);
        assertFalse(result.running || result.ceUnlocked || result.foreground);
    }

    @Test public void existingPasswordIsVerifiedAndNeverReenrolled() throws Exception {
        Platform p = new Platform();
        resume(p);
        assertEquals(Arrays.asList("verify", "lock", "admin", "enable", "complete"), p.mutations);
        assertFalse(p.ceUnlocked);
    }

    @Test public void alreadyEnabledAdminMustAuthenticateAndStopAgain() throws Exception {
        Platform p = new Platform();
        p.admin = p.enabled = p.running = p.foreground = p.ceUnlocked = true;
        resume(p);
        assertEquals(Arrays.asList("verify", "lock", "complete"), p.mutations);
        assertFalse(p.running || p.foreground || p.ceUnlocked);
    }

    @Test public void aospRejectionIsPreservedAndDoesNotGrantAuthority() throws Exception {
        Platform p = new Platform();
        p.passwordAccepted = false;
        try { resume(p); fail("Rejected password accepted"); }
        catch (SecurityException rejected) { assertSame(p.authenticationFailure, rejected); }
        assertTrue(p.mutations.isEmpty());
        assertEquals(PENDING, p.marker);
        assertFalse(p.admin || p.enabled);
    }

    @Test public void reusedIdentityAndUnexpectedAccountsFailBeforeMutation() throws Exception {
        for (int variant = 0; variant < 9; variant++) {
            Platform p = new Platform();
            switch (variant) {
                case 0: p.user = new AospIdentityBackend.UserKey(10, 43); break;
                case 1: p.name = "Someone else"; break;
                case 2: p.partial = true; break;
                case 3: p.enabled = true; break; // enabled non-admin cannot be promoted by recovery
                case 4: p.running = true; break; // disabled but unexpectedly started
                case 5: p.credential = InitialAdminRecovery.Credential.OTHER; break;
                case 6: p.admin = true; p.credential = InitialAdminRecovery.Credential.NONE; break;
                case 7: p.otherUsers = true; break;
                case 8: p.credential = null; break;
                default: throw new AssertionError();
            }
            fails(p);
            assertTrue(p.mutations.isEmpty());
        }
    }

    @Test public void aStopReturnAloneDoesNotProveStorageLocked() throws Exception {
        for (int variant = 0; variant < 3; variant++) {
            Platform p = new Platform();
            p.lockLeavesRunning = variant == 0;
            p.lockLeavesUnlocked = variant == 1;
            p.lockLeavesForeground = variant == 2;
            fails(p);
            assertFalse(p.admin || p.enabled);
        }
    }

    @Test public void revokedClientCannotFinishRecovery() throws Exception {
        for (String operation : Arrays.asList("verify", "lock", "admin", "enable")) {
            Platform p = new Platform();
            p.revokeAfter = operation;
            fails(p);
            assertEquals(PENDING, p.marker);
        }
    }

    @Test public void changedReservationCannotBeOverwritten() throws Exception {
        for (String operation : Arrays.asList("verify", "lock", "admin", "enable")) {
            Platform p = new Platform();
            p.changeRecordAfter = operation;
            fails(p);
            assertEquals("created:11:99", p.marker);
        }
    }

    @Test public void eachInterruptedMutationCanResumeWithoutResettingPassword() throws Exception {
        for (String operation : Arrays.asList("enroll", "verify", "lock", "admin", "enable")) {
            Platform p = new Platform();
            if ("enroll".equals(operation)) p.credential = InitialAdminRecovery.Credential.NONE;
            p.failAfter = operation;
            fails(p);
            int enrollmentCount = (int) p.mutations.stream().filter("enroll"::equals).count();
            p.mutations.clear();
            resume(p);
            assertEquals("verify", p.mutations.get(0));
            assertFalse(p.mutations.contains("enroll"));
            assertEquals("enroll".equals(operation) ? 1 : 0, enrollmentCount);
            assertEquals(DONE, p.marker);
            assertFalse(p.ceUnlocked || p.running || p.foreground);
        }
    }

    @Test public void availableReservedAndCompletedRecordsCannotAdoptAUser() throws Exception {
        for (String marker : Arrays.asList(null, "reserved", DONE)) {
            Platform p = new Platform();
            p.marker = marker;
            fails(p);
            assertTrue(p.mutations.isEmpty());
        }
    }

    @Test public void unconfirmedPlatformChangesNeverReturnSuccess() throws Exception {
        for (String operation : Arrays.asList("admin", "enable", "complete")) {
            Platform p = new Platform();
            p.ineffective = operation;
            try { resume(p); fail("Unconfirmed " + operation + " accepted"); }
            catch (IllegalStateException | SecurityException expected) { }
            assertEquals(PENDING, p.marker);
            if ("admin".equals(operation)) assertFalse(p.mutations.contains("enable"));
        }
    }
}
