# SELinux- und Init-Integration der Runtime

## Tatsächliche GNU-Ausführung in 6a807692

Der vollständige Build `aosp-20260929T092225Z-6a807692-0febd375` enthält
ausschließlich die gezielte zusätzliche Verknüpfungsfreigabe des Aufsehers
für die unveränderliche Basis. Er wurde über GitHub verifiziert übertragen
und im separaten lokalen Profil `8b1e5ec2-3bae-4601-b8ef-eb8f156867d4`
getestet. Enforcing, FBE, sicheres ADB und tatsächliches dm-verity bestehen.

Alpha und Beta starten tatsächliche Bash-Sitzungen in
`u:r:aegis_runtime_program:s0`, mit interner UID/GID 1000 und verschiedenen
Host-Zuordnungen. Der produktive PTY-/Exec-Übergang funktioniert. Geprüfte
fremde Datei- und Prozesszugriffe werden in beiden Richtungen verhindert;
die unabhängigen Positivkontrollen bestätigen den weiterhin laufenden
fremden Prozess. Benutzerwechsel und Bildschirmsperre widerrufen die aktive
PTY, aber nicht die Hintergrundkontexte. Logout entfernt die zugehörigen
Kontexte und sperrt CE. Beide GNU-Dateien überstehen den Neustart desselben
Profils mit erhaltenem KeyMint-Zustand bytegenau.

Keine zusätzliche FSETID-, Ptrace- oder allgemeine Cgroup-Freigabe wurde
dafür eingeführt. Drei nachträgliche Terminalwiderrufe nach frischer Anmeldung
bleiben als funktionaler Fehler offen; erneute Anmeldung funktioniert.
Diese Tests sind keine vollständige Isolationsexpertise und decken noch
keine Pakettransaktionen oder Benutzerlöschung ab. Genaue Versionen,
Beobachtungen, Grenzen und Prüfsummen: [GNU-Test](../docs/runtime-gnu-qemu-test.md).

## Persönlicher Kontext startet in 4366aa25; Bash noch nicht

Build `aosp-20260929T085012Z-4366aa25-7160e5c0` wurde mit `UPLOAD_VERIFIED`
veröffentlicht und lokal in Profil `a5a1ba9e-9836-4e66-9d16-3b8c506f33d5`
getestet. Raw-SHA-256:
`47b156d77edd3262b5808f9b236d557b581c80f71974d04229c05f127eea1064`;
AVB-Digest:
`7bedc8751c4ceebbb193d789928b4180c65265d7d19fb7f3eaa884a001be43a1`.
Boot, Enforcing, FBE, tatsächliches dm-verity, sichere ADB-Anmeldung und private
Cgroup-Delegation bestehen. Die Testanmeldung weist ein falsches Passwort ab;
unabhängige AOSP-CE-Abfragen davor und danach bleiben `[0]`, erst nach dem
korrekten Passwort `[0, 10]`.

`linux start` um 09:16:03 UTC meldet `runtime=ready`. Der laufende Prozess 4806
ist tatsächlich `u:r:aegis_runtime_init:s0`, mit UID/GID-Abbildung
`0 -> 1005000 (1000 IDs)`, `1000 -> 1007500`, `65534 -> 1007501` und Cgroup
`/aegis-runtime/contexts/u10-s10`. Das Readback der Aufseher-Mounts und
Namespaces stammt von Entwicklungs-root, **nicht von einer GNU-Sitzung**.
Die O_PATH-Empfangskorrektur und der feste Setup-zu-Init-Übergang sind damit
im Produkt beobachtet.

`linux shell` um 09:16:20 scheitert an `init base:lnk_file read` für `bin`
auf `loop94`. Die gemeinsame Debian-Basis verwendet `/bin -> usr/bin`;
das Kind löst diesen Pfad bei `execve` noch in der Aufseher-Domäne auf. Die neue
Policy erlaubt nur `{ getattr read }` für Verknüpfungen dieser unveränderlichen
Basis im Aufseher. Andere Daten- und Gerätearten bleiben unverändert.
Zwei vorherige `setup self:cap_userns fsetid`-Verweigerungen brechen den
Kontextstart nicht ab; sie werden aufgezeichnet, nicht pauschal freigegeben.
PTY-Übergabe und tatsächliche GNU-Ausführung sind weiterhin ausstehend.

`linux stop` um 09:18:00 und AOSP-Logout um 09:18:07 funktionieren. Das
unabhängige Readback um 09:18:35 bestätigt nur Benutzer 0 gestartet und
CE-entsperrt, `populated 0`, `frozen 0` und die entfernte Gruppe `u10-s10`.
Belege einschließlich Prüfsummen:
`out/full-build-4366aa25/identity-test/`; Bootbelege unter `boot-1/`.
Native/Java-Quellen sind unverändert. Die Verknüpfungskorrektur benötigt noch
einen neuen vollständigen Build und den lokalen Ausführungsnachweis.

## Vollständiger Boot und persönlicher Start in 7c6b9b1c

Build `aosp-20260929T081643Z-7c6b9b1c-e191fec1` ist mit `UPLOAD_VERIFIED`
veröffentlicht. Das getrennte Profil `412e9f67-2943-40b8-9cfb-1d6eeb0fda76`
bootet mit Enforcing, FBE, authentifiziertem ADB, tatsächlichem dm-verity und
korrekter privater Cgroup-Delegation. Das lokale Raw-Image hat SHA-256
`9b7a8a16f2debb9a9b4d30fedc91936c4cddd138c86218697395ea9eb2a22bf0`.
Bootbelege: `out/full-build-7c6b9b1c/boot-1/`.

Nach AOSP-Anlage und Anmeldung scheitert `linux start` um 08:43:40 UTC an
`aegis_runtime_setup aegis_runtime_home_file:dir ioctl` auf dem privaten
Home-Mount (`dm-102`, Inode 4944). Der Setup-Helfer läuft damit nachweislich
in seiner vorgesehenen Domäne. Die vorigen Übergangs- und Kill-Verweigerungen
sind nicht mehr vorhanden. Die nachfolgenden Init-/PTY-Übergänge sind damit
noch nicht funktional bewiesen. `linux stop` um 08:44:25, `logout` um 08:44:32
und unabhängiges Readback um 08:45:39 bestätigen Abbau, CE-Sperre und entfernte
Kontext-Cgroup. Belege: `out/full-build-7c6b9b1c/identity-test/`, einschließlich
Prüfsummen. **Noch keine GNU-Ausführung.**

Im unveränderten gepinnten Kernel ruft `selinux_file_receive` in
`security/selinux/hooks.c:3964` `file_has_perm(..., file_to_av(file))` auf.
`file_to_av` bildet Deskriptoren ohne `FMODE_READ`/`FMODE_WRITE` auf
`FILE__IOCTL` ab. `open_tree` und `fsmount` liefern hier `O_PATH`-Referenzen.
Der private Setup-Kanal überträgt genau die vom Broker erzeugten Basis-,
Home- und Geräte-Mounts sowie den unveränderlichen Init-Code. Empfänger prüfen
Rolle, Benutzer-ID, Seriennummer, Anzahl und Commit-Reihenfolge; unvollständige
Übergaben werden geschlossen. Der Client kann diese Deskriptoren nicht wählen.

Die Basis besitzt das benötigte Recht bereits über `r_dir_perms`. Die neue
Freigabe ergänzt ausschließlich `setup { home_file devices_file }:dir ioctl`
für die beiden weiteren privaten Mountwurzeln. Sie ergänzt keine Xperm-
Befehlsnummern. AOSPs allgemeine Befehlsliste bleibt hier auf `FIOCLEX` und
`FIONCLEX` beschränkt; die Vold-only-Schlüsselregeln bleiben unverändert.
Dies ist eine Empfangsprüfung, kein tatsächlicher Fscrypt-Ioctl durch Setup.
Native und Java-Quellen bleiben unverändert. Die Policy-Korrektur benötigt
Kompilierung und den erneuten vollständigen lokalen Startnachweis.

## Vollständiger Boot und persönlicher Start in eb0ba22d

Build `aosp-20260929T074031Z-eb0ba22d-f143a89d` ist mit `UPLOAD_VERIFIED`
veröffentlicht. Das getrennte Profil `de199957-af99-4599-8011-1bdd70a021c7`
bootet mit Enforcing, FBE, authentifiziertem ADB, tatsächlichem dm-verity und
korrekter privater Cgroup-Delegation. Die ADB-Einrichtung funktioniert direkt.
Die frühere Tmpfs-Wurzelverweigerung tritt beim persönlichen Start nicht mehr
auf. Bootbelege: `out/full-build-eb0ba22d/boot-1/`.

Nach AOSP-Anlage und Anmeldung scheitert `linux start` um 08:05:28 UTC am
Übergang `broker -> setup`, Klasse `process2`, Recht `nosuid_transition`.
Die zusätzliche `execute_no_trans`-Verweigerung ist der verweigerte Rückfall
in die alte Domäne, keine Aufforderung, diesen Rückfall zu erlauben.
Bei der anschließenden Bereinigung fehlt dem Broker `self:cap_userns kill`.
`linux stop` um 08:06:10 und `logout` um 08:10:22 bestätigen dennoch den
vollständigen Abbau und die CE-Sperre. Das unabhängige Readback um 08:11:27
bestätigt nur Benutzer 0 gestartet/CE-offen, `populated 0` und die entfernte
Gruppe `u10-s10`. Belege: `out/full-build-eb0ba22d/identity-test/`, einschließlich
Prüfsummen. **Noch keine GNU-Ausführung.**

Der gepinnte Kernel `50eb8d5d443b43f38d6e72f005f1b8601ac88a05` behandelt in
`fs/namespace.c:mnt_may_suid` fremde Mounts als nosuid. Dies betrifft den
unveränderlichen System-Mount nach Eintritt in den Kind-User-Namespace und
auch den gehaltenen Init-Code-Deskriptor nach `pivot_root`.
`security/selinux/hooks.c:check_nnp_nosuid` prüft die jeweilige feste
Domänenpaarung; bei verweigertem automatischem Übergang setzt der Exec-Hook
die alte Domäne zurück. Die Korrektur erlaubt daher ausschließlich
`broker -> setup` und `setup -> init` mit `nosuid_transition`. Sie erlaubt
keinen `execute_no_trans`-Rückfall, keine dynamischen Übergänge und keine
zusätzlichen Fähigkeiten gewöhnlicher GNU-Prozesse.

`kernel/signal.c:kill_ok_by_cred` prüft beim Signal an das UID-gemappte Kind
`CAP_KILL` in dessen User-Namespace. Die bestehende Host-Capability umfasst
diese SELinux-Klasse nicht. Die ergänzte Broker-Freigabe gilt für
`cap_userns kill`; der native Pfad bleibt auf den eigenen bestätigten Pidfd
beschränkt und verwendet weder fremde PID-Auflösung noch Ptrace.

Die Quellprüfung des nachfolgenden Terminalpfads zeigt zwei weitere präzise
Lücken: `fs/devpts/inode.c:mknod_ptmx` erzeugt den Master beim Mount durch
Setup; erst die Slave-Erzeugung erfolgt als Init. Beide müssen den privaten
Typ `aegis_runtime_init_devpts` tragen. Der neue Setup-Typübergang und sein
reines `getattr`-Readback ersetzen die bisherigen allgemeinen Devpts-Rechte.
Init erhält auf dem privaten Typ `TIOCGPTN` und `TIOCSPTLCK`; der Broker erhält
für die Eigentümerprüfung `TIOCGPTN` und `TIOCGPTPEER` (0x5441, im gepinnten
Kernel definiert, ohne AOSP-Makro). Diese Befehle fehlen in AOSPs
`unpriv_tty_ioctls`. `TIOCSTI` bleibt für alle Domänen auf diesem Typ verboten.
Dies ist vorerst Quellbegründung, kein beobachteter Terminalerfolg.

Die tatsächliche Android-Wurzel ist EROFS; Setup besitzt bereits das
Unmount-Recht für `labeledfs`. Eine zusätzliche Rootfs-Freigabe wird nicht
auf Verdacht erteilt. Native und Java-Quellen bleiben unverändert. Neuer
Policy-Build und lokaler produktiver Startnachweis sind erforderlich.

## Vollständiger Boot und persönlicher Start in ba081a55

Build `aosp-20260929T070023Z-ba081a55-79212b67` ist mit `UPLOAD_VERIFIED`
veröffentlicht. Alle Assets, AVB und die lokal erzeugte Platte sind geprüft.
Profil `d13333cc-ccd5-4803-83e9-a1e91c56caad` bootet ohne Fenster mit Enforcing,
FBE, authentifiziertem ADB und tatsächlichem dm-verity. Alle privaten
Cgroup-Werte und Labels sind bestätigt; die Init-`create`-AVCs treten nicht
mehr auf. Bootbelege: `out/full-build-ba081a55/boot-1/boot-health.json`
und `delegation-check.json`.

Der erste persönliche Benutzer wird durch AOSP angelegt; falsches Passwort
wird abgewiesen, korrektes Passwort entsperrt CE. `linux start` um 07:33:34 UTC
scheitert an `aegis_runtime_broker tmpfs:dir read` für die neue Wurzel-Inode 1.
Der Kontext wird gesperrt. `linux stop` um 07:34:11 und `logout` um 07:34:25 UTC
bestätigen Abbau und CE-Sperre. Unabhängiges Readback bestätigt ausschließlich
Benutzer 0 gestartet/CE-offen, `populated 0` und entfernte Gruppe `u10-s10`.
Belege: `out/full-build-ba081a55/identity-test/`. **Keine GNU-Ausführung.**

Der gepinnte Kernel entscheidet in `security/selinux/hooks.c:1500` bei
`SECURITY_FS_USE_TRANS` anhand der tatsächlichen Inode-Klasse. Die Wurzel
einer neuen Tmpfs ist `dir`; die vorhandenen Übergänge `tmpfs:file` erfassen
sie nicht. `devices.c` öffnet diese eigene, noch nicht eingehängte Wurzel
lesend, um ausschließlich die sechs festen Zeichengeräte, drei Verzeichnisse
und fünf Links einzurichten. Die Kernel-Erzeugungsprüfung für Kinder verwendet
anschließend den Typ des Elternverzeichnisses (`selinux_determine_inode_label`).

Die Korrektur ergänzt bei den vorhandenen Übergängen von Broker und Setup
jeweils ausschließlich die Klasse `dir`. Neue Tmpfs-Wurzeln erhalten damit die
schon vorgesehenen privaten Geräte-/Scratch-Labels. Sie erteilt keine neuen
Lese- oder Schreibrechte auf generisches Tmpfs, keine Ptrace-Rechte und keine
Ausnahme von AOSP-Neverallows. Native und Java-Implementierung bleiben
unverändert. Die Korrektur ist vorbereitet; Kompilierung und vollständiger
produktiver Startnachweis bleiben erforderlich.

Die erste ADB-Einrichtung erreichte beim langen öffentlichen Schlüsselbefehl
das Konsolenzeitlimit; der Befehl wurde später mit Exit 0 bestätigt.
Abschalten der interaktiven mksh-Zeilenbearbeitung an der dedizierten
Entwicklungskonsole erlaubt die unveränderte authentifizierte Einrichtung.
`connect-local-adb.py` führt dies nun vor seinen Konsolenbefehlen aus.

## Vollständiger Boot und persönlicher Start in ebf3610

Build `aosp-20260929T053136Z-ebf36104-d60e8839` ist mit
`UPLOAD_VERIFIED` veröffentlicht. Alle 21 Assets und die AVB-Kette sind
lokal geprüft. Das eigene Profil `86e85222-c404-496e-89a5-96bf40b319ea`
bootet mit Enforcing, FBE, authentifiziertem ADB und tatsächlichem dm-verity.
Der Init-gestartete Broker meldet `AEGIS_RUNTIME_BROKER_LISTENING` und liegt
in Cgroup v2 unter `/aegis-runtime/broker`. Eigentümer, Modi, private Labels,
Controller und sämtliche Delegations-/Kontextgrenzen sind aus dem Gast
zurückgelesen. Die alte Cgroup-Schreibverweigerung tritt beim Startversuch
nicht mehr auf.

Die erste Auswertung scheiterte an zwei Annahmen des Prüfskripts: Der
SELinux-Prozesskontext enthält ein abschließendes NUL, und `/proc/PID/cgroup`
enthält neben der relevanten v2-Zeile auch Androids Legacy-Controller. Diese
Auswertung ist korrigiert; die tatsächliche Gastkonfiguration wurde dafür
nicht verändert. Fünf Init-`create`-AVCs bleiben dokumentiert, obwohl die
erwarteten Werte gesetzt sind. Der Kandidat ist damit noch nicht abgenommen.

AOSP legt Testadministrator 10/Seriennummer 10 an. Das falsche Passwort wird
abgewiesen, das richtige entsperrt CE. `linux start` am 29. September um
06:03:15 UTC scheitert anschließend an
`aegis_runtime_broker self:cap_userns sys_ptrace`. `linux status` meldet
`sealed`. Um 06:04:27 UTC bestätigt `linux stop` den Abbau; nach erneuter
Anmeldung bestätigt `logout` Benutzerstopp und CE-Sperre. **GNU wurde noch
nicht ausgeführt.** Belege liegen unter `out/full-build-ebf3610/boot-1/`
und `out/full-build-ebf3610/identity-test/`.

Der gepinnte Kernel `50eb8d5d443b43f38d6e72f005f1b8601ac88a05` prüft in
`security/commoncap.c:cap_ptrace_access_check` bei unterschiedlichen
User-Namespaces `ns_capable(child_cred->user_ns, CAP_SYS_PTRACE)`. Der
Namespace-Eigentümer im unmittelbaren Eltern-Namespace erfüllt die
Capability-Prüfung in `cap_capable`; SELinux prüft zusätzlich `cap_userns`.
`selinux_capable` verwendet die Host-Klasse `capability` ausschließlich für
`init_user_ns`. Der native Pfad öffnet die Proc-Metadaten seines per Pidfd
gehaltenen, noch gesperrten Kindes zur Einrichtung und Prüfung der ID-Maps.

Der Vollbuild `aosp-20260929T061337Z-430f91da-820a3c19` endete am
29. September um 06:19:53 UTC mit `FAILED`. Die AOSP-Neverallow-Regel in
`system/sepolicy/private/domain.te:1627` verbietet dem Broker `sys_ptrace`
auch in der Klasse `cap_userns`. Die versuchte Policy-Erweiterung ist daher
verworfen; die AOSP-Regel bleibt unverändert. `inactive` und
`ExecMainStatus=0` nach dem Einsammeln des Wrappers widerlegen diesen Fehler
nicht. Kein Image dieses Laufs wurde veröffentlicht oder lokal übernommen.

Die folgende native Korrektur vermeidet den fremden Proc-Pfad vollständig.
Der eigene vertrauenswürdige Raw-Clone öffnet vor Ausführung des Setup-Helfers
seine vier Proc-Inodes (`uid_map`, `gid_map`, `setgroups`, `oom_score_adj`)
als `O_PATH` und seinen User-Namespace als NSFS-Deskriptor. Er übergibt sie
über den nur zwischen Elternprozess und Kind geerbten privaten Gate-Socket.
Der Elternprozess akzeptiert genau diese fünf unterschiedlichen Referenzen,
prüft Typen, Dateisysteme und Namespace-Art sowie den eigenen lebenden Pidfd.
Die Referenzen werden durch die eigene `/proc/self/fd`-Tabelle neu geöffnet.
So bindet der Kernel die Map-Dateien an die ursprünglichen Eltern-Credentials;
ein vom Kind bereits schreibbar geöffneter Map-Deskriptor wäre für die
mehrteilige Abbildung nicht ausreichend (`new_idmap_permitted`). Es wird weder
eine Kind-PID aufgelöst noch dessen Namespace-Magic-Link vom Elternprozess
verfolgt. Kein Client kann diesen Kanal, eine PID oder einen Deskriptor wählen.

Die bisherigen Einmal-Schreibvorgänge, exakte Map-Rückprüfung, OOM-Rücksetzung,
Ausführungssperre und Pidfd-Abbau bleiben erhalten. Alle temporären Referenzen
werden geschlossen; nur der verifizierte Namespace bleibt bis zum Abbau.
Ein explizites Neverallow umfasst jetzt beide Capability-Klassen. Die neue
Fassung `c0d8c16c` ist inzwischen kompiliert; alle AOSP-Neverallow- und
Kompatibilitätsprüfungen sowie 124/124 native Tests im lokalen QEMU bestehen.
Ein vollständiger Boot mit dem neuen Broker bleibt erforderlich. Root-Fixtures
allein beweisen weiterhin nicht den produktiven SELinux-Pfad.

Die fünf Initialisierungszugriffe wechseln in dasselbe Init-Taskprofil, vor
dessen abschließender PID-Zuweisung. Der gepinnte
`WriteFileAction::WriteValueToFile` öffnet vorhandene Dateien mit
`O_WRONLY | O_CLOEXEC`; Inits `write` verwendet zusätzlich `O_CREAT` und
`O_TRUNC`. So entfällt der unnötige Erzeugungsversuch ohne neue
Dateierzeugungsrechte oder unterdrückte Audits. Die native Prüfung aller
Werte vor jeder Kontextbereinigung bleibt zwingend. Auch diese Änderung
bedarf noch der Kompilierung und des vollständigen Bootnachweises.

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
Controller-Grenzen nachweisen. Die folgende Quelländerung setzt diese
Umstellung um; Build und Gastnachweis dieser neuen Fassung stehen noch aus.
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

### Private Delegation im neuen Quellstand

Init erstellt `/sys/fs/cgroup/aegis-runtime` mit einem separaten `broker`-Blatt
und aktiviert dort den bereits von Android delegierten Speichercontroller.
Das Gerät ergänzt genau ein Taskprofil `AegisRuntimeBroker`; AOSPs vorhandene
Plattformprofile bleiben erhalten. Der gepinnte Profilparser ignoriert
`JoinCgroup` für Cgroup v2. Deshalb verwendet das Profil die unterstützte
`WriteFile`-Aktion mit explizitem `ProcFilePath` und `<pid>`. Init wendet sie
nach seiner normalen Gruppenaktivierung und vor dem Wechsel zur Broker-Domäne
an. Der Broker prüft seine exklusive Mitgliedschaft selbst, weil Init einen
Profilfehler nur protokolliert. Ein fehlgeschlagener oder falsch zugeordneter
Start darf die Kontextbereinigung nicht erreichen.

Der Broker darf ausschließlich den Zweig `contexts` erstellen und bereinigen.
Dessen bestehende 2-GiB-Gesamtgrenze, persönliche 1-GiB-Grenzen, maximale
Kontextanzahl und `cgroup.kill` schließen das Broker-Blatt aus. Eigene Labels
trennen Delegationssteuerdateien, Broker-Blatt, gemeinsamen `cgroup.procs`-
Vorfahren und Benutzergruppen. Die Runtime erhält keine Schreibrechte auf
Androids gemeinsame Cgroup-Dateien oder die Steuerdateien ihrer Delegation
bzw. ihres Brokers. Nur die für `clone3` nötige Schreibprüfung des privaten
gemeinsamen Vorfahren ist freigegeben. Der Broker hält dessen aufgelösten
Deskriptor bis nach dem Kontextabbau; jede persönliche Gruppe hält entsprechend
ihren Ziel-Deskriptor bis zur bestätigten Entfernung.

Init sendet im gepinnten `libprocessgroup/processgroup.cpp` auch nach einer
Gruppenmigration mindestens dem ursprünglichen Dienstprozess ein Signal.
Der Broker bleibt für den bestätigten Abbau seiner persönlichen Prozesse
zuständig; PID1-PDEATHSIG und die unverändert erforderliche Recovery bleiben
zusätzliche Grenzen. Der tatsächliche Signal-/Shutdown-Pfad in der neuen
Hierarchie ist noch im vollständigen Gast zu prüfen.

Sechs zusätzliche native Tests prüfen die neue Delegation, Fehler vor Recovery,
getrennten Broker-Erhalt und die Lebensdauer des Ziel-Deskriptors. Sie müssen
auf dem Builder kompiliert und im lokalen Android-QEMU ausgeführt werden;
Root-Fixtures ersetzen weiterhin nicht den tatsächlichen Produktions-Boot.

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
