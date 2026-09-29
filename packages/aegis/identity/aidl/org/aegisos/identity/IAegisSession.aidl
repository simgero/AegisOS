package org.aegisos.identity;

import org.aegisos.identity.IAegisTerminal;

/** Credential parcels are sensitive. Each operation also checks the original process. */
@SensitiveData
interface IAegisSession {
    String login(String name, in byte[] password);
    String status();
    String listUsers();
    String changePassword(in byte[] previous, in byte[] replacement);
    String setupFirstAdmin(String name, in byte[] password);
    String addUser(String name, boolean administrator, in byte[] adminPassword, in byte[] password);
    String removeUser(String name, in byte[] adminPassword);
    String logout();
    void close();
    String resumeFirstAdmin(String name, in byte[] password);
    // Personal target is the service's authenticated binding, never a client userId.
    String linuxStart();
    String linuxStatus();
    String linuxStop();
    IAegisTerminal linuxShell(int rows, int columns);
    // Selects only the Android login target; grants neither identity nor CE access.
    // The CLI requests the password only after this preparation completes.
    void prepareLogin(String name);
}
