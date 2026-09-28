# Phase 1: Implementierungsstand

Stand: 28. September 2026. Keine Abnahme des Gesamtziels.

| Anforderung | Nachweis / verbleibende Arbeit |
| --- | --- |
| Lokaler Android-Start | Bootabschluss und sichtbare Oberfläche bestätigt, siehe `qemu-first-boot.md`. |
| ADB und Bildschirm | Authentifizierte Verbindung, Dateiübertragung und Bildschirmaufnahme geprüft; siehe `local-adb.md`. |
| Bedienung | Virtuelle Tastatur schreibt den vollständigen Testtext; relative Maus öffnet mit linkem Klick eine Einstellungsseite. Native Mac-Fensterbedienung noch prüfen; Mac beim Versuch gesperrt. |
| Dauerhafte Daten und Schlüssel | Gekoppelte Profile implementiert und mit echten Diskdateien getestet. Neuer Helper-Build und echter Passwort-/Neustarttest fehlen; siehe `persistent-qemu.md`. |
| Gerätedienste | Bluetooth stürzt im bisherigen Image wegen fehlender HCI-Gegenstelle ab. Der nächste Produktbuild schaltet Bluetooth über den vorhandenen AOSP-Schalter ab; noch kein Gastnachweis. Thread/UWB/NFC und weitere geerbte Geräte bleiben zu bereinigen. |
| AEGIS-Identität/CLI | Interner AOSP-Adapter für Identitätsprüfung, Passwortprüfung/-wechsel und den Android-Teil des Logouts im Quelltext angelegt. Noch unkompiliert und nicht eingebunden; CLI, Aufrufer-/Sitzungsbindung und Adminaktionen fehlen. Siehe `packages/aegis/identity/README.md`. |
| AOSP-Passwortgrundlage | Ein persönlicher Testbenutzer: falsches Passwort abgewiesen, CE-Sperre nach Benutzerstopp bestätigt, richtiges Passwort stellt Dateizugriff wieder her. Nach Passwortwechsel wird das alte Passwort abgewiesen; das neue erhält dieselben Daten. Test über Android-Dialoge, noch nicht über AEGIS; siehe `identity-platform-test.md`. |
| GNU/Linux-Runtime | Noch zu implementieren. Der aktuelle Kernel erfüllt die notwendigen Namespace-Anforderungen nicht. |
| Pakete und Isolation | Noch zu implementieren und mit zwei AOSP-Benutzern praktisch zu prüfen. |
| Vollständiger Ablauf | Noch kein Nachweis für Login, Wechsel, Logout mit CE-Sperrung und Neustart mit zwei passwortgeschützten Benutzern. |

## Bestätigte Kernel-Lücke

Der laufende Kernel 6.12.18 meldet in `/proc/config.gz`:

```text
# CONFIG_SYSVIPC is not set
# CONFIG_USER_NS is not set
# CONFIG_PID_NS is not set
# CONFIG_VIRTIO_NET is not set
CONFIG_UTS_NS=y
CONFIG_NET_NS=y
CONFIG_EXT4_FS=y
CONFIG_OVERLAY_FS=y
```

Die Zeile zu `CONFIG_VIRTIO_NET` beschreibt nur die GKI-Konfiguration: Im
laufenden Gast ist `virtio_net` als separates, passendes Virtual-Device-Modul
bereits geladen (`/proc/modules`, Lauf `mouse-1`). Daraus folgt kein fehlender
Netzwerktreiber. Der Launcher richtet bislang keine Gast-Netzwerkkarte ein.

Wegen der fehlenden User-/PID-Namespaces und System-V-IPC ist für die
beauftragte gemeinsame GNU/Linux-Runtime ein gezielter
Kernel-Build einschließlich passender Module erforderlich. User-, PID-, Mount-
und IPC-Isolation müssen anschließend tatsächlich funktionieren. Ein chroot
allein oder eine zusätzliche Linux-VM erfüllt den Auftrag nicht. Der genaue
Kernel-Quellstand, Konfiguration, Module und Android-Integration sind vor dem
Build abzugleichen; das bloße Hinzufügen von Konfigurationszeilen reicht nicht.

Die passenden 40 Kernel-Quellprojekte sind inzwischen aus dem offiziellen
Manifest des laufenden Builds `13257114` festgelegt. Ein Buildrezept mit
gemeinsamem Namespace-Fragment für Kernel und Module liegt unter
[`kernel/`](../kernel/README.md). Es ist noch nicht auf dem Builder ausgeführt;
die Integration der Ergebnisse in neue AOSP-Images und Gasttests fehlen.

## Reihenfolge

1. Neuen Helper auf `aegis-build` bauen, über GitHub beziehen und gekoppelten
   Neustart mit geschützten Daten prüfen.
2. QEMU-Hardwarekonfiguration und Bedienung bereinigen; Kernel-Build vorbereiten.
3. AOSP-vermittelte AEGIS-Anmeldung und Benutzerlebenszyklus integrieren.
4. Runtime und Paketoperationen mit AOSP-Adminautorisierung integrieren.
5. Alle Anmelde-, Daten-, Prozess- und Logout-Anforderungen mit zwei Benutzern
   einschließlich Fehlerfällen prüfen.

Bei der letzten Verbindungsprüfung war `aegis-build` per SSH nicht erreichbar;
Tailscale meldete den Peer als offline (zuletzt gesehen 2026-09-28 00:20 UTC).
Es wurde kein Ersatzbuild auf dem Mac gestartet und keine Serverkonfiguration
geändert. Diese Erreichbarkeit muss vor dem nächsten Build erneut geprüft werden.
