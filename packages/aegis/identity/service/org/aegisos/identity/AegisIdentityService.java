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

import java.nio.CharBuffer;
import java.util.Arrays;
import java.util.NoSuchElementException;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicLong;

/**
 * AOSP system-server service for the first developer-console integration.
 * No runtime is installed in this stage. A different runtime mode MUST supply
 * runtime teardown/transaction coordination before this service can be started.
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

    public AegisIdentityService(Context context) { super(context); }

    @Override public void onStart() {
        if (!Build.IS_DEBUGGABLE) {
            throw new IllegalStateException("Developer console requires a debuggable build");
        }
        requireRuntimeAbsent();
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

    private static void requireRuntimeAbsent() {
        // Explicit product contract, never a default when configuration is missing.
        // When a runtime is integrated this must be replaced by its real lifecycle coordinator.
        if (!"absent".equals(SystemProperties.get("ro.aegis.runtime.mode"))) {
            throw new IllegalStateException("Runtime lifecycle coordinator is not configured");
        }
    }

    private long epoch(int userId) {
        return revocations.computeIfAbsent(userId, ignored -> new AtomicLong()).get();
    }

    @Override public void onUserStopping(TargetUser user) {
        // Do not acquire operations here: user stop is awaited on a Binder worker.
        revoke(user.getUserIdentifier());
    }

    private void revoke(int userId) {
        revocations.computeIfAbsent(userId, ignored -> new AtomicLong()).incrementAndGet();
        for (Session session : sessions) {
            AospIdentityBackend.UserKey selected = session.selected;
            if (selected != null && selected.id == userId) session.selected = null;
        }
    }

    private interface Operation<T> { T run() throws RemoteException; }

    private final class Session extends IAegisSession.Stub implements IBinder.DeathRecipient {
        private final CallerProcess owner;
        private final IBinder lifetime;
        private volatile boolean alive = true;
        private volatile AospIdentityBackend.UserKey selected;

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
            AospIdentityBackend.UserKey user = selected;
            if (user == null) throw new SecurityException("Log in in this terminal first");
            AospIdentityBackend.State state = backend.state(user);
            if (!state.running || !state.ceUnlocked || selected != user) {
                selected = null;
                throw new SecurityException("Personal session is no longer unlocked");
            }
            return user;
        }

        @Override public String login(String name, byte[] password) {
            try {
                return checked(() -> {
                    AospIdentityBackend.UserKey target = backend.resolveName(name);
                    long before = epoch(target.id);
                    AospIdentityBackend.State state;
                    try (LockscreenCredential credential = credential(password)) {
                        state = backend.authenticate(target, credential, true);
                    }
                    requireOwner();
                    if (epoch(target.id) != before) {
                        throw new IllegalStateException("User stopped during authentication");
                    }
                    // This changes THIS terminal's binding only; background clients keep theirs.
                    selected = target;
                    if (epoch(target.id) != before) {
                        selected = null;
                        throw new IllegalStateException("User stopped during session binding");
                    }
                    return describe(state);
                });
            } finally {
                wipe(password);
            }
        }

        @Override public String status() {
            return checked(() -> {
                AospIdentityBackend.UserKey user = selected;
                if (user == null) return "terminal=unauthenticated runtime=not-installed";
                AospIdentityBackend.State state = backend.state(user);
                if (!state.running || !state.ceUnlocked) selected = null;
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
                            .append(" admin=").append(state.admin).append('\n');
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

        @Override public String logout() {
            return checked(() -> {
                AospIdentityBackend.UserKey user = requireAuthenticated();
                requireRuntimeAbsent();
                // Revoke all authority for this user before stopping Android. If stop fails,
                // status remains unauthenticated; a new login must verify an AOSP password.
                revoke(user.id);
                AospIdentityBackend.State stopped = backend.stopAndroidUserAndLock(user);
                return "Android user stopped; CE storage locked; runtime not installed\n"
                        + describe(stopped);
            });
        }

        @Override public void close() {
            owner.requireSameCaller();
            dispose();
        }

        private void dispose() {
            alive = false;
            selected = null;
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

    private static String describe(AospIdentityBackend.State state) {
        return "user=" + state.user.id + " serial=" + state.user.serial
                + " name=" + safeName(state.name) + " admin=" + state.admin
                + " foreground=" + state.foreground + " running=" + state.running
                + " ce=" + (state.ceUnlocked ? "unlocked" : "locked")
                + " runtime=not-installed";
    }
}
