package org.aegisos.identity;

import android.os.UserHandle;

import java.util.Objects;

/**
 * Numeric layout for one AOSP owner's future user namespace. This is NOT an
 * authorization, live namespace, CE handle, or proof that a context may start.
 * The coordinator must recheck the authenticated owner/serial, CE state and
 * lifecycle guard immediately before use and reject surviving old contexts.
 */
public final class RuntimeUidMap {
    public static final int NORMAL_ID = 1000;
    private final AospIdentityBackend.UserKey owner;

    public RuntimeUidMap(AospIdentityBackend.UserKey owner) {
        this.owner = Objects.requireNonNull(owner);
        // Same exclusive allocation bound as the pinned UserManagerService.
        if (UserHandle.PER_USER_RANGE != RuntimeUidLayout.PER_USER_RANGE
                || owner.id < UserHandle.MIN_SECONDARY_USER_ID
                || owner.id >= UserHandle.MAX_SECONDARY_USER_ID) {
            throw new IllegalArgumentException("Unsupported personal AOSP user range");
        }
    }

    public AospIdentityBackend.UserKey owner() { return owner; }

    /** Stable metadata component; names and foreground-user state never participate. */
    public String storageKey() { return "u" + owner.id + "-s" + owner.serial; }

    /** Linux UID and GID use the same numbers, in their separate kernel ID spaces. */
    public int hostId(int linuxId) {
        for (int[] range : RuntimeUidLayout.ranges()) {
            if (linuxId >= range[0] && linuxId < range[0] + range[2]) {
                return compose(range[1] + linuxId - range[0]);
            }
        }
        throw new IllegalArgumentException("Linux ID has no reserved AOSP mapping");
    }

    private int compose(int appId) {
        int result = Math.addExact(Math.multiplyExact(owner.id, RuntimeUidLayout.PER_USER_RANGE), appId);
        if (result != UserHandle.getUid(owner.id, appId)
                || UserHandle.getUserId(result) != owner.id || UserHandle.getAppId(result) != appId) {
            throw new IllegalStateException("AOSP UID composition differs from the runtime layout");
        }
        return result;
    }

    /** Bytes for the future broker's uid_map AND gid_map; does not write procfs. */
    public String namespaceMap() {
        StringBuilder result = new StringBuilder();
        for (int[] range : RuntimeUidLayout.ranges()) {
            result.append(range[0]).append(' ').append(compose(range[1]))
                    .append(' ').append(range[2]).append('\n');
        }
        return result.toString();
    }
}
