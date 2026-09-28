# Phase 1: Implementierungsstand

Stand: 28. September 2026. Keine Abnahme des Gesamtziels.

| Anforderung | Nachweis / verbleibende Arbeit |
| --- | --- |
| Lokaler Android-Start | Bootabschluss und sichtbare Oberfläche bestätigt, siehe `qemu-first-boot.md`. |
| ADB und Bildschirm | Authentifizierte Verbindung, Dateiübertragung und Bildschirmaufnahme geprüft; siehe `local-adb.md`. |
| Bedienung | Virtuelle Eingaben erreichen den Kernel, ADB-Eingabe funktioniert. Native Fensterbedienung noch vollständig testen. |
| Dauerhafte Daten und Schlüssel | Gekoppelte Profile implementiert und mit echten Diskdateien getestet. Neuer Helper-Build und echter Passwort-/Neustarttest fehlen; siehe `persistent-qemu.md`. |
| Gerätedienste | Bluetooth stürzt wegen fehlender HCI-Gegenstelle ab. Thread/UWB/NFC und weitere vom Cuttlefish-Produkt geerbte Geräte müssen zur tatsächlich vorhandenen QEMU-Hardware passen. |
| AEGIS-Identität/CLI | Noch zu implementieren; AOSP bleibt alleinige Passwort-, Benutzer- und Schlüsselautorität. |
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

Für die beauftragte gemeinsame GNU/Linux-Runtime ist deshalb ein gezielter
Kernel-Build einschließlich passender Module erforderlich. User-, PID-, Mount-
und IPC-Isolation müssen anschließend tatsächlich funktionieren. Ein chroot
allein oder eine zusätzliche Linux-VM erfüllt den Auftrag nicht. Der genaue
Kernel-Quellstand, Konfiguration, Module und Android-Integration sind vor dem
Build abzugleichen; das bloße Hinzufügen von Konfigurationszeilen reicht nicht.

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
