# Kernel für die gemeinsame GNU/Linux-Runtime

Stand 28. September 2026: **Erster Kernel- und Treiber-Build beendet, anschließend
von Androids Kompatibilitätsprüfung abgewiesen.** Kein neuer Kernel ist in QEMU
abgenommen. Der Android-Lauf `aosp-20260928T155300Z-96f9ed6b-c975dba9`
brach um 16:57 UTC bei `check_vintf_all` ab: FCM 202504 verlangt ausdrücklich
`CONFIG_SYSVIPC=n`; der erste Kernel enthielt `y`.

Das korrigierte Fragment lässt System-V-IPC aus und behält `POSIX_MQUEUE=y`
sowie `IPC_NS=y`. Im gepinnten `common/init/Kconfig` lautet die Abhängigkeit
`SYSVIPC || POSIX_MQUEUE`; persönliche IPC-Namespaces bleiben also erforderlich.
Kernelübernahme und lokale Releaseprüfung weisen aktiviertes oder unbekanntes
SYSVIPC jetzt schon vor einem Image-Build beziehungsweise Boot ab.
VINTF-, SELinux- und Bootprüfungen werden nicht abgeschaltet oder abgeschwächt.
Programme, die zwingend System-V-IPC benötigen, gehören damit nicht zum
unterstützten Runtime-Umfang; eine Emulation ist nicht implementiert.
Kernel und passende Treiber werden mit diesem Fragment erneut gebaut:
Commit `64e66d772f22465682dd2c41fdb486774cc15a31`, Start 17:06:24 UTC,
Dienst `aegis-kernel.service`, InvocationID `f2437d0948874ea88de7fe2288bc9a74`,
Lauf `kernel-20260928T170624Z-64e66d77-qW9h9L`. Der Start und der Eintritt in
Kleafs Kernelkonfiguration sind bestätigt; Abschluss und neue Images stehen aus.
14 Hosttests zur Kernelübernahme sowie zehn Tests zur lokalen Imagevorbereitung
bestehen, einschließlich aktivierter beziehungsweise fehlender SYSVIPC-Konfiguration.

Die folgenden Angaben betreffen den ersten, inzwischen abgewiesenen Kernel:

Start 14:54:46 UTC aus GitHub-Commit
`87ab3f54e52a3e312500011ab9f65278ac72ac0d`, Dienst `aegis-kernel.service`,
InvocationID `330aeafe17884e91ad9d7ea87ebe5af3`, Lauf
`kernel-20260928T145446Z-87ab3f54-wgP3Hz`.
Der Lauf meldet `BUILT_UNVERIFIED`; beide Kleaf-Befehle und die abschließende
Prüfung aller 40 Quellprojekte sind erfolgreich. GKI- und Virtual-Device-`Image`
sind bytegleich (SHA-256
`bfb463982b48178d01f931a56e9990e33417aabde2d8c4928283ca5f43cf56d8`).
Die Ausgabe-Konfiguration bestätigt User-/PID-/IPC-Namespaces, System-V-IPC,
Tmpfs-Xattrs, Speichercontroller, F2FS-Xattrs/-Security und SELinux.
Die vollständige Eingabeprüfung mit Commit `96f9ed6b` besteht ebenfalls:
105 GKI- und 51 Vendor-Module, 162 inventarisierte Dateien, Bundle
`a0526a0805ced49bee7c723c62b9d5ae297c4a1a15e90d453374c7f3775363dd`.
Prüfbericht auf dem Builder:
`/srv/aegis/work/kernel-inspect-96f9ed6-RNWE3m/kernel-inspection.json`.
Status `CHECKED_INPUTS_NOT_BOOTED`; Module wurden gelesen, nicht geladen.
Version und gemeinsames Vermagic beginnen mit
`6.12.18-android16-1-maybe-dirty-4k`. `-maybe-dirty` ist der feste Wert des
gepinnten Kleaf-`stamp.bzl` bei ausgeschaltetem Stamping; die separate
40-Projekte-Prüfung bestätigte unveränderte Checkouts und die erwarteten Pins.
Die Überwachung dieses abgeschlossenen Kernel-Laufs wurde beendet.

Kernel-Journal vom Mac lesen:

```sh
ssh -o BatchMode=yes -o StrictHostKeyChecking=yes aegis-build \
  'journalctl _SYSTEMD_INVOCATION_ID=330aeafe17884e91ad9d7ea87ebe5af3 --no-pager'
```

Quellen und Ergebnisse des abgeschlossenen Laufs bleiben erhalten.

## Festgelegte Grundlage

Der laufende Entwicklungsgast meldet
`6.12.18-android16-1-g50eb8d5d443b-ab13257114-4k`.
Das [offizielle Manifest dieses Builds](https://ci.android.com/builds/submitted/13257114/kernel_aarch64/latest/manifest_13257114.xml)
legt alle 40 Quellprojekte einschließlich Toolchains und Treibern auf Commits fest.
Die Herkunft stimmt mit dem
[Prebuilt-Eintrag unseres AOSP-Stands](https://android.googlesource.com/kernel/prebuilts/6.12/arm64/+/0af99653adede9524c7dbb36ba901d22e6266404/prebuilt-info.txt)
überein.

- Kernel: `50eb8d5d443b43f38d6e72f005f1b8601ac88a05`
- Virtual-Device-Module: `b972a07579955c4a6757e46899040922f60ef06c`
- Kleaf: `a02488f06e024e940862a398ad7c38ea1279014e`
- SHA-256 des unveränderten heruntergeladenen Manifests:
  `23a76af3c306287c5b6cc0ad9e4b9d64bcc1946888cfa703628945d6dea70bd4`

`manifest.xml` übernimmt sämtliche Projekt-Pins und Linkfiles daraus. Nur der
bewegliche Superproject-Verweis und die nicht benötigte Default-Revision sind
entfernt; die XML-Formatierung ist normalisiert. Das ist die historische
Entwicklungsbasis, kein aktueller Sicherheitsstand.

## Konfiguration und Build

`aegis_runtime_defconfig` ergänzt User-/PID-/IPC-Namespaces, POSIX-Nachrichtenqueues und
Tmpfs-Dateiattribute. Der erste native QEMU-Testlauf bestätigte beim alten
Kernel `CONFIG_TMPFS_XATTR=n`; drei CE-Negativtests konnten dadurch ihre
Seriennummern-Fixtures nicht anlegen. Diese Tests bleiben bis zur Wiederholung
mit dem neuen Kernel fehlgeschlagen.
Die Optionen für SELinux, Signaturen und andere Android-Sicherheitsmechanismen
werden nicht abgeschaltet. `CONFIG_VIRTIO_NET` wird nicht erzwungen: Der
Virtual-Device-Build erzeugt bereits das im bisherigen Gast geladene Modul.

Die gepinnte [Kleaf-Dokumentation](https://android.googlesource.com/kernel/build/+/a02488f06e024e940862a398ad7c38ea1279014e/kleaf/docs/kernel_config.md#defconfig_fragment-flag)
unterstützt ein zusätzliches Fragment über `--defconfig_fragment`. Das Rezept
wendet dasselbe Fragment auf den GKI-Build **und** den Build aller passenden
Virtual-Device-Module an. Die vorhandenen Build-/ABI-Prüfungen bleiben aktiv.
Ein Fehler ist zu untersuchen; er berechtigt nicht dazu, alte Module mit dem
geänderten Kernel zu mischen.

Nach Commit und GitHub-Sync führt der unprivilegierte Build-Account auf
`aegis-build` aus dem exakt passenden Projektcheckout aus:

```sh
bash scripts/kernel/build.sh VOLLSTAENDIGER_PROJEKT_COMMIT
```

Voraussetzungen sind das vorhandene gepinnte Repo-Werkzeug und Schreibrechte
für `/srv/aegis/work` und `/srv/aegis/runs`. Die Quellen liegen separat unter
`/srv/aegis/work/kernel`; die bestehende AOSP-Quelle wird nicht verändert.
Der gemeinsame Build-Lock verhindert parallele AOSP-/Helper-/Kernel-Builds.
Der Prozess muss für den langen Lauf auf dem Server verwaltet gestartet werden;
das Rezept selbst richtet keinen Dienst ein.

Vor und nach dem Compilerlauf prüft das Rezept alle 40 tatsächlichen Git-Checkouts auf
den erwarteten Commit und lokale Änderungen. Es speichert Quellmanifest,
Projektcommit, Fragment, Sync- und Buildlogs. Ergebnisse landen getrennt in
`gki/` und `virtual-device/` eines neuen Laufverzeichnisses.
`BUILT_UNVERIFIED` bedeutet ausschließlich, dass beide Buildbefehle erfolgreich
waren. Das Rezept lädt nichts hoch und installiert oder startet keine Images.

## Übergabe an den vollständigen Android-Build

Der Bootstrap unterstützt die optionale Umgebungsvariable
`AEGIS_KERNEL_RUN=/srv/aegis/runs/kernel-KONKRETER_LAUF`. Sie muss in der Umgebung
des privilegierten Bootstrap-Prozesses gesetzt sein; dessen systemd-Dienst
übernimmt sie ausdrücklich. Ein normaler Build ohne diese Auswahl verwendet
weiter die gepinnten AOSP-Prebuilts und übernimmt keine frühere Auswahl stillschweigend.
Der Pfad darf nur einen konkreten Lauf direkt unter `/srv/aegis/runs` bezeichnen.
Die Quellskripte müssen zuvor über GitHub auf dem gewünschten Commit angekommen sein.

`scripts/kernel/integrate.py` läuft im vorhandenen AOSP-Build-Lock vor dem
Compiler. Es prüft unter anderem:

- abgeschlossenen Kernel-Lauf, Projektcommit, Quellmanifest und unverändertes
  Konfigurationsfragment; aufgelöste und gepinnte Quellrevisionen müssen übereinstimmen;
- identische ARM64-`Image`-Dateien aus GKI- und Virtual-Device-Ausgabe;
- die komprimierte Konfiguration **innerhalb der ausführbaren Kernel-Datei**,
  Übereinstimmung mit `kernel_aarch64_dot_config`, Namespace-Funktionen sowie wesentliche Android-
  Voraussetzungen wie SELinux, Seccomp, Modulsignaturunterstützung, Dateiverschlüsselung
  und dm-verity, außerdem die bereits im bisherigen GKI vorhandenen Cgroup-/
  Speichercontroller sowie F2FS einschließlich Xattrs und SELinux-Dateiattributen
  für Androids tatsächliche Datenpartition;
- echte ELF-Metadaten aller übernommenen ARM64-Module, passende Kernelversion und
  identisches `vermagic`; doppelt gelieferte GKI-Module müssen bytegleich sein;
- die festgelegten frühen Boot-Treiber, Dateigrößen und reguläre Dateien ohne Symlinks.

Geprüfte Dateien werden unter
`device/aegis/runtime-kernels/INHALTSHASH/` im Server-AOSP-Checkout abgelegt.
Unvollständige Kopien liegen außerhalb der Produktsuche unter `out/`. Veränderte
vorhandene Eingaben werden erhalten und abgewiesen. Die Produktregistrierung
ergänzt eine erzeugte Make-Datei, die **vor** der geerbten Board-Konfiguration
`TARGET_KERNEL_PATH`, `SYSTEM_DLKM_SRC` und `KERNEL_MODULES_PATH` zusammen festlegt.
Nach `lunch` wird kontrolliert, dass AOSP tatsächlich alle drei Pfade übernommen hat.
Die normalen verwalteten Produktquellen und deren Vorgänger bleiben nachvollziehbar.

Anschließend erzeugt der bestehende vollständige AOSP-Build seine Boot-, Vendor-
Boot-, DLKM-, Super- und VBMeta-Images mit den normalen Sicherheitsregeln. Der
ausgelieferte Kernel und die Kernel-Nutzdaten in `boot.img` werden nochmals über
ihre SHA-256-Werte zugeordnet; ein stehen gebliebenes altes Boot-Image wird abgewiesen.
`kernel-inputs.json` hält Quellrevisionen, Kernelversion, Modul- und Eingabehashes
fest. Der Worker nimmt diesen Nachweis in Prüfsummen, GitHub-Upload und erneuten
Bytevergleich auf. Bei ausdrücklich ausgewähltem Kernel verhindert ein fehlender
Nachweis die Erfolgsmeldung. Binärdateien erreichen den Mac weiterhin über GitHub.

`CHECKED_INPUTS_NOT_BOOTED` besagt nur, dass diese Eingabeprüfungen bestanden sind.
Gleiche Versionsstrings sind kein vollständiger ABI- oder Signaturnachweis.
Insbesondere müssen die tatsächlich erzeugten Images, geladene Module und deren
Fehlerprotokolle noch geprüft werden. Es werden keine Kernel-Signaturprüfungen,
AVB-Regeln oder SELinux-Regeln abgeschaltet und keine alten Images einzeln gepatcht.

Die Hosttests verwenden nicht ausführbare ELF-/Image-Metadatenfixtures. Sie prüfen
defekte und vermischte Eingaben, Konfigurationsabweichungen, abgebrochene Kopien,
Erhalt alter Quellen sowie Auswahl und Ergebniszuordnung. Linux-CI prüft zusätzlich
den Transport des Kernel-Nachweises und den Abbruch bei fehlendem Nachweis.
Als reale Formatprüfung wurden das vorhandene Kernel-Image und 19 Module seines
Vendor-Ramdisks gelesen, ohne sie zu verändern, zu laden oder auszuführen. Dabei
bestätigte die Prüfroutine die fehlenden Namespace-Optionen des bisherigen Kernels.
**Der neue Kernel ist inzwischen gebaut, aber noch nicht in Android integriert
oder lokal gestartet.** Der erste reale Kleaf-Lauf liefert die Metadaten als
`kernel_aarch64_dot_config` und `kernel_aarch64_Module.symvers`; die Übernahme
verwendet diese festen Namen und vergleicht die Konfiguration weiterhin mit
dem tatsächlich ausführbaren Kernel.

## Noch notwendige Integration und Abnahme

1. Das Rezept auf dem erreichbaren Builder ausführen. Im Ergebnis die tatsächlich
   wirksame Konfiguration sowie Herkunft und Versionsdaten aller Module prüfen.
2. Den fertigen Lauf ausdrücklich über `AEGIS_KERNEL_RUN` auswählen und die
   vorbereitete Übernahme erstmals mit echten neuen Ausgaben ausführen.
   Boot-, Vendor-Boot-, DLKM- und VBMeta-Images konsistent neu erzeugen und deren
   tatsächlichen Inhalt kontrollieren. Der Quelltext der Übergabe ersetzt diesen Lauf nicht.
3. Geprüfte Images und Prüfsummen über GitHub veröffentlichen und auf dem Mac
   beziehen. Das bisherige QEMU-Profil behalten; eine Profilmigration ist noch
   nicht implementiert.
4. In lokalem QEMU Boot, Modulladen, Grafik, Eingaben, ADB, SELinux und
   Verschlüsselung prüfen. User-/PID-/Mount-/IPC-Isolation praktisch ausführen,
   einschließlich UID-Mappings und negativer Zugriffsversuche.

Das Fragment allein ist kein Isolationsnachweis. Laufzeitverwaltung,
SELinux-Regeln und AOSP-vermittelte Benutzerautorisierung fehlen weiterhin.
