# Vollständige Phase-1-Abnahme: Arbeitsstand

Beginn: 1. Oktober 2026. Ziel ist die vollständige
[DoD](architecture/phase-1-dod.md), einschließlich aller Varianten T01–T17.
**Status: aktiv, keine vollständige Abnahme.**

Ausgangspunkt ist das geprüfte Image `c52657113becde42d735669d4d64405c7740cc74`.
Die [fünf bisherigen Meilensteine](server-acceptance.md) bleiben gültige
Teilbelege. Die neue Prüfung verwendet ein frisches Profil unter
`out/phase1-dod/c526571/`; bestehende Profile und Logs bleiben erhalten.
Ein nötiger Produktfix erhält einen neuen Build und betroffene Nachweise
werden auf diesem Stand erneut erhoben.

## Anforderung und noch benötigter Nachweis

Diese Tabelle ist eine Arbeitsliste, kein PASS-Manifest. „Teilbeleg“ bedeutet
nicht, dass die ganze Zeile der DoD bereits geschlossen ist.

| ID | Vorhandene Grundlage | Für die vollständige Abnahme zu ergänzen/zuordnen |
| --- | --- | --- |
| T01 | Zwei echte CLI-Benutzer, AOSP-Authentifizierung, Logscan | Vollständige Zuordnung der Transport-/History-/Dateiprüfungen und keine zweite persönliche Identitätsquelle |
| T02 | Neues Passwort, altes abgewiesen, Datei nach Reboot identisch | In den finalen Ergebnisindex übernehmen; bei Produktänderung betroffene Abläufe wiederholen |
| T03 | UID/GID, sechs getrennte Namespaces, zwei fortlaufende Prozesse | Nachweise zum finalen Versions-/Referenzablauf zuordnen |
| T04 | Admission-/Namespace-/Mapping-Komponententests | Gezielte Gasttests für alle fehlenden Voraussetzungen und fehlende Berechtigung zuordnen/ausführen |
| T05 | GNU-Shell, Exit und Zugriff auf eigene Datei | Ablehnungen für gesperrte/fremde Kontexte und Exit-Lebenszyklus zum finalen Stand binden |
| T06 | Gegenseitige Dateizugriffe, Signalversuche, POSIX-Mqueues | Konfiguration, private Paketmetadaten, temporäre Dateien und Test-Secrets beidseitig ergänzen |
| T07 | Datei, Alpha-Konfiguration, private Installation nach Reboot | Beide Konfigurationen, `/tmp`/`/run`-Erneuerung und abweichende Paketversion nach Neustart |
| T08 | Hintergrundbetrieb; ältere Bildschirmsperrtests | Sperre und AOSP-Ressourcenstopp im aktuellen Image mit tatsächlichen Zuständen prüfen |
| T09 | Runtime-Start/-Stopp vorhanden | Nur eigener Kontext beendet, Sitzung/CE und anderer Benutzer erhalten |
| T10 | Bestätigter Logout, CE gesperrt, Peer weiter aktiv | Konkurrierenden Start und vollständigen Ressourcenabbau gezielt zuordnen/prüfen |
| T11 | Echter EBUSY-/CE-Timeout mit sicherem Wiederanlauf | Zusammenwirken mit laufenden Paketaktionen ergänzen |
| T12 | Gepaarter Reboot; frühere CLI-Löschung und Allocator-Tests | Vollständige aktuelle Löschung, neue Identität, ID-Stilllegung bis Systemserver-Ende und sichere Wiederverwendung nach Neustart |
| T13 | Installation beider Bereiche mit gültiger/falscher/Nicht-Adminfreigabe | Alle sechs Aktion-/Bereichskombinationen, fehlende Autorisierung/Bereiche und manipulierte Eigentümer |
| T14 | Gemeinsames ed und privates hello | Nachträglich angelegter Benutzer C und getrennte persönliche Konfiguration gemeinsamer Software |
| T15 | Private Installation eines anderen Programms | Dasselbe Paket in zwei Versionen samt Abhängigkeiten, unauflösbare Version, private Entfernung/Rückkehr |
| T16 | Ausstehende Aktivierung bei laufendem Kontext | Gemeinsames Update bei privater Version und konsistenter Neustart oder erklärter Konflikt |
| T17 | Abgelehnte Paketfreigaben erhalten laufende Kontexte | Parallelität, Abbruch, Installationsfehler und Logout in unterschiedlichen Transaktionsphasen |

## Paketversionen

Die bestehenden Produktquellen bleiben unverändert: Debian trixie,
trixie-updates und trixie-security über HTTPS mit Debian-Signaturen.
Ein lesender Hostvergleich findet `jq` und `libjq1` jeweils in
`1.7.1-6+deb13u3` sowie `1.7.1-6+deb13u4`. `jq` fordert die exakt passende
`libjq1`-Version. Dadurch ist die Prüfung nicht auf zwei voneinander unabhängige
Programme oder bloß unterschiedliche Metadaten reduziert.

Index-URLs, SHA-256 und Kandidaten liegen unter
`out/phase1-dod/package-research/`. Diese Vorauswahl ist noch kein
Installationsnachweis: Der echte Gastplaner muss Signaturen, Verfügbarkeit
und Abhängigkeiten prüfen; anschließend sind Programme und Paketdatenbank
im jeweiligen GNU-Kontext zu beobachten. Fehlende Archive oder inzwischen
geänderte Quellen werden als echte Testvoraussetzung behandelt, nicht durch
unverifizierte Downloads oder einen zusätzlichen Produkt-Testschlüssel ersetzt.

## Nachweisführung

Der finale Ergebnisindex muss die einzelnen Varianten jeder DoD-Zeile mit
Erwartung, tatsächlichem Ergebnis, Image-/Profil-/Bootbindung und Hashes
enthalten. Ein noch fehlender Teilfall hält die betreffende Zeile offen.
Ältere Komponententests bleiben als solche gekennzeichnet. Der vollständige
Referenzablauf mit unterschiedlichen Versionen desselben Pakets wird getrennt
vom bisherigen ed/hello-Durchlauf nachgewiesen.


## Benutzer-ID-Wiederverwendung

Die geprüfte AOSP-Anpassung hält entfernte Nummern für die Lebensdauer des
Systemservers zurück, auch bei erschöpftem Nummernraum. Das ist keine
persistente Reservierung über einen Neustart. Die Abnahme muss deshalb
zusätzlich die abgeschlossene CLI-Löschung, einen Neustart und die Anlage
einer neuen Identität prüfen. Wird die alte Nummer wiederverwendet, müssen
Seriennummer, Schlüsselzugriff und private Daten eindeutig zur neuen Person
gehören; alte Pfade und Runtime-Zuordnungen dürfen keinen Zugriff eröffnen.


## Erste ältere-Version-Regressionsausführung

Der neue Gast startet mit SELinux Enforcing, FBE und authentifiziertem ADB;
SystemServer PID 1358, Profil `3a50ba79-8e0c-4ff6-99b5-1688ddcc09b3`.
Bootanimation Status 0. Der erste native Lauf ist **nicht bestanden**:
Der unveränderte Vergleichsfall überschreitet seine zehnsekündige Wartefrist;
der neue Test übergab irrtümlich 30000 ms an eine API, die höchstens 10000 ms
pro Beobachtung akzeptiert. Damit ist noch kein Ergebnis zur Versionsauflösung
belegt. Log und JSON bleiben unter `out/phase1-dod/c526571/` erhalten.

Die zwei Testfälle beobachten nun denselben gehaltenen Worker wiederholt mit
zulässiger Einzelwartezeit, insgesamt begrenzt auf 180 Sekunden. Nur ETIMEDOUT
wird erneut beobachtet; andere Fehler werden sofort gemeldet. Produktionscode,
Startup-/Cleanup-Fristen und Signaturprüfung bleiben unverändert. Erst der
Folgelauf kann den eigentlichen Versionsfall bestätigen oder widerlegen.


## Reproduzierter Versionsfehler und gezielte Korrektur

Mit korrekter begrenzter Beobachtung besteht der unveränderte Vergleichsfall.
Die explizite ältere Version scheitert dagegen reproduzierbar mit APT-Status
100: `aegis-probe-app` benötigt Bibliothek 1, APT wählt Kandidat 2.
`version-regression-bounded.log` und die zugehörige JSON-Datei binden den
Fehler an das aktuelle Image; SystemServer bleibt PID 1358.

Die Korrektur verwendet den bereits für Reconciliation eingesetzten APT-3.0-
Solver auch bei expliziten Versionswünschen und lässt passende ältere
Abhängigkeiten zu. Unversionierte normale Aktionen behalten ihre bisherige
Konfiguration. Der reine Downloadplan akzeptiert dabei auch ausdrücklich
angeforderte Downgrades; die tatsächliche Installation benötigt weiterhin den
vollständigen gebundenen Plan und frische AOSP-Adminfreigabe. Quellen-, TLS-,
Signatur- und exakte Versionsprüfung werden nicht abgeschwächt.

Zusätzliche Gerätetests prüfen einen Downgrade von bereits vorhandener Version
2 samt Bibliothek auf Version 1 sowie die Ablehnung einer nicht verfügbaren
Version ohne stillen Ersatz. Die Korrektur ist vor dem Folgelauf noch nicht
als bestanden oder als neuer Image-Abnahmestand auszugeben.


## Verifizierter Versionsfix und neues Image

Am 2. Oktober 2026 bestehen alle vier nativen Versionsfälle auf Quellstand
`d1497661b70e301aec6c92190f2bb5ec58401caf`: Vergleichsfall, explizite ältere
Version mit passender Abhängigkeit, Downgrade beider bereits vorhandenen
Pakete sowie Ablehnung der nicht verfügbaren Version ohne stillen Ersatz.
Der Lauf dauert 160 Sekunden; SystemServer bleibt PID 1358.
`out/phase1-dod/c526571/version-regression-fixed.json` bindet das Ergebnis
an Profil, Boot-ID und Log-SHA-256
`c98d0556b0eea41b68653f5174e0895ac2fbb7fa30a2cbb17d1dde9e2b42313b`.
Es sind aktualisierte Komponenten im vorherigen Image, keine vollständige
Image-Abnahme. Der fehlgeschlagene Vorlauf bleibt als Regressionsevidenz erhalten.

Die Hostprüfung desselben Quellstands ergibt 276 bestandene Tests und vier
plattformbedingte macOS-Auslassungen. Der vollständige lokale Build
`/srv/aegis/runs/local-20261001T235735Z-d1497661-oQgZGF` endet mit
`LOCAL_BUILD_VERIFIED`; alle 20 Image-Prüfsummen stimmen. Images und
Prüfbelege bleiben lokal. Die echte CLI-Installation beider jq-Versionen und
der vollständige Referenzablauf auf diesem neuen Image stehen noch aus.


Auch die acht Fälle `RuntimePackageResolver.*` bestehen mit den neuen
Komponenten (277 Sekunden): unveränderliche Planerbasis, unsigniertes und
abgelaufenes Repository, veränderter authentifizierter Index, verändertes
Archiv, manipulierte Planerkonfiguration, abhängige Entfernung und Update.
Log-SHA-256: `d019fdb6ed86e6d7a6e24232b2dfa4103f9b7a089962c11f7ea83e46821c2e87`.
Der Beobachter bestätigt denselben Boot und SystemServer. Ein vorheriger
Kopierversuch in das bereits belegte ADB-Ziel scheiterte vor Testbeginn; der
erfolgreiche Lauf verwendet ein separates neues Komponentenverzeichnis.
Das diagnostische Android-/KeyMint-Paar wurde anschließend sauber gestoppt.

Das neue Vollimage wird separat unter `out/phase1-dod/d149766/` vorbereitet:
AVB-Digest `b4ff48eb9f5e51cc043e11b7e1dacd19727111eada494ee4eeded0e1a7753450`,
Disk-SHA-256 `30fcbd8b9fe74e5c7571b371599f52ff1c61565e8fb3b7c4ae041085f0b85719`.
Der erste Boot mit neuem Profil ist gestartet, die integrierte Abnahme bleibt
offen. Neue Treibersteuerungen für beidseitige Konfiguration/Secrets,
flüchtige Dateien und Runtime-Stopp haben eine Syntaxprüfung; sie gelten
erst nach tatsächlicher Ausführung als Gastnachweis.
