package org.aegisos.identity;

import android.os.RemoteException;
import com.android.internal.widget.LockscreenCredential;
import com.android.internal.widget.VerifyCredentialResponse;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Internal one-shot approval and bounded handoff of an already prepared plan.
 * Not a Binder endpoint, package resolver, credential cache or worker owner.
 * The actual installer/CLI is not yet connected. Identifiers and plan hashes
 * alone grant no authority; only the enclosing authenticated session may call.
 */
public final class PackageApproval {
    public enum Scope {
        USER, ALL;
        public static Scope fromArgument(String value) {
            if ("user".equals(value)) return USER;
            if ("all".equals(value)) return ALL;
            throw new IllegalArgumentException("Explicit package scope user or all required");
        }
    }
    public enum Action { INSTALL, UPDATE, REMOVE }

    /**
     * Create from the authenticated REQUESTER and a trusted native planner's
     * immutable job/plan, before asking an administrator to confirm it.
     * The native planner must bind this digest to the complete resolved action,
     * versions/dependencies, previous generations and private-removal effects.
     * No caller-provided target, host pathname or file descriptor belongs here.
     */
    public static final class Prepared {
        public final AospIdentityBackend.UserKey requester;
        public final Scope scope;
        public final Action action;
        public final long job;
        public final String planSha256;
        private final AtomicBoolean claimed = new AtomicBoolean();

        public Prepared(AospIdentityBackend.UserKey requester, Scope scope, Action action,
                long job, String planSha256) {
            this.requester = Objects.requireNonNull(requester);
            this.scope = Objects.requireNonNull(scope);
            this.action = Objects.requireNonNull(action);
            if (requester.id < 10 || requester.id >= 21473 || job <= 0
                    || planSha256 == null || !planSha256.matches("[0-9a-f]{64}")) {
                throw new IllegalArgumentException("Invalid prepared package plan");
            }
            this.job = job;
            this.planSha256 = planSha256;
        }

        /** Shared changes have no private target; the admin is never a target selector. */
        public AospIdentityBackend.UserKey privateOwner() {
            return scope == Scope.USER ? requester : null;
        }

        private void claim() {
            if (!claimed.compareAndSet(false, true)) {
                throw new SecurityException("Package confirmation already attempted");
            }
        }
    }

    public interface Authority {
        /** Fresh AOSP serial/enabled/running/CE and action restrictions of the REQUESTER. */
        void requireRequester(Prepared plan) throws RemoteException;
        /** Fresh AOSP admin/password verification for EVERY operation; never session-only. */
        VerifyCredentialResponse confirm(AospIdentityBackend.UserKey administrator,
                Action action, LockscreenCredential credential) throws RemoteException;
    }

    public interface Handoff {
        /**
         * Short registration/start only, with the exact requester/serial/scope,
         * immutable job and plan digest rechecked by the native owner. Must
         * acquire the requester's existing RuntimeAdmission gate, recheck its
         * session guard under that gate, and register cancellation/CE ownership
         * BEFORE releasing the gate. Do not wait for APT, copy or hash here.
         * Any allocated resources remain owned on exceptions or disconnect.
         */
        void start(Prepared plan, AospIdentityBackend.OperationGuard guard) throws RemoteException;
        /**
         * Seal/request cancellation of this same owned job, including a partial
         * start. Not proof of worker exit, unmount, rollback or CE closure.
         * The registered owner must keep teardown responsibility until confirmed.
         */
        void cancel(Prepared plan) throws RemoteException;
    }

    public static final class RejectedCredential extends SecurityException {
        public final int retryAfterMs;
        private RejectedCredential(int retryAfterMs) {
            super("AOSP rejected package confirmation");
            this.retryAfterMs = retryAfterMs;
        }
    }

    /** A handoff may have begun; callers must query its actual state, not claim no change. */
    public static final class UnconfirmedHandoff extends IllegalStateException {
        private UnconfirmedHandoff() { super("Package handoff was not confirmed; inspect job state"); }
    }

    private final Authority authority;
    public PackageApproval(Authority authority) { this.authority = Objects.requireNonNull(authority); }

    /**
     * Consumes the supplied credential even on early rejection. Never returns a
     * reusable approval. Normal return means bounded HANDOFF only, not installed
     * packages or publication. A failed attempt consumes its preparation too.
     * No AOSP call runs inside RuntimeAdmission: only Handoff owns that scope.
     */
    public void confirmAndStart(Prepared plan, AospIdentityBackend.UserKey administrator,
            LockscreenCredential credential, AospIdentityBackend.OperationGuard guard,
            Handoff handoff) throws RemoteException {
        try (LockscreenCredential owned = Objects.requireNonNull(credential)) {
            Objects.requireNonNull(plan);
            Objects.requireNonNull(administrator);
            Objects.requireNonNull(guard);
            Objects.requireNonNull(handoff);
            // A concurrent losing attempt MUST NOT cancel the winning attempt's job.
            plan.claim();
            boolean startAttempted = false;
            try {
                if (!owned.isPassword()) throw new IllegalArgumentException("Password required");
                owned.validateBasicRequirements();
                guard.check();
                authority.requireRequester(plan);
                guard.check();
                VerifyCredentialResponse response = Objects.requireNonNull(
                        authority.confirm(administrator, plan.action, owned));
                if (response.getResponseCode() != VerifyCredentialResponse.RESPONSE_OK) {
                    int delay = response.getResponseCode() == VerifyCredentialResponse.RESPONSE_RETRY
                            ? response.getTimeout() : 0;
                    throw new RejectedCredential(Math.max(0, delay));
                }
                guard.check();
                authority.requireRequester(plan);
                guard.check();
                startAttempted = true;
                handoff.start(plan, guard);
                guard.check();
            } catch (RemoteException | RuntimeException failure) {
                // Release the prepared plan even if its password was rejected.
                // A failed start reply may follow registration or execution.
                boolean cancellationRequested = false;
                try { handoff.cancel(plan); cancellationRequested = true; }
                catch (RemoteException | RuntimeException cleanup) {
                    // No exception text, credentials or private paths are exported.
                    // The worker owner still has the failed cleanup obligation.
                }
                if (startAttempted || !cancellationRequested) throw new UnconfirmedHandoff();
                throw failure;
            }
        }
    }
}
