package org.aegisos.identity;

import static org.junit.Assert.*;
import android.os.RemoteException;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Real admission serialization plus injected native replies/time, no fake
 * authority presented as AOSP authentication or native cleanup proof. */
@RunWith(AndroidJUnit4.class)
public final class RuntimeStartWaiterTest {
    private interface Action { void run() throws Exception; }
    private static <T extends Throwable> T fails(Class<T> type, Action action) throws Exception {
        try { action.run(); } catch (Throwable error) {
            if (type.isInstance(error)) return type.cast(error);
            throw new AssertionError("Unexpected failure", error);
        }
        throw new AssertionError("Expected " + type.getSimpleName());
    }
    private static RuntimeAdmission.Binding fakeLogin(RuntimeAdmission gate) {
        try (RuntimeAdmission.Access access = gate.afterAuthentication(gate.beforeAuthentication(10, 42))) {
            return access.binding();
        }
    }
    @Test public void waitsWithoutHoldingAdmissionAndRechecksEveryContinuation() throws Exception {
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> {});
        RuntimeAdmission.Binding binding = fakeLogin(gate);
        AtomicLong clock = new AtomicLong(100); AtomicInteger sleeps = new AtomicInteger();
        List<Long> jobs = new ArrayList<>();
        RuntimeStartWaiter waiter = new RuntimeStartWaiter(clock::get, nanos -> {
            // Reentrant access would throw if the probe kept the CE gate held.
            try (RuntimeAdmission.Storage storage = gate.storage(10, false)) { sleeps.incrementAndGet(); }
            clock.addAndGet(nanos);
        }, 1_000_000_000L);
        RuntimeBrokerProtocol.StartReply result = waiter.await((job, deadline) -> {
            try (RuntimeAdmission.Access access = gate.existing(binding)) {
                jobs.add(job); assertTrue(deadline > clock.get()); access.checkCurrent();
                return new RuntimeBrokerProtocol.StartReply(17, jobs.size() == 3);
            }
        });
        assertTrue(result.ready); assertEquals(List.of(0L, 17L, 17L), jobs); assertEquals(2, sleeps.get());
    }
    @Test public void logoutDuringWaitSealsTheOriginalBindingBeforeAnotherNativeRequest() throws Exception {
        AtomicInteger stopped = new AtomicInteger();
        RuntimeAdmission gate = new RuntimeAdmission((user, deadline) -> stopped.incrementAndGet());
        RuntimeAdmission.Binding binding = fakeLogin(gate); AtomicInteger nativeCalls = new AtomicInteger();
        AtomicLong clock = new AtomicLong(100);
        RuntimeStartWaiter waiter = new RuntimeStartWaiter(clock::get, nanos -> {
            try (RuntimeAdmission.Storage storage = gate.storage(10, true)) { }
            fakeLogin(gate); // A later authentication cannot revive original binding.
            clock.addAndGet(nanos);
        }, 1_000_000_000L);
        fails(SecurityException.class, () -> waiter.await((job, deadline) -> {
            try (RuntimeAdmission.Access access = gate.existing(binding)) {
                nativeCalls.incrementAndGet(); return new RuntimeBrokerProtocol.StartReply(17, false);
            }
        }));
        assertEquals(1, nativeCalls.get()); assertEquals(3, stopped.get());
    }
    @Test public void staleNativeJobAfterStopIsNeverReplacedWithAnInitialStart() throws Exception {
        AtomicLong clock = new AtomicLong(100); List<Long> jobs = new ArrayList<>();
        RuntimeStartWaiter waiter = new RuntimeStartWaiter(clock::get, clock::addAndGet, 1_000_000_000L);
        fails(IllegalStateException.class, () -> waiter.await((job, deadline) -> {
            jobs.add(job);
            if (job != 0) throw new IllegalStateException("Native ESTALE after STOP/replacement");
            return new RuntimeBrokerProtocol.StartReply(17, false);
        }));
        assertEquals(List.of(0L, 17L), jobs);
    }
    @Test public void changedOrMissingJobCannotBeAcceptedAsCompletion() throws Exception {
        for (long replacement : new long[] {0, 18, -1}) {
            AtomicLong clock = new AtomicLong(100); AtomicInteger calls = new AtomicInteger();
            RuntimeStartWaiter waiter = new RuntimeStartWaiter(clock::get, clock::addAndGet, 1_000_000_000L);
            fails(IllegalStateException.class, () -> waiter.await((job, deadline) ->
                    new RuntimeBrokerProtocol.StartReply(calls.incrementAndGet() == 1 ? 17 : replacement, job != 0)));
            assertEquals(2, calls.get());
        }
    }
    @Test public void totalDeadlineBoundsPendingAndRejectsLateReady() throws Exception {
        AtomicLong clock = new AtomicLong(100); AtomicInteger calls = new AtomicInteger();
        RuntimeStartWaiter waiter = new RuntimeStartWaiter(clock::get, clock::addAndGet, 10);
        fails(IllegalStateException.class, () -> waiter.await((job, deadline) -> {
            calls.incrementAndGet(); return new RuntimeBrokerProtocol.StartReply(17, false);
        }));
        assertEquals(1, calls.get());
        clock.set(100);
        fails(IllegalStateException.class, () -> waiter.await((job, deadline) -> {
            clock.set(deadline); return new RuntimeBrokerProtocol.StartReply(17, true);
        }));
    }
    @Test public void interruptionPreservesCancellationAndDoesNotSendAnotherRequest() throws Exception {
        AtomicInteger calls = new AtomicInteger();
        RuntimeStartWaiter waiter = new RuntimeStartWaiter(() -> 100, nanos -> { throw new InterruptedException(); }, 10);
        try {
            fails(IllegalStateException.class, () -> waiter.await((job, deadline) -> {
                calls.incrementAndGet(); return new RuntimeBrokerProtocol.StartReply(17, false);
            }));
            assertTrue(Thread.currentThread().isInterrupted()); assertEquals(1, calls.get());
        } finally { Thread.interrupted(); }
    }
    @Test public void freshAospFailureIsPropagatedWithoutRetry() throws Exception {
        AtomicInteger calls = new AtomicInteger(); RemoteException expected = new RemoteException();
        assertSame(expected, fails(RemoteException.class, () -> new RuntimeStartWaiter().await((job, deadline) -> {
            calls.incrementAndGet(); throw expected;
        })));
        assertEquals(1, calls.get());
    }
    @Test public void legacyReadyDoesNotInventAJobAndInvalidClockDoesNotCallNative() throws Exception {
        AtomicInteger calls = new AtomicInteger();
        assertEquals(0, new RuntimeStartWaiter().await((job, deadline) -> {
            calls.incrementAndGet(); assertEquals(0, job); return new RuntimeBrokerProtocol.StartReply(0, true);
        }).job);
        assertEquals(1, calls.get());
        for (long bad : new long[] {-1, Long.MAX_VALUE}) {
            fails(IllegalStateException.class, () -> new RuntimeStartWaiter(() -> bad, nanos -> {}, 10).await((job, deadline) -> {
                throw new AssertionError("Native must not be called");
            }));
        }
    }
}
