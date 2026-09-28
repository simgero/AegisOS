# Privater AOSP-Kanal und Besitzer der Runtime-Kontexte

Stand 28. September 2026: **Native ARM64-Objekte und Java-Klassen auf dem Builder
kompiliert; noch nicht vollständig gelinkt, als Produkt gebaut oder im Gast ausgeführt.**
`broker_protocol.c`, `broker_owner.c` und `context.c` wurden aus `b0b3de6e`
mit den gepinnten Bionic-Headern und `-Wall -Wextra -Werror` übersetzt.
Die drei Java-Klassen einschließlich des privaten Kanals wurden aus
`ac699aab` gegen die tatsächlichen AOSP-Systemmodule und `framework.jar`
übersetzt (zehn Klassendateien). Der erste Java-Versuch fand zwei in
`OsConstants` nicht exportierte Socket-Flags; der korrigierte Code verwendet
den bereits nichtblockierenden Deskriptor und Bionics festes `MSG_NOSIGNAL`.
Diese Einzelprüfungen ersetzen weder Soong/DEX noch Geräteprüfungen.
Der laufende Vollbuild `96f9ed6b` enthält
diese späteren Änderungen nicht. Es gibt noch keinen installierten Broker-Daemon,
keinen aktiven Socket und keine Registrierung im Identitätsdienst.
`ro.aegis.runtime.mode=absent` bleibt bestehen.

## Zuständigkeit und Gegenstelle

`RuntimeBrokerConnection` ist ausschließlich für den durchsetzenden
`system_server` vorgesehen. Die Gegenstelle muss über kernelbestätigte
Socket-Credentials UID/GID 0 und den exakten SELinux-Kontext
`u:r:aegis_runtime_broker:s0` besitzen. Der native Empfänger prüft umgekehrt
UID/GID 1000 **und** `u:r:system_server:s0`. Die Socketadresse alleine gewährt
keine Berechtigung. Entwicklungs-root, System-Apps und Runtime-Prozesse sind
keine gültigen persönlichen Authentifizierungsstellen.

Der Identitätsdienst muss vor jedem Start AOSPs Benutzer, Seriennummer,
Passwort-/Sitzungsautorisierung und tatsächlichen CE-Status prüfen sowie die
`RuntimeAdmission`-Sperre halten. Das Protokoll ersetzt keine dieser Prüfungen.
Es überträgt weder Passwörter noch Schlüssel, freie Pfade, Prozesskennungen oder
vom CLI übernommene Dateideskriptoren. Shell-Übergaben und Paketaktionen sind
noch nicht implementiert.

## Nachrichten und begrenzte Wartezeit

Die Verbindung ist AF_UNIX/SOCK_SEQPACKET. Jede Anfrage und Antwort hat exakt
32 Bytes in Little-Endian-Reihenfolge. Anfragen tragen Version, Operation,
streng steigende Sequenznummer, absolute CLOCK_MONOTONIC-Frist, AOSP-ID und
Seriennummer. Antworten spiegeln Anfrage und Identität und melden einen
Fehlercode sowie `ABSENT`, `READY` oder `SEALED`.

| Operation | Erforderlicher Nachweis |
| --- | --- |
| `HELLO` | Erste Anfrage einer neuen Verbindung; sämtliche vorherigen besessenen Kontexte sind bestätigt abgebaut. Kein automatisches Übernehmen alter Ressourcen. |
| `START` | Exakte ID/Seriennummer; tatsächlich bestätigte READY-Antwort des Namespace-Aufsehers. Ein bereits gültiger Kontext derselben Identität kann weiterverwendet werden. |
| `STOP_USER` | Alle besessenen Seriennummern dieser numerischen AOSP-ID sind beendet und ihre Referenzen freigegeben. Seriennummer im Auftrag ist fest 0 als Operationskonvention. |
| `STATUS` | Fehlender, lebender oder gesperrter Kontext für exakt diese Identität; eine fremde Seriennummer wird nicht übernommen. |

Socket und Verbindung sind von Anfang an nicht blockierend. Polling, Senden,
Lesen und interne Serialisierung verwenden dieselbe Frist von höchstens zehn
Sekunden. `RuntimeAdmission.Access.deadlineNanos()` gibt seine bestehende Frist
weiter; der native Kontextstart übernimmt sie ebenfalls. Ein blockierter
Kernelaufruf kann trotzdem einen gesondert bestätigten Abbau erfordern.

Fehlende, überlange, verwechselte oder verspätete Antworten sowie unerwartete
Deskriptoren vergiften die Verbindung. Empfangene Deskriptoren werden geschlossen;
es gibt keine automatische Wiederholung einer möglicherweise bereits wirksamen
Operation und keine stille Wiederverbindung. Der spätere Daemon muss nach
Peer-Verlust alle Kontexte sperren und abbauen. Das Schließen eines Sockets ist
allein **kein** Abbaunachweis und erlaubt keinen CE-Schlüsselentzug.

Ein korrekt gerahmter nativer Operationsfehler erhält dagegen den Kanal,
damit ein gesperrter partieller Kontext erneut kontrolliert gestoppt werden
kann. Ein Fehler darf weder `READY` noch erfolgreiches `ABSENT` behaupten.

## Native Ressourcenverwaltung

`broker_owner.c` besitzt bis zu 16 persönliche Kontextplätze und eigene
CLOEXEC-Kopien der vertrauenswürdigen Basis-/Helfer-/Cgroup-Deskriptoren.
Besitz ist an die tatsächliche Kernel-Prozess-ID und den Hauptthread gebunden;
Bionics nach direktem Clone geerbter PID-Cache genügt nicht.

Ein fehlgeschlagener Start behält den vom Kontextmodul zurückgegebenen
partiellen Besitz. Eine andere Seriennummer kann diesen Platz nicht übernehmen.
Auch ein bereits leerer Cgroup-Verzeichniseintrag muss über seinen bestehenden
Besitzer entfernt werden. Der globale Stopp besucht nach Fehlern oder Ablauf
der Wartefrist weiterhin alle Plätze, um sämtliche Kontrollkanäle zu sperren
und Beendigung anzufordern. Freigeben ist erst ohne jeden verbliebenen Kontext
möglich. AOSP-Schlüsselsperrung bleibt ein zusätzlicher, separater Schritt.

## Noch erforderliche Integration

Der Bootstrap muss die gemeinsame Generation und statischen Helfer aus den
unveränderlichen Systemimages prüfen, die Basis schreibgeschützt einbinden,
den privaten Ressourcenbereich vorbereiten und Vorgängerreste eindeutig
bereinigen. Danach fehlen noch der tatsächliche Listener/Ereignisloop,
SELinux-/init-Anbindung, Registrierung der Speicherkoordination, private
Terminalübergabe, Paketbesitz/-abbruch und echte Zwei-Benutzer-Tests.
Ein leerer neuer In-Memory-Besitzer beweist nicht, dass Reste eines abgestürzten
Daemons auf dem System verschwunden sind.

Vorbereitet sind sieben Java-Protokolltests, ein Test der unveränderten
Admission-Frist, sieben native Protokolltests, ein Kontext-Fristtest und drei
Tests der Ressourcenverwaltung. Insgesamt enthält der Quellstand **52 Java-
und 74 native Tests**. Die neuen Tests sind **noch nicht ausgeführt**. Die
bisher tatsächlich kompilierten und ausgeführten Stände bleiben in
[`docs/component-tests.md`](../docs/component-tests.md) getrennt dokumentiert.
