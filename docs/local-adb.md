# Lokales ADB für das QEMU-Entwicklungsimage

Der diagnostische QEMU-Start richtet keine Gast-Netzwerkkarte ein. Ein
zusätzlicher VirtIO-Serial-Port (`hvc17`) verbindet den lokalen QEMU-Port mit
dem ADB-Dienst innerhalb des Gastes. Der Host-Port bindet ausschließlich an
`127.0.0.1`. Es gibt keine Gast-Netzwerkkarte und keine Hostverzeichnisfreigabe.

`scripts/run-local-qemu.sh` öffnet das bestehende persistente Profil
`foundation-bfe90925` auf Port **15755**. Bei direktem Aufruf des Python-Launchers
`--adb-port 15755` ergänzen; parallele Testgäste benötigen einen anderen Port
und ein eigenes Profil. Danach, sobald Android gestartet ist, das ausgegebene
neue Laufverzeichnis verwenden:

```sh
python3 scripts/connect-local-adb.py out/full-build-bfe90925/RUN
```

Den angezeigten Mac-Schlüssel im Gast freigeben. Für einen ausdrücklich lokal
provisionierten Entwicklungsgast kann stattdessen
`--authorize-this-mac` angegeben werden. Das hinterlegt ausschließlich den
öffentlichen Schlüssel aus `~/.android/adbkey.pub` und zeigt dessen Fingerprint.
Die private Schlüsseldatei wird nicht gelesen oder transportiert.

Die Einrichtung verlangt `ro.adb.secure=1`. Im bisherigen Image fehlt die
Eigenschaft; dort wird sie über die autorisierte userdebug-Konsole einmalig
gesetzt. Ein expliziter Wert `0` führt zum Fehler. Das Produkt setzt sie für
künftige Systembuilds fest auf `1`.

Die Bridge hält den seriellen Deskriptor offen, bevor sie den Raw-Modus setzt.
Andernfalls setzt der Treiber beim erneuten Öffnen seine Terminalkonfiguration
zurück und beschädigt binäre ADB-Pakete. Ports aus dem üblichen Emulatorbereich
um 5555 vermeiden: Die automatische ADB-Suche kann sonst den Kanal belegen.

```sh
adb -s 127.0.0.1:15755 shell
adb -s 127.0.0.1:15755 exec-out screencap -p > out/screen.png
python3 scripts/qemu_control.py out/full-build-bfe90925/RUN query-status
```

Im Lauf `out/qemu-first-boot/adb-1` wurde am 28. September geprüft:

- Ohne freigegebenen Schlüssel: ADB `unauthorized`.
- Nach Hinterlegen des abgeglichenen Mac-Schlüssels: Verbindung, Shell,
  `ro.adb.secure=1`, SELinux `Enforcing`, `sys.boot_completed=1`.
- Binäre Übertragung von 262144 Bytes mit allen Bytewerten in beide Richtungen,
  bytegenau identisch; Bildschirmaufnahme als PNG.
- Display 720 × 1280; `wm` und `input` funktionieren über ADB.
- Start der Android-Einstellungen und bestätigter Fensterfokus.
- QEMU-Tastatur liefert KEY_TAB Down/Up; Tablet liefert Position sowie
  BTN_MOUSE Down/Up im Gast. Die vollständige interaktive Bedienbarkeit ist
  noch nicht abgenommen: Der ADB-Dialog reagierte auf QMP-Klicks nicht zuverlässig.

Der Gast wurde anschließend regulär heruntergefahren. Dieser Lauf verwendete
weiterhin flüchtigen Android- und TPM-Zustand.

Die neue Einrichtung wurde danach im frischen Lauf `adb-3` ohne manuelle
Gastbefehle über `connect-local-adb.py --authorize-this-mac` wiederholt.
Nach einer frischen Authentifizierungswiederholung waren ADB, Bootabschluss,
SELinux `Enforcing`, Display-Abfrage, Eingabe und PNG-Aufnahme bestätigt.

## Virtuelle Maus und Tastatur

Der Launcher verwendet jetzt standardmäßig `virtio-mouse-pci`. Android erkennt
dieses Gerät als Cursor mit den Bildschirmgrenzen 720 × 1280. Das bisherige
Tablet wurde als Touchpad mit Gestenübersetzung eingeordnet. Es bleibt mit
`--pointer tablet` für Diagnosezwecke verfügbar.

Im Lauf `out/qemu-first-boot/mouse-1` wurde geprüft:

- QMP-Tastaturereignisse schreiben `aegis` vollständig in das Suchfeld;
  `keyboard-ui.xml` bestätigt den Text im Android-EditText.
- Relative QMP-Mausbewegung und linke Maustaste öffnen aus der Startseite der
  Einstellungen die Seite „Apps“. `settings-before.xml`, `settings-after.xml`
  und `mouse-after.png` halten den tatsächlichen UI-Wechsel fest.
- Android meldet MOUSE-Ereignisse einschließlich DOWN, BUTTON_PRESS,
  BUTTON_RELEASE und UP. Mausbeschleunigung wirkt auf relative Bewegungen;
  deren Delta ist deshalb keine absolute Pixelposition.

Diese Tests laufen über QEMUs virtuelle Eingabegeräte. Die direkte Weitergabe
physischer Mac-Eingaben durch das Cocoa-Fenster ist noch separat zu prüfen:
Der Mac war beim versuchten Computer-Use-Test gesperrt. UI-Abfragen während
eines Seitenwechsels können vorübergehend keinen Root-Knoten liefern; vor
weiteren Eingaben den tatsächlichen Fokus und die fertig geladene Seite prüfen.
