# Persistente lokale QEMU-Profile

Stand 28. September 2026: Implementierung vorbereitet. Die Hosttests verwenden
echte qcow2- und ext4-Dateien. Der neue Helper wurde auf `aegis-build`
kompiliert und als [secure-env-20260928T134804Z-024354c1](https://github.com/simgero/AegisOS/releases/tag/secure-env-20260928T134804Z-024354c1)
mit zurückgelesenem Archiv veröffentlicht. Downloadprüfung und QEMU-Test stehen aus.
Ein erfolgreicher Android-Neustart mit erhaltenen Schlüsseln ist **noch nicht
nachgewiesen**. Der zuvor verwendete Helper wird für persistente Profile
absichtlich zurückgewiesen.

## Zusammengehöriger Zustand

Ein Profil enthält `android.qcow2`, `secure-env.ext4` und `profile.json`.
Android verwendet das unveränderte lokale Basisimage als qcow2-Backing.
Alle veränderlichen Android-Partitionen einschließlich Userdata, Metadata und FRP
bleiben im selben Overlay. Das gesamte Arbeitsverzeichnis des TPM-Helfers liegt
auf seiner eigenen ext4-Disk.
Diese wird synchron eingehängt, damit bestätigte TPM-Dateischreibvorgänge nicht
allein im flüchtigen Linux-Seitencache verbleiben. Ein praktischer Stromausfall-
oder Absturztest steht ebenfalls noch aus.

Eine gemeinsame UUID steht im Manifest, im ext4-Superblock, innerhalb des
Helferspeichers und als interner Identifikations-Snapshot im qcow2-Overlay.
Dieser Snapshot ist kein Backup und darf nicht einzeln zurückgespielt werden.
Kernel, Ramdisks, Bootkonfiguration, Helper-Archiv und Basisdisk werden per
SHA-256 gebunden. Änderungen erfordern eine ausdrückliche Migration, die dieser
erste Launcher noch nicht implementiert. Beide VMs halten gemeinsam eine
exklusive Profilsperre.

Der Helper prüft die UUID vor dem Start von `secure_env`. Ein einmal verbrauchter
Initialisierungsmarker erlaubt bei später fehlendem oder falsch großem `NVChip`
keine neue Schlüsselerzeugung. Fehler werden nicht durch Formatieren oder
flüchtigen Ersatzspeicher übergangen. Das Original-TPM schreibt seinen Zustand
in `NVChip`; siehe [AOSP NVMem.c](https://android.googlesource.com/platform/external/ms-tpm-20-ref/+/refs/tags/android-16.0.0_r1/TPMCmd/Platform/src/NVMem.c).

Das ist weiterhin ein softwarebasierter Entwicklungs-TPM. Die Profildateien sind
vor anderen lokalen Accounts durch Dateirechte geschützt; der Mac-Eigentümer
kann sie lesen oder verändern. Eine Hardware-Vertrauensbasis entsteht dadurch
nicht. Getrennte Kopien oder Rücksetzungen der beiden Disks sind unzulässig.

## Verwendung nach geprüftem Helper-Build

Der neue Helper muss `etc/aegis-helper-protocol` mit `persistent-state-v1`
enthalten. Die erste explizite Anlage benötigt `qemu-img` und `mkfs.ext4`:

```sh
python3 scripts/qemu-with-secure-env.py \
  out/qemu-first-boot/images downloads/NEW_VERIFIED_HELPER_RELEASE \
  out/qemu-frp/android.raw out/qemu-first-boot/runtime.bootconfig \
  out/qemu-first-boot/persistent-first \
  --profile out/qemu-profiles/development --create-profile \
  --seconds 0 --display cocoa --adb-port 15555
```

Beim nächsten Start `--create-profile` weglassen und ein neues Logverzeichnis
angeben. Fehlende Profildateien werden niemals automatisch neu angelegt.
Ohne `--profile` bleibt der diagnostische Start vollständig flüchtig.

Bei regulärem Beenden fordert der Launcher zuerst Androids Herunterfahren an.
Danach stoppt der Helper sein Kind, synchronisiert den Speicher, hängt ext4 aus
und fährt herunter. Ein erzwungenes Schließen des Fensters kann Android abrupt
beenden. Nicht bestätigte Abschlüsse werden als solche protokolliert; die
Dateien bleiben zur Diagnose erhalten. `helper-shutdown.txt` allein beweist
weder einen sauberen Android-Abschluss noch erfolgreiche Wiederentschlüsselung.

## Noch erforderlicher Nachweis

1. Mit dem auf dem Server gebauten Helper Android bis `sys.boot_completed=1`
   starten; SELinux und Verschlüsselung prüfen.
2. Testdateien und einen persönlichen AOSP-Benutzer mit Passwort anlegen.
3. Android und Helper geordnet beenden. Dieselben Profildateien erneut öffnen.
4. Falsches Passwort zurückweisen, mit richtigem Passwort entsperren und
   Testdateien bytegenau prüfen. Dass Systembenutzer 0 startet, reicht nicht.
5. Verlorenen/fremden Helferspeicher sowie Doppelstarts zurückweisen und
   kontrollierte Fehlerfälle ohne automatische Datenlöschung prüfen.

Erst danach ist der Persistenzteil von Phase 1 bestanden.
