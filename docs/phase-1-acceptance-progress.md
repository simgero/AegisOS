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


## Beginn des integrierten Versionslaufs im neuen Image

Profil `fe26fa6c-2cb1-4b31-bf5a-bad017b6a54a`, Boot-ID
`6134993e-afda-4733-84fc-b1b990ab1323`, ursprünglicher SystemServer PID 1356.
Der vollständige Erstboot ist bestätigt; SELinux Enforcing, FBE,
Metadatenverschlüsselung und authentifiziertes ADB sind beobachtet. Ein echter
256-KiB-ADB-Rundlauf ist bytegleich. Die Bootlogs enthalten bis zu dieser
Beobachtung keinen Treffer der geprüften Fatal-/Watchdog-/Panic-Muster.
Dies ersetzt keine abschließende Dienstbewertung nach dem ganzen Testlauf.

Alpha wurde als AOSP-Admin 10/10 angelegt und blieb zunächst CE-gesperrt.
Sein erster korrekter Login ohne vorherigen Fehlversuch, eine danach stabile
Sitzung und echte GNU-Ausführung bestehen. Datei und persönliche Konfiguration
sind in GNU erzeugt; die Basis ist schreibgeschützt, interne UID/GID sind 1000,
Capabilities null, NoNewPrivs und Seccomp aktiv. Beta ist als normaler Benutzer
11/11 angelegt und noch nicht angemeldet.

Die echte CLI plant und veröffentlicht gemeinsam `jq` und `libjq1` in
`1.7.1-6+deb13u3` sowie `libonig5` in `6.9.9-1+b1` nach frischer
AOSP-Adminfreigabe. Der ursprüngliche Alpha-Kontext PID 5667 bleibt mit
derselben Startzeit erhalten; die CLI weist auf den erforderlichen Neustart
zur Aktivierung hin. `shared-v1-publication.json` belegt die Veröffentlichung.
Der nächste private CLI-Plan wählt für Alpha `jq` und `libjq1` exakt in
`1.7.1-6+deb13u4`. Tatsächliche Programmausführung beider Versionen,
Aktivierung, Beta-Erstlogin und Persistenz stehen an diesem Prüfpunkt noch aus.

Die früheren 177 Java-Komponententests sind ergänzend an identische Git-Objekte
von Service, Plattform, Tests und Storage-Registrierung gebunden
(`prior-java-source-binding.json`). Das ist keine neue Ausführung dieser
Tests und schließt die geänderten nativen Paketquellen ausdrücklich aus.


## Zwei explizite Versionen tatsächlich ausgeführt

Im selben neuen Vollimage sind nun beide ersten korrekten CLI-Anmeldungen
ohne vorherigen Fehlversuch und echte GNU-Ausführung bestätigt. Alpha führt
privat `jq`/`libjq1` `1.7.1-6+deb13u4` aus, Beta gemeinsam
`1.7.1-6+deb13u3`. Beide berechnen aus einem JSON-Array korrekt die Summe 10.
Paketdatenbank und Prüfsummen werden aus ihren gewöhnlichen GNU-Kontexten
gelesen. Das kleine `jq`-Frontend ist in beiden Debian-Revisionen bytegleich;
die tatsächlich verwendeten `libjq1`-Bibliotheken unterscheiden sich:
Alpha `92012c8c198ed5f8e44042a124c3271e89a0a2867fc3344642d9ed391ef75f50`,
Beta `58a6c82e3cc0b55f2e11e85ffa487bd2381c4cd30068874504c639c76a3e59d6`.

`two-version-execution-before-reboot.json` bindet die tatsächlichen
Terminalereignisse, Identitäten und den Boot aneinander, SHA-256
`4874794ab294b224f1809ad03541a3dae2d97982112c5afada15236c79a4377e`.
Dies schließt nur den Ausführungs-Teilfall; T15 bleibt bis zu Entfernung und
weiteren Pflichtvarianten offen. Der vollständige gepaarte Reboot steht noch aus.

Die private Veröffentlichung änderte die gemeinsame Generation nicht. Im alten
Alpha-Kontext blieb `jq` tatsächlich unauffindbar, bis dieser ausdrücklich
neugestartet wurde. Dabei blieb Alpha CE-entsperrt; ursprüngliche Datei und
Konfiguration wurden anschließend bytegleich gelesen, die alte `/tmp`-Probe
war verschwunden. Neue Proben für Konfiguration, Test-Secret, `/tmp` und `/run`
sind von beiden Benutzern in GNU erzeugt. Beide Hintergrundjobs sind auf zwei
Stunden begrenzt; gegenseitige Datei-/Prozess-/IPC-Prüfungen laufen.


## Gegenseitige Isolation und Runtime-Stopp im Versionslauf

Beide Benutzer bestehen die GNU-Lese-/Signalversuche mit lebendem fremdem
Prozess vor und nach der Aktion sowie getrennten Host-IDs und sechs getrennten
Namespaces. Zusätzliche Lese-/Schreibversuche auf Konfiguration, Test-Secrets,
`/tmp` und `/run` sind beidseitig abgewiesen; die tatsächlich vorhandenen fremden
Bytes bleiben gleich. Beide POSIX-Mqueue-Prüfungen bestehen. Beta kann auch
Alphas vorhandene private Paketauswahl nicht lesen/verändern. Beta besitzt an
diesem Punkt keine eigene private Paketauswahl; deren Fehlen zählt ausdrücklich
nicht als bestandener Metadaten-Isolationstest in Gegenrichtung.

Ein echter Login über eine zweite CLI widerruft Betas offene GNU-PTY und die
alte Terminalberechtigung. Betas ursprünglicher Hintergrundprozess bleibt mit
seiner eigenen Identität aktiv. Alphas anschließender `linux stop` entfernt
nur dessen ursprünglichen Prozess und Kontext; Sitzung, CE-Liste und Betas
ursprünglicher Prozess bleiben erhalten. Nach erneutem Start sind Alphas
persistente Proben bytegleich, alte temporäre Dateien und Message Queues weg.
Erst danach wird eine neue begrenzte Alpha-Hintergrundprobe angelegt; der
ursprüngliche Prozess wird nicht nachträglich als überlebend ausgegeben.

Beleg: `isolation-and-runtime-stop-before-reboot.json`, SHA-256
`80698c455ddf255bb297667c33756071cc9eaea56b33645d856a60a62ed8c42b`.
Bildschirmsperre, vollständiger Logout und gepaarter Neustart folgen gesondert.


## Bildschirmsperre, Passwortwechsel und Vorbereitung des gepaarten Neustarts

Im Vollimage `d1497661b70e301aec6c92190f2bb5ec58401caf` widerruft die
beobachtete Android-Bildschirmsperre Betas GNU-Terminal. Beide ursprünglichen
Hintergrundprozesse laufen mit unveränderter Identität weiter, während Android
`Asleep` und eine sichtbare Keyguard-Sperre meldet. CE bleibt dabei entsperrt;
das wird ausdrücklich nicht als Logout gewertet (`screen-lock-proof.json`).

Betas Passwortwechsel erfolgt über AOSP. Nach bestätigtem Logout scheitert das
alte Passwort ohne CE-Entsperrung; ein Runtime-Start ohne Anmeldung wird
abgewiesen. Das neue Passwort erlaubt den Zugriff auf die unveränderte
GNU-Datei, Konfiguration und gemeinsame `jq`-Version. Alpha liest anschließend
seine ursprüngliche Datei, HOME-Anpassung und private Version unverändert.
Beide Benutzer werden ausdrücklich abgemeldet: persönliche Prozesse und
Kontexte verschwinden, AOSP meldet nur CE-Benutzer 0. Bekannte persönliche
Dateien liefern keine Bytes. `identity-test/reboot-checkpoint.json` bindet
Identitäten, ursprüngliche Dateihashes, Profil und Boot an diesen Zustand.

QMP-Tastaturereignisse öffnen in Androids Einstellungen „Network & internet“;
der QMP-Mausklick auf die beobachtete Zurück-Schaltfläche führt zur Übersicht.
Gerenderte Bilder, UI-Bäume und Androids Eingabekoordinaten sind in
`qmp-ui-proof.json` verknüpft. Ein erster UI-Dump während eines Übergangs hatte
keine Wurzel; die späteren abgeschlossenen Beobachtungen belegen die Navigation.

Die 44 nativen Tests für fscrypt-Entzug, Namespaces und Speicher-/Prozessgruppen
bestehen jetzt auch direkt im vollständigen neuen Image (131 Sekunden
Beobachtungsdauer). Boot-ID, SystemServer PID 1356, gesperrte persönliche
Benutzer und leere Kontextliste bleiben vor/nach dem Lauf unverändert.
Log-SHA-256: `75884580da9b37442a3ee3affb9d11205c1f4c2b331edcfa55a4f670108b9d23`.
Das ist ein Komponentenbeleg und keine Gesamtfreigabe aller Systemfälle.

Android und KeyMint-Helfer sind danach gemeinsam sauber beendet worden; der
Launcher bestätigt beide Prozessenden mit Exitcode 0. Profilmanifest und
Dateibindungen sind in `paired-shutdown-before-reboot.json` festgehalten.
Der zweite Start verwendet dasselbe Profil ohne Neuanlage. Der Nachweis nach
diesem Neustart steht noch aus; die Testzugänge bleiben ausschließlich im
Speicher des weiterlaufenden Treibers.


## Gepaarter Neustart mit ursprünglichen Daten und beiden Paketversionen

Der zweite Boot desselben Profils ist vollständig gestartet: Boot-ID
`c85c6448-6bd3-4a70-b2fc-bdc53b6910dd`, SystemServer PID 1146. Profilmanifest,
Dateiidentitäten beider Profilhälften und AVB-Digest bleiben gebunden. ADB
funktioniert mit dem bestehenden Hostschlüssel ohne erneute Provisionierung;
der erste Transportversuch benötigte den vorgesehenen authentifizierten Retry.
SELinux, FBE und Metadatenverschlüsselung bleiben aktiv. Vor jeglicher Anmeldung
sind nur CE-Benutzer 0 und kein persönlicher Runtime-Kontext vorhanden. Bekannte
private Dateien und Alphas Paketauswahl liefern keine Bytes.

Beide ersten korrekten Anmeldungen nach diesem Neustart bestehen ohne vorherigen
Fehlversuch. Alpha liest seine ursprüngliche 1024-Byte-Datei, HOME-Anpassung,
Konfiguration und sein Test-Secret bytegleich; private `jq`/`libjq1` `u4` werden
mit derselben Bibliotheksprüfsumme tatsächlich ausgeführt. Beta liest seine
ursprüngliche Datei und privaten Proben bytegleich und führt die gemeinsame
Version `u3` aus. Beide Programme berechnen wieder die Summe 10. Die alten
`/tmp`-/`/run`-Proben bleiben verschwunden. Gemeinsame und private
Paketauswahl sind exakt dieselben wie vor dem Reboot.

Alpha wird vor Betas Anmeldung ausdrücklich abgemeldet; während Beta arbeitet,
bleibt Alpha CE-gesperrt. Betas neues Passwort funktioniert nach dem Neustart;
nach dessen erneutem Logout scheitert das alte Passwort, CE bleibt `[0]`.
Beide bekannten GNU-Dateien sind nach dem jeweiligen Logout wieder unlesbar.

Zusammenhängender Beleg mit ausgewählten tatsächlichen Terminalereignissen:
`out/phase1-dod/d149766/paired-reboot-persistence-proof.json`, SHA-256
`417a570939c8a4266915e714c7b23abbc47062f1555af42e83fa84b8d71ff78a`.
Zwei vorangehende Beobachterzugriffe verwendeten falsche gemeinsame Store-Pfade
und scheiterten vor dem Vergleich; der erfolgreiche Vergleich verwendet den
im Gast und Brokerquelltext bestätigten Pfad. Diese Orchestrierungsfehler
werden nicht als Produktfehler oder erfolgreiche Prüfungen gezählt.

Dieser Lauf schließt die beschriebenen Persistenz-/Passwortvarianten. D1–D7
und T01–T17 bleiben bis zur vollständigen ergänzenden Matrix insgesamt offen.
Insbesondere Updates, Entfernung/Rückkehr, Parallelität, Löschung/ID-Reuse
und der nachträglich angelegte Benutzer sind noch zu ergänzen.


## Private Auswahl eines normalen Benutzers mit gesperrtem Administrator

Beta hält nach dem Reboot über die echte CLI `jq=1.7.1-6+deb13u3` ausdrücklich
privat fest. Der Plan zeigt die unveränderte Version und die private Auswahl
vor der Freigabe. Alphas frische AOSP-Adminbestätigung veröffentlicht die
private Generation `f13b9e50578621cd37b36b2f3112ef181a97694aadc99ca24b22bdb085b97285`
für **Beta 11/11**, nicht für den bestätigenden Administrator. Gemeinsame
Generation und ursprünglicher Beta-Hintergrundprozess bleiben erhalten.

AOSP meldet vor und nach der Bestätigung ausschließlich CE `[0, 11]`;
Alphas bekannte private Paketauswahl bleibt unlesbar. Die Laufzeitaktivierung
dieser Auswahl und ihre Erhaltung beim gemeinsamen Update stehen noch aus.
Beleg `beta-private-owner-approval-proof.json`, SHA-256
`e40ba57a592de1125bc2fedc61aa1cd02a686b46000a8425491349adc157c108`.
Die Beobachtung erteilt dem bestätigenden Administrator kein zusätzliches
persönliches Leserecht. Die gesamte Autorisierungsmatrix bleibt offen.


## D1 erneut offen: Bootanimation-FORTIFY beim zweiten Boot

Die vollständige spätere Logprüfung findet im zweiten `d149766`-Boot einen
relevanten Absturz, der im Init-Exitcode nicht sichtbar ist. PID 983 beendet
seinen eigentlichen Renderthread 1052 und protokolliert den Objektabbau im
Hauptthread um 07:02:53 UTC. Um 07:02:57 UTC meldet ein anderer Thread 1997
`pthread_mutex_lock called on a destroyed mutex` und SIGABRT. Init protokolliert
trotzdem Exitcode 0 und beendet anschließend die Prozessgruppe. Bei der
späteren Prüfung ist kein Tombstone vorhanden.

Beleg: `out/phase1-dod/d149766/bootanimation-shutdown-failure.json` mit
`bootanimation-shutdown-failure.log`, Ausschnitt-SHA-256
`742162429c4f87da326f22ef63a33a87d6cebb5bd27d473c33f204c3342dd757`.
Die vorherige `boot->join()`-Ergänzung ist in diesem Image enthalten und reicht
für diesen Fall nachweislich nicht aus. Der Besitzer des zerstörten Mutex und
der genaue Bibliothekspfad sind noch nicht geklärt; eine reine Vermutung über
TLS-/Grafik-/Binder-Abbau ist noch keine Ursachenanalyse oder Korrektur.

**D1 ist nicht bestanden.** Exitcode 0 ist kein Ersatz für die vollständige
Fatal-/FORTIFY-Prüfung. Die erfolgreichen persönlichen Daten-, Passwort- und
Paketabläufe bleiben Teilbelege; sie ergeben keine stabile Gesamtabnahme.
Eine Korrektur, gezielte Wiederholung und erneute Prüfung der betroffenen
Boot-/Bedienungsfälle sind erforderlich. Der SystemServer blieb während des
beobachteten zweiten Ablaufs PID 1146.


## Dritter Benutzer und tatsächlicher AOSP-Hintergrundstopp

Gamma wurde erst nach den Paketinstallationen und dem gepaarten Neustart als
normaler AOSP-Benutzer 12/12 über die CLI angelegt. Sein Speicher blieb bis zur
ersten korrekten Anmeldung gesperrt. Der neue GNU-Kontext hat das unveränderte
private HOME-Grundlayout, intern UID/GID 1000 und Host-Zuordnung 1207500. Gamma
führt die gemeinsame `jq`/`libjq1`-Version `u3` mit erwarteter Bibliotheksprüfsumme
aus; die Berechnung liefert 10.

AOSP stoppt dabei Beta als Hintergrundbenutzer. Der zuvor positiv beobachtete
GNU-Prozess PID 5547 mit Startzeit 165052 verschwindet samt Kontext; Betas CE
ist gesperrt. Androids Stop-Broadcasts und Prozessabbau sind aufgezeichnet.
Das ist ein tatsächlicher AOSP-Ressourcenstopp, kein behauptetes Weiterlaufen
beim Wechsel. Ein anfänglicher Beobachter erwartete fälschlich drei gleichzeitig
entsperrte persönliche Benutzer und verweigerte den Nachweis; die korrigierte
Beobachtung benennt die tatsächlich aktiven Benutzer.

Gammas GNU-Leseversuche auf beide fremden Dateien, Konfigurationen, Test-Secrets
und privaten Paketauswahlen liefern keine Bytes. Alphas tatsächlich vorhandene
Dateien bleiben bei entsperrtem CE vor/nach den Versuchen bytegleich. Beta ist
bei dieser Prüfung bereits AOSP-gesperrt; das zählt nicht als zusätzlicher
Gamma/Beta-Isolationstest bei gleichzeitig entsperrten Speichern. Dessen
späterer Datenwiederzugriff nach dem Ressourcenstopp bleibt gesondert zu prüfen.

Beleg: `third-user-and-background-stop-proof.json`, SHA-256
`8ee5be06db6393949ea9192144d34b787e5da3e66d9d2082300816466e0831d7`.
Der separat dokumentierte Bootanimation-Absturz hält D1 weiter offen.
