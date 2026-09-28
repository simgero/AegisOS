package org.aegisos.identity;

import android.app.ActivityManager;
import android.app.IActivityManager;
import android.app.IStopUserCallback;
import android.app.admin.PasswordMetrics;
import android.content.Context;
import android.content.pm.UserInfo;
import android.os.Binder;
import android.os.Looper;
import android.os.Process;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;
import android.os.storage.IStorageManager;

import com.android.internal.widget.ILockSettings;
import com.android.internal.widget.LockPatternUtils;
import com.android.internal.widget.LockscreenCredential;
import com.android.internal.widget.VerifyCredentialResponse;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Internal AOSP adapter, pinned to android-16.0.0_r1.
 *
 * This is NOT a Binder endpoint. The enclosing system service must authenticate the
 * client, bind its session to the original caller, and authorize the requested user
 * before calling this adapter. A UserKey is an identifier, never an authorization.
 * Runtime teardown and package transactions belong to the enclosing coordinator.
 *
 * Mutating operations serialize here; read-only status queries do not take that
 * lock. Blocking operations must run on a worker, not the system-server main
 * thread. Lifecycle callbacks must enqueue work rather than wait on this adapter.
 * Credentials are consumed and zeroized, never
 * retained, logged, written to disk, or converted into Strings.
 */
public final class AospIdentityBackend {
    private static final long STATE_TIMEOUT_MS = 30_000;
    private final UserManager users;
    private final IActivityManager activity;
    private final ILockSettings locks;
    private final IStorageManager storage;
    private final LockPatternUtils lockUtils;

    /** The serial distinguishes a deleted user from a later reuse of its numeric id. */
    public static final class UserKey {
        public final int id;
        public final int serial;

        public UserKey(int id, int serial) {
            if (id <= UserHandle.USER_SYSTEM || serial < 0) {
                throw new IllegalArgumentException("A personal AOSP user is required");
            }
            this.id = id;
            this.serial = serial;
        }

        @Override public boolean equals(Object other) {
            return other instanceof UserKey && ((UserKey) other).id == id
                    && ((UserKey) other).serial == serial;
        }

        @Override public int hashCode() { return 31 * id + serial; }
    }

    /** Unknown/unavailable state throws; it is never represented as 'locked'. */
    public static final class State {
        public final UserKey user;
        public final String name;
        public final boolean admin;
        public final boolean foreground;
        public final boolean running;
        public final boolean ceUnlocked;

        private State(UserInfo info, boolean foreground, boolean running, boolean unlocked) {
            user = new UserKey(info.id, info.serialNumber);
            name = info.name;
            admin = info.isAdmin();
            this.foreground = foreground;
            this.running = running;
            ceUnlocked = unlocked;
        }
    }

    /** Carries AOSP's retry delay without creating an independent lockout mechanism. */
    public static final class AuthenticationFailure extends SecurityException {
        public final int retryAfterMs;

        private AuthenticationFailure(int retryAfterMs) {
            super(retryAfterMs > 0 ? "AOSP requires an authentication delay"
                    : "AOSP rejected the credential");
            this.retryAfterMs = Math.max(0, retryAfterMs);
        }
    }

    public AospIdentityBackend(Context context) {
        if (Process.myUid() != Process.SYSTEM_UID) {
            throw new SecurityException("Identity backend requires the system service");
        }
        users = Objects.requireNonNull(context.getSystemService(UserManager.class));
        activity = Objects.requireNonNull(ActivityManager.getService());
        locks = Objects.requireNonNull(ILockSettings.Stub.asInterface(
                ServiceManager.getService("lock_settings")), "LockSettings unavailable");
        storage = Objects.requireNonNull(IStorageManager.Stub.asInterface(
                ServiceManager.getService("mount")), "StorageManager unavailable");
        lockUtils = new LockPatternUtils(context);
    }

    private static boolean personal(UserInfo info) {
        return info != null && info.id != UserHandle.USER_SYSTEM && info.isEnabled()
                && !info.partial && !info.preCreated && !info.isGuest()
                && UserManager.USER_TYPE_FULL_SECONDARY.equals(info.userType);
    }

    private UserInfo requireCurrent(UserKey key) {
        UserInfo info = users.getUserInfo(key.id);
        if (!personal(info) || info.serialNumber != key.serial) {
            throw new SecurityException("AOSP user was removed, replaced or disabled");
        }
        return info;
    }

    /** Caller-facing visibility filtering belongs to the authenticated service. */
    public List<UserKey> personalUsers() {
        long identity = Binder.clearCallingIdentity();
        try {
            List<UserKey> result = new ArrayList<>();
            for (UserInfo info : users.getAliveUsers()) {
                if (personal(info)) result.add(new UserKey(info.id, info.serialNumber));
            }
            return result;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public UserKey resolveName(String name) {
        Objects.requireNonNull(name);
        long identity = Binder.clearCallingIdentity();
        try {
            UserKey result = null;
            for (UserInfo info : users.getAliveUsers()) {
                if (!personal(info) || !name.equals(info.name)) continue;
                if (result != null) throw new IllegalArgumentException("Ambiguous AOSP user name");
                result = new UserKey(info.id, info.serialNumber);
            }
            if (result == null) throw new IllegalArgumentException("No such personal AOSP user");
            return result;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public State state(UserKey key) throws RemoteException {
        long identity = Binder.clearCallingIdentity();
        try {
            UserInfo info = requireCurrent(key);
            boolean foreground = activity.getCurrentUserId() == key.id;
            boolean running = users.isUserRunning(key.id);
            // Use the Binder API directly: absence/failure must not look like locked storage.
            boolean unlocked = storage.isCeStorageUnlocked(key.id);
            requireCurrent(key);
            return new State(info, foreground, running, unlocked);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private static void workerThread() {
        if (Looper.getMainLooper().isCurrentThread()) {
            throw new IllegalStateException("Identity operation cannot block the main thread");
        }
    }

    private static void password(LockscreenCredential credential) {
        Objects.requireNonNull(credential);
        if (!credential.isPassword()) {
            throw new IllegalArgumentException("A password credential is required");
        }
        credential.validateBasicRequirements();
    }

    private interface Condition { boolean satisfied() throws RemoteException; }

    private void await(UserKey key, Condition condition, String failure) throws RemoteException {
        long deadline = SystemClock.elapsedRealtime() + STATE_TIMEOUT_MS;
        do {
            requireCurrent(key);
            if (condition.satisfied()) {
                requireCurrent(key);
                return;
            }
            if (Thread.currentThread().isInterrupted()) {
                throw new IllegalStateException("Identity operation interrupted");
            }
            SystemClock.sleep(50);
        } while (SystemClock.elapsedRealtime() < deadline);
        throw new IllegalStateException(failure);
    }

    private void verify(UserKey key, LockscreenCredential credential) throws RemoteException {
        password(credential);
        requireCurrent(key);
        if (locks.getCredentialType(key.id) != LockPatternUtils.CREDENTIAL_TYPE_PASSWORD) {
            throw new SecurityException("Target AOSP user has no password credential");
        }
        if (!activity.startUserInBackground(key.id)) {
            throw new IllegalStateException("AOSP did not start the target user");
        }
        // No exported CE keys, escrow token, cached challenge or password-handle request.
        VerifyCredentialResponse response = locks.verifyCredential(credential, key.id, 0);
        if (response == null) throw new IllegalStateException("No AOSP credential response");
        if (response.getResponseCode() != VerifyCredentialResponse.RESPONSE_OK) {
            throw new AuthenticationFailure(response.getResponseCode()
                    == VerifyCredentialResponse.RESPONSE_RETRY ? response.getTimeout() : 0);
        }
        await(key, () -> users.isUserUnlocked(key.id) && storage.isCeStorageUnlocked(key.id),
                "AOSP authenticated but user/storage unlock was not confirmed");
    }

    /** Verifies even an already-unlocked target. Foreground state grants no authority. */
    public synchronized State authenticate(UserKey key, LockscreenCredential credential,
            boolean bringToForeground) throws RemoteException {
        long identity = Binder.clearCallingIdentity();
        try (LockscreenCredential owned = credential) {
            workerThread();
            verify(key, owned);
            if (bringToForeground) {
                if (!activity.switchUser(key.id)) {
                    throw new IllegalStateException("AOSP refused the user switch");
                }
                await(key, () -> activity.getCurrentUserId() == key.id,
                        "AOSP user switch was not confirmed");
            }
            State resultState = state(key);
            if (!resultState.running || !resultState.ceUnlocked
                    || (bringToForeground && !resultState.foreground)) {
                throw new IllegalStateException("AOSP user state changed during authentication");
            }
            return resultState;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public synchronized void changePassword(UserKey key, LockscreenCredential previous,
            LockscreenCredential replacement) throws RemoteException {
        long identity = Binder.clearCallingIdentity();
        byte[] historyFactor = null;
        try (LockscreenCredential oldOwned = previous; LockscreenCredential newOwned = replacement) {
            workerThread();
            password(newOwned);
            verify(key, oldOwned);
            if (!PasswordMetrics.validatePasswordMetrics(lockUtils.getRequestedPasswordMetrics(key.id),
                    lockUtils.getRequestedPasswordComplexity(key.id),
                    PasswordMetrics.computeForCredential(newOwned)).isEmpty()) {
                throw new IllegalArgumentException("Password does not satisfy AOSP policy");
            }
            historyFactor = locks.getHashFactor(oldOwned, key.id);
            if (historyFactor == null) throw new IllegalStateException("AOSP password history unavailable");
            if (lockUtils.checkPasswordHistory(newOwned.getCredential(), historyFactor, key.id)) {
                throw new IllegalArgumentException("Password is in AOSP password history");
            }
            requireCurrent(key);
            if (!locks.setLockCredential(newOwned, oldOwned, key.id)) {
                throw new AuthenticationFailure(0);
            }
            requireCurrent(key);
        } finally {
            if (historyFactor != null) Arrays.fill(historyFactor, (byte) 0);
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Android part of logout ONLY. The coordinator must first stop and reap the
     * user's runtimes/transactions and remove their mounts/IPC resources. It must
     * keep new runtime starts blocked until this method succeeds or repair finishes.
     * A callback alone is insufficient: AOSP evicts the CE key asynchronously later.
     */
    public synchronized State stopAndroidUserAndLock(UserKey key) throws RemoteException {
        long identity = Binder.clearCallingIdentity();
        try {
            workerThread();
            requireCurrent(key);
            if (activity.getCurrentUserId() == key.id) {
                if (!activity.switchUser(UserHandle.USER_SYSTEM)) {
                    throw new IllegalStateException("AOSP refused to leave the personal user");
                }
                await(key, () -> activity.getCurrentUserId() == UserHandle.USER_SYSTEM,
                        "System-user handover was not confirmed");
            }
            AtomicBoolean stopped = new AtomicBoolean();
            AtomicBoolean aborted = new AtomicBoolean();
            int result = activity.stopUserWithCallback(key.id, new IStopUserCallback.Stub() {
                @Override public void userStopped(int id) { if (id == key.id) stopped.set(true); }
                @Override public void userStopAborted(int id) { if (id == key.id) aborted.set(true); }
            });
            if (result != ActivityManager.USER_OP_SUCCESS) {
                throw new IllegalStateException("AOSP refused to stop the personal user");
            }
            await(key, () -> {
                if (aborted.get()) throw new IllegalStateException("AOSP aborted user stop");
                return stopped.get() && !users.isUserRunning(key.id)
                        && !storage.isCeStorageUnlocked(key.id);
            }, "Android user stop and CE storage locking were not both confirmed");
            State resultState = state(key);
            if (resultState.running || resultState.ceUnlocked || resultState.foreground) {
                throw new IllegalStateException("AOSP user restarted during storage locking");
            }
            return resultState;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }
}
