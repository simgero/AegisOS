# Komponentenläufe in lokalem QEMU

## Mount-Besitz nach vollständigem Boot von a187a309

Build [`aosp-20260929T034544Z-a187a309-0129b0ef`](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T034544Z-a187a309-0129b0ef)
ist veröffentlicht und nach Asset-/AVB-Prüfung im eigenen gekoppelten Profil
gestartet. Enforcing, FBE, authentifiziertes ADB, dm-verity und die drei
Telefonie-Overrides sind bestätigt. Die vorherige ext4-Kontextverweigerung
ist überwunden. Der Broker scheitert nun mit `private base anchor errno=22`;
die aufgezeichneten Runtime-AVCs sind leer. Beleg:
`out/full-build-a187a309/boot-1/boot-health.json`.

Der gepinnte Kernel `50eb8d5d443b43f38d6e72f005f1b8601ac88a05`
markiert die `fsmount`-Dateibeschreibung mit `FMODE_NEED_UNMOUNT`.
Ihr letztes Schließen ruft `dissolve_on_fput()` auf, auch wenn eine separat
geöffnete Wurzel noch einen Pfadverweis hält. `move_mount()` weist diesen
nicht mehr eingebundenen Mount anschließend mit `EINVAL` zurück. Der
Basisöffner muss deshalb den ursprünglichen Mount-Deskriptor zurückgeben;
der lesbare Deskriptor dient nur der unveränderten Labelprüfung.

Zwei neue native Gerätetests sind vorbereitet: ein tatsächlich abgetrennter
tmpfs-Mount reproduziert den Verlust beim Ersetzen des Besitzers; ein Test
des echten Basisöffners prüft die unveränderliche ext4-Datei, private
Einbindung, persönlichen ID-Mount und abgewiesenen Schreibzugriff. Sie sind
noch nicht kompiliert oder ausgeführt. Kein persönlicher AOSP-Benutzer wurde
im neuen Gast angelegt und GNU-Programme laufen weiterhin nicht.

## Terminal- und Basisvorbereitung: 026665fb auf Image 030dd177

Der Komponentenlauf `identity-20260929T030627Z-026665fb-TMiVTr` kompiliert
CLI, Dienst, JNI, Broker und Policy einschließlich der Verbotsprüfungen.
Die Korrekturen betreffen den öffentlichen widerrufbaren Terminalkanal,
ueventds asynchrone Loop-Gerätedatei und den Kernel-Lesezugriff auf die
unveränderliche Basis. Der vorherige Lauf `05bdef02` ist wegen des unter
Bionic fehlenden `explicit_bzero` fehlgeschlagen; der Ersatz verwendet
`memset_explicit` aus dem gepinnten Android.

Nach [GitHub-Export und Verifikation](https://github.com/simgero/AegisOS/releases/tag/components-20260929T030923Z-026665fb-026665fb-sJgvbF)
bestehen im separaten lokalen Profil `runtime-030dd177` **114/114 native
Tests in 18 Suiten und 62/62 Java-Tests**, ohne Filter oder übersprungene
Tests. Der Gast verwendet Enforcing, FBE und authentifiziertes ADB. Nur die
Testprogramme und das Test-APK wurden in den Entwicklungsbereich übertragen;
Systemdienst, JNI und Policy des alten Images wurden nicht ersetzt.

Der Broker dieses alten Images scheitert weiterhin beim Öffnen der Basis.
Diese Komponentennachweise ersetzen daher weder den neuen Produktionspfad
noch Passwort-/Rohmodusprüfungen der öffentlichen CLI, GNU-Ausführung oder
Zwei-Benutzer-Isolation.

- Rohbelege: `out/components-026665fb/component-tests/`.
- Native-Log SHA-256: `fb7e22c651f451849b305e42ea46a76b875f80a216bde0936d67a7fc1fe63fc4`.
- Java-Log SHA-256: `2d0664d0f8f39d609b3fedfdda32c1d8a4baa96170a48646b8934a0f7acc952b`.
- Gastbeleg SHA-256: `e1b02a074ce729bd0c9240e94e6e5fa7ff478243b2243fb81b3f677f4e055387`.

Der anschließende vollständige Build
[`aosp-20260929T031210Z-026665fb-02ff1cf0`](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T031210Z-026665fb-02ff1cf0)
ist mit `UPLOAD_VERIFIED` abgeschlossen. Nach Prüfung aller 21 Assets und
der AVB-Kette bootet das neue Profil `runtime-026665fb` mit Enforcing,
FBE, authentifiziertem ADB, tatsächlichen Verity-Tabellen und allen drei
Telefonie-Booleans auf false. Der alte Testgast wurde geordnet mit bestätigtem
Android-Powerdown und sauberem KeyMint-Helper beendet; seine Daten bleiben erhalten.

Der neue Basis-Wartepfad erreicht beim ersten Start die ext4-Superblock-Erzeugung,
ohne den früheren Kernel-FD-AVC. Dort scheitert er mit `errno=13` und
`aegis_runtime_broker -> aegis_runtime_base_file:filesystem relabelfrom`.
Im gepinnten Kernel `50eb8d5d443b43f38d6e72f005f1b8601ac88a05` setzt
`selinux_set_mnt_opts()` erst die Superblock-SID aus `context=`, anschließend
prüft `may_context_mount_inode_relabel()` erneut `relabelfrom` auf dieser SID.
Die bereits vorhandene AOSP-Regel `allow fs_type self:filesystem associate`
deckt die folgende Zuordnungsprüfung ab. Deshalb wird ausschließlich die
fehlende Berechtigung für Broker und Basis-Dateisystem ergänzt. Kein permissiver
Betrieb und keine Regeländerung im laufenden Gast. Die neue Policy braucht
einen weiteren vollständigen Build und Bootnachweis; Native-/Java-Quellen und
Tests bleiben identisch. Rohbeleg: `out/full-build-026665fb/boot-1/boot-health.json`.

Im laufenden vollständigen Image `026665fb` wurden zusätzlich die tatsächliche
CLI und JNI-Passworteingabe geprüft. Ein unbekannter persönlicher Benutzer
bleibt abgewiesen und das Terminal unauthentifiziert; der zufällige Testwert
wird nicht zurückgeschrieben und fehlt in den drei erfassten Gastlogs.
`linux start`, `status`, `stop` und `shell` werden ohne Anmeldung zurückgewiesen.
Ein separater Strg+C-Test an der aktiven Passwortabfrage bestätigt identische
Termios-Werte vor und nach der CLI sowie deren Exitcode 130. Der umgebende
su/ADB-Prozess erhält dasselbe PTY-Signal und endet ebenfalls mit 130; die
erste Testfassung hatte dort fälschlich 0 erwartet. Der korrigierte Test prüft
weiterhin den separat gemeldeten CLI-Exitcode und die exakte Wiederherstellung.
Es wurden keine persönlichen Benutzer angelegt. Das sind öffentliche
Negativ-/Passwortmodus-Tests, kein AOSP-Passwortnachweis und kein GNU-Rohmodus-,
Terminalwiderrufs- oder Isolationsnachweis. Belege:
`out/full-build-026665fb/identity-test/negative-result.json` und
`interrupted-password.json` im selben Verzeichnis.

## Init-Namespace-Übergabe: Komponenten 3b350e74 auf Image 4e53dc18

Am 29. September 2026 bestehen **114/114 native Tests in 18 Suiten**, ohne
Filter oder übersprungene Tests. Lauf `identity-20260929T014019Z-3b350e74-FRvHfp`
kompiliert die Komponenten und die NSFS-Policy erfolgreich; der
[Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260929T014924Z-3b350e74-3b350e74-d9cLz4)
wurde mit geprüften Artefakten über GitHub übertragen. Die neue Prüfung
weist falsche, vertauschte oder ausgetauschte Namespace-Handles zurück und
prüft die Freigabe temporärer Deskriptoren. Die positive wiederholte Übergabe
derselben Identität besteht; der vorhandene Fork-Test weist eine erneute
Besitzbindung im Kind zurück.

Der Test lief im separaten lokalen Profil `runtime-4e53dc18`, mit Bootabschluss,
authentifiziertem ADB und SELinux Enforcing. Eigene Test-Cgroups bleiben nicht
zurück. Es gibt nur Systembenutzer 0; der produktive Broker dieses alten
Images ist weiter gestoppt. Entwicklungs-root-Fixtures beweisen weder die
neue NSFS-Policy im Gast noch den Init-gestarteten Dienst oder eine GNU-Sitzung.
Unveränderte Java-Tests wurden nicht erneut ausgeführt.

- Rohbelege: `out/components-3b350e74/native-tests/`.
- Native-Log SHA-256: `5a9a08697c5b62c027d776dbcee4261623d7b2132055c0628ae7f57b73f13412`.
- Gastbeleg SHA-256: `3ac12c40d8263be284d58182c688263fa969cea44f32191a184388727dbbc419`.

Der folgende Image-Commit `afaf6462` ändert gegenüber diesen getesteten
Komponenten ausschließlich die benannte Cgroup-Type-Transition samt
Dateisystemzuordnung und deren Dokumentation. Native Quellen und Tests sind
identisch. Die zusätzliche Policy muss im vollständigen Build kompiliert
und mit dem tatsächlichen Dienst geprüft werden; sie ist kein Ergebnis
dieses Komponentenlaufs.

## Komponentenstand 4e53dc18 auf dem verwalteten Testimage a8d38b97

Am 29. September 2026 bestehen **113/113 native Tests** aus
`4e53dc180da1129a06b7b670f364c5ab0cf90e72`, ohne Filter oder übersprungene
Tests. Komponentenlauf `identity-20260929T005029Z-4e53dc18-mPGAUM` ist
erfolgreich kompiliert; sein
[Release](https://github.com/simgero/AegisOS/releases/tag/components-20260929T005737Z-4e53dc18-4e53dc18-3W0vyB)
wurde über GitHub mit geprüften Artefakten übertragen. Das lokale Profil
`runtime-a8d38b97` bootet mit authentifiziertem ADB, FBE, Verity und SELinux
Enforcing. Es enthält keine persönlichen Benutzer; sein produktiver Broker
ist nach dem bereits dokumentierten Startabbruch gestoppt.

Die Tests prüfen weiterhin Entwicklungs-root-Fixtures mit eigenen temporären
Ressourcen. Sie bestätigen auch nach Ergänzung der phasenbezogenen Diagnose
das bisherige native Verhalten, nicht den Start des echten Dienstes, die
Helferprüfung im Broker oder eine persönliche GNU-Sitzung. Das unveränderte
Image enthält die neuen Broker-Diagnosen noch nicht. Java wurde in diesem
Lauf nicht erneut getestet.

- Rohbelege: `out/components-4e53dc18/native-tests/`.
- Native-Log SHA-256: `56254b4ff8ae0b836193567f908409a7a267b4e4fabac352125822fe352c949f`.
- Gastbeleg SHA-256: `38be475a91518d789ae8308dd1da275899c73b03d05df4898a265b22ea678051`.

## Komponentenstand a8d38b97 auf dem geprüften Image 2a766ab5

Am 29. September 2026 bestehen **113/113 native Tests**, ohne Filter oder
übersprungene Tests, im lokalen Mac-QEMU mit SELinux Enforcing und
authentifiziertem ADB. Die auf `aegis-build` erzeugten Komponenten stammen aus
`a8d38b97b8e3c8fb15365327fdadb04506baa019`, Lauf
`identity-20260929T000603Z-a8d38b97-WVXn2V`. Der
[Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260929T001323Z-a8d38b97-a8d38b97-RodhFL)
wurde über GitHub übertragen und vor Ausführung auf seine Prüfsummen geprüft.
Das gebootete Image bleibt unverändert `2a766ab5`; keine Framework-Dateien
wurden ersetzt. Dieser Lauf enthält keine erneute Java-Testausführung.

Die zwei neuen Cgroup-Tests bestätigen, dass Androids Elternbereich
`system:system 0775` akzeptiert wird, private Gruppen weiterhin zwingend
`root:root 0700` verlangen und abweichende Eigentümer oder zu offene Modi vor
dem Anlegen eines Runtime-Unterbaums scheitern. Die Tests verändern nur ihren
eigenen temporären Unterbaum, nicht den wirklichen Android-Cgroup-Elternbereich.

- Rohbelege: `out/components-a8d38b97/native-tests/`.
- Native-Log SHA-256: `131fa1d21bf9831afc6185109164cad3f9e2b2454b9d21089b1a237171bab761`.
- Gastbeleg SHA-256: `ae923c374233a284952958c950f4958e07066e8b45c64405d887642ec3f88845`.

Diese Entwicklungs-root-Fixtures beweisen weiterhin keine Ausführung in den
neuen produktiven SELinux-Domänen. Das gebootete Produkt bleibt `absent`;
der verwaltete Dienststart und eine persönliche GNU/Linux-Sitzung sind offen.

## Vollständiges Image und Komponentenstand 2a766ab5

Am 29. September 2026 bestehen im neuen lokalen Mac-QEMU **111/111 native
und 62/62 Java-Tests**, ohne Abwahl. Image und Komponenten stammen beide aus
`2a766ab5d6a6ef5ed4c01fd87fa4e97bb2e2961d`. Der
[vollständige Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260928T224751Z-2a766ab5-ed1329db)
wurde nach `UPLOAD_VERIFIED` über GitHub bezogen und lokal auf Prüfsummen,
Eingangsbelege und AVB-Ketten geprüft.

Die Product-RRO ist tatsächlich aktiv: `config_sms_capable`,
`config_voice_capable` und `config_force_phone_globals_creation` sind false.
Der zuvor fehlgeschlagene, unveränderte Produktkonfigurationstest besteht nun.
Bootabschluss, authentifiziertes ADB, SELinux Enforcing, FBE und tatsächliche
dm-verity-Tabellen mit `restart_on_corruption` sind bestätigt. Die installierte
AEGIS-CLI antwortet und verweigert alle drei Linux-Lebenszyklusbefehle, solange
der Produktmodus `absent` ist.

- Profil: `out/qemu-profiles/foundation-2a766ab5`.
- Bootbelege: `out/full-build-2a766ab5/boot-1/`.
- Vollständige Testergebnisse: `out/full-build-2a766ab5/component-tests/`.
- Native-Log SHA-256: `e6e81922c8b930e2703611f76264d464d6045a8e7578f59250118201a1db389c`.
- Java-Log SHA-256: `7a526a3fef77b23f93a5f2cd0452b640fdcf51873a9b78439fe4598325d23fd5`.

Diese Komponentenprüfung führt weiterhin keine produktive persönliche
GNU/Linux-Sitzung aus. Die nativen Tests laufen als Entwicklungs-root;
produktive Runtime-SELinux-Domänen, Paketverwaltung und der gesamte
Zwei-Benutzer-Linux-Ablauf bleiben unbewiesen.

## Komponentenstand 2a766ab5 auf bfe90925

Am 29. September 2026 wurden die auf `aegis-build` kompilierten Komponenten
`2a766ab5d6a6ef5ed4c01fd87fa4e97bb2e2961d` im getrennten lokalen
`runtime-bfe90925`-QEMU geprüft: **111/111 native, 61/62 Java-Tests**.
Die Sitzung verwendet SELinux Enforcing, authentifiziertes ADB und unveränderte
Images von `bfe90925`. Es wurde kein Test abgewählt. Die einzige Java-Abweichung
ist `config_sms_capable=true` aus der geerbten Vendor-RRO dieses Images.

Der unmittelbar vorhergehende Komponentenstand `ac87df1f` hatte 106/110 native
Tests bestanden. Vier positive Terminaltests öffneten Androids Tmpfs-`/dev/ptmx`
statt des vom echten Supervisor verwendeten Devpts-Knotens. Die Fixture wurde
korrigiert und ein zusätzlicher Test für die fortgesetzte Legacy-Ablehnung
hinzugefügt; der produktive Terminalprüfer wurde nicht gelockert.

Drei neue Java-Tests bestätigen, dass widerrufene interne Sitzungsbindungen auch
nach einer anderen Anmeldung ungültig bleiben, mehrere gültige Anmeldungen
bis zum Widerruf bestehen können und fremde Gate-/geschlossene Scope-Bindungen
abgewiesen werden. Dabei kommt ein simulierter Quiescer zum Einsatz.

- Build: `identity-20260928T223826Z-2a766ab5-sutWPl`.
- [Geprüfter Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T224549Z-2a766ab5-2a766ab5-u7YQ7B).
- Lokale Rohbelege: `out/components-2a766ab5/component-tests/`.
- Native-Log SHA-256: `7e01d37b9e025b3d579233329e932063adac73f12ef5fa19db837cbc698f5ee2`.
- Java-Log SHA-256: `cb46cc4cb258be79e7fff0e43b150af201439bdf3b819632163688f72f9809a6`.

Die tatsächlich angebundenen verwalteten Dienstpfade sind kompiliert, aber im
Produkt weiterhin deaktiviert. Diese root-/Komponententests beweisen weder
produktive SELinux-Domänen noch persönliche Debian-Ausführung, Paketverwaltung
oder vollständigen CE-Ressourcenabbau. Ältere Ergebnisse unten gelten nur für
ihre jeweils genannten Stände.

Stand: 28. September 2026. Einzelne Module und der separate CLI-Ablauf sind
geprüft; eine persönliche GNU/Linux-Sitzung ist noch nicht integriert.

## Korrigierter nativer Lauf `bfe90925`

Im späteren vollständigen Image desselben Commits, Lauf
`out/full-build-bfe90925/boot-1`, bestehen erneut **88 von 88 native Tests**.
Von **53 Java-Tests bestehen 52**. Die zusätzliche Prüfung
`productDoesNotForceCellularInitializationWithoutAModem` findet
`config_sms_capable=true`: Die aktivierte Vendor-RRO
`android.cuttlefish.phone.overlay` überschreibt den bereits auf false gesetzten
Framework-Wert. Die beiden anderen Telefonie-Booleans sind false und sämtliche
Telefonie-Features fehlen wie vorgesehen. Es wurde kein Test abgeschwächt.
Eine gezielte Product-RRO ist vorbereitet, noch nicht im Image gebaut oder
im Gast geprüft. Nachweise: `out/full-build-bfe90925/component-tests/` und
`out/full-build-bfe90925/boot-1/sms-overlay.txt`.

Alle **88 nativen Tests bestehen**, ohne Abwahl, im lokalen Mac-QEMU-Lauf
`out/components-7c09d1f/boot-1` auf dem vollständigen Image `25fde995`.
Die geprüften ARM64-Module stammen aus Commit
`bfe90925aa1873761f091a74a70e2ee1cf7b8bf9`, Builderlauf
`identity-20260928T205138Z-bfe90925-XDN6PJ`, InvocationID
`ab79d182a8684ad4bd5797097c4d4174`. Der
[Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T205942Z-bfe90925-bfe90925-LhHoqY)
wurde über GitHub transportiert und auf dem Mac geprüft.

Die drei zuvor fehlgeschlagenen Mounttests bestehen nach der privaten
Broker-Einhängung. Zwei zusätzliche Tests bestätigen die Namespace-Bindung
und den zunächst abgewiesenen detached Mount, der erst nach der kontrollierten
Einhängung geklont werden kann. Androids eigener Mount-Baum bleibt unverändert.
Bootabschluss, authentifiziertes ADB, Kernel und SELinux Enforcing wurden vorab
geprüft. Nachweise: `out/components-7c09d1f/native-tests/` mit Gastzustand,
vollständigem Rohlog und SHA-256-Ergebnisdatei.

Dies sind Entwicklungs-root-Tests mit einer kleinen Tmpfs-Basis. Sie beweisen
weder die produktiven SELinux-Domänen noch einen tatsächlichen Debian-Start,
persönliche Linux-Isolation oder Paketinstallation. Die neue Telefonie-
Produktprüfung ist in diesen Komponenten enthalten, verlangt aber das neue
vollständige Image; Java wurde deshalb hier nicht erneut ausgeführt.

## Vollständiger Komponentenlauf im gebooteten Image `25fde995`

QEMU-Lauf `out/full-build-25fde995/boot-1` meldet `sys.boot_completed=1` und
SELinux Enforcing. Derselbe Komponentenstand `336e9275` wurde erneut gegen das
vollständig gestartete Produkt geprüft: **52 von 52 Java-Tests und 83 von 86
nativen Tests bestanden**. Die drei unten beschriebenen Basis-Mount-Fehler
bleiben unverändert. Die vier bisher ausgenommenen Produktkonfigurationstests
bestätigen nun die installierten Kennungsregister, die genau einmal konfigurierte
AEGIS-Dienstklasse, mindestens vier Benutzerplätze und die ausgeschlossenen
Funkfunktionen. Nachweise: `out/full-build-25fde995/component-tests/`.

Zusätzlich erreichen die installierten CLI-Befehle `aegis user list` und
`aegis status` den echten Dienst: keine persönlichen Benutzer, Terminal nicht
angemeldet, Ersteinrichtung verfügbar, Runtime nicht installiert. Das ersetzt
keine interaktive Passwortprüfung. Der spätere, separate
[CLI-Gasttest](identity-cli-qemu-test.md) bestätigt inzwischen zwei persönliche
Benutzer samt Passwortwechsel, Abmeldung und geordnetem Neustart.

## Früherer vollständiger nativer Lauf mit dem neuen Kernel

Im Image `be1ad9ad` wurden alle **86 nativen Tests des Komponentencommits
`336e9275`** ausgeführt: **83 bestanden, drei fehlgeschlagen, keiner abgewählt**.
Lokaler Mac-QEMU, Kernel `6.12.18-android16-1-maybe-dirty-4k`, authentifiziertes
ADB und SELinux Enforcing. Androids Framework-Boot bleibt wegen der separat
beschriebenen Audiokonfiguration unvollständig; hier wurden ausschließlich
native Kernel-/Dateisystembausteine geprüft, keine Java- oder Anmeldetests.

Neu positiv belegt sind die tatsächlichen Namensräume, getrennte Host-IDs
zweier gleichzeitig lebender Kinder, geschlossene Startfreigabe, private
Geräteansichten, reservierte Bionic-Kennungen und der kombinierte Speichergruppen-/
Namespace-Start. Alle fünf CE-Negativtests bestehen nun einschließlich der
Tmpfs-Seriennummernattribute. Dies sind Tests als Entwicklungs-root, kein Nachweis
produktiver SELinux-Domänen oder persönlicher Linux-Sitzungen.

Drei Basis-Mount-Tests scheitern weiterhin mit `EINVAL`:

- `PreparedMappingsKeepExecBlockedWhileBaseMountIsBuilt`
- `TwoViewsKeepSharedInodesWithSeparateUserOwnership`
- `WritableOrNonDirectorySourcesAreRefusedWithoutMutationOrLeaks`

Der gepinnte Kernel ruft bei `open_tree(OPEN_TREE_CLONE)` `__do_loopback` auf,
das über `check_mnt` einen Quellmount im aktuellen Mount-Namespace verlangt.
Die Fixtures liefern einen detached `fsmount`; auch `aegis_base_open` liefert
gegenwärtig einen solchen Mount. Die Besitz-/Namespace-Anbindung muss vor der
Runtime-Aktivierung korrigiert werden. Prüfungen wurden nicht gelockert und
kein alternativer `chroot`-Betrieb eingeführt.

Nachweise: `out/full-build-be1ad9ad/native-tests-before-boot/` mit Rohlog,
Gastzustand, Ergebnis und SHA-256. Android und der gepaarte Schlüssel-Helper
wurden anschließend mit bestätigtem Powerdown beendet.

Die folgenden Abschnitte halten frühere, eingeschränkte Läufe fest.

## Herkunft

- Buildcommit: `9d3295106d7c6f4841ab5db1bf29a8cd085cccf3`.
- Builderlauf: `identity-20260928T143037Z-9d329510-HLLRGJ`,
  InvocationID `2d1f199b35a0432287e727df2208c4eb`.
- AOSP meldete nach 3:14 Minuten erfolgreichen Abschluss. Status:
  `IDENTITY_COMPILED_NOT_INSTALLED`; 16 Artefakte wurden eingesammelt.
- [Geprüfter GitHub-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T143513Z-9d329510-9d329510-RPcQHk).
  Exporter und Mac haben Transport, Manifest und Datei-Prüfsummen geprüft.
- QEMU-Lauf `out/qemu-first-boot/components-9d32951`, authentifiziertes ADB
  auf `127.0.0.1:15655`, Bootabschluss `1`, SELinux `Enforcing`, FBE `file`.
  Weiterhin das bisherige Android-Image und dessen Kernel; kein Austausch
  von Framework-JARs im laufenden Gast.

## Tatsächliche Ergebnisse

| Gruppe | Ausgeführt | Bestanden | Fehlgeschlagen | Nicht ausgewählt |
| --- | ---: | ---: | ---: | ---: |
| Java/Android | 40 | 40 | 0 | 4 |
| Native ARM64 | 35 | 32 | 3 | 16 |

Die Java-Auswahl umfasst `CredentialTransportTest`, `InitialAdminRecoveryTest`,
`RuntimeUidMapTest`, `RuntimeAdmissionTest` und `AegisRuntimeStorageTest`.
Sie prüft unter anderem Parcel-Kopien, Wiederaufnahme, Zuordnungsrechnung,
Serialisierung und Fehlerbehandlung innerhalb der Testanwendung. Vier
`ProductConfigurationTest`-Tests verlangen die Integration im neuen Image
und wurden ausdrücklich nicht ausgewählt.

Die nativen Tests prüfen Kontrollnachrichten, Descriptor-Übergaben, das
Abweisen direkter Helferstarts, Seccomp, Kindprozessbeobachtung sowie
Speicher- und Setup-Fehlerpfade. Die 15 Namespace-Tests und der Test der
installierten AOSP-Kennungsregister wurden nicht ausgewählt; ihnen fehlen
im alten Image die benötigten Voraussetzungen.

Drei ausgeführte `RuntimeCe`-Tests scheitern beim Anlegen beziehungsweise
Lesen ihrer Tmpfs-Seriennummernattribute mit `EOPNOTSUPP`:

- `MissingOrNonCanonicalSerialIsNeverRepairedOrAccepted`
- `ReusedNumberWithAnotherSerialIsRejectedWithoutMutation`
- `PlausibleDirectoryNamesAndSerialDoNotSubstituteForEncryption`

Der Gast bestätigt `# CONFIG_TMPFS_XATTR is not set`. Der geplante Kernel
fordert diese Option nun ausdrücklich an; die drei Tests müssen danach
erneut bestehen. Ihre Voraussetzungen werden nicht durch Überspringen,
Lockerung der Prüfungen oder Ausschalten von SELinux ersetzt.

## Nachvollziehen und Grenzen

Die lokalen Nachweise liegen unter `out/component-tests-9d329510/`:
`java.log`, `native.log`, `guest.txt`, `result.json`. Letzteres enthält Zähler,
Commit, Release, Grenzen und SHA-256 der drei Rohprotokolle.

```sh
adb -s 127.0.0.1:15655 shell am instrument -w -r \
  -e class org.aegisos.identity.CredentialTransportTest,org.aegisos.identity.InitialAdminRecoveryTest,org.aegisos.identity.RuntimeUidMapTest,org.aegisos.identity.RuntimeAdmissionTest,com.android.server.aegis.AegisRuntimeStorageTest \
  org.aegisos.identity.tests/androidx.test.runner.AndroidJUnitRunner
adb -s 127.0.0.1:15655 shell su 0 \
  /data/local/tmp/aegis-components-9d329510/AegisRuntimeNativeTests \
  '--gtest_filter=-RuntimeNamespace.*:RuntimeRegistry.*'
```

Der Testgast und sein KeyMint-Hilfssystem wurden danach geordnet beendet;
beide Dateisysteme sind ausgehängt. Vor einer Wiederholung muss dasselbe
Profil erneut gestartet und ADB verbunden werden.

Dies belegt weder den Systemserver-Dienst noch AEGIS-Passwortanmeldung,
Runtime-Start, SELinux-Übergänge oder Isolation zweier persönlicher Benutzer.
Diese Nachweise folgen mit dem vollständig integrierten Image.

## Zweiter Lauf: Speichergruppen und Ressourcenbesitz

Commit `481f738c7417f644c01d08f5a1cb4426aa761eaa` wurde im Lauf
`identity-20260928T154101Z-481f738c-NDVInX` erfolgreich kompiliert
(InvocationID `3fdadaffe02b4620943459ee1a881045`, Buildzeit 7:43 Minuten).
Die nativen Module einschließlich F2FS-Korrektur, Speichergruppen und
zusammengesetztem Kontextbesitzer umfassen jetzt **63 kompilierte Tests**.
Der [zweite Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T154958Z-481f738c-96f9ed6b-WR0G9Q)
wurde vom Exporter zurückgeladen und verglichen sowie auf dem Mac geprüft
heruntergeladen und entpackt.

Im lokalen QEMU-Lauf `out/qemu-first-boot/components-481f738` wurden **elf neue
native Tests ausgeführt; alle elf bestehen**. Drei davon prüfen den Abbau
abgewiesener Starts, erhaltene Aufrufer-Deskriptoren, den Schutz vorhandener
Gruppen und Kontextbesitz nach direktem `clone3`. Acht prüfen tatsächliche
Speicher-Cgroup-Dateien und Grenzen, einmalige Freigabe, falsche Identität,
fehlende Controller, manipulierte Limits und getrennten Stopp zweier Kinder.
Eine leere Gruppe wird dabei ausdrücklich von einem verbrauchten Pidfd-Exit
unterschieden. Die direkten Clone-Tests erfassen Bionics geerbten PID-Cache.

```sh
adb -s 127.0.0.1:15655 shell su 0 \
  /data/local/tmp/aegis-native-481f738/AegisRuntimeNativeTests \
  '--gtest_filter=RuntimeMemoryGroup.*:RuntimeContext.*-RuntimeMemoryGroup.NamespaceIsAlreadyBoundedWhileExecGateIsClosed'
```

Dieser Lauf verwendet weiterhin den bisherigen Kernel. Der kombinierte neue
Namespace-/Cgroup-Test wurde deshalb nicht ausgewählt. Insgesamt sind in
diesem Lauf 52 native Tests nicht ausgewählt; die Java-Tests wurden nicht erneut
ausgeführt. Es wurde kein tatsächlicher CE-HOME-Start und kein produktiver
SELinux-Übergang geprüft. Das ist kein Speicherdruck-/OOM-Nachweis und keine
Abnahme persönlicher GNU/Linux-Sitzungen oder des vollständigen Logouts.

Der Gast meldete Bootabschluss `1`, `ro.adb.secure=1`, FBE `file`, globales
SELinux `Enforcing` und den Cgroup-Speichercontroller. Root-Gerätetests allein
belegen keine durchgesetzte Produktionspolicy des künftigen Brokers.
Alle eigenen Testgruppen wurden entfernt; Android und KeyMint-Helfer wurden
geordnet beendet. Android bestätigte das Aushängen von `/data` und `/metadata`,
der Helper `AEGIS_HELPER_SHUTDOWN_CLEAN`.

Lokale Nachweise: `out/component-tests-481f738/{native-memory.log,guest.txt,cleanup.txt,result.json}`.
SHA-256 des nativen Rohprotokolls:
`e657c2d6ecd9ce0fa79d1d893bef3ab63eb7d39d02303075a45cd77ebb8551b6`.
Das Ergebnis-JSON enthält auch die Hashes der Gast- und Bereinigungsprotokolle.

## Dritter Lauf: Brokerprotokoll, Wiederherstellung und Basisparser

Commit `878fc970ace1a19a7a6f637e8e7df3763536b649` wurde im Lauf
`identity-20260928T174855Z-878fc970-sllLJ3` vollständig gebaut und über den
[geprüften Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T175708Z-878fc970-878fc970-sYlkeV)
bezogen. Im lokalen QEMU bestehen **48 von 48 ausgewählten Java-Tests**
(vier Produktkonfigurationstests nicht ausgewählt). Die Auswahl enthält jetzt
auch `RuntimeBrokerProtocolTest`. Von **23 ausgewählten nativen Tests bestehen
22; einer schlägt fehl**. 63 weitere native Tests wurden nicht ausgewählt.

Bestanden haben neun Protokolltests, drei Besitztests, fünf echte Cgroup-
Wiederherstellungstests, ein Fristtest und vier der fünf Basisparser-Tests.
`RuntimeBaseImage.DuplicateUnknownAndTrailingDataCannotAuthorizeMount` nahm
eine unzulässige Variante an. Die gepinnte JsonCpp-Version überspringt an
bestimmten Objektpositionen Kommentare trotz `allowComments=false`. Commit
`336e9275` weist daher Schrägstriche außerhalb von JSON-Zeichenketten bereits
in der begrenzten lexikalischen Vorprüfung ab. Er ergänzt mehrere Kommentar-
positionen und einen zulässigen String mit URL/Kommentarsyntax als Regression.
Der korrigierte Komponentenlauf
`identity-20260928T181002Z-336e9275-y2tzSs` endete um 18:10:51 UTC erfolgreich;
im gesonderten Gast-Nachtest bestehen **alle fünf Parser-Tests**. Der
[neue Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T181151Z-336e9275-336e9275-V8Iq0V)
ist geprüft. 81 weitere native Tests und unveränderte Java-Tests wurden in diesem
gezielten Nachtest nicht wiederholt. Nachweis: `out/component-tests-336e9275/`,
einschließlich `native-base.log`, `guest.txt` und Hash-/Ergebnisdatei `result.json`.

Nachweise des fehlgeschlagenen Ausgangslaufs liegen unverändert in
`out/component-tests-878fc970/{guest.txt,native.log,java.log,cleanup.txt,result.json}`.
Die eigenen Cgroups wurden entfernt. Nach dem gezielten Parser-Nachtest
wurden Android und KeyMint-Helfer geordnet beendet: `/data` und `/metadata`
ausgehängt, Android Power-down und bestätigter sauberer Helper-Abschluss. Authentifiziertes ADB und SELinux Enforcing
sind bestätigt. Root-Komponententests belegen weiterhin weder Produktions-
SELinux-Regeln noch den aktivierten Broker, echte Basismounts oder persönliche
GNU/Linux-Sitzungen. Der neue Kernel ist noch nicht in diesem Gast gestartet.
