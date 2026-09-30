# Privater AOSP-Kanal und Besitzer der Runtime-Kontexte

Stand 30. September 2026: Der Lebenszyklus- und Terminalkanal verwendet
**Version 3**. Im lokal gebooteten Vollimage `40179351` läuft der Broker über
init in `u:r:aegis_runtime_broker:s0` mit SELinux Enforcing und
`ro.aegis.runtime.mode=managed-v1`. Alle 393 aktivierten nativen Tests dieses
Image-Stands bestehen. Die vier realen CE-Integrationstests bleiben deaktiviert.

Der nachfolgende Komponentenstand `49f038cd` ergänzt den privaten Paketkanal.
Er ist auf `aegis-build` kompiliert und über GitHub übertragen. Im lokalen
401-QEMU bestehen alle 400 nativen und 140 Java-Komponententests. Der
Produktdienst im Image wurde dabei nicht durch die neuen Komponenten ersetzt. Die öffentliche Paket-Binder-/CLI-Anbindung und
frische AOSP-Adminfreigabe sind noch nicht angeschlossen. Eine erfolgreiche
Kompilierung des Adapters ist kein Nachweis produktiver Paketinstallation.

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
Es überträgt weder Passwörter noch Schlüssel, Hostpfade, Prozesskennungen oder
vom CLI übernommene Dateideskriptoren. Die ergänzte interne
[Terminalübergabe](terminal-handoff.md) erlaubt begrenzte Programmpfade und
Argumente innerhalb des persönlichen Runtime-Kontexts. Die persönliche
CLI-Sitzungsbindung und der Widerruf gehören zum Terminaldienst; die
öffentliche Paketaktions-Anbindung bleibt ein eigener, noch offener Schritt.

## Nachrichten und begrenzte Wartezeit

Die Verbindung ist AF_UNIX/SOCK_SEQPACKET. Jeder Kopf hat exakt 32 Bytes in
Little-Endian-Reihenfolge. Anfragen tragen Version, Operation,
streng steigende Sequenznummer, absolute CLOCK_MONOTONIC-Frist, AOSP-ID und
Seriennummer. Antworten spiegeln Anfrage und Identität und melden einen
Fehlercode sowie `ABSENT`, `READY` oder `SEALED`.

| Operation | Erforderlicher Nachweis |
| --- | --- |
| `HELLO` | Erste Anfrage einer neuen Verbindung; sämtliche vorherigen besessenen Kontexte sind bestätigt abgebaut. Kein automatisches Übernehmen alter Ressourcen. |
| `START` | Exakte ID/Seriennummer; eine asynchrone Auswahl bleibt mit ihrer Auftragskennung registriert. READY erst nach bestätigtem Kontextstart. |
| `CONTINUE_START` | Dieselbe positive Auswahlkennung und dieselbe zugelassene Sitzung; kein automatischer neuer Auftrag nach Fehler oder Widerruf. |
| `STOP_USER` | Alle besessenen Seriennummern dieser numerischen AOSP-ID sind beendet und ihre Referenzen freigegeben. Seriennummer im Auftrag ist fest 0 als Operationskonvention. |
| `STATUS` | Fehlender, lebender oder gesperrter Kontext für exakt diese Identität; eine fremde Seriennummer wird nicht übernommen. |
| `EXEC` | Bereits lebender Kontext; bestätigter Programmstart mit genau einem geprüften persönlichen PTY-Master. Kein impliziter Kontextstart. |
| `RESULT` | Genau dieser Kontext und eine dort vergebene, noch nicht abgeholte Befehlskennung. „Läuft“ ist getrennt von beendetem Exitstatus 0. |

`EXEC` ergänzt den 32-Byte-Kopf um Argumentzahl, Bytelänge und einzeln
NUL-terminierte Argumente, zusammen höchstens 8192 Bytes. `RESULT` ergänzt
eine positive 64-Bit-Befehlskennung und hat exakt 40 Bytes. Beide Antworten
sind exakt 48 Bytes; nur erfolgreiches `EXEC` enthält einen Deskriptor.
`START` und `CONTINUE_START` antworten mit 40 Bytes; `EAGAIN` mit positiver
Auftragskennung bezeichnet die registrierte Vorbereitung. `CONTINUE_START`
trägt ebenfalls 40 Bytes. Alte Protokollversionen werden nicht umgedeutet.

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

## Interner Paketkanal

Die zusätzlichen Operationen 8–14 heißen `PACKAGE_BEGIN`, `PACKAGE_PLAN`,
`PACKAGE_REVIEW`, `PACKAGE_PREPARE`, `PACKAGE_STATUS`, `PACKAGE_START` und
`PACKAGE_CANCEL`. Sie benutzen denselben geprüften Socket, dieselbe Sequenz
und Frist. `BEGIN` enthält ausschließlich Aktion, persönlichen/gemeinsamen
Bereich, Paketname und optionale Version. Alle weiteren Aufrufe verwenden die
bereits registrierte positive Auftragskennung; nur `START` ergänzt den Digest
des dienstintern festgehaltenen vollständigen Plans. Der Digest ist keine Freigabe.

Antworten haben einen 48-Byte-Präfix und höchstens 64 KiB Gesamtgröße. Die
Nutzlast ist entweder leer, ein Status oder ein vollständiger Änderungsplan.
Der Java-Decoder verwirft doppelte/unerwartete JSON-Felder, falsche Typen,
fehlerhaftes UTF-8, unsortierte/doppelte Pakete und unvollständige Effekte.
Es werden keine Deskriptoren übertragen. Status und Plan enthalten die ursprüngliche
Absicht; die Dienstsitzung muss sie mit ihrem unveränderlichen Auftrag vergleichen.

Auch ein fehlgeschlagenes `BEGIN` kann bereits eine Auftragskennung besitzen.
Sie muss vor Freigabe der AOSP-Zulassung registriert bleiben. Ein gezielter
Abbruch bestätigt weder Rollback noch CE-Schlüsselsperrung. Ein bereits
veröffentlichtes Ergebnis darf nicht als abgebrochen ausgegeben werden; verlorene
Antworten lösen niemals einen automatischen Ersatzauftrag aus. Vor `START` sind
die frische AOSP-Adminprüfung und erneute Zulassung der ursprünglichen Sitzung
weiterhin Pflicht. Diese öffentliche Verbindung ist noch nicht implementiert.

## Native Ressourcenverwaltung

`broker_owner.cpp` besitzt bis zu 16 persönliche Kontextplätze und eigene
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

Zusätzlich hält derselbe Besitzer bis zu 16 vorbereitete oder gestartete
Paketveröffentlichungen. Vorbereitungen besitzen bereits ihre benötigten
FD-Kopien und werden beim Benutzerstopp ebenfalls geschlossen. Die internen
Auftragsnummern werden je Besitzer erzeugt und nicht wiederverwendet. Start,
Status und gezielter Abbruch verlangen dieselbe ID, Seriennummer und denselben
Plan-Digest. Ein fehlgeschlagener Teilstart bleibt registriert. Solange sein
Abbau unbestätigt ist, sind neue Runtime-/Paketstarts dieses Benutzers gesperrt.

`STOP_USER`, `HELLO` und der globale Stopppfad schließen jetzt auch diese
Ressourcen ein. Zunächst wird allen betroffenen Publishern Beendigung signalisiert;
erst danach beginnen die begrenzten Warte-/Abbauschritte. Fehler bei einem
Auftrag überspringen keine anderen Aufträge. Ein bestätigtes `ABSENT` setzt
vollständigen Kontext- **und** Paketabbau voraus. Der Daemon räumt beendete
Publisher außerdem ohne abwartenden CLI-Client auf. Antworten zur eigentlichen
Veröffentlichung bleiben von dieser Ressourcenbestätigung getrennt.

Noch offen sind die Verbindung des vertrauenswürdigen Planers und der frischen
AOSP-Freigabe mit diesen internen Eingängen sowie Paket-Cgroup-/SELinux-/CE-
Bootstrap, APT und öffentliche Paketbefehle. Der private Socket akzeptiert
weiterhin keine eingehenden FDs oder Paketoperationen. Vorbereitungen dürfen
einen Verbindungs-/Besitzerwechsel nicht überleben; die spätere Java-Anbindung
muss sie dabei widerrufen. Der neue Besitzerpfad ist noch nicht im laufenden
Systemserver/Daemon des bisherigen Vollimages installiert. Kompilierung und
acht direkte native Besitzer-/Lifecycle-Tests sind im Komponentenstand
`74bb0db9` bestätigt: **155/155 native Tests** bestehen am 29. September 2026
um 19:29:19 UTC im lokalen `927cf51d`-Gast. Der injizierte Metadatenfehler einer
eigenen Test-Cgroup hält deren Besitzer fest, während der zweite Auftrag weiter
aufgeräumt wird; nach Wiederherstellung ist vollständiger Abbau bestätigt.
[Beleg und Grenzen](../docs/component-tests.md).

## Vorbereiteter Dienststart und Wiederherstellung

`broker.c` verbindet jetzt Basisprüfung, statische Helfer, Kontextbesitz und
den privaten Listener. Dieser neue Quelltext ist zunächst ein explizites
Kompilierziel; er wird noch nicht über init oder `PRODUCT_PACKAGES` aktiviert.
Vor jeder Wiederherstellung verlangt er eine exklusive Dateisperre in einem
eigenen, streng geprüften DE-Verzeichnis. Erst nach bestätigtem Abbau von
Vorgängerresten, geprüftem Basismount und Helfern wird der Socket geöffnet.
Der Prozess muss Root im ursprünglichen Host-Namespace, im exakten Broker-
SELinux-Kontext und im konfigurierten Modus `managed-v1` laufen, wie die
AOSP-Speicherkoordination. Der Modusabgleich wurde nach Komponentencommit
`336e9275` korrigiert; sein erneuter Build und die produktive Aktivierung stehen
noch aus. Der aktuell laufende Vollbuild behält den Modus `absent`.

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

Es fehlen noch Gastprüfungen des neuen
Diensts, SELinux-/init-Anbindung, Registrierung der Speicherkoordination,
private Terminalübergabe, Paketbesitz/-abbruch und echte Zwei-Benutzer-Tests.
Insbesondere muss init dem Socket den richtigen Sicherheitskontext geben;
ein Dateisystemlabel allein beweist nicht den mit `SO_PEERSEC` geprüften Peer.

Vorbereitet sind neun native Protokolltests einschließlich Zusatzdaten-/FD-
Leckprüfungen, fünf Wiederherstellungstests sowie die bisherigen Java-,
Kontext-, Basisparser- und Ressourcenverwaltungstests. Insgesamt enthält der
Quellstand **52 Java- und 86 native kompilierte Tests**. Im lokalen QEMU bestehen 48 Java- und 22 von 23 ausgewählten nativen Tests,
darunter alle neuen Protokoll-/Besitz-/Wiederherstellungstests. Der fehlgeschlagene
Basisparser-Test wird mit `336e9275` korrigiert; alle fünf Parser-Tests bestehen im lokalen Nachtest.
Die tatsächlich kompilierten und ausgeführten Stände bleiben in
[`docs/component-tests.md`](../docs/component-tests.md) getrennt dokumentiert.
