package org.aegisos.identity;

import android.os.IBinder;
import org.aegisos.identity.IAegisSession;

/** Internal developer-console protocol; no implicit session based on foreground user. */
interface IAegisIdentity {
    IAegisSession openSession(IBinder clientLifetime);
}
