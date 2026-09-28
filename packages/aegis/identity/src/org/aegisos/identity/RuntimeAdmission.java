package org.aegisos.identity;

import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Internal per-user serialization for the forthcoming managed runtime owner.
 * This is NOT authentication, a Binder interface or proof of native teardown.
 * The caller must use AOSP's real authentication, serial and lifecycle checks.
 * No AOSP/LockSettings call may run while one of these scopes is held.
 */
public final class RuntimeAdmission {
    public interface Quiescer {
        /**
         * Synchronously stop/reap ALL serials for this userId, finish/cancel
         * package work and close every CE reference, including queued FDs.
         * Return only with proof; throw otherwise and retain incomplete work
         * for recovery. Implementations must enforce the monotonic deadline,
         * never call AOSP or wait for work that needs this same gate.
         */
        void stopAndReleaseAll(int userId, long deadlineNanos);
    }

    private static final long DEFAULT_TIMEOUT_NANOS = TimeUnit.SECONDS.toNanos(10);
    private final ConcurrentHashMap<Integer, Slot> slots = new ConcurrentHashMap<>();
    private final ThreadLocal<Slot> held = new ThreadLocal<>();
    private final Quiescer quiescer;
    private final long timeoutNanos;

    private static final class Slot {
        final ReentrantLock lock = new ReentrantLock(true);
        final AtomicLong epoch = new AtomicLong();
        // Read/written only under lock. No persistent authority or personal keys.
        long admittedEpoch = -1;
        int serial = -1;
    }

    public RuntimeAdmission(Quiescer quiescer) {
        this(quiescer, DEFAULT_TIMEOUT_NANOS);
    }

    RuntimeAdmission(Quiescer quiescer, long timeoutNanos) {
        this.quiescer = Objects.requireNonNull(quiescer);
        if (timeoutNanos <= 0 || timeoutNanos > DEFAULT_TIMEOUT_NANOS) {
            throw new IllegalArgumentException("Invalid runtime serialization timeout");
        }
        this.timeoutNanos = timeoutNanos;
    }

    /** Opaque, one-use attempt; mint immediately BEFORE asking AOSP to authenticate. */
    public final class AuthenticationAttempt {
        private final Slot slot;
        private final int userId;
        private final int serial;
        private final long epoch;
        private final AtomicBoolean consumed = new AtomicBoolean();

        private AuthenticationAttempt(int userId, int serial, Slot slot) {
            this.userId = userId;
            this.serial = serial;
            this.slot = slot;
            this.epoch = slot.epoch.get();
        }

        /** Discard on rejected credentials or a failed AOSP lifecycle check. */
        public void discard() { consumed.set(true); }
    }

    public AuthenticationAttempt beforeAuthentication(int userId, int serial) {
        if (serial < 0) throw new IllegalArgumentException("Invalid AOSP serial");
        return new AuthenticationAttempt(userId, serial, slot(userId));
    }

    /**
     * Call ONLY after fresh AOSP authentication and actual user/serial/CE checks.
     * No boolean supplied by a client can satisfy those checks. Serial changes
     * or a sealed slot require native quiescence before any new CE access.
     * Returned scope must cover start preparation and registration with the
     * resource owner; checkCurrent() again before publishing a ready context.
     */
    public Access afterAuthentication(AuthenticationAttempt attempt) {
        Objects.requireNonNull(attempt);
        if (slots.get(attempt.userId) != attempt.slot
                || !attempt.consumed.compareAndSet(false, true)) {
            throw new SecurityException("Authentication attempt is invalid or consumed");
        }
        Slot slot = attempt.slot;
        long deadline = acquire(slot);
        boolean handedOff = false;
        boolean reconciled = false;
        try {
            requireEpoch(slot, attempt.epoch);
            long admittedEpoch = attempt.epoch;
            if (slot.serial != attempt.serial || slot.admittedEpoch != admittedEpoch) {
                // Invalidate concurrent old authentication attempts, including
                // attempts against a previous occupant of this numeric userId.
                admittedEpoch = advance(slot, attempt.epoch);
                reconciled = true;
                slot.admittedEpoch = -1;
                quiesce(attempt.userId, deadline);
                requireEpoch(slot, admittedEpoch);
                slot.serial = attempt.serial;
                slot.admittedEpoch = admittedEpoch;
            }
            Access access = new Access(slot, attempt.userId, attempt.serial, admittedEpoch, deadline);
            access.checkCurrent();
            handedOff = true;
            return access;
        } catch (RuntimeException | Error failure) {
            if (reconciled) {
                slot.admittedEpoch = -1;
                invalidate(slot);
            }
            throw failure;
        } finally {
            if (!handedOff) release(slot);
        }
    }

    /**
     * Existing-session access only, after the caller's AOSP/session/admin checks.
     * Holds serialization for the bounded start/shell/package handoff. Long
     * operations must be owned/cancellable by Quiescer, not hold this scope.
     */
    public Access existing(Binding binding) {
        if (binding == null || slots.get(binding.userId) != binding.slot) {
            throw new SecurityException("Missing or foreign personal runtime binding");
        }
        Slot slot = binding.slot;
        long deadline = acquire(slot);
        boolean handedOff = false;
        try {
            // Use THIS session's authenticated epoch, never the slot's newest
            // admission. Another terminal's later login cannot revive this one.
            Access access = new Access(slot, binding.userId, binding.serial, binding.epoch, deadline);
            access.checkCurrent();
            handedOff = true;
            return access;
        } finally {
            if (!handedOff) release(slot);
        }
    }

    /** Internal, non-parcelable evidence of an admitted authentication, not a client token. */
    public final class Binding {
        private final Slot slot;
        private final int userId, serial;
        private final long epoch;
        private Binding(Slot slot, int userId, int serial, long epoch) {
            this.slot = slot;
            this.userId = userId;
            this.serial = serial;
            this.epoch = epoch;
        }
        public boolean matches(int userId, int serial) {
            return this.userId == userId && this.serial == serial;
        }
    }

    /**
     * Nonblocking revocation for lifecycle callbacks. This alone does not stop
     * processes, complete a user stop or prove CE lock. The owner must schedule
     * teardown and independently prevent admission while AOSP is stopping.
     */
    public void revoke(int userId) { invalidate(slot(userId)); }

    /**
     * Used by the storage-bridge provider, around vold AND AOSP's state update.
     * Destructive entry revokes before waiting, even if acquisition times out.
     * Exit revokes again: authentication begun while a key mutation was pending
     * may not reopen the runtime after the mutation finishes (success or error).
     * Nondestructive unlock/protection still serialize but do not kill sessions.
     */
    public Storage storage(int userId, boolean destructive) {
        Slot slot = slot(userId);
        if (destructive) invalidate(slot);
        boolean acquired = false;
        try {
            long deadline = acquire(slot);
            acquired = true;
            if (destructive) {
                slot.admittedEpoch = -1;
                quiesce(userId, deadline);
            }
            return new Storage(slot, destructive);
        } catch (RuntimeException | Error failure) {
            if (destructive) invalidate(slot);
            if (acquired) release(slot);
            throw failure;
        }
    }

    public abstract class Scope implements AutoCloseable {
        final Slot slot;
        final Thread owner = Thread.currentThread();
        private boolean closed;

        private Scope(Slot slot) { this.slot = slot; }
        final void requireOwner() {
            if (owner != Thread.currentThread() || closed) {
                throw new IllegalStateException("Runtime scope is closed or belongs to another thread");
            }
        }
        abstract void beforeClose();
        @Override public final void close() {
            requireOwner();
            closed = true;
            try { beforeClose(); } finally { release(slot); }
        }
    }

    public final class Access extends Scope {
        private final int userId;
        private final int serial;
        private final long admittedEpoch;
        private final long deadline;
        private Access(Slot slot, int userId, int serial, long admittedEpoch, long deadline) {
            super(slot);
            this.userId = userId;
            this.serial = serial;
            this.admittedEpoch = admittedEpoch;
            this.deadline = deadline;
        }
        /** Publish only after the enclosing service has rechecked its terminal lifetime. */
        public Binding binding() {
            checkCurrent();
            return new Binding(slot, userId, serial, admittedEpoch);
        }
        public void checkCurrent() {
            requireOwner();
            if (serial < 0 || slot.serial != serial || admittedEpoch < 0
                    || slot.admittedEpoch != admittedEpoch) {
                throw new SecurityException("Runtime admission is sealed or belongs to another serial");
            }
            requireEpoch(slot, admittedEpoch);
            requireTime(deadline);
        }
        /** Carry this same deadline into native I/O; never restart the wait budget. */
        public long deadlineNanos() {
            checkCurrent();
            return deadline;
        }
        @Override void beforeClose() { /* Closing a handle never admits anyone. */ }
    }

    public final class Storage extends Scope {
        private final boolean destructive;
        private Storage(Slot slot, boolean destructive) { super(slot); this.destructive = destructive; }
        @Override void beforeClose() { if (destructive) invalidate(slot); }
    }

    private Slot slot(int userId) {
        // The Android system user never owns a personal runtime. This class is
        // not the native UID-range check; the resource owner must also enforce it.
        if (userId <= 0) throw new IllegalArgumentException("Expected a personal AOSP userId");
        return slots.computeIfAbsent(userId, unused -> new Slot());
    }

    private long acquire(Slot slot) {
        if (held.get() != null || slot.lock.isHeldByCurrentThread()) {
            throw new IllegalStateException("Reentrant runtime operations are forbidden");
        }
        long deadline = System.nanoTime() + timeoutNanos;
        try {
            if (!slot.lock.tryLock(timeoutNanos, TimeUnit.NANOSECONDS)) {
                throw new IllegalStateException("Runtime serialization timed out");
            }
        } catch (InterruptedException interrupted) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Runtime serialization interrupted");
        }
        try {
            requireTime(deadline);
            held.set(slot);
        } catch (RuntimeException | Error failure) {
            held.remove();
            slot.lock.unlock();
            throw failure;
        }
        return deadline;
    }

    private void release(Slot slot) {
        if (held.get() != slot) throw new IllegalStateException("Runtime scope ownership changed");
        held.remove();
        slot.lock.unlock();
    }

    private void quiesce(int userId, long deadline) {
        requireTime(deadline);
        quiescer.stopAndReleaseAll(userId, deadline);
        requireTime(deadline);
    }

    private static void requireTime(long deadline) {
        if (deadline - System.nanoTime() <= 0) {
            throw new IllegalStateException("Runtime teardown deadline exceeded");
        }
    }

    private static void requireEpoch(Slot slot, long expected) {
        if (expected < 0 || expected == Long.MAX_VALUE || slot.epoch.get() != expected) {
            throw new SecurityException("Runtime operation was revoked");
        }
    }

    private static long advance(Slot slot, long expected) {
        if (expected < 0 || expected == Long.MAX_VALUE
                || !slot.epoch.compareAndSet(expected, expected + 1)) {
            throw new SecurityException("Runtime operation was revoked");
        }
        return expected + 1;
    }

    private static void invalidate(Slot slot) {
        // Saturation permanently seals the slot; never wrap to a previous epoch.
        slot.epoch.updateAndGet(value -> value == Long.MAX_VALUE ? value : value + 1);
    }
}
