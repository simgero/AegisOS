# AOSP-Speicheroperationen und Runtime-Abbau

## Verwaltete CLI-Löschung vorbereitet

Nach dem unten dokumentierten echten Plattform-Nachweis wird die bisherige
pauschale Sperre im CLI-Service entfernt. Der bestehende Backendpfad verlangt
weiterhin eine angemeldete andere AOSP-Adminidentität, frische Passwortprüfung,
zulässige AOSP-Restrictions und unveränderte ID/Seriennummer. AOSP selbst führt
den bestätigten Stopp und Abbau innerhalb der reservierten Identität aus.
Der Service hält dabei keinen Runtime-Gate über Framework-Aufrufe hinweg und
führt nach Freigabe der ID keine zerstörende Nachbereinigung aus. Erfolg wird
erst nach bestätigter AOSP-Abwesenheit, Benutzerstopp und CE-Sperrung gemeldet.

Vier verweigerte Berechtigungsfälle und eine erfolgreiche Löschung bei laufenden
GNU-Jobs sind im lokalen Testtreiber vorbereitet. **Die CLI-Änderung ist noch
nicht gebaut oder im Gast ausgeführt.** Die ältere interne Plattformprüfung
beweist nicht diese frische Adminautorisierung. Die historischen Abschnitte
unten beschreiben die jeweilige damalige Sperre.

## Kennungen während desselben Systemserver-Laufs nicht recyceln

Die Quellprüfung nach dem erfolgreichen 73ddcb61-Durchlauf bestätigt eine
noch nicht durch diesen Test abgedeckte Grenze: `LockSettingsStrongAuth.removeUser()`
stellt eine Nachricht mit numerischer ID in die Main-Handler-Queue. Seine
Alarmrückmeldungen tragen ebenfalls nur diese ID. Gleichzeitig kann AOSPs
`getNextAvailableId()` bei erschöpftem Nummernraum `mRemovingUserIds` leeren
und alte, vollständig gelöschte IDs wieder vergeben. Das ist eine belegte
Quellkonstellation, kein beobachteter Angriff oder fehlgeschlagener Gasttest.

Die neue lokale Korrektur entfernt diesen Recycling-Fallback. Die vorhandene
AOSP-Reservierung bleibt für die Lebensdauer des Systemservers erhalten,
auch nach bestätigtem Entfernen von `UserData`. Bei Erschöpfung schlägt die
Anlage fehl, ohne Reservierungen zu verwerfen. Der manuelle Testschalter kann
im neuen Finalisierungspfad ebenfalls keine ID freigeben. Es entsteht keine
zweite Benutzerverwaltung. Nach Neustart können vollständig entfernte IDs
normal wiederverwendet werden; partielle Benutzer bleiben durch ihre
persistenten AOSP-Einträge reserviert und werden wie zuvor wiederhergestellt.

Diese Änderung ist im [Vollimage 927cf51d](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T153553Z-927cf51d-17d08c57)
kompiliert, über GitHub verifiziert und am 29. September um 16:01 UTC im lokalen
Gast mit Enforcing, FBE und tatsächlichem dm-verity gestartet. Die gezielte
Allocator-Gastprüfung ist im Komponentenstand `be9d0d54` um 16:05 UTC zusammen
mit allen bisherigen Java-Tests bestanden (91/91). Die Hostprüfungen kontrollieren Einbau
und Erhalt der Quellinvarianten, nicht das Verhalten eines erschöpften echten
AOSP-Allocators. In diesem Image bleibt verwaltete CLI-Löschung noch gesperrt.
Die Änderung behauptet weder synchrone
StrongAuth-Aufräumarbeiten noch eine umfassende Prüfung fremder nativer Dienste
über einen Systemserver-Neustart hinweg.

Für die gezielte Gastprüfung wurden drei Instrumentierungstests ausgeführt:
eine tatsächlich freie Lücke, ein erschöpfter Nummernraum mit mehr entfernten
IDs als der bisherigen Recent-Liste und ausschließlich entfernte persönliche
IDs. Wiederholte Vergabeversuche dürfen Reservierungen nicht abbauen.
`AegisIdentityTests` bindet dafür den tatsächlich integrierten `services.core`
ein, einschließlich der bisherigen AEGIS-Speicherklassen, ohne zweite Kopie
dieser Klassen. Die Fixture verwendet wie AOSPs ursprünglicher Recycling-Test
einen prozesslokalen `UserManagerService` mit eigenem App-Cache-Verzeichnis.
Sie bearbeitet ausschließlich dessen Testtabellen und erzeugt keine echten
persönlichen AOSP-Benutzer oder Schlüssel. Der komplette bisherige
Java-Testumfang wurde wegen des geänderten Test-APKs erneut ausgeführt.
Echte Benutzer, gestartete Sitzungen, CE-Zustände, Schlüsselverzeichnisnamen
und Runtime-Kontexte sind vor und nach den Tests identisch.
Die [Komponentenbelege](../docs/component-tests.md) enthalten APK-/Log-Hashes
und die genaue Trennung zwischen isolierter Fixture und live Systemserver.

## Plattformlöschung, Wiederherstellung und neue Identität: 73ddcb61

Der vollständige [Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T142612Z-73ddcb61-404e18ff)
enthält die unten beschriebene gezielte SELinux-Korrektur. AOSP-Kompilierung
einschließlich `neverallow`, Upload-Rückprüfung, alle 21 heruntergeladenen
Assets und AVB sind bestätigt. Ein GitHub-HTTP-500 betraf ausschließlich
`config.sh`; nur dieses fehlende Asset wurde erneut geladen, ohne Buildneustart.

Am 29. September 2026 wurde das separate lokale Profil
`e20922c4-e760-42d3-b65a-3b22b58c60a5` ohne sichtbares Fenster geprüft.
AVB-Digest: `3a628f2676209fa08a862a29b74871643098c7782bffa333070d8405737ece55`.
Beide ersten persönlichen Anmeldungen funktionieren ohne Aufwärmversuch,
mit verzögerter Sitzungsprüfung und tatsächlicher GNU-Ausführung. Die folgenden
Zeiten sind UTC:

| Prüfung | Tatsächlich beobachtet |
| --- | --- |
| Normale Plattformlöschung | Beta `11/11` wird um 15:03:36 bei laufendem GNU-Hintergrundprozess entfernt. Um 15:03:57 sind AOSPs Abschlussmeldung, fehlende CE-/DE-Schlüsselverzeichnisse, sämtliche geprüften Benutzer-Datenpfade und XML-Kopien sowie die bereinigten Benutzerlisten bestätigt. |
| Prozess- und Peer-Kontrolle | Betas Originalprozess `6022`, Startzeit `49654`, ist beendet. Alphas ursprünglicher Prozess `4902`, Startzeit `38669`, läuft weiter; sein GNU-Readback um 15:04:39 ergibt dieselben 1.024 Bytes. |
| Kontrollierter Abbruch | Ein leerer, eindeutig identifizierter Testordner im SP-Inventar führt bei Alphas Löschung um 15:06:09 zu `Credential-state inventory unconfirmed`, vor dem Protectorabbau. Alpha `10/10` bleibt partiell reserviert; Originalprozess und Kontext verschwinden, CE ist gesperrt. Eine wiederholte öffentliche Anfrage gibt die Kennung nicht frei. |
| Wiederherstellung | Nach Entfernung ausschließlich des leeren Testordners wird dasselbe Android-/KeyMint-Paar geordnet neu gestartet. Ohne neue Löschanfrage schließt AOSP Alphas Bereinigung ab; um 15:08:44 bestehen dieselben Abwesenheitsprüfungen. Beta bleibt ebenfalls entfernt. |
| Tatsächliche ID-Wiederverwendung | Ein zuvor regulär angelegter Administrator `12/12` meldet sich nach Reboot an. Mit frischer Admin-Passwortprüfung entsteht um 15:09:28 der neue Benutzer `11/13`; der Allocator wird nicht verändert. Betas altes Passwort wird abgewiesen, CE bleibt gesperrt und kein Kontext entsteht. |
| Neue GNU-Dateien | Nach Anmeldung mit dem neuen Passwort findet `11/13` weder Alphas noch Betas Probedatei. Eigene 1.024 Bytes lassen sich über GNU schreiben und bytegleich lesen. Beide verbliebenen Benutzer werden danach regulär abgemeldet. |

Die Boot-ID wechselt von `cccb9b83-f5fc-4cca-987f-179bacb92b39` zu
`a00a969b-093b-4072-b20a-1bf0a32c8fbe`. Profil und AVB bleiben gleich.
Die unabhängige Abschlussprüfung um 15:16:12 bestätigt ausschließlich Benutzer
0 gestartet und CE-entsperrt, keine persönlichen Kontexte, `populated 0`,
Enforcing, FBE, authentifiziertes ADB und tatsächliche Verity-Tabellen für
`system` und `system_ext`. Ein vorheriger Diagnoseversuch verwendete den
falschen Gerätenamen `system`; die korrigierte Prüfung nutzt `system-verity`.
Dabei wurde kein Gastzustand verändert. Sieben Bootlogs enthalten keines der
vollständigen generierten Testpasswörter. Der Treiber ist beendet und seine
Passwortpuffer sind verworfen; das Testprofil bleibt erhalten.

**Dieser Nachweis betrifft die AOSP-Plattformlöschung über Entwicklungs-root.**
Die verwaltete AEGIS-CLI-Löschung bleibt gesperrt. Eine doppelte öffentliche
Anfrage beweist keine manipulierte oder verspätete Rückmeldung. Wiederverwendung
nach Reboot deckt weder Allocator-Erschöpfung im selben Boot noch sämtliche
asynchronen numerischen Nacharbeiten ab. Ein Fehler nach bereits erfolgtem
Schlüsselabbau wurde hier nicht injiziert. Weaver-Zustände und eingeschriebene
Biometrie wurden nicht geprüft; die echten leeren HAL-Rückmeldungen schon.
Pakettransaktionen bleiben unimplementiert. Die unveränderten Komponentenbelege
Java 88/88 und native 128/128 wurden nicht als neue Testausführung ausgegeben.

Rohbelege: `out/full-build-73ddcb61/removal-test/`,
`identity-test/events-accepted.json` und beide Bootverzeichnisse. SHA-256:

- `removal-test/result.json`: `e913f8e20a21f9bf51b7d93da7473090a1b7fe5eb6515513eda5a571211fbe16`.
- `identity-test/events-accepted.json`: `d567c9eba3421adf713e3f57a557741f14615bdd490c43b30b2297f027ffc661`.
- Lokale Treibervariante: `7c8b13644c2563dfda7cb98eeed8749f7766df61af917ece1f7cf1540924c348`.

Der Neustart verwendet einen unveränderten lokalen Launcher-Abzug aus
`73ddcb61`, damit parallele Grafikänderungen die Testbedingungen nicht ändern.
Der sichtbare Launcher und alle früheren gekoppelten Profile bleiben erhalten.

## Leere Biometrie bestätigt, privater Abbau noch verweigert: 760474df

Der vollständige [Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T134115Z-760474df-4114780f)
ist auf dem Builder und nach dem GitHub-Download verifiziert. Profil
`1d116bce-a5cd-462b-92ed-47391c7f90ae`, Boot
`71e454f3-53a5-41e1-aeb0-81ad638bb1e1`, AVB-Digest
`7cccb2eb16084e9a23bb082adf6a4cb4151964affb6600767799e970f7b300cd`
bootet mit Enforcing, FBE, sicherem ADB und tatsächlichem dm-verity.
Beide ersten AOSP-Anmeldungen und tatsächliche GNU-Dateien/-Hintergrundprozesse
funktionieren. Das sichtbare Profil wurde nicht ersetzt.

Beim Entfernen Betas (`11/11`) am 29. September um 14:17:09 UTC bestätigt der
Fingerprint-Client die tatsächliche leere HAL-Antwort erfolgreich. LockSettings
gelangt nun über beide Biometrieprüfungen hinaus; der frühere Timeout ist damit
im realen Gast behoben. Danach scheitert der AOSP-Datenabbau: SELinux verweigert
`vold_prepare_subdirs` das `getattr` auf einer verschlüsselten Verzeichnisbenennung
mit Typ `aegis_runtime_home_file`. Sein `rm` scheitert, vold meldet
`/data/misc_ce/11` als nichtleer und die geprüfte Framework-Finalisierung behält
um 14:17:10.554 UTC den partiellen Datensatz samt Seriennummer reserviert.

Betas ursprünglicher Prozess `5957`, Startzeit `18962`, und sein Kontext sind
weg; CE ist gesperrt. CE-/DE-Schlüsselverzeichnisse und die übrigen geprüften
Datenverzeichnisse fehlen; Runtime-CE-Reste sowie primäre und Reserve-XML bleiben.
Eine doppelte öffentliche Löschanforderung gibt dieselbe Kennung nicht frei.
Alphas ursprünglicher Prozess `4883`, Startzeit `11708`, läuft weiter und seine
1024 GNU-geschriebenen Bytes bleiben identisch. Alpha wird danach regulär
abgemeldet; sein Prozess verschwindet und AOSP bestätigt ausschließlich CE `[0]`.
Das Paar bleibt als Fehlernachweis erhalten. Es wurden keine Schlüssel gelesen,
Reste erzwungen gelöscht oder Benutzer-Metadaten zurückgesetzt.

Die folgende, **noch nicht kompilierte oder im Gast geprüfte Policy-Korrektur**
gibt ausschließlich dem vorhandenen `vold_prepare_subdirs` das Durchlaufen und
Entfernen dieses dedizierten privaten Baums. Normale Dateien, Verknüpfungen,
FIFOs und Socket-Dateien erhalten nur `getattr`/`unlink`; die Verzeichnisse
zusätzlich die benötigten Lese-/Durchlauf-/Entfernungsrechte. Inhaltszugriff,
Erstellung und Relabeln werden durch `neverallow` ausgeschlossen. Keine neue
Capability, keine Lockerung für normale GNU-Prozesse und kein nachträglicher
privilegierter Ersatz-Löschpfad werden eingeführt. 18 Hostprüfungen zur
Quellintegration und zu Build-Eingaben bestehen; die echte Policy-Kompilierung
und der erneute Systemtest stehen noch aus.

Belege: `out/full-build-760474df/removal-test/`; `beta.json` SHA-256
`17c195a508551e9e638bb0351e011429df087710fff831fbe1834a68ca3a27f0`,
gezielter SELinux-/Bereinigungsauszug `beta-subdirs-focused.log`
`c691b0e8e76690fa58d2a6ffc2bcbaf0cc5597dda3e9069b4e8eb7aefab67a26`.
Künstlicher Inventarfehler und Neustart-Wiederherstellung wurden wegen dieses
realen Fehlers nicht ausgeführt. Verwaltete CLI-Löschung bleibt gesperrt;
Adminfreigabe, echte verspätete Rückmeldungen und ID-Wiederverwendung sind
weiterhin nicht abgenommen.

## Gasttest der reservierten Löschung: 2e27713e

Der vollständige [Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T124901Z-2e27713e-10dee9cb)
wurde am 29. September nach Build und GitHub-Rückprüfung lokal in einem eigenen
Profil ohne sichtbares Fenster gestartet. AVB, tatsächliches dm-verity, FBE,
Enforcing und der durch Init gestartete Broker bestehen. Beide neuen
Testbenutzer können sich beim ersten Versuch anmelden und tatsächlich GNU
ausführen. Das ist **noch keine erfolgreiche Benutzerlöschung**.

Um 13:20:43 UTC wird Beta (`11/11`) über den Entwicklungszugang von AOSP
entfernt, während sein echter GNU-Hintergrundprozess läuft. Der Originalprozess
`5961`, Startzeit `17005`, und sein Kontext werden beendet, CE 11 gesperrt.
Alphas Originalprozess `4881`, Startzeit `9742`, läuft weiter; sein späterer
GNU-Readback ergibt dieselben 1.024 Bytes. Danach wird auch Alpha regulär
abgemeldet. Beide Profile früherer Builds bleiben erhalten.

Die Bereinigung stoppt jedoch um 13:20:54 mit `Biometric removal unconfirmed`.
Der tatsächliche virtuelle Fingerabdruck-HAL meldet eine leere Antwort auf
`removeEnrollments(size:0)`. Der gepinnte AIDL-Handler übersetzt sie in
`onRemoved(null, 0)`; `RemovalClient` behandelt das als Fehler. Der
Fingerprint-Service reicht diesen Fehler nicht an den wartenden
`removeAll`-Empfänger weiter. Die neue Zehn-Sekunden-Grenze hält deshalb die
ursprüngliche AOSP-Kennung mit `partial=true` reserviert. Primär- und
Reservekopie des Benutzerrecords bestätigen das. Ein wiederholter öffentlicher
Löschauftrag gibt die Kennung ebenfalls nicht frei, obwohl `pm remove-user`
bereits „Success“ meldet. Der Test wertet diese Meldung nicht als Abschluss.

Die folgende **noch nicht im Gast geprüfte Korrektur** trennt eine tatsächlich
empfangene leere AIDL-Antwort von generischem `null`/Fehler: Nur bei leerem
Originalauftrag und leerem AOSP-Eintrag darf sie bestätigt werden. Nichtleere
Antworten und andere Consumer behalten ihre bisherigen Pfade; ein fehlender
Callback oder Timeout bleibt ein Fehler. Fünf weitere gepinnte AOSP-Dateien
werden dafür integriert. Schema 5 akzeptiert das bisherige Schema 4 nur mit
seinem exakten Dateisatz und übernimmt keine fremden Änderungen. Die 38
Hostprüfungen betreffen Quellintegration, Build-Eingaben und Richtlinie; sie
ersetzen weder Android-Kompilierung noch einen erneuten Systemtest.

Belege: `out/full-build-2e27713e/removal-test/`. `beta.json` hat SHA-256
`efadf137988e3811061dfb058e72f150ac9dc3381cefc4379146d3cc2e656b15`,
der zeitlich begrenzte HAL-/AOSP-Auszug `biometric-timeout.log`
`6be7453c5ae87f2bb93ad11109642ec82b8c61be40f37d00eedf361184622004`.
Die erste Metadatenprüfung musste für Androids binäres XML auf dessen
`abx2xml` mit Ausgabe nach stdout umgestellt werden; keine Metadaten wurden
dabei umgeschrieben. Der geplante künstliche Inventarfehler und Neustarttest
wurden wegen des vorher gefundenen echten Fehlers noch nicht ausgeführt.
Die verwaltete CLI-Löschung bleibt gesperrt; weder deren Adminprüfung noch
Stale-Callback-/ID-Wiederverwendungsfestigkeit sind damit abgenommen.

## Vorheriger Nachweis: Anmeldung, Logout und Persistenz

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
`UserDataPreparer`, `Installer` und die beteiligten LockSettings-/SP-Quellen.
Berichtsschema 4 übernimmt nur vollständig passende bisherige Nachweise der
Schemata 1/2/3. Neue unbekannte Dateiänderungen verhindern die Installation.
17 lokale Tests prüfen Integration und Migration;
sie führen keine Android-Dateioperation aus.

Komponentenstand `c239ab17` ist auf dem Builder kompiliert und über GitHub
verifiziert. Im lokalen Gast `2f29f0ac` bestehen 77/78 Java-Tests. Der einzige
Fehler ist das von SELinux verweigerte Anlegen einer Hardlink-Testfixture im
App-Prozess. Die Korrektur bereitet ausschließlich diese app-eigene Testdatei
über die vorhandene userdebug-Test-Shell vor; keine Policy wird gelockert.
Nachweis: `out/components-c239ab1/component-tests/`; Java-Protokoll SHA-256
`d0394a52a702210496defdae971d8eb2235cc94a5417030a8f2b42c72959413a`.

Die Korrektur und fünf Tests für Systemverzeichnis-Bereinigung sind im
[Komponentenstand 5ef3e01e](https://github.com/simgero/AegisOS/releases/tag/components-20260929T123300Z-5ef3e01e-5ef3e01e-Gi3Tkv)
gebaut und verifiziert. Im lokalen Gast `2f29f0ac` bestehen **83/83 Java-Tests**
(0,543 s). Java-Protokoll SHA-256:
`da2fa58a6af636703712f2de6757047cd6ba0886f5b452db74776e27e055d9a9`.
Die unveränderten nativen Quellen verwenden weiterhin den geprüften 128er-
Nachweis `d44ccb33`; sie wurden hierfür nicht erneut ausgeführt.

**Die bestätigenden Methoden sind in AOSPs Benutzerlöschung eingebunden und
kompiliert; der Vollimage-Nachweis dieser Löschroutine steht noch aus.**
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

Der folgende Quellstand ergänzt Bestätigungen in AOSPs
`LockSettingsService`, `SyntheticPasswordManager` und `SyntheticPasswordCrypto`:
fehlgeschlagene Keystore-Löschung, GateKeeper-Rückrufe oder Inventarlesefehler
behalten die Benutzerreservierung. Protector- und Profilalias-Löschung prüft
auch die anschließende Abwesenheit. Das ursprüngliche Inventar bleibt bis
zum bestätigten Schlüsselabbau erhalten. Legacy-Boot-/Wiederverwendungspfade
bleiben unverändert; `LockPatternUtils` ruft über den vorhandenen AOSP-
LocalService den neuen bestätigenden Pfad auf.

Die QEMU-Konfiguration meldet Face- und Fingerprint-Funktionen. Der neue Pfad
verlangt für beide tatsächlich `remaining=0`; Fehler, Unterbrechung und ein
Timeout nach zehn Sekunden führen zum Abbruch mit erhaltener Reservierung.
Eine fehlgeschlagene Hardware-/Enrollments-Abfrage gilt nicht als leere Liste.
Für Benutzer mit vorhandener Weaver-Zustandsdatei wird die Löschung vor dem
Protectorabbau abgewiesen, bis ein bestätigender Weaver-Pfad implementiert ist.
Die beiden bisherigen QEMU-Testbenutzer haben keine solchen Zustandsdateien.
Fünf zusätzliche Android-Tests prüfen die Inventarlesefunktion; insgesamt
88 Java-Tests bestehen im
[Komponentenstand 2e27713e](https://github.com/simgero/AegisOS/releases/tag/components-20260929T124701Z-2e27713e-2e27713e-tJgUtD)
im lokalen Gast `2f29f0ac` (0,430 s). Protokoll SHA-256:
`442387962fe5b54fe4b24515ab0b48b897bcfafc7f0385f2ff31af5d7bfec204`.
Die neuen Plattformpfade sind auf `aegis-build` kompiliert, aber noch nicht im
Gast installiert oder als tatsächliche Benutzerlöschung nachgewiesen. Boot-
Wiederherstellung und vorbereitete Benutzer verwenden eigene Arbeitsthreads,
damit das Warten auf Biometrie-Rückmeldungen deren Handler nicht blockiert.
Ein vollständiges Image für diesen Stand folgt hinter diesen Komponentenprüfungen.

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
