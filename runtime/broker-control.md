# Privater AOSP-Kanal und Besitzer der Runtime-Kontexte

Stand 28. September 2026: **Die neu ergänzte Terminalübergabe der Protokollversion
2 wartet auf Build und Gerätetests. Der Dienst ist noch nicht im Produkt aktiviert.**
Die folgende erfolgreiche Kompilierung betrifft den früheren Lebenszykluskanal:
Commit `878fc970ace1a19a7a6f637e8e7df3763536b649`, Lauf
`identity-20260928T174855Z-878fc970-sllLJ3`, beendet um 17:56:40 UTC mit
`IDENTITY_COMPILED_NOT_INSTALLED`. Broker und natives Testprogramm sind gelinkt;
Java-Dienst, DEX und Test-APK sind ebenfalls erfolgreich gebaut.

Die früheren Einzelprüfungen hatten nur ARM64-Objekte beziehungsweise Java
gegen vollständige Core-Systemmodule geprüft. Erst Soong zeigte die fehlende
statische Android-`libcrypto`-Variante, direkte Link-Abhängigkeiten und die
nicht verfügbare stabile `StructUcred`-API. Der korrigierte Stand nutzt die
reguläre dynamische `libcrypto`, explizite JsonCpp-/libbase-Abhängigkeiten und
`LocalSocket.getPeerCredentials()`. Letzteres liest im gepinnten Framework
weiterhin `SO_PEERCRED`. UID/GID- und SELinux-Prüfungen bleiben erhalten.
Namespace-Helfer bleiben statisch, das Testprogramm ist dynamisch gelinkt.
Der Vollbuild `96f9ed6b` endete an einer Kernel-/VINTF-Unvereinbarkeit;
der korrigierte Kernel ist gebaut und geprüft, aber noch nicht gebootet. Dieser alte Vollbuild enthält
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
Es überträgt weder Passwörter noch Schlüssel, Hostpfade, Prozesskennungen oder
vom CLI übernommene Dateideskriptoren. Die ergänzte interne
[Terminalübergabe](terminal-handoff.md) erlaubt begrenzte Programmpfade und
Argumente innerhalb des persönlichen Runtime-Kontexts. CLI-Sitzungsbindung,
Widerruf und Paketaktionen sind noch nicht implementiert.

## Nachrichten und begrenzte Wartezeit

Die Verbindung ist AF_UNIX/SOCK_SEQPACKET. Lebenszyklus-Anfragen und -Antworten
haben exakt 32 Bytes in Little-Endian-Reihenfolge. Anfragen tragen Version, Operation,
streng steigende Sequenznummer, absolute CLOCK_MONOTONIC-Frist, AOSP-ID und
Seriennummer. Antworten spiegeln Anfrage und Identität und melden einen
Fehlercode sowie `ABSENT`, `READY` oder `SEALED`.

| Operation | Erforderlicher Nachweis |
| --- | --- |
| `HELLO` | Erste Anfrage einer neuen Verbindung; sämtliche vorherigen besessenen Kontexte sind bestätigt abgebaut. Kein automatisches Übernehmen alter Ressourcen. |
| `START` | Exakte ID/Seriennummer; tatsächlich bestätigte READY-Antwort des Namespace-Aufsehers. Ein bereits gültiger Kontext derselben Identität kann weiterverwendet werden. |
| `STOP_USER` | Alle besessenen Seriennummern dieser numerischen AOSP-ID sind beendet und ihre Referenzen freigegeben. Seriennummer im Auftrag ist fest 0 als Operationskonvention. |
| `STATUS` | Fehlender, lebender oder gesperrter Kontext für exakt diese Identität; eine fremde Seriennummer wird nicht übernommen. |
| `EXEC` | Bereits lebender Kontext; bestätigter Programmstart mit genau einem geprüften persönlichen PTY-Master. Kein impliziter Kontextstart. |
| `RESULT` | Genau dieser Kontext und eine dort vergebene, noch nicht abgeholte Befehlskennung. „Läuft“ ist getrennt von beendetem Exitstatus 0. |

`EXEC` ergänzt den 32-Byte-Kopf um Argumentzahl, Bytelänge und einzeln
NUL-terminierte Argumente, zusammen höchstens 8192 Bytes. `RESULT` ergänzt
eine positive 64-Bit-Befehlskennung und hat exakt 40 Bytes. Beide Antworten
sind exakt 48 Bytes; nur erfolgreiches `EXEC` enthält einen Deskriptor.
Version 1 wird nicht stillschweigend als Version 2 interpretiert.

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
