package com.android.server.aegis;

import static org.junit.Assert.*;

import android.content.pm.UserInfo;
import android.os.Process;
import android.os.UserManager;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.android.internal.widget.LockscreenCredential;
import com.android.internal.widget.VerifyCredentialResponse;

import java.nio.CharBuffer;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Isolated policy/ownership tests. No real administrator password or CE unlock proof. */
@RunWith(AndroidJUnit4.class)
public final class AegisPackageCredentialsTest {
    private static LockscreenCredential password() {
        // Public inert test data; never enrolled in LockSettings.
        return LockscreenCredential.createPassword(CharBuffer.wrap(new char[]{'t','e','s','t'}));
    }

    private static void throwsType(Class<? extends Throwable> expected, Runnable action) {
        try { action.run(); }
        catch (Throwable failure) {
            if (expected.isInstance(failure)) return;
            throw new AssertionError("Unexpected exception", failure);
        }
        fail("Expected " + expected.getSimpleName());
    }

    private static final class Fixture implements AegisPackageCredentials.Directory {
        UserInfo info = new UserInfo(10, "Inert administrator", UserInfo.FLAG_ADMIN | UserInfo.FLAG_FULL);
        final Set<String> restrictions = new HashSet<>();
        final AtomicInteger checks = new AtomicInteger();
        Runnable during = () -> {};
        VerifyCredentialResponse response = VerifyCredentialResponse.OK;
        final AegisPackageCredentials check = new AegisPackageCredentials(this, (credential, id) -> {
            assertEquals(10, id);
            assertTrue(credential.isPassword());
            checks.incrementAndGet();
            during.run();
            return response;
        });
        Fixture() { info.serialNumber = 42; info.userType = UserManager.USER_TYPE_FULL_SECONDARY; }
        @Override public UserInfo user(int id) { return info; }
        @Override public boolean restricted(int id, String restriction) { return restrictions.contains(restriction); }
        VerifyCredentialResponse call(AegisPackageCredentials.Action action) {
            try (LockscreenCredential credential = password()) {
                return check.verifyOwned(10, 42, action, credential);
            }
        }
    }

    @Test public void everyActionAndEveryAttemptRequiresAnotherCredentialCheck() {
        Fixture fixture = new Fixture();
        for (AegisPackageCredentials.Action action : AegisPackageCredentials.Action.values()) {
            assertEquals(VerifyCredentialResponse.RESPONSE_OK, fixture.call(action).getResponseCode());
            assertEquals(VerifyCredentialResponse.RESPONSE_OK, fixture.call(action).getResponseCode());
        }
        assertEquals(6, fixture.checks.get());
    }

    @Test public void otherIdOrReusedSerialCannotReachTheCredentialVerifier() {
        Fixture fixture = new Fixture();
        try (LockscreenCredential credential = password()) {
            for (int[] key : new int[][]{{0,42}, {11,42}, {10,43}, {10,-1}}) {
                throwsType(SecurityException.class, () -> fixture.check.verifyOwned(key[0], key[1],
                        AegisPackageCredentials.Action.INSTALL, credential));
            }
        }
        assertEquals(0, fixture.checks.get());
    }

    @Test public void nonPersonalDisabledIncompleteOrNonAdminUsersAreDeniedBeforePassword() {
        for (int kind = 0; kind < 7; kind++) {
            Fixture fixture = new Fixture();
            switch (kind) {
                case 0: fixture.info.flags &= ~UserInfo.FLAG_ADMIN; break;
                case 1: fixture.info.flags |= UserInfo.FLAG_DISABLED; break;
                case 2: fixture.info.partial = true; break;
                case 3: fixture.info.preCreated = true; break;
                case 4:
                    // Android16 derives isGuest from userType, not FLAG_GUEST.
                    fixture.info.flags |= UserInfo.FLAG_GUEST;
                    fixture.info.userType = UserManager.USER_TYPE_FULL_GUEST;
                    assertTrue(fixture.info.isGuest());
                    break;
                case 5: fixture.info.userType = UserManager.USER_TYPE_PROFILE_MANAGED; break;
                case 6: fixture.info = null; break;
                default: throw new AssertionError();
            }
            try {
                throwsType(SecurityException.class, () -> fixture.call(AegisPackageCredentials.Action.INSTALL));
            } catch (AssertionError failure) {
                throw new AssertionError("Rejected-user fixture kind=" + kind, failure);
            }
            assertEquals(0, fixture.checks.get());
        }
    }

    @Test public void noPasswordOrMissingActionCannotReachTheVerifier() {
        Fixture fixture = new Fixture();
        try (LockscreenCredential none = LockscreenCredential.createNone()) {
            throwsType(IllegalArgumentException.class, () -> fixture.check.verifyOwned(10, 42,
                    AegisPackageCredentials.Action.INSTALL, none));
        }
        throwsType(NullPointerException.class, () -> fixture.call(null));
        assertEquals(0, fixture.checks.get());
    }

    @Test public void generalRestrictionRejectsAllThreeActionsBeforePassword() {
        Fixture fixture = new Fixture();
        fixture.restrictions.add(UserManager.DISALLOW_APPS_CONTROL);
        for (AegisPackageCredentials.Action action : AegisPackageCredentials.Action.values()) {
            throwsType(SecurityException.class, () -> fixture.call(action));
        }
        assertEquals(0, fixture.checks.get());
    }

    @Test public void installAndUninstallRestrictionsApplyToTheCorrespondingActions() {
        Fixture fixture = new Fixture();
        fixture.restrictions.add(UserManager.DISALLOW_INSTALL_APPS);
        throwsType(SecurityException.class, () -> fixture.call(AegisPackageCredentials.Action.INSTALL));
        throwsType(SecurityException.class, () -> fixture.call(AegisPackageCredentials.Action.UPDATE));
        assertEquals(0, fixture.checks.get());
        assertEquals(VerifyCredentialResponse.RESPONSE_OK,
                fixture.call(AegisPackageCredentials.Action.REMOVE).getResponseCode());
        fixture.restrictions.clear();
        fixture.restrictions.add(UserManager.DISALLOW_UNINSTALL_APPS);
        throwsType(SecurityException.class, () -> fixture.call(AegisPackageCredentials.Action.REMOVE));
        assertEquals(1, fixture.checks.get());
    }

    @Test public void demotionDuringVerificationRejectsEvenMatchingPassword() {
        Fixture fixture = new Fixture();
        fixture.during = () -> fixture.info.flags &= ~UserInfo.FLAG_ADMIN;
        throwsType(SecurityException.class, () -> fixture.call(AegisPackageCredentials.Action.INSTALL));
        assertEquals(1, fixture.checks.get());
    }

    @Test public void serialReplacementDuringVerificationRejectsEvenMatchingPassword() {
        Fixture fixture = new Fixture();
        fixture.during = () -> fixture.info.serialNumber++;
        throwsType(SecurityException.class, () -> fixture.call(AegisPackageCredentials.Action.UPDATE));
        assertEquals(1, fixture.checks.get());
    }

    @Test public void newlyAppliedRestrictionIsNotIgnoredAfterPassword() {
        Fixture fixture = new Fixture();
        fixture.during = () -> fixture.restrictions.add(UserManager.DISALLOW_UNINSTALL_APPS);
        throwsType(SecurityException.class, () -> fixture.call(AegisPackageCredentials.Action.REMOVE));
        assertEquals(1, fixture.checks.get());
    }

    @Test public void failedPasswordRemainsRejected() {
        Fixture fixture = new Fixture();
        fixture.response = VerifyCredentialResponse.ERROR;
        assertEquals(VerifyCredentialResponse.RESPONSE_ERROR,
                fixture.call(AegisPackageCredentials.Action.INSTALL).getResponseCode());
        assertEquals(1, fixture.checks.get());
    }

    @Test public void retryPreservesAospDelayWithoutMintingSuccess() {
        Fixture fixture = new Fixture();
        fixture.response = VerifyCredentialResponse.fromTimeout(30000);
        VerifyCredentialResponse response = fixture.call(AegisPackageCredentials.Action.REMOVE);
        assertEquals(VerifyCredentialResponse.RESPONSE_RETRY, response.getResponseCode());
        assertEquals(30000, response.getTimeout());
        assertNull(response.getGatekeeperHAT());
        assertEquals(0, response.getGatekeeperPasswordHandle());
    }

    @Test public void successfulCheckCannotExportHatOrPasswordHandle() {
        Fixture fixture = new Fixture();
        fixture.response = new VerifyCredentialResponse.Builder().setGatekeeperHAT(new byte[]{1,2,3})
                .setGatekeeperPasswordHandle(123L).build();
        VerifyCredentialResponse response = fixture.call(AegisPackageCredentials.Action.UPDATE);
        assertEquals(VerifyCredentialResponse.RESPONSE_OK, response.getResponseCode());
        assertNull(response.getGatekeeperHAT());
        assertEquals(0, response.getGatekeeperPasswordHandle());
    }

    @Test public void missingOrFailedProviderIsNeverAccepted() {
        Fixture fixture = new Fixture();
        fixture.response = null;
        throwsType(NullPointerException.class, () -> fixture.call(AegisPackageCredentials.Action.UPDATE));
        fixture.during = () -> { throw new IllegalStateException("Unavailable AOSP fixture"); };
        throwsType(IllegalStateException.class, () -> fixture.call(AegisPackageCredentials.Action.UPDATE));
        assertEquals(2, fixture.checks.get());
    }

    @Test public void applicationProcessCannotUsePublicEntryAndItsCredentialIsWiped() {
        assertNotEquals(Process.SYSTEM_UID, Process.myUid());
        Fixture fixture = new Fixture();
        LockscreenCredential credential = password();
        byte[] bytes = credential.getCredential();
        throwsType(SecurityException.class, () -> fixture.check.verify(10, 42,
                AegisPackageCredentials.Action.INSTALL, credential));
        assertArrayEquals(new byte[4], bytes);
        assertEquals(0, fixture.checks.get());
    }
}
