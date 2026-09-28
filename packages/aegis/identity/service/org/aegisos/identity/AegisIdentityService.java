package org.aegisos.identity;

import android.content.Context;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;
import android.os.ServiceSpecificException;
import android.os.SystemProperties;

import com.android.internal.widget.LockscreenCredential;
import com.android.server.SystemService;
import com.android.server.aegis.AegisRuntimeStorage;

import java.nio.CharBuffer;
import java.util.Arrays;
import java.util.NoSuchElementException;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
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
    private final ConcurrentHashMap<Integer, AtomicLong> revocations = new ConcurrentHashMap<>();
    private AospIdentityBackend backend;
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
                // No public PTY/package endpoint is enabled in this stage; all
                // CE/mount/process references belong to the native owner.
                revokeTerminalBindings(user);
                runtime.stopAndReleaseAll(user, deadline);
            });
            AegisRuntimeStorage.register(new RuntimeStorageController(admission));
        } else if (!"absent".equals(mode)) {
            throw new IllegalStateException("Unknown runtime lifecycle mode");
        }
        backend = new AospIdentityBackend(getContext());
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
        if (phase == PHASE_BOOT_COMPLETED) bootCompleted = true;
    }

    private static void requireRuntimeAbsent() {
        // User deletion still requires AOSP's confirmed cleanup before numeric
        // ID release. Do not enable managed removal with a post-removal retry.
        if (!"absent".equals(SystemProperties.get("ro.aegis.runtime.mode"))) {
            throw new IllegalStateException("Runtime lifecycle coordinator is not configured");
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
            Selection selected = session.selection.get();
            if (selected != null && selected.user.id == userId) {
                // A concurrent switch to another user must not be cleared by
                // this old user's storage callback. No identity monitor here.
                session.selection.compareAndSet(selected, null);
            }
        }
    }

    private static final class Selection {
        final AospIdentityBackend.UserKey user;
        final RuntimeAdmission.Binding runtime;
        Selection(AospIdentityBackend.UserKey user, RuntimeAdmission.Binding runtime) {
            this.user = user;
            this.runtime = runtime;
        }
    }

    private interface Operation<T> { T run() throws RemoteException; }

    private final class Session extends IAegisSession.Stub implements IBinder.DeathRecipient {
        private final CallerProcess owner;
        private final IBinder lifetime;
        private volatile boolean alive = true;
        private final AtomicReference<Selection> selection = new AtomicReference<>();

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
                    requireOwner();
                    return result;
                } catch (AospIdentityBackend.AuthenticationFailure e) {
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
                    || selection.get() != selected) {
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

        @Override public String login(String name, byte[] password) {
            try {
                return checked(() -> {
                    AospIdentityBackend.UserKey target = backend.resolveName(name);
                    long before = epoch(target.id);
                    RuntimeAdmission.AuthenticationAttempt attempt = admission == null ? null
                            : admission.beforeAuthentication(target.id, target.serial);
                    AospIdentityBackend.State state;
                    try {
                        // No runtime gate is held across LockSettings/UserManager/vold.
                        try (LockscreenCredential credential = credential(password)) {
                            state = backend.authenticate(target, credential, true);
                        }
                        requireOwner();
                        if (epoch(target.id) != before || !state.user.equals(target)
                                || !state.enabled || state.partial || !state.running || !state.ceUnlocked) {
                            throw new IllegalStateException("User changed during authentication");
                        }
                        if (admission == null) {
                            Selection selected = new Selection(target, null);
                            selection.set(selected);
                            if (epoch(target.id) != before) {
                                selection.compareAndSet(selected, null);
                                throw new IllegalStateException("User stopped during session binding");
                            }
                        } else {
                            try (RuntimeAdmission.Access access = admission.afterAuthentication(attempt)) {
                                // Reconciliation may have retired predecessor sessions. The
                                // current attempt has fresh AOSP proof and a different epoch.
                                requireOwner();
                                Selection selected = new Selection(target, access.binding());
                                selection.set(selected);
                                try { access.checkCurrent(); }
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
                    requireRuntimeAbsent();
                    try (LockscreenCredential admin = credential(adminPassword)) {
                        backend.removePersonalUser(actor, admin, target, mutationGuard(actor));
                    }
                    revoke(target.id);
                    return "AOSP user absent; user stopped and CE storage locked"
                            + " user=" + target.id + " serial=" + target.serial
                            + " runtime=not-installed";
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
                try (RuntimeAdmission.Access access = admission.existing(binding)) {
                    requireRuntimeBinding(user, binding);
                    int state;
                    if (operation == RuntimeBrokerProtocol.START) {
                        runtime.start(user.id, user.serial, access.deadlineNanos());
                        state = RuntimeBrokerProtocol.READY;
                    } else if (operation == RuntimeBrokerProtocol.STOP_USER) {
                        // Stop keeps AOSP authentication and CE unlocked. It does
                        // not revoke other clients' identity or claim a logout.
                        runtime.stopAndReleaseAll(user.id, access.deadlineNanos());
                        state = RuntimeBrokerProtocol.ABSENT;
                    } else {
                        state = runtime.state(user.id, user.serial, access.deadlineNanos());
                    }
                    access.checkCurrent();
                    requireRuntimeBinding(user, binding);
                    return "user=" + user.id + " serial=" + user.serial + " runtime="
                            + (state == RuntimeBrokerProtocol.READY ? "ready"
                                : state == RuntimeBrokerProtocol.ABSENT ? "stopped" : "sealed")
                            + " ce=unlocked";
                }
            });
        }

        private void requireRuntimeBinding(AospIdentityBackend.UserKey user,
                RuntimeAdmission.Binding binding) {
            requireOwner();
            Selection selected = selection.get();
            if (selected == null || selected.user != user || selected.runtime != binding) {
                throw new SecurityException("Personal runtime session was revoked");
            }
        }

        @Override public void close() {
            owner.requireSameCaller();
            dispose();
        }

        private void dispose() {
            alive = false;
            selection.set(null);
            sessions.remove(this);
            try {
                lifetime.unlinkToDeath(this, 0);
            } catch (NoSuchElementException notLinked) {
                // linkToDeath itself may have failed, or death/close already removed the link.
            }
        }

        @Override public void binderDied() { dispose(); }
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
