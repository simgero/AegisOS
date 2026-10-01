package org.aegisos.identity;

import android.os.Bundle;
import android.system.OsConstants;
import java.util.ArrayList;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.LongSupplier;

/** One registered job, including uncertain replies and cleanup after its session dies. */
final class PackageTransaction {
    interface Channel {
        PackageBrokerProtocol.Reply call(int operation, long job,
                PackageBrokerProtocol.Intent intent, String digest, long deadline);
    }
    private final AospIdentityBackend.UserKey requester;
    private final PackageBrokerProtocol.Intent intent;
    private final Channel channel;
    private final LongSupplier seconds;
    private final AtomicBoolean sealed = new AtomicBoolean();
    private final AtomicBoolean retired = new AtomicBoolean();
    private long job;
    private boolean begun, handedOff;
    private PackageBrokerProtocol.Metadata review, terminal;
    private PackageApproval.Prepared prepared;
    private int phase = PackageBrokerProtocol.SELECTING;

    PackageTransaction(AospIdentityBackend.UserKey requester, PackageBrokerProtocol.Intent intent,
            Channel channel, LongSupplier seconds) {
        this.requester = Objects.requireNonNull(requester);
        this.intent = Objects.requireNonNull(intent);
        this.channel = Objects.requireNonNull(channel);
        this.seconds = Objects.requireNonNull(seconds);
    }
    PackageApproval.Action action() { return PackageApproval.Action.values()[intent.action - 1]; }
    boolean retired() { return retired.get(); }
    boolean sealed() { return sealed.get(); }
    // These callbacks may run under AOSP storage locks: never acquire this object's monitor.
    void seal() { sealed.set(true); }
    void stopped() { seal(); retired.set(true); }
    private void requireOpen() {
        if (sealed.get() || retired.get()) throw new SecurityException("Package job is sealed");
    }
    private void requireFreshReview() {
        if (review == null || (review.validUntil != 0 && seconds.getAsLong() >= review.validUntil)) {
            seal();
            throw new IllegalStateException("Package plan expired or unavailable");
        }
    }
    private PackageBrokerProtocol.Reply exchange(int operation, long deadline) {
        return channel.call(operation, job, operation == PackageBrokerProtocol.BEGIN ? intent : null,
                operation == PackageBrokerProtocol.START ? review.digest : null, deadline);
    }
    private PackageBrokerProtocol.Reply success(int operation, long deadline) {
        requireOpen();
        PackageBrokerProtocol.Reply reply = exchange(operation, deadline);
        if (reply.error != 0) throw new IllegalStateException("Package operation unconfirmed");
        return reply;
    }
    // Caller registers this placeholder globally BEFORE BEGIN, inside admission.
    synchronized void begin(long deadline) {
        requireOpen();
        if (begun) throw new IllegalStateException("Package BEGIN cannot be replayed");
        begun = true;
        try {
            PackageBrokerProtocol.Reply reply = exchange(PackageBrokerProtocol.BEGIN, deadline);
            job = reply.job; // Including a failed BEGIN's partially allocated job.
            if (reply.error != 0) {
                if (job == 0) retired.set(true); // Well-formed rejection before allocation.
                throw new IllegalStateException("Package BEGIN unconfirmed");
            }
        } catch (RuntimeException failure) { seal(); throw failure; }
        // A lost BEGIN reply leaves job=0 owned until a confirmed whole-user STOP.
    }
    synchronized Bundle poll(long deadline) {
        if (terminal != null) return view(false);
        requireOpen();
        if (!begun || job == 0) throw new IllegalStateException("Missing owned job");
        try {
            PackageBrokerProtocol.Metadata value = success(PackageBrokerProtocol.STATUS, deadline).metadata;
            value.requireIntent(intent);
            if (review != null && !review.digest.equals(value.digest)) {
                throw new IllegalStateException("Package plan changed");
            }
            phase = value.phase;
            switch (phase) {
                case PackageBrokerProtocol.COMPLETE:
                    if (value.outcome == PackageBrokerProtocol.PUBLISHED && !handedOff) {
                        throw new IllegalStateException("Publication without approval handoff");
                    }
                    terminal = value;
                    retired.set(true);
                    break;
                case PackageBrokerProtocol.SELECTED:
                    success(PackageBrokerProtocol.PLAN, deadline);
                    break;
                case PackageBrokerProtocol.COLLECTED:
                case PackageBrokerProtocol.REVIEWED:
                    if (review == null) {
                        review = success(PackageBrokerProtocol.REVIEW, deadline).metadata;
                        review.requireIntent(intent);
                        requireFreshReview();
                    } else {
                        requireFreshReview();
                        success(PackageBrokerProtocol.PREPARE, deadline);
                    }
                    break;
                case PackageBrokerProtocol.PREPARED:
                    requireFreshReview();
                    if (handedOff) throw new IllegalStateException("Started job regressed");
                    if (prepared == null) prepared = new PackageApproval.Prepared(requester,
                            intent.scope == PackageBrokerProtocol.PERSONAL
                                    ? PackageApproval.Scope.USER : PackageApproval.Scope.ALL,
                            action(), job, review.digest);
                    break;
                case PackageBrokerProtocol.RUNNING:
                case PackageBrokerProtocol.VALIDATING:
                case PackageBrokerProtocol.PUBLISHING:
                    if (!handedOff) throw new IllegalStateException("Worker started without handoff");
                    break;
                case PackageBrokerProtocol.SEALED:
                    seal();
                    break;
                default: break; // Selecting, planning or preparing: the native owner still works.
            }
            return view(false);
        } catch (RuntimeException failure) { seal(); throw failure; }
    }
    synchronized PackageApproval.Prepared approval() {
        requireOpen();
        requireFreshReview();
        if (prepared == null || phase != PackageBrokerProtocol.PREPARED || handedOff) {
            throw new IllegalStateException("Package is not awaiting approval");
        }
        return prepared;
    }
    // Only PackageApproval's fresh credential path calls this, inside admission.
    synchronized void start(PackageApproval.Prepared approved, long deadline) {
        if (approval() != approved) throw new SecurityException("Different package plan");
        handedOff = true; // Uncertain start replies must never become a fresh attempt.
        try { success(PackageBrokerProtocol.START, deadline); }
        catch (RuntimeException failure) { seal(); throw failure; }
    }
    /** Exact-job cancellation only. No admission, AOSP calls, BEGIN or continuation. */
    synchronized void cleanup(long deadline) {
        if (!sealed.get() || retired.get() || job == 0) return;
        PackageBrokerProtocol.Reply cancelled = exchange(PackageBrokerProtocol.CANCEL, deadline);
        if (cancelled.error == OsConstants.ENOENT) { retired.set(true); return; }
        if (cancelled.error != 0 && cancelled.error != OsConstants.EALREADY) return;
        // A successful cancel seals the worker first. Poll only its cleanup receipt;
        // EALREADY means publication won the race, so collect that actual outcome.
        PackageBrokerProtocol.Reply status = exchange(PackageBrokerProtocol.STATUS, deadline);
        if (status.error == OsConstants.ENOENT) { retired.set(true); return; }
        if (status.error != 0) return;
        PackageBrokerProtocol.Metadata value = status.metadata;
        value.requireIntent(intent);
        if (review != null && !review.digest.equals(value.digest)) return;
        if (value.phase == PackageBrokerProtocol.COMPLETE) {
            terminal = value;
            retired.set(true);
        }
    }
    synchronized Bundle view(boolean cleanupOnly) {
        Bundle result = new Bundle();
        String state = terminal != null ? "complete"
                : sealed.get() ? (retired.get() ? "closed" : "cancelling")
                : handedOff ? "running"
                : prepared != null ? "approval_required" : "preparing";
        result.putString("state", state);
        result.putString("outcome", terminal == null ? "unconfirmed"
                : terminal.outcome == PackageBrokerProtocol.PUBLISHED ? "published"
                : terminal.outcome == PackageBrokerProtocol.FAILED ? "failed" : "unconfirmed");
        if (cleanupOnly) return result;
        result.putString("action", action().name().toLowerCase(java.util.Locale.ROOT));
        result.putString("scope", intent.scope == PackageBrokerProtocol.PERSONAL ? "user" : "all");
        if (prepared != null && review != null) {
            ArrayList<Bundle> changes = new ArrayList<>();
            for (PackageBrokerProtocol.Change change : review.changes) {
                Bundle item = new Bundle();
                item.putString("name", change.name);
                item.putString("architecture", change.architecture);
                item.putString("before", change.before);
                item.putString("after", change.after);
                item.putString("reason", change.reason == 1 ? "manual" : "automatic");
                item.putString("effect", change.before.equals(change.after) ? "private_selection" : "package_change");
                changes.add(item);
            }
            result.putParcelableArrayList("changes", changes);
        }
        return result;
    }
}
