# SELinux- und Init-Integration der Runtime

## Persönlicher Start in d308ea6a

Der vollständige Build `aosp-20260929T042930Z-d308ea6a-57b5567f` wurde am
29. September 2026 mit `UPLOAD_VERIFIED` veröffentlicht und lokal samt
AVB-Kette geprüft. Im frischen Profil `157bd001-a912-4a2a-85c1-3d8803f03c0c`
bootet Android mit Enforcing, FBE, authentifiziertem ADB und tatsächlichem
dm-verity. Init startet den Broker regulär: `AEGIS_RUNTIME_BROKER_LISTENING`,
Domäne `u:r:aegis_runtime_broker:s0`. Der frühere Mount-Ankerfehler ist behoben.

Nach AOSP-Anmeldung von Testbenutzer 10/Seriennummer 10 scheitert `linux start`
um 04:56:22 UTC. Der AVC verweigert dem Broker `write` auf Cgroup-Inode 10132
mit Label `cgroup_v2`. Der anschließende VFS-Befund identifiziert genau diese
Inode als `/sys/fs/cgroup/aegis-runtime/u10-s10/cgroup.procs`, nun mit dem
vorgesehenen privaten Label `aegis_runtime_cgroup`. Die Gruppe ist leer.
`linux status` meldet `sealed`; `linux stop` entfernt die Gruppe bestätigt,
und der aggregierte Bereich meldet weiterhin `populated 0`.

Der gepinnte Kernel `50eb8d5d443b43f38d6e72f005f1b8601ac88a05` erklärt zwei
getrennte Voraussetzungen:

1. `cgroup_css_set_fork` prüft das Ziel über `cgroup_may_write` und
   `kernfs_get_inode` direkt mit `inode_permission(MAY_WRITE)`. Ein bislang
   nicht über VFS aufgelöstes `cgroup.procs` hat dabei keinen Dentry-Alias für
   das pfadabhängige Genfs-Label. SELinux kann erst bei einer späteren
   VFS-Auflösung den privaten Pfad zuordnen. Das passt zum beobachteten
   Labelwechsel derselben Inode. Die Zielreferenz muss vor dem atomaren Clone
   korrekt aufgelöst und währenddessen gehalten werden.
2. Danach prüft `cgroup_attach_permissions` zusätzlich Schreibrecht auf
   `cgroup.procs` des gemeinsamen Vorfahren. Der Broker liegt tatsächlich in
   `/system/uid_0/pid_626`; der geplante Kontext liegt unter `/aegis-runtime`.
   Ihr gemeinsamer Vorfahr ist die Android-Wurzel. Eine Auflösung allein
   behebt diese zweite Anforderung nicht.

Es wurde weder ein allgemeines Schreibrecht auf `cgroup_v2` erteilt noch die
atomare Prozesszuordnung abgeschwächt. Als nächste Lösung ist eine durch Init
zugewiesene private Hierarchie mit getrenntem Broker- und Kontextzweig zu
prüfen. Sie muss den gemeinsamen Vorfahren privat halten, den Broker aus
Kontext-Kill-/Speichergruppen ausschließen und Start, Absturzbereinigung sowie
Controller-Grenzen nachweisen. Diese Umstellung ist **noch nicht implementiert**.
Die bisherigen Root-Fixtures prüfen diesen Produktions-SELinux-Pfad nicht.

Belege: `out/full-build-d308ea6a/boot-1/boot-health.json` und
`out/full-build-d308ea6a/identity-test/runtime-start-diagnostic.json` samt
AVCs und CLI-Ereignissen. Eine nach Anmeldung geschriebene CE-Dateiprobe ist
nach Logout nicht lesbar; AOSP meldet nur Benutzer 0 als CE-entsperrt. Erneute
Anmeldung liefert dieselben 4096 Bytes zurück. Der Leseversuch bei gesperrtem
CE meldet `ENOENT`; die zunächst auf `ENOKEY` beschränkte Testauswertung ist
deshalb fehlgeschlagen und wird nicht als bestandener Einzeltest ausgegeben.
Diese Entwicklungs-root-Probe ist kein GNU-Isolationsnachweis. Eine tatsächliche
GNU-Sitzung oder Übernahme als sichtbarer geprüfter Launcher ist nicht erfolgt.

## Aktueller Bootbefund: immutable Basis

Das separat gepaarte Image `030dd177` bootet vollständig mit Enforcing,
FBE und authentifiziertem ADB. Die vorherige `rootfs:dir mounton`-Korrektur
lässt den Broker nun bis zur unveränderlichen Linux-Basis gelangen.
Beim ersten Start meldet er dort `errno=2`. `/dev/block/loop94` ist danach
vorhanden und unbenutzt. Ein einmaliger, kontrollierter Init-Neustart im
Gast ohne persönliche Benutzer oder belegte Runtime-Kontexte gelangt bis
zum ext4-Superblock, scheitert aber mit `errno=5`. Der Kernel meldet dazu:
`kernel -> aegis_runtime_broker:fd use` verweigert beim Lesen von `base.ext4`.
Belege: `out/full-build-030dd177/boot-1/boot-health.json` und
`controlled-start.log`. Keine GNU-Sitzung wurde gestartet.

Die nächste Quellfassung wartet höchstens zwei Sekunden insgesamt auf die
exakte, vom Kernel zugewiesene Loop-Gerätedatei. Das berücksichtigt ueventds
asynchrone Erzeugung, wie auch AOSPs `apexd_loop.cpp`. Andere Öffnungsfehler
bleiben Fehler; es gibt weder selbst erzeugte Geräte noch Ersatzpfade.
`LOOP_CONFIGURE`, Geräteidentität, Eigentümer, Readonly-/Autoclear-Flags und
unveränderliche Image-Prüfung bleiben zwingend. Feste Diagnosephasen erhalten
den ursprünglichen Fehlercode. Entsprechend AOSPs Kernel-/APEX-Regel wird
nur dem Kernel die Benutzung der Broker-Deskriptoren und das Lesen des eigenen
Image-Dateityps erlaubt. `026665fb` kompiliert diese Policy einschließlich
Neverallow-, Treble- und Kontextprüfungen erfolgreich. Der kalte lokale Start
dieses Images erreicht die ext4-Superblock-Erzeugung ohne den alten Kernel-FD-
AVC, scheitert dort aber mit `errno=13` und
`aegis_runtime_broker -> aegis_runtime_base_file:filesystem relabelfrom`.
Der gepinnte Kernel setzt bei `context=` erst die Superblock-SID und prüft
danach das Root-Inode-Label gegen genau diese neue SID. `a187a309` ergänzt
ausschließlich die dafür fehlende Broker-/Basis-Berechtigung. Sein vollständiger
Build ist kompiliert und wird paketiert; der neue Bootnachweis steht aus.
Beleg des fehlgeschlagenen Kaltstarts:
`out/full-build-026665fb/boot-1/boot-health.json`.

Stand 29. September 2026: Die Policy und Komponenten von
`6633a0862796d70304456c363158d133e9711513` wurden auf `aegis-build` erfolgreich
kompiliert, einschließlich Neverallow-, API-Freeze-, Treble-, Kontext- und
Policy-Tests. Lauf `identity-20260928T235046Z-6633a086-BVktdL` endete mit
`IDENTITY_COMPILED_NOT_INSTALLED`. Das ist noch kein Nachweis eines gestarteten,
erzwingend getrennten Linux-Kontexts.
Das bisher als sichtbarer Stand geprüfte Profil bleibt im Modus `absent`.
Der neue Vollbuild `a8d38b97` bootet im separaten Profil tatsächlich mit
`managed-v1`, SELinux Enforcing und bestätigtem Verity. Init startet den Broker,
dieser bricht jedoch vor dem Anlegen seiner Zustandsdatei mit Exitcode 1 ab.
Ein spezifischer AVC fehlt; seine bisherigen stderr-Diagnosen werden von
Init verworfen. Damit ist weder ein produktiver Runtime-Start noch dessen
Isolation nachgewiesen. Feste Android-Logmeldungen und die Korrektur der
beobachteten Helfer-Eigentümerschaft `root:shell` sind als Quellstand `4e53dc18`
kompiliert. Seine 113 nativen root-Fixture-Tests bestehen im unveränderten
Testimage `a8d38b97`; der neue produktive Dienst und seine tatsächlichen
Diagnosemeldungen sind damit weiterhin nicht geprüft.

Der Teststand aktiviert `managed-v1` nur mit den explizit geprüften
Basis- und Kernel-Eingaben des Build-Workers. Eine normale Quellregistrierung
entfernt diese Auswahl; ohne Basis bleiben Modus und Dienste inaktiv, mit Basis
aber ohne ausgewählten Kernel bricht bereits die Produktkonfiguration ab.

Vier eigene Domänen trennen den vertrauenswürdigen Speicher-/Prozessbesitzer,
die kurzlebige Namespace-Einrichtung, den persönlichen Aufseher und normale
GNU-Programme. Nur SystemServer darf den init-eigenen Broker-Socket verbinden;
Dateirechte und native Peer-/Protokollprüfung gelten zusätzlich. Es wird kein
öffentlicher Binder- oder Shell-Zugang zum nativen Owner eingeführt.

Die gepinnte Plattformpolicy verbietet neuen Domänen Mounts, Mknod und DAC-
Ausnahmen. `register-runtime-policy.py` ergänzt deshalb ausdrücklich benannte
private Attribute für genau diese neuen vertrauenswürdigen Komponenten. Die
eingefrorene öffentliche Android-Policy bleibt unverändert. Eine weitere,
geschlossene Zuordnung erlaubt nur dem Aufseher und normalen GNU-Programmen
die Ausführung aus dem unveränderlichen Basis-Dateisystem. Dieses trägt nach
AOSP-Vorgabe `fs_type` und niemals zugleich `file_type`; die bestehenden
Schreibverbote für `contextmount_type` gelten unverändert. Es entfernt
keine Neverallow-Regel und nimmt keine bestehende Android-Domäne neu aus.
Zusätzliche Neverallows schließen die Attributmitgliedschaft auf die genannten
AEGIS-Typen. Normale GNU-Programme erhalten keine Capability, Mountberechtigung
oder Schreibberechtigung auf die gemeinsame Softwarebasis. Die normale
Android-Shell-Ausführungsfreigabe wird für sämtliche Runtime-Domänen entfernt.
Ebenso entfällt deren allgemeiner Android-Schreibzugriff auf Host-Cgroups;
der Broker erhält ausdrücklich nur seinen eigenen Unterbaum. Ein zusätzliches
Neverallow verbietet Runtime-Schreibzugriffe auf die übrige Cgroup-Hierarchie.
SD-Card-/FUSE-Attribute und permissive Domänen werden nicht verwendet.

Die AOSP-Revision und vier ursprüngliche Policydateien sind gehasht gepinnt.
Der Integrator prüft sämtliche Zieldateien vor Änderungen, erhält Sicherungen,
verweigert fremde Bearbeitungen/Symlinks und prüft die tatsächlichen Bytes nach
der Integration sowie nach dem Build erneut. Sieben inerte Hosttests prüfen
diese Quellübernahme. Zwei weitere prüfen die Übernahme einer eigenen älteren
Quellregistrierung und den Schutz fremder Änderungen beim Ergänzen verwalteter
Dateien. Sie kompilieren keine Policy und belegen keine Isolation.

Die Produktpolicy kennzeichnet unveränderliche Images, private CE-Verzeichnisse,
Geräteansichten, temporäre Dateien und den eigenen Cgroup-Unterbaum getrennt.
FScrypt-Ioctl-Freigaben betreffen ausschließlich Richtlinien-/Schlüsselstatus,
nicht Erzeugung oder Entzug von Schlüsseln. Die ursprüngliche Vold-Neverallow
für Schlüsseländerung und Status wird dafür getrennt: Nur beim lesenden
Statusaufruf erhält der neue Broker eine Ausnahme, zusätzlich auf seine
CE-Dateitypen begrenzt. Die Verbote für Schlüsseländerung und das Setzen einer
Verschlüsselungsrichtlinie bleiben bestehen. AOSP bleibt dafür zuständig.
Die einzige `no_new_privs`-/`nosuid`-Transition führt vom geprüften Aufseher
zu einem gewöhnlichen Programm aus der unveränderlichen Softwarebasis.

Der Init-Eintrag startet nur im expliziten verwalteten Modus nach `post-fs-data`.
Ein fehlgeschlagener Broker wird nicht automatisch neu gestartet. Der native
Handshake darf erst nach bestätigter Altprozessbereinigung erfolgreich sein.
Der aktivierte Teststand wird erst nach Policy-Kompilierung in einem eigenen
neuen QEMU-Profil geprüft. Noch erforderlich sind die tatsächlichen Domänenübergänge,
Mount-/CE-/PTY-Zugriffe und Fehlerfälle einschließlich Benutzerisolation und
vollständigem Abbau vor CE-Sperre. Ein erfolgreicher Compiler allein genügt nicht.
# Init-Namespace-Übergabe nach dem verwalteten Bootversuch

Das vollständige Image `4e53dc18` bootet im lokalen QEMU mit Enforcing und
Verity. Der Init-gestartete Broker meldet im durchgehenden seriellen Log
`AEGIS_RUNTIME_NAMESPACE_FAILED: open init proc directory errno=2`.
Der Gast verwendet `/proc` mit `hidepid=invisible`. Die bisherigen Root-
Gerätetests im Entwicklungsbereich konnten PID 1 lesen und deckten diese
Produktionsgrenze nicht ab.

Die vorbereitete Korrektur verwendet drei feste `file /proc/1/ns/... r`-
Einträge im Init-Dienst. Der gepinnte AOSP-Init öffnet diese vor dem Fork,
publiziert sie im Kind und wechselt anschließend zum Dienstprogramm.
Der Broker akzeptiert ausschließlich die bekannten Deskriptoreinträge aus
diesem Startpfad, prüft NSFS, Zugriffsmodus, Namespace-Art, eigene Namespace-
Identität und die bisherigen Root-/Mapping-/Einzelthreadbedingungen. Er hält
CLOEXEC-Kopien; ein Fork oder der Austausch einer einmal gebundenen Namespace-
Identität kann keine neue Besitzerberechtigung erzeugen. Die private Mount-
Namespace bleibt danach an ihren ursprünglichen Prozess gebunden.

Ein NSFS-eigenes Genfs-Label ersetzt hier das zuvor beobachtete `unlabeled`
für Kernel-Namespace-Objekte. Die Laufzeit erhält keine Leserechte auf
allgemeine unbeschriftete Dateien. Init und Vold behalten ihren vorhandenen
lesenden Namespace-Zugriff; die Runtime erhält nur die notwendigen Lese-
und Typ-/Eigentümerabfragen. Ptrace auf Init, Readproc-Gruppenzugehörigkeit,
Schreibzugriff auf Namespace-Objekte oder Abschalten von SELinux sind kein
Bestandteil der Korrektur. Die bisherigen ungenutzten PID-1-Procrechte des
Brokers entfallen.

Der Komponentenstand `3b350e74` ist einschließlich Policy kompiliert. Alle
114 nativen Tests bestehen im vorherigen lokalen Image `4e53dc18`; diese
Root-Fixtures allein prüfen den neuen produktiven Dienststart noch nicht.

Bei der nachfolgenden Prüfung des gepinnten Kernelpfads `may_create()` fällt
eine weitere Startvoraussetzung auf: Vor dem tatsächlichen Genfs-Lookup wird
das Anlegen des Cgroup-Verzeichnisses gegen dessen Type-Transition geprüft.
Eine ausschließlich benannte Transition für `aegis-runtime` und die Zuordnung
des privaten Typs zum Cgroup2-Dateisystem ergänzen deshalb die Policy.
Dies erlaubt weder beliebige Android-Cgroup-Verzeichnisse noch Schreiben
globaler Controllerdateien. Die Änderung betrifft keine nativen Quelltexte;
der Vollbuild `afaf6462` ist inzwischen kompiliert und über GitHub verifiziert.
Sein separates lokales Profil bootet mit Enforcing, FBE und tatsächlichem
dm-verity. Die drei Namespace-Objekte tragen das neue NSFS-Label. Init-Übergabe,
Namespace-Prüfung und Cgroup-Vorbereitung gelingen: Der private Unterbaum ist
`root:root 0700` mit `aegis_runtime_cgroup`, aktiviertem Memory-Controller,
2-GiB-Limit und `populated 0`.

Der nächste Startfehler ist nun konkret im Kernel-Audit belegt:
`aegis_runtime_broker` erhält beim festen privaten Propagationswechsel von `/`
ein verweigertes `rootfs:dir mounton`; der Broker meldet
`private mount namespace errno=13`. Der gepinnte Kernel prüft in
`selinux_mount()` auch `MS_PRIVATE` über `FILE__MOUNTON`.
Die folgende Korrektur ergänzt ausschließlich dieses Recht für den Broker.
Der native Pfad führt den Wechsel erst nach `unshare(CLONE_NEWNS)` aus und
akzeptiert dafür weder einen Clientpfad noch Mountflags von außen. Andere
Runtime-Domänen erhalten kein solches Recht. Der geänderte Policy-Stand muss
erneut kompiliert und im vollständigen Image gestartet werden; persönliche
GNU-Ausführung und Isolation bleiben offen.
Rohbelege: `out/full-build-afaf6462/boot-1/boot-health.json`,
`namespace-labels.txt`, `cgroup-metadata.txt` und `runtime-kernel-avcs.txt`.
