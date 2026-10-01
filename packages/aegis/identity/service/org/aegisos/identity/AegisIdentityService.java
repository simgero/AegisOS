package org.aegisos.identity;

import android.content.Context;
import android.app.KeyguardManager;
import android.app.ActivityManagerInternal;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.Process;
import android.os.PowerManager;
import android.os.RemoteException;
import android.os.ServiceSpecificException;
import android.os.SystemProperties;
import android.os.SystemClock;
import android.os.UserHandle;
import android.util.Pair;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;

import com.android.internal.widget.LockscreenCredential;
import com.android.server.SystemService;
import com.android.server.LocalServices;
import com.android.server.aegis.AegisRuntimeStorage;

import java.nio.CharBuffer;
import java.io.IOException;
import java.util.Arrays;
import java.util.NoSuchElementException;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/**
 * AOSP system-server service for the first developer-console integration.
 * AOSP authenticates personal identities. Managed-mode lifecycle admission is
 * internal to system_server; the product remains runtime-absent until the native
 * owner, init and SELinux policy are installed and verified together.
 */
public final class AegisIdentityService extends SystemService {
    public static final String SERVICE_NAME = "aegis_identity";
    private static final int MAX_SESSIONS = 16;
    private static final int ERROR_AUTH = 1;
    private static final int ERROR_RETRY = 2;
    private static final int ERROR_STATE = 3;
    private final Object operations = new Object();
    private final Set<Session> sessions = ConcurrentHashMap.newKeySet();
    // Includes closed channels whose native command result still needs reaping.
    // This bound is below the native owner's 32 result slots per context.
    private final Set<Session.PersonalTerminal> terminals = ConcurrentHashMap.newKeySet();
    private final ConcurrentHashMap<Integer, AtomicLong> revocations = new ConcurrentHashMap<>();
    private final AtomicLong interactiveEpoch = new AtomicLong();
    private ActivityManagerInternal activityInternal;
    private AospIdentityBackend backend;
    private AospPackageAuthority packageAuthority;
    private final Set<Session.PackageJob> packages = ConcurrentHashMap.newKeySet();
    private RuntimeBrokerConnection runtime;
    private RuntimeAdmission admission;
    private volatile boolean bootCompleted;

    public AegisIdentityService(Context context) { super(context); }

    @Override public void onStart() {
        if (!Build.IS_DEBUGGABLE) {
            throw new IllegalStateException("Developer console requires a debuggable build");
        }
        String mode = SystemProperties.get("ro.aegis.runtime.mode");
        if ("managed-v1".equals(mode)) {
            runtime = new RuntimeBrokerConnection();
            admission = new RuntimeAdmission((user, deadline) -> {
                // Called under the per-user storage/admission gate, potentially
                // from AOSP storage locks. Never acquire operations or call AOSP.
                revokeTerminalBindings(user);
                runtime.stopAndReleaseAll(user, deadline);
                retirePackages(user);
                retireTerminals(user);
            });
            AegisRuntimeStorage.register(new RuntimeStorageController(admission));
            Executors.newSingleThreadScheduledExecutor(task -> {
                Thread thread = new Thread(task, "AEGIS terminal result cleanup");
                thread.setDaemon(true);
                return thread;
            }).scheduleWithFixedDelay(this::reapTerminals, 1, 1, TimeUnit.SECONDS);
        } else if (!"absent".equals(mode)) {
            throw new IllegalStateException("Unknown runtime lifecycle mode");
        }
        backend = new AospIdentityBackend(getContext());
        packageAuthority = new AospPackageAuthority(getContext(), backend);
        activityInternal = java.util.Objects.requireNonNull(
                LocalServices.getService(ActivityManagerInternal.class));
        publishBinderService(SERVICE_NAME, new IAegisIdentity.Stub() {
            @Override public IAegisSession openSession(IBinder clientLifetime)
                    throws RemoteException {
                int uid = Binder.getCallingUid();
                // This admits the already-authorized development terminal, not ordinary apps
                // or runtime users. It does NOT establish personal identity or admin rights.
                if (uid != Process.ROOT_UID && uid != Process.SHELL_UID) {
                    throw new SecurityException("An authorized development terminal is required");
                }
                if (!bootCompleted) throw new IllegalStateException("AOSP boot is not complete");
                if (clientLifetime == null) throw new IllegalArgumentException("Missing client");
                CallerProcess owner = new CallerProcess();
                synchronized (operations) {
                    if (sessions.size() >= MAX_SESSIONS) {
                        throw new IllegalStateException("Too many terminal sessions");
                    }
                    Session session = new Session(owner, clientLifetime);
                    sessions.add(session);
                    try {
                        clientLifetime.linkToDeath(session, 0);
                        if (!clientLifetime.isBinderAlive() || !session.alive) {
                            throw new RemoteException("Client disconnected");
                        }
                        return session;
                    } catch (RemoteException e) {
                        session.dispose();
                        throw e;
                    }
                }
            }
        }, false);
    }

    @Override public void onBootPhase(int phase) {
        if (phase == PHASE_BOOT_COMPLETED) {
            // A screen/keyguard lock revokes interactive authority only. It
            // neither stops background contexts nor claims to evict CE keys.
            getContext().registerReceiver(new BroadcastReceiver() {
                @Override public void onReceive(Context context, Intent intent) {
                    if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) revokeInteractive();
                }
            }, new IntentFilter(Intent.ACTION_SCREEN_OFF), Context.RECEIVER_NOT_EXPORTED);
            getContext().getSystemService(KeyguardManager.class)
                    .addKeyguardLockedStateListener(Runnable::run, locked -> {
                        if (locked) revokeInteractive();
                    });
            bootCompleted = true;
        }
    }

    @Override public void onUserSwitching(TargetUser from, TargetUser to) {
        if (from != null && from.getUserIdentifier() > 0) {
            revokeTerminalBindings(from.getUserIdentifier());
        }
    }

    private void requireInteractive() {
        if (!getContext().getSystemService(PowerManager.class).isInteractive()) {
            throw new SecurityException("Wake the development console before personal interaction");
        }
    }

    private void revokeInteractive() {
        interactiveEpoch.incrementAndGet();
        for (Session.PackageJob job : packages) job.transaction.seal();
        for (Session session : sessions) {
            session.selection.set(null);
            session.discardLoginPreparation();
        }
        for (Session.PersonalTerminal terminal : terminals) {
            try { terminal.closeChannel(); }
            catch (RuntimeException failure) { /* Registered owner retries; CE barrier stays strict. */ }
        }
    }

    private long epoch(int userId) {
        return revocations.computeIfAbsent(userId, ignored -> new AtomicLong()).get();
    }

    @Override public void onUserStopping(TargetUser user) {
        // Do not acquire operations here: user stop is awaited on a Binder worker.
        // This only revokes bindings. AOSP catches callback exceptions and then
        // continues stopping; an async runtime stop here is not a CE-lock barrier.
        // Managed mode's registered storage controller is the actual CE barrier.
        revoke(user.getUserIdentifier());
    }

    private void revoke(int userId) {
        if (admission != null && userId > 0) admission.revoke(userId);
        revokeTerminalBindings(userId);
    }

    private void revokeTerminalBindings(int userId) {
        revocations.computeIfAbsent(userId, ignored -> new AtomicLong()).incrementAndGet();
        for (Session session : sessions) {
            LoginPreparation preparing = session.loginPreparation.get();
            if (preparing != null && preparing.user.id == userId) {
                preparing.revoke();
                session.loginPreparation.compareAndSet(preparing, null);
            }
            Selection selected = session.selection.get();
            if (selected != null && selected.user.id == userId) {
                // A concurrent switch to another user must not be cleared by
                // this old user's storage callback. No identity monitor here.
                session.selection.compareAndSet(selected, null);
            }
        }
        // Package revocation must not depend on successfully closing a PTY.
        sealPackages(userId);
        closeTerminals(userId);
    }

    private void closeTerminals(int userId) {
        // No identity monitor, AOSP call, per-user gate acquisition or blocking
        // native protocol call. Each terminal lock only protects nonblocking IO.
        for (Session.PersonalTerminal terminal : terminals) {
            if (terminal.selected.user.id == userId) terminal.closeChannel();
        }
    }

    private void retireTerminals(int userId) {
        // Call only AFTER the native owner has confirmed whole-context cleanup.
        for (Session.PersonalTerminal terminal : terminals) {
            if (terminal.selected.user.id == userId) terminal.stopped();
        }
    }

    private void sealPackages(int userId) {
        for (Session.PackageJob job : packages) {
            if (job.selected.user.id == userId) job.transaction.seal();
        }
    }

    private void retirePackages(int userId) {
        // Only after confirmed whole-user STOP. Atomic callbacks, no job monitor.
        for (Session.PackageJob job : packages) {
            if (job.selected.user.id == userId) {
                job.transaction.stopped();
                job.retireIfFinished();
            }
        }
    }

    private void reapTerminals() {
        for (Session session : sessions) {
            // A caller-supplied lifetime Binder is not proof that the original
            // kernel process still exists (it could refer to another process).
            try { session.owner.requireAlive(); }
            catch (SecurityException gone) { session.dispose(); }
        }
        for (Session.PackageJob job : packages) {
            try {
                try { job.requireBinding(); }
                catch (SecurityException revoked) { job.transaction.seal(); }
                job.transaction.cleanup(System.nanoTime() + TimeUnit.SECONDS.toNanos(2));
            } catch (RuntimeException unconfirmed) {
                // Failed replies or cancellation retain ownership until retry/whole STOP.
            } finally { job.retireIfFinished(); }
        }
        for (Session.PersonalTerminal terminal : terminals) {
            try {
                if (!terminal.needsResult()) continue;
                // Metadata-only cleanup of a previously authorized command.
                // No new exec, file access, AOSP call or identity authority.
                terminal.collectResult(System.nanoTime() + TimeUnit.SECONDS.toNanos(2));
            } catch (RuntimeException failure) {
                // Retain unresolved ownership. A later confirmed user STOP
                // retires it; a broken native connection never proves cleanup.
            }
        }
    }

    private static final class Selection {
        final AospIdentityBackend.UserKey user;
        final RuntimeAdmission.Binding runtime;
        final long interactive;
        Selection(AospIdentityBackend.UserKey user, RuntimeAdmission.Binding runtime, long interactive) {
            this.user = user;
            this.runtime = runtime;
            this.interactive = interactive;
        }
    }

    private interface Operation<T> { T run() throws RemoteException; }
    private interface TerminalOperation<T> { T run(long deadline) throws RemoteException; }

    private final class Session extends IAegisSession.Stub implements IBinder.DeathRecipient {
        private final CallerProcess owner;
        private final IBinder lifetime;
        private volatile boolean alive = true;
        private final AtomicReference<Selection> selection = new AtomicReference<>();
        private final AtomicReference<LoginPreparation> loginPreparation = new AtomicReference<>();
        private final AtomicReference<PersonalTerminal> currentTerminal = new AtomicReference<>();
        private final AtomicReference<PackageJob> currentPackage = new AtomicReference<>();

        Session(CallerProcess owner, IBinder lifetime) {
            this.owner = owner;
            this.lifetime = lifetime;
        }

        private void requireOwner() {
            owner.requireSameCaller();
            if (!alive || !lifetime.isBinderAlive()) {
                throw new SecurityException("Terminal session is closed");
            }
        }

        private <T> T checked(Operation<T> operation) {
            requireOwner();
            synchronized (operations) {
                requireOwner();
                try {
                    T result = operation.run();
                    try { requireOwner(); return result; }
                    catch (RuntimeException failure) {
                        if (result instanceof byte[]) wipe((byte[]) result);
                        throw failure;
                    }
                } catch (AospIdentityBackend.AuthenticationFailure e) {
                    throw new ServiceSpecificException(e.retryAfterMs > 0 ? ERROR_RETRY : ERROR_AUTH,
                            e.retryAfterMs > 0 ? Integer.toString(e.retryAfterMs)
                                    : "AOSP rejected the credential");
                } catch (PackageApproval.RejectedCredential e) {
                    throw new ServiceSpecificException(e.retryAfterMs > 0 ? ERROR_RETRY : ERROR_AUTH,
                            e.retryAfterMs > 0 ? Integer.toString(e.retryAfterMs)
                                    : "AOSP rejected the credential");
                } catch (SecurityException e) {
                    throw new SecurityException("Action was not authorized by AOSP");
                } catch (IllegalArgumentException e) {
                    throw new IllegalArgumentException("Invalid identity request");
                } catch (RemoteException | RuntimeException e) {
                    // No backend stack traces or exception strings cross this boundary.
                    throw new ServiceSpecificException(ERROR_STATE,
                            "Operation was not confirmed; check the actual user/storage state");
                }
            }
        }

        private AospIdentityBackend.UserKey requireAuthenticated() throws RemoteException {
            Selection selected = selection.get();
            if (selected == null) throw new SecurityException("Log in in this terminal first");
            AospIdentityBackend.UserKey user = selected.user;
            AospIdentityBackend.State state = backend.state(user);
            if (!state.enabled || state.partial || !state.running || !state.ceUnlocked
                    || selection.get() != selected || selected.interactive != interactiveEpoch.get()) {
                selection.compareAndSet(selected, null);
                throw new SecurityException("Personal session is no longer unlocked");
            }
            return user;
        }

        private AospIdentityBackend.UserKey selectedUser() {
            Selection selected = selection.get();
            return selected == null ? null : selected.user;
        }

        private AospIdentityBackend.OperationGuard mutationGuard(AospIdentityBackend.UserKey actor) {
            long initialEpoch = actor == null ? 0 : epoch(actor.id);
            return () -> {
                owner.requireAlive();
                if (!alive || !lifetime.isBinderAlive() || selectedUser() != actor
                        || (actor != null && epoch(actor.id) != initialEpoch)) {
                    throw new SecurityException("Administration session was revoked");
                }
            };
        }

        private void discardLoginPreparation() {
            LoginPreparation previous = loginPreparation.getAndSet(null);
            if (previous != null) previous.revoke();
        }

        private void requireCompletedForeground(AospIdentityBackend.UserKey target)
                throws RemoteException {
            Pair<Integer, Integer> foreground = activityInternal.getCurrentAndTargetUserIds();
            AospIdentityBackend.State state = backend.state(target);
            if (foreground.first != target.id || foreground.second != UserHandle.USER_NULL
                    || !state.user.equals(target) || !state.enabled || state.partial
                    || !state.running || !state.foreground) {
                throw new SecurityException("Android login target is not stable");
            }
        }

        @Override public void prepareLogin(String name) {
            checked(() -> {
                requireInteractive();
                discardLoginPreparation();
                AospIdentityBackend.UserKey target = backend.resolveName(name);
                selection.set(null);
                sealCurrentPackage();
                closeCurrentTerminal();
                // This is Android's login-target selection, before any password
                // input or personal authority. It may show the target's keyguard.
                if (com.android.server.aegis.AegisCeLock.isPending(target.id)) {
                    // Also recover if a non-AEGIS Android entry point started the
                    // still-locked user since the failed logout. Never reuse auth.
                    backend.stopAndroidUserAndLock(target);
                }
                backend.selectLoginTarget(target);
                long deadline = SystemClock.elapsedRealtime() + AospIdentityBackend.STATE_TIMEOUT_MS;
                while (true) {
                    requireOwner();
                    requireInteractive();
                    AospIdentityBackend.State state = backend.state(target);
                    if (!state.enabled || state.partial) {
                        throw new SecurityException("Login target changed");
                    }
                    Pair<Integer, Integer> foreground = activityInternal.getCurrentAndTargetUserIds();
                    if (foreground.first == target.id && foreground.second == UserHandle.USER_NULL
                            && state.running && state.foreground) break;
                    if (SystemClock.elapsedRealtime() >= deadline
                            || Thread.currentThread().isInterrupted()) {
                        throw new IllegalStateException("Android user switch was not completed");
                    }
                    SystemClock.sleep(25);
                }
                long interaction = interactiveEpoch.get();
                long revision = epoch(target.id);
                LoginPreparation ready = new LoginPreparation(target, interaction, revision);
                loginPreparation.set(ready);
                try {
                    requireCompletedForeground(target);
                    requireInteractive();
                    if (interaction != interactiveEpoch.get() || revision != epoch(target.id)
                            || loginPreparation.get() != ready) {
                        throw new SecurityException("Login target was revoked");
                    }
                } catch (RemoteException | RuntimeException failure) {
                    ready.revoke();
                    loginPreparation.compareAndSet(ready, null);
                    throw failure;
                }
                return null;
            });
        }

        @Override public String login(String name, byte[] password) {
            try {
                return checked(() -> {
                    requireInteractive();
                    long interactionBefore = interactiveEpoch.get();
                    LoginPreparation prepared = loginPreparation.getAndSet(null);
                    if (prepared == null) {
                        throw new SecurityException("Prepare login before entering credentials");
                    }
                    AospIdentityBackend.UserKey target = backend.resolveName(name);
                    long before = epoch(target.id);
                    prepared.claim(target, interactionBefore, before);
                    requireCompletedForeground(target);
                    RuntimeAdmission.AuthenticationAttempt attempt = admission == null ? null
                            : admission.beforeAuthentication(target.id, target.serial);
                    AospIdentityBackend.State state;
                    try {
                        // No runtime gate is held across LockSettings/UserManager/vold.
                        try (LockscreenCredential credential = credential(password)) {
                            // The expected switch/keyguard transition happened before
                            // password input. No cached proof survives a later lock.
                            state = backend.authenticate(target, credential);
                        }
                        requireOwner();
                        requireCompletedForeground(target);
                        if (interactiveEpoch.get() != interactionBefore || epoch(target.id) != before || !state.user.equals(target)
                                || !state.enabled || state.partial || !state.running || !state.ceUnlocked) {
                            throw new IllegalStateException("User changed during authentication");
                        }
                        if (admission == null) {
                            Selection selected = new Selection(target, null, interactionBefore);
                            closeCurrentTerminal();
                            selection.set(selected);
                            if (epoch(target.id) != before || interactiveEpoch.get() != interactionBefore) {
                                selection.compareAndSet(selected, null);
                                throw new IllegalStateException("User stopped during session binding");
                            }
                        } else {
                            try (RuntimeAdmission.Access access = admission.afterAuthentication(attempt)) {
                                // Reconciliation may have retired predecessor sessions. The
                                // current attempt has fresh AOSP proof and a different epoch.
                                requireOwner();
                                Selection selected = new Selection(target, access.binding(), interactionBefore);
                                closeCurrentTerminal();
                                selection.set(selected);
                                try {
                                    access.checkCurrent();
                                    if (interactiveEpoch.get() != interactionBefore) {
                                        throw new SecurityException("Console locked during login");
                                    }
                                }
                                catch (RuntimeException failure) {
                                    selection.compareAndSet(selected, null);
                                    throw failure;
                                }
                            }
                        }
                    } finally {
                        if (attempt != null) attempt.discard();
                    }
                    return describe(state);
                });
            } finally {
                wipe(password);
            }
        }

        @Override public String status() {
            return checked(() -> {
                Selection selected = selection.get();
                if (selected == null) return "terminal=unauthenticated " + runtimeDescription()
                        + " setup=" + backend.setupStatus();
                AospIdentityBackend.UserKey user = selected.user;
                AospIdentityBackend.State state = backend.state(user);
                if (!state.enabled || state.partial || !state.running || !state.ceUnlocked) {
                    selection.compareAndSet(selected, null);
                }
                return describe(state);
            });
        }

        @Override public String listUsers() {
            return checked(() -> {
                StringBuilder result = new StringBuilder();
                for (AospIdentityBackend.UserKey key : backend.personalUsers()) {
                    AospIdentityBackend.State state = backend.state(key);
                    result.append("user=").append(key.id).append(" serial=").append(key.serial)
                            .append(" name=").append(safeName(state.name))
                            .append(" admin=").append(state.admin)
                            .append(" enabled=").append(state.enabled)
                            .append(" partial=").append(state.partial).append('\n');
                }
                return result.length() == 0 ? "No personal AOSP users" : result.toString();
            });
        }

        @Override public String changePassword(byte[] previous, byte[] replacement) {
            try {
                return checked(() -> {
                    AospIdentityBackend.UserKey user = requireAuthenticated();
                    try (LockscreenCredential oldCredential = credential(previous);
                            LockscreenCredential newCredential = credential(replacement)) {
                        backend.changePassword(user, oldCredential, newCredential);
                    }
                    requireAuthenticated();
                    return "Password changed by AOSP";
                });
            } finally {
                wipe(previous);
                wipe(replacement);
            }
        }

        @Override public String setupFirstAdmin(String name, byte[] password) {
            return initialAdmin(name, password, false);
        }

        @Override public String resumeFirstAdmin(String name, byte[] password) {
            return initialAdmin(name, password, true);
        }

        private String initialAdmin(String name, byte[] password, boolean resume) {
            try {
                return checked(() -> {
                    if (Binder.getCallingUid() != Process.ROOT_UID || selection.get() != null) {
                        throw new SecurityException("Initial setup requires the development root console");
                    }
                    try (LockscreenCredential initial = credential(password)) {
                        AospIdentityBackend.State state = resume
                                ? backend.resumeFirstAdmin(name, initial, mutationGuard(null))
                                : backend.bootstrapFirstAdmin(name, initial, mutationGuard(null));
                        return "First AOSP administrator ready; CE storage locked; log in separately\n"
                                + describe(state);
                    }
                });
            } finally {
                wipe(password);
            }
        }

        @Override public String addUser(String name, boolean administrator, byte[] adminPassword,
                byte[] password) {
            try {
                return checked(() -> {
                    AospIdentityBackend.UserKey actor = requireAuthenticated();
                    try (LockscreenCredential admin = credential(adminPassword);
                            LockscreenCredential initial = credential(password)) {
                        AospIdentityBackend.State created = backend.createPersonalUser(actor, admin,
                                name, initial, administrator, mutationGuard(actor));
                        return "Personal AOSP user created; CE storage locked\n" + describe(created);
                    }
                });
            } finally {
                wipe(adminPassword);
                wipe(password);
            }
        }

        @Override public String removeUser(String name, byte[] adminPassword) {
            try {
                return checked(() -> {
                    AospIdentityBackend.UserKey actor = requireAuthenticated();
                    AospIdentityBackend.UserKey target = backend.resolveName(name);
                    try (LockscreenCredential admin = credential(adminPassword)) {
                        // The integrated AOSP stop/removal paths own the storage
                        // barrier and retain the original identity until cleanup
                        // completes. Do not hold a runtime gate across AOSP calls
                        // or perform destructive cleanup after its ID is released.
                        backend.removePersonalUser(actor, admin, target, mutationGuard(actor));
                    }
                    revoke(target.id);
                    return "AOSP user absent; user stopped and CE storage locked"
                            + " user=" + target.id + " serial=" + target.serial
                            + (runtime == null ? " runtime=not-installed" : " runtime=removed");
                });
            } finally {
                wipe(adminPassword);
            }
        }

        @Override public String logout() {
            return checked(() -> {
                AospIdentityBackend.UserKey user = requireAuthenticated();
                // Revoke all authority for this user before stopping Android. If stop fails,
                // status remains unauthenticated; a new login must verify an AOSP password.
                revoke(user.id);
                if (admission != null) {
                    // Finish teardown before asking Android to stop. Release the
                    // gate before entering AOSP, whose storage path obtains its
                    // own lease again around key eviction/state publication.
                    try (RuntimeAdmission.Storage stopped = admission.storage(user.id, true)) { }
                }
                AospIdentityBackend.State stopped = backend.stopAndroidUserAndLock(user);
                return "Android user stopped; CE storage locked; "
                        + (runtime == null ? "runtime not installed" : "runtime stopped and released") + "\n"
                        + describe(stopped);
            });
        }

        @Override public String linuxStart() { return linuxLifecycle(RuntimeBrokerProtocol.START); }
        @Override public String linuxStatus() { return linuxLifecycle(RuntimeBrokerProtocol.STATUS); }
        @Override public String linuxStop() { return linuxLifecycle(RuntimeBrokerProtocol.STOP_USER); }

        private String linuxLifecycle(int operation) {
            return checked(() -> {
                if (runtime == null) throw new IllegalStateException("Runtime is not installed");
                AospIdentityBackend.UserKey user = requireAuthenticated(); // AOSP checks outside the gate
                Selection selected = selection.get();
                RuntimeAdmission.Binding binding = selected == null ? null : selected.runtime;
                if (binding == null || !binding.matches(user.id, user.serial)) {
                    throw new SecurityException("Fresh personal runtime admission is required");
                }
                if (operation == RuntimeBrokerProtocol.START) {
                    awaitRuntime(user, binding);
                    return "user=" + user.id + " serial=" + user.serial + " runtime=ready ce=unlocked";
                }
                try (RuntimeAdmission.Access access = admission.existing(binding)) {
                    requireRuntimeBinding(user, binding);
                    int state;
                    String packageStatus = "";
                    if (operation == RuntimeBrokerProtocol.STOP_USER) {
                        // Stop keeps AOSP authentication and CE unlocked. It does
                        // not revoke other clients' identity or claim a logout.
                        sealPackages(user.id);
                        closeTerminals(user.id);
                        runtime.stopAndReleaseAll(user.id, access.deadlineNanos());
                        retirePackages(user.id);
                        retireTerminals(user.id);
                        state = RuntimeBrokerProtocol.ABSENT;
                    } else {
                        RuntimeBrokerProtocol.Reply observation = runtime.activation(user.id, user.serial, access.deadlineNanos());
                        state = observation.state;
                        switch (observation.activation) {
                            case RuntimeBrokerProtocol.PACKAGES_INACTIVE:
                                packageStatus = " packages=not-active"; break;
                            case RuntimeBrokerProtocol.PACKAGES_CURRENT:
                                packageStatus = " packages=current"; break;
                            case RuntimeBrokerProtocol.PACKAGES_PENDING:
                                packageStatus = " packages=activation-pending; Neue Software wartet auf den nächsten Linux-Start."; break;
                            case RuntimeBrokerProtocol.PACKAGES_UNKNOWN:
                                packageStatus = " packages=unconfirmed; Paketstand derzeit ungeprüft; der laufende Kontext bleibt unverändert."; break;
                            default: throw new IllegalStateException("Invalid package observation");
                        }
                    }
                    access.checkCurrent();
                    requireRuntimeBinding(user, binding);
                    return "user=" + user.id + " serial=" + user.serial + " runtime="
                            + (state == RuntimeBrokerProtocol.READY ? "ready"
                                : state == RuntimeBrokerProtocol.ABSENT ? "stopped" : "sealed")
                            + " ce=unlocked" + packageStatus;
                }
            });
        }

        private RuntimeBrokerProtocol.StartReply awaitRuntime(AospIdentityBackend.UserKey user,
                RuntimeAdmission.Binding binding) throws RemoteException {
            return new RuntimeStartWaiter().await((job, deadline) -> {
                // AOSP calls outside the gate; compare the original session
                // identity instead of adopting a later login from this terminal.
                if (requireAuthenticated() != user) throw new SecurityException("Runtime login changed");
                try (RuntimeAdmission.Access access = admission.existing(binding)) {
                    requireRuntimeBinding(user, binding);
                    RuntimeBrokerProtocol.StartReply reply = runtime.start(user.id, user.serial, job,
                            Math.min(deadline, access.deadlineNanos()));
                    access.checkCurrent();
                    requireRuntimeBinding(user, binding);
                    return reply;
                }
            });
        }

        @Override public IAegisPackage packageBegin(String action, String scope, String name, String version) {
            return checked(() -> {
                requireInteractive();
                if (runtime == null) throw new IllegalStateException("Runtime is not installed");
                int operation;
                switch (action) {
                    case "install": operation = PackageBrokerProtocol.INSTALL; break;
                    case "update": operation = PackageBrokerProtocol.UPDATE; break;
                    case "remove": operation = PackageBrokerProtocol.REMOVE; break;
                    default: throw new IllegalArgumentException("Unknown package action");
                }
                PackageBrokerProtocol.Intent intent = new PackageBrokerProtocol.Intent(operation,
                        PackageApproval.Scope.fromArgument(scope) == PackageApproval.Scope.USER
                                ? PackageBrokerProtocol.PERSONAL : PackageBrokerProtocol.SHARED,
                        name, version);
                PackageJob previous = currentPackage.get();
                if ((previous != null && !previous.transaction.retired()) || packages.size() >= MAX_SESSIONS) {
                    throw new IllegalStateException("Package cleanup still owns its resources");
                }
                AospIdentityBackend.UserKey user = requireAuthenticated();
                Selection selected = selection.get();
                if (selected == null || selected.runtime == null) throw new SecurityException("No runtime binding");
                PackageJob request = new PackageJob(selected, intent);
                packageAuthority.requireRequester(user, request.transaction.action()); // Outside admission.
                synchronized (request) {
                    try (RuntimeAdmission.Access access = admission.existing(selected.runtime)) {
                        request.requireBinding();
                        packages.add(request); // Even a lost BEGIN reply retains cleanup ownership.
                        currentPackage.set(request);
                        try {
                            request.transaction.begin(access.deadlineNanos());
                            access.checkCurrent();
                            request.requireBinding();
                            return request;
                        } catch (RuntimeException failure) {
                            request.transaction.seal();
                            request.retireIfFinished();
                            throw failure;
                        }
                    }
                }
            });
        }

        private void sealCurrentPackage() {
            PackageJob job = currentPackage.get();
            if (job != null) job.transaction.seal();
        }

        private final class PackageJob extends IAegisPackage.Stub {
            final Selection selected;
            final long revision;
            final PackageTransaction transaction;

            PackageJob(Selection selected, PackageBrokerProtocol.Intent intent) {
                this.selected = selected;
                revision = epoch(selected.user.id);
                transaction = new PackageTransaction(selected.user, intent,
                        (op, job, request, digest, deadline) -> runtime.packageCall(op,
                                selected.user.id, selected.user.serial, job, request, digest, deadline),
                        () -> System.currentTimeMillis() / 1000);
            }

            void requireBinding() {
                owner.requireAlive();
                if (!alive || !lifetime.isBinderAlive() || selection.get() != selected
                        || selected.interactive != interactiveEpoch.get()
                        || revision != epoch(selected.user.id) || transaction.sealed()) {
                    throw new SecurityException("Package belongs to a revoked login");
                }
            }

            void retireIfFinished() {
                if (transaction.retired()) {
                    packages.remove(this);
                    currentPackage.compareAndSet(this, null);
                }
            }

            @Override public android.os.Bundle status() {
                return checked(() -> {
                    try {
                        requireInteractive();
                        requireBinding();
                        if (requireAuthenticated() != selected.user) throw new SecurityException("Login changed");
                        packageAuthority.requireRequester(selected.user, transaction.action());
                        try (RuntimeAdmission.Access access = admission.existing(selected.runtime)) {
                            requireBinding();
                            android.os.Bundle result = transaction.poll(access.deadlineNanos());
                            access.checkCurrent();
                            requireBinding();
                            return result;
                        }
                    } catch (RemoteException | RuntimeException failure) {
                        transaction.seal();
                        throw failure;
                    } finally { retireIfFinished(); }
                });
            }

            @Override public android.os.Bundle approve(String administrator, byte[] password) {
                try {
                    return checked(() -> {
                        requireInteractive();
                        requireBinding();
                        if (requireAuthenticated() != selected.user) throw new SecurityException("Login changed");
                        PackageApproval.Prepared prepared = transaction.approval();
                        AospIdentityBackend.UserKey admin = backend.resolveName(administrator);
                        // Credential verification and AOSP restrictions run outside admission
                        // and outside the transaction monitor. There is no cached admin grant.
                        new PackageApproval(packageAuthority).confirmAndStart(prepared, admin,
                                credential(password), this::requireBinding, new PackageApproval.Handoff() {
                                    @Override public void start(PackageApproval.Prepared approved,
                                            AospIdentityBackend.OperationGuard guard) {
                                        try (RuntimeAdmission.Access access = admission.existing(selected.runtime)) {
                                            guard.check();
                                            transaction.start(approved, access.deadlineNanos());
                                            access.checkCurrent();
                                            guard.check();
                                        }
                                    }
                                    @Override public void cancel(PackageApproval.Prepared ignored) {
                                        transaction.seal(); // Registered reaper owns confirmation/retries.
                                    }
                                });
                        return transaction.view(false);
                    });
                } finally { wipe(password); }
            }

            @Override public android.os.Bundle cancel() {
                owner.requireSameCaller();
                transaction.seal();
                try { transaction.cleanup(System.nanoTime() + TimeUnit.SECONDS.toNanos(2)); }
                catch (RuntimeException unconfirmed) { /* Keep the job for the registered reaper. */ }
                retireIfFinished();
                return transaction.view(true);
            }
        }

        @Override public IAegisTerminal linuxShell(int rows, int columns) {
            return checked(() -> {
                requireInteractive();
                if (runtime == null) throw new IllegalStateException("Runtime is not installed");
                dimensions(rows, columns);
                if (currentTerminal.get() != null || terminals.size() >= MAX_SESSIONS) {
                    throw new IllegalStateException("Terminal resources are still in use");
                }
                AospIdentityBackend.UserKey user = requireAuthenticated();
                Selection selected = selection.get();
                RuntimeAdmission.Binding binding = selected == null ? null : selected.runtime;
                if (binding == null || !binding.matches(user.id, user.serial)) {
                    throw new SecurityException("Fresh personal runtime admission is required");
                }
                RuntimeBrokerProtocol.StartReply started = awaitRuntime(user, binding);
                if (requireAuthenticated() != user) throw new SecurityException("Runtime login changed");
                try (RuntimeAdmission.Access access = admission.existing(binding)) {
                    requireRuntimeBinding(user, binding);
                    // Recheck the same selection under the EXEC gate. Never
                    // create a replacement if a STOP happened after await.
                    boolean ready = started.job != 0
                            ? runtime.start(user.id, user.serial, started.job, access.deadlineNanos()).ready
                            : runtime.state(user.id, user.serial, access.deadlineNanos()) == RuntimeBrokerProtocol.READY;
                    if (!ready) throw new IllegalStateException("Runtime is no longer ready");
                    // Another caller may have opened a terminal while this
                    // request waited outside admission. Recheck before EXEC.
                    if (currentTerminal.get() != null || terminals.size() >= MAX_SESSIONS) {
                        throw new IllegalStateException("Terminal resources are still in use");
                    }
                    PersonalTerminal terminal = new PersonalTerminal(selected,
                            runtime.execute(user.id, user.serial,
                                    new String[] {"/bin/bash", "-i"}, access.deadlineNanos()));
                    terminals.add(terminal);
                    boolean published = false;
                    try {
                        terminal.prepare(rows, columns);
                        access.checkCurrent();
                        requireRuntimeBinding(user, binding);
                        if (!currentTerminal.compareAndSet(null, terminal)) {
                            throw new IllegalStateException("Another terminal already exists");
                        }
                        requireOwner();
                        published = true;
                        return terminal;
                    } finally {
                        if (!published) terminal.closeChannel();
                    }
                }
            });
        }

        private void closeCurrentTerminal() {
            PersonalTerminal terminal = currentTerminal.getAndSet(null);
            if (terminal != null) terminal.closeChannel();
        }

        private final class PersonalTerminal extends IAegisTerminal.Stub {
            private final Selection selected;
            private final RuntimeBrokerConnection.Terminal nativeTerminal;
            private final Object io = new Object();
            private boolean closeRequested, closed, resultCollected, eof;
            private int exit = -1;

            PersonalTerminal(Selection selected, RuntimeBrokerConnection.Terminal terminal) {
                this.selected = selected;
                this.nativeTerminal = terminal;
            }

            void prepare(int rows, int columns) {
                synchronized (io) {
                    try {
                        int flags = Os.fcntlInt(nativeTerminal.master.getFileDescriptor(), OsConstants.F_GETFL, 0);
                        Os.fcntlInt(nativeTerminal.master.getFileDescriptor(), OsConstants.F_SETFL,
                                flags | OsConstants.O_NONBLOCK);
                        resizeOwned(rows, columns);
                    } catch (ErrnoException failure) {
                        throw new IllegalStateException("Cannot prepare private terminal");
                    }
                }
            }

            private <T> T admitted(TerminalOperation<T> action) {
                return checked(() -> {
                    requireInteractive();
                    AospIdentityBackend.UserKey user = requireAuthenticated(); // outside the gate
                    if (selection.get() != selected || user != selected.user) {
                        throw new SecurityException("Terminal belongs to a previous login");
                    }
                    try (RuntimeAdmission.Access access = admission.existing(selected.runtime)) {
                        requireRuntimeBinding(user, selected.runtime);
                        synchronized (io) {
                            if (closed) throw new SecurityException("Terminal is closed");
                        }
                        T result = action.run(access.deadlineNanos());
                        try {
                            access.checkCurrent();
                            requireRuntimeBinding(user, selected.runtime);
                            return result;
                        } catch (RuntimeException failure) {
                            if (result instanceof byte[]) wipe((byte[]) result);
                            throw failure;
                        }
                    }
                });
            }

            @Override public byte[] read() {
                return admitted(deadline -> {
                    synchronized (io) {
                        if (closed) throw new SecurityException("Terminal is closed");
                        if (eof) return null;
                        byte[] buffer = new byte[4096];
                        try {
                            int count = Os.read(nativeTerminal.master.getFileDescriptor(), buffer, 0, buffer.length);
                            if (count == 0) { eof = true; return null; }
                            return Arrays.copyOf(buffer, count);
                        } catch (ErrnoException failure) {
                            if (failure.errno == OsConstants.EAGAIN || failure.errno == OsConstants.EINTR) {
                                return new byte[0];
                            }
                            if (failure.errno == OsConstants.EIO) { eof = true; return null; }
                            throw new IllegalStateException("Private terminal read failed");
                        } catch (IOException failure) {
                            throw new IllegalStateException("Private terminal read interrupted");
                        } finally { wipe(buffer); }
                    }
                });
            }

            @Override public int write(byte[] bytes) {
                try {
                    if (bytes == null || bytes.length == 0 || bytes.length > 4096) {
                        throw new IllegalArgumentException("Invalid terminal input size");
                    }
                    return admitted(deadline -> {
                        synchronized (io) {
                            if (closed || eof) throw new SecurityException("Terminal input is closed");
                            try {
                                return Os.write(nativeTerminal.master.getFileDescriptor(), bytes, 0, bytes.length);
                            } catch (ErrnoException failure) {
                                if (failure.errno == OsConstants.EAGAIN || failure.errno == OsConstants.EINTR) return 0;
                                throw new IllegalStateException("Private terminal write failed");
                            } catch (IOException failure) {
                                throw new IllegalStateException("Private terminal write interrupted");
                            }
                        }
                    });
                } finally { wipe(bytes); }
            }

            @Override public void resize(int rows, int columns) {
                dimensions(rows, columns);
                admitted(deadline -> {
                    synchronized (io) {
                        if (closed) throw new SecurityException("Terminal is closed");
                        resizeOwned(rows, columns);
                    }
                    return null;
                });
            }

            private void resizeOwned(int rows, int columns) {
                try { TerminalNative.resize(nativeTerminal.master.getFd(), rows, columns); }
                catch (LinkageError unavailable) {
                    throw new IllegalStateException("Verified terminal support is unavailable");
                }
            }

            @Override public int exitStatus() {
                return admitted(this::collectResult);
            }

            int collectResult(long deadline) {
                synchronized (io) { if (resultCollected) return exit; }
                RuntimeBrokerProtocol.Reply reply = runtime.result(selected.user.id, selected.user.serial,
                        nativeTerminal.command, deadline);
                synchronized (io) {
                    if (reply.exited) {
                        int signal = reply.waitStatus & 0x7f;
                        exit = signal == 0 ? (reply.waitStatus >>> 8) & 0xff : 128 + signal;
                        resultCollected = true;
                        if (closed) terminals.remove(this);
                    }
                    return exit;
                }
            }

            boolean needsResult() {
                synchronized (io) {
                    if (!closeRequested || resultCollected) return false;
                }
                closeChannel(); // Retry an unconfirmed close before collecting metadata.
                return true;
            }

            void closeChannel() {
                synchronized (io) {
                    closeRequested = true;
                    if (!closed) {
                        try { nativeTerminal.close(); }
                        catch (IOException failure) {
                            // A failed close cannot satisfy the CE barrier. Keep
                            // ownership registered so that teardown must retry.
                            throw new IllegalStateException("Private terminal close is unconfirmed");
                        }
                        closed = true;
                    }
                    currentTerminal.compareAndSet(this, null);
                    if (resultCollected) terminals.remove(this);
                }
            }

            void stopped() {
                closeChannel();
                synchronized (io) { resultCollected = true; terminals.remove(this); }
            }

            @Override public void close() {
                owner.requireSameCaller();
                closeChannel();
            }
        }

        private void requireRuntimeBinding(AospIdentityBackend.UserKey user,
                RuntimeAdmission.Binding binding) {
            requireOwner();
            Selection selected = selection.get();
            if (selected == null || selected.user != user || selected.runtime != binding) {
                throw new SecurityException("Personal runtime session was revoked");
            }
            if (selected.interactive != interactiveEpoch.get()) {
                throw new SecurityException("Console interaction was revoked");
            }
        }

        @Override public void close() {
            owner.requireSameCaller();
            dispose();
        }

        private void dispose() {
            alive = false;
            selection.set(null);
            sealCurrentPackage();
            discardLoginPreparation();
            sessions.remove(this);
            try {
                closeCurrentTerminal();
            } catch (RuntimeException failure) {
                // The global owner retains and retries an unconfirmed close;
                // losing the Java session must not lose resource ownership.
            } finally {
                try { lifetime.unlinkToDeath(this, 0); }
                catch (NoSuchElementException notLinked) { /* Already unlinked. */ }
            }
        }

        @Override public void binderDied() { dispose(); }
    }

    private static void dimensions(int rows, int columns) {
        if (rows < 1 || rows > 1000 || columns < 1 || columns > 1000) {
            throw new IllegalArgumentException("Invalid terminal dimensions");
        }
    }

    private static LockscreenCredential credential(byte[] bytes) {
        if (bytes == null || bytes.length < 4 || bytes.length > 128) {
            throw new IllegalArgumentException("Password length is outside the protocol bounds");
        }
        char[] chars = new char[bytes.length];
        try {
            for (int i = 0; i < bytes.length; i++) {
                int value = bytes[i] & 0xff;
                if (value < 32 || value > 126) {
                    throw new IllegalArgumentException("Unsupported password character");
                }
                chars[i] = (char) value;
            }
            return LockscreenCredential.createPassword(CharBuffer.wrap(chars));
        } finally {
            Arrays.fill(chars, '\0');
            wipe(bytes);
        }
    }

    private static void wipe(byte[] bytes) { if (bytes != null) Arrays.fill(bytes, (byte) 0); }

    private static String safeName(String value) {
        if (value == null) return "(unnamed)";
        StringBuilder result = new StringBuilder();
        value.codePoints().limit(80).forEach(c -> {
            int type = Character.getType(c);
            if (Character.isISOControl(c) || type == Character.FORMAT
                    || type == Character.LINE_SEPARATOR || type == Character.PARAGRAPH_SEPARATOR) {
                result.append('?');
            } else result.appendCodePoint(c);
        });
        return result.toString();
    }

    private String runtimeDescription() {
        return runtime == null ? "runtime=not-installed" : "runtime=managed;check-linux-status";
    }

    private String describe(AospIdentityBackend.State state) {
        return "user=" + state.user.id + " serial=" + state.user.serial
                + " name=" + safeName(state.name) + " admin=" + state.admin
                + " enabled=" + state.enabled + " partial=" + state.partial
                + " foreground=" + state.foreground + " running=" + state.running
                + " ce=" + (state.ceUnlocked ? "unlocked" : "locked")
                + " " + runtimeDescription();
    }
}
