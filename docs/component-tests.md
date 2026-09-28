# Komponentenläufe in lokalem QEMU

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
