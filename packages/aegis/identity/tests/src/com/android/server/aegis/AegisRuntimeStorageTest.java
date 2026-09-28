package com.android.server.aegis;

import static org.junit.Assert.*;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Isolated bridge policy tests, not vold calls or native-teardown proof. */
@RunWith(AndroidJUnit4.class)
public final class AegisRuntimeStorageTest {
    private static <T extends Throwable> T assertThrows(Class<T> type, Runnable action) {
        try {
            action.run();
        } catch (Throwable error) {
            if (type.isInstance(error)) return type.cast(error);
            throw new AssertionError("Unexpected exception", error);
        }
        throw new AssertionError("Expected " + type.getSimpleName());
    }

    @Test public void absentModeAndSystemUserDoNotInvokeAController() {
        AegisRuntimeStorage.Controller unexpected = (user, operation) -> { throw new AssertionError(); };
        for (AegisRuntimeStorage.Operation op : AegisRuntimeStorage.Operation.values()) {
            AegisRuntimeStorage.begin(10, op, "absent", unexpected).close();
            AegisRuntimeStorage.begin(0, op, "managed-v1", unexpected).close();
        }
    }

    @Test public void unspecifiedModeAndMissingControllerNeverPermitPersonalMutation() {
        for (String mode : new String[] {"", "managed", "unknown"}) {
            assertThrows(IllegalStateException.class, () ->
                    AegisRuntimeStorage.begin(10, AegisRuntimeStorage.Operation.LOCK, mode, null));
        }
        assertThrows(IllegalStateException.class, () -> AegisRuntimeStorage.begin(10,
                AegisRuntimeStorage.Operation.LOCK, "managed-v1", null));
        assertThrows(IllegalArgumentException.class, () -> AegisRuntimeStorage.begin(-1,
                AegisRuntimeStorage.Operation.LOCK, "absent", null));
    }

    @Test public void mutationRunsOnlyAfterAcquisitionAndBeforeRelease() {
        List<String> events = new ArrayList<>();
        AegisRuntimeStorage.Controller controller = (user, operation) -> {
            assertEquals(11, user);
            assertEquals(AegisRuntimeStorage.Operation.DESTROY, operation);
            events.add("quiescence");
            return () -> events.add("release");
        };
        try (AegisRuntimeStorage.Lease lease = AegisRuntimeStorage.begin(11,
                AegisRuntimeStorage.Operation.DESTROY, "managed-v1", controller)) {
            events.add("vold-and-state-update");
        }
        assertEquals(List.of("quiescence", "vold-and-state-update", "release"), events);
    }

    @Test public void failedOrUnconfirmedAcquisitionNeverExecutesMutation() {
        AtomicInteger calls = new AtomicInteger();
        for (boolean missing : new boolean[] {false, true}) {
            assertThrows(RuntimeException.class, () -> {
                try (AegisRuntimeStorage.Lease lease = AegisRuntimeStorage.begin(10,
                        AegisRuntimeStorage.Operation.LOCK, "managed-v1", (user, operation) -> {
                            if (missing) return null;
                            throw new IllegalStateException("teardown not confirmed");
                        })) {
                    calls.incrementAndGet();
                }
            });
        }
        assertEquals(0, calls.get());
    }

    @Test public void voldFailureStillReleasesSerializationAndRemainsAnError() {
        AtomicInteger releases = new AtomicInteger();
        IllegalStateException failure = new IllegalStateException("vold failed");
        RuntimeException caught = assertThrows(RuntimeException.class, () -> {
            try (AegisRuntimeStorage.Lease lease = AegisRuntimeStorage.begin(10,
                    AegisRuntimeStorage.Operation.LOCK, "managed-v1",
                    (user, operation) -> () -> releases.incrementAndGet())) {
                throw failure;
            }
        });
        assertSame(failure, caught);
        assertEquals(1, releases.get());
    }

    @Test public void releaseIsBoundToItsThreadAndCannotBeRetried() throws Exception {
        AtomicInteger releases = new AtomicInteger();
        AegisRuntimeStorage.Lease lease = AegisRuntimeStorage.begin(10,
                AegisRuntimeStorage.Operation.LOCK, "managed-v1",
                (user, operation) -> () -> releases.incrementAndGet());
        AtomicReference<Throwable> rejected = new AtomicReference<>();
        Thread other = new Thread(() -> {
            try { lease.close(); } catch (Throwable error) { rejected.set(error); }
        });
        other.start(); other.join(5000);
        assertFalse(other.isAlive());
        assertTrue(rejected.get() instanceof IllegalStateException);
        assertEquals(0, releases.get());
        lease.close();
        assertThrows(IllegalStateException.class, lease::close);
        assertEquals(1, releases.get());
    }

    @Test public void releaseFailureIsPropagatedAndCannotReportSuccess() {
        AtomicInteger releases = new AtomicInteger();
        AegisRuntimeStorage.Lease lease = AegisRuntimeStorage.begin(10,
                AegisRuntimeStorage.Operation.LOCK, "managed-v1", (user, operation) -> () -> {
                    releases.incrementAndGet();
                    throw new IllegalStateException("lease release unconfirmed");
                });
        assertThrows(IllegalStateException.class, lease::close);
        assertThrows(IllegalStateException.class, lease::close);
        assertEquals(1, releases.get());
    }

    @Test public void protectionAndUnlockDoNotRequestDestructiveQuiescence() {
        for (AegisRuntimeStorage.Operation op : AegisRuntimeStorage.Operation.values()) {
            boolean expected = op == AegisRuntimeStorage.Operation.CREATE
                    || op == AegisRuntimeStorage.Operation.DESTROY || op == AegisRuntimeStorage.Operation.LOCK;
            assertEquals(expected, op.requiresQuiescence);
            try (AegisRuntimeStorage.Lease lease = AegisRuntimeStorage.begin(10, op,
                    "managed-v1", (user, actual) -> {
                        assertSame(op, actual);
                        return () -> {};
                    })) {
                // All five operations acquire a lease, including nondestructive changes.
            }
        }
    }
}
