# Gemeinsame Softwaregeneration

Stand: 28. September 2026. **Echtes Basis-Image auf `aegis-build` erfolgreich
erzeugt, geprüft und im ausgelieferten Android-Image nachgewiesen; noch nicht
in QEMU eingebunden.**
`ro.aegis.runtime.mode=absent` bleibt unverändert.

Lauf `runtime-base-20260928T145204Z-87ab3f54-WclcLl`, Commit
`87ab3f54e52a3e312500011ab9f65278ac72ac0d`, Abschluss 14:53:38 UTC:
`BUILT_VERIFIED_NOT_MOUNTED`. Die zwei 256-MiB-Images sind bytegleich;
`e2fsck` sowie die vollständige Prüfung von Inhalt, Eigentümern, Modi,
Verzeichnisinventaren und Links bestanden. SHA-256 des Basis-Images:
`1332b0fbd28b2bdea0f5b59b7dc09b4ea825727100fdbc57f0edd9660928d7fa`.

Die vorigen Versuche wurden korrekt zurückgewiesen: Drei unterschiedliche
Bytes betrafen den Zugriffszeitstempel von `lost+found`; dessen Eigentümer
entsprach zudem dem Buildkonto. Das korrigierte Rezept überlässt dieses
Verzeichnis vollständig `mke2fs`, weil das gepinnte `e2fsdroid` es von seiner
nachträglichen Metadatenkorrektur ausnimmt. Die verworfenen Diagnoseimages
bleiben getrennt vom erfolgreichen Lauf erhalten.

## Inhalt und Kennungen

`scripts/runtime/generation.py` übernimmt den vollständig geprüften
[Debian-Import](README.md). Der tatsächliche Pin ergibt 78 installierte Pakete
und nach den Anpassungen 3.271 Einträge. Der Plan erfasst Dateityp, Modus,
numerische Eigentümer, Dateigrößen und SHA-256 sowie Linkziele; er enthält keine
persönlichen AOSP-Benutzer oder Benutzerdaten.

Der einzige zusätzliche NSS-Eintrag heißt `runtime`, verwendet UID/GID 1000
und HOME `/home/user`. Sein Shadow- und Group-Shadow-Eintrag ist gesperrt,
wie die bereits vorhandenen technischen Debian-Konten. Es gibt keine neue
Passwortprüfung. Die Identitätsdatenbanken verwenden weiterhin ausschließlich
lokale technische Dateien; fremde NSS-Anmeldedienste werden zurückgewiesen.
Ein Name oder eine vorhandene Gruppe auf GID 1000 verhindert eine unbemerkte
Überschreibung.

Die Software behält die ursprünglichen **Linux-Kennungen im Image**. Auf dem
Builder wird keine dieser Kennungen mit `chown` vergeben. Für die spätere
gemeinsame Nutzung braucht jeder Kontext eine passende Mount-ID-Zuordnung zu
seinem [AOSP-Kennungsbereich](uid-mapping.md). Die
[Kernel-Dokumentation zu ID-Mappings](https://docs.kernel.org/filesystems/idmappings.html)
beschreibt diese Trennung zwischen gespeicherten Eigentümern und der Sicht
eines Mounts/Prozesses. Sie muss mit unserem neuen Kernel tatsächlich geprüft
werden; ein globales Umbenennen aller Eigentümer pro Benutzer ist kein Ersatz.

Das Rezept entfernt Set-UID-/Set-GID-Bits, erhält normale Rechte einschließlich
Sticky-Bits und weist zusätzliche TAR-ACL-/Capability-Metadaten zurück. Die
späteren Grenzen `nosuid`, `no_new_privs`, Capabilities und SELinux bleiben
trotzdem erforderlich. Der gemeinsame `sudo`-Gruppenname aus Debian erteilt
keine AOSP-Adminberechtigung; ihm wird niemand hinzugefügt.

`home/user`, `run/user/1000` und weitere benötigte Mountpunkte sind leere
Platzhalter. Dateien unter HOME, `/tmp`, `/run`, `/dev`, `/proc` oder `/sys`
würden als eingebetteter persönlicher/flüchtiger Zustand abgewiesen. Vorhandene
leere Verzeichnisse sind zulässig. Der feste Hostname lautet `aegis`, Hosts
enthält nur localhost; Resolver und Machine-ID bleiben leer. Keine lokale
Mac-/Builder-Konfiguration wird importiert. Persönliches HOME, temporäre
Verzeichnisse, Geräte, Procfs und Netzwerk-/DNS-Konfiguration muss später der
autorisierte Broker bereitstellen.

## Ablauf ausschließlich auf aegis-build

Aus einem sauberen, über GitHub bezogenen Projektcheckout als unprivilegierter
Build-Account:

```sh
bash scripts/runtime/build-base.sh VOLLSTAENDIGER_PROJEKT_COMMIT
```

Das Rezept nutzt die gemeinsame Buildsperre und den festgelegten AOSP-Stand.
Der öffentliche Debian-Import wird unter `/srv/aegis/work/runtime-imports/`
geprüft wiederverwendet. Die ausgewählten AOSP-Quellen für das Dateisystem-
Werkzeug, seine Konfiguration und den Kennungsparser sind zusätzlich durch
[`filesystem-tools.json`](filesystem-tools.json) gebunden. Danach werden
`mkuserimg_mke2fs`, `mke2fs`, `e2fsdroid`, `e2fsck` und `debugfs` im AOSP-Build
kompiliert. Keine Debian-Binärdatei, kein Paketinstallationsskript und keine VM
werden auf dem Builder gestartet.

Eine neue private Staging-Fläche nimmt zuerst reguläre Dateien auf, danach
Links. Es gibt kein `extractall`, keine Auswertung von Archiv-Skripten und
keine übernommenen Host-Eigentümer oder ausführbaren Host-Dateirechte.
Dateiinhalte werden gegen den vorherigen Plan geprüft. Bestehende Ziele werden
abgewiesen und erhalten; bei einem Kopierfehler wird nur die eigens angelegte
Staging-Fläche entfernt. Die in diesem Pin verwendeten Symlinks passen in
ext4s kurze Linkdarstellung. Längere oder nicht darstellbare Links in künftigen
Basen erfordern eine Anpassung und Prüfung des Rezepts. `lost+found` steht im
Soll-Inventar, wird aber ausschließlich von `mke2fs` erzeugt: Eine Hostkopie
würde in diesem AOSP-Werkzeug ungeprüfte Host-Eigentümer und Zeitstempel
übernehmen. Eigentümer, Modus und leeres Inventar werden anschließend wie bei
allen anderen Verzeichnissen aus dem tatsächlichen Image gelesen.

Der vollständige `fs_config.txt` ordnet allen Einträgen die vorgesehenen
Eigentümer und Rechte zu, mit Dateicapabilities null. Der gepinnte
[AOSP-Parser](https://android.googlesource.com/platform/system/core/+/refs/tags/android-16.0.0_r1/libcutils/canned_fs_config.cpp)
akzeptiert für das Wurzelverzeichnis ausdrücklich `/`. Das
[AOSP-Imagewerkzeug](https://android.googlesource.com/platform/system/extras/+/refs/tags/android-16.0.0_r1/ext4_utils/mkuserimg_mke2fs.py)
übergibt diese Angaben an `e2fsdroid`.

Das Ergebnis ist ein rohes ext4-Image mit 256 MiB, 4-KiB-Blöcken und ohne Journal.
Zeitstempel, UUID und Directory-Hash-Seed werden festgelegt. Der Plan und die
Hashes der tatsächlich gebauten Werkzeuge bestimmen die UUID. Zwei vollständige
Erzeugungsläufe müssen bytegleich sein; dies belegt Reproduzierbarkeit in diesem
Buildlauf, noch keine Gleichheit auf anderen Buildhosts oder Toolchains.

Danach liest das Rezept das Image ohne Mount: Superblock/UUID, `e2fsck`, alle
Dateiinhalte, Eigentümer, Modi, Typen, Directory-Inventare, Symlink-Ziele und
gemeinsame Inodes von Hardlinks werden geprüft. Fehlerausgaben von `debugfs`
werden nicht allein anhand dessen Exitcodes als Erfolg interpretiert.
Geänderte Quellen, Werkzeuge oder Importdateien verhindern die Freigabe.

## Ergebnis und Grenzen

Ein abgeschlossener Lauf unter `/srv/aegis/runs/runtime-base-*` enthält:

- `artifacts/runtime-base.ext4`: noch nicht gemountete gemeinsame Software;
- `artifacts/plan.json` und `fs_config.txt`: vollständige geplante Eingaben;
- `artifacts/generation.json`: tatsächliche Image-Prüfsumme als Generation-ID,
  Werkzeughashes und Ergebnis des Vergleichs beider Erzeugungsläufe;
- Projektcommit, Buildlog, Status und `SHA256SUMS` im Laufverzeichnis.

`BUILT_VERIFIED_NOT_MOUNTED` bestätigt nur Dateisystem-/Inhaltsprüfungen und
den wiederholten Build. Es bedeutet weder Upload noch Start, Anmeldung,
Schreibschutz im Gast oder nachgewiesene Benutzerisolation. Ein einfacher
Dateihash ist keine zusätzliche Signatur oder eigenständige Vertrauensbasis.
Die folgende Auswahl bindet Basisdateien in einen ausdrücklich gewählten
AOSP-Build ein. Der Vollbuild `aosp-20260928T181511Z-336e9275-4af8ec7d` hat diese Auswahl
erfolgreich verwendet. Die autorisierte
Mount-Anbindung und Aktivierung fehlen weiterhin; ein existierendes QEMU-Profil
wird nicht verändert.

## Auswahl für einen vollständigen Systembuild

Der Bootstrap akzeptiert zusätzlich
`AEGIS_RUNTIME_RUN=/srv/aegis/runs/runtime-base-RUN`: `RUN` ist durch den konkreten
abgeschlossenen Lauf zu ersetzen. Es gibt keine automatische Auswahl des
neuesten Verzeichnisses. Quellen kommen über GitHub; die bereits auf demselben
Builder erzeugten Dateien bleiben beim Übergang zum Systembuild auf dem Server.

`scripts/runtime/integrate.py prepare` verlangt den erfolgreichen Basisstatus
und einen vollständigen Projektcommit. Es rekonstruiert den Plan aus dem
gepinnten Originalimport und dem aktuellen Rezept, prüft Besitzerkonfiguration,
Image-Hash, UUID und Werkzeughashes und liest erneut alle geplanten Inodes und
Dateiinhalte mit dem geprüften AOSP-`debugfs`. Ein selbstkonsistenter
Prüfsummenbericht genügt nicht. Geänderte Eingaben oder Werkzeuge verhindern
die Auswahl. Die Prüfung startet keine Debian-Programme und mountet nichts.

Die vier Eingabedateien werden unter `device/aegis/runtime-bases/` in einem
inhaltlich bestimmten Verzeichnis abgelegt. Eine generierte Produktauswahl
nimmt `base.ext4` und `generation.json` unter
`$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/aegis/runtime/` in `PRODUCT_COPY_FILES` auf.
Eine gleichzeitig über `AEGIS_KERNEL_RUN` gewählte Kernelkonfiguration bleibt
erhalten und muss ihrem konkreten Nachweis entsprechen. Fremde oder veränderte
Produktdateien werden nicht überschrieben; vorherige verwaltete Fassungen
bleiben in den vorhandenen Backups erhalten.

Nach dem AOSP-Build müssen die tatsächlichen Bytes im Produkt-Staging der
ausgewählten Generation entsprechen. Ein späterer Build ohne Basiswahl bricht
ab, falls dort alte Runtime-Dateien liegen bleiben. Die normale Registrierung
entfernt die Produktauswahl, erhält aber vorherige Basisartefakte.

Der Release enthält zusätzlich `runtime-base-inputs.json`,
`runtime-base-plan.json`, `runtime-base-generation.json` und
`runtime-base-fs_config.txt`. Sie werden wie die Image-Archive mit Prüfsummen
hochgeladen, erneut heruntergeladen und byteweise verglichen. Fehlende oder
beim Rücklesen veränderte Nachweise verhindern `UPLOAD_VERIFIED`.

**Der echte Image-Nachweis besteht:** Der vollständige Android-Build aus
`336e9275` bestätigte um 18:27:09 UTC exakt diese Basisdateien in
`system_ext_a` innerhalb von `super.img` (`PACKAGED_BASE_BYTES_VERIFIED_NOT_BOOTED`).
Der Release `aosp-20260928T181511Z-336e9275-4af8ec7d` wurde um 18:40:13 UTC
mit 20 byteweise zurückgeprüften Assets veröffentlicht. Die Sicht im
gebooteten QEMU und Runtime-Ausführung stehen weiterhin aus. Das Installieren der Basisdateien startet keine
Runtime: `ro.aegis.runtime.mode=absent` bleibt gesetzt, bis Broker, Namespaces,
CE-Mounts und AOSP-Lebenszyklus integriert sind.

## Prüfung innerhalb des ausgelieferten Super-Images

`scripts/runtime/verify_product_image.py` liest nach dem AOSP-Build ausdrücklich
`super.img`. Eine separate `system_ext.img` oder das Produkt-Staging wird nicht
als Ersatz akzeptiert. Die bestehende QEMU-Konfiguration bootet Slot A; der
Prüfschritt verwendet entsprechend Metadatenslot 0 und `system_ext_a`.

Der Builder erzeugt die Hostwerkzeuge `simg2img`, `lpunpack` und `fsck.erofs`
aus seinem AOSP-Stand. Eine Android-Sparse-Datei wird zunächst in einer privaten
temporären Fläche dekodiert; Header und expandierte Größe sind begrenzt.
Das offizielle [lpunpack](https://android.googlesource.com/platform/system/extras/+/refs/tags/android-16.0.0_r1/partition_tools/lpunpack.cc)
liest daraus die ausgewählte logische Partition. Das bestehende Produktimage
hat EROFS-Magie; andere Dateisysteme werden nicht stillschweigend angenommen.
Der [EROFS-Leser](https://android.googlesource.com/platform/external/erofs-utils/+/refs/tags/android-16.0.0_r1/fsck/main.c)
prüft und entpackt das Dateisystem ohne Mount und ohne Ausführung eines
Gastprogramms. Die Prüfung läuft unprivilegiert auf dem Builder, ohne
Wiederherstellung von Host-Eigentümern oder erweiterten Dateiattributen.

Die beiden eingebetteten Basisdateien müssen exakt die gewählten Größen und
SHA-256-Werte besitzen. Fehlende, zusätzliche oder über Symlinks erreichbare
Basisdateien führen zum Abbruch. Ohne Basiswahl darf auch im fertigen Image
kein altes Runtime-Verzeichnis vorhanden sein. Änderungen an Eingabeimage,
Auswahlbericht oder Werkzeugen während der Prüfung verhindern den Abschluss.
Die private temporäre Fläche wird auch nach einem Fehler entfernt.

Jeder vollständige Release benötigt zusätzlich `runtime-base-image.json`:
Hash des tatsächlich geprüften Super-Images, Partition, Dateisystem,
Werkzeugdateien und gegebenenfalls der ausgewählten Basisdateien samt
Eingabenachweis. Der Bericht durchläuft ebenfalls Upload und Rücklesevergleich.
`PACKAGED_BASE_BYTES_VERIFIED_NOT_BOOTED` beziehungsweise
`PACKAGED_BASE_ABSENT_NOT_BOOTED` bezeichnen ausschließlich diesen Dateinachweis.
Sie bestätigen keine AVB-Signaturkette, keine SELinux-Zugriffsrechte, keine
Laufzeitaktivierung und keine Benutzerisolation.

## Bisherige Prüfungen

Die echte Originalbasis wurde nur zu einem Plan gelesen, nicht entpackt oder
ausgeführt. Vierzehn zusätzliche Hosttests verwenden öffentliche inerte
Archivfixtures und kleine ext4-Dateien mit Text und Links. Sie prüfen unter
anderem Namenskonflikte, gesperrte technische Konten, andere NSS-Dienste,
Set-ID-Bits, private Ausgangsdaten, Pfad-/Linkgrenzen, Abbrüche, Quelländerungen
sowie tatsächlich gelesene Dateiinhalte und Inode-Metadaten. Kein solches
Dateisystem wird gemountet oder als Betriebssystem verwendet. Der Image-Befehl
weist den Mac und andere Accounts bereits vor dem Aufruf eines Werkzeugs ab.

Elf weitere Integrationstests verwenden ausschließlich inerte Metadaten und
simulieren die bereits separat geprüfte Inode-Lesegrenze. Sie prüfen gefälschte
Pläne, geänderte Image-/Werkzeugbytes, Herkunft, Symlinks, Kopierabbrüche,
Erhalt einer Kernelwahl und alte Staging-Dateien. Vier zusätzliche Linux-
Worker-Tests prüfen den Release-Ablauf mit kleinen lokalen Transportfixtures;
sie bauen oder starten kein Android. Eine Bootstrap-Prüfung weist manipulierte
Laufpfade ab.

Weitere Fehlerfalltests prüfen den Super-Image-Ablauf mit inerten Fixtures,
einschließlich fehlgeschlagener Leser, falscher Dateiinhalte, alter Basisdateien,
Symlinks und während der Prüfung veränderter Eingaben. Die LP-/Sparse-
Werkzeuggrenze ist dabei simuliert. Drei Linux-Tests lesen zusätzlich kleine,
komprimierte EROFS-Dateisysteme mit ausschließlich Text über das installierte
Distributionswerkzeug; dafür wird kein neuer nativer Code kompiliert. Zwei
Worker-Tests verhindern Erfolg bei fehlendem oder beschädigt zurückgelesenem
Image-Bericht. Der tatsächliche Lauf mit den AOSP-Werkzeugen und dem neuen echten
Systemimage ist oben separat dokumentiert; die Fixtures ersetzen ihn nicht.

Das echte AOSP-Dateisystemrezept und dessen wiederholter Build sind weiterhin
unausgeführt. Erst nach Server-Build und GitHub-Transport müssen im lokalen
QEMU der schreibgeschützte gemeinsame Bestand, korrekte ID-Zuordnung, private
CE-Mounts, Linux-Programme und zwei getrennte Benutzer getestet werden.
Gemeinsame/private Pakettransaktionen samt frischer AOSP-Adminprüfung sind
ein weiterer, noch nicht implementierter Schritt.
