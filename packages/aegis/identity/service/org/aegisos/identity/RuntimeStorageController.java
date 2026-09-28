package org.aegisos.identity;

import com.android.server.aegis.AegisRuntimeStorage;
import java.util.Objects;

/**
 * Provider adapter for a fully initialized managed runtime owner. Deliberately
 * NOT registered by the current runtime-absent identity service: the real
 * Quiescer, start/package/lifecycle wiring and SELinux policy are still missing.
 */
final class RuntimeStorageController implements AegisRuntimeStorage.Controller {
    private final RuntimeAdmission admission;

    RuntimeStorageController(RuntimeAdmission admission) {
        this.admission = Objects.requireNonNull(admission);
    }

    @Override public AegisRuntimeStorage.Lease begin(int userId,
            AegisRuntimeStorage.Operation operation) {
        RuntimeAdmission.Storage storage = admission.storage(userId,
                Objects.requireNonNull(operation).requiresQuiescence);
        return storage::close;
    }
}
