package org.aegisos.identity;

import android.os.Binder;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.ServiceSpecificException;

import java.io.IOError;
import java.lang.ref.Reference;
import java.util.Arrays;

/** Interactive development terminal. No bearer token, password or session file is written. */
public final class Aegis {
    private static final String SERVICE_NAME = "aegis_identity";
    private TerminalConsole console;
    private final IBinder lifetime = new Binder();
    private IAegisSession session;
    private IAegisPackage packageJob;

    public static void main(String[] args) {
        Aegis cli = new Aegis();
        int code;
        try {
            code = cli.run(args);
        } catch (SecurityException e) {
            System.err.println("Zugriff abgelehnt oder persönliche Sitzung nicht angemeldet.");
            code = 1;
        } catch (RemoteException e) {
            System.err.println("AEGIS-Dienst nicht erreichbar. Abschluss der Aktion ist unbestätigt.");
            code = 1;
        } catch (RuntimeException | IOError e) {
            System.err.println("AEGIS konnte die Eingabe oder den Sitzungszustand nicht bestätigen.");
            code = 2;
        } catch (LinkageError unavailable) {
            System.err.println("Die geprüfte Terminal-Unterstützung fehlt in diesem Systemimage.");
            code = 2;
        } finally {
            if (cli.session != null) {
                try { cli.session.close(); } catch (RemoteException | RuntimeException ignored) { }
            }
            Reference.reachabilityFence(cli.lifetime);
        }
        System.exit(code);
    }

    private int run(String[] args) throws RemoteException {
        if (args.length == 1 && ("help".equals(args[0]) || "--help".equals(args[0]))) {
            help();
            return 0;
        }
        console = TerminalConsole.current();
        IBinder binder = ServiceManager.checkService(SERVICE_NAME);
        if (binder == null) {
            System.err.println("AEGIS-Anmeldedienst ist in diesem System nicht verfügbar.");
            return 1;
        }
        IAegisIdentity identity = IAegisIdentity.Stub.asInterface(binder);
        session = identity.openSession(lifetime);
        int result = 0;
        if (args.length > 0) {
            result = command(args);
            boolean login = "login".equals(args[0]) || "switch".equals(args[0]);
            if (result != 0 || !login) return result;
        }
        if (console == null) {
            System.err.println("Ein interaktives Terminal ist erforderlich; keine Passwort-Pipe.");
            return 2;
        }
        System.out.println("AEGIS-Terminal. help zeigt Befehle; logout meldet ab; exit schließt nur das Terminal.");
        for (;;) {
            String line = console.readLine("aegis> ");
            if (line == null || "exit".equals(line.trim())) {
                System.out.println("Terminalkanal beendet. Dies meldet den AOSP-Benutzer nicht ab.");
                return result;
            }
            if (line.isBlank()) continue;
            try {
                String[] words = tokenize(line);
                if (words.length > 0 && "aegis".equals(words[0])) {
                    words = Arrays.copyOfRange(words, 1, words.length);
                }
                result = command(words);
            } catch (SecurityException e) {
                System.err.println("Aktion abgelehnt. In diesem Terminal erneut anmelden.");
                result = 1;
            } catch (RuntimeException | IOError e) {
                System.err.println("Ungültige Eingabe oder nicht bestätigter Zustand.");
                result = 2;
            }
        }
    }

    private int command(String[] args) throws RemoteException {
        if (args.length == 0) { help(); return 2; }
        try {
            switch (args[0]) {
                case "help":
                    exact(args, 1);
                    help();
                    return 0;
                case "login":
                case "switch": {
                    exact(args, 2);
                    session.prepareLogin(args[1]);
                    byte[] password = password("Passwort: ");
                    try { System.out.println(session.login(args[1], password)); }
                    finally { wipe(password); }
                    return 0;
                }
                case "status":
                    exact(args, 1);
                    System.out.println(session.status());
                    return 0;
                case "setup": {
                    boolean resume = args.length == 3 && "--resume".equals(args[1]);
                    if (!resume) exact(args, 2);
                    if (Process.myUid() != Process.ROOT_UID) {
                        System.err.println("Die einmalige Ersteinrichtung benötigt die autorisierte Entwicklungs-Rootkonsole.");
                        return 1;
                    }
                    byte[] initial = confirmedPassword(resume
                            ? "Passwort der begonnenen Ersteinrichtung (falls gesetzt: bisheriges Passwort): "
                            : "Passwort des ersten Administrators: ");
                    String name = args[resume ? 2 : 1];
                    try { System.out.println(resume ? session.resumeFirstAdmin(name, initial)
                            : session.setupFirstAdmin(name, initial)); }
                    finally { wipe(initial); }
                    return 0;
                }
                case "package":
                    return packageCommand(args);
                case "user":
                    return userCommand(args);
                case "passwd": {
                    exact(args, 1);
                    byte[] previous = null, replacement = null, confirmation = null;
                    try {
                        previous = password("Bisheriges Passwort: ");
                        replacement = password("Neues Passwort: ");
                        confirmation = password("Neues Passwort wiederholen: ");
                        if (!Arrays.equals(replacement, confirmation)) {
                            System.err.println("Die neuen Passwörter stimmen nicht überein.");
                            return 2;
                        }
                        System.out.println(session.changePassword(previous, replacement));
                        return 0;
                    } finally {
                        wipe(previous); wipe(replacement); wipe(confirmation);
                    }
                }
                case "logout":
                    exact(args, 1);
                    System.out.println(session.logout());
                    return 0;
                case "linux":
                    if (args.length >= 2 && "package".equals(args[1])) {
                        return packageCommand(PackageCommand.fromLinux(args));
                    }
                    exact(args, 2);
                    switch (args[1]) {
                        case "start": System.out.println(session.linuxStart()); return 0;
                        case "status": System.out.println(session.linuxStatus()); return 0;
                        case "stop": System.out.println(session.linuxStop()); return 0;
                        case "shell": return linuxShell();
                        default:
                            System.err.println("Verfügbar: linux start, status, stop, shell oder package.");
                            return 2;
                    }
                default:
                    System.err.println("Unbekannter Befehl. help zeigt verfügbare Befehle.");
                    return 2;
            }
        } catch (ServiceSpecificException e) {
            if (e.errorCode == 1) {
                System.err.println("AOSP hat das Passwort abgewiesen.");
            } else if (e.errorCode == 2) {
                try {
                    int millis = Integer.parseInt(e.getMessage());
                    System.err.println("AOSP verlangt eine Wartezeit von mindestens "
                            + ((Math.max(0L, millis) + 999L) / 1000L) + " Sekunden.");
                } catch (NumberFormatException invalidDelay) {
                    System.err.println("AOSP verlangt eine Wartezeit vor dem nächsten Versuch.");
                }
            } else {
                System.err.println("Aktion nicht bestätigt. Tatsächlichen Benutzer-/Speicherzustand prüfen.");
            }
            return 1;
        }
    }

    private int packageCommand(String[] args) throws RemoteException {
        if (console == null) throw new IllegalStateException("Interactive terminal required");
        if (args.length == 2) {
            if (packageJob == null) throw new IllegalStateException("No package job in this terminal");
            switch (args[1]) {
                case "status": printPackage(packageJob.status()); return 0;
                case "cancel": printPackage(packageJob.cancel()); return 0;
                case "approve": return approvePackage(packageJob.status());
                default: throw new IllegalArgumentException("Unknown package command");
            }
        }
        if (args.length < 3 || !("--user".equals(args[2]) || "--all".equals(args[2]))) {
            throw new IllegalArgumentException("Explicit package scope required");
        }
        String name = "", version = "";
        switch (args[1]) {
            case "install":
                if (args.length != 4 && args.length != 5) throw new IllegalArgumentException("Package/version required");
                name = args[3]; version = args.length == 5 ? args[4] : ""; break;
            case "remove": exact(args, 4); name = args[3]; break;
            case "update": exact(args, 3); break;
            default: throw new IllegalArgumentException("Unknown package action");
        }
        packageJob = session.packageBegin(args[1], args[2].substring(2), name, version);
        return approvePackage(waitForPackage(false));
    }

    private android.os.Bundle waitForPackage(boolean started) throws RemoteException {
        String previous = "";
        for (;;) {
            android.os.Bundle state = packageJob.status();
            String phase = state.getString("state");
            if (!phase.equals(previous) && !"approval_required".equals(phase)) {
                printPackage(state); previous = phase;
            }
            if ("complete".equals(phase) || "closed".equals(phase) || "cancelling".equals(phase)
                    || (!started && "approval_required".equals(phase))) return state;
            try { Thread.sleep(300); }
            catch (InterruptedException interrupted) {
                Thread.currentThread().interrupt();
                packageJob.cancel();
                throw new IllegalStateException("Package wait interrupted");
            }
        }
    }

    private int approvePackage(android.os.Bundle state) throws RemoteException {
        if (!"approval_required".equals(state.getString("state"))) {
            printPackage(state);
            return "published".equals(state.getString("outcome")) ? 0 : 1;
        }
        printPackage(state);
        byte[] credential = null;
        boolean submitted = false;
        try {
            String admin = console.readLine("Admin-Benutzer für diesen Plan (leer bricht ab): ");
            if (admin == null || admin.isBlank()) return 1;
            credential = password("Admin-Passwort für diesen Plan: ");
            printPackage(packageJob.approve(admin, credential));
            submitted = true;
        } finally {
            wipe(credential);
            if (!submitted) printPackage(packageJob.cancel());
        }
        return "published".equals(waitForPackage(true).getString("outcome")) ? 0 : 1;
    }

    private void printPackage(android.os.Bundle result) {
        switch (result.getString("state")) {
            case "preparing": System.out.println("Paketplan wird vorbereitet. Strg+C beendet den Auftrag."); break;
            case "approval_required":
                System.out.println("Paketplan: " + result.getString("action") + ", Bereich: "
                        + ("all".equals(result.getString("scope")) ? "gemeinsame Software" : "eigene Pakete"));
                java.util.ArrayList<android.os.Bundle> changes = result.getParcelableArrayList("changes", android.os.Bundle.class);
                if (changes == null || changes.isEmpty()) throw new IllegalStateException("Missing complete review");
                for (android.os.Bundle change : changes) {
                    String before = change.getString("before"), after = change.getString("after");
                    if ("private_selection".equals(change.getString("effect"))) {
                        System.out.println("  " + change.getString("name") + ": privat festhalten, Version " + after + " unverändert");
                        continue;
                    }
                    System.out.println("  " + change.getString("name") + " (" + change.getString("architecture") + "): "
                            + (before.isEmpty() ? "nicht installiert" : before) + " -> "
                            + (after.isEmpty() ? "entfernt" : after) + " ["
                            + ("manual".equals(change.getString("reason")) ? "angefordert" : "Abhängigkeit") + "]");
                }
                break;
            case "running": System.out.println("Paketänderung läuft; Veröffentlichung wird geprüft."); break;
            case "cancelling": System.out.println("Abbruch angefordert; Aufräumen noch nicht bestätigt."); break;
            case "closed": System.out.println("Auftrag geschlossen. Paketänderungen sind nicht bestätigt."); break;
            case "complete":
                if ("published".equals(result.getString("outcome"))) {
                    System.out.println("Paketänderung erfolgreich veröffentlicht. Für die neue Software den eigenen Linux-Kontext neu starten.");
                } else if ("failed".equals(result.getString("outcome"))) {
                    System.out.println("Paketauftrag fehlgeschlagen. Keine erfolgreiche Veröffentlichung bestätigt.");
                } else System.out.println("Paketauftrag beendet; Ergebnis unbestätigt.");
                break;
            default: throw new IllegalStateException("Unknown package state");
        }
    }

    private int linuxShell() throws RemoteException {
        if (console == null) throw new IllegalStateException("Interactive terminal required");
        int[] size = TerminalNative.consoleSize();
        IAegisTerminal terminal = session.linuxShell(size[0], size[1]);
        byte[] pending = null;
        boolean raw = false;
        try {
            TerminalNative.beginMode(1);
            raw = true;
            long nextStatus = 0, nextResize = 0, eofDeadline = 0;
            for (;;) {
                byte[] output = terminal.read();
                if (output == null) {
                    if (eofDeadline == 0) eofDeadline = System.nanoTime() + 10_000_000_000L;
                } else {
                    try { System.out.write(output, 0, output.length); System.out.flush(); }
                    finally { wipe(output); }
                }
                long now = System.nanoTime();
                if (now >= nextStatus) {
                    int code = terminal.exitStatus();
                    nextStatus = now + 200_000_000L;
                    if (code >= 0) {
                        // Drain the bounded kernel buffer, but never wait for a
                        // background process that keeps writing after bash exits.
                        for (int count = 0; count < 64; ++count) {
                            output = terminal.read();
                            if (output == null || output.length == 0) break;
                            try { System.out.write(output, 0, output.length); }
                            finally { wipe(output); }
                        }
                        System.out.flush();
                        return code;
                    }
                }
                if (eofDeadline != 0 && now >= eofDeadline) {
                    throw new IllegalStateException("Terminal ended without a confirmed command result");
                }
                if (now >= nextResize) {
                    int[] next = TerminalNative.consoleSize();
                    if (!Arrays.equals(size, next)) {
                        terminal.resize(next[0], next[1]);
                        size = next;
                    }
                    nextResize = now + 500_000_000L;
                }
                if (pending == null && eofDeadline == 0) {
                    pending = TerminalNative.readInput(4096, 20);
                    if (pending == null) throw new IllegalStateException("Local terminal disconnected");
                }
                if (pending != null && pending.length > 0) {
                    int written = terminal.write(pending);
                    if (written < 0 || written > pending.length) {
                        throw new IllegalStateException("Invalid terminal write acknowledgement");
                    }
                    byte[] remainder = written == pending.length ? null
                            : Arrays.copyOfRange(pending, written, pending.length);
                    wipe(pending);
                    pending = remainder;
                } else { wipe(pending); pending = null; }
                if (pending != null || eofDeadline != 0) {
                    try { Thread.sleep(10); }
                    catch (InterruptedException interrupted) {
                        Thread.currentThread().interrupt();
                        throw new IllegalStateException("Terminal interrupted");
                    }
                }
            }
        } finally {
            wipe(pending);
            try { if (raw) TerminalNative.endMode(); }
            finally { terminal.close(); }
        }
    }

    private int userCommand(String[] args) throws RemoteException {
        if (args.length == 2 && "list".equals(args[1])) {
            System.out.println(session.listUsers());
            return 0;
        }
        if (args.length >= 2 && "add".equals(args[1])) {
            if (args.length != 3 && !(args.length == 4 && "--admin".equals(args[3]))) {
                throw new IllegalArgumentException("Unexpected user-add arguments");
            }
            byte[] initial = null, admin = null;
            try {
                initial = confirmedPassword("Passwort des neuen Benutzers: ");
                admin = password("Passwort des angemeldeten Administrators für diese Anlage: ");
                System.out.println(session.addUser(args[2], args.length == 4, admin, initial));
                return 0;
            } finally {
                wipe(initial); wipe(admin);
            }
        }
        if (args.length == 3 && "remove".equals(args[1])) {
            byte[] admin = password("Adminpasswort für die Löschung dieses Benutzers und seiner Daten: ");
            try { System.out.println(session.removeUser(args[2], admin)); }
            finally { wipe(admin); }
            return 0;
        }
        throw new IllegalArgumentException("Expected user list, add NAME [--admin], or remove NAME");
    }

    private byte[] confirmedPassword(String prompt) {
        byte[] first = null, second = null;
        boolean confirmed = false;
        try {
            first = password(prompt);
            second = password("Passwort wiederholen: ");
            if (!Arrays.equals(first, second)) {
                throw new IllegalArgumentException("Passwords do not match");
            }
            confirmed = true;
            return first;
        } finally {
            wipe(second);
            if (!confirmed) wipe(first);
        }
    }

    private byte[] password(String prompt) {
        if (console == null) throw new IllegalStateException("Interactive terminal required");
        char[] chars = console.readPassword("%s", prompt);
        if (chars == null) throw new IllegalStateException("Password input ended");
        byte[] result = null;
        boolean complete = false;
        try {
            if (chars.length < 4 || chars.length > 128) {
                throw new IllegalArgumentException("Password length outside protocol bounds");
            }
            result = new byte[chars.length];
            for (int i = 0; i < chars.length; i++) {
                // Match the pinned AOSP credential character model without UTF-8/truncation.
                if (chars[i] < 32 || chars[i] > 126) {
                    throw new IllegalArgumentException("Unsupported password character");
                }
                result[i] = (byte) chars[i];
            }
            complete = true;
            return result;
        } finally {
            Arrays.fill(chars, '\0');
            if (!complete) wipe(result);
        }
    }

    private static void wipe(byte[] bytes) { if (bytes != null) Arrays.fill(bytes, (byte) 0); }

    private static void exact(String[] args, int count) {
        if (args.length != count) {
            // Never echo unexpected arguments: a user may have accidentally entered a password.
            throw new IllegalArgumentException("Unexpected argument count");
        }
    }

    /** Quotes group display names; there is no shell expansion or command execution. */
    private static String[] tokenize(String line) {
        java.util.List<String> words = new java.util.ArrayList<>();
        StringBuilder word = new StringBuilder();
        char quote = 0;
        boolean present = false;
        for (int i = 0; i < line.length(); i++) {
            char c = line.charAt(i);
            if (quote != 0) {
                if (c == quote) quote = 0;
                else word.append(c);
                present = true;
            } else if (c == '\'' || c == '"') {
                quote = c;
                present = true;
            } else if (Character.isWhitespace(c)) {
                if (present) { words.add(word.toString()); word.setLength(0); present = false; }
            } else {
                word.append(c);
                present = true;
            }
        }
        if (quote != 0) throw new IllegalArgumentException("Unclosed quote");
        if (present) words.add(word.toString());
        return words.toArray(new String[0]);
    }

    private static void help() {
        System.out.println("AEGIS – interaktiver Entwicklungszugang\n"
                + "  aegis [login NAME]     Terminal öffnen, optional sofort anmelden\n"
                + "Im selben Terminal:\n"
                + "  setup NAME            einmalig den ersten Admin einrichten (Entwicklungs-Root)\n"
                + "  setup --resume NAME   protokollierte Ersteinrichtung fortsetzen; kein Passwortreset\n"
                + "  user list             persönliche AOSP-Benutzer auflisten\n"
                + "  user add NAME [--admin] Benutzer mit Passwort anlegen; frische Adminprüfung\n"
                + "  user remove NAME      anderen Benutzer löschen; frische Adminprüfung\n"
                + "  login NAME            Passwort prüfen und Benutzer aktivieren\n"
                + "  switch NAME           Ziel authentifizieren; bisherigen Benutzer nicht abmelden\n"
                + "  passwd                eigenes Passwort über AOSP ändern\n"
                + "  status                diesen Sitzungs- und Speicherzustand anzeigen\n"
                + "  linux start|status|stop eigenen Kontext steuern, sobald die Runtime installiert ist\n"
                + "  linux shell           persönliche GNU/Linux-Shell; exit beendet nur die Shell\n"
                + "  logout                Android-Benutzer stoppen und CE-Sperre bestätigen\n"
                + "  exit                  nur den Terminalkanal schließen\n"
                + "  linux package install NAME[=VERSION] --scope user|all  Plan prüfen und freigeben\n"
                + "  linux package update --scope user|all                 Pakete aktualisieren\n"
                + "  linux package remove NAME --scope user|all            Paket entfernen\n"
                + "  linux package status|approve|cancel                   Auftrag dieses Terminals\n"
                + "  Die Kurzform package … --user|--all bleibt verfügbar.\n"
                + "Ein getrennt gestartetes aegis erbt keine persönliche Anmeldung.");
    }
}
