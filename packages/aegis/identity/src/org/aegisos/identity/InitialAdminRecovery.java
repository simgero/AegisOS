package org.aegisos.identity;

import android.os.RemoteException;

/** Resumes only the recorded first-admin enrollment. Never resets an existing credential. */
final class InitialAdminRecovery {
    enum Phase { AVAILABLE, RESERVED, CREATED, COMPLETE }
    enum Credential { NONE, PASSWORD, OTHER }

    static final class Record {
        final Phase phase;
        final AospIdentityBackend.UserKey user;
        final String value;

        private Record(Phase phase, AospIdentityBackend.UserKey user, String value) {
            this.phase = phase;
            this.user = user;
            this.value = value;
        }

        static Record parse(String value) {
            if (value == null) return new Record(Phase.AVAILABLE, null, null);
            if ("reserved".equals(value)) return new Record(Phase.RESERVED, null, value);
            if (!value.matches("(created|complete):[1-9][0-9]*:(0|[1-9][0-9]*)")) {
                throw new IllegalStateException("Invalid first-admin setup record");
            }
            String[] parts = value.split(":");
            try {
                return new Record("created".equals(parts[0]) ? Phase.CREATED : Phase.COMPLETE,
                        new AospIdentityBackend.UserKey(Integer.parseInt(parts[1]),
                                Integer.parseInt(parts[2])), value);
            } catch (IllegalArgumentException invalid) {
                throw new IllegalStateException("Invalid first-admin setup identity", invalid);
            }
        }
    }

    static final class Snapshot {
        final AospIdentityBackend.UserKey user;
        final String name;
        final boolean enabled, admin, partial, running, foreground, ceUnlocked;
        final Credential credential;

        Snapshot(AospIdentityBackend.UserKey user, String name, boolean enabled, boolean admin,
                boolean partial, boolean running, boolean foreground, boolean ceUnlocked,
                Credential credential) {
            this.user = user;
            this.name = name;
            this.enabled = enabled;
            this.admin = admin;
            this.partial = partial;
            this.running = running;
            this.foreground = foreground;
            this.ceUnlocked = ceUnlocked;
            this.credential = credential;
        }
    }

    /** All credentials stay in the AOSP adapter's owned scope, never in this coordinator. */
    interface Platform {
        // Recheck the development-root client, AOSP restrictions and absence of other users.
        void guard() throws RemoteException;
        String readRecord();
        Snapshot inspect() throws RemoteException;
        void enrollPassword() throws RemoteException;
        void verifyPassword() throws RemoteException;
        void stopAndLock() throws RemoteException;
        void grantAdmin();
        void enable();
        void writeComplete(String value);
    }

    private static Snapshot checked(Platform platform, Record record, String name, String marker)
            throws RemoteException {
        platform.guard();
        if (!marker.equals(platform.readRecord())) {
            throw new SecurityException("First-admin reservation changed during recovery");
        }
        Snapshot state = platform.inspect();
        if (state == null || !record.user.equals(state.user) || !name.equals(state.name)
                || state.partial) {
            throw new SecurityException("Recorded first-admin user was changed or is incomplete");
        }
        return state;
    }

    private static void stoppedAndLocked(Snapshot state) {
        if (state.running || state.foreground || state.ceUnlocked) {
            throw new IllegalStateException("First-admin stop and CE lock were not confirmed");
        }
    }

    static Snapshot resume(Record record, String name, Platform platform) throws RemoteException {
        if (record.phase != Phase.CREATED || name == null) {
            throw new SecurityException("Recovery requires the recorded pending user");
        }
        Snapshot initial = checked(platform, record, name, record.value);
        if (initial.credential == null || initial.credential == Credential.OTHER
                || (initial.enabled && (!initial.admin || initial.credential != Credential.PASSWORD))
                || (!initial.enabled && (initial.running || initial.foreground))
                || (initial.credential == Credential.NONE && initial.admin)) {
            throw new SecurityException("Unexpected AOSP state for interrupted first-admin setup");
        }

        if (initial.credential == Credential.NONE) {
            // The recorded, disabled enrollment has no password yet.
            platform.enrollPassword();
        } else {
            // Existing passwords are authenticated through AOSP, never replaced or cleared.
            platform.verifyPassword();
        }
        Snapshot current = checked(platform, record, name, record.value);
        if (current.enabled != initial.enabled || current.admin != initial.admin
                || current.credential != Credential.PASSWORD
                || (!current.enabled && (current.running || current.foreground))) {
            throw new IllegalStateException("First-admin account changed during password operation");
        }

        // AOSP verification can unlock CE even while a disabled user is stopped.
        // Completion always requires the actual stopped and locked state again.
        platform.stopAndLock();
        current = checked(platform, record, name, record.value);
        stoppedAndLocked(current);
        if (current.enabled != initial.enabled || current.admin != initial.admin
                || current.credential != Credential.PASSWORD) {
            throw new IllegalStateException("First-admin account changed while stopping");
        }
        if (!current.admin) {
            platform.grantAdmin();
            current = checked(platform, record, name, record.value);
            stoppedAndLocked(current);
            if (!current.admin || current.enabled || current.credential != Credential.PASSWORD) {
                throw new IllegalStateException("AOSP admin assignment was not confirmed");
            }
        }
        if (!current.enabled) {
            platform.enable();
            current = checked(platform, record, name, record.value);
        }
        stoppedAndLocked(current);
        if (!current.admin || !current.enabled || current.credential != Credential.PASSWORD) {
            throw new IllegalStateException("Recovered first-admin account was not confirmed");
        }
        String complete = "complete:" + record.user.id + ":" + record.user.serial;
        platform.writeComplete(complete);
        current = checked(platform, record, name, complete);
        stoppedAndLocked(current);
        if (!current.admin || !current.enabled || current.credential != Credential.PASSWORD) {
            throw new IllegalStateException("Recovered first-admin account changed at completion");
        }
        return current;
    }
}
