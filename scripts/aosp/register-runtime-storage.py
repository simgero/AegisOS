#!/usr/bin/env python3
"""Prepare strictly pinned AOSP storage hooks; source editing only, never compilation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

STORAGE = 'services/core/java/com/android/server/StorageManagerService.java'
USERS = 'services/core/java/com/android/server/am/UserController.java'
RESILIENT = 'services/core/java/com/android/server/pm/ResilientAtomicFile.java'
MANAGER = 'services/core/java/com/android/server/pm/UserManagerService.java'
PREPARER = 'services/core/java/com/android/server/pm/UserDataPreparer.java'
INSTALLER = 'services/core/java/com/android/server/pm/Installer.java'
LOCK_SETTINGS = 'services/core/java/com/android/server/locksettings/LockSettingsService.java'
SYNTHETIC = 'services/core/java/com/android/server/locksettings/SyntheticPasswordManager.java'
PROTECTOR_CRYPTO = 'services/core/java/com/android/server/locksettings/SyntheticPasswordCrypto.java'
BIOMETRIC_REMOVAL = 'services/core/java/com/android/server/biometrics/sensors/RemovalClient.java'
FINGERPRINT_REMOVAL = 'services/core/java/com/android/server/biometrics/sensors/fingerprint/aidl/FingerprintRemovalClient.java'
FACE_REMOVAL = 'services/core/java/com/android/server/biometrics/sensors/face/aidl/FaceRemovalClient.java'
FINGERPRINT_RESPONSE = 'services/core/java/com/android/server/biometrics/sensors/fingerprint/aidl/AidlResponseHandler.java'
FACE_RESPONSE = 'services/core/java/com/android/server/biometrics/sensors/face/aidl/AidlResponseHandler.java'
BRIDGE = 'services/core/java/com/android/server/aegis/AegisRuntimeStorage.java'
SOURCE = 'packages/aegis/identity/platform/com/android/server/aegis/AegisRuntimeStorage.java'
REMOVAL_FILES = 'services/core/java/com/android/server/aegis/AegisRemovalFiles.java'
REMOVAL_SOURCE = 'packages/aegis/identity/platform/com/android/server/aegis/AegisRemovalFiles.java'
REMOVAL_DATA = 'services/core/java/com/android/server/aegis/AegisRemovalData.java'
REMOVAL_DATA_SOURCE = 'packages/aegis/identity/platform/com/android/server/aegis/AegisRemovalData.java'
SCHEMA_THREE_ORIGINALS = {STORAGE, USERS, RESILIENT, MANAGER, PREPARER, INSTALLER}
SCHEMA_FOUR_ORIGINALS = SCHEMA_THREE_ORIGINALS | {LOCK_SETTINGS, SYNTHETIC, PROTECTOR_CRYPTO}
BIOMETRIC_ORIGINALS = {BIOMETRIC_REMOVAL, FINGERPRINT_REMOVAL, FACE_REMOVAL,
                       FINGERPRINT_RESPONSE, FACE_RESPONSE}
ORIGINAL_FILES = SCHEMA_FOUR_ORIGINALS | BIOMETRIC_ORIGINALS
LEGACY_SOURCE_FILES = {BRIDGE: SOURCE, REMOVAL_FILES: REMOVAL_SOURCE, REMOVAL_DATA: REMOVAL_DATA_SOURCE}
PACKAGE_CREDENTIALS = 'services/core/java/com/android/server/aegis/AegisPackageCredentials.java'
PACKAGE_CREDENTIALS_SOURCE = 'packages/aegis/identity/platform/com/android/server/aegis/AegisPackageCredentials.java'
SOURCE_FILES = {**LEGACY_SOURCE_FILES, PACKAGE_CREDENTIALS: PACKAGE_CREDENTIALS_SOURCE}
MARKER = 'out/aegis-runtime-storage/sources.json'
PREFIX = 'com.android.server.aegis.AegisRuntimeStorage'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('Pinned AOSP hook anchor is missing or ambiguous')
    return text.replace(old, new, 1)


def patch_storage(data):
    text = data.decode('utf-8')
    for name, operation in (
            ('createUserStorageKeys', 'CREATE'), ('destroyUserStorageKeys', 'DESTROY'),
            ('setCeStorageProtection', 'PROTECT'), ('unlockCeStorage', 'UNLOCK'),
            ('lockCeStorage', 'LOCK'), ('destroyUserStorage', 'DESTROY')):
        start = f'    public void {name}('
        if text.count(start) != 1:
            raise ValueError('Storage method is missing or ambiguous')
        begin = text.index(start)
        end = text.index('\n    }\n', begin) + len('\n    }')
        method = text[begin:end]
        permission = f'        super.{name}_enforcePermission();\n'
        if method.count(permission) != 1:
            raise ValueError('Storage permission anchor changed')
        prefix, body = method.split(permission)
        body = body.removesuffix('\n    }').strip('\n')
        # A caller must not receive normal return/keyEvicted after vold failed.
        if operation in ('CREATE', 'DESTROY', 'LOCK'):
            old = '            Slog.wtf(TAG, e);\n'
            if operation == 'LOCK':
                old += '            return;\n'
            body = replace_once(body, old,
                    '            Slog.wtf(TAG, e);\n'
                    '            throw new IllegalStateException("AOSP storage mutation failed", e);\n')
        indented = '\n'.join('    ' + line if line else '' for line in body.splitlines())
        replacement = (prefix + permission + '\n'
                f'        try ({PREFIX}.Lease aegisStorageLease =\n'
                f'                {PREFIX}.begin(userId,\n'
                f'                        {PREFIX}.Operation.{operation})) {{\n'
                + indented + '\n        }\n    }')
        text = text[:begin] + replacement + text[end:]
    return text.encode('utf-8')


def patch_users(data):
    text = data.decode('utf-8')
    old = '''                mInjector.getStorageManager().lockCeStorage(userId);
            } catch (RemoteException re) {
                throw re.rethrowAsRuntimeException();
            }
            if (keyEvictedCallbacks == null) {'''
    new = '''                mInjector.getStorageManager().lockCeStorage(userId);
                if (mInjector.getStorageManager().isCeStorageUnlocked(userId)) {
                    Slogf.e(TAG, "CE key eviction not confirmed for user %d", userId);
                    return;
                }
            } catch (RemoteException | RuntimeException failure) {
                // Runtime quiescence or vold failed: preserve the failed state,
                // do not crash FgThread or report successful key eviction.
                Slogf.e(TAG, "CE key eviction failed for user %d", userId);
                return;
            }
            if (keyEvictedCallbacks == null) {'''
    return replace_once(text, old, new).encode('utf-8')


def patch_resilient(data):
    text = data.decode('utf-8')
    anchor = '    public void failWrite(FileOutputStream str) {\n'
    addition = '''    /** Checked commit for removal; the original UserData must remain reserved on error. */
    public void finishWriteChecked(FileOutputStream str) throws IOException {
        if (mMainOutStream != str || str == null) {
            throw new IllegalStateException("Invalid incoming stream.");
        }
        try {
            com.android.server.aegis.AegisRemovalFiles.commit(mFile, mTemporaryBackup,
                    mReserveCopy, mMainOutStream, mMainInStream, mReserveOutStream, mFileMode);
            // fs-verity rejects enabling a file with a live writable descriptor.
            // Retain only the two read descriptors, as AOSP's ordinary commit does.
            mMainOutStream.close();
            mMainOutStream = null;
            mReserveOutStream.close();
            mReserveOutStream = null;
            // Keep AOSP's best-effort fs-verity protection for both committed copies.
            try (ParcelFileDescriptor mainPfd = ParcelFileDescriptor.dup(mMainInStream.getFD());
                 ParcelFileDescriptor copyPfd = ParcelFileDescriptor.dup(mReserveInStream.getFD())) {
                FileIntegrity.setUpFsVerity(mainPfd);
                FileIntegrity.setUpFsVerity(copyPfd);
            } catch (IOException e) {
                Slog.e(LOG_TAG, "Failed to verity-protect " + mDebugName, e);
            }
        } finally {
            close();
        }
    }

    /** No ignored delete booleans and no fallback file left behind on success. */
    public void deleteChecked() throws IOException {
        if (mMainOutStream != null || mMainInStream != null || mReserveOutStream != null
                || mReserveInStream != null || mCurrentInStream != null) {
            throw new IllegalStateException("Metadata file is still in use");
        }
        com.android.server.aegis.AegisRemovalFiles.delete(mFile, mTemporaryBackup, mReserveCopy);
    }

'''
    return replace_once(text, anchor, addition + anchor).encode('utf-8')


def method(text, signature):
    if text.count(signature) != 1:
        raise ValueError('Pinned AOSP removal method is missing or ambiguous')
    start = text.index(signature)
    end = text.index('\n    }\n', start) + len('\n    }')
    return text[start:end]


def patch_installer(data):
    text = data.decode('utf-8')
    original = method(text, '    public void destroyUserData(String uuid, int userId, int flags)')
    checked = replace_once(original, 'void destroyUserData(', 'void destroyUserDataChecked(')
    checked = replace_once(checked, '        if (!checkBeforeRemote()) return;', '''        if (!checkBeforeRemote()) {
            throw new InstallerException("User removal requires an actual installd acknowledgement");
        }''')
    return replace_once(text, original, original + '\n\n' + checked).encode('utf-8')


def patch_preparer(data):
    text = data.decode('utf-8')
    anchor = '    void destroyUserData(int userId, int flags) {\n'
    addition = '''    /** Reserved-user finalization: every destructive operation must acknowledge success. */
    void destroyUserDataChecked(int userId, int flags) throws Exception {
        try (PackageManagerTracedLock installLock = mInstallLock.acquireLock()) {
            final StorageManager storage = com.android.server.aegis.AegisRemovalData
                    .requireInternalStorageOnly(mContext.getSystemService(StorageManager.class));
            mInstaller.destroyUserDataChecked(null, userId, flags);
            if ((flags & StorageManager.FLAG_STORAGE_DE) != 0) {
                com.android.server.aegis.AegisRemovalData.deleteSystemDirectory(
                        getUserSystemDirectory(userId), true);
                com.android.server.aegis.AegisRemovalData.deleteSystemDirectory(
                        getDataSystemDeDirectory(userId), false);
            }
            if ((flags & StorageManager.FLAG_STORAGE_CE) != 0) {
                com.android.server.aegis.AegisRemovalData.deleteSystemDirectory(
                        getDataSystemCeDirectory(userId), false);
            }
            // The patched StorageManagerService propagates vold failure under its own lease.
            storage.destroyUserStorage(null, userId, flags);
            com.android.server.aegis.AegisRemovalData.requireInternalStorageOnly(storage);
        }
    }

'''
    return replace_once(text, anchor, addition + anchor).encode('utf-8')


def patch_protector_crypto(data):
    text = data.decode('utf-8')
    old = method(text, '    public static void destroyProtectorKey(String keyAlias)')
    new = replace_once(old, 'void destroyProtectorKey(', 'void destroyProtectorKeyChecked(')
    new = replace_once(new, '            keyStore.deleteEntry(keyAlias);',
                       '            keyStore.deleteEntry(keyAlias);\n'
                       '            if (keyStore.containsAlias(keyAlias)) {\n'
                       '                throw new IllegalStateException("SP protector key remains");\n'
                       '            }')
    new = replace_once(new, '            Slog.e(TAG, "Failed to delete SP protector key " + keyAlias, e);',
                       '            throw new IllegalStateException("SP protector key deletion failed", e);')
    return replace_once(text, old, old + '\n\n' + new).encode('utf-8')


def patch_synthetic(data):
    text = data.decode('utf-8')
    anchor = '    public void removeUser(IGateKeeperService gatekeeper, int userId) {\n'
    addition = '''    /** Synchronous checked path for a still-reserved AOSP user; original path is unchanged. */
    public void removeUserChecked(IGateKeeperService gatekeeper, int userId) {
        final java.util.List<String> files;
        try {
            files = com.android.server.aegis.AegisRemovalData.listSystemFiles(
                    mStorage.getSyntheticPasswordDirectoryForUser(userId));
        } catch (java.io.IOException failure) {
            throw new IllegalStateException("Credential-state inventory unconfirmed", failure);
        }
        // Phase 1's QEMU uses GateKeeper. Never claim Weaver erasure from the
        // original best-effort method (which can return without erasing a slot).
        // Refuse BEFORE deleting any protector if this user has Weaver state.
        for (String file : files) {
            if (file.endsWith("." + WEAVER_SLOT_NAME)) {
                throw new IllegalStateException("Checked Weaver user removal is not implemented");
            }
        }
        final java.util.List<Long> protectors = new java.util.ArrayList<>();
        for (String file : files) {
            if (file.endsWith("." + SP_BLOB_NAME)) {
                String id = file.substring(0, file.length() - SP_BLOB_NAME.length() - 1);
                if (!id.matches("[0-9a-f]{16}")) {
                    throw new IllegalStateException("Malformed AOSP protector-state name");
                }
                protectors.add(Long.parseUnsignedLong(id, 16));
            }
        }
        for (long protectorId : protectors) {
            SyntheticPasswordCrypto.destroyProtectorKeyChecked(getProtectorKeyAlias(protectorId));
        }
        try {
            java.util.Objects.requireNonNull(gatekeeper, "AOSP GateKeeper unavailable")
                    .clearSecureUserId(fakeUserId(userId));
        } catch (RemoteException failure) {
            throw new IllegalStateException("GateKeeper synthetic SID removal failed", failure);
        }
        // Keep the state files until LockSettings commits removal; on failure the
        // original inventory must survive so a later boot can retry it.
    }

'''
    return replace_once(text, anchor, addition + anchor).encode('utf-8')


def patch_lock_settings(data):
    text = data.decode('utf-8')
    # LockPatternUtils -> LockSettingsInternal remains the AOSP authority.
    # Do not change early-boot/reused-user internal best-effort call sites.
    text = replace_once(text, '            LockSettingsService.this.removeUser(userId);',
                        '            LockSettingsService.this.removeUserChecked(userId);')
    registration = '        LocalServices.addService(LockSettingsInternal.class, new LocalService());'
    text = replace_once(text, registration, registration + '''
        LocalServices.addService(com.android.server.aegis.AegisPackageCredentials.class,
                new com.android.server.aegis.AegisPackageCredentials(
                    new com.android.server.aegis.AegisPackageCredentials.Directory() {
                        @Override public UserInfo user(int id) { return mUserManager.getUserInfo(id); }
                        @Override public boolean restricted(int id, String restriction) {
                            return mUserManager.hasUserRestriction(restriction, UserHandle.of(id));
                        }
                    }, (credential, id) -> {
                        try { return verifyAegisPackageCredential(credential, id); }
                        finally { scheduleGc(); }
                    }));''')
    credential_anchor = '    private void removeUser(@UserIdInt int userId) {\n'
    text = replace_once(text, credential_anchor, '''    /**
     * Fresh package confirmation through the existing AOSP password protector.
     * Intentionally does NOT call doVerifyCredential/onCredentialVerified:
     * those unlock Keystore, CE and the Android user, activate escrow, reset
     * biometric lockout and notify success listeners. No GK handle is requested.
     * This local service returns only sanitized status, never SP/HAT material.
     */
    private VerifyCredentialResponse verifyAegisPackageCredential(
            LockscreenCredential credential, int userId) {
        if (!mThirdPartyAppsStarted) {
            throw new IllegalStateException("LockSettings password verification is not ready");
        }
        final VerifyCredentialResponse response;
        synchronized (mSpManager) {
            if (getCredentialType(userId) != LockPatternUtils.CREDENTIAL_TYPE_PASSWORD) {
                throw new SecurityException("Personal administrator password required");
            }
            long protectorId = getCurrentLskfBasedProtectorId(userId);
            AuthenticationResult result = mSpManager.unlockLskfBasedProtector(
                    getGateKeeperService(), protectorId, credential, userId, null);
            response = java.util.Objects.requireNonNull(result.gkResponse);
            if (response.getResponseCode() == VerifyCredentialResponse.RESPONSE_OK
                    && result.syntheticPassword == null) {
                throw new IllegalStateException("AOSP protector was not verified");
            }
        }
        if (response.getResponseCode() == VerifyCredentialResponse.RESPONSE_RETRY
                && response.getTimeout() > 0) {
            requireStrongAuth(STRONG_AUTH_REQUIRED_AFTER_LOCKOUT, userId);
        }
        // Preserve normal AOSP failure notifications and its HAL retry delay.
        // Success cannot be used as a login/unlock notification by other services.
        if (response.getResponseCode() != VerifyCredentialResponse.RESPONSE_OK) {
            notifyLockSettingsStateListeners(false, userId);
        }
        if (response.getResponseCode() == VerifyCredentialResponse.RESPONSE_OK) {
            return VerifyCredentialResponse.OK;
        }
        if (response.getResponseCode() == VerifyCredentialResponse.RESPONSE_RETRY) {
            return VerifyCredentialResponse.fromTimeout(response.getTimeout());
        }
        return VerifyCredentialResponse.ERROR;
    }

''' + credential_anchor)
    anchor = '    private void removeUser(@UserIdInt int userId) {\n'
    addition = '''    private void removeUserChecked(@UserIdInt int userId) {
        synchronized (mUserCreationAndRemovalLock) {
            if (!mThirdPartyAppsStarted) {
                throw new IllegalStateException("LockSettings removal is not ready");
            }
            removeBiometricsChecked(userId);
            mSpManager.removeUserChecked(getGateKeeperService(), userId);
            mStrongAuth.removeUser(userId);
            int result = AndroidKeyStoreMaintenance.onUserRemoved(userId);
            if (result != 0) {
                throw new IllegalStateException("AOSP Keystore user removal failed: " + result);
            }
            mUnifiedProfilePasswordCache.removePassword(userId);
            try {
                java.util.Objects.requireNonNull(getGateKeeperService(), "AOSP GateKeeper unavailable")
                        .clearSecureUserId(userId);
            } catch (RemoteException failure) {
                throw new IllegalStateException("AOSP GateKeeper SID removal failed", failure);
            }
            try {
                final String encryptAlias = PROFILE_KEY_NAME_ENCRYPT + userId;
                final String decryptAlias = PROFILE_KEY_NAME_DECRYPT + userId;
                mKeyStore.deleteEntry(encryptAlias);
                mKeyStore.deleteEntry(decryptAlias);
                if (mKeyStore.containsAlias(encryptAlias) || mKeyStore.containsAlias(decryptAlias)) {
                    throw new IllegalStateException("AOSP profile key remains");
                }
            } catch (KeyStoreException failure) {
                throw new IllegalStateException("AOSP profile-key removal failed", failure);
            }
            // Only now may the credential inventory and serial record disappear.
            mStorage.removeUser(userId);
        }
    }

    private static void awaitRemovalAcknowledgement(
            java.util.concurrent.CompletableFuture<Void> done) {
        try {
            done.get(10, TimeUnit.SECONDS);
        } catch (InterruptedException failure) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Biometric removal interrupted", failure);
        } catch (java.util.concurrent.ExecutionException | java.util.concurrent.TimeoutException failure) {
            throw new IllegalStateException("Biometric removal unconfirmed", failure);
        }
    }

    private void removeBiometricsChecked(int userId) {
        android.content.pm.PackageManager pm = mContext.getPackageManager();
        if (pm.hasSystemFeature(android.content.pm.PackageManager.FEATURE_FINGERPRINT)) {
            FingerprintManager manager = java.util.Objects.requireNonNull(
                    mInjector.getFingerprintManager(), "Fingerprint service unavailable");
            java.util.concurrent.CompletableFuture<Void> done = new java.util.concurrent.CompletableFuture<>();
            manager.removeAll(userId, new FingerprintManager.RemovalCallback() {
                @Override public void onRemovalError(Fingerprint fp, int error, CharSequence message) {
                    done.completeExceptionally(new IllegalStateException("Fingerprint removal failed: " + error));
                }
                @Override public void onRemovalSucceeded(Fingerprint fp, int remaining) {
                    if (remaining == 0) done.complete(null);
                }
            });
            awaitRemovalAcknowledgement(done);
        }
        if (pm.hasSystemFeature(android.content.pm.PackageManager.FEATURE_FACE)) {
            FaceManager manager = java.util.Objects.requireNonNull(
                    mInjector.getFaceManager(), "Face service unavailable");
            java.util.concurrent.CompletableFuture<Void> done = new java.util.concurrent.CompletableFuture<>();
            manager.removeAll(userId, new FaceManager.RemovalCallback() {
                @Override public void onRemovalError(Face face, int error, CharSequence message) {
                    done.completeExceptionally(new IllegalStateException("Face removal failed: " + error));
                }
                @Override public void onRemovalSucceeded(Face face, int remaining) {
                    if (remaining == 0) done.complete(null);
                }
            });
            awaitRemovalAcknowledgement(done);
        }
    }

'''
    return replace_once(text, anchor, addition + anchor).encode('utf-8')


def patch_biometric_removal(data):
    text = data.decode('utf-8')
    anchor = '    public void onRemoved(@NonNull BiometricAuthenticator.Identifier identifier, int remaining) {\n'
    # Keep generic null/error handling intact. Only the two AIDL clients call
    # this method, after an actual empty HAL completion for their empty request.
    addition = '''    /** An actual AIDL completion, not a missing callback or cached no-enrollment hint. */
    protected final void acknowledgeEmptyRemoval(int requestedCount) {
        if (requestedCount != 0
                || !mBiometricUtils.getBiometricsForUser(getContext(), getTargetUserId()).isEmpty()) {
            onRemoved(null, 0); // Preserve the original failure path for inconsistent completion.
            return;
        }
        if (getListener() == null) {
            mCallback.onClientFinished(this, false);
            return;
        }
        mAuthenticatorIds.put(getTargetUserId(), 0L);
        try {
            getListener().onRemoved(null, 0);
        } catch (RemoteException failure) {
            Slog.w(TAG, "Empty removal acknowledgement delivery failed", failure);
            mCallback.onClientFinished(this, false);
            return;
        }
        mCallback.onClientFinished(this, true);
    }

'''
    # The annotation belongs to the original public method, not the new helper.
    return replace_once(text, '    @Override\n' + anchor,
                        addition + '    @Override\n' + anchor).encode('utf-8')


def patch_aidl_removal(data):
    text = data.decode('utf-8')
    anchor = '    @Override\n    protected void startHalOperation() {\n'
    addition = '''    /** Called only by this sensor's actual empty AIDL HAL completion. */
    public void onEmptyRemovalResponse() {
        acknowledgeEmptyRemoval(mBiometricIds == null ? -1 : mBiometricIds.length);
    }

'''
    return replace_once(text, anchor, addition + anchor).encode('utf-8')


def patch_aidl_response(data, client):
    text = data.decode('utf-8')
    original = method(text, '    public void onEnrollmentsRemoved(int[] enrollmentIds)')
    old = '''            handleResponse(RemovalConsumer.class, (c) -> c.onRemoved(null /* identifier */,
                    0 /* remaining */));'''
    new = '''            handleResponse(RemovalConsumer.class, (c) -> {
                if (c instanceof CLIENT) {
                    ((CLIENT) c).onEmptyRemovalResponse();
                } else {
                    // Other consumers retain the original null/error semantics.
                    c.onRemoved(null /* identifier */, 0 /* remaining */);
                }
            });'''.replace('CLIENT', client)
    return replace_once(text, original, replace_once(original, old, new)).encode('utf-8')


def patch_manager(data):
    text = data.decode('utf-8')
    # Framework handlers such as StrongAuth still queue work by numeric ID.
    # Keep AOSP's own removal set for this system-server lifetime, including
    # allocator exhaustion. A new system server has no old Java handler queue;
    # persisted partial users are independently recovered before their reuse.
    allocator = method(text, '    int getNextAvailableId()')
    text = replace_once(text, allocator, '''    int getNextAvailableId() {
        synchronized (mUsersLock) {
            final int nextId = scanNextAvailableIdLocked();
            if (nextId >= 0) return nextId;
            throw new IllegalStateException(
                    "No user id available without reusing a removed identity");
        }
    }''')
    text = replace_once(text, '        @NonNull UserInfo info;\n', '''        @NonNull UserInfo info;
        // Claims belong to this exact AOSP object, never to a reusable numeric ID.
        // Guarded by the enclosing service's mUsersLock; failures retain both claims.
        boolean aegisRemovalNotified;
        boolean aegisRemovalCleanupClaimed;
''')
    text = replace_once(text, '    private @SystemService.BootPhase int mCurrentBootPhase;',
                        '    private volatile @SystemService.BootPhase int mCurrentBootPhase;')
    # Keep early marking, so partial/pre-created users cannot be started. Actual cleanup
    # waits until LockSettings no longer queues numeric-ID removal for later in boot.
    text = replace_once(text, '                mUms.registerStatsCallbacks();\n            }',
                        '''                mUms.registerStatsCallbacks();
            } else if (phase == SystemService.PHASE_BOOT_COMPLETED) {
                // Biometric acknowledgements are delivered through framework handlers.
                // Never wait for them on the boot/main thread itself.
                new Thread(() -> mUms.cleanupPartialUsers(), "aegis-user-recovery").start();
            }''')
    for signature, collection, kind in (
            ('    private void cleanupPartialUsers()', 'partials', 'partial'),
            ('    private void cleanupPreCreatedUsers()', 'preCreatedUsers', 'precreated')):
        old = method(text, signature)
        new = old.replace('ArrayList<UserInfo>', 'ArrayList<UserData>')
        new = replace_once(new, '                UserInfo ui = mUsers.valueAt(i).info;',
                           '                UserData original = mUsers.valueAt(i);\n'
                           '                UserInfo ui = original.info;')
        new = replace_once(new, collection + '.add(ui);', collection + '.add(original);')
        new = replace_once(new, f'            UserInfo ui = {collection}.get(i);',
                           f'            UserData original = {collection}.get(i);\n'
                           '            UserInfo ui = original.info;')
        new = replace_once(new, '            removeUserState(ui.id);',
                           '            removeUserState(original, ui.serialNumber, false);')
        text = replace_once(text, old, new)

    # A checked writer serializes the same AOSP format without swallowing errors.
    anchor = '    private void writeUserLP(UserData userData) {\n'
    checked_writer = '''    private void writeUserLPChecked(UserData userData) throws Exception {
        UserManager.invalidateCacheOnUserDataChanged();
        try (ResilientAtomicFile file = getUserFile(userData.info.id)) {
            FileOutputStream stream = file.startWrite();
            writeUserLP(userData, stream);
            file.finishWriteChecked(stream);
        }
    }

    @GuardedBy({"mPackagesLock"})
'''
    text = replace_once(text, anchor, checked_writer + anchor)
    old = method(text, '    private void writeUserListLP()')
    new = replace_once(old, '    private void writeUserListLP() {', '''    private void writeUserListLP(boolean checked, int excludedUserId) {
        // During finalization the removed user's original UserData remains in RAM.
        // Exclude it from persistent copies without releasing its ID reservation.''')
    new = replace_once(new, '                for (int id : userIdsToWrite) {',
                       '                for (int id : userIdsToWrite) {\n'
                       '                    if (id == excludedUserId) continue;')
    new = replace_once(new, '                file.finishWrite(fos);',
                       '                if (checked) file.finishWriteChecked(fos);\n'
                       '                else file.finishWrite(fos);')
    new = replace_once(new, '                file.failWrite(fos);',
                       '                if (checked) throw new IllegalStateException(\n'
                       '                        "AOSP user-list commit unconfirmed", e);\n'
                       '                file.failWrite(fos);')
    wrapper = '''    private void writeUserListLP() {
        writeUserListLP(false, UserHandle.USER_NULL);
    }

    @GuardedBy({"mPackagesLock"})
'''
    text = replace_once(text, old, wrapper + new)

    old = method(text, '    private boolean removeUserUnchecked(@UserIdInt int userId)')
    new = replace_once(old, '            final UserData userData;',
                       '            final UserData userData;\n            final int userSerial;\n'
                       '            if (mCurrentBootPhase < SystemService.PHASE_BOOT_COMPLETED) {\n'
                       '                return false;\n            }\n'
                       '            com.android.server.aegis.AegisRemovalData.requireInternalStorageOnly(\n'
                       '                    mContext.getSystemService(StorageManager.class));')
    new = replace_once(new, '                    userData = mUsers.get(userId);',
                       '                    userData = mUsers.get(userId);\n'
                       '                    userSerial = userData.info.serialNumber;')
    new = replace_once(new, '                writeUserLP(userData);', '''                try {
                    writeUserLPChecked(userData);
                } catch (Exception failure) {
                    Slog.e(LOG_TAG, "User removal marker was not committed; ID remains reserved",
                            failure);
                    return false;
                }''')
    new = replace_once(new, '                                finishRemoveUser(userIdParam);',
                       '                                if (userIdParam != userId\n'
                       '                                        || !finishRemoveUser(userData, userSerial)) return;')
    text = replace_once(text, old, new)

    old = method(text, '    private void finishRemoveUser(final @UserIdInt int userId)')
    new = replace_once(old, '    private void finishRemoveUser(final @UserIdInt int userId) {',
                       '    private boolean finishRemoveUser(final UserData original, final int serial) {\n'
                       '        final int userId = original.info.id;')
    new = replace_once(new, '            user = getUserInfoLU(userId);',
                       '            if (!isOriginalRemovingUserLU(original, serial)\n'
                       '                    || original.aegisRemovalNotified) return false;\n'
                       '            original.aegisRemovalNotified = true;\n'
                       '            user = original.info;')
    new = replace_once(new, '''            LocalServices.getService(ActivityTaskManagerInternal.class).onUserStopped(userId);
            removeUserState(userId);
            return;''', '''            new Thread(() -> removeUserState(original, serial, true),
                    "aegis-precreated-removal").start();
            return true;''')
    new = replace_once(new, '''                                getActivityManagerInternal().onUserRemoving(userId);
                                removeUserState(userId);''',
                       '                                removeUserState(original, serial, true);')
    new = new[:-len('\n    }')] + '\n        return true;\n    }'
    text = replace_once(text, old, new)

    old = method(text, '    private void removeUserState(final @UserIdInt int userId)')
    replacement = '''    @GuardedBy("mUsersLock")
    private boolean isOriginalRemovingUserLU(UserData original, int serial) {
        return original != null && original.info.id != UserHandle.USER_SYSTEM
                && mUsers.get(original.info.id) == original
                && original.info.serialNumber == serial && original.info.partial
                && mRemovingUserIds.get(original.info.id);
    }

    private void removeUserState(final UserData original, final int serial,
            boolean notifyActivityManager) {
        // Early LockSettings removal is deferred by numeric ID. Do not release an ID
        // before that service is ready; boot completion retries original partial users.
        if (mCurrentBootPhase < SystemService.PHASE_BOOT_COMPLETED) return;
        final int userId = original.info.id;
        synchronized (mUsersLock) {
            if (!isOriginalRemovingUserLU(original, serial)
                    || original.aegisRemovalCleanupClaimed) return;
            original.aegisRemovalCleanupClaimed = true;
        }
        try {
            if (ActivityManager.getService().isUserRunning(userId, 0)) {
                throw new IllegalStateException("AOSP user is still running");
            }
            final StorageManager storage = com.android.server.aegis.AegisRemovalData
                    .requireInternalStorageOnly(mContext.getSystemService(StorageManager.class));
            synchronized (mPackagesLock) {
                synchronized (mUsersLock) {
                    if (!isOriginalRemovingUserLU(original, serial)) return;
                }
                writeUserLPChecked(original);
            }
            // Close before AOSP calls: storage-key/data hooks acquire their own lease.
            // Closing never reopens runtime admission for this user.
            try (com.android.server.aegis.AegisRuntimeStorage.Lease lease =
                    com.android.server.aegis.AegisRuntimeStorage.begin(userId,
                            com.android.server.aegis.AegisRuntimeStorage.Operation.DESTROY)) {
                // Acquisition confirms native runtime quiescence for all serials of this ID.
            }
            if (notifyActivityManager) {
                if (original.info.preCreated) {
                    LocalServices.getService(ActivityTaskManagerInternal.class).onUserStopped(userId);
                } else {
                    getActivityManagerInternal().onUserRemoving(userId);
                }
            }
            // LockSettings needs DE data. No swallowed error may reach ID release.
            mLockPatternUtils.removeUser(userId);
            storage.destroyUserStorageKeys(userId);
            if (storage.isCeStorageUnlocked(userId)) {
                throw new IllegalStateException("Removed user's CE storage is still unlocked");
            }
            mPm.cleanUpUser(this, userId);
            mUserDataPreparer.destroyUserDataChecked(userId,
                    StorageManager.FLAG_STORAGE_DE | StorageManager.FLAG_STORAGE_CE);
            com.android.server.aegis.AegisRemovalData.requireInternalStorageOnly(storage);

            // Perform numeric-ID cleanup while this exact AOSP object still reserves the ID.
            getActivityManagerInternal().onUserRemoved(userId);
            synchronized (mUserStates) {
                mUserStates.delete(userId);
            }
            synchronized (mRestrictionsLock) {
                mBaseUserRestrictions.remove(userId);
                mAppliedUserRestrictions.remove(userId);
                mCachedEffectiveUserRestrictions.remove(userId);
                if (mDevicePolicyUserRestrictions.remove(userId)) {
                    applyUserRestrictionsForAllUsersLR();
                }
            }
            // Creation and deferred XML rewrites also take mPackagesLock. Keep it
            // through all disk work and final in-memory removal; do not hold mUsersLock
            // while the list serializer acquires mGuestRestrictions.
            synchronized (mPackagesLock) {
                synchronized (mUsersLock) {
                    if (!isOriginalRemovingUserLU(original, serial)) {
                        throw new IllegalStateException("AOSP removal reservation changed");
                    }
                }
                try (ResilientAtomicFile file = getUserFile(userId)) {
                    file.deleteChecked();
                }
                writeUserListLP(true, userId);
                synchronized (mUsersLock) {
                    removeUserDataLU(userId);
                    mIsUserManaged.delete(userId);
                    updateUserIds();
                    // Keep the numeric ID retired for this system-server lifetime.
                    // Queued legacy handler work must never address a new identity.
                }
            }
            // Persistent removal is complete; the numeric ID remains retired in RAM.
            Slog.i(LOG_TAG, "AOSP user removal committed for " + userId + "/" + serial);
        } catch (Exception failure) {
            // Retain partial UserData and its claim even if cleanup partially succeeded.
            // Only reboot recovery may retry; delayed duplicate callbacks are inert.
            Slog.e(LOG_TAG, "AOSP removal incomplete; user ID remains reserved "
                    + userId + "/" + serial, failure);
        }
    }'''
    return replace_once(text, old, replacement).encode('utf-8')


def checked_path(root, relative):
    current = root
    if root.is_symlink() or not root.is_dir():
        raise ValueError('Expected an ordinary source root')
    for part in Path(relative).parts:
        if part in ('..', '.') or part == '/' or not part:
            raise ValueError('Invalid integration path')
        current = current / part
        if current.is_symlink():
            raise ValueError('Refusing a symlink in an integration path')
        if current.exists() and current != root / relative and not current.is_dir():
            raise ValueError('Integration ancestor is not a directory')
    if current.exists() and not current.is_file():
        raise ValueError('Integration destination is not a regular file')
    return current


def write_atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix='.aegis-storage-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as file:
            file.write(data)
            file.flush()
            os.fsync(file.fileno())
        os.chmod(temporary, 0o644)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)  # Only our newly allocated temporary file.


def encoded(value):
    return (json.dumps(value, sort_keys=True, indent=2) + '\n').encode()


def originals_from_git(base):
    result = {}
    for name in sorted(ORIGINAL_FILES):
        result[name] = subprocess.run(['git', '-C', str(base), 'show', 'HEAD:' + name],
                                      check=True, capture_output=True).stdout
    return result


def validate_record(record):
    schema = record.get('schema') if isinstance(record, dict) else None
    if type(schema) is not int or schema not in (1, 2, 3, 4, 5, 6):
        raise ValueError('Invalid storage-source receipt schema')
    expected_inputs = {1: {STORAGE, USERS}, 2: {STORAGE, USERS, RESILIENT},
                       3: SCHEMA_THREE_ORIGINALS, 4: SCHEMA_FOUR_ORIGINALS}.get(schema, ORIGINAL_FILES)
    expected_outputs = {1: {STORAGE, USERS, BRIDGE},
                        2: {STORAGE, USERS, RESILIENT, BRIDGE, REMOVAL_FILES},
                        3: SCHEMA_THREE_ORIGINALS | LEGACY_SOURCE_FILES.keys(),
                        4: SCHEMA_FOUR_ORIGINALS | LEGACY_SOURCE_FILES.keys(),
                        5: ORIGINAL_FILES | LEGACY_SOURCE_FILES.keys()}.get(
                                schema, ORIGINAL_FILES | SOURCE_FILES.keys())
    if (not isinstance(record, dict) or schema not in (1, 2, 3, 4, 5, 6)
            or record.get('status') != 'FRAMEWORK_SOURCES_PREPARED_NOT_TESTED'
            or record.get('aosp_tag') != 'android-16.0.0_r1'
            or not isinstance(record.get('inputs'), dict)
            or set(record['inputs']) != expected_inputs
            or not isinstance(record.get('outputs'), dict)
            or set(record['outputs']) != expected_outputs
            or any(not isinstance(value, str) or not re.fullmatch('[0-9a-f]{64}', value)
                   for value in [*record['inputs'].values(), *record['outputs'].values()])
            or record.get('bridge_sha256') != record['outputs'][BRIDGE]):
        raise ValueError('Invalid storage-source receipt')


def prepare(project, aosp, originals=None, pins=None):
    project, aosp = Path(project).absolute(), Path(aosp).absolute()
    # Optional in-process inputs are only for inert integration fixtures. CLI
    # always reads the committed AOSP Git blobs and repository pin, no override.
    base = aosp / 'frameworks/base'
    for name in ORIGINAL_FILES:
        checked_path(aosp, 'frameworks/base/' + name)
    if originals is None:
        originals = originals_from_git(base)
    if pins is None:
        pins = json.loads(checked_path(project, 'runtime/aosp-storage-hooks.json').read_bytes())
    if (pins.get('schema') != 1 or pins.get('aosp_tag') != 'android-16.0.0_r1'
            or set(pins.get('files', {})) != ORIGINAL_FILES
            or set(originals) != ORIGINAL_FILES
            or any(digest(originals[name]) != pins['files'][name] for name in originals)):
        raise ValueError('AOSP Git source does not match the exact pinned storage baseline')
    sources = {name: checked_path(project, source).read_bytes()
               for name, source in SOURCE_FILES.items()}
    outputs = {STORAGE: patch_storage(originals[STORAGE]),
               USERS: patch_users(originals[USERS]),
               RESILIENT: patch_resilient(originals[RESILIENT]),
               MANAGER: patch_manager(originals[MANAGER]),
               PREPARER: patch_preparer(originals[PREPARER]),
               INSTALLER: patch_installer(originals[INSTALLER]),
               LOCK_SETTINGS: patch_lock_settings(originals[LOCK_SETTINGS]),
               SYNTHETIC: patch_synthetic(originals[SYNTHETIC]),
               PROTECTOR_CRYPTO: patch_protector_crypto(originals[PROTECTOR_CRYPTO]),
               BIOMETRIC_REMOVAL: patch_biometric_removal(originals[BIOMETRIC_REMOVAL]),
               FINGERPRINT_REMOVAL: patch_aidl_removal(originals[FINGERPRINT_REMOVAL]),
               FACE_REMOVAL: patch_aidl_removal(originals[FACE_REMOVAL]),
               FINGERPRINT_RESPONSE: patch_aidl_response(originals[FINGERPRINT_RESPONSE], 'FingerprintRemovalClient'),
               FACE_RESPONSE: patch_aidl_response(originals[FACE_RESPONSE], 'FaceRemovalClient'), **sources}
    record = {'schema': 6, 'status': 'FRAMEWORK_SOURCES_PREPARED_NOT_TESTED',
              'aosp_tag': pins['aosp_tag'], 'inputs': pins['files'],
              'bridge_sha256': digest(sources[BRIDGE]),
              'outputs': {name: digest(data) for name, data in outputs.items()}}
    marker = checked_path(aosp, MARKER)
    previous = None
    if marker.exists():
        previous = json.loads(marker.read_bytes())
        validate_record(previous)
        if any(pins['files'].get(name) != value for name, value in previous['inputs'].items()):
            raise ValueError('Invalid storage-source ownership record')
    changes = {}
    before = {}
    # Validate EVERY destination before writing any of them.
    for name, data in outputs.items():
        target = checked_path(aosp, 'frameworks/base/' + name)
        current = target.read_bytes() if target.exists() else None
        if current == data:
            continue  # Also recognizes an interrupted installation of these exact bytes.
        allowed = current is None if name in SOURCE_FILES else current == originals[name]
        if previous and current is not None and name in previous['outputs']:
            allowed |= digest(current) == previous['outputs'][name]
        if not allowed:
            raise ValueError('Unmanaged or modified AOSP storage source; preserving local work')
        changes[name] = data
        before[name] = current
    if changes:
        # Keep backups outside AOSP Java-source discovery. A partially completed
        # installation is never a license to overwrite unrelated changes.
        backup_parent = checked_path(aosp, 'out/aegis-runtime-storage/backups/.anchor').parent
        backup_parent.mkdir(parents=True, exist_ok=True)
        backup = Path(tempfile.mkdtemp(prefix='install-', dir=backup_parent))
        for name, data in before.items():
            if data is not None:
                target = backup / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
        (backup / 'intent.json').write_bytes(encoded(record))
        for name, data in changes.items():
            write_atomic(checked_path(aosp, 'frameworks/base/' + name), data)
    verify(aosp, record)
    write_atomic(marker, encoded(record))
    return record


def verify(aosp, record):
    aosp = Path(aosp).absolute()
    validate_record(record)
    for name, expected in record['outputs'].items():
        if digest(checked_path(aosp, 'frameworks/base/' + name).read_bytes()) != expected:
            raise ValueError('AOSP storage sources changed after preparation')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path)
    parser.add_argument('--aosp', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    if args.verify:
        verify(args.aosp, json.loads(args.receipt.read_bytes()))
    else:
        if args.project is None:
            parser.error('--project is required for source preparation')
        if args.receipt.exists() or args.receipt.is_symlink():
            raise ValueError('Refusing to overwrite an existing build receipt')
        result = prepare(args.project, args.aosp)
        write_atomic(args.receipt, encoded(result))
