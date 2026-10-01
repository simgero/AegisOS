> Historischer Entwurf. Entwicklungsumgebung und Umfang wurden durch den [verbindlichen Entwicklerauftrag](phase-1-developer-brief.md) ersetzt. Aktuelle Nachweise: [Implementierungsstand](../phase-1-progress.md).

# Phase 1 — Terminal-Foundation

**Status:** Umsetzungsplan, 2026-09-27. Befehle mit `aegis` sind unten spezifiziert, noch nicht vorhanden.

## P0 — Architektur und Entwicklungsrechner prüfen

Diese Änderung liefert Architektur, Bedrohungsmodell und `scripts/check-host.sh`. Auf dem Linux-Entwicklungsrechner im Repository ausführen:

```bash
bash scripts/check-host.sh
```

Die Prüfung verändert weder VM-Konfiguration noch Netzwerk, Pakete oder Benutzerkonten. Sie meldet fehlende Voraussetzungen; sie ist kein Cuttlefish-Starttest.

**Wichtig:** Ein Ubuntu-Host ist nicht AOSP. Dort lassen sich Werkzeuge und plattformunabhängige Tests ausführen, aber kein Android-FBE-/Gatekeeper-Nachweis erbringen.

## P1 — Unveränderte Android-Referenz starten

Für einen x86_64-Linux-Entwicklungsrechner zunächst ein passendes x86_64-Cuttlefish-Image verwenden. ARM64 bleibt zusätzliches Ziel, ist aber nicht Voraussetzung für die ersten Integrationstests.

Vor Download werden konkrete Build-ID, Target, Artefaktnamen, Herkunft und Prüfsummen dokumentiert. Host-Paket und Images müssen zusammenpassen; keine undokumentierten `latest`-Downloads. Cuttlefish benötigt nutzbare Virtualisierung/KVM. In einer Hyper-V-Gastmaschine muss verschachtelte Virtualisierung gesondert geprüft werden. [AOSP-Anleitung](https://source.android.com/docs/devices/cuttlefish/get-started)

Abnahme: Start, ADB-Zugriff, Neustart und persistente Testdaten funktionieren. Security-Dienste und SELinux-Modus werden festgestellt, nicht aus dem Image-Namen abgeleitet. Keine öffentlich erreichbaren ADB- oder Debugging-Ports.

Ein vollständiger AOSP-Checkout ist dafür nicht vorgesehen. Falls später selbst gebaut wird, werden Ressourcen neu geplant: AOSP nennt aktuell x86_64-Linux, mindestens 400 GB freien Speicher und 64 GB RAM für den vollständigen Buildpfad. Das ist keine Mindestanforderung dieser Dokumentation oder der CLI. ARM64-Zielarchitektur ist nicht gleich ARM64-Buildhost. [AOSP-Buildvoraussetzungen](https://source.android.com/docs/setup/start/requirements)

## P2 — Kleinen Core mit lesender Plattformanbindung entwickeln

Die bestehende Repository-Struktur bleibt erhalten. Vorgeschlagene spätere Module unter `packages/aegis/`:

```text
cli/               Argumente, verdeckte Eingabe, Ausgabe
core/              Zustandsmodell und Autorisierungsregeln
platform/android/  versionsgebundene AOSP-Anbindung
platform/test/     ausdrücklich markierter Adapter nur für Tests
```

Der erste ausführbare Befehl soll `aegis status --json` werden. Vorgesehene Felder: Schema-Version, Plattform, Backend, Benutzerstatus, CE-Verfügbarkeit, SELinux-Modus und Hardware-Sicherheitsniveau. Nicht ermittelte Werte werden als `unknown` ausgegeben, nicht als sicher vorausgesetzt.

Tests: Eingabevalidierung, Aufrufer-/Zielbenutzer-Prüfung, Race Conditions, Abbruch, Idempotenz und verweigerte Operationen. Mocks nur in Unit-Tests und expliziten Testprogrammen mit synthetischen Daten; nicht als Login-Alternative im ausgelieferten CLI.

## P3 — Enrollment und Passwortanmeldung an AOSP anbinden

Geplante, noch nicht implementierte Oberfläche:

```text
aegis user add <name>        # administrativ; verdeckte Passwortabfrage
aegis user list             # nur mit passender Berechtigung
aegis login <name>          # echte Plattformauthentifizierung
aegis passwd               # Passwortwechsel über die Plattform
aegis logout               # Sitzung beenden und Datensperre bestätigen
aegis status --json
```

Vor Implementierung eine kleine Integrationsmatrix erstellen: benötigte AOSP-API, Sichtbarkeit, Signatur-/Systemberechtigung, Aufrufer-Domäne und erforderliche Image-Änderung. Für nicht sicher erreichbare APIs gibt es einen begrenzten Produktbuild, keinen unsicheren Shell-Workaround. `locksettings` mit Passwort im Prozessargument ist kein zulässiger endgültiger Login-Pfad.

Eine einzelne erfolgreiche Authentifizierung muss mit dem richtigen Benutzer, Aufrufer und der richtigen Sitzung verbunden sein. Abbruch oder falsches Passwort erzeugen keine nutzbare Sitzung. Administrative Erstinitialisierung und Passwortverlust erhalten getrennte Abläufe; ein Reset darf nicht als Entschlüsselung ohne Credential ausgegeben werden.

## P4 — Benutzer- und Storage-Lifecycle belegen

Automatisierte Integrationstests mit zwei synthetischen Android-Benutzern:

| Prüfung | Erwartung |
|---|---|
| Falsches Passwort | Keine Sitzung, kein CE-Zugriff. |
| Anmeldung A | A liest eigene Datei; B bleibt unverändert gesperrt. |
| Beide angemeldet | A erhält durch Aegis keine Rechte auf Bs private Daten. |
| Logout A | Sitzungen geschlossen; negative Dateizugriffstests und tatsächlicher Storage-Status bestätigen Sperre. |
| Offene Datei / verweigerter Stop | Kein falscher `LOCKED`-Status, nachvollziehbarer Fehler. |
| Passwortwechsel | Neues Passwort funktioniert; altes nicht; Dateien unverändert lesbar. |
| Neustart | Vor Entsperrung kein privater Klartextzugriff, danach persistente Daten. |
| Löschung und neuer Benutzer | Keine Wiederverwendung alter Berechtigungen, Schlüsselzugriffe oder Storage-Zuordnung. |
| Dienstabsturz | Keine automatische Wiederherstellung einer autorisierten Sitzung ohne überprüften Plattformstatus. |
| Protokollierung | Keine Credentials, Rohschlüssel oder Dateiinhalte in Logs. |

Tests müssen als passende unprivilegierte Clients laufen. Administrative Diagnosezugriffe sind getrennt zu protokollieren; sie ersetzen keinen Test zwischen Benutzern.

## P5 — GNU/Linux-Runtime erst danach

Noch kein Rootfs-Download und kein Container-Daemon. Erst nach P4 werden Kernel-/Namespace-Voraussetzungen, Host-UID-Zuordnung, CE-Speicher, Mount-/IPC-/Netzwerkgrenzen und Paketupdates implementiert und geprüft.

## Nicht Bestandteil dieser Änderung

Keine CLI-Implementierung, keine Systeminstallation, kein AOSP-Build, keine Kryptografieimplementierung und keine Änderung von SSH, Tailscale, Hyper-V oder GitHub-Berechtigungen. Der Architektur-PR soll zuerst geprüft werden; die vorhandene CI bleibt unverändert.
