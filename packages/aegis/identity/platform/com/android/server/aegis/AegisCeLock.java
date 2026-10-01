/* Copyright 2026 The AegisOS Authors. SPDX-License-Identifier: Apache-2.0 */
package com.android.server.aegis;

import android.os.ServiceSpecificException;
import android.os.SystemClock;
import android.system.OsConstants;
import java.util.Objects;
import java.util.function.LongSupplier;

/** Waits for the patched vold's confirmed kernel eviction, never a cached CE flag. */
public final class AegisCeLock {
    @FunctionalInterface public interface Attempt { void run() throws Exception; }
    @FunctionalInterface interface Sleep { void run(long millis) throws InterruptedException; }
    private AegisCeLock() {}

    public static void complete(Attempt attempt) throws Exception {
        complete(attempt, SystemClock::elapsedRealtime, Thread::sleep, 10_000);
    }

    // Device tests use the same retry policy with isolated callbacks and a fake clock.
    static void complete(Attempt attempt, LongSupplier clock, Sleep sleep, long timeout)
            throws Exception {
        Objects.requireNonNull(attempt); Objects.requireNonNull(clock); Objects.requireNonNull(sleep);
        if (timeout <= 0 || timeout > 10_000) throw new IllegalArgumentException("Invalid CE lock deadline");
        long started = clock.getAsLong();
        for (;;) {
            if (Thread.currentThread().isInterrupted()) throw new InterruptedException("CE lock interrupted");
            long elapsed = clock.getAsLong() - started;
            if (elapsed < 0 || elapsed >= timeout) throw new IllegalStateException("CE key eviction remains pending");
            try {
                attempt.run();
                return; // Only patched vold's authoritative ABSENT confirmation succeeds.
            } catch (ServiceSpecificException pending) {
                if (pending.errorCode != OsConstants.EBUSY) throw pending;
            }
            long remaining = timeout - (clock.getAsLong() - started);
            if (remaining <= 0) throw new IllegalStateException("CE key eviction remains pending");
            try { sleep.run(Math.min(100, remaining)); }
            catch (InterruptedException interrupted) {
                Thread.currentThread().interrupt();
                throw interrupted;
            }
        }
    }
}
