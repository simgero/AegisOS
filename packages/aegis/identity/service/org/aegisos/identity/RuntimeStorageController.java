package org.aegisos.identity;

import com.android.server.aegis.AegisRuntimeStorage;
import java.util.Objects;

/**
 * Provider adapter for a fully initialized managed runtime owner.
 * Registered by the identity service only in managed-v1 mode. The current
 * product remains absent pending native init/SELinux integration. The quiescer
 * must be extended before enabling any public terminal or package endpoints.
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
