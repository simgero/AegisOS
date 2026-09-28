package org.aegisos.identity;

import static org.junit.Assert.*;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Device-side concurrency tests using a fake quiescer; no AOSP/native proof. */
@RunWith(AndroidJUnit4.class)
public final class RuntimeAdmissionTest {
    private static <T extends Throwable> T fails(Class<T> type, Runnable action) {
        try { action.run(); } catch (Throwable error) {
            if (type.isInstance(error)) return type.cast(error);
            throw new AssertionError("Unexpected error", error);
        }
        throw new AssertionError("Expected " + type.getSimpleName());
    }

    private static void check(RuntimeAdmission.Access access) {
        try (RuntimeAdmission.Access owned = access) { owned.checkCurrent(); }
    }

    private static void fakeLogin(RuntimeAdmission gate, int user, int serial) {
        // The real caller must authenticate/check the user in AOSP BETWEEN these
        // calls. This fixture exercises only internal serialization and epochs.
        check(gate.afterAuthentication(gate.beforeAuthentication(user, serial)));
    }

    @Test public void nativeDeadlineIsTheSameOwnedAdmissionBudget() {
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> {});
        RuntimeAdmission.Access access = gate.afterAuthentication(gate.beforeAuthentication(10, 100));
        long deadline = access.deadlineNanos();
        assertTrue(deadline > System.nanoTime());
        assertEquals(deadline, access.deadlineNanos());
        access.close();
        fails(IllegalStateException.class, access::deadlineNanos);
    }

    @Test public void initialAdmissionAndDestructiveStorageRequireQuiescence() {
        List<Integer> stopped = new ArrayList<>();
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> stopped.add(user));
        fails(SecurityException.class, () -> check(gate.existing(10, 100)));
        fakeLogin(gate, 10, 100);
        check(gate.existing(10, 100));
        try (RuntimeAdmission.Storage storage = gate.storage(10, false)) {
            assertEquals(List.of(10), stopped);
        }
        check(gate.existing(10, 100));
        try (RuntimeAdmission.Storage storage = gate.storage(10, true)) {
            assertEquals(List.of(10, 10), stopped);
        }
        fails(SecurityException.class, () -> check(gate.existing(10, 100)));
        fakeLogin(gate, 10, 100);
        assertEquals(List.of(10, 10, 10), stopped);
    }

    @Test public void attemptsAreOneUseDiscardableAndBoundToTheirGate() {
        RuntimeAdmission first = new RuntimeAdmission((user, deadline) -> {});
        RuntimeAdmission other = new RuntimeAdmission((user, deadline) -> {});
        RuntimeAdmission.AuthenticationAttempt attempt = first.beforeAuthentication(10, 100);
        fails(SecurityException.class, () -> check(other.afterAuthentication(attempt)));
        check(first.afterAuthentication(attempt));
        fails(SecurityException.class, () -> check(first.afterAuthentication(attempt)));
        RuntimeAdmission.AuthenticationAttempt discarded = first.beforeAuthentication(10, 100);
        discarded.discard();
        fails(SecurityException.class, () -> check(first.afterAuthentication(discarded)));
    }

    @Test public void attemptsBeforeAndDuringKeyMutationCannotReopenAfterward() {
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> {});
        fakeLogin(gate, 10, 100);
        RuntimeAdmission.AuthenticationAttempt before = gate.beforeAuthentication(10, 100);
        RuntimeAdmission.AuthenticationAttempt during;
        try (RuntimeAdmission.Storage storage = gate.storage(10, true)) {
            during = gate.beforeAuthentication(10, 100);
        }
        fails(SecurityException.class, () -> check(gate.afterAuthentication(before)));
        fails(SecurityException.class, () -> check(gate.afterAuthentication(during)));
        fakeLogin(gate, 10, 100); // Only an attempt begun after completion is eligible.
    }

    @Test public void failedKeyMutationAlsoInvalidatesAttemptsMadeInsideIt() {
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> {});
        fakeLogin(gate, 10, 100);
        AtomicReference<RuntimeAdmission.AuthenticationAttempt> attempt = new AtomicReference<>();
        IllegalStateException failure = new IllegalStateException("fake vold failure");
        assertSame(failure, fails(IllegalStateException.class, () -> {
            try (RuntimeAdmission.Storage storage = gate.storage(10, true)) {
                attempt.set(gate.beforeAuthentication(10, 100));
                throw failure;
            }
        }));
        fails(SecurityException.class, () -> check(gate.afterAuthentication(attempt.get())));
        fails(SecurityException.class, () -> check(gate.existing(10, 100)));
    }

    @Test public void timedOutDestructionSealsAnAlreadyHeldAccessWithoutCallingQuiescer()
            throws Exception {
        AtomicInteger stops = new AtomicInteger();
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> stops.incrementAndGet(),
                TimeUnit.MILLISECONDS.toNanos(200));
        fakeLogin(gate, 10, 100);
        AtomicReference<Throwable> result = new AtomicReference<>();
        try (RuntimeAdmission.Access access = gate.existing(10, 100)) {
            Thread waiting = new Thread(() -> {
                try (RuntimeAdmission.Storage storage = gate.storage(10, true)) {
                    result.set(new AssertionError("acquired while another thread owns access"));
                } catch (Throwable failure) { result.set(failure); }
            });
            waiting.start(); waiting.join(3000);
            assertFalse(waiting.isAlive());
            assertTrue(result.get() instanceof IllegalStateException);
            fails(SecurityException.class, access::checkCurrent);
            assertEquals(1, stops.get()); // Only the earlier fake login drained.
        }
        fails(SecurityException.class, () -> check(gate.existing(10, 100)));
        try (RuntimeAdmission.Storage storage = gate.storage(10, true)) {
            assertEquals(2, stops.get()); // Explicit retry can now obtain exclusivity.
        }
    }

    @Test public void interruptedDestructionPreservesInterruptAndLeavesAdmissionSealed()
            throws Exception {
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> {});
        fakeLogin(gate, 10, 100);
        CountDownLatch entered = new CountDownLatch(1);
        AtomicBoolean interrupted = new AtomicBoolean();
        AtomicReference<Throwable> result = new AtomicReference<>();
        try (RuntimeAdmission.Access access = gate.existing(10, 100)) {
            Thread waiting = new Thread(() -> {
                entered.countDown();
                try (RuntimeAdmission.Storage storage = gate.storage(10, true)) {
                    result.set(new AssertionError("unexpected acquisition"));
                } catch (Throwable failure) {
                    result.set(failure);
                    interrupted.set(Thread.currentThread().isInterrupted());
                }
            });
            waiting.start();
            assertTrue(entered.await(3, TimeUnit.SECONDS));
            waiting.interrupt(); waiting.join(3000);
            assertFalse(waiting.isAlive());
            assertTrue(result.get() instanceof IllegalStateException);
            assertTrue(interrupted.get());
            fails(SecurityException.class, access::checkCurrent);
        }
    }

    @Test public void quiescerFailureRetainsSealingAndDoesNotReuseAuthentication() {
        AtomicBoolean broken = new AtomicBoolean();
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> {
            if (broken.get()) throw new IllegalStateException("owned descriptors remain");
        });
        fakeLogin(gate, 10, 100);
        broken.set(true);
        fails(IllegalStateException.class, () -> {
            try (RuntimeAdmission.Storage storage = gate.storage(10, true)) {
                fail("storage mutation must not be reached");
            }
        });
        RuntimeAdmission.AuthenticationAttempt attempt = gate.beforeAuthentication(10, 100);
        RuntimeAdmission.AuthenticationAttempt concurrent = gate.beforeAuthentication(10, 100);
        fails(IllegalStateException.class, () -> check(gate.afterAuthentication(attempt)));
        broken.set(false);
        fails(SecurityException.class, () -> check(gate.afterAuthentication(attempt)));
        fails(SecurityException.class, () -> check(gate.afterAuthentication(concurrent)));
        fails(SecurityException.class, () -> check(gate.existing(10, 100)));
        fakeLogin(gate, 10, 100);
    }

    @Test public void serialReplacementDrainsOldOwnershipAndDoesNotAffectAnotherUser() {
        List<Integer> stopped = new ArrayList<>();
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> stopped.add(user));
        fakeLogin(gate, 10, 100);
        fakeLogin(gate, 11, 101);
        RuntimeAdmission.AuthenticationAttempt old = gate.beforeAuthentication(10, 100);
        fakeLogin(gate, 10, 102);
        assertEquals(List.of(10, 11, 10), stopped);
        fails(SecurityException.class, () -> check(gate.afterAuthentication(old)));
        fails(SecurityException.class, () -> check(gate.existing(10, 100)));
        check(gate.existing(10, 102));
        check(gate.existing(11, 101));
        gate.revoke(10);
        fails(SecurityException.class, () -> check(gate.existing(10, 102)));
        check(gate.existing(11, 101));
    }

    @Test public void scopesRejectAnotherThreadDoubleCloseAndRecursiveEntry() throws Exception {
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> {});
        fakeLogin(gate, 10, 100);
        RuntimeAdmission.Access access = gate.existing(10, 100);
        AtomicReference<Throwable> failure = new AtomicReference<>();
        Thread other = new Thread(() -> {
            try { access.close(); } catch (Throwable error) { failure.set(error); }
        });
        other.start(); other.join(3000);
        assertFalse(other.isAlive());
        assertTrue(failure.get() instanceof IllegalStateException);
        access.checkCurrent();
        fails(IllegalStateException.class, () -> check(gate.existing(10, 100)));
        fails(IllegalStateException.class, () -> check(gate.existing(11, 101)));
        access.close();
        fails(IllegalStateException.class, access::close);
        fails(IllegalStateException.class, access::checkCurrent);
        check(gate.existing(10, 100));
    }

    @Test public void quiescerCannotReenterTheGateAndAuthorizeItsOwnResources() {
        AtomicReference<RuntimeAdmission> ref = new AtomicReference<>();
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) ->
                check(ref.get().existing(user, 100)));
        ref.set(gate);
        fails(IllegalStateException.class, () -> fakeLogin(gate, 10, 100));
        fails(SecurityException.class, () -> check(gate.existing(10, 100)));
    }

    @Test public void anotherUserCanAcquireOnAnotherThreadWhileThisUserIsHeld() throws Exception {
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> {});
        fakeLogin(gate, 10, 100);
        fakeLogin(gate, 11, 101);
        AtomicReference<Throwable> failure = new AtomicReference<>();
        AtomicBoolean completed = new AtomicBoolean();
        try (RuntimeAdmission.Access access = gate.existing(10, 100)) {
            Thread other = new Thread(() -> {
                try {
                    check(gate.existing(11, 101));
                    completed.set(true);
                } catch (Throwable error) { failure.set(error); }
            });
            other.start(); other.join(3000);
            assertFalse(other.isAlive());
            assertNull(failure.get());
            assertTrue(completed.get());
            access.checkCurrent();
        }
    }

    @Test public void systemUserInvalidSerialAndNonpositiveTimeoutAreRejected() {
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> fail("unexpected quiescer"));
        fails(IllegalArgumentException.class, () -> gate.beforeAuthentication(0, 0));
        fails(IllegalArgumentException.class, () -> gate.beforeAuthentication(10, -1));
        fails(IllegalArgumentException.class, () -> gate.revoke(-1));
        fails(IllegalArgumentException.class, () -> new RuntimeAdmission((user, deadline) -> {}, 0));
    }
}
