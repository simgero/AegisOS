package org.aegisos.identity;

import android.content.ContentResolver;
import android.os.Bundle;
import android.os.UserHandle;
import android.provider.Settings;

/** AOSP-owned provisioning metadata only. Never consulted for identity or admin privileges. */
final class AospSetupState {
    private static final String KEY = "aegis_initial_setup_state";
    private final ContentResolver resolver;

    AospSetupState(ContentResolver resolver) { this.resolver = resolver; }

    String read() {
        Bundle arguments = new Bundle();
        arguments.putInt(Settings.CALL_METHOD_USER_KEY, UserHandle.USER_SYSTEM);
        // A normal Global.getString can return null on provider failure as well as absence.
        // The pinned provider returns a non-null bundle containing VALUE even for a missing key.
        Bundle result = resolver.call(Settings.Global.CONTENT_URI,
                Settings.CALL_METHOD_GET_GLOBAL, KEY, arguments);
        if (result == null || !result.containsKey(Settings.NameValueTable.VALUE)) {
            throw new IllegalStateException("AOSP provisioning state is unavailable");
        }
        return result.getString(Settings.NameValueTable.VALUE);
    }

    void write(String value) {
        Bundle arguments = new Bundle();
        arguments.putInt(Settings.CALL_METHOD_USER_KEY, UserHandle.USER_SYSTEM);
        arguments.putString(Settings.NameValueTable.VALUE, value);
        resolver.call(Settings.Global.CONTENT_URI, Settings.CALL_METHOD_PUT_GLOBAL, KEY, arguments);
        if (!value.equals(read())) {
            throw new IllegalStateException("AOSP did not confirm provisioning state");
        }
    }
}
