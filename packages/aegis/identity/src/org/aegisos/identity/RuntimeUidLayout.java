// Generated from runtime/uid-map.json by scripts/runtime/uid_layout.py.
package org.aegisos.identity;

/** Layout only; AOSP authentication and runtime lifecycle checks remain mandatory. */
final class RuntimeUidLayout {
    static final int PER_USER_RANGE = 100000;
    private RuntimeUidLayout() {}

    /** A fresh array prevents callers from changing the process-wide layout. */
    static int[][] ranges() {
        return new int[][] {
            {0, 5000, 1000},
            {1000, 7500, 1},
            {65534, 7501, 1}
        };
    }
}
