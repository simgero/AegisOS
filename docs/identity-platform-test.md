# AOSP-Passwort- und CE-Test in QEMU

Am 28. September 2026 im lokalen Lauf `out/qemu-first-boot/mouse-1`
durchgeführt. Getestet wurden die vorhandenen Android-Dialoge und AOSP-Dienste,
**nicht** die noch unkompilierte AEGIS-Identitätsbibliothek oder eine AEGIS-CLI.

## Ablauf und Ergebnis

Ein temporärer persönlicher AOSP-Testbenutzer mit `userId=10`, Seriennummer 10
und einem zufälligen Passwort wurde eingerichtet. AOSP meldete den
Credential-Typ `PASSWORD`. Eine zufällige 4096-Byte-Datei lag in seinem
CE-Verzeichnis unter `/data/misc_ce/10/aegis-test/probe.bin`.

| Prüfung | Tatsächlich beobachtet |
| --- | --- |
| Angemeldeter Benutzer | `RUNNING_UNLOCKED`; CE-Benutzer `[0, 10]`; Datei bytegenau lesbar. |
| Benutzerwechsel zu 0 und Stoppen von Benutzer 10 | Nach dem asynchronen Schlüsselentzug nur CE-Benutzer `[0]`. Lesen des bisherigen Dateipfades als Gast-root über `adb shell` scheiterte mit Exitcode 1 und `ENOENT`. |
| Erneuter Start von Benutzer 10 | `RUNNING_LOCKED`; CE-Benutzer `[0]`. |
| Falsches Passwort | Android meldete „Wrong password. Try again.“; Benutzer und CE-Speicher blieben gesperrt. |
| Richtiges Passwort | `RUNNING_UNLOCKED`; CE-Benutzer `[0, 10]`; ursprüngliche Datei bytegenau wieder lesbar. |
| Passwortwechsel über Android-Einstellungen | Bisheriges Passwort bestätigt und ein neues Passwort gesetzt. |
| Altes Passwort nach erneutem Stoppen/Starten | Zurückgewiesen; `RUNNING_LOCKED`; CE-Benutzer `[0]`. |
| Neues Passwort | Benutzer und CE entsperrt; ursprüngliche 4096 Byte weiterhin unverändert. |
| Testabschluss | Zum Systembenutzer zurückgekehrt; Benutzer 10 beendet; CE-Benutzer `[0]`. |

SHA-256 der unveränderten Testdatei:

```text
ae14f6140f1be9d689318dbc20bea6cdb6f78fdcfdc184a7fbe3bd36dc9a70a0
```

Beide zufälligen Testpasswörter wurden über virtuelle Tastatureingaben
übermittelt, nicht als Befehlsargumente oder Umgebungsvariablen. Die beiden
Hilfsprozesse hielten sie nur im Speicher und überschrieben ihre
Credential-Puffer beim Beenden. Die vollständigen Passwörter wurden in keinem
der drei untersuchten Laufprotokolle (`android.log`, `helper.log`,
`logcat.log`) gefunden. Das ist eine Prüfung dieser Dateien, kein allgemeiner
Nachweis aller möglichen Speicher- oder Protokollkopien.

Die lokale Ergebnisdatei ist `out/qemu-first-boot/mouse-1/identity-evidence.json`.
Sie enthält keine Passwörter. Für den fehlgeschlagenen Dateizugriff wurde
`adb shell` verwendet: `adb exec-out` allein liefert keinen zuverlässigen
Exitcode des Gastbefehls. Erfolgreiche Binärlesevorgänge wurden zusätzlich
byteweise mit der ursprünglichen Datei verglichen.

## Aussagegrenzen

Der Gast lief mit SELinux `Enforcing` und authentifiziertem ADB. Die erfolgreiche
Passwortprüfung erfolgte durch AOSP; es wurde keine eigene Passwortdatenbank
oder Ersatzentschlüsselung eingesetzt. Die konkrete Dateisperre nach einem
Benutzerstopp wurde nachgewiesen; daraus folgt keine allgemeine Sicherheitsgarantie
gegen Gast-root oder den Mac-Eigentümer.

Android-Disk und Helper waren in diesem Lauf flüchtig. **Ein vollständiger
Neustart mit erhaltenen Dateien und Schlüsseln wurde nicht geprüft.** Dafür
fehlt der neue persistente Helper-Build. Ebenso offen sind der integrierte
AEGIS-Dienst, CLI, zwei persönliche Benutzer, Linux-Prozess-/Mount-Isolation,
Adminautorisierung und Pakettransaktionen. Die Benutzerwechsel dieses Tests
ersetzen insbesondere keinen vollständigen AEGIS-Logout mit Runtime-Abbau.
