# Zusammengesetzter Runtime-Start und Ressourcenbesitz

Stand: Auf dem Builder kompiliert; **drei Fehlerpfadtests im lokalen QEMU
bestanden**. Positiver Runtime-Start und vollständiger Logout sind nicht belegt.
Siehe [zweiter Komponentenlauf](../docs/component-tests.md#zweiter-lauf-speichergruppen-und-ressourcenbesitz).
Der interne Besitzer `context.c` verbindet erstmals den begrenzten Namespace-Start,
die echten persönlichen Mounts und das private Startprotokoll. Der öffentliche
AOSP-/CLI-Aufrufer, die Basis-Einbindung und SELinux-Integration fehlen weiterhin.
Der Produktmodus bleibt `absent`.

Eine nachfolgende [Terminalübergabe](terminal-handoff.md) ergänzt im Quelltext
den persönlichen Befehlskanal und dessen Besitz beim Kontextabbau. Ihre
ARM64-Syntax ist auf dem Builder geprüft; Linken, Gasttest und Anbindung an
die AEGIS-CLI stehen noch aus.

Der ausschließlich vertrauenswürdige, einthreadige Host-Broker erhält aus seiner
verifizierten Systembasis bereits geöffnete Basis-, Setup- und Init-Deskriptoren
sowie den privaten Cgroup-Elternknoten. Er muss AOSP-Authentifizierung, Seriennummer,
entsperrten CE-Speicher und Lebenszyklus-Serialisierung sicherstellen. Die API nimmt
keine CLI-Pfade, Passwörter oder vom Client behauptete Berechtigungen entgegen.

Der Start läuft in dieser Reihenfolge:

1. Exklusive Speichergruppe für die unveränderliche AOSP-ID und Seriennummer.
2. Neues Namespace-Kind, atomar in dieser Gruppe; der Exec-Zugang bleibt geschlossen.
3. OOM-Einstellung und UID/GID-Maps setzen und zurücklesen.
4. Readonly-Basis, echtes fscrypt-geprüftes CE-HOME und private Geräteansicht vorbereiten.
5. Vier Deskriptoren plus Abschlussdatensatz über das private Socket-Paar übergeben.
6. Sämtliche temporären Host-Mount-/CE-Deskriptoren schließen, Kind freigeben.
7. Exakte `READY`-Antwort des Init mit derselben ID und Seriennummer, PID 1 und
   ohne zusätzliche Deskriptoren abwarten. Gemeinsame Startfrist: zehn Sekunden.

Ein erfolgreicher Start ist keine AOSP-Autorisierung und keine Garantie, dass das
Kind weiterlebt. Vor jeder Nutzung des privaten Kanals wird erneut sein Pidfd
geprüft. Der Broker behält den Kanal; CLI-Clients erhalten ihn niemals.
Die vorbereitete Terminalübergabe besitzt Sequenznummern und Befehlsergebnisse;
die Autorisierung und Weitergabe der PTYs an CLI-Sitzungen bleiben Aufgaben
des noch zu integrierenden Broker-/AOSP-Aufrufers.

Schon ein fehlgeschlagener Start kann Ressourcen besitzen. Die Ausgabe behält
deshalb den Besitzer, bis `aegis_context_stop` die Bereinigung bestätigt. Fehler
schließen temporäre Mounts und Sockets und fordern den Kindstopp an; sie melden
nicht eigenständig erfolgreiches Aufräumen. Eine vorhandene fremde Gruppe wird
auch beim Abbau eines abgewiesenen Starts nicht übernommen oder entfernt.

Beim Stopp wird der Kanal geschlossen und ein erneuter Start über diesen Besitzer
dauerhaft ausgeschlossen. Kindstopp, leere Speichergruppe und Pidfd-Exit werden
getrennt geprüft. Beide Wartevorgänge teilen sich eine monotone Gesamtfrist von
höchstens zehn Sekunden. Erst dann werden Namespace-Referenzen geschlossen und
die eigene Gruppe entfernt. Bei Fehlern bleibt der Besitzer für erneuten Abbau
erhalten; auch ein anderweitig verbrauchter Kindstatus wird nicht als Erfolg gewertet.

Diese API hält nach Rückkehr selbst keine offenen CE-Mountreferenzen. Sie bestätigt
weder die Freigabe anderer Broker-/Clientreferenzen noch Pakettransaktionen oder
AOSPs Schlüsselentzug. Der vollständige Logout muss alle diese Schritte zusammen
nachweisen. Einzelne blockierende Kernel-Dateisystemoperationen besitzen keine
eigenständige harte Laufzeitgarantie; überschrittene Fristen berechtigen nie zu
einer erfundenen Abschlussmeldung.

Drei zusätzliche Gerätetests prüfen abgewiesene Setup-Dateien mit anschließender
vollständiger Bereinigung und erhaltenen Aufrufer-Deskriptoren, Schutz vorhandener
Gruppen und Besitzprüfung nach direktem `clone3`. Sie erzeugen keine persönlichen
AOSP-Benutzer. Der positive Start mit echter verschlüsselter HOME-Sicht und
durchgesetzter SELinux-Policy muss separat im neuen lokalen QEMU erfolgen.
