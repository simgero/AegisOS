package org.aegisos.identity;

import android.content.Context;
import android.os.Binder;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import com.android.internal.widget.LockscreenCredential;
import com.android.internal.widget.VerifyCredentialResponse;
import com.android.server.LocalServices;
import com.android.server.aegis.AegisPackageCredentials;
import java.util.Objects;

/** Real AOSP adapter for PackageApproval; not exposed over Binder or wired to a CLI yet. */
final class AospPackageAuthority implements PackageApproval.Authority {
    private final AospIdentityBackend backend;
    private final UserManager users;

    AospPackageAuthority(Context context, AospIdentityBackend backend) {
        this.backend = Objects.requireNonNull(backend);
        users = Objects.requireNonNull(context.getSystemService(UserManager.class));
    }

    @Override public void requireRequester(PackageApproval.Prepared plan) throws RemoteException {
        long caller = Binder.clearCallingIdentity();
        try {
            AospIdentityBackend.State state = backend.state(plan.requester);
            if (!state.user.equals(plan.requester) || !state.enabled || state.partial
                    || !state.running || !state.ceUnlocked) {
                throw new SecurityException("Package requester is no longer admitted by AOSP");
            }
            UserHandle user = UserHandle.of(plan.requester.id);
            if (users.hasUserRestriction(UserManager.DISALLOW_APPS_CONTROL, user)
                    || users.hasUserRestriction(plan.action == PackageApproval.Action.REMOVE
                            ? UserManager.DISALLOW_UNINSTALL_APPS : UserManager.DISALLOW_INSTALL_APPS, user)) {
                throw new SecurityException("AOSP restricts the requester's package action");
            }
        } finally { Binder.restoreCallingIdentity(caller); }
    }

    @Override public VerifyCredentialResponse confirm(AospIdentityBackend.UserKey administrator,
            PackageApproval.Action action, LockscreenCredential credential) {
        // No fallback to the normal login verifier: it would unlock the admin's
        // storage. Missing new LockSettings service means this operation fails.
        AegisPackageCredentials verifier = Objects.requireNonNull(
                LocalServices.getService(AegisPackageCredentials.class), "Package verifier unavailable");
        return verifier.verify(administrator.id, administrator.serial,
                AegisPackageCredentials.Action.valueOf(action.name()), credential);
    }
}
