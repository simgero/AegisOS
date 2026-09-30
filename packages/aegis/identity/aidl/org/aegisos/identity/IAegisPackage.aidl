package org.aegisos.identity;

import android.os.Bundle;

/** Immutable process/session-bound transaction. No client-selected user, job or digest. */
@SensitiveData
interface IAegisPackage {
    Bundle status();
    Bundle approve(String administrator, in byte[] password);
    // Returns cleanup/publication status, never an unconditional rollback claim.
    Bundle cancel();
}
