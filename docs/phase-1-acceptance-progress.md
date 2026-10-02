# Vollständige Phase-1-Abnahme: Arbeitsstand

Beginn: 1. Oktober 2026. Ziel ist die vollständige
[DoD](architecture/phase-1-dod.md), einschließlich aller Varianten T01–T17.
**Status: aktiv, keine vollständige Abnahme.** Aktueller integrierter Prüfstand:
Image `31551159cd11d66b0fa18442b5f62106edf4bfb5`, Profil
`1c53b76e-bb1d-4ed6-a0df-7bbc21f2cfee`. Erster normaler Boot, Bedienung,
ADB und 27 gezielte Java-Tests sind bestanden. Der gepaarte Wiederholungsstart
mit bytegleichen persönlichen Daten und getrennten jq-Versionen ist ebenfalls
belegt. Gemeinsames Update mit privatem Abgleich und die vollständige Testmatrix
bleiben offen.
Der frühere Bootanimation-Absturz auf `d149766` bleibt als Regressionsevidenz erhalten.

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
| T06 | Gegenseitige Dateien, Konfiguration, Test-Secrets, temporäre Dateien, Prozesse und POSIX-Mqueues | Zusätzliche Gegenrichtung für Betas inzwischen vorhandenen privaten Paketbestand; alle Varianten zum finalen Image binden |
| T07 | Beide ursprünglichen Dateien/Konfigurationen, private Version und getrennte gemeinsame Version nach Reboot; alte `/tmp`-/`/run`-Proben fehlen | In den finalen Ergebnisindex übernehmen; bei betroffenen Produktänderungen erneut prüfen |
| T08 | Bildschirmsperre bei weiterlaufender Arbeit; tatsächlicher AOSP-Stopp Betas beim Start Gammas; spätere Datenwiederherstellung | Einzelne Wechsel-/Hintergrundvarianten vollständig zum finalen Ergebnisindex zuordnen |
| T09 | Eigener Kontext gestoppt, Sitzung/CE und ursprünglicher Peer-Prozess erhalten | Belege zum finalen Image zuordnen; kein allgemeiner Logout-Nachweis |
| T10 | Bestätigter Logout, CE gesperrt, Peer weiter aktiv | Konkurrierenden Start und vollständigen Ressourcenabbau gezielt zuordnen/prüfen |
| T11 | Echter EBUSY-/CE-Timeout mit sicherem Wiederanlauf | Zusammenwirken mit laufenden Paketaktionen ergänzen |
| T12 | Gepaarter Reboot; frühere CLI-Löschung und Allocator-Tests | Vollständige aktuelle Löschung, neue Identität, ID-Stilllegung bis Systemserver-Ende und sichere Wiederverwendung nach Neustart |
| T13 | Installation beider Bereiche mit gültiger/falscher/Nicht-Adminfreigabe | Alle sechs Aktion-/Bereichskombinationen, fehlende Autorisierung/Bereiche und manipulierte Eigentümer |
| T14 | Gemeinsame jq-Version bei Beta und nachträglich angelegtem Gamma; Alphas private Variante und getrennte Konfiguration | Varianten und Eigentumsnachweise vollständig zum finalen Ergebnisindex zuordnen |
| T15 | Dasselbe jq mit passender libjq1 in u3/u4 tatsächlich ausgeführt, auch nach Reboot; erfolglose Versionsanforderung erhält Bestand | Genaue Ursache der abgewiesenen Versionsanforderung und unauflösbare Abhängigkeit; private Entfernung/Rückkehr |
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

## Wiederanmeldung nach AOSP-Ressourcenstopp und private Aktivierung

Beta meldet sich nach dem oben belegten automatischen AOSP-Hintergrundstopp
mit dem geänderten Passwort erneut an. Vor der Passwortprüfung ist sein CE
weiter gesperrt und kein Kontext vorhanden; danach bleibt die Sitzung gültig.
Der neue Kontext meldet `packages=current`. Seine tatsächlich aktive Root-Mount
verweist auf Betas zuvor veröffentlichte private Generation `f13b9e50…`; die
private und gemeinsame Auswahl sind unverändert. Der SystemServer bleibt
PID 1146, die Boot-ID unverändert, nur Betas persönliches CE ist entsperrt.

Aus der echten GNU-Shell liest Beta die ursprüngliche 1024-Byte-Datei sowie
Konfiguration und synthetisches Secret bytegleich. Die alten `/tmp`-/`/run`-Proben
fehlen. Private `jq` und `libjq1` in Version `1.7.1-6+deb13u3` sind ausführbar;
die erwartete Bibliotheksprüfsumme und das Rechenergebnis 10 werden geprüft.
Damit ist auch der zuvor offene Wiederzugriff nach dem Ressourcenstopp belegt.
Die Erhaltung dieser privaten Auswahl bei einem gemeinsamen Update steht noch aus.

Beleg: `out/phase1-dod/d149766/beta-resource-stop-recovery-proof.json`, SHA-256
`1cc824fd4ffc03a5e84a7f214b7ffcb4648b37c698c5cf0386f0a215130a9d98`.

## Abgewiesene Versionsanforderung erhält den laufenden Bestand

Die tatsächliche CLI-Anforderung `linux package install jq=0.aegis-unavailable
--scope user` endet ohne Adminabfrage mit einem sichtbaren Planungsfehler.
Auch `linux package status` zeigt den Fehler. Gemeinsame und private Auswahl,
aktive Root-Mount und ursprünglicher Runtime-Initprozess bleiben identisch;
`linux status` meldet weiterhin `packages=current`. Die anschließende GNU-Shell
führt das bisherige `jq` samt passender Bibliothek in Version `u3` erfolgreich aus.

Beleg: `out/phase1-dod/d149766/unavailable-version-preservation-proof.json`,
SHA-256 `d0d6d193db9028f7e9bb611e44028d67ea86dec5328f8f49dcbde915a7ce9632`.
**Grenze:** Die CLI nennt nur einen allgemeinen Planungsfehler. Dieser Lauf
beweist Ablehnung und Erhalt des Bestands, aber allein nicht die genaue
APT-Fehlerursache. T15 ist damit noch nicht vollständig abgenommen.

## Ergänzende Bootanimation-Diagnose ohne Reproduktion

Sechs kurze und ein vollständiger eigenständiger Animationslauf beenden sich
mit Exitcode 0 ohne erneute FORTIFY-Meldung. Der vollständige Lauf spielt den
ersten Abschnitt ab und beendet sich anschließend über die normale
Exit-Eigenschaft. Diese Diagnose lief im Entwicklungs-root-Kontext, nicht als
Init-Dienst; sie ersetzt keinen Boot-Regressionsnachweis. Der zusätzliche
Diagnosecode verändert keine Schutzprüfung. Der beim zweiten Boot tatsächlich
beobachtete Absturz bleibt ungeklärt, D1 bleibt offen.

Lokaler Abschlussbeleg: `out/phase1-dod/bootanimation-diagnostic/full-animation-1.json`;
zugehöriger Logcat-SHA-256
`8f43cb0e538b67d94ebba83ade9616a4bcdf06222ce54bb99ea4ee562b6e5393`.

## Gemeinsames Update ohne Adminfreigabe abgebrochen

Beta fordert über die echte CLI `linux package update --scope all` an. Der
vollständige Plan zeigt fünf Aktualisierungen, darunter `jq`/`libjq1` von `u3`
auf `u4`. Der leere Adminname bricht vor der Passwortabfrage ab; keine
Adminfreigabe wird übergeben. Die CLI bestätigt keine Veröffentlichung.

Gemeinsame und private Paketauswahl, aktive Root-Mount und ursprünglicher
Runtime-Initprozess sind vor/nach dem Abbruch identisch. CE bleibt `[0, 11]`,
der SystemServer PID 1146. Der Kontext meldet `packages=current`; Betas
private `jq`-/`libjq1`-Version `u3` ist danach mit unveränderter Bibliotheksprüfsumme
ausführbar und berechnet 10.

Beleg: `out/phase1-dod/d149766/update-all-missing-approval-proof.json`, SHA-256
`96adfa3e6824aec492290d4126fc7b15b9c6edc3e7bedb9300aa38ae5964af51`.
Das deckt `update/all` ohne Freigabe und Abbruch bei der Planprüfung ab.
Falsche/Nicht-Adminfreigabe, erfolgreiche Aktualisierung und Abbruch während
der Installation bleiben separate offene Fälle. Ein anschließender Scan
findet keine vollständigen Testpasswörter in den sieben vorhandenen Bootlogs;
dies ersetzt nicht die noch offene umfassende Transport-/Dateiprüfung T01.

## Gemeinsames Update: falsches Passwort und Nicht-Admin abgewiesen

Zwei weitere vollständige `update --scope all`-Pläne zeigen die gleichen fünf
Aktualisierungen. Alphas falsches Passwort wird ausdrücklich von AOSP abgewiesen.
Betas korrektes Passwort erteilt als Nicht-Admin ebenfalls keine Freigabe.
Gemeinsame/private Auswahl, aktive Root-Mount und ursprünglicher Initprozess
bleiben nach jedem Versuch unverändert. Nur Betas persönliches CE ist entsperrt.
Seine Sitzung ist anschließend weiterhin nutzbar; seine bisherige private
`jq`-/`libjq1`-Version `u3` führt die geprüfte Berechnung erfolgreich aus.

Die allgemeine CLI-Fehlermeldung beim Nicht-Admin-Versuch empfiehlt eine erneute
Anmeldung. Der tatsächlich folgende Status und GNU-Zugriff belegen aber keine
Abmeldung; die Dokumentation behauptet hier keinen Sitzungsentzug.

Beleg: `out/phase1-dod/d149766/update-all-denied-approvals-proof.json`, SHA-256
`c42bd2e1506ada3f3aebfd1e4a8539797e456b4288fdd1df28c97ce62b09ea84`.
Zusammen mit dem vorherigen Abbruch sind für `update/all` die Varianten fehlende,
falsche und Nicht-Adminfreigabe belegt. Die positive Aktualisierung und die
anderen Aktion-/Bereichskombinationen sind dadurch nicht abgenommen.

## T01: Quellprüfung und tatsächliche technische Konten

Die zum Image `d149766` identischen CLI-/Terminalquellen lesen Passwörter über
eine eigene Eingabe ohne Terminal-Echo und bereinigen ihre temporären Puffer.
Die Session-/Paket-AIDLs sind als sensibel markiert; der lokale LockSettings-
Transport serialisiert die Credential-Objekte und setzt `FLAG_CLEAR_BUF`.
Drei tatsächlich bestandene frühere `CredentialTransportTest`-Fälle werden
mit identischem Transport-/Testquelltext und geprüftem Original-Loghash gebunden.
Das ist keine erneute Java-Testausführung.

Eine lesende Gastbeobachtung bestätigt die drei festen CLI-Startargumente ohne
Benutzerargumente. Im tatsächlich aktiven Beta-Root sind sämtliche Passwortfelder
der 19 technischen Benutzer und 39 Gruppen gesperrt; UID/GID 1000 gehört dem
generischen Eintrag `runtime`. Der Beleg enthält keine Shadow-Inhalte oder
Passwortwerte. Die Erzeugungsquellen prüfen gesperrte technische Konten und
lokale NSS-Dateien; persönliche Passwortprüfung bleibt bei AOSP.

Beleg: `out/phase1-dod/d149766/credential-source-review.json`, SHA-256
`7c8920dee33cac0e9ee8cb36df7c6b35a21f1306f05359d06e5fb4161c517626`,
mit gehashtem `credential-structure-observer.json`.
**Offen:** Eine punktuelle Argument-/Kontenprüfung und Quellprüfung ersetzen
keinen vollständigen History-/Umgebungs-/Dateiscan über die Anmeldeabläufe.
T01 bleibt bis zur vollständigen Zuordnung und Prüfung offen.

## Gemeinsames Update veröffentlicht; T16-Aktivierung noch fehlgeschlagen

Der regulär mit Alphas AOSP-Passwort freigegebene gemeinsame Updateplan wird
veröffentlicht: neue gemeinsame Generation `ce7bc27b…`. Betas laufender Kontext
bleibt auf seiner unveränderten privaten Generation; seine bisherige `u3`-Version
wird nach der Veröffentlichung erfolgreich ausgeführt. Der Status zeigt korrekt
`activation-pending`, nur Betas persönliches CE ist entsperrt.

Nach `linux stop` beginnt ein frisch authentifizierter zweiter CLI-Kanal den
Start samt privatem Abgleich. Der Aufruf bestätigt nach genau zwei Minuten
keinen Erfolg. Derselbe Planungsprozess ist danach noch aktiv; später ist er
beendet, ohne dass ein Runtime-Kontext entstanden oder die private Auswahl
veröffentlicht worden wäre. Der Status bleibt `sealed`, `packages=not-active`.
Der Quelltext von `RuntimeStartWaiter` begrenzt den gesamten Start auf zwei
Minuten; schon die tatsächliche Paketplanung braucht hier etwa vier bis fünf
Minuten. Ein erneuter Start wird während des laufenden Auftrags nicht erzeugt.

Beleg: `out/phase1-dod/d149766/common-update-activation-timeout.json`, SHA-256
`b2d5638a3900224d77b01caca2a4d4d191e0cb65795abde1f84f58647ee7d15e`.
Gemeinsame Veröffentlichung und unveränderter laufender Kontext sind Teilbelege.
**Die Aktivierung ist nicht bestanden.** Das ist kein erklärter Versionskonflikt
und wird nicht als zulässiger Ersatz für T16 gewertet. Der Start-/Fortsetzungsweg
muss korrigiert und im passenden Image erneut geprüft werden.

## EGL-Cache-Lebenszyklus reproduziert

Die gezielte Prüfung der unveränderten Gastbibliothek reproduziert einen
vier Sekunden verspäteten Zugriff auf den zerstörten Cache-Mutex, ebenfalls
bei Exitcode 0. Eine Korrektur der Singleton-Lebensdauer ist jetzt integriert;
Gastvergleich und vollständige Bootregression stehen aus.
[Ursache, Beleg, Korrektur und Wiederholung](egl-cache-lifetime.md).

## Startwartezeit nach langsamer Paketplanung

Der Start wartet künftig insgesamt höchstens 15 Minuten auf denselben Auftrag.
Die ARM64-TCG-Beobachtung mit vier bis fünf Minuten Planung überschreitet die
bisherigen zwei Minuten bereits vor Installation und abschließender Prüfung.
Einzelne native Aufrufe behalten die kürzere CE-Zugangsfrist. Vor jeder
Fortsetzung prüft der Dienst die ursprüngliche Anmeldung und ihre Freigabe;
eine spätere Anmeldung darf einen widerrufenen Start nicht übernehmen.

Drei ergänzende Komponententests mit synthetischer Uhr prüfen lange Planung
und Installation im selben Auftrag, die unverlängerbare Gesamtfrist sowie
Abmeldung während der Wartephase. Der Hosttreiber wartet beim Start bis zu
16 Minuten auf die abschließende Antwort. Syntaxprüfung und Quellprüfung sind
erfolgt; Android-Kompilierung, Testausführung und echte Paketaktivierung im
korrigierten Vollimage stehen noch aus. T16 bleibt offen.

Die vorherigen Treiber-/VM-Handles sind bei der Fortsetzung nicht mehr vorhanden;
auch die Prozessliste und ADB bestätigen keinen laufenden Gast. Die ausschließlich
im Treiber gehaltenen synthetischen Passwörter sind damit nicht mehr verfügbar.
Profile und bisherige Belege bleiben erhalten. Neue Anmeldetests beginnen mit
einem frischen Profil; ein solcher Lauf zählt nicht als Fortsetzung der früheren
Passwort- oder Bytepersistenznachweise.

## Korrigiertes Vollimage 3155115 gebaut; regulärer Gastlauf begonnen

Der lokale Run
`/srv/aegis/runs/local-20261002T120452Z-31551159-qqHuxm` endet mit
`LOCAL_BUILD_VERIFIED` und Exitcode 0. Quellstand ist
`31551159cd11d66b0fa18442b5f62106edf4bfb5`, einschließlich EGL-Lebensdauer-
und Startwartezeitkorrektur. Alle 20 Image-Prüfsummen stimmen; die integrierte
Debian-Basis wurde in der ausgelieferten Partition geprüft. Kernel- und
Runtime-Eingaben sind gegenüber `d149766` bytegleich gebunden.

Die dazugehörige `AegisIdentityTests.apk` ist erfolgreich kompiliert;
Quellinventar und Image stimmen überein. APK-SHA-256:
`ce0cb926dfbf6ba624ca749cc070884290bd7513af16018ec00e59e8e1b4e0dd`.
Die elf `RuntimeStartWaiterTest`- und 16 `RuntimeAdmissionTest`-Fälle sind
für die nächste Gastausführung ausgewählt, **noch nicht ausgeführt**.

Der reguläre Erststart verwendet das neue Profilpaar
`1c53b76e-bb1d-4ed6-a0df-7bbc21f2cfee` unter
`out/phase1-dod/3155115/profile`. Die AEGIS-Bootgrafik ist in 720 × 1280
sichtbar; Android registriert noch die Systempakete. Der ADB-Beobachter wartet
auf `sys.boot_completed=1`. Das ist noch keine Boot-/Bedienungsabnahme und
kein Nachweis erfolgreicher Paketaktivierung. D1 und T16 bleiben offen.
Der gezielte Absturz-Diagnosepfad wird nicht wieder aufgenommen.

Lokaler Buildindex: `out/phase1-dod/3155115/build-validation.json`, SHA-256
`656c045241fd797d5a8809902951537756726c2e469d4981b6a361b80c6d81eb`.
Er bindet Manifest, Quellbelege, Partitionsprüfung, AVB-/Diskbelege und
Test-App-Build. Images und Profile bleiben auf dem Server; es wurde kein
Build-Release oder Artefaktupload erzeugt.

## Regulärer Erststart, Bedienung und 27 Java-Tests auf 3155115

Boot `c0dd316f-92c0-4f39-bdc0-40aa8d4c9416` erreicht
`sys.boot_completed=1`, authentifiziertes ADB und SELinux Enforcing. Vor der
Benutzeranlage ist nur CE `[0]` entsperrt. SystemServer bleibt PID 1293 mit
Startzeit 36603; im gesicherten Bootlog steht genau ein SystemServer-Start.
Die Bootanimation endet regulär mit Status 0. Bis zur Beobachtung um
12:48:52 UTC finden sich keine FORTIFY-, Fatal-Signal-, Watchdog-, Java-Fatal-
oder ANR-Meldungen. Das ist ein begrenzter Erststartnachweis, kein Abschluss D1.

Die Gastprüfsummen der korrigierten EGL-Bibliothek und des Identitätsdiensts
entsprechen dem neuen Build. Alle elf `RuntimeStartWaiterTest`- und 16
`RuntimeAdmissionTest`-Fälle bestehen; Boot und SystemServer bleiben gleich.
Die synthetische Uhr prüft längere Planung, unverlängerbare Gesamtfrist und
Widerruf der ursprünglichen Freigabe. Diese Komponententests ersetzen keine
echte Anmeldung oder Paketaktivierung.

QMP-TAB/RET öffnet von der bestätigten Einstellungsübersicht die Netzwerkeinstellungen.
Der anschließende Mausklick bei beobachtetem Cursor `(57.165,106.163)` auf
„Navigate up“ führt zurück. Screenshots und XML bestätigen beide Übergänge.
Erste XML-Abfragen während der Animation lieferten keinen Root-Knoten;
die späteren Beobachtungen wiederholten die Eingabe nicht. Ein separater
256-KiB-ADB-Rücktransfer stimmt bytegleich.

Frühe Init-Rückgaben sind separat eingeordnet: ausdrücklich übersprungener
System-Mainline-Initializer bei erfolgreichem aktivem Mainline-Initializer,
`misctrl` mit erfolgreicher boolescher Property-Setzung im Exitcode sowie
Recovery-Refresh passend zu leerem pstore ohne frühere Protokolle. Reguläre
`ctl.stop`-/`ctl.restart`-Vorgänge sind mit den Signal-Exitmeldungen verknüpft.
Die Recovery-Einordnung bleibt eine Quell-/Zustandsinferenz, kein erneut
provozierter Ablauf. Rohmeldungen bleiben unverändert erhalten.

Lokale, gehashte Teilnachweise unter `out/phase1-dod/3155115/`:

- `java-selected/result.json`: `2021091f48b210459e5abc5d8d7e6dbbdcc8fecfd670b6132a1d442f09fbd596`
- `qmp-ui-proof.json`: `930a89652cb6860f115d4038829247603eb28ba1fdb53b051b268a2e4d0f1535`
- `adb-roundtrip.json`: `32d3a3a7fb661b5d0d9840cf73e8ab2f576edc7e1c7152365869b8b15d5c6d64`
- `init-exit-classification.json`: `fbda0db3f2e303cd90b2a3ae3d530bd9da91bb125f965236fc21d4a0f7827e65`

Der neue reguläre CLI-Lauf hat Alpha 10/10 als Administrator und Beta 11/11
als normalen Benutzer angelegt. Alphas erster korrekter Login gelingt ohne
Fehlversuch davor; echte GNU-Ausführung, private Datei und Konfiguration sind
nachgewiesen. Beta bleibt zunächst gesperrt. Die gemeinsame jq-Planung hat
begonnen; noch keine Installation oder T16-Aktivierung wird daraus behauptet.

## Gemeinsame jq-Installation auf 3155115 tatsächlich aktiviert

Die echte CLI veröffentlicht mit frischer Alpha-Adminfreigabe die gemeinsame
Generation `0c064ea4…`: `jq` und `libjq1` jeweils `1.7.1-6+deb13u3`,
`libonig5` `6.9.9-1+b1`. Der ursprüngliche Alpha-Kontext PID 5932,
Startzeit 178981, bleibt zunächst unverändert ohne jq; `linux status` meldet
`activation-pending`. Die ursprüngliche 1024-Byte-Datei und Konfiguration
werden aus der GNU-Shell bytegleich gelesen.

Nach eigenem `linux stop`/`linux start` ist der alte Prozess entfernt. Der
neue Kontext PID 6727, Startzeit 247431, verwendet nachweislich die veröffentlichte
Imagegeneration als Root. Der Status meldet `packages=current`; CE bleibt
`[0,10]`, Beta ist noch gesperrt und SystemServer bleibt 1293.

Ein echter GNU-Prozess bestätigt beide u3-Versionen, führt jq mit Ergebnis 10
aus und prüft die Bibliothek gegen SHA-256
`58a6c82e3cc0b55f2e11e85ffa487bd2381c4cd30068874504c639c76a3e59d6`.
Alphas ursprüngliche Datei behält SHA-256
`9735fb49aa4c255d32cfb4797f849045cce21eb59080865d7a80b2b350dca85b`;
Konfiguration und Test-Secret bleiben ebenfalls erhalten. Alte persönliche
`/tmp`- und `/run`-Proben fehlen erwartungsgemäß nach Kontextneustart.

Beleg: `out/phase1-dod/3155115/shared-u3-activation-proof.json`, SHA-256
`685f6de2f565ac6134603e024e66ae2f9f9f0ac5bfef9f1dbccfb3fc4ac22682`,
mit eingefrorenem Ereignispräfix und gehashten Beobachtungen vor/nach Aktivierung.
Dies ist die erste gemeinsame Installation bei einem Benutzer, **noch kein
T16-Nachweis für ein gemeinsames Update mit vorhandener privater Version**.
Der anschließende private u4-Plan ist in Arbeit; sein Erfolg wird separat geprüft.


## Zwei Benutzer führen getrennte jq-Versionen auf 3155115 aus

Alpha veröffentlicht mit eigener frischer AOSP-Adminfreigabe eine private
Generation `d8d15a97…` mit `jq` und `libjq1` `1.7.1-6+deb13u4`.
Die gemeinsame u3-Generation bleibt unverändert. Der laufende Alpha-Kontext
verwendet bis zum eigenen Kontextneustart weiterhin u3; der Status meldet
`activation-pending`. Danach läuft PID 6947, Startzeit 313901, mit der privaten
CE-Generation als Root. Tatsächliche GNU-Ausführung bestätigt u4, jq-Ergebnis
10 und die Bibliotheksprüfsumme
`92012c8c198ed5f8e44042a124c3271e89a0a2867fc3344642d9ed391ef75f50`.
Alphas ursprüngliche Datei und Konfiguration bleiben bytegleich; alte flüchtige
Dateien fehlen nach dem Kontextneustart.

Betas erster korrekter Login gelingt ohne vorherigen Fehlversuch. Vor Eingabe
des Passworts bleibt Beta gesperrt und ohne Runtime. Nach erfolgreicher Anmeldung
verwendet sein Kontext PID 7848, Startzeit 344923, die gemeinsame Generation
`0c064ea4…`. Beta führt jq tatsächlich in u3 aus, mit passendem `libjq1`,
Bibliotheksprüfsumme und Ergebnis 10. Beide Kontexte existieren gleichzeitig.
Alphas Hintergrundprozess behält beim Benutzerwechsel PID 6976, Startzeit
321693, Host-UID und Namespaces; sein Fortschrittszähler steigt weiter.

Beta erzeugt seine eigene ursprüngliche 1024-Byte-Datei, SHA-256
`2a70df68a2b078f38052836b6beb588db2370f164fff323124aa5913922cb027`,
sowie eigene Konfiguration. Das ist ein Nachweis getrennter normaler Nutzung;
die gegenseitigen Zugriffsprüfungen werden dadurch nicht ersetzt.

Lokale Teilnachweise unter `out/phase1-dod/3155115/`:

- `alpha-private-u4-activation-proof.json`:
  `d3cda13235e37a6d656ab24d34b8e43c0ef803e045127abde9c911eaab6c3179`
- `two-version-execution-proof.json`:
  `c52a0a0cef6aef998b33c556d17149a42b30cfd47b357589f72572019d67f90e`

Der zweite Beleg friert die Ereignisse bis 13:22:06 UTC am 2. Oktober ein und
bindet die Beobachtung beider Root-Generationen. T15 bleibt wegen noch offener
Entfernungs-/Konfliktvarianten offen. Gemeinsames Update mit privatem Abgleich
(T16), gegenseitige Isolation und gepaarter Neustart dieses Profils sind damit
noch nicht nachgewiesen. Der stillgelegte gezielte Absturztest wurde nicht
wieder aufgenommen.


## Bildschirmzyklus und reguläre Abmeldung beider Benutzer auf 3155115

Bei `mWakefulness=Asleep` schreiten beide ursprünglichen GNU-Hintergrundprozesse
mit gleicher PID und Startzeit weiter; persönliches CE bleibt `[0,10,11]`.
Nach dem Aufwecken ist der Terminalkanal widerrufen. Der zunächst ohne frische
Anmeldung gesendete Shell-Aufruf wird korrekt abgewiesen. Dadurch schlagen vier
Treiberassertionen fehl (Shell-Zugang und drei davon abhängige Steuerungen);
sie bleiben im Rohbeleg erhalten und zählen nicht als bestandene Prüfungen.
Nach expliziter erneuter Beta-Anmeldung gelingen die ursprünglichen Datei- und
Konfigurationsreads. Ein aktiver Shell-Kanal während der Sperre wurde in diesem
Zyklus nicht gesondert geprüft.

Betas anschließender CLI-Logout beendet seinen ursprünglichen Prozess
8373/353517 und entfernt den persönlichen Kontext. AOSP bestätigt CE `[0,10]`;
Betas bekannte, zuvor gelesene GNU-Datei liefert keine Bytes. Alphas ursprünglicher
Prozess 6976/321693 schreitet dabei weiter. Nach erneuter Alpha-Anmeldung sind
Alphas ursprüngliche Datei und Konfiguration unverändert lesbar; seine private
jq-u4-Version wird weiterhin ausgeführt.

Auch Alpha wird über die CLI abgemeldet, ohne vorgeschalteten Runtime-Stopp.
Der originale Prozess ist beendet, der Kontext entfernt und CE enthält nur `[0]`.
Beide bekannten Testdateien liefern ohne Anmeldung keine Bytes. Der
Neustart-Checkpoint bestätigt ausschließlich Systembenutzer 0 als gestartet und
eine leere Runtime-Kontextgruppe. SystemServer bleibt bis dahin der ursprüngliche
Prozess 1293/36603, SELinux bleibt Enforcing. Im gesicherten Logcat vor dem
Neustart finden sich keine Fatal-Signal-, FORTIFY-, Watchdog-, Java-Fatal- oder
ANR-Meldungen; dies ist eine begrenzte Protokollbeobachtung.

Beleg: `out/phase1-dod/3155115/screen-logout-before-reboot-proof.json`, SHA-256
`81b4e4ed52b65742c370554c1ec493ec362ca5701ed06d2b3cbab271c0eab8b9`.
Er bindet den vollständigen Ereignisstand, Checkpoint und SystemServer-Beobachtung.
Der Treiber findet in vier lokalen Bootlogs kein vollständiges generiertes
Passwort; der umfassendere T01-Datei-/History-Nachweis bleibt offen.
Neustart und anschließende bytegleiche GNU-Reads werden separat geprüft;
T08/T10 sind wegen weiterer Pflichtvarianten noch nicht vollständig abgenommen.


## Gepaarter Neustart erhält ursprüngliche Daten und getrennte Versionen

Android bestätigt `Power down`, der Helfer `AEGIS_HELPER_SHUTDOWN_CLEAN`.
Beide VM-Prozesse enden regulär. Anschließend startet dasselbe Profilpaar
`1c53b76e-bb1d-4ed6-a0df-7bbc21f2cfee` ohne Neuanlage; das Profilmanifest bleibt
bytegleich. Der neue Boot `21332e3e-c1c0-444a-aed1-b7b78a2e9f78` erreicht den
Bootabschluss mit unverändertem AVB-Digest, SELinux Enforcing, FBE und
aktivierter Metadatenverschlüsselung. ADB authentifiziert die bestehende
Hostidentität nach dem vorgesehenen Wiederverbindungsversuch; kein neuer
Schlüssel wird aufgenommen.

Vor jeder persönlichen Anmeldung ist nur Systembenutzer 0 gestartet und
entsperrt, die Runtime-Kontextgruppe leer. Beide ursprünglichen GNU-Testdateien
liefern keine Bytes. Danach gelingen die ersten korrekten Anmeldungen von Alpha
und Beta jeweils ohne vorgeschalteten Fehlversuch. Alphas Anmeldung entsperrt
zunächst nur CE `[0,10]`; Beta bleibt bis zur eigenen Passwortprüfung gesperrt.

Beide Benutzer lesen aus ihren echten GNU-Shells die ursprünglichen 1024 Bytes
und Konfigurations-/Test-Secret-Dateien bytegleich. Es werden keine Ersatzdateien
erzeugt. Alte persönliche `/tmp`- und `/run`-Proben sind verschwunden. Alpha
führt seine private jq-/libjq1-Version `1.7.1-6+deb13u4` aus, Beta die gemeinsame
`1.7.1-6+deb13u3`. Beide jq-Ausführungen ergeben 10 und bestätigen jeweils die
ursprüngliche Bibliotheksprüfsumme. Neue begrenzte Hintergrundproben werden
explizit von den vor dem Neustart beendeten Prozessidentitäten unterschieden.

Beleg: `out/phase1-dod/3155115/paired-reboot-user-proof.json`, SHA-256
`8528af31846cdc46a304b38dd05305cd215b7f7beed7f101e851bbc795e9ef0e`.
Der Beleg enthält den vollständigen Ereignisstand bis 13:56:04 UTC am
2. Oktober sowie gehashte Shutdown-, Voranmelde-, Kryptographie- und
Protokollnachweise. Die vier früheren Treiberassertionen nach dem Bildschirmzyklus
bleiben sichtbar; nach dem Neustart treten bis zu diesem Beleg keine neuen auf.

Auch die zweite Bootanimation endet regulär mit Status 0. Der eingefrorene
Protokollabschnitt enthält keine Fatal-Signal-, FORTIFY-, Watchdog-, Java-Fatal-
oder ANR-Meldungen. Ein zusätzlicher früher Init-Rückgabecode 1 stammt von der
AOSP-Aufräumaktion für das temporäre VirtualizationService-Verzeichnis.
Quelltext, ursprüngliche Aufrufer-ID und spätere Verzeichnismodi sind gesichert;
der Rückgabecode ist mit fehlendem Schreibrecht des Aufrufers im Elternverzeichnis
vereinbar. Das bleibt eine Quell-/Zustandsinferenz: Die fehlgeschlagene Systemoperation
und ihr stderr wurden nicht erfasst, die Aufräumaktion wird nicht wiederholt.
Es ist damit keine vollständige D1-Abnahme behauptet.

T07/T12 haben hier einen weiteren integrierten Persistenzbeleg. Passwortwechsel,
Benutzerlöschung/ID-Wiederverwendung, gegenseitige Isolation im neuen Image,
nachträgliche Benutzeranlage, T16-Abgleich und die übrigen Matrixvarianten
bleiben vollständig erforderlich. Dieser Ablauf ersetzt ihre Abnahme nicht.
