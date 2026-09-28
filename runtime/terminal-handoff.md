# Terminalübergabe aus einem persönlichen Runtime-Kontext

Stand 28. September 2026: **ARM64-Syntax auf dem Builder geprüft, noch nicht
gelinkt oder im Gast ausgeführt.** Commit `8bd04981f094e89877e9f3fc2714fe63ddab5cc8`
besteht die drei Prüfungen für `exec.c`, `context.c` und `exec_tests.cpp` im Lauf
`runtime-syntax-20260928T213328Z-8bd04981-NOs0h3`. Dabei werden die vorhandenen
AOSP-Compilerregeln nur gelesen und mit `-fsyntax-only` auf einen getrennten,
über GitHub bezogenen Checkout angewandt. AOSP-Quellen und Build-Ausgaben
werden nicht verändert. Der erste Versuch fand einen Typvergleich im Test;
die Korrektur ändert dessen Typ, nicht die Warnungsregeln oder Erwartung.

Der laufende Vollbuild `bfe90925` enthält diese spätere Erweiterung nicht.
Ein Linux-Befehl über die AEGIS-CLI ist damit noch nicht verfügbar.

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
übernimmt den Master; Übergabe an den angemeldeten CLI-Prozess und dessen
Widerruf müssen noch im Broker und Java-Dienst integriert werden.

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
Mit ihnen enthält die Suite 100 Tests; dieser neue Gesamtstand ist noch ungeprüft.
