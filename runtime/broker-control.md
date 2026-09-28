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
Der Vollbuild `96f9ed6b` endete an einer Kernel-/VINTF-Unvereinbarkeit;
der korrigierte Kernel läuft separat. Dieser alte Vollbuild enthält
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
Operation und keine stille Wiederverbindung. Der vorbereitete Daemon sperrt nach
Peer-Verlust alle Kontexte und verlangt ihren bestätigten Abbau, bevor er
eine neue Verbindung annimmt. Das Schließen eines Sockets ist
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

## Vorbereiteter Dienststart und Wiederherstellung

`broker.c` verbindet jetzt Basisprüfung, statische Helfer, Kontextbesitz und
den privaten Listener. Dieser neue Quelltext ist zunächst ein explizites
Kompilierziel; er wird noch nicht über init oder `PRODUCT_PACKAGES` aktiviert.
Vor jeder Wiederherstellung verlangt er eine exklusive Dateisperre in einem
eigenen, streng geprüften DE-Verzeichnis. Erst nach bestätigtem Abbau von
Vorgängerresten, geprüftem Basismount und Helfern wird der Socket geöffnet.
Der Prozess muss Root im ursprünglichen Host-Namespace, im exakten Broker-
SELinux-Kontext und im konfigurierten Modus `managed` laufen.

`broker_cgroup.c` inventarisiert die private Cgroup `aegis-runtime`, bevor
es Prozesse beendet. Nur bis zu 16 direkte Kinder mit kanonischem
`u<userId>-s<serial>`-Namen, Eigentümer Root, Modus 0700 und ohne Nachfahren
werden als Vorgängerreste behandelt. Fremde Namen oder veränderte Metadaten
führen zum Abbruch, ohne ihre Prozesse zu beanspruchen. Danach müssen
`cgroup.kill`, die Beobachtung `populated=0` und die Entfernung derselben
Verzeichnis-Inodes erfolgreich sein. Der neue Besitzer übernimmt keine alten
Kontexte oder persönlichen Anmeldungen.

Die private Gesamtgruppe erhält `memory.max=2 GiB`, `memory.high=1,5 GiB`,
keinen Swap und höchstens eine Ebene mit 16 persönlichen Gruppen. Der Dienst
verschiebt sich nicht in diesen Bereich und verändert keine Controller am
gemeinsamen Android-Cgroup-Root. Das ist noch keine vollständige CPU-/Prozess-
oder Speicherdruckabsicherung. Die vorhandenen persönlichen Limits gelten
zusätzlich. Aktuelle eigene Kinder werden weiterhin separat über Pidfds
beendet und abgeholt; eine leere Cgroup ersetzt dieses Warten nicht.

Der Ereignisloop akzeptiert genau eine authentifizierte AOSP-Verbindung,
verlangt zeitlich begrenztes HELLO und schließt unerwartete Dateideskriptoren
auch bei abgeschnittenen Zusatzdaten. Ein zweiter Verbindungsversuch verdrängt
die bestehende Sitzung nicht. SIGCHLD wird als Signal beobachtet; nur der
Kontextbesitzer verbraucht den eigentlichen Kindstatus. SIGTERM, SIGINT und
SIGHUP führen zu kontrolliertem Abbau. Ein fehlgeschlagener Abbau erzeugt
keine erfolgreiche Bestätigung für einen CE-Schlüsselentzug.

## Noch erforderliche Integration

Es fehlen noch vollständiger Soong-/Linknachweis und Gastprüfungen des neuen
Diensts, SELinux-/init-Anbindung, Registrierung der Speicherkoordination,
private Terminalübergabe, Paketbesitz/-abbruch und echte Zwei-Benutzer-Tests.
Insbesondere muss init dem Socket den richtigen Sicherheitskontext geben;
ein Dateisystemlabel allein beweist nicht den mit `SO_PEERSEC` geprüften Peer.

Vorbereitet sind neun native Protokolltests einschließlich Zusatzdaten-/FD-
Leckprüfungen, fünf Wiederherstellungstests sowie die bisherigen Java-,
Kontext-, Basisparser- und Ressourcenverwaltungstests. Insgesamt enthält der
Quellstand **52 Java- und 86 native Tests**. Die neuen Tests sind **noch nicht ausgeführt**. Die
bisher tatsächlich kompilierten und ausgeführten Stände bleiben in
[`docs/component-tests.md`](../docs/component-tests.md) getrennt dokumentiert.
