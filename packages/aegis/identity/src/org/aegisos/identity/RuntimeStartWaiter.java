package org.aegisos.identity;

import android.os.RemoteException;
import java.util.Objects;
import java.util.concurrent.TimeUnit;
import java.util.function.LongSupplier;

/** Wait outside AOSP serialization. Each probe must freshly check AOSP and its
 * original session, then hold admission only across one bounded native call.
 * A positive job can only be continued, never replaced by a new START after
 * STOP/HELLO. Timeout/interruption do not claim native cleanup or key eviction.
 */
final class RuntimeStartWaiter {
    interface Probe {
        RuntimeBrokerProtocol.StartReply step(long job, long deadline) throws RemoteException;
    }
    interface Pause { void sleep(long nanos) throws InterruptedException; }
    private static final long MAX_WAIT = TimeUnit.MINUTES.toNanos(2);
    private static final long INTERVAL = TimeUnit.MILLISECONDS.toNanos(200);
    private final LongSupplier clock;
    private final Pause pause;
    private final long limit;
    RuntimeStartWaiter() { this(System::nanoTime, TimeUnit.NANOSECONDS::sleep, MAX_WAIT); }
    RuntimeStartWaiter(LongSupplier clock, Pause pause, long limit) {
        this.clock = Objects.requireNonNull(clock); this.pause = Objects.requireNonNull(pause);
        if (limit <= 0 || limit > MAX_WAIT) throw new IllegalArgumentException("Invalid start wait bound");
        this.limit = limit;
    }
    RuntimeBrokerProtocol.StartReply await(Probe probe) throws RemoteException {
        Objects.requireNonNull(probe);
        long now = clock.getAsLong();
        if (now < 0 || now > Long.MAX_VALUE - limit) throw new IllegalStateException("Invalid start clock");
        final long deadline = now + limit;
        long job = 0;
        for (;;) {
            remaining(deadline);
            RuntimeBrokerProtocol.StartReply reply = Objects.requireNonNull(probe.step(job, deadline));
            remaining(deadline);
            if (reply.job < 0 || (job != 0 && reply.job != job) || (!reply.ready && reply.job == 0)) {
                throw new IllegalStateException("Runtime startup lost its registered job");
            }
            if (reply.ready) return reply;
            job = reply.job;
            // Probe has closed its admission scope BEFORE any sleep. No retry
            // of exceptions, no implicit new authentication and no new START.
            try { pause.sleep(Math.min(INTERVAL, remaining(deadline))); }
            catch (InterruptedException interrupted) {
                Thread.currentThread().interrupt();
                throw new IllegalStateException("Runtime startup interrupted; cleanup is unconfirmed");
            }
        }
    }
    private long remaining(long deadline) {
        if (Thread.currentThread().isInterrupted())
            throw new IllegalStateException("Runtime startup interrupted; cleanup is unconfirmed");
        long now = clock.getAsLong();
        if (now < 0 || now >= deadline)
            throw new IllegalStateException("Runtime startup timed out; cleanup is unconfirmed");
        return deadline - now;
    }
}
