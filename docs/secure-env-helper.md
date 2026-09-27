# Lokale Cuttlefish-Sicherheits-Hilfs-VM

Status: Implementierung vorbereitet und lokal statisch geprüft; der Lauf mit
den originalen Hostprogrammen ist bis zum administrativen Export blockiert.
Kein vollständiger Android-Bootnachweis.

Der originale Rust-KeyMint-Thread in AOSP `secure_env` ist unter `__linux__`
aktiviert. Der abgeschlossene Build enthält bereits
`out/host/linux_musl-arm64/bin/secure_env` und `libkmr_cf_ffi.so`.
Eine zusätzliche ARM64-QEMU-VM führt diese Programme auf dem Mac aus. Android
und Hilfs-VM verwenden HVF; kein OS-Test läuft auf dem Buildserver.

## Datenfluss

| Android VirtIO-Konsole | Dienst | Hilfs-VM-Konsole |
| --- | --- | --- |
| hvc3 | C++ Keymaster | hvc2 |
| hvc4 | Gatekeeper | hvc1 |
| hvc10 | OEMLock | hvc3 |
| hvc11 | Rust KeyMint | hvc0 |

QEMU verbindet diese Kanäle über lokale Unix-Sockets. `secure_env` benutzt
den originalen In-Process-TPM und die TPM-Implementierungen seiner Dienste.
Das bleibt eine Entwicklungs-Emulation und ist kein Hardware-TEE.
Die Hilfs-VM hat flüchtigen Zustand. Android verwendet eine Disk im
Snapshot-Modus; persistente Android-Nutzerdaten dürfen mit dieser flüchtigen
Schlüsselhaltung nicht kombiniert werden. Netzwerk und Hostverzeichnisfreigaben
sind in diesem Diagnoselauncher ausgeschaltet.

## Export auf aegis-build

`scripts/export-secure-env.sh --token-stdin FULL_COMMIT` läuft mit sudo.
Es lädt den gepinnten Helferquelltext über GitHub, kompiliert nur den kleinen
Init-Prozess als Benutzer `aegis-build`, sammelt die schon vorhandenen
AArch64-ELF-Abhängigkeiten und veröffentlicht einen eigenen `secure-env-*`-Release.
Der Downloadvergleich vor Veröffentlichung bestätigt den Transport. Ein neuer
Android-Vollbuild wird dabei nicht gestartet.

Das Token wird nur über stdin gelesen und nicht an Compiler/Packager vererbt.
Die bestehenden Android-Outputs werden ausschließlich gelesen. Der Export
verlangt einen freien Build-Lock und lehnt einen aktiven Builddienst ab.

## Lokaler Start nach dem Export

Den konkreten `secure-env-*`-Release mittels GitHub CLI in ein neues Verzeichnis
herunterladen. Der Launcher prüft `secure-env-arm64.tar.gz` gegen dessen
SHA256SUMS, bevor er die Hilfs-Ramdisk erstellt:

```sh
python3 scripts/qemu-with-secure-env.py \
  out/qemu-first-boot/images downloads/SECURE_ENV_RELEASE \
  out/qemu-first-boot/android.raw out/qemu-first-boot/runtime.bootconfig \
  out/qemu-first-boot/with-helper --seconds 180
```

`--display cocoa` öffnet optional die Android-Test-VM als Fenster. Eine
erfolgreiche Grafikinitialisierung ist noch nicht nachgewiesen.
Nach Ablauf der Diagnosezeit oder Ende einer VM werden beide QEMU-Prozesse
gezielt beendet. Logs und exakte Befehle bleiben im neuen Ausgabeverzeichnis.

Validiert: C-Init ohne Compilerwarnungen im Syntaxcheck, neuec-Archiv durch
unabhängigen cpio-Leser, Zurückweisung korrupter Helper-Archive, Symlinks und
Pfadtraversal. Ausstehend: echte AArch64-Kompilierung auf aegis-build,
Helper-Start, Protokollhandshake, Bootloader-Bootinformationen für KeyMint,
anschließend vollständiger Android-Boot und QEMU-Grafik.

Referenz: [AOSP secure_env](https://android.googlesource.com/device/google/cuttlefish/+/android-16.0.0_r1/host/commands/secure_env/README.md)
und [Rust-KeyMint-Start](https://android.googlesource.com/device/google/cuttlefish/+/android-16.0.0_r1/host/commands/secure_env/secure_env_not_windows_main.cpp).
