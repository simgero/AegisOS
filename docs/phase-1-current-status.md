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
| T03 Identität/Namespaces | Beide GNU-Kontexte intern UID/GID 1000, getrennte Hostbereiche und sechs Namespaces; konkrete Prozessidentitäten und Wechsel erhalten. | Abschließender Abgleich der Identitätsbindung über die geforderten Wechsel- und Lebenszyklusfälle. |
| T04 Startbedingungen | Start/Shell nach falschem Passwort beziehungsweise ohne Terminalautorisierung verweigert. | CE-Sperrung unabhängig von fehlender CLI-Anmeldung prüfen; fehlende Namespaces, ungültige Zuordnung und jede erforderliche Sicherheitsvoraussetzung gezielt nachweisen. Kein schwächerer Ersatzbetrieb. |
| T05 Shell | Tatsächliche GNU-Ausführung für Alpha, Beta und Gamma; eigenes HOME/Identität; Shell-Ende erhält Alpha-Kontext und AOSP-Sitzung. | Vollständige Zuordnung des fremden/gesperrten Shell-Ziels sowie der Exitcode-/Terminalfälle. |
| T06 Gegenseitiger Zugriff | Konkrete HOME-, Konfigurations-, synthetische Secret-, temporäre Datei-, Prozess- und POSIX-Mqueue-Prüfungen bei gleichzeitig entsperrtem Alpha/Beta. Vorhandene private Paketstores inzwischen in beiden Richtungen geprüft. | Abschließender Abgleich aller geforderten Pfade und Ressourcen mit diesen konkreten Prüfumfängen. Keine pauschale Aussage über beliebige Syscalls oder IPC-Verfahren. |
| T07 Persistenz | Ursprüngliche Dateien, Einstellungen und unterschiedliche jq/libjq-Versionen nach Runtime-Neustarts und gepaartem VM-Neustart erhalten; ursprüngliche flüchtige Proben nach Reboot abwesend. | Vollständige Zuordnung der Reinigung von `/tmp` und `/run` nach einem reinen Kontextneustart mit unmittelbar zuvor vorhandenen Proben. Bereits vorher abwesende Proben zählen nicht erneut als Reinigung. |
| T08 Wechsel/Sperre/Ressourcen | Hintergrundfortschritt mit konkreter Identität, Bildschirmsperre ohne behaupteten CE-Entzug; AOSP stoppt Beta beim Erreichen seines Benutzerlimits; frische Anmeldung stellt ursprüngliche Daten wieder bereit. | Verbleibende Wechselrichtungen/Terminalwiderrufe vollständig zuordnen. Der Ressourcenfall belegt zwei gleichzeitig laufende persönliche Benutzer, nicht drei. |
| T09 Runtime-Stopp | Reguläre eigene Stopps bei Paketaktivierung erhalten AOSP-Sitzung/CE und den anderen Kontext; Prozessende mit PID plus Startzeit geprüft. | Alle flüchtigen Ressourcen sowie beide Richtungen mit gültigen Fortschrittsproben vollständig zuordnen. Abgelaufene Proben beweisen keine Wirkung des Stopps. |
| T10 Logout | Direkter Beta- und Alpha-Logout ohne vorheriges `linux stop`: CE gesperrt, ursprüngliche Prozesse/Kontexte entfernt, bekannte Originaldateien unlesbar; spätere positive Rücklesungen vorhanden. | Vollständiger Abbau offener Zugriffe, Mounts und IPC; konkurrierender Start und Vordergrundwechsel während Abmeldung. Der fehlgeschlagene zusätzliche Loop-Beobachter bleibt ein Teilprotokoll. |
| T11 Logoutfehler | Erfolgreiche direkte Abmeldungen sind dokumentiert. | Gezielte ausstehende und fehlgeschlagene CE-Sperrung, sicherer Wiederanlauf nach Ursachenbehebung sowie Paketaktionen beim Logout auf diesem Image. Die frühere ungeklärte f098-Abmeldung ist weder ein aktueller bestandener Test noch eine nachgewiesene Fehlerbehebung. |
| T12 Reboot/Löschung | Android und KeyMint-Helfer geordnet gestoppt; gleiches Profilpaar, neue Boot-ID; beide persönlichen Speicher vor Anmeldung gesperrt und Originaldaten danach erhalten. | CLI-Benutzerlöschung, Ende alten Schlüsselzugriffs und privater Zuordnungen sowie neuer Benutzer/kontrollierte ID-Wiederverwendung. |
| T13 Paketautorisierung | Gültige gemeinsame/persönliche Installation und Entfernung; gültiges gemeinsames Update bei beiden Benutzern aktiviert. Je drei Ablehnungen für `install all`, `install user`, `update all`, `update user`. Beta besitzt private Installation trotz abweichendem Admin Alpha; dessen CE blieb während dieses Falls gesperrt. | Gültiges `update user`; sechs Ablehnungen für beide Entfernungsbereiche; vollständige Aufrufer-/Eigentümerbindung und CE-Zuordnung über die gesamte Aktionsmatrix. CLI-Parserablehnung allein belegt keine vollständige Serviceidentitätsprüfung. |
| T14 Paketbereiche | Gemeinsame Software bei bestehenden Benutzern und nachträglich angelegtem Gamma tatsächlich ausgeführt; getrennte persönliche Einstellungen; Gamma kann sieben bekannte A/B-Pfade nicht lesen. | Abschließende Zuordnung aller privaten Änderungs-/Einstellungsvarianten. Während Gammas Prüfung war Beta durch AOSP gestoppt; daraus folgt kein gleichzeitiger Drei-Benutzer-Nachweis. |
| T15 Private Versionen | Dasselbe jq mit passenden libjq-Abhängigkeiten in u3/u4 tatsächlich ausgeführt; private Version hat Vorrang. Alpha entfernt private u4 mit ausdrücklich angekündigtem Rückfall auf gemeinsame u3. | Nicht verfügbare Version, unauflösbare Abhängigkeiten und private Entfernung bei bereits gleicher gemeinsamer Version ohne stille Ersetzung/Restbindung. |
| T16 Aktivierung | Gemeinsames Update veröffentlicht, beide laufenden Kontexte unverändert; beide ersten neuen Starts erfolgreich. Alpha führt gemeinsame jq/libjq u4 aus, Beta behält ausdrücklich private u3. Beide übernehmen neue Basisbibliotheken und behalten Originaldaten; vollständige Paketbestände und Bindung an die neue Basis geprüft. | Abschließende Zuordnung des konkreten bestandenen Updatefalls zu allen Detailanforderungen; weitere geforderte Konfliktfälle bleiben gesondert erforderlich. |
| T17 Fehler/Parallelität | Abbruch der dokumentierten Pläne vor Adminfreigabe erhält den vorherigen Zustand. | Gleichzeitige gemeinsame/private Aktionen, verschiedene Antragsteller, Abbruch während Ausführung, Installationsfehler in den erforderlichen Phasen und Logout während einer Transaktion; sichtbarer konsistenter Abschluss oder Reparaturbedarf. |

## D1–D7 und Referenzablauf

Alle sieben Abschlusskriterien bleiben offen. Außer den obigen Varianten
gehören dazu weiterhin:

- **D1:** vollständiges reproduzierbares Source-/Build-/Startinventar und
  relevante Dienststabilität über den Abnahmelauf. Bildschirm, Eingabe und
  authentifiziertes ADB sind belegt. Die Boot-3-Beobachtung bis 07:27 UTC ist
  begrenzt; zwei Starthelfer-Erklärungen bleiben Schlussfolgerungen. Der frühe
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
