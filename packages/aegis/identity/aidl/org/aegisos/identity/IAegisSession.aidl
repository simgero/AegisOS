package org.aegisos.identity;

/** Credential parcels are sensitive. Each operation also checks the original process. */
@SensitiveData
interface IAegisSession {
    String login(String name, in byte[] password);
    String status();
    String listUsers();
    String changePassword(in byte[] previous, in byte[] replacement);
    String logout();
    void close();
}
