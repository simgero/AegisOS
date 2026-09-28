# Persistente lokale QEMU-Profile

Stand 28. September 2026: **Passwortgeschützte Daten haben einen vollständigen
lokalen Neustart überstanden.** Der neue Helper wurde auf `aegis-build`
kompiliert und als [secure-env-20260928T134804Z-024354c1](https://github.com/simgero/AegisOS/releases/tag/secure-env-20260928T134804Z-024354c1)
mit zurückgelesenem Archiv veröffentlicht. Alle drei heruntergeladenen Assets
wurden zusätzlich auf dem Mac gegen GitHubs SHA-256-Digests geprüft; Helper-Commit,
ARM64-Init und Persistenzprotokoll stimmen. Der zuvor verwendete Helper wird
für persistente Profile absichtlich zurückgewiesen.

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

## Tatsächlicher Neustarttest

Die lokalen Läufe `out/qemu-first-boot/persistent-20260928-1` und `-2`
verwenden das gleiche Profil `out/qemu-profiles/persistence-20260928`, UUID
`6e801519-963c-484b-96cd-2b0133d75846`. Beide erreichen `sys.boot_completed=1`,
SELinux `Enforcing` und dateibasierte Verschlüsselung (`file`, `encrypted`).
ADB authentifiziert weiterhin denselben Mac-Schlüssel. Der alte flüchtige
Gast `mouse-1` wurde dabei nicht verändert.

Der persönliche AOSP-Testbenutzer `aegis-persistence-test`, ID und Seriennummer
10, erhielt ein zufälliges Passwort über AOSPs Einstellungsdialog und virtuelle
Tastatureingaben. Es wurde nur im Host-Testprozess gehalten, nicht als
Befehlsargument übergeben. Der Credential-Typ lautet `PASSWORD`.

| Prüfung | Beobachtung |
| --- | --- |
| Erster Start | 4096 zufällige Bytes unter `/data/misc_ce/10/aegis-persistence/probe.bin` geschrieben und bytegenau gelesen. |
| Vollständiges Beenden | Android protokolliert das Aushängen von Data und Metadata und `Power down`; der Helper hängt seinen Speicher aus und meldet `AEGIS_HELPER_SHUTDOWN_CLEAN`. Beide QEMU-Prozesse enden. |
| Neuer Start desselben Profils | Geänderte Kernel-Boot-ID; Benutzer und Passwort bleiben erhalten. Der Helper findet seinen bisherigen Zustand, statt einen neuen anzulegen. |
| Vor Anmeldung | Benutzer 10 gestoppt beziehungsweise nach Wechsel `RUNNING_LOCKED`; CE-Liste `[0]`. Selbst Gast-root kann den bisherigen Dateipfad nicht lesen: Exitcode 1, `ENOENT`. |
| Falsches Passwort | AOSP meldet „Wrong password. Try again.“; Benutzer und CE bleiben gesperrt, Datei weiterhin unzugänglich. |
| Richtiges Passwort | `RUNNING_UNLOCKED`, CE-Liste `[0, 10]`; dieselben 4096 Bytes vollständig identisch. |
| Doppelstart | Ein zweiter Launcher lehnt das aktive Profil wegen seiner exklusiven Sperre ab. |
| Fehlende Helper-Disk | Auf einer Profilkopie vor VM-Start zurückgewiesen; keine Neuerzeugung. |
| Fremde Helper-UUID | Auf der Kopie vor VM-Start zurückgewiesen. |
| Gelöschtes `NVChip` | Auf der Kopie erkennt der echte Helper den fehlenden TPM-Zustand und verweigert Schlüsselerzeugung. Android wird nicht gestartet. |
| Original nach Negativtests | Manifest und beide Disks haben vor und nach den Tests identische SHA-256-Werte. |

SHA-256 der nach dem Neustart bytegenau geprüften Datei:
`a1fa4d25691d1b892d7644e8e68776699e8a3caf60eecc38786b30b4db856066`.
Die Boot-IDs ändern sich von `12cb93f8-b0fa-4c1c-a1e9-f5d45593e98f` auf
`2708d0ca-9d45-49b9-a1d4-d38e87632899`. Lokale Nachweise liegen unter
`out/persistence-test/`, insbesondere `evidence.json`, `negative-evidence.json`
und den Zustandsaufnahmen vor/nach dem Neustart. Beide vollständigen
Testpasswörter fehlen zuletzt in elf untersuchten Android-, Helper- und
Logcat-Protokollen; das beweist keine Abwesenheit aus
allen denkbaren Speicher- oder Protokollkopien.

Ein dritter Start des Originalprofils erreicht ebenfalls den Bootabschluss.
Nach erneuter Prüfung von Name, ID und Seriennummer wurde ausschließlich das
temporäre Testkonto über AOSP entfernt; danach sind nur Benutzer 0 und dessen
CE-Speicher vorhanden. Android und Helper wurden erneut geordnet beendet und
die Credential-Puffer des Host-Testprozesses überschrieben. Die absichtlich
beschädigte Profilkopie bleibt getrennt als Diagnoseartefakt erhalten.

Diese Prüfung bestätigt einen geordneten Neustart mit einem persönlichen
Benutzer. Stromausfall/erzwungener Abbruch, Migration auf neue Systemimages,
zwei Benutzer samt AEGIS-CLI und Runtime-Abmeldung bleiben offen. Die
Plattformdialoge ersetzen keinen Test des noch nicht integrierten AEGIS-Diensts.
