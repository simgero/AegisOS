package com.android.server.aegis;

import android.content.pm.UserInfo;
import android.os.Binder;
import android.os.Looper;
import android.os.Process;
import android.os.UserManager;

import com.android.internal.widget.LockscreenCredential;
import com.android.internal.widget.VerifyCredentialResponse;

import java.util.Objects;

/**
 * Internal system-server credential check for forthcoming package approval.
 * No Binder endpoint, cached authorization, personal registry or returned key.
 * Success is ONLY fresh password evidence for the named AOSP administrator.
 * The package coordinator must bind the action, requesting session and target,
 * and recheck them before its bounded, cancellable worker handoff.
 */
public final class AegisPackageCredentials {
    public enum Action { INSTALL, UPDATE, REMOVE }

    public interface Directory {
        UserInfo user(int id);
        boolean restricted(int id, String restriction);
    }

    public interface Verifier {
        /** Use AOSP's existing SP/Gatekeeper/Weaver check, WITHOUT user/CE unlock. */
        VerifyCredentialResponse verify(LockscreenCredential credential, int userId);
    }

    private final Directory directory;
    private final Verifier verifier;

    /** Constructed and registered in LocalServices by LockSettingsService itself. */
    public AegisPackageCredentials(Directory directory, Verifier verifier) {
        this.directory = Objects.requireNonNull(directory);
        this.verifier = Objects.requireNonNull(verifier);
    }

    /** Consumes the credential on every path. No SP, HAT or password handle leaves AOSP. */
    public VerifyCredentialResponse verify(int userId, int serial, Action action,
            LockscreenCredential credential) {
        try (LockscreenCredential owned = Objects.requireNonNull(credential)) {
            if (Process.myUid() != Process.SYSTEM_UID) {
                throw new SecurityException("Package credential check requires system_server");
            }
            if (Looper.getMainLooper().isCurrentThread()) {
                throw new IllegalStateException("Credential check requires a worker thread");
            }
            long calling = Binder.clearCallingIdentity();
            try {
                return verifyOwned(userId, serial, action, owned);
            } finally {
                Binder.restoreCallingIdentity(calling);
            }
        }
    }

    // Package-private entry for isolated device fixtures. It neither publishes
    // this object in LocalServices nor bypasses the public entry's process check.
    VerifyCredentialResponse verifyOwned(int userId, int serial, Action action,
            LockscreenCredential owned) {
        Objects.requireNonNull(action);
        if (!owned.isPassword()) throw new IllegalArgumentException("Password required");
        owned.validateBasicRequirements();
        requireAdministrator(userId, serial, action);
        VerifyCredentialResponse result = Objects.requireNonNull(verifier.verify(owned, userId));
        // A removal, role/restriction change, disable or reused id during the
        // expensive check must not turn an old credential into current authority.
        requireAdministrator(userId, serial, action);
        if (result.getResponseCode() == VerifyCredentialResponse.RESPONSE_OK) {
            return VerifyCredentialResponse.OK;
        }
        if (result.getResponseCode() == VerifyCredentialResponse.RESPONSE_RETRY) {
            return VerifyCredentialResponse.fromTimeout(result.getTimeout());
        }
        return VerifyCredentialResponse.ERROR;
    }

    private void requireAdministrator(int id, int serial, Action action) {
        UserInfo info = directory.user(id);
        if (id <= 0 || serial < 0 || info == null || info.id != id
                || info.serialNumber != serial || !info.isAdmin() || !info.isEnabled()
                || info.partial || info.preCreated || info.isGuest()
                || !UserManager.USER_TYPE_FULL_SECONDARY.equals(info.userType)) {
            throw new SecurityException("Current personal AOSP administrator required");
        }
        if (directory.restricted(id, UserManager.DISALLOW_APPS_CONTROL)
                || directory.restricted(id, action == Action.REMOVE
                        ? UserManager.DISALLOW_UNINSTALL_APPS : UserManager.DISALLOW_INSTALL_APPS)) {
            throw new SecurityException("AOSP disallows this package action");
        }
    }
}
