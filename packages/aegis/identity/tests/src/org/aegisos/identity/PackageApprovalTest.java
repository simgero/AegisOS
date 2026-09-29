package org.aegisos.identity;

import static org.junit.Assert.*;

import android.os.RemoteException;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.android.internal.widget.LockscreenCredential;
import com.android.internal.widget.VerifyCredentialResponse;
import java.nio.CharBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Isolated coordinator fixtures; no AOSP authentication, APT or CE teardown claim. */
@RunWith(AndroidJUnit4.class)
public final class PackageApprovalTest {
    private static final AospIdentityBackend.UserKey REQUESTER = new AospIdentityBackend.UserKey(11, 43);
    private static final AospIdentityBackend.UserKey ADMIN = new AospIdentityBackend.UserKey(10, 42);
    private static final String DIGEST = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    private interface Checked { void run() throws Exception; }
    private static <T extends Throwable> T rejected(Class<T> expected, Checked operation) {
        try { operation.run(); }
        catch (Throwable failure) {
            if (expected.isInstance(failure)) return expected.cast(failure);
            throw new AssertionError("Unexpected failure", failure);
        }
        throw new AssertionError("Expected " + expected.getSimpleName());
    }
    private static LockscreenCredential password() {
        return LockscreenCredential.createPassword(CharBuffer.wrap(new char[]{'t','e','s','t'}));
    }
    private static PackageApproval.Prepared plan(PackageApproval.Scope scope, PackageApproval.Action action, long job) {
        return new PackageApproval.Prepared(REQUESTER, scope, action, job, DIGEST);
    }
    private static final class Fixture implements PackageApproval.Authority, PackageApproval.Handoff {
        final PackageApproval approval = new PackageApproval(this);
        final PackageApproval.Prepared plan = plan(PackageApproval.Scope.USER, PackageApproval.Action.INSTALL, 23);
        volatile boolean live = true;
        boolean requesterAllowed = true, failCancel, failStart, failVerify;
        int requesterChecks, verifications, starts, cancellations;
        PackageApproval.Prepared started, cancelled;
        Runnable verifying = () -> {}, starting = () -> {};
        VerifyCredentialResponse response = VerifyCredentialResponse.OK;
        final AospIdentityBackend.OperationGuard guard = () -> {
            if (!live) throw new SecurityException("Fixture session revoked");
        };
        @Override public void requireRequester(PackageApproval.Prepared request) {
            assertEquals(REQUESTER, request.requester);
            requesterChecks++;
            if (!requesterAllowed) throw new SecurityException("Requester CE/restriction fixture changed");
        }
        @Override public VerifyCredentialResponse confirm(AospIdentityBackend.UserKey admin,
                PackageApproval.Action action, LockscreenCredential credential) throws RemoteException {
            assertEquals(ADMIN, admin);
            assertTrue(credential.isPassword());
            verifications++;
            if (failVerify) throw new RemoteException("Unavailable verifier fixture");
            verifying.run();
            return response;
        }
        @Override public void start(PackageApproval.Prepared request, AospIdentityBackend.OperationGuard current) {
            current.check(); // Real owner must do this under RuntimeAdmission.
            started = request;
            starts++;
            starting.run();
            if (failStart) throw new IllegalStateException("Ambiguous native acknowledgement fixture");
        }
        @Override public void cancel(PackageApproval.Prepared request) {
            cancelled = request;
            cancellations++;
            if (failCancel) throw new IllegalStateException("Unconfirmed native cancellation fixture");
        }
        void run(PackageApproval.Prepared request) throws RemoteException {
            approval.confirmAndStart(request, ADMIN, password(), guard, this);
        }
        void run() throws RemoteException { run(plan); }
    }

    @Test public void anotherAdminCannotBecomeThePrivatePackageOwner() throws Exception {
        Fixture f = new Fixture();
        f.run();
        assertSame(f.plan, f.started);
        assertSame(REQUESTER, f.started.privateOwner());
        assertNotEquals(ADMIN, f.started.privateOwner());
        assertEquals(23, f.started.job);
        assertEquals(DIGEST, f.started.planSha256);
        assertEquals(2, f.requesterChecks);
        assertEquals(1, f.verifications);
        assertEquals(0, f.cancellations);
    }

    @Test public void sharedActionRetainsRequesterLifetimeWithoutCreatingAPrivateAdminTarget() throws Exception {
        Fixture f = new Fixture();
        PackageApproval.Prepared request = plan(PackageApproval.Scope.ALL, PackageApproval.Action.REMOVE, 24);
        f.run(request);
        assertSame(request, f.started);
        assertNull(f.started.privateOwner());
        assertSame(REQUESTER, f.started.requester);
    }

    @Test public void everyActionInBothScopesRequiresFreshConfirmation() throws Exception {
        Fixture f = new Fixture();
        long job = 1;
        for (PackageApproval.Scope scope : PackageApproval.Scope.values()) {
            for (PackageApproval.Action action : PackageApproval.Action.values()) {
                f.run(plan(scope, action, job++));
            }
        }
        assertEquals(6, f.verifications);
        assertEquals(6, f.starts);
    }

    @Test public void wrongPasswordCancelsOnlyItsPreparedJobAndWipesInput() {
        Fixture f = new Fixture();
        f.response = VerifyCredentialResponse.ERROR;
        LockscreenCredential credential = password();
        byte[] bytes = credential.getCredential();
        rejected(PackageApproval.RejectedCredential.class, () -> f.approval.confirmAndStart(
                f.plan, ADMIN, credential, f.guard, f));
        assertEquals(0, f.starts);
        assertEquals(1, f.cancellations);
        assertSame(f.plan, f.cancelled);
        assertArrayEquals(new byte[4], bytes);
    }

    @Test public void retryCarriesAospDelayAndNeverStartsWork() {
        Fixture f = new Fixture();
        f.response = VerifyCredentialResponse.fromTimeout(30000);
        PackageApproval.RejectedCredential error = rejected(PackageApproval.RejectedCredential.class, f::run);
        assertEquals(30000, error.retryAfterMs);
        assertEquals(0, f.starts);
        assertSame(f.plan, f.cancelled);
    }

    @Test public void revokedSessionCannotEvenBeginCredentialVerification() {
        Fixture f = new Fixture(); f.live = false;
        rejected(SecurityException.class, f::run);
        assertEquals(0, f.verifications);
        assertEquals(0, f.starts);
        assertSame(f.plan, f.cancelled);
    }

    @Test public void logoutDuringPasswordCheckPreventsHandoff() {
        Fixture f = new Fixture();
        f.verifying = () -> f.live = false;
        rejected(SecurityException.class, f::run);
        assertEquals(1, f.verifications);
        assertEquals(0, f.starts);
        assertSame(f.plan, f.cancelled);
    }

    @Test public void requesterCeOrPolicyChangeIsCheckedAgainAfterConfirmation() {
        Fixture f = new Fixture();
        f.verifying = () -> f.requesterAllowed = false;
        rejected(SecurityException.class, f::run);
        assertEquals(2, f.requesterChecks);
        assertEquals(0, f.starts);
        assertSame(f.plan, f.cancelled);
    }

    @Test public void revocationAfterNativeStartIsUnconfirmedAndCancelsThatExactJob() {
        Fixture f = new Fixture();
        f.starting = () -> f.live = false;
        rejected(PackageApproval.UnconfirmedHandoff.class, f::run);
        assertEquals(1, f.starts);
        assertSame(f.started, f.cancelled);
    }

    @Test public void failedAcknowledgementAndFailedCancellationNeverReportCleanFailure() {
        Fixture f = new Fixture(); f.failStart = true; f.failCancel = true;
        rejected(PackageApproval.UnconfirmedHandoff.class, f::run);
        assertEquals(1, f.starts);
        assertEquals(1, f.cancellations);
        assertSame(f.plan, f.cancelled);
    }

    @Test public void rejectedPreparationCannotBeRetriedWithAnotherPassword() {
        Fixture f = new Fixture(); f.response = VerifyCredentialResponse.ERROR;
        rejected(PackageApproval.RejectedCredential.class, f::run);
        f.response = VerifyCredentialResponse.OK;
        rejected(SecurityException.class, f::run);
        assertEquals(1, f.verifications);
        assertEquals(1, f.cancellations);
        assertEquals(0, f.starts);
    }

    @Test public void concurrentLosingAttemptCannotCancelTheWinningConfirmation() throws Exception {
        Fixture f = new Fixture();
        CountDownLatch entered = new CountDownLatch(1), proceed = new CountDownLatch(1);
        ExecutorService worker = Executors.newSingleThreadExecutor();
        f.verifying = () -> {
            entered.countDown();
            try { if (!proceed.await(5, TimeUnit.SECONDS)) throw new AssertionError("No test release"); }
            catch (InterruptedException failure) { throw new AssertionError(failure); }
        };
        try {
            Future<?> winning = worker.submit(() -> {
                try { f.run(); } catch (RemoteException failure) { throw new AssertionError(failure); }
            });
            assertTrue(entered.await(5, TimeUnit.SECONDS));
            LockscreenCredential duplicate = password(); byte[] bytes = duplicate.getCredential();
            rejected(SecurityException.class, () -> f.approval.confirmAndStart(f.plan, ADMIN,
                    duplicate, f.guard, f));
            assertArrayEquals(new byte[4], bytes);
            assertEquals(0, f.cancellations);
            proceed.countDown(); winning.get(5, TimeUnit.SECONDS);
            assertEquals(1, f.verifications);
            assertEquals(1, f.starts);
            assertEquals(0, f.cancellations);
        } finally {
            proceed.countDown(); worker.shutdownNow();
            assertTrue(worker.awaitTermination(5, TimeUnit.SECONDS));
        }
    }

    @Test public void absentScopeOrMalformedPlanIsNotDefaultedOrReinterpreted() {
        for (String scope : new String[]{null, "", "ALL", " user", "user|all"}) {
            rejected(IllegalArgumentException.class, () -> PackageApproval.Scope.fromArgument(scope));
        }
        assertEquals(PackageApproval.Scope.USER, PackageApproval.Scope.fromArgument("user"));
        assertEquals(PackageApproval.Scope.ALL, PackageApproval.Scope.fromArgument("all"));
        for (String digest : new String[]{null, "", "../image", DIGEST + "\n"}) {
            rejected(IllegalArgumentException.class, () -> new PackageApproval.Prepared(REQUESTER,
                    PackageApproval.Scope.USER, PackageApproval.Action.INSTALL, 2, digest));
        }
        rejected(IllegalArgumentException.class, () -> plan(PackageApproval.Scope.USER,
                PackageApproval.Action.UPDATE, 0));
    }

    @Test public void verifierFailureHasNoFallbackToCachedSessionAuthority() {
        Fixture f = new Fixture(); f.failVerify = true;
        rejected(RemoteException.class, f::run);
        assertEquals(0, f.starts);
        assertEquals(1, f.cancellations);
    }
}
