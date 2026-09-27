# Lokaler Bootversuch vom 28. September 2026

Release: `aosp-20260927T181835Z-38f4c95f-e6f263b2`.
Alle elf Release-Assets wurden durch `scripts/fetch-release.py` gegen die
SHA256SUMS geprüft. Das Archiv enthält 3.36 GB einzelne Dateien.

## Nachgewiesen

- ARM64-Kernel 6.12.18 läuft unter QEMU 11.1.1 mit HVF auf dem Mac.
- Vendor- und generische Ramdisk starten Android init und laden VirtIO-Module.
- Eine experimentelle GPT-Disk enthält Bootpartitionen, Super, Userdata,
  eine neue ext4-Metadatenpartition und Misc. Sparse Android-Images werden beim
  Zusammensetzen expandiert; die lokale Datei bleibt sparse.
- Android findet die logischen Partitionen und richtet dm-verity ein.
- SELinux-Policy wird geladen und die zweite init-Phase erreicht.
- Mit den Cuttlefish-APEX-Auswahlangaben startet apexd-bootstrap erfolgreich.
- KeyMint scheitert am fehlenden `/dev/hvc11`; vold wartet daraufhin auf
  `android.security.maintenance`. Vollständiger Boot, ADB und Oberfläche sind
  nicht erreicht. Auch Grafikparameter und Hostdienste fehlen noch.

Der direkte Kernelstart ist kein verifizierter Bootloader und kein Nachweis
einer sicheren Bootkette. Der übergebene VBMeta-Digest wurde mit AOSPs avbtool
aus den unveränderten Images berechnet. Die eingebetteten Signaturen, Hashes
und Hashtrees wurden mit `verify_image --follow_chain_partitions` geprüft;
es handelt sich weiterhin um Testschlüssel. AVB/SELinux wurden nicht abgeschaltet,
die nonsecure-KeyMint/Gatekeeper-Alternativen nicht ausgewählt.

## Lokal reproduzieren

Die heruntergeladenen Originale liegen unter `downloads/RELEASE_TAG`.
Entpackte Images und Versuchsdaten liegen unter `out/qemu-first-boot`.
Die GPT-Datei wurde mit dem Algorithmus in `scripts/make-qemu-disk.py` erstellt.
Die ext4-Metadatenpartition ist 16 MiB groß; erstellt mit dem separat installierten
Homebrew e2fsprogs 1.47.4. Misc ist eine leere 1-MiB-Datei.

```sh
# Nur bei noch nicht vorhandener android.raw:
python3 scripts/make-qemu-disk.py out/qemu-first-boot

# Das Zielverzeichnis muss neu sein. Die VM stoppt spätestens nach 60 Sekunden.
python3 scripts/qemu-init.py out/qemu-first-boot/images \
  out/qemu-first-boot/repeat --disk out/qemu-first-boot/android.raw \
  --bootconfig out/qemu-first-boot/runtime.bootconfig --seconds 60
```

Der Launcher protokolliert den exakten QEMU-Aufruf und die serielle Ausgabe.
Die Disk wird im Snapshot-Modus verwendet; keine Hostverzeichnisse und kein
Netzwerk werden freigegeben. Dies ist ein Headless-Diagnosestart.

Der letzte Test ist `out/qemu-first-boot/disk-runtime/serial.log`.
`runtime.bootconfig` ergänzt den Original-Vendor-Bootconfig um:

```text
androidboot.vbmeta.hash_alg=sha256
androidboot.vbmeta.size=22848
androidboot.vbmeta.digest=4c1fbc7fcac3d138d4e4b20d11915d2088a6e4255b99fe59bad59b49bd849395
androidboot.vendor.apex.com.android.hardware.keymint=com.android.hardware.keymint.rust_cf_remote
androidboot.vendor.apex.com.android.hardware.gatekeeper=com.android.hardware.gatekeeper.cf_remote
androidboot.vendor.apex.com.android.hardware.graphics.composer=com.android.hardware.graphics.composer.ranchu
```

Diese Werte gelten ausschließlich für den genannten Release. Der nächste
Integrationsschritt sind die tatsächlichen Host-Gegenstellen für die
Cuttlefish-Sicherheitsdienste einschließlich der VirtIO-Serial-Anbindung.
Ein leerer Serial-Port alleine implementiert das KeyMint-Protokoll nicht.

Referenzen: [AOSP QEMU-Manager](https://android.googlesource.com/device/google/cuttlefish/+/android-16.0.0_r1/host/libs/vm_manager/qemu_manager.cpp),
[Cuttlefish-Bootparameter](https://android.googlesource.com/device/google/cuttlefish/+/android-16.0.0_r1/host/commands/assemble_cvd/bootconfig_args.cpp),
[AVB-Werkzeug](https://android.googlesource.com/platform/external/avb/+/android-16.0.0_r1/avbtool.py).
