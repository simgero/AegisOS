# Komponentenläufe in lokalem QEMU

Stand: 28. September 2026. Einzelne Module geprüft, noch keine Abnahme der
installierten AEGIS-Dienste oder einer GNU/Linux-Sitzung.

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
