# Terminalübergabe aus einem persönlichen Runtime-Kontext

Die Sitzungskontrolle prüft beim periodischen Aufräumen zusätzlich die
ursprüngliche Kernel-Prozessidentität. Ein fremder, länger lebender Binder
als Lifetime-Objekt kann dadurch kein Terminal eines beendeten CLI-Prozesses
offen halten. Ein lokaler Gerätetest dieses öffentlichen Pfads steht noch aus.

## Öffentliche CLI-Anbindung in Arbeit

Die neue Quellfassung ergänzt `aegis linux shell` und einen pro CLI-Prozess,
Anmeldung und AOSP-Benutzerseriennummer gebundenen Binder-Kanal. Stand
`026665fb` ist einschließlich CLI, Dienst, JNI und Policy auf `aegis-build`
kompiliert. Die 114 nativen und 62 Java-Komponententests bestehen im älteren
lokalen Image `030dd177`; sie führen den neuen öffentlichen Dienstpfad nicht
aus. Dessen Abnahme verlangt den folgenden vollständigen Image-Boot. Der
erste Compileversuch `05bdef02` scheiterte am nicht vorhandenen
`explicit_bzero`; die gepinnte Bionic-Funktion `memset_explicit` ist jetzt
eingebunden. Die folgenden früheren Nachweise beziehen sich weiterhin auf
die internen Komponenten.

Der persönliche PTY-Master bleibt ausschließlich im Systemdienst. Die CLI
erhält begrenzte Byte-Nachrichten, Größenänderung und bestätigten Exitstatus,
niemals einen Deskriptor, der einen Widerruf überleben könnte. AOSP-Prüfungen
erfolgen vor der Runtime-Sperre; kurze nichtblockierende Dateideskriptorzugriffe
unterliegen derselben Zulassungs- und Lebenszyklusbarriere. Stop und Logout
schließen sämtliche Terminalreferenzen vor der bestätigten nativen Bereinigung.
CLI-Ende oder ein normaler Shell-Exit sind dagegen kein persönlicher Logout.

Ein begrenzter Besitzerbestand behält auch geschlossene Kanäle, bis der native
Befehlsstatus abgeholt oder der ganze Kontext bestätigt abgebaut wurde. Der
Hintergrundsammler führt keine neuen Programme aus und ruft keine AOSP-
Identitätsfunktionen unter der Runtime-Sperre auf. Bildschirm-/Keyguard-Sperre
und Benutzerwechsel widerrufen den interaktiven Zugang; erlaubte Hintergrund-
Kontexte werden dadurch nicht als abgemeldet oder CE-gesperrt ausgegeben.

Die CLI verwendet einen einzigen Eingabeleser für Befehle, Passwortabfragen und
Rohmodus. Dadurch können keine vorgelesenen Zeichen zwischen `Console` und
einem zweiten Dateideskriptor-Leser verloren gehen. Eine kleine Bibliothek aus
dem schreibgeschützten Systemimage übernimmt Termios, begrenztes Lesen und
Fenstergröße. Passwörter gehen weiterhin ohne Echo, Argumente oder Passwort-
Strings direkt an den bestehenden AOSP-Prüfpfad. Rohmodus, EOF, Teil-Schreibvorgänge,
Ctrl-C, Größenänderung, Rückkehr zur CLI und Fehler-Wiederherstellung müssen
noch mit der tatsächlichen neuen CLI im lokalen QEMU nachgewiesen werden.

Erforderlich bleiben außerdem Gegenproben mit einem fremden Prozess, konkurrierender
Abmeldung/Bildschirmsperre, wiederholtem Shell-Ende, Hintergrundprogrammen und
zwei echten persönlichen GNU-Kontexten. Diese Quelländerung ersetzt keinen
dieser Laufzeitnachweise.

## Frühere Komponentennachweise

Stand 29. September 2026: Komponentencommit `ac87df1f` ist auf `aegis-build`
kompiliert und gelinkt. Im lokalen Android-QEMU bestehen 106/110 native und
58/59 Java-Tests. Die vier Terminalfehler liegen am Fixture-Gerät: Androids
`posix_openpt()` öffnet den Tmpfs-Knoten `/dev/ptmx`, den die produktive
Devpts-Prüfung ausdrücklich abweist. Der nächste Quellstand verwendet wie der
echte Supervisor `/dev/pts/ptmx` und testet die Legacy-Ablehnung zusätzlich.
Die Korrektur ist in `2a766ab5` gebaut: 111/111 native und 61/62 Java-Tests
bestehen im lokalen QEMU, einschließlich der Legacy-Ablehnung. Der Java-Fehler betrifft
die geerbte SMS-RRO; siehe [aktuellen Stand](../docs/phase-1-progress.md).
Ein Linux-Befehl über die AEGIS-CLI ist weiterhin nicht verfügbar.

`aegis_context_exec` verwendet nach der bestätigten Namespace-READY-Antwort
einen eigenen Besitzer des privaten Kontrollkanals. Nur der ursprüngliche
einzelne Host-root-Prozess darf ihn verwenden. Der Kontext schließt dessen
Socketkopie beim Stoppen mit; ein unklarer Austausch versiegelt den Kontext
und fordert dessen Beendigung an. Erfolgreicher Abbau bleibt separat über
Cgroup und Pidfd nachzuweisen.

Jeder Start übernimmt die unveränderte absolute Frist der AOSP-Zugangsfreigabe.
Begrenzte Argumente gehen ohne Shell-Zusammensetzung an den schon vorbereiteten
Namespace-Aufseher. Der private Kanal verwendet steigende Befehlskennungen
und verwechselt verzögerte Ergebnisse anderer Befehle nicht mit dem neuen Start.
Bis zu 32 Ergebnisse bleiben gespeichert, bis ihr Besitzer sie abholt; volle
Plätze führen zur Ablehnung, nicht zum stillen Verlust eines Ergebnisses.

Erst eine passende STARTED-Antwort mit einem Terminaldeskriptor wird akzeptiert.
Geprüft werden CLOEXEC, Les-/Schreibzugriff, Devpts-Dateisystem, PTY-Mastertyp
und die vom Kernel geöffnete Slave-Gegenstelle mit der vorgesehenen persönlichen
Host-UID/GID. Diese Zuordnung ersetzt weder die Herkunft des privaten Sockets
noch AOSP-Authentifizierung oder die Produktions-SELinux-Regeln. Der Aufrufer
übernimmt den Master. Die zusätzliche interne Broker-/Java-Verbindung ist unten
beschrieben; Bindung an den angemeldeten CLI-Prozess und Widerruf durch dessen
Sitzungsverwaltung müssen noch integriert werden.

Bestätigte Programmfehler wie ein fehlendes Programm lassen den Kanal nutzbar.
Fehlende/unerwartete Deskriptoren, falsche Befehlskennungen, doppelte Exitmeldungen,
falsche Prozesszuordnung, Zeitüberschreitung und Verbindungsverlust erzeugen
keinen Erfolgsstatus und dürfen nicht auf demselben Kanal wiederholt werden.
Ein Programmende ist kein persönlicher Logout und kein Nachweis entzogener
CE-Schlüssel.

Zwölf neue native Gerätetests verwenden einen begrenzten Protokollpeer und
ausschließlich selbst erzeugte PTYs im lokalen Android-Testgast. Sie prüfen
Übergabe, mehrere Befehle, Ergebnisbesitz, Fehler-/Fristverhalten,
Deskriptorfreigabe sowie Abweisung eines fremden Benutzerterminals und eines
geerbten Prozessbesitzes. Sie starten selbst keine Debian-Programme und ersetzen
keine persönliche Linux-Sitzung oder deren vollständigen Logout-Nachweis.
Die zwölf Tests sind Bestandteil des geprüften 110er-Komponentenstands;
die vier positiven Handoff-Fälle erfordern die oben beschriebene Fixture-Korrektur.

## Interne Broker-/AOSP-Verbindung

Commit `ac87df1fc333893d08db0ceb9c67b13c59eed758` besteht außerdem alle acht
ARM64-Syntaxprüfungen im Builderlauf
`runtime-syntax-20260928T215856Z-ac87df1f-rOLeXi`, einschließlich Broker,
Protokoll und der ergänzten nativen Tests. Anschließend lieferte der Komponentenlauf
`identity-20260928T222328Z-ac87df1f-Iu66cE` die über GitHub geprüften Testartefakte.
Die Tests verwenden ein eigenes lokales Profil; das sichtbare Benutzer-QEMU
bleibt getrennt davon.

Die vorbereitete Version 2 ergänzt den weiterhin nur für `system_server`
zugelassenen SEQPACKET-Kanal um `EXEC` und `RESULT`. Persönliche Kennung,
AOSP-Seriennummer, Verbindungssequenz und unveränderte absolute Frist bleiben
in jedem Auftrag enthalten. Ein Befehl darf nur einen bereits gestarteten,
lebenden Kontext verwenden. Der Broker nimmt niemals Deskriptoren oder
Hostpfade vom Absender entgegen. Argumente sind bis zu 32 einzeln terminierte
Werte in höchstens 8192 Paketbytes, ohne implizite Shell-Auswertung.

Der Kontextbesitzer vergibt pro Brokerprozess einmalige, nicht wiederverwendete
Befehlsnummern und ordnet sie den internen Supervisor-Nummern zu. Ein Stopp mit
späterem Neustart desselben AOSP-Benutzers kann dadurch keine alte Anfrage einem
neuen Befehl zuordnen. Diese Nummern sind keine Autorisierungsbelege. Ihre
Verwendung setzt weiterhin AOSP-Sitzungsbindung und aktuelle Freigabe voraus.

Erfolgreiches `EXEC` übergibt genau einen geprüften PTY-Master via `SCM_RIGHTS`.
Der Broker schließt seine Übergabekopie nach dem Sendeversuch, auch bei Fehlern.
Java prüft zusätzlich Mastertyp und Zugriffsmodus, setzt CLOEXEC und übernimmt
eine eigene `ParcelFileDescriptor`-Kopie. Unerwartete oder abgewiesene Deskriptoren
werden geschlossen. `RESULT` übergibt niemals Deskriptoren und unterscheidet
einen laufenden Befehl ausdrücklich von einem bestätigten Exitstatus 0.
Fehler oder unklare Übertragung behaupten weder Start noch Ausführungserfolg.

Zehn zusätzliche native Tests prüfen diese Paketgrenzen, Deskriptorübergabe,
Ablehnung fremder Eingaben sowie fehlender, teilweise gestarteter und veralteter
Kontexte. Sechs Java-Gerätetests prüfen gemeinsame Literal-Testpakete,
Unicode-/Größenbegrenzung und widersprüchliche Antworten. Der Quellstand enthält
damit 110 native und 59 Java-Tests; Ergebnisse stehen oben. Die aktuelle
Weiterentwicklung bindet Start/Status/Stopp an die tatsächlich über AOSP
authentifizierte CLI-Sitzung und registriert den Speichercontroller nur im
künftigen verwalteten Modus. Eine interne Epoch-Bindung bleibt nach Widerruf
ungültig, auch wenn ein anderer Terminalkanal denselben Benutzer neu anmeldet.
Ein persönliches PTY wird noch nicht an die CLI herausgegeben.
SELinux-/Init-Aktivierung, echte Debian-Sitzungen, PTY-Widerruf und Paketabläufe
bleiben offen. `runtime.mode=absent` bleibt im Produkt gesetzt. Die nächste
Suite umfasst 111 native und 62 Java-Tests; ihre Ergebnisse stehen oben. Die
verwaltete Dienstanbindung selbst ist kompiliert, aber noch nicht im Gast aktiviert.
