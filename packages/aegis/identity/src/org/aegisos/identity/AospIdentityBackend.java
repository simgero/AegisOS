package org.aegisos.identity;

import android.app.ActivityManager;
import android.app.IActivityManager;
import android.app.IStopUserCallback;
import android.app.admin.PasswordMetrics;
import android.app.admin.DevicePolicyManager;
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
import android.os.storage.VolumeInfo;
import android.os.storage.VolumeRecord;

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
 * Caller-owned credentials are consumed and zeroized, never retained, logged,
 * written to disk, or converted into Strings. AOSP owns its parcelled copies.
 */
public final class AospIdentityBackend {
    private static final long STATE_TIMEOUT_MS = 30_000;
    private final UserManager users;
    private final IActivityManager activity;
    private final ILockSettings locks;
    private final IStorageManager storage;
    private final LockPatternUtils lockUtils;
    private final AospSetupState setup;

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
        public final boolean enabled;
        public final boolean partial;
        public final boolean foreground;
        public final boolean running;
        public final boolean ceUnlocked;

        private State(UserInfo info, boolean foreground, boolean running, boolean unlocked) {
            user = new UserKey(info.id, info.serialNumber);
            name = info.name;
            admin = info.isAdmin();
            enabled = info.isEnabled();
            partial = info.partial;
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
        locks = CredentialTransport.connect(Objects.requireNonNull(
                ServiceManager.getService("lock_settings"), "LockSettings unavailable"));
        storage = Objects.requireNonNull(IStorageManager.Stub.asInterface(
                ServiceManager.getService("mount")), "StorageManager unavailable");
        lockUtils = new LockPatternUtils(context);
        setup = new AospSetupState(context.getContentResolver());
    }

    private static boolean personal(UserInfo info) {
        return info != null && info.id != UserHandle.USER_SYSTEM
                && !info.preCreated && !info.isGuest()
                && UserManager.USER_TYPE_FULL_SECONDARY.equals(info.userType);
    }

    private UserInfo requireExisting(UserKey key) {
        UserInfo info = users.getUserInfo(key.id);
        if (!personal(info) || info.serialNumber != key.serial) {
            throw new SecurityException("AOSP user was removed or replaced");
        }
        return info;
    }

    private UserInfo requireCurrent(UserKey key) {
        UserInfo info = requireExisting(key);
        if (!info.isEnabled() || info.partial) {
            throw new SecurityException("AOSP user is disabled or incomplete");
        }
        return info;
    }

    /** Caller-facing visibility filtering belongs to the authenticated service. */
    public List<UserKey> personalUsers() {
        long identity = Binder.clearCallingIdentity();
        try {
            List<UserKey> result = new ArrayList<>();
            for (UserInfo info : users.getUsers(false, false, false)) {
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
            for (UserInfo info : users.getUsers(false, false, false)) {
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
            UserInfo info = requireExisting(key);
            boolean foreground = activity.getCurrentUserId() == key.id;
            boolean running = users.isUserRunning(key.id);
            // Use the Binder API directly: absence/failure must not look like locked storage.
            boolean unlocked = storage.isCeStorageUnlocked(key.id);
            requireExisting(key);
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
        await(key, condition, failure, false);
    }

    private void await(UserKey key, Condition condition, String failure, boolean allowDisabled)
            throws RemoteException {
        long deadline = SystemClock.elapsedRealtime() + STATE_TIMEOUT_MS;
        do {
            if (allowDisabled) requireExisting(key); else requireCurrent(key);
            if (condition.satisfied()) {
                if (allowDisabled) requireExisting(key); else requireCurrent(key);
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
        checkCredentialResponse(response);
        await(key, () -> users.isUserUnlocked(key.id) && storage.isCeStorageUnlocked(key.id),
                "AOSP authenticated but user/storage unlock was not confirmed");
    }

    private static void checkCredentialResponse(VerifyCredentialResponse response) {
        if (response == null) throw new IllegalStateException("No AOSP credential response");
        if (response.getResponseCode() != VerifyCredentialResponse.RESPONSE_OK) {
            throw new AuthenticationFailure(response.getResponseCode()
                    == VerifyCredentialResponse.RESPONSE_RETRY ? response.getTimeout() : 0);
        }
    }

    /**
     * Selects a personal Android login target before the CLI asks for a password.
     * This grants no personal identity, does not verify credentials or call vold,
     * and does not start a GNU context. The system service must additionally wait
     * for ActivityManagerInternal's completed switch, not only the early userId.
     */
    public synchronized void selectLoginTarget(UserKey key) throws RemoteException {
        long identity = Binder.clearCallingIdentity();
        try {
            workerThread();
            requireCurrent(key);
            if (locks.getCredentialType(key.id) != LockPatternUtils.CREDENTIAL_TYPE_PASSWORD) {
                throw new SecurityException("Target AOSP user has no password credential");
            }
            if (!activity.switchUser(key.id)) {
                throw new IllegalStateException("AOSP refused the login target");
            }
            requireCurrent(key);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Verifies even an already-unlocked target. This never initiates a foreground
     * transition after verification; the terminal prepares that before password
     * input. Foreground state itself grants no personal authority.
     */
    public synchronized State authenticate(UserKey key, LockscreenCredential credential)
            throws RemoteException {
        long identity = Binder.clearCallingIdentity();
        try (LockscreenCredential owned = credential) {
            workerThread();
            verify(key, owned);
            State resultState = state(key);
            if (!resultState.enabled || resultState.partial
                    || !resultState.running || !resultState.ceUnlocked) {
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

    /** Rechecks the validated client; must not call back into framework lifecycle operations. */
    public interface OperationGuard { void check(); }

    private interface CreationCheck { void check(UserKey created) throws RemoteException; }

    private void administrator(UserKey actor, String restriction, boolean grantingAdmin) {
        if (!requireCurrent(actor).isAdmin()) {
            throw new SecurityException("AOSP administrator required");
        }
        if (users.hasUserRestriction(restriction, UserHandle.of(actor.id))
                || (grantingAdmin && users.hasUserRestriction(UserManager.DISALLOW_GRANT_ADMIN,
                        UserHandle.of(actor.id)))) {
            throw new SecurityException("AOSP restricts this administration action");
        }
    }

    private static void userName(String name) {
        if (name == null || name.isBlank() || !name.equals(name.strip())
                || name.length() > UserManager.MAX_USER_NAME_LENGTH) {
            throw new IllegalArgumentException("Invalid personal user name");
        }
        name.codePoints().forEach(c -> {
            int type = Character.getType(c);
            if (Character.isISOControl(c) || type == Character.FORMAT
                    || type == Character.LINE_SEPARATOR || type == Character.PARAGRAPH_SEPARATOR) {
                throw new IllegalArgumentException("Invalid personal user name");
            }
        });
    }

    private void unusedName(String name) {
        for (UserInfo info : users.getUsers(false, false, false)) {
            if (name.equals(info.name)) {
                throw new IllegalArgumentException("An AOSP user already has this name");
            }
        }
    }

    private void validateNewPassword(int userId, LockscreenCredential credential) {
        password(credential);
        if (!PasswordMetrics.validatePasswordMetrics(lockUtils.getRequestedPasswordMetrics(userId),
                lockUtils.getRequestedPasswordComplexity(userId),
                PasswordMetrics.computeForCredential(credential)).isEmpty()) {
            throw new IllegalArgumentException("Password does not satisfy AOSP policy");
        }
    }

    /** No credential is cached and no reusable admin ticket escapes this operation. */
    public synchronized State createPersonalUser(UserKey actor, LockscreenCredential adminPassword,
            String name, LockscreenCredential initialPassword, boolean makeAdmin,
            OperationGuard guard) throws RemoteException {
        long identity = Binder.clearCallingIdentity();
        try (LockscreenCredential adminOwned = adminPassword;
                LockscreenCredential initialOwned = initialPassword) {
            workerThread();
            userName(name);
            guard.check();
            administrator(actor, UserManager.DISALLOW_ADD_USER, makeAdmin);
            verify(actor, adminOwned); // fresh AOSP authentication, even if already unlocked
            return createProtectedUser(name, initialOwned, makeAdmin, created -> {
                guard.check();
                administrator(actor, UserManager.DISALLOW_ADD_USER, makeAdmin);
            });
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private void onlyBootstrapUser(UserKey allowed) {
        for (UserInfo info : users.getUsers(false, false, false)) {
            if (info.id != UserHandle.USER_SYSTEM
                    && UserManager.USER_TYPE_FULL_SECONDARY.equals(info.userType)
                    && (allowed == null || info.id != allowed.id
                            || info.serialNumber != allowed.serial)) {
                // Partial, disabled, pre-created and pending-removal secondary users count too.
                throw new SecurityException("Personal AOSP users already exist");
            }
        }
    }

    private void bootstrapRestrictions() {
        if (users.hasUserRestriction(UserManager.DISALLOW_ADD_USER, UserHandle.SYSTEM)
                || users.hasUserRestriction(UserManager.DISALLOW_GRANT_ADMIN, UserHandle.SYSTEM)) {
            throw new SecurityException("AOSP restricts initial administrator creation");
        }
    }

    public String setupStatus() {
        long identity = Binder.clearCallingIdentity();
        try {
            String value = setup.read();
            if (value == null) {
                for (UserInfo info : users.getUsers(false, false, false)) {
                    if (info.id != UserHandle.USER_SYSTEM
                            && UserManager.USER_TYPE_FULL_SECONDARY.equals(info.userType)) {
                        return "existing-users";
                    }
                }
                return "available";
            }
            InitialAdminRecovery.Record record;
            try {
                record = InitialAdminRecovery.Record.parse(value);
            } catch (IllegalStateException invalid) {
                return "invalid";
            }
            if (record.phase == InitialAdminRecovery.Phase.COMPLETE) return "complete";
            if (record.phase == InitialAdminRecovery.Phase.RESERVED
                    || record.phase == InitialAdminRecovery.Phase.CREATED) {
                return "incomplete";
            }
            return "invalid";
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /** Caller must be the explicitly authorized development root console, not just any session. */
    public synchronized State bootstrapFirstAdmin(String name, LockscreenCredential initialPassword,
            OperationGuard guard) throws RemoteException {
        long identity = Binder.clearCallingIdentity();
        try (LockscreenCredential owned = initialPassword) {
            workerThread();
            userName(name);
            preflightPassword(owned);
            unusedName(name);
            guard.check();
            onlyBootstrapUser(null);
            bootstrapRestrictions();
            if (setup.read() != null) {
                throw new SecurityException("First-admin setup has already been reserved or completed");
            }
            setup.write("reserved"); // never automatically cleared, including after failure
            return finishFirstAdminCreation(name, owned, guard);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private State finishFirstAdminCreation(String name, LockscreenCredential owned,
            OperationGuard guard) throws RemoteException {
        State created = createProtectedUser(name, owned, true, pending -> {
            guard.check();
            onlyBootstrapUser(pending);
            bootstrapRestrictions();
            String expected = pending == null ? "reserved"
                    : "created:" + pending.id + ":" + pending.serial;
            String recorded = setup.read();
            if (pending != null && "reserved".equals(recorded)) setup.write(expected);
            else if (!expected.equals(recorded)) {
                throw new SecurityException("Provisioning reservation changed");
            }
        });
        guard.check();
        onlyBootstrapUser(created.user);
        bootstrapRestrictions();
        if (!("created:" + created.user.id + ":" + created.user.serial).equals(setup.read())) {
            throw new SecurityException("Provisioning reservation changed before completion");
        }
        setup.write("complete:" + created.user.id + ":" + created.user.serial);
        return created;
    }

    /** Explicit continuation, not a reset or an adoption of an unrecorded user. */
    public synchronized State resumeFirstAdmin(String name, LockscreenCredential credential,
            OperationGuard guard) throws RemoteException {
        long identity = Binder.clearCallingIdentity();
        try (LockscreenCredential owned = credential) {
            workerThread();
            userName(name);
            password(owned);
            guard.check();
            bootstrapRestrictions();
            InitialAdminRecovery.Record record = InitialAdminRecovery.Record.parse(setup.read());
            if (record.phase == InitialAdminRecovery.Phase.RESERVED) {
                // If creation happened before its id/serial could be recorded, do not guess
                // ownership from a matching name. Any secondary user blocks this path.
                onlyBootstrapUser(null);
                return finishFirstAdminCreation(name, owned, guard);
            }
            if (record.phase != InitialAdminRecovery.Phase.CREATED) {
                throw new SecurityException("No recorded incomplete first-admin enrollment");
            }
            UserKey key = record.user;
            try {
                InitialAdminRecovery.resume(record, name, new InitialAdminRecovery.Platform() {
                    @Override public void guard() {
                        guard.check();
                        bootstrapRestrictions();
                        onlyBootstrapUser(key);
                    }
                    @Override public String readRecord() { return setup.read(); }
                    @Override public InitialAdminRecovery.Snapshot inspect() throws RemoteException {
                        State s = state(key);
                        int type = locks.getCredentialType(key.id);
                        requireExisting(key);
                        return new InitialAdminRecovery.Snapshot(s.user, s.name, s.enabled, s.admin,
                                s.partial, s.running, s.foreground, s.ceUnlocked,
                                type == LockPatternUtils.CREDENTIAL_TYPE_NONE
                                        ? InitialAdminRecovery.Credential.NONE
                                        : type == LockPatternUtils.CREDENTIAL_TYPE_PASSWORD
                                                ? InitialAdminRecovery.Credential.PASSWORD
                                                : InitialAdminRecovery.Credential.OTHER);
                    }
                    @Override public void enrollPassword() throws RemoteException {
                        enrollInitialPassword(key, owned);
                    }
                    @Override public void verifyPassword() throws RemoteException {
                        if (requireExisting(key).isEnabled()) {
                            verify(key, owned);
                        } else {
                            requireUnstartedDisabled(key);
                            if (locks.getCredentialType(key.id) != LockPatternUtils.CREDENTIAL_TYPE_PASSWORD) {
                                throw new SecurityException("Recorded password credential changed");
                            }
                            // LSS verifies without enabling the account, but also unlocks CE.
                            // The coordinator must therefore confirm a subsequent stop/lock.
                            checkCredentialResponse(locks.verifyCredential(owned, key.id, 0));
                        }
                    }
                    @Override public void stopAndLock() throws RemoteException { stopAndroidUserAndLock(key); }
                    @Override public void grantAdmin() { users.setUserAdmin(key.id); }
                    @Override public void enable() { users.setUserEnabled(key.id); }
                    @Override public void writeComplete(String value) { setup.write(value); }
                });
                guard.check();
                State result = state(key);
                if (!result.enabled || !result.admin || result.partial || result.running
                        || result.foreground || result.ceUnlocked || !name.equals(result.name)) {
                    throw new IllegalStateException("Recovered administrator changed before return");
                }
                return result;
            } catch (RemoteException | RuntimeException failure) {
                relockIncomplete(key);
                throw failure;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private static void preflightPassword(LockscreenCredential credential) {
        password(credential);
        if (!PasswordMetrics.validatePasswordMetrics(
                new PasswordMetrics(LockPatternUtils.CREDENTIAL_TYPE_NONE),
                DevicePolicyManager.PASSWORD_COMPLEXITY_NONE,
                PasswordMetrics.computeForCredential(credential)).isEmpty()) {
            throw new IllegalArgumentException("Password does not satisfy base AOSP policy");
        }
    }

    private State createProtectedUser(String name, LockscreenCredential initialPassword,
            boolean makeAdmin, CreationCheck authority) throws RemoteException {
        preflightPassword(initialPassword);
        unusedName(name);
        authority.check(null);
        // Do not grant ADMIN until after credential enrollment and CE locking.
        UserInfo created = users.createUser(name, UserManager.USER_TYPE_FULL_SECONDARY,
                UserInfo.FLAG_DISABLED);
        if (created == null || !personal(created) || created.partial) {
            throw new IllegalStateException("AOSP did not finish creating a personal user");
        }
        UserKey key = new UserKey(created.id, created.serialNumber);
        try {
            requireUnstartedDisabled(key);
            authority.check(key);
            enrollInitialPassword(key, initialPassword);
            // AOSP initially unlocks a freshly created CE key even while the user is stopped.
            // lockCeStorage can log a vold failure and return, so its result must be re-read.
            storage.lockCeStorage(key.id);
            requireUnstartedDisabled(key);
            if (storage.isCeStorageUnlocked(key.id)) {
                throw new IllegalStateException("New user's CE storage is still unlocked");
            }
            authority.check(key);
            if (makeAdmin) users.setUserAdmin(key.id);
            requireUnstartedDisabled(key);
            authority.check(key);
            users.setUserEnabled(key.id);
            State result = state(key);
            if (!result.enabled || result.partial || result.running || result.ceUnlocked
                    || result.admin != makeAdmin || !name.equals(result.name)) {
                throw new IllegalStateException("Provisioned AOSP state was not confirmed");
            }
            return result;
        } catch (RemoteException | RuntimeException failure) {
            // Preserve incomplete accounts for explicit admin inspection/removal. Never enable
            // or reset a failed account, nor silently delete an account that became usable.
            relockIncomplete(key);
            throw failure;
        }
    }

    private void enrollInitialPassword(UserKey key, LockscreenCredential credential) throws RemoteException {
        requireUnstartedDisabled(key);
        validateNewPassword(key.id, credential);
        if (locks.getCredentialType(key.id) != LockPatternUtils.CREDENTIAL_TYPE_NONE) {
            throw new IllegalStateException("AOSP enrollment already has a credential");
        }
        try (LockscreenCredential empty = LockscreenCredential.createNone()) {
            if (!locks.setLockCredential(credential, empty, key.id)) {
                throw new IllegalStateException("AOSP rejected initial credential enrollment");
            }
        }
        requireUnstartedDisabled(key);
        if (locks.getCredentialType(key.id) != LockPatternUtils.CREDENTIAL_TYPE_PASSWORD) {
            throw new IllegalStateException("Initial AOSP password was not confirmed");
        }
    }

    private void relockIncomplete(UserKey key) {
        // Finish the accepted enrollment's protective cleanup even if its client died.
        // Never delete data, alter credentials or stop a now-enabled user's session here.
        try {
            UserInfo current = requireExisting(key);
            if (!current.isEnabled() && !current.partial) stopAndroidUserAndLock(key);
        } catch (RemoteException | RuntimeException ignored) { }
    }

    private void requireUnstartedDisabled(UserKey key) {
        UserInfo info = requireExisting(key);
        if (info.isEnabled() || info.partial || users.isUserRunning(key.id)) {
            throw new IllegalStateException("New account became enabled or running unexpectedly");
        }
    }

    /** Runtime processes/mounts must already be quiesced by the enclosing coordinator. */
    public synchronized void removePersonalUser(UserKey actor, LockscreenCredential adminPassword,
            UserKey target, OperationGuard guard) throws RemoteException {
        long identity = Binder.clearCallingIdentity();
        try (LockscreenCredential owned = adminPassword) {
            workerThread();
            if (actor.equals(target)) {
                throw new SecurityException("Removal requires a different administrator session");
            }
            guard.check();
            administrator(actor, UserManager.DISALLOW_REMOVE_USER, false);
            verify(actor, owned);
            requireInternalStorageOnly();
            UserInfo info = requireExisting(target);
            if (info.partial) throw new IllegalStateException("User removal or creation is incomplete");
            // The authenticated different administrator remains; system user 0 is never a target.
            guard.check();
            administrator(actor, UserManager.DISALLOW_REMOVE_USER, false);
            stopAndroidUserAndLock(target);
            guard.check();
            administrator(actor, UserManager.DISALLOW_REMOVE_USER, false);
            requireExisting(target);
            if (!users.removeUser(target.id)) {
                throw new IllegalStateException("AOSP did not accept user removal");
            }
            long deadline = SystemClock.elapsedRealtime() + STATE_TIMEOUT_MS;
            do {
                UserInfo current = users.getUserInfo(target.id);
                if (current != null && current.serialNumber != target.serial) {
                    throw new IllegalStateException("User id was reused during removal");
                }
                if (current == null && !users.isUserRunning(target.id)
                        && !storage.isCeStorageUnlocked(target.id)) {
                    // Pinned AOSP finalization owns every destructive operation while
                    // the original UserData still reserves the ID. Never delete by an
                    // already-released numeric ID here; it may now belong to another user.
                    return;
                }
                if (Thread.currentThread().isInterrupted()) {
                    throw new IllegalStateException("User removal wait interrupted");
                }
                SystemClock.sleep(50);
            } while (SystemClock.elapsedRealtime() < deadline);
            throw new IllegalStateException("AOSP user removal has not completed");
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private void requireInternalStorageOnly() throws RemoteException {
        VolumeInfo[] volumes = storage.getVolumes(0);
        if (volumes == null) throw new IllegalStateException("Storage inventory unavailable");
        for (VolumeInfo volume : volumes) {
            if (volume == null) throw new IllegalStateException("Storage inventory incomplete");
            if (volume.getType() == VolumeInfo.TYPE_PRIVATE
                    && !VolumeInfo.ID_PRIVATE_INTERNAL.equals(volume.getId())) {
                throw new IllegalStateException("Adopted private volumes require removal integration");
            }
        }
        // A disconnected adopted disk can be absent from getVolumes while its record remains.
        // Never confirm full removal while private data on such a disk cannot be cleaned up.
        VolumeRecord[] records = storage.getVolumeRecords(0);
        if (records == null) throw new IllegalStateException("Storage records unavailable");
        for (VolumeRecord record : records) {
            if (record == null) throw new IllegalStateException("Storage records incomplete");
            if (record.getType() == VolumeInfo.TYPE_PRIVATE) {
                throw new IllegalStateException("Adopted private storage requires removal integration");
            }
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
            if (requireExisting(key).partial) {
                throw new IllegalStateException("Cannot stop an incomplete personal user");
            }
            if (!users.isUserRunning(key.id)) {
                // A never-started user can still have the initially unlocked key from creation.
                // ActivityManager's already-stopped callback does not itself evict that key.
                storage.lockCeStorage(key.id);
                State stopped = state(key);
                if (!stopped.running && !stopped.ceUnlocked && !stopped.foreground) return stopped;
            }
            if (activity.getCurrentUserId() == key.id) {
                if (!activity.switchUser(UserHandle.USER_SYSTEM)) {
                    throw new IllegalStateException("AOSP refused to leave the personal user");
                }
                await(key, () -> activity.getCurrentUserId() == UserHandle.USER_SYSTEM,
                        "System-user handover was not confirmed", true);
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
            }, "Android user stop and CE storage locking were not both confirmed", true);
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
