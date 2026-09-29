# AOSP-Speicheroperationen und Runtime-Abbau

Stand 29. September 2026: Die fünf AOSP-Hooks und der Controller sind im
lokal getesteten Image `2f29f0ac` mit `managed-v1` installiert. Zwei tatsächliche
GNU-Kontexte, Hintergrundbetrieb nach Benutzerwechsel und Bildschirmsperre,
Abbau beider ursprünglicher Hintergrundprozesse bei Logout und unabhängige
CE-Sperre sind geprüft. Die GNU-Dateien bleiben nach Neustart desselben
Android-/KeyMint-Paars und eigener Anmeldung bytegleich. Beide ersten
Anmeldungen vor und nach Reboot funktionieren direkt. Der frühere nachträgliche
Widerruf wurde mit korrigierter Anmeldereihenfolge nicht mehr beobachtet;
Widerrufe bei tatsächlicher Sperre und Wechsel bleiben nachgewiesen.
Paketoperationen und verwaltete Benutzerlöschung fehlen. Belege und Grenzen:
[GNU-Test](../docs/runtime-gnu-qemu-test.md).

## Erstmalige persönliche Home-Struktur

Der native Provisionierungspfad legt `Desktop`, `Documents`,
`Downloads`, `Pictures`, `Videos`, `Music`, `Books`, `.config`, `.local` und
`.cache` ausschließlich im leeren, noch root-eigenen Staging-Home an. Die
Verzeichnisse erhalten den tatsächlichen gemappten Eigentümer des jeweiligen
AOSP-Benutzers und Modus `0700`. Alle Pfade sind feste relative Namen; die
Verzeichnisöffnung folgt weder Verknüpfungen noch anderen Mounts.

Die Provisionierung prüft anschließend die unveränderte, von AOSP geerbte
CE-Policy jedes Verzeichnisses. Erst nach Metadatenprüfung, Synchronisation
und erneuter Prüfung des CE-Schlüsselstatus wird der vollständige Anker unter
seinem endgültigen Namen veröffentlicht. Bei Fehler bleibt das unveröffentlichte
Staging-Verzeichnis erhalten und wird nicht automatisch übernommen.

Ein bereits vorhandenes Home wird weiterhin nur validiert und geöffnet.
Benutzerseitig gelöschte, umbenannte oder ersetzte Standardordner werden nicht
zurückgesetzt. Die Layout-Hilfsfunktion lehnt nichtleere und nichtprivate
Staging-Verzeichnisse ab; sie ist keine Authentisierung und kein CE-Nachweis.
Vier zusätzliche native Tmpfs-Tests decken Eigentümerzuordnung, genaue
Erststruktur, Verweigerung bei bestehenden Daten/Verknüpfungen und ungültige
Identitäten beziehungsweise Staging-Metadaten ab. Kompiliert auf `aegis-build`
und lokal ausgeführt bestehen im Stand `d44ccb33` **128/128 native Tests**.
Die Layout-Fixtures verwenden unverschlüsseltes Tmpfs. Zusätzlich besteht
inzwischen der reale CE-/GNU-Nachweis im vollständigen `d44ccb33`: zwei
persönliche Erststrukturen, getrennte Eigentümer, Erhalt eigener gelöschter,
umbenannter oder durch Verknüpfungen ersetzter Ordner sowie unveränderte
Konfigurationsbytes nach Kontextstopp und Neustart desselben Android-/KeyMint-
Paars. Betas eigener Bestand bleibt von Alphas Änderungen unberührt.
Zeitpunkte, Grenzen und Rohbelege stehen im [GNU-Test](../docs/runtime-gnu-qemu-test.md).

## Warum eine vorgeschaltete Sperre erforderlich ist

Im festgelegten AOSP-Stand `android-16.0.0_r1` kann die Benachrichtigung über
gesperrten CE-Speicher erst nach dem Schlüsselentzug erfolgen.
`onUserStopping` allein bestätigt ebenfalls keinen vollständigen Ressourcenabbau.
Persönliche Linux-Prozesse, offene Dateien und Mountreferenzen müssen daher
bereits vor der eigentlichen Speicheroperation beendet beziehungsweise
geschlossen sein. AOSP bleibt allein für Passwörter und Schlüssel zuständig.

Der vorbereitete Integrator ergänzt `StorageManagerService` an fünf Stellen:

| Operation | Vertrag des Controllers |
| --- | --- |
| Schlüssel erzeugen, löschen oder CE sperren | Neue Runtime-/Paketaktionen für alle Seriennummern der numerischen Benutzer-ID sperren; laufende Transaktionen abschließen oder abbrechen; Prozesse beenden und einsammeln; sämtliche CE-, Mount-, Terminal- und Socketreferenzen schließen. Erst danach darf die AOSP-Operation beginnen. |
| CE entsperren oder Passwortschutz aktualisieren | Gegen konkurrierenden Schlüsselentzug serialisieren, ohne einen gültigen laufenden Kontext allein deshalb zu beenden. |

Die erworbene Sperre umfasst den ursprünglichen vold-Aufruf und AOSPs
Statusaktualisierung. Auch ein bereits als gesperrt gemeldeter CE-Speicher
darf im verwalteten Modus den vorgeschalteten Abbau nicht überspringen.
Fehlender Controller, unbekannter Modus oder fehlende Abbaubestätigung führen
zu einem Fehler. Der dauerhaft erforderliche Systembenutzer erhält keine
persönliche Runtime und bleibt von diesem Controller ausgenommen.

Das Freigeben der Sperre erlaubt **keinen** neuen Runtime-Start. Nach einem
Abbau muss eine spätere frisch autorisierte AOSP-Prüfung Benutzer-ID,
Seriennummer und tatsächlichen CE-Status erneut abgleichen. Ein Fehler lässt
die Zulassung gesperrt. Alle Wartezeiten müssen begrenzt sein. Der Controller
darf weder den Operationsmonitor des Identitätsdienstes erwerben noch in
Storage/LockSettings zurückrufen: Diese Sperren können beim Aufruf bereits
gehalten werden.

## Fehler bleiben sichtbar

Die Originalmethoden für Erzeugen, Löschen und Sperren protokollieren bestimmte
vold-Fehler, ohne sie ihrem Aufrufer weiterzugeben. Der Patch ergänzt die
Fehlerweitergabe. `UserController` behandelt fehlgeschlagene Sperrung ohne
Absturz seines Hintergrundthreads und ruft in diesem Fall keine erfolgreiche
`keyEvicted`-Bestätigung auf. Nach normaler Rückkehr prüft er zusätzlich den
von AOSP gemeldeten CE-Status. Das ist kein eigenständiger kryptographischer
Schlüsseltest und ersetzt den späteren Gastnachweis nicht.

## Gepinnte und nachvollziehbare Integration

[`aosp-storage-hooks.json`](aosp-storage-hooks.json) enthält die vollständigen
SHA-256-Werte der beiden Originaldateien aus dem festgelegten AOSP-Tag:
[StorageManagerService](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/StorageManagerService.java)
und [UserController](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/am/UserController.java).

`scripts/aosp/register-runtime-storage.py` prüft die Git-Originale und alle
Zieldateien vor dem ersten Schreiben. Fremde Änderungen und symbolische
Verknüpfungen werden abgewiesen. Ersetzte verwaltete Dateien bleiben unter
`out/aegis-runtime-storage/backups/` erhalten, außerhalb der Java-Quellsuche.
Eine unterbrochene Installation akzeptiert nur exakt bekannte Zwischenstände.

Der Bericht `runtime-storage-source.json` hält Eingaben und erzeugte Quellen
fest. Komponenten- und Vollbuild prüfen nach dem Kompilieren deren Hashes.
Der Komponentenlauf baut dafür auch `services` und sammelt `services.jar`.
Ein Vollbuild liefert den Bericht über GitHub mit; fehlender Bericht oder
veränderte heruntergeladene Bytes verhindern `UPLOAD_VERIFIED`.
Der Bericht bestätigt Quelltextzuordnung, keinen ausgeführten Abmeldevorgang.

## Tests und verbleibende Arbeit

Neun Hosttests prüfen echte Originalmethoden als reine Textfixtures,
Eigentümerschaft, Backups, Wiederholung, Unterbrechungen, Symlinks und
Berichtsprüfung. Zwei weitere Worker-Tests prüfen fehlende beziehungsweise
veränderte Release-Berichte. Sie kompilieren oder starten keinen Android-Code.
Die Quellintegration wurde außerdem mit den vollständigen gepinnten Dateien
in einem temporären Verzeichnis geprüft.

Die acht Android-Java-Tests für Reihenfolge, fehlgeschlagene Akquisition,
Modusprüfung, Threadbindung, einmalige Freigabe und Fehlerweitergabe bestehen
im 59er-Komponentenstand. Sie verwenden kontrollierte Provider; dies beweist
keinen tatsächlichen nativen Ressourcenabbau vor AOSP-Schlüsselentzug.

Die neu angebundene Verwaltung hält keinen Runtime-Gate während AOSP-Aufrufen.
Vor Logout sperrt sie die Zulassung und verlangt nativen Stopp; die anschließende
AOSP-Keyoperation erwirbt selbst nochmals ihre Storage-Lease. Neue Anmeldungen
müssen ihre eigene interne Bindung erhalten. Lifecycle-Callbacks widerrufen
Sitzungen ohne den Identitätsmonitor zu erwerben; beim gleichzeitigen Wechsel
verhindert ein atomarer Vergleich das Löschen der neuen Benutzerbindung.

Weiterhin offen sind öffentlicher GNU-Terminalbetrieb, Pakettransaktionen sowie reale
Start/Stop/CE-Rennentests.
Direkte privilegierte vold-Aufrufe durchlaufen die Java-Schnittstelle nicht.
Die Löschbereinigung nach Freigabe einer numerischen AOSP-ID muss in den noch
reservierten AOSP-Lebenszyklus verlegt werden. Für Benutzerlöschung bleibt
`requireRuntimeAbsent()` deshalb bestehen.

## Benutzerlöschung vor Freigabe der AOSP-ID

Der erste Implementierungsschritt ergänzt nun `AegisRemovalFiles` und zwei
ausdrücklich bestätigende Methoden in der gepinnten AOSP-`ResilientAtomicFile`.
Beim Schreiben werden die geöffneten Inodes mit den Dateinamen abgeglichen,
beide neuen Kopien geschrieben und synchronisiert und erst danach die alte
Backupkopie entfernt. Auch das Elternverzeichnis wird synchronisiert.
Datei-, Berechtigungs-, Unlink- oder Synchronisationsfehler werden weitergegeben.
Beim Löschen müssen Hauptdatei und beide Fallbackkopien fehlen; Verzeichnisse,
Symlinks und mehrfach verlinkte Dateien werden vor dem ersten Unlink abgewiesen.
Die vorhandene bestmögliche AOSP-fs-verity-Absicherung bleibt im neuen
Commitpfad erhalten. Bestehende normale AOSP-Aufrufer sind unverändert.

Der Quellintegrator pinnt zusätzlich `ResilientAtomicFile`, `UserManagerService`,
`UserDataPreparer` und `Installer`. Berichtsschema 3 übernimmt nur vollständig
passende bisherige Nachweise der Schemata 1/2. Neue unbekannte Dateiänderungen
verhindern die Installation. 15 lokale Tests prüfen Integration und Migration;
sie führen keine Android-Dateioperation aus.

Komponentenstand `c239ab17` ist auf dem Builder kompiliert und über GitHub
verifiziert. Im lokalen Gast `2f29f0ac` bestehen 77/78 Java-Tests. Der einzige
Fehler ist das von SELinux verweigerte Anlegen einer Hardlink-Testfixture im
App-Prozess. Die Korrektur bereitet ausschließlich diese app-eigene Testdatei
über die vorhandene userdebug-Test-Shell vor; keine Policy wird gelockert.
Nachweis: `out/components-c239ab1/component-tests/`; Java-Protokoll SHA-256
`d0394a52a702210496defdae971d8eb2235cc94a5417030a8f2b42c72959413a`.

**Der neue Quellstand bindet die bestätigenden Methoden nun in AOSPs
Benutzerlöschung ein; Kompilierung und Vollimage-Nachweis dafür stehen aus.**
Stop- und Broadcast-Rückmeldungen behalten das ursprüngliche `UserData` und
dessen Seriennummer. Getrennte einmalige Benachrichtigungs-/Abbauansprüche
verhindern doppelte numerische Löschwirkungen. Fehler behalten den partiellen
Eintrag samt Anspruch bis zur Boot-Wiederherstellung.

Vor LockSettings wird die Runtime stillgelegt. Anschließend müssen Schlüssel,
installd und vold den Abbau bestätigen. Ein sechster Storage-Hook schützt die
Datenlöschung und gibt vold-Fehler weiter. Systemverzeichnisse werden ohne
Folgen von Symlinks geleert; fehlgeschlagene Verzeichnislesevorgänge gelten
nicht als leere Verzeichnisse. Das ist auf feste, bereits stillgelegte AOSP-
Pfade beschränkt. Fünf zusätzliche Gasttests prüfen diesen Dateibaustein.

Alle numerischen Abschlussarbeiten erfolgen vor Freigabe von `UserData`.
Unter `mPackagesLock` verschwinden die drei Benutzer-XML-Kopien und wird die
Benutzerliste bestätigt ohne diesen Benutzer geschrieben, während dessen
Eintrag im Speicher weiterhin die ID reserviert. Erst danach wird er entfernt.
Die direkte nachträgliche vold-Löschung im Backend entfällt.

Frühe Boot-Bereinigung markiert weiterhin partielle Benutzer, führt den Abbau
aber erst bei `PHASE_BOOT_COMPLETED` aus: LockSettings würde ihn davor intern
nach numerischer ID aufschieben. Angeschlossene und gespeicherte abgetrennte
private Zusatzvolumes verhindern die Löschung. Diese Änderungen sind noch
kein realer Nachweis für Fehlerbehandlung, Wiederanlauf oder ID-Wiederverwendung.
Verwaltete CLI-Benutzerlöschung bleibt durch `requireRuntimeAbsent()` gesperrt.

Die folgende Prüfliste erklärt die abgedeckten Quellpfade und die noch
ausstehenden Systemtests. Sie ist kein bestandener Löschtest:

- `UserManagerService.removeUserState()` entfernt zunächst LockSettings,
  Schlüssel und Daten. Fehler beim Schlüsselabbau werden bisher abgefangen;
  `UserDataPreparer` und `StorageManagerService.destroyUserStorage()`
  verschlucken ebenfalls Fehler. Ein ungeklärter Abbau muss den partiellen
  AOSP-Benutzereintrag erhalten und darf nicht den Systemserver beenden.
- Stop- und Broadcast-Rückmeldungen verwenden nur die numerische ID.
  Sie müssen das ursprüngliche AOSP-Objekt und seine Seriennummer behalten
  und jede Finalisierung einmalig beanspruchen, bevor auch AM-Rückmeldungen
  oder andere Löschwirkungen ausgeführt werden. Das gilt ebenfalls für
  Bereinigung beim Booten und vorbereitete Benutzer.
- Der Runtime-Abbau muss vor LockSettings beginnen. Die Runtime-Lease wird
  vor den AOSP-Aufrufen geschlossen; diese erwerben für ihre Speicheraktionen
  eigene Leases. Eine geschlossene Lease öffnet die Runtime-Zulassung nicht.
- Die Benutzer-ID bleibt reserviert, bis Daten und Metadaten bestätigt
  beseitigt sind. `mRemovingUserIds` allein reicht nicht: Der ID-Allocator
  darf diese Liste bei Erschöpfung ausdünnen. Der weiterhin vorhandene
  `UserData`-Eintrag und die Sperrordnung von `mPackagesLock` vor `mUsersLock`
  müssen Wiederverwendung während numerischer Abschlussarbeiten verhindern.
- `ResilientAtomicFile.delete()` ignoriert die Ergebnisse beim Löschen von
  XML-Datei, Backup und Reservekopie. Diese Ergebnisse müssen vor ID-Freigabe
  geprüft werden. Der verzögerte `WRITE_USER_MSG` liest hingegen unter
  `mPackagesLock` den dann aktuellen `UserData`-Eintrag; er hält keinen alten
  Datensatz fest. Dafür ist keine spekulative Snapshot-Korrektur erforderlich.
- Die bisherige direkte vold-Nachbereinigung im AEGIS-Backend erfolgt erst
  nach Freigabe der ID. Sie muss durch die bestätigte Plattformbereinigung
  ersetzt werden, bevor verwaltete CLI-Löschung aktiviert wird. Angeschlossene
  und gespeicherte, aber getrennte adoptierte Privatvolumes müssen weiterhin
  vor der ersten irreversiblen Änderung ausdrücklich geprüft werden.

Die Quellprüfung und exakten Git-Blob-Hashes der zusätzlich betroffenen
Plattformdateien stehen in
`out/full-build-026665fb/removal-followup.json`. Eine Erweiterung des
Integrators muss den bisherigen Besitznachweis kontrolliert migrieren,
alle zusätzlichen Originaldateien pinnen und fremde Änderungen erhalten.
Nach dem Kompilieren bleiben reale Erfolgs-, Fehler-, Doppelrückmeldungs-
und ID-Wiederverwendungstests mit persönlichen Runtime-Kontexten erforderlich.
