/*
 * Copyright 2026 The AegisOS Authors
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.server.aegis;

import android.os.Process;
import android.os.SystemProperties;
import android.os.UserHandle;

import java.util.Objects;

/**
 * System-server-only bridge at AOSP's storage-key mutations. No Binder endpoint,
 * personal identity, passwords, keys or authorization tickets live here.
 * The actual native-runtime controller is NOT implemented by this bridge.
 */
public final class AegisRuntimeStorage {
    public enum Operation {
        CREATE(true), DESTROY(true), LOCK(true), UNLOCK(false), PROTECT(false);
        public final boolean requiresQuiescence;
        Operation(boolean requiresQuiescence) { this.requiresQuiescence = requiresQuiescence; }
    }

    public interface Lease extends AutoCloseable {
        /** Release serialization; never reopen runtime admission or authenticate anyone. */
        @Override void close();
    }

    public interface Controller {
        /**
         * Return only after obtaining exclusive per-user storage serialization.
         * CREATE/DESTROY/LOCK must FIRST seal runtime/package admission for ALL
         * contexts carrying this numeric AOSP userId, including obsolete serials,
         * finish/cancel package mutations, kill/reap owned runtime trees and close
         * every CE/mount/PTY/queued-socket reference. Keep admission sealed on
         * success, failure and lease close. Only a later fresh AOSP-authorized
         * reconciliation may reopen it, after checking the actual id+serial/CE.
         *
         * UNLOCK/PROTECT serialize against removal without killing a currently
         * valid runtime or automatically reopening admission. The lease spans
         * vold and AOSP's storage-state update on the SAME calling thread.
         *
         * Bound waits, fail on missing proof, and do not acquire the identity
         * service's operations monitor or call back into storage/LockSettings:
         * its caller may already hold those locks while awaiting this operation.
         * Caller is trusted system-server code, never a CLI-supplied userId.
         */
        Lease begin(int userId, Operation operation);
    }

    private static volatile Controller controller;
    private static final Lease ABSENT = () -> {};
    private AegisRuntimeStorage() {}

    /** Install exactly once before enabling managed runtime admission. */
    public static synchronized void register(Controller value) {
        requireSystemServer();
        if (!"managed-v1".equals(SystemProperties.get("ro.aegis.runtime.mode"))) {
            throw new IllegalStateException("Managed runtime mode is not configured");
        }
        if (controller != null) throw new IllegalStateException("Runtime controller already registered");
        controller = Objects.requireNonNull(value);
    }

    public static Lease begin(int userId, Operation operation) {
        requireSystemServer();
        return begin(userId, operation, SystemProperties.get("ro.aegis.runtime.mode"), controller);
    }

    private static void requireSystemServer() {
        if (Process.myUid() != Process.SYSTEM_UID) {
            throw new SecurityException("Runtime storage bridge requires system-server credentials");
        }
    }

    // Same policy used by production, with isolated inputs for device tests.
    // No product property override, registry reset or Binder-accessible test flag.
    static Lease begin(int userId, Operation operation, String mode, Controller value) {
        if (userId < 0) throw new IllegalArgumentException("Invalid AOSP userId");
        Objects.requireNonNull(operation);
        if (!"absent".equals(mode) && !"managed-v1".equals(mode)) {
            throw new IllegalStateException("Unknown runtime storage mode");
        }
        // AEGIS never creates a personal runtime for the persistent system user.
        if ("absent".equals(mode) || userId == UserHandle.USER_SYSTEM) return ABSENT;
        if (value == null) throw new IllegalStateException("Runtime quiescence controller unavailable");
        Lease acquired = Objects.requireNonNull(value.begin(userId, operation),
                "Runtime storage lease was not confirmed");
        Thread owner = Thread.currentThread();
        return new Lease() {
            private boolean closed;
            @Override public void close() {
                if (Thread.currentThread() != owner) {
                    throw new IllegalStateException("Storage lease belongs to another thread");
                }
                if (closed) throw new IllegalStateException("Storage lease already closed");
                // A failing release is terminal as well; never retry a potentially
                // partially released native lease or claim successful cleanup.
                closed = true;
                acquired.close();
            }
        };
    }
}
