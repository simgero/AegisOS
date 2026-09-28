package org.aegisos.identity;

import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;

import com.android.internal.widget.ILockSettings;

/** Preserves AIDL value/ownership semantics even when both services live in system_server. */
final class CredentialTransport {
    private CredentialTransport() { }

    static ILockSettings connect(IBinder target) {
        // An un-attached Binder has no local interface: the generated AIDL proxy therefore
        // marshals each request. A direct asInterface(target) can return LockSettingsService
        // itself and share our credential object with its asynchronous password-change work.
        return ILockSettings.Stub.asInterface(new Binder() {
            @Override protected boolean onTransact(int code, Parcel data, Parcel reply, int flags)
                    throws RemoteException {
                data.markSensitive();
                if (reply != null) reply.markSensitive();
                return target.transact(code, data, reply, flags | IBinder.FLAG_CLEAR_BUF);
            }
        });
    }
}
