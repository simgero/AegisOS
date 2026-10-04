# Phase 1 – aktueller Abnahmestand b832d6c

Stand: 4. Oktober 2026. **Teilabnahme; keine Gesamtfreigabe.**
Maßgeblich bleiben die vollständige [DoD](architecture/phase-1-dod.md)
und der [Entwicklerauftrag](architecture/phase-1-developer-brief.md).
Diese Übersicht ersetzt deren Detailanforderungen nicht. Sie verhindert,
dass bestandene Fälle älterer Images als aktuelle Ergebnisse erscheinen.

Getestetes Image: `b832d6c077baeee4324e00d00dc3618372f3e9d9`.
Profil: `1943dcb7-d438-48de-8e62-d9967b32b9b2`.
Die genannten Belege liegen lokal unter `out/phase1-dod/b832d6c-base/`.
Der [Ergebnisindex](phase-1-result-index.md) beschreibt Abläufe, Grenzen,
Prüfsummen und erhaltene Fehlversuche. Seine ältere T01–T17-Tabelle gehört
zum Image `209278de`; sie ist keine aktuelle Freigabeliste.

## Alle 17 Pflichtbereiche

„Belegt“ bezeichnet hier ausschließlich den ausdrücklich genannten Teilfall.
Eine offene Variante bleibt erforderlich, auch wenn andere Varianten derselben
Zeile bestanden sind. Insbesondere ersetzt ein erfolgreicher Paketplan weder
die Veröffentlichung noch die tatsächliche Aktivierung und Programmausführung.

| DoD-Bereich | Aktuell belegte Teilfälle | Noch vollständig nachzuweisen oder zuzuordnen |
| --- | --- | --- |
| T01 Benutzer/Anmeldung | Anlage, Auflistung, AOSP-Admin Alpha und normaler Beta; falsches Beta-Passwort ohne neue CE-Freigabe; korrekte Anmeldung und GNU-Zugang. Erster korrekter Zugang beider Benutzer nach Reboot ohne vorherigen Fehlversuch. | Vollständige Zuordnung der Identitätsquelle; dynamischer Passwortaudit für Argumente, History, Dateien, Logs, Transport und Fehlerpfade. Die gebundenen neun Quelldateien und begrenzten Logprüfungen schließen diesen Audit nicht ab. |
| T02 Passwortwechsel | Frühere Images besitzen Ergebnisse; diese werden hier nicht übernommen. | Passwortwechsel auf diesem Image, erneute Sperre, Ablehnung des alten und Annahme des neuen Passworts, gepaarter Neustart sowie Erhalt ursprünglicher Daten ohne vollständige Neuverschlüsselung. |
| T03 Identität/Namespaces | UID und GID aus tatsächlichen GNU-Ausgaben geprüft: jeweils intern 1000, alle 1002 gemappten Kennungen pro Benutzer überschneidungsfrei, kein Host-Root/Host-UID 1000; sechs getrennte Namespaces. Identität und Kontexte bleiben bei Vordergrundwechseln erhalten. Variantenabgleich abgeschlossen. | T03 belegt. Fehlende Voraussetzungen und ungültige Maps bleiben eigenständige T04-Pflichtfälle. |
| T04 Startbedingungen | Start/Shell nach falschem Passwort beziehungsweise ohne Terminalautorisierung verweigert. | CE-Sperrung unabhängig von fehlender CLI-Anmeldung prüfen; fehlende Namespaces, ungültige Zuordnung und jede erforderliche Sicherheitsvoraussetzung gezielt nachweisen. Kein schwächerer Ersatzbetrieb. |
| T05 Shell | Tatsächliche GNU-Ausführung für Alpha, Beta und Gamma; eigenes HOME/Identität; Shell-Ende erhält Alpha-Kontext und AOSP-Sitzung. | Vollständige Zuordnung des fremden/gesperrten Shell-Ziels sowie der Exitcode-/Terminalfälle. |
| T06 Gegenseitiger Zugriff | Konkrete HOME-, Konfigurations-, synthetische Secret-, temporäre Datei-, Prozess- und POSIX-Mqueue-Prüfungen bei gleichzeitig entsperrtem Alpha/Beta. Vorhandene private Paketstores inzwischen in beiden Richtungen geprüft. | Abschließender Abgleich aller geforderten Pfade und Ressourcen mit diesen konkreten Prüfumfängen. Keine pauschale Aussage über beliebige Syscalls oder IPC-Verfahren. |
| T07 Persistenz | Ursprüngliche Dateien, Einstellungen und private Pakete über Runtime-/VM-Neustart erhalten; Paketdatenbank und Generationen über VM-Neustart identisch. Private jq/libjq u3 bleibt beim Kontextneustart für das gemeinsame Update erhalten. Frische `/tmp`-/`/run`-Proben beider Benutzer verschwinden nach eigenem Stopp/Start; Alphas reiner Neustart erhält exakt Paketdatenbank und Generation. Variantenabgleich abgeschlossen. | T07 belegt. Passwortwechsel, Abmeldefehler und Löschung bleiben T02/T11/T12 zugeordnet. |
| T08 Wechsel/Sperre/Ressourcen | Hintergrundfortschritt mit konkreter Identität, Bildschirmsperre ohne behaupteten CE-Entzug; AOSP stoppt Beta beim Erreichen seines Benutzerlimits; frische Anmeldung stellt ursprüngliche Daten wieder bereit. | Verbleibende Wechselrichtungen/Terminalwiderrufe vollständig zuordnen. Der Ressourcenfall belegt zwei gleichzeitig laufende persönliche Benutzer, nicht drei. |
| T09 Runtime-Stopp | Eigene Stopps in beiden Richtungen erhalten AOSP-Sitzung/CE und den anderen Kontext; genaue Init-/Hintergrundidentitäten beendet, gültige Peer-Proben davor/danach fortschreitend. Frische `/tmp`-/`/run`-Dateien beider Benutzer verschwinden. Alphas Fall enthält keine Paketänderung. | Sämtliche geforderten Ressourcen einschließlich verbleibender IPC-Zuordnung vollständig bewerten. Die beiden konkreten Stopprichtungen mit gültigen Fortschrittsproben sind belegt. |
| T10 Logout | Direkter Beta- und Alpha-Logout ohne vorheriges `linux stop`: CE gesperrt, ursprüngliche Prozesse/Kontexte entfernt, bekannte Originaldateien unlesbar; spätere positive Rücklesungen vorhanden. | Vollständiger Abbau offener Zugriffe, Mounts und IPC; konkurrierender Start und Vordergrundwechsel während Abmeldung. Der fehlgeschlagene zusätzliche Loop-Beobachter bleibt ein Teilprotokoll. |
| T11 Logoutfehler | Erfolgreiche direkte Abmeldungen sind dokumentiert. | Gezielte ausstehende und fehlgeschlagene CE-Sperrung, sicherer Wiederanlauf nach Ursachenbehebung sowie Paketaktionen beim Logout auf diesem Image. Die frühere ungeklärte f098-Abmeldung ist weder ein aktueller bestandener Test noch eine nachgewiesene Fehlerbehebung. |
| T12 Reboot/Löschung | Android und KeyMint-Helfer geordnet gestoppt; gleiches Profilpaar, neue Boot-ID; beide persönlichen Speicher vor Anmeldung gesperrt und Originaldaten danach erhalten. | CLI-Benutzerlöschung, Ende alten Schlüsselzugriffs und privater Zuordnungen sowie neuer Benutzer/kontrollierte ID-Wiederverwendung. |
| T13 Paketautorisierung | Gültige gemeinsame/persönliche Installation, Aktualisierung und Entfernung aktiviert; persönliches Update für Beta mit Alpha-Adminfreigabe. Je drei Ablehnungen für `install all`, `install user`, `update all`, `update user`. Beta besitzt private Installation trotz abweichendem Admin Alpha; dessen CE blieb während dieses Installationsfalls gesperrt. | Sechs Ablehnungen für beide Entfernungsbereiche; vollständige Aufrufer-/Eigentümerbindung und CE-Zuordnung über die gesamte Aktionsmatrix. CLI-Parserablehnung allein belegt keine vollständige Serviceidentitätsprüfung. |
| T14 Paketbereiche | Gemeinsame Software bei bestehenden Benutzern und späterem Gamma ausgeführt; private Versionen/Änderungen erhalten gemeinsamen Bestand und anderen Kontext. Dieselbe gemeinsame Bash lädt für Alpha/Beta verschiedene Einstellungen unter demselben HOME-Pfad und nach Rückwechsel unverändert erneut. Variantenabgleich abgeschlossen. | T14 belegt. Gamma lief unter dem dokumentierten AOSP-Limit; kein gleichzeitiger Drei-Benutzer-Nachweis. Die übrigen Paketpflichtfälle bleiben T13/T15–T17 zugeordnet. |
| T15 Private Versionen | Dasselbe jq mit passenden libjq-Abhängigkeiten in u3/u4 tatsächlich ausgeführt; private Version hat Vorrang. Alpha entfernt private u4 mit ausdrücklich angekündigtem Rückfall auf gemeinsame u3. Beta entfernt die persönliche u4-Auswahl bei bereits gemeinsamer u4 mit Alpha-Adminfreigabe: erster neuer Kontext ohne private Auswahl, unveränderte Paketdatenbank und 81 Paketversionen, tatsächliche GNU-Ausführung und Originaldaten erhalten. | Nicht verfügbare Version und unauflösbare Abhängigkeiten ohne stille Ersetzung nachweisen. |
| T16 Aktivierung | Gemeinsames Update veröffentlicht, beide laufenden Kontexte unverändert; beide ersten neuen Starts erfolgreich. Alpha führt gemeinsame jq/libjq u4 aus, Beta behält ausdrücklich private u3. Beide übernehmen neue Basisbibliotheken und behalten Originaldaten; vollständige Paketbestände und neue Basisbindung geprüft. Erfolgreiche Aktivierungskette vollständig zugeordnet. | Geforderte Konfliktfälle bleiben offen; die erfolgreiche Updatekette ersetzt sie nicht. |
| T17 Fehler/Parallelität | Abbruch der dokumentierten Pläne vor Adminfreigabe erhält den vorherigen Zustand. | Gleichzeitige gemeinsame/private Aktionen, verschiedene Antragsteller, Abbruch während Ausführung, Installationsfehler in den erforderlichen Phasen und Logout während einer Transaktion; sichtbarer konsistenter Abschluss oder Reparaturbedarf. |

## D1–D7 und Referenzablauf

Der Variantenabgleich `current-variant-audit.json`, SHA-256
`e04258b81dbf1fe1feb624d01f91820fb3e9502d5ac9195c916f23c74e356aa9`,
bindet 86 lokale Belegdateien an die tatsächlichen GNU-Ausgaben und Zustände.
Er schließt T03, T07 und T14 am genannten Image; der neue gemeinsame
Programm-/Konfigurationsfall ist zusätzlich an `shared-bash-config-proof.json`,
SHA-256 `c0eb58f69b5935045660f4a44ac0360dbe6815af37ad1577523babb9004160da`,
gebunden. Umfang, historische Ereignisrekonstruktion und zwei erhaltene lokale
Auditfehler stehen im Ergebnisindex. Es handelt sich nicht um eine erneute
Ausführung sämtlicher älterer Tests oder eine Gesamtfreigabe.

Alle sieben Abschlusskriterien bleiben offen. Außer den obigen Varianten
gehören dazu weiterhin:

- **D1:** vollständiges reproduzierbares Source-/Build-/Startinventar und
  relevante Dienststabilität über den Abnahmelauf. Bildschirm, Eingabe und
  authentifiziertes ADB sind belegt. Die Boot-3-Beobachtung ist bis zum
  Protokollausschnitt um 11:17 UTC erweitert: unveränderter SystemServer,
  keine neuen nichtnulligen Dienstrückgaben und acht zusätzliche zugeordnete
  idmap2d-Stopps. Zwei Starthelfer-Ursachen bleiben ungeklärt. Der frühe
  Host-SIGTERM ist erhalten und kein bestandener Boot.
- **D2/D3:** vollständige Benutzerverwaltung, Passworttransport, Passwortwechsel,
  Abmeldefehler und Löschlebenszyklus gemäß T01/T02/T10–T12.
- **D4/D5:** vollständige Zuordnung von Lebenszyklus, Startvoraussetzungen,
  Isolation und Rechtebegrenzung. Beobachtete schreibgeschützte Basis,
  SELinux-Domäne, fehlende Capabilities, NoNewPrivs und Seccomp sind konkrete
  Nachweise; sie ersetzen keine fehlenden Pflichtvarianten.
- **D6:** gesamte Aktions-/Autorisierungs- und Fehler-/Parallelitätsmatrix.
- **D7:** vollständiger aktueller Variantenabgleich, Referenzablauf und
  Abschlussbericht. Start-/CLI-Anleitung, Architektur, zwölf Threat-Model-
  Szenarien und Kryptographiegrundlage müssen am finalen Stand zusammenpassen.

Für die Referenzschritte 1–11 liegen die oben beschriebenen aktuellen Teilbelege
vor. Schritt 12 umfasst weiterhin sämtliche ergänzenden Pflichtfälle; er ist
nicht durch eine erfolgreiche Demonstration der Kernabläufe ersetzt.
Weder diese Übersicht noch die Existenz von Nachweisdateien erteilt eine
Gesamtfreigabe. Rohdaten, Profile und Builds bleiben lokal.

Die bei dieser Bestandsaufnahme gelesenen 20 bereits vorhandenen Nachweise sind
mit ihren Prüfsummen in `current-status-evidence-inventory.json` inventarisiert,
SHA-256 `a5fa62a784f4c3e956c2ecab4faf4b22b7e3b616b904184b7f757a8cd2e4df76`.
Das ist eine Zuordnung vorhandener Belege, keine erneute Ausführung ihrer Tests.
Der anschließende vollständige gemeinsame Updatefall ist zusätzlich an
`common-update-both-proof.json` gebunden, SHA-256
`07eba4383de20c49df82095772f91a0b393841eeefa7bef03f7c3f842f5fc1d3`;
sein Umfang und seine Grenzen stehen im Ergebnisindex.
Das persönliche Update samt neuer Beta-`/tmp`-/`/run`-Prüfung bindet
`personal-update-active-proof.json`, SHA-256
`eb76073b9788d7f1b8139bb1b0b4d59419bdcda47ac99ad6d86a43e8419af3c1`.
Alphas reinen Runtime-Neustart mit frischen flüchtigen Dateien bindet
`alpha-restart-active-proof.json`, SHA-256
`cbdacd18d92b0da5f473120b0521012b0c65056c543f0d542d8062ee52a23cea`.
Betas Entfernung der persönlichen Auswahl bei gleicher gemeinsamer Version
bindet `private-remove-same-active-proof.json`, SHA-256
`171b84e65c750dba0e8eb7178c7391778098528f7d74a2ca700af630d33cdfa8`.
