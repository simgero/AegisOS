package org.aegisos.identity;

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
    // No shell/PTY/package endpoint until revocation and production policy are integrated.
    String linuxStart();
    String linuxStatus();
    String linuxStop();
}
