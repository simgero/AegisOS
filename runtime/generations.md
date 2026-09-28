# Gemeinsame Softwaregeneration

Stand: 28. September 2026. **Buildrezept vorbereitet, noch nicht auf dem Builder
ausgeführt.** Die echte Debian-Basis wurde nur gelesen und geplant. Es gibt
noch kein daraus erzeugtes, veröffentlichtes oder in QEMU eingebundenes
Runtime-Dateisystem. `ro.aegis.runtime.mode=absent` bleibt unverändert.

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
Basen erfordern eine Anpassung und Prüfung des Rezepts.

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
GitHub-Veröffentlichung, geschützte Ablage/Aktivierung in Android und die
autorisierte Mount-Anbindung fehlen noch. Die Produktkonfiguration installiert
das Image nicht automatisch. Ein existierendes QEMU-Profil wird nicht verändert.

## Bisherige Prüfungen

Die echte Originalbasis wurde nur zu einem Plan gelesen, nicht entpackt oder
ausgeführt. Vierzehn zusätzliche Hosttests verwenden öffentliche inerte
Archivfixtures und kleine ext4-Dateien mit Text und Links. Sie prüfen unter
anderem Namenskonflikte, gesperrte technische Konten, andere NSS-Dienste,
Set-ID-Bits, private Ausgangsdaten, Pfad-/Linkgrenzen, Abbrüche, Quelländerungen
sowie tatsächlich gelesene Dateiinhalte und Inode-Metadaten. Kein solches
Dateisystem wird gemountet oder als Betriebssystem verwendet. Der Image-Befehl
weist den Mac und andere Accounts bereits vor dem Aufruf eines Werkzeugs ab.

Das echte AOSP-Dateisystemrezept und dessen wiederholter Build sind weiterhin
unausgeführt. Erst nach Server-Build und GitHub-Transport müssen im lokalen
QEMU der schreibgeschützte gemeinsame Bestand, korrekte ID-Zuordnung, private
CE-Mounts, Linux-Programme und zwei getrennte Benutzer getestet werden.
Gemeinsame/private Pakettransaktionen samt frischer AOSP-Adminprüfung sind
ein weiterer, noch nicht implementierter Schritt.
