# Vollständige Phase-1-Abnahme: Arbeitsstand

Beginn: 1. Oktober 2026. Ziel ist die vollständige
[DoD](architecture/phase-1-dod.md), einschließlich aller Varianten T01–T17.
**Status: aktiv, keine vollständige Abnahme.** Aktueller Prüfstand:
Image `209278def7d5bc5612eeb397bdd8ee20ccb16d86`, Profil
`2366ca04-d587-4170-8c56-a63c8a8e1774`, Boot-ID
`44dbff5f-3a76-4e97-9334-beb437fcd461`.
Vollbuild und Imageprüfung, Bedienung per Tastatur/Maus, authentifizierter
binärer ADB-Rundlauf und die begrenzte Boot-/Kryptographiekontrolle sind
belegt. Auf diesem Image bestehen 38 Java-Pakettests, sieben native Plantests,
sechs Auswahltests, fünf Ausführungstests und beide Veröffentlichungstests.
Der erste persönliche Administrator Alpha ist über die CLI angelegt. Sein
erster Login, GNU-Grundprüfung, persönliche Testdateien und Shell-Ende bei
weiterlaufender Sitzung und demselben Hintergrundprozess sind belegt.
Gemeinsames jq/libjq1 `u3` und anschließend Alphas privates `u4` sind
veröffentlicht, regulär aktiviert und tatsächlich ausgeführt.
Beta ist als normaler Benutzer angelegt; auch sein erster Login und seine
GNU-Grundprüfung bestehen. Er führt gemeinsames u3 aus, während Alpha privates
u4 behält. Beide ursprünglichen persönlichen Dateien und fortlaufenden
Hintergrundprozesse sind erfasst. Der vollständige Isolations-, Paketfehler-,
Logout- und Neustartablauf bleibt offen. Die einzelnen Belege und ihre Grenzen
stehen weiter unten.

## Frühere integrierte Prüfstände

Image `20d7d6d33fb3243cd87d0eb90fa2fd09bd2cc178`, Profil
`06adfa48-f55f-46b7-a962-82bef7af5e83`. Normaler Boot und vier gezielte native
Pakettests sind bestanden. Im neuen persönlichen CLI-Ablauf sind beide Benutzer
angelegt; beide ersten Zugänge und GNU-Grundprüfungen bestehen. Beide führen
unterschiedliche private jq-/libjq-Versionen aus. Das gemeinsame Update ist
veröffentlicht; die bisherigen persönlichen Kontexte bleiben bis zum eigenen
Neustart konsistent und laufen weiter. Betas anschließende Aktivierung meldet
jedoch `packages=current`, obwohl gemeinsame PCRE2-/OpenSSL-Updates fehlen.
Dieser reproduzierte T16-Fehler und der vollständige Paket-/Persistenzablauf
sind noch offen.
Die folgenden früheren integrierten Belege gehören zu Image
`31551159cd11d66b0fa18442b5f62106edf4bfb5`, Profil
`1c53b76e-bb1d-4ed6-a0df-7bbc21f2cfee`: normaler Boot, Bedienung,
ADB und 27 gezielte Java-Tests. Der dortige gepaarte Wiederholungsstart
mit bytegleichen persönlichen Daten und getrennten jq-Versionen ist ebenfalls
belegt. Das anschließende gemeinsame Update scheitert beim nativen Paketstart
an der Steuerkanalfrist; die ausgewählten Paketgenerationen bleiben unverändert,
beide laufenden Runtime-Kontexte werden jedoch beendet. Der Fehler bleibt als
älterer Befund erhalten; der neue Stand veröffentlicht diesen Updatefall ohne
Abbau beider laufender Kontexte. Die vollständige Testmatrix bleibt offen.
Der frühere Bootanimation-Absturz auf `d149766` bleibt als Regressionsevidenz erhalten.

Ausgangspunkt ist das geprüfte Image `c52657113becde42d735669d4d64405c7740cc74`.
Die [fünf bisherigen Meilensteine](server-acceptance.md) bleiben gültige
Teilbelege. Die neue Prüfung verwendet ein frisches Profil unter
`out/phase1-dod/c526571/`; bestehende Profile und Logs bleiben erhalten.
Ein nötiger Produktfix erhält einen neuen Build und betroffene Nachweise
werden auf diesem Stand erneut erhoben.

## Anforderung und noch benötigter Nachweis

Diese Tabelle ist eine Arbeitsliste, kein PASS-Manifest. Die integrierten
Teilbelege stammen überwiegend aus den oben genannten früheren Prüfständen;
sie ersetzen den noch offenen Ablauf auf `209278d` nicht. „Teilbeleg“ bedeutet
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
| T16 | Auf 20d7d6d gemeinsames Update bei zwei privaten Versionen veröffentlicht; beide ursprünglichen Hintergrundprozesse bleiben erhalten | Fehler: Betas Aktivierung übernimmt gemeinsame automatische Bibliotheksupdates nicht, meldet aber aktuell; Korrektur und erneute Aktivierung beider Kontexte nachweisen |
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


## Beta hält u3 privat fest; eigener Stopp erhält Anmeldung und Alpha

Beta fordert über seine echte CLI-Sitzung `jq=1.7.1-6+deb13u3` für `user` an.
Der Plan zeigt ausdrücklich die unveränderte Version als private Auswahl.
Alpha bestätigt mit frischer AOSP-Adminprüfung; die veröffentlichte Generation
`2dba0c4f…` gehört dennoch Benutzer/Seriennummer 11/11. Gemeinsame Generation,
Alphas private Auswahl und beide bisher laufenden Root-Mounts bleiben gleich.
Betas Status meldet zunächst ausstehende Aktivierung; seine reale jq-Ausführung
und ursprünglichen privaten Daten bleiben nutzbar.

Ein eigener `linux stop` entfernt Betas ursprünglichen Prozess 4418/89924 und
seinen Kontext, erhält aber die tatsächliche AOSP-Anmeldung und CE `[0,10,11]`.
Alphas ursprünglicher Prozess 3639/76829 schreitet vorher und nachher weiter.
Der folgende Start aktiviert Betas private Generation. Aus GNU bestätigt Beta
jq/libjq1 u3, die bekannte Bibliotheksprüfsumme und Ergebnis 10; Datei und
Konfiguration sind bytegleich, alte flüchtige Proben fehlen. Die neue begrenzte
Hintergrundprobe hat eine separat beobachtete Identität 8054/180190.

Beleg: `out/phase1-dod/3155115/beta-private-u3-activation-proof.json`, SHA-256
`06a9f93e1940585d1b41ca59dca5d6ffbf491341a85980ec64499770aadc5cd6`.
Er bindet die tatsächlichen CLI-/GNU-Ereignisse und die Paket-/Kontextbeobachtungen
vor Veröffentlichung, vor Aktivierung und danach. Das deckt die Beta-Richtung
des eigenen Runtime-Stopps und einen Eigentumsfall mit fremder Adminfreigabe ab;
es ersetzt weder die gesamte T09- noch die T13-Matrix.

Damit sind vor dem gemeinsamen Update beide privaten Versionen vorhanden:
Alpha u4, Beta u3, gemeinsame Generation u3. Die gemeinsame Updateplanung läuft;
T16 ist erst nach nachgewiesener konsistenter Aktivierung zu bewerten.


## Gemeinsames Update scheitert an der Paketstartfrist

Der reguläre Updateplan vom 2. Oktober, 14:18:30 UTC, enthält jq/libjq1 von
u3 auf u4 sowie Aktualisierungen von libpcre2-8-0, libssl3t64 und
openssl-provider-legacy. Nach Alphas echter Adminfreigabe meldet die CLI um
14:19:45 UTC einen unbestätigten Abbruch. Der SystemServer protokolliert
`Control channel failed: operation=13 cause=IllegalStateException deadlineExpired=true`.
Operation 13 ist der native Paketstart. Das ist ein fehlgeschlagener normaler
Funktionslauf; es wurde kein Absturz absichtlich ausgelöst.

Die Beobachtung um 14:19:55 UTC bestätigt unveränderte gemeinsame und private
Paketgenerationen. Beide zuvor beobachteten Hintergrundprozesse sind beendet.
Der ursprüngliche SystemServer 1140/21254 besteht weiter; CE enthält weiterhin
`[0,10,11]`. Das ist weder eine erfolgreiche Aktualisierung noch ein bestätigter
Logout. Unveränderte Generationsverweise allein belegen auch keine vollständige
Byteprüfung sämtlicher persönlicher Dateien nach diesem Fehler.

Der eingefrorene Beleg enthält alle 243 bisherigen Treiberereignisse und bindet
den lokalen Logcat-Schnappschuss sowie die Beobachtungen vor und nach dem Fehler:
`out/phase1-dod/3155115/common-update-start-failure-proof.json`, SHA-256
`ecaa987180dd4bac6f70c9b9433284189bb2ccd089b09ca26c9463cc6e4c9cd7`.
T16 bleibt fehlgeschlagen/offen; das Verhalten ist zusätzlich für T17 relevant.

Die Quellprüfung zeigt, dass `PackageExecutorStart` synchron auf die
READY-Antwort wartet. Davor richtet der Worker seinen Kontext ein und prüft
rekursiv die Dateikennzeichnungen des Kandidaten. Welcher Abschnitt die Frist
überschritten hat, ist noch nicht gemessen. Die nächste Korrektur muss diese
Prüfungen, die Adminbindung und die begrenzte Zulassungsfrist erhalten; ein
erneuter identischer Fehlerlauf ist kein Ersatz für die Ursachenanalyse.

Die gemeldete Plattform-Sicherheitswarnung hat keinen nachgewiesenen Auslöser
in diesen AEGIS-Protokollen. Sie wird nicht mit dem Paketfehler gleichgesetzt.
Die ausgeschlossene gezielte Absturzdiagnostik bleibt eingestellt; vorhandene
Ergebnisse werden dadurch weder gelöscht noch als bestanden umgedeutet.


## Paketstartkorrektur gebaut; erster Komponentenlauf nicht bestanden

Produktcommit `20d7d6d33fb3243cd87d0eb90fa2fd09bd2cc178` verschiebt die
vollständige Kennzeichnungsprüfung in die überwachte Ausführungsphase, weiterhin
vor Guard und jedem Debian-Prozess. Einrichtung, Mount-Inventur, Rechtebegrenzung
und die kurze Startfrist bleiben erhalten. READY bestätigt nur den eingerichteten
Worker; ein späterer Prüffehler verhindert die Veröffentlichung. Dies behebt
einen größenabhängigen Arbeitsschritt innerhalb der kurzen Steuerkanalfrist;
welcher Abschnitt den früheren konkreten Timeout auslöste, bleibt ungemessen.

Der vollständige lokale Build
`/srv/aegis/runs/local-20261002T144125Z-20d7d6d3-RhemVS` endet mit
`LOCAL_BUILD_VERIFIED`, Exitcode 0 und 20 bestätigten Image-Prüfsummen. AVB und
die neue vollständige GPT-Basisdisk sind geprüft. Lokale Belege unter
`out/phase1-dod/20d7d6d/`:

- `build-validation.json`:
  `02d8e8f5a83e7b5fdd470f8991d19441501022cf36fc1701774641aad0aecba4`
- `avb-checked.json`:
  `b89d52541c4508fb79541cc1bebfb6d06756fcd1a8ac038b5ea0dbf6fc57d5b9`
- `android.raw.json`:
  `0fd2b32f00905312b4bbad5790ccb84ed3df6d63bad55c67eecf7e43e8f733d4`

Der erste gezielte Komponentenlauf im alten Gast ist **nicht bestanden**:
Die Root-Kennzeichnungsprüfung besteht; die verschachtelte Prüfung erreicht
wegen eines noch schreibend geöffneten Fixture-Deskriptors die Mount-Übergabe
nicht (`EBUSY`). Der normale Installations-/Update-/Entfernungsfall überschreitet
die neunsekündige Abschlusswartezeit des Tests. Der letzte Fall zur privaten
Versionsauswahl besitzt keinen Abschlussbericht. Später sind QEMU, Testprozess
und der ausschließlich im Speicher gehaltene Passworttreiber nicht mehr
vorhanden. Ursache und geordneter Shutdown sind nicht belegt; die bisherigen
Profile und Rohprotokolle bleiben unverändert erhalten.

`native-selected/interrupted-result.json`, SHA-256
`13b70ffdfcddfbed3796bd55a4f7bbdb6b9f76abb96532da9a7ebf7c0e5eeadd`,
bindet diesen unvollständigen Fehlerstand. Testcommit `3012569` schließt den
Fixture-Schreibdeskriptor vor der Übergabe und fragt denselben asynchronen
Paketauftrag bis zu einer begrenzten Gesamtdauer ab. Die separate Startfrist
bleibt unverändert. Alle produktiven Helfer des erneuten Komponentenbuilds
sind bytegleich mit `20d7d6d`; nur Tests und Dokumentation wurden geändert.
Beleg `native-corrected-build-receipt.json`, SHA-256
`e9d363b9a1ddad7dec69dc878127da84cb3607b1eb2327d30ff55e584e6bb9b3`.

Das neue Profilpaar `06adfa48-f55f-46b7-a962-82bef7af5e83` startet regulär mit
Image `20d7d6d`. Die korrigierten Tests und der echte CLI-Updateablauf sind an
diesem Prüfpunkt noch nicht bestanden. T16 und die vollständige DoD bleiben offen.


## Vier korrigierte Pakettests und normaler Boot auf 20d7d6d bestanden

Alle vier ausgewählten `RuntimePackageExecutor`-Fälle bestehen im neuen
Vollimage mit Teststand `30125695024ab851a146eb756adf48a1fa839a57`:
ungültige Root-Kennzeichnung, ungültige Kennzeichnung einer verschachtelten
Datei, geprüfte Installation/Aktualisierung/Entfernung mit erhaltenen
Konfigurationsbytes und Abhängigkeitsmarkierungen sowie eine private Auswahl
derselben Version ohne erneute Installation, auch nach erneutem Mounten.
Die beiden Kennzeichnungsfälle bestätigen die Ablehnung nach erfolgreicher
Einrichtung, aber vor Debian-Programmen und Paketskripten. Der zuvor beobachtete
`EBUSY`-Fixturefehler tritt nach Schließen des Schreibdeskriptors nicht mehr auf.

Die vier Fälle benötigen zusammen rund 337 Sekunden einschließlich
Hostbeobachtung; Exitcode 0, keine übersprungenen Fälle. Vorher und nachher
gelten Boot-ID `34a66c93-f598-471b-9ef0-f4db218dbbe6`, SystemServer-PID 1349,
SELinux Enforcing und CE `[0]`. Beleg:
`out/phase1-dod/20d7d6d/native-selected-corrected/result.json`, SHA-256
`1bc20cc773f6c379845115a761528a4344297f6778ed2ca941e27c7ce9c52d9f`.
Der frühere fehlgeschlagene/unvollständige Lauf bleibt separat erhalten.

Der normale Erstboot bestätigt authentifiziertes ADB, FBE, `managed-v1` und den
erwarteten AVB-Digest. Die Bootanimation endet mit Status 0. Ein tatsächlicher
QMP-Bildschirm zeigt die normale Android-Sperransicht in 720 × 1280. Der
eingefrorene Boot-/Testabschnitt enthält keine Fatal-Signal-, FORTIFY-,
Watchdog-Kill-, Java-Fatal- oder ANR-Meldungen. Andere Init-Rückgabecodes und
beendete Dienste sind im Beleg ausdrücklich erhalten; dies ist keine vollständige
D1-Abnahme. Beleg `boot-observation/result.json` unter demselben Verzeichnis,
SHA-256 `e052a80ce81dcf33fa7212ded6a2a7ef59a5090c81dab2efd06fdc1b2025dbfe`.

Die Pakettests benutzen eigene inaktive ext4-Kopien und synthetische Pakete.
Sie ersetzen weder AOSP-Adminfreigabe noch den tatsächlichen gemeinsamen
CLI-Updateablauf mit zwei privaten Versionen. T16 und der vollständige
Referenzablauf müssen auf diesem Stand noch ausgeführt werden; T15 enthält
weiterhin die offene private Entfernung mit Rückkehr zur gemeinsamen Variante.


## Persönlicher CLI-Ablauf auf 20d7d6d begonnen

Die echte AEGIS-CLI legt Alpha 10/10 als ersten AOSP-Administrator an. CE bleibt
danach `[0]`. Auch die anschließende Vorbereitung des ersten Logins wechselt
nur das Vordergrundziel; Alphas CE ist vor Passworteingabe gesperrt und seine
Runtime fehlt. Der erste korrekte Login gelingt ohne Aufwärmversuch. Eine
verzögerte Statusprüfung und tatsächlich ausgeführte GNU-Befehle bestätigen die
fortbestehende Sitzung, interne UID/GID 1000 und HOME `/home/user`.

Zehn persönliche Grundverzeichnisse besitzen jeweils 1000:1000 und Modus 0700.
Bash, apt, dpkg und GNU-Werkzeuge laufen mit glibc 2.41; die Basis ist
schreibgeschützt, der Programmprozess trägt keine Capabilities und meldet
NoNewPrivs sowie Seccomp. Das ist eine Einzelbenutzerprüfung, noch kein
gegenseitiger Isolationsnachweis. Alpha erzeugt seine ursprüngliche 1024-Byte-Datei,
SHA-256 `ab868c1520a320bf6b6e206731fbee6d71dbbd4e4318160a8f4bc38fac5b397d`,
sowie getrennte Konfigurations- und flüchtige Proben aus seiner GNU-Shell.
Der begrenzte Hintergrundprozess wird als PID 6491/Startzeit 206114 und
Host-UID 1007500 mit fortschreitendem Zähler beobachtet.

Nach regulärem Shell-Ende legt die CLI mit frischer Alpha-Adminprüfung Beta
11/11 als normalen Benutzer an; dessen CE bleibt gesperrt. Beleg:
`out/phase1-dod/20d7d6d/alpha-first-gnu-and-two-users-proof.json`, SHA-256
`f391c851879b21ddcceb6b236bc94513f83585437d57a0eb093814d8eb91538c`.
Die 30 eingefrorenen Ereignisse reichen bis zur Beta-Anlage. Die erste gemeinsame
jq-u3-Planung läuft danach; eine Paketveröffentlichung ist an diesem Prüfpunkt
noch nicht belegt. Der Live-Treiber hält die neuen Passwörter ausschließlich
im Speicher. Vorherige Profile und deren ursprüngliche Nachweise werden nicht
zurückgesetzt oder ersetzt.

## Gemeinsame jq-Installation und Aktivierung auf 20d7d6d nachgewiesen

Die echte CLI veröffentlicht nach frischer Alpha-AOSP-Adminfreigabe jq und
libjq1 `1.7.1-6+deb13u3` sowie libonig5 `6.9.9-1+b1` im gemeinsamen Bereich.
Alphas bisheriger Kontext meldet ausstehende Aktivierung und sein ursprünglicher
Hintergrundprozess läuft bis zum ausdrücklich angeforderten Runtime-Stopp weiter.
Nach `linux stop` und `linux start` meldet der Kontext `packages=current`.
Sein tatsächlich eingebundenes, schreibgeschütztes Root-Image gehört zur
veröffentlichten Generation
`f03dffa8923f4147875d8e6d1b300071c05fda2de1ae9ddf9629ff82d66871ba`.

Aus Alphas authentifizierter GNU-Shell bestätigen `dpkg-query` die drei
Paketversionen und jq die Summe 10 aus `[1,2,3,4]`. Die ausgeführte libjq-Datei
besitzt SHA-256
`58a6c82e3cc0b55f2e11e85ffa487bd2381c4cd30068874504c639c76a3e59d6`.
Die ursprüngliche persönliche 1024-Byte-Datei und beide Konfigurationsproben
bleiben bytegleich; die ursprünglichen flüchtigen Proben unter `/tmp` und
`/run/user/1000` sind verschwunden. Keine Probe wurde dafür neu geschrieben.

Boot-ID und SystemServer-PID/Startzeit bleiben unverändert; CE ist `[0,10]`,
Beta war noch nicht angemeldet. Der lokale Beleg
`out/phase1-dod/20d7d6d/shared-u3-activation-proof.json`, SHA-256
`cd9004d3c453a81c8ee5dbd124ac6c02498ec41d8fee6a2383f35c318fffa2de`,
enthält 48 eingefrorene CLI-Ereignisse und die beobachtete Root-Image-Zuordnung.
Dies belegt die erste gemeinsame Installation und Aktivierung mit erhaltenen
persönlichen Dateien, noch kein gemeinsames Update bei bestehenden privaten
Versionen und keinen VM-Neustart. T15, T16 und die Gesamtfreigabe bleiben offen.

## Unterschiedliche jq-Versionen und Betas erster Login auf 20d7d6d

Alpha installiert mit frischer AOSP-Adminfreigabe jq und die dazugehörige
libjq1 `1.7.1-6+deb13u4` im persönlichen Bereich. Die gemeinsame Generation
bleibt unverändert auf u3. Nach ausgewiesener ausstehender Aktivierung und
regulärem Kontextneustart führt Alpha u4 tatsächlich aus. `dpkg-query`
bestätigt beide u4-Pakete, jq berechnet die erwartete Summe; die libjq-Datei
besitzt SHA-256
`92012c8c198ed5f8e44042a124c3271e89a0a2867fc3344642d9ed391ef75f50`.
Alphas ursprüngliche persönliche Datei bleibt bytegleich. Sein Root-Image
liegt in seinem CE-Speicher und gehört zur privaten Generation
`5ac6d6af78838946cd1af65840e001da2a26cdbdc8d6eb392aded2bcac9f7b44`.

Beta 11/11 meldet sich erstmals erfolgreich an, ohne vorangegangenen
Fehlversuch. Vor Passworteingabe ist seine CE weiterhin gesperrt und seine
Runtime fehlt. Verzögerte Statusprüfung und echte GNU-Ausführung bestätigen
die gültige Sitzung. Beta führt die gemeinsame jq-/libjq1-Version u3 mit
deren bereits dokumentierter Bibliotheksprüfsumme aus; sein Root-Image ist
weiterhin die gemeinsame Generation. Beide Varianten verwenden libonig5
`6.9.9-1+b1`. Die UID/GID-Abbildungen unterscheiden sich: interne UID/GID
1000 wird bei Alpha auf 1007500 und bei Beta auf 1107500 abgebildet.

Alphas nach dem eigenen Runtime-Neustart neu angelegte Hintergrundprobe
behält beim Wechsel zu Beta PID 8372 und Startzeit 405185; der Zähler steigt
auch bei Vordergrundbenutzer 11 weiter. Dieser Nachweis ersetzt nicht die
frühere, ausdrücklich gestoppte Probe. Beta erzeugt seine eigene ursprüngliche
1024-Byte-Datei, SHA-256
`25fc4776250a104217a27a61bdc69561f47079a964544c3e670803088c2d97cf`,
und getrennte persönliche und flüchtige Konfigurationsproben, SHA-256
`154a5e94f4e1179e5312a3b8cda87d29f8de5f92f9407a339fa0de2f36cd94d2`.
Seine normalen GNU-Grundprüfungen bestehen ebenfalls.

Der lokale Beleg
`out/phase1-dod/20d7d6d/distinct-jq-versions-and-beta-first-login-proof.json`,
SHA-256 `e615244284c5a78486cdb7867386dd283147bbbd392708531bc69e1e151d8c97`,
bindet 88 Ereignisse, beide Root-Images und die unveränderte SystemServer-
Identität an dasselbe Image und Profil. CE ist am Ende `[0,10,11]`.
Damit ist die tatsächliche Ausführung unterschiedlicher Versionen desselben
Pakets belegt. Gegenseitige Isolation, VM-Neustart, private Entfernung und
gemeinsames Update bei privaten Versionen sind für diesen Stand weiterhin
gesondert nachzuweisen; T15 und T16 sind nicht vollständig bestanden.

## Beide privaten Versionen als Ausgangsstand für das gemeinsame Update

Beta beantragt über seine eigene CLI, jq `1.7.1-6+deb13u3` privat festzuhalten.
Der Plan weist die unveränderte Version ausdrücklich aus. Alpha erteilt eine
frische AOSP-Adminfreigabe; veröffentlicht wird eine persönliche Generation
für Beta 11/11, nicht für den freigebenden Admin. Die gemeinsame Generation
und Alphas private Generation bleiben unverändert. Betas bisheriger
Hintergrundprozess überlebt die Veröffentlichung bis zum ausdrücklichen
Kontextstopp mit derselben PID/Startzeit und fortschreitendem Zähler.

Nach eigenem Kontextneustart führt Beta u3 mit passender libjq-Datei aus.
Seine ursprüngliche Datei und Konfiguration bleiben bytegleich; die alten
flüchtigen Proben sind verschwunden. Seine neue private Generation lautet
`1b37c5522a6183cbc5f419dcb006ee8164efa6899ea0d63c47b02aaffebbcb7b`.
Die tatsächlich eingebundenen persönlichen Images enthalten jetzt die
expliziten jq-Auswahlen u4 für Alpha und u3 für Beta. Beide verweisen noch auf
die gemeinsame Generation `f03dffa8923f4147875d8e6d1b300071c05fda2de1ae9ddf9629ff82d66871ba`.

Vorher-Beleg `out/phase1-dod/20d7d6d/shared-update-before.json`, SHA-256
`e0f7d6c696e988e0a9ccbe0b15ff4057a2f86de817d3a4fdbbdf6b24d663ee61`,
bindet 112 CLI-Ereignisse, private Absichten, ausgewählte und eingebundene
Images sowie Hintergrundprozesse Alpha 8372/405185 und Beta 12994/555841 an
denselben Boot. Der gemeinsame Updateplan läuft an diesem Prüfpunkt erst;
eine Veröffentlichung oder erfolgreiche anschließende Aktivierung wird noch
nicht behauptet.

## Gemeinsames Update bei zwei privaten Versionen veröffentlicht

Der tatsächliche gemeinsame Updateplan erhöht jq/libjq1 auf u4,
libpcre2-8-0 auf `10.46-1~deb13u3` sowie libssl3t64 und
openssl-provider-legacy auf `3.5.7-1~deb13u3`. Beta beantragt den Auftrag;
Alpha erteilt die frische AOSP-Adminfreigabe. Der Auftrag wird erfolgreich
als gemeinsame Generation
`cd3f9fa71e0080b121909f81be983d5e801563c909ed48d983eb5afe3c28bdc2`
veröffentlicht. Anders als im alten 3155115-Lauf gibt es in diesem Ablauf
keinen Steuerkanalabbruch beim Paketstart.

Beide ursprünglichen privaten Selektoren und eingebundenen Images bleiben
zunächst unverändert. Die Hintergrundprozesse Alpha 8372/405185 und Beta
12994/555841 behalten ihre Identität und fortschreitende Zähler. Betas CLI
zeigt ausstehende Aktivierung; seine tatsächlich ausgeführte GNU-Shell besitzt
weiterhin jq/libjq1 u3 sowie die alten PCRE2-/OpenSSL-Bibliotheken u2.
Ein frisch veröffentlichter gemeinsamer Stand verändert also den bereits
laufenden Kontext in diesem Fall nicht teilweise.

Beleg `out/phase1-dod/20d7d6d/shared-update-published-before-activation.json`,
SHA-256 `756d6fdef7ff1726105028c4f5ef7024c1c00ae0b78551d24104a10ff02ff08d`,
bindet 123 Ereignisse, Selektoren und Prozessidentitäten an das unveränderte
Image, Profil, den Boot und SystemServer. Erst anschließend wird Beta bewusst
gestoppt und dessen normaler Start mit automatischem Paketabgleich angefordert.
Dieser Beleg enthält noch keinen abgeschlossenen Aktivierungsnachweis.

## T16-Fehler: Aktivierung lässt gemeinsame Bibliotheksupdates zurück

Betas normaler Start nach dem gemeinsamen Update endet mit `runtime=ready`
und `packages=current`. Seine veröffentlichte persönliche Generation
`8415295537365c0e1069b7a32d6b6365ff41acd05522a7d03ebf1c553370cc6a`
verweist bereits auf den neuen gemeinsamen Stand `cd3f9fa7…`. Die tatsächlich
ausgeführte GNU-Prüfung findet jedoch libpcre2-8-0 weiterhin in
`10.46-1~deb13u2` und libssl3t64/openssl-provider-legacy weiterhin in
`3.5.7-1~deb13u2`. Gemeinsam wurden jeweils u3-Versionen veröffentlicht.
Betas private jq-/libjq1-Version u3 bleibt korrekt erhalten und ausführbar.

Die zusammenhängende Prüfung erwartete neben dieser privaten Version die
gemeinsamen Bibliotheksupdates und scheitert. Ein anschließender reiner
Versions-/Ausführungsnachweis bestätigt die tatsächlichen alten Bibliotheken.
Ein gesonderter Diagnoseversuch aus GNU konnte die geschützte interne
Auswahlmanifestdatei nicht lesen; diese erwartete Zugriffsverweigerung ist
vom Paketversionsfehler zu unterscheiden. Der unabhängige Beobachter bestätigt
die unveränderte private Absicht und die neue gemeinsame Basisbindung.

Betas ursprüngliche Datei und Konfiguration bleiben bytegleich; alte flüchtige
Proben fehlen. Alphas ursprünglicher Kontext bleibt aktiv, seine Aktivierung
wurde noch nicht angefordert. Boot und SystemServer-Identität sind unverändert.
Fehlerbeleg `out/phase1-dod/20d7d6d/shared-update-beta-activation-failure.json`,
SHA-256 `c9d84e6bd7f2a5e87925c5c531d9b7dd43944190761c72847a9cf103f2770ebf`,
enthält 138 Ereignisse und die tatsächlich aktivierte Paketgeneration.

Die Quellcodeprüfung zeigt eine Lücke im Abgleich: Die Ziele umfassen gemeinsame
manuelle Hauptpakete plus explizite private Auswahlen; der Resolver führt dafür
`install` aus. Unveränderte Hauptpakete verlangen dadurch keinen allgemeinen
Upgrade-Schritt für bereits ausreichende automatische Abhängigkeiten.
Das entspricht der Unterscheidung von `install` und `upgrade` in der
[Debian-APT-Dokumentation](https://manpages.debian.org/trixie/apt/apt-get.8.en.html).
Eine Korrektur muss gemeinsame Bibliotheksänderungen berücksichtigen und
zugleich passende ältere Abhängigkeiten expliziter privater Versionen erhalten.
Ein bloßes Erzwingen sämtlicher gemeinsamer Bibliotheksversionen würde den
privaten jq-u3/libjq-u3-Fall verletzen. T16 bleibt fehlgeschlagen; neue
Regressionstests, korrigierter Build und erneuter Systemnachweis stehen aus.

## T16: Abhängigkeitsregression eingegrenzt, Solverkorrektur noch in Prüfung

Die Testergänzung `bb3c2f3` reproduziert den fehlenden Bibliothekswechsel mit
unverändertem gemeinsamen Hauptpaket und einer privaten Auswahl. Der Kontrollfall
mit einer neueren, aber noch nicht gemeinsam veröffentlichten Repository-Version
bleibt unverändert. Lokaler Baseline-Beleg:
`out/phase1-dod/20d7d6d/native-dependency-baseline/result.json`, SHA-256
`2a1f70bf825dfc24eee78ec4cc345d1c50a56141ef7767297f107e8f2b70f760`.

Der erste Korrekturstand `0d28883` verwendet einen Upgrade-Schritt mit begrenzten
Versionen. Sein abgeschlossener nativer Lauf besteht 25 Tests und verfehlt zwei:
Eine ältere private Programmversion samt Bibliothek wird beim gemeinsamen Update
oder Entfernen nicht beibehalten. Der native Ergebnisvergleich weist die falschen
Pläne zurück. Log:
`out/phase1-dod/20d7d6d/native-dependency-fixed-tests/tests.log`, SHA-256
`50e8a78d65573cd7fce615719d98e8ad89826c3e2e7a1d757f45222ba2bcf543`.

`173a547` korrigiert die allgemeine APT-Präferenzsyntax, bevorzugt exakte
Hauptpakete und ergänzt den umgekehrten privaten Versionsfall bei gemeinsamem
Downgrade. Die abgeschlossene Prüfung besteht 26 Tests und verfehlt zwei,
für ältere und neuere private Hauptpakete: Höhere Präferenzen allein erzwingen
sie bei einem nichtstrikten Solver nicht. Dieser Stand ist ebenfalls nicht
abgenommen. Beleg `out/phase1-dod/20d7d6d/native-exact-roots-tests/result.json`,
SHA-256 `fd4cc2af742dc2f201aa216136e75a7a8e93943cb2ef425bcfe1115d360c7440`.
Boot-ID, SystemServer, SELinux Enforcing und CE-Zustand sind vorher/nachher gleich.

Der [APT-3.0.3-Quellcode](https://sources.debian.org/src/apt/3.0.3/apt-pkg/solver3.cc/)
zeigt, dass striktes Pinning neben Kandidaten auch bereits installierte Versionen
zulässt. `1ebaa7a` verwendet deshalb im internen Abgleich striktes Pinning mit
den abgeleiteten Präferenzen. Explizite Installationen behalten ihren bisherigen
Solverpfad für weitere passende Abhängigkeitsversionen. Die unabhängige Prüfung
der exakten Ziele und der veröffentlichten Versionen bleibt erhalten.

Der native Build aus `1ebaa7a` ist erfolgreich; Herkunftsbeleg
`out/phase1-dod/20d7d6d/native-strict-reconciliation-build-receipt.json`, SHA-256
`d92e5de6634815cff2773f04c9c885e3c484f930acbfddee6c52c35760580da7`.
Die beiden zuvor fehlgeschlagenen privaten Versionsfälle bestehen mit diesem
Stand, ebenso sämtliche 19 Tests zur Zielableitung und Planbindung: 21 von 21
Tests erfolgreich. Beleg
`out/phase1-dod/20d7d6d/native-strict-reconciliation-tests/result.json`, SHA-256
`e4041ee4dd961ae370189397a982c28983ff7c1d6adc5570c4ac650c14dcfe2b`.
Auch die sieben übrigen APT-Szenarien bestehen. Beleg
`out/phase1-dod/20d7d6d/native-strict-reconciliation-remaining-tests/result.json`,
SHA-256 `a5c263f172e7db0ab584f8092123f6e961d52146dad855f55858076c2394852e`.
Dabei bleiben Boot und SystemServer einschließlich Prozessstartzeit unverändert.
Alle bisherigen Vergleiche nutzen
isolierte Testdaten im weiterhin älteren Image `20d7d6d`; sie beweisen weder
einen neuen Vollimage-Build noch die korrigierte reale CLI-Aktivierung. T16 und
die vollständige Phase-1-Abnahme bleiben offen.

## Veröffentlichungstests: Beobachtungsfrist und belegter Speicherbedarf

Die zwei signierten Veröffentlichungstests auf `1ebaa7a` sind fehlgeschlagen.
Der erste beendet die Auswahlbeobachtung nach 200 Abfragen mit je 50 ms Pause
weiterhin mit `EAGAIN`, bevor die Planung beginnt. Der zweite scheitert beim
Anlegen seines synthetischen gemeinsamen Testbestands. Sein ursprünglicher
Fehlercode wurde nicht ausgegeben; die anschließende Speicherprüfung findet
nur noch 39 MB freien Platz auf der 7,9-GB-Gastpartition.

Beleg `out/phase1-dod/20d7d6d/native-strict-reconciliation-publication-tests/result.json`,
SHA-256 `dbfdbb0e79168325ad4636de1e170c9e019917718a3867b261860c97a63db16a`.
Keiner der beiden Fälle zählt als bestanden. Die Testbeobachtung wird auf
wiederholtes Abfragen desselben Auftrags innerhalb einer monoton gemessenen
180-Sekunden-Frist umgestellt; Produktfristen und Erfolgsbedingungen bleiben
unverändert. Die Fixture-Veröffentlichung erhält eine Fehlercodeausgabe.

21 noch vorhandene synthetische Fixture-Verzeichnisse lassen sich den lokalen
Fehlertestprotokollen zuordnen. Ihre vollständigen Dateien werden lokal archiviert
und gegen SHA-256-Werte aus dem Gast geprüft, bevor ausschließlich diese
gesicherten Testverzeichnisse entfernt werden. Benutzerprofile, deren Daten und
Paketbestände sind nicht Teil dieser Bereinigung. Weitere Image-Tests warten
auf den Abschluss der Sicherung und wieder ausreichend freien Gastplatz.

## T15: Native Entfernungsvorbereitung implementiert

`edfa33c` ergänzt eine getrennte Ableitung der Entfernungsziele und einen
getrennten Modus zur Auswahl eines aktuellen privaten/gemeinsamen Imagepaars.
Die ursprüngliche private Absicht wird vor dem Aufheben genau einer Auswahl
geprüft. Gemeinsame Hauptpakete und alle anderen privaten Auswahlen bleiben
Ziele; weiterhin benötigte Abhängigkeiten können automatisch erhalten bleiben.
Eine veraltete Basis ist kein impliziter Auftrag zum gemeinsamen Update.

Der native Build ist erfolgreich. Sieben neue Tests zur Zielableitung und
19 bestehende Reconciliation-Tests bestehen: 26 von 26. Beleg
`out/phase1-dod/20d7d6d/native-private-removal-goals-tests/result.json`, SHA-256
`5922f9afd615a8d7a8b1e2c1f7645622175f871b8a4212653089b100136a0a02`.
Die Auswahltests mit echten Images sind vorbereitet, aber noch nicht ausgeführt.
Die durchgängige Anbindung an CLI, Adminfreigabe, Planbindung und Veröffentlichung
steht ebenfalls aus; T15 bleibt offen.

## T15: Getrennte Planbindung und Ausführungsprüfung

`0e4fbe605c7fd9599f7978c6628a29277f61590b` ergänzt einen eigenen Binder und
Ausführungsmodus für die ausdrückliche private Entfernung. Die Bindung umfasst
Antragsteller, ursprüngliche Auswahl, unveränderte gemeinsame Basis, genau eine
entfernte private Auswahl, sämtliche Paketwirkungen und den vollständigen
erwarteten Paketbestand samt Installationsmarkierungen. Ein anderer privater
Eintrag darf dabei nicht verändert werden. Der interne Basisabgleich behält
seine bisherige Forderung nach unveränderten privaten Auswahlen.

Der ARM64-Komponentenbuild `/srv/aegis/runs/native-0e4fbe60-yNQxqd` ist nach
74 Sekunden erfolgreich. Im bestehenden QEMU bestehen **71 von 71 ausgewählten
Tests**, darunter sechs neue Bindungstests und fünf neue Ausführungsprüfungen
mit kleinen synthetischen Kontrolldateien. Die übrigen Fälle prüfen die bisherigen
Planbindungen und Zielableitungen. Unverändert bleiben Boot-ID
`34a66c93-f598-471b-9ef0-f4db218dbbe6`, SystemServer PID 1349 mit Startzeit 36616,
SELinux Enforcing und CE `[0, 10, 11]`.

Lokaler Ergebnisbeleg:
`out/phase1-dod/20d7d6d/native-private-removal-bound-tests/result.json`, SHA-256
`e4a73ef1e75f9e4b4318ebf3131e7483759b36e6310cf82db0e8cf78b57f5bb4`.
Rohprotokoll-SHA-256:
`a1f4cd3b76a6d39b82484ecd1a49b59986b97be8672bcc79672ec480b8cd617f`.
Buildherkunft:
`out/phase1-dod/20d7d6d/native-private-removal-bound-build-receipt.json`, SHA-256
`b17be55fba2f887114a46931b46a834e41d1e711c73133b88676e9a83f890701`.

Zwei neue Tests für echte Paketimages sind mitgebaut, aber wegen des weiterhin
knappen Gastplatzes noch nicht ausgeführt. Die laufende Sicherung früherer
Testimages ist noch nicht abgeschlossen; es wurde keines dieser Images gelöscht.
Der Planner-Transport weist den neuen Entfernungsmodus vorerst ausdrücklich ab.
Resolver, Brokerauftrag, frische Adminfreigabe und CLI-Vorschau müssen noch
durchgängig angebunden werden. Ein neuer Vollimage-Build und die T15-/T16-Abläufe
bleiben erforderlich; dieser Lauf schließt weder T15 noch Phase 1 ab.

## T15: Auftrag, Resolver und CLI-Vorschau verbunden

`209278def7d5bc5612eeb397bdd8ee20ccb16d86` verbindet die ausdrückliche persönliche
Entfernung mit dem getrennten Auswahlmodus, Planner-Protokoll 9, der signierten
APT-Zielableitung, dem eigenen Binder und der vorhandenen frischen
Adminfreigabe. Der ursprüngliche Remove-Auftrag bleibt im selben Besitzerauftrag;
er wird nicht in einen internen Basisabgleich umgedeutet. Die Vorschau unterscheidet
gemeinsame Variante, verbleibende Abhängigkeit und tatsächliche Entfernung,
einschließlich des Falls ohne Paketversionsänderung.

Native Komponenten, Java-Service, CLI und Gerätetest-APK wurden gemeinsam im
Lauf `/srv/aegis/runs/native-209278de-h7ZKvB` erfolgreich gebaut (104 Sekunden).
**76 von 76 ausgewählten kleinen nativen Tests bestehen**, einschließlich der
Modus-/Transportprüfungen. Boot-ID, SystemServer PID und Startzeit sowie SELinux
und CE-Zustand bleiben identisch zum vorherigen Komponentenlauf.

Ergebnis:
`out/phase1-dod/20d7d6d/native-private-removal-connected-tests/result.json`, SHA-256
`304262e92e9452cd858b234466280d43cb38dc7b9442f4c0bce948ee65241b74`.
Rohprotokoll-SHA-256:
`4c2f13c9be60dd8afca6a7b591eb0186e7f54f7b70647f81c69bd86db4f061f8`.
Buildherkunft:
`out/phase1-dod/20d7d6d/native-private-removal-connected-build-receipt.json`, SHA-256
`c60ae4f429de7e975fa2287c1b0c482cc8210f728339470af364fc4c27bea34a`.

Die sieben neuen APT-/Brokerübergabetests und sechs neuen Java-Tests sind bislang
nur mitgebaut. Ebenso offen bleiben die zuvor ergänzten Image-Ausführungstests,
die Veröffentlichungstests und die realen T15-/T16-CLI-Abläufe. Die lokale Sicherung
der alten Testimages läuft weiter; bislang wurde nichts daraus gelöscht.
Ein lokaler Vollimage-Build aus diesem Stand wurde gestartet. Das ist noch kein
Boot- oder Systemnachweis und schließt kein vollständiges DoD-Kriterium ab.

## Neuer Vollbuild für die Paketintegration vorbereitet

Der Vollbuild `local-20261002T214847Z-209278de-FdoErg` aus
`209278def7d5bc5612eeb397bdd8ee20ccb16d86` ist mit `LOCAL_BUILD_VERIFIED`
abgeschlossen. Das Vorbereitungsskript aus `d7a62ef235492c9bd3aedf957ea4fba4651da93f`
wurde erstmals auf diesen Build angewendet: alle 20 Image-Prüfsummen,
Kernel-/Runtime-Belege, die AVB-Kette mit AOSP-Entwicklungsschlüsseln und der
frisch erzeugte GPT-Datenträger sind erfolgreich geprüft.

Die ausschließlich lokale Vorbereitung liegt unter
`/srv/aegis/runs/phase1-209278de`. Ihr `build-validation.json` hat SHA-256
`4d573af1e337549c20b74b6dbea7d90bde9a8f7501432c8492e2e760daf1c7fa`.
Der Basisdatenträger hat SHA-256
`de828bf42764f351bccd724e0d3943e68da534a2fbf3a05e38848087b772389c`,
der AVB-Digest lautet
`d823f1aff0a7215522695547e97e7614a5ae931a200e7d225b4af005fdadb657`.

Ein neues gepaartes Profil wird für den Boot- und Integrationstest angelegt;
das bisherige Profil aus `20d7d6d` wird dafür weder ersetzt noch migriert.
Diese Vorbereitung ist noch kein bestandener Boot, kein T15-/T16-Systemnachweis
und keine vollständige Phase-1-Abnahme. Images, Profile und Rohbelege bleiben
auf dem Buildserver.

## Erster Boot des neuen Paketimages

Das Vollimage `209278def7d5bc5612eeb397bdd8ee20ccb16d86` hat im neuen Profil
`2366ca04-d587-4170-8c56-a63c8a8e1774` den ersten Boot abgeschlossen.
Authentifiziertes ADB ist über den lokalen Port 15873 bestätigt. Die Boot-ID
lautet `44dbff5f-3a76-4e97-9334-beb437fcd461`; SystemServer hat PID 1368 und
Startzeit 37230. Der aufgezeichnete Ausgangszustand enthält ausschließlich
Benutzer 0, CE `[0]`, leere Runtime-Kontexte, SELinux Enforcing und `managed-v1`.
Android meldet 720 × 1280 Pixel bei 320 dpi und 7.4 GiB freien Gastplatz.
Bootanimation und anschließende Bildausgabe wurden lokal betrachtet;
die vollständige Eingabeprüfung und Dienstabnahme bleiben offen.

Der lokale Beleg `out/phase1-dod/209278de/boot-baseline.json` hat SHA-256
`19eb1f8ed1ce946c7b9b0ecebaf50bdab17f72c54db681788f29756f7ac2f7eb`.
Dieser erste Boot schließt weder D1 noch die Paket- oder Benutzerabnahme ab.
Die Komponentenprüfungen werden vor der Anlage persönlicher Benutzer
ausgeführt; die realen CLI-Abläufe und gepaarten Neustarts folgen getrennt.

## Java-Paketprüfungen auf dem neuen Vollimage

Die beiden Klassen `PackageBrokerProtocolTest` und `PackageTransactionTest`
bestehen mit **38 von 38 Tests** auf dem neuen Image, einschließlich der sechs
zuletzt ergänzten Fälle zur privaten Entfernung. Der Lauf verwendet das
geprüfte Test-APK aus dem Komponentenbuild desselben Source-Commits und das
versionierte Werkzeug `scripts/qemu-package-component-tests.py`. Boot-ID,
SystemServer PID/Startzeit, SELinux Enforcing und CE `[0]` bleiben unverändert.

Ergebnis: `out/phase1-dod/209278de/java-package-components/result.json`, SHA-256
`699b55c347adaf7c8b31d039dd5d08955de902c2e6a372797cfd0592be99dd02`.
Log-SHA-256:
`94fa7a38992689b2f1a8e171c1422941a32b487b4fd7acb3030ec48f4eb1d7f9`.
Testtreiber-SHA-256:
`b5a938ee646a6164a776d5547f66eaa0492ba5235e10fc82829c0a4d54ca1c8d`.

Diese Tests prüfen Metadaten und Transaktionszustände. Sie belegen keine echte
AOSP-Anmeldung, Adminfreigabe oder Paketinstallation. Die nativen Imageprüfungen
und die durchgängigen CLI-Nachweise bleiben offen.

## Native Auswahl: unvollständige Beobachtung der Vorbereitung

Alle sechs ausgewählten nativen Auswahlfälle scheitern auf `209278d` bereits
in der gemeinsamen Testvorbereitung. Die Testschleife erwartet nach etwa neun
Sekunden `Prepared`, beobachtet aber weiterhin `Preparing` mit Fehlercode 0.
Sie beendet damit die Beobachtung eines noch laufenden Auftrags; die eigentlichen
Auswahlwirkungen sind durch diesen Lauf nicht geprüft. SystemServer, Boot-ID,
SELinux und CE bleiben unverändert. Die fehlgeschlagenen Testimages bleiben erhalten.

Ergebnis: `out/phase1-dod/209278de/native-selection-components/result.json`, SHA-256
`8600f2592b6fb6cafe643eefc77b8676403c7d414a660388630335a20b26d88d`.
Log-SHA-256:
`6ef4b65479771d9de6a9b8ed7dd31f54cd2f1bfdc73d3300c7adc940b4eeed65`.

`6946970adc5158ffafa0d8771b22afbee12fd5a5` korrigiert ausschließlich die
Beobachtungsbedingungen in `runtime/package_executor_tests.cpp`: derselbe
Vorbereitungs-/Veröffentlichungsauftrag wird bis zu 180 Sekunden beobachtet;
ein laufender Auftrag wird nicht erneut gestartet. Produktcode, Zulassungsfristen
und die explizite Abbruchprüfung bleiben unverändert. Der Komponentenbuild
`/srv/aegis/runs/native-6946970a-Q1YLjb` besteht nach 47 Sekunden. Die vollständigen
Quellinventare unterscheiden sich ausschließlich in dieser Testdatei;
alle zehn Helper-Binaries und das Java-Test-APK sind bytegleich zum Image-Stand.

Der neue Buildbeleg
`out/phase1-dod/209278de/native-observed-preparation-build-receipt.json` hat SHA-256
`ffb57deaf0ab87bf2d91201f387028eb786e3964bcb8e8f414ef2e58fe643633`.
Die Wiederholung mit korrigierter Beobachtung steht noch aus. Der längere
Beobachtungszeitraum allein ist kein bestandener Test und kein Produktnachweis.

## Sieben Plantests für private Entfernung bestanden

Die sieben Fälle aus `RuntimePrivateRemovalPlanning` bestehen auf dem neuen
Vollimage mit unveränderten Komponenten aus `209278d`. Der Lauf dauert rund
805 Sekunden einschließlich Nachbeobachtung. Geprüft sind die Rückkehr zu
neuerer und älterer gemeinsamer Version samt passender Bibliothek, Entfernung
einer privaten Festlegung ohne Paketwirkung, Entfernung nicht mehr benötigter
privater Pakete, Erhalt einer weiterhin benötigten Abhängigkeit, Ablehnung eines
verbleibenden Versionskonflikts und die durchgängige Broker-Zuordnung zu
Antragsteller, ursprünglichem Remove-Auftrag und aktueller gemeinsamer Basis.

Boot-ID `44dbff5f-3a76-4e97-9334-beb437fcd461`, SystemServer PID 1368/Startzeit
37230, SELinux Enforcing und CE `[0]` bleiben unverändert.
Ergebnis: `out/phase1-dod/209278de/native-planner-components/result.json`, SHA-256
`44133a596587d837ef72e4613e09d58c524733965ac3df7c5db4cc14391d753e`.
Log-SHA-256:
`da97d6499ab8baf6bfa6f4926ddb8c8c8a7f1bdc150889b520b2213c61385fca`.

Dies belegt die APT-Zielableitung und den Brokerauftrag. Die vollständige
Ausführung und Veröffentlichung sowie frische CLI-Adminfreigabe bleiben
separate Nachweise. Die Wiederholung der sechs Auswahltests mit `6946970`
ist gestartet; sie ist zum Zeitpunkt dieses Eintrags noch nicht abgeschlossen.

## Bedienung, ADB und Kryptographiekonfiguration auf 209278d

Die normale Bedienfolge besteht auf dem neuen Image: Von der bestätigten
Settings-Hauptseite öffnen einmaliges QMP-TAB/RET die Netzwerkeinstellungen.
Ein QMP-Mausklick auf „Navigate up“ bei der tatsächlich beobachteten Position
`(57.165,106.163)` führt zurück. Screenshots und UI-XML bestätigen beide
Übergänge. Erste XML-Abfragen während des Seitenwechsels lieferten keinen
Root-Knoten; die erneute Beobachtung wiederholt keine Tastatur- oder Mauseingabe.

Der binäre ADB-Rundlauf überträgt alle 256 Bytewerte, jeweils 1024-mal, und liest
262144 identische Bytes zurück. Der eindeutige temporäre Gast-Probeeintrag wird
nach dem Vergleich entfernt. ADB-Authentifizierung ist weiterhin aktiviert.
Boot-ID, SystemServer PID/Startzeit, SELinux Enforcing und CE `[0]` bleiben
unverändert zum Ausgangszustand.

Lokale Nachweise unter `out/phase1-dod/209278de/`:

- `qmp-ui-proof.json`: SHA-256
  `9f1851ce0aa0891f646692d986386fe2634654dc4d1471e799a8295f728975ee`.
- `adb-binary-proof.json`: SHA-256
  `fe10580c61b365251bfac95c0f90317b29e5e66c01aead03a1efcdd4e04e995a`.
- `crypto-current-observation.json`: SHA-256
  `4e35d69d8b48a5376766c80b12a3b408ef65203be12d66121ef356e3021b1479`.

Der letzte Beleg bestätigt aktuelle FBE-/Metadaten-Properties, die unveränderte
installierte fstab und die Kernelbeobachtung von AES-256-XTS/HCTR2. Vier
dokumentierte AOSP-Kryptoquellen sind bytegleich; siehe
[Kryptographiegrundlage](identity-crypto-baseline.md). Dies ersetzt keine
persönliche Anmeldung, keinen CE-Entzug und keinen gepaarten Neustart. D1 und
die vollständige Phase-1-Abnahme bleiben offen.

Die begrenzte Bootbeobachtung ist zusätzlich lokal eingefroren:
`out/phase1-dod/209278de/init-exit-classification.json`, SHA-256
`69f85c407c7e21cae1dd3cbdd90fdb134759748a04090ae0fdc2251bc49396cf`.
Die darin gehashten Log-Snapshots enthalten genau einen SystemServer-Start und
keine Java-Fatal-, Fatal-Signal-, FORTIFY-, Watchdog-Abbruch- oder ANR-Einträge.
Die frühen einmaligen Rückgaben sind mit aktuellen Quellen und Gastzustand
abgeglichen: übersprungener System-Mainline-Initializer bei erfolgreichem aktivem
Initializer, `misctrl` mit erfolgreicher boolescher Property-Setzung im Exitcode
und Recovery-Refresh bei leerem pstore. Letzteres bleibt ausdrücklich eine
Quell-/Zustandsinferenz. Die Signal-Exits von odsign, hwservicemanager, idmap2d
und adbd passen zu den protokollierten Stop-/Restart-Vorgängen. Es wurden keine
Dienste umkonfiguriert oder Diagnoseabstürze ausgelöst. Der Nachweis gilt für
den erfassten Zeitraum und ersetzt keine spätere Lebenszyklusprüfung.

## Sechs native Auswahltests mit korrigierter Beobachtung bestanden

Die Wiederholung mit Testbuild `6946970adc5158ffafa0d8771b22afbee12fd5a5`
besteht mit **6 von 6 Fällen** auf dem unveränderten Vollimage `209278d`.
Der Lauf dauert rund 1423 Sekunden. Geprüft sind private/alte/aktuelle Ansichten,
bereits aktuelle private Basis, Auswahl für ausdrückliche private Entfernung,
Ablehnung einer veralteten Basis, Nutzung der aktuellen Werksbasis ohne
gemeinsamen Paketstore und Abbruch mit unverändertem Bestand und geschlossenen
Ansichten. Der frühere fehlgeschlagene Lauf bleibt als eigener Beleg erhalten.

Der Treiber hat vor dem Start die vollständigen Quellinventare verglichen:
nur `runtime/package_executor_tests.cpp` unterscheidet sich. Alle zehn Helper
und das Java-Test-APK sind bytegleich. Boot-ID, ursprünglicher SystemServer,
SELinux Enforcing und CE `[0]` bleiben vor und nach dem Lauf identisch.

Ergebnis:
`out/phase1-dod/209278de/native-selection-observed-components/result.json`, SHA-256
`7ddd2fbb32643a50bd37963cab37d44f1aa3ec6dd7cfb602a293cc265533aaed`.
Log-SHA-256:
`e77161d7d8a690e85ac25f089c84ec1d6a6310bd13bfbb288ab8a1ff965fa110`.
Die Paket-Ausführungsgruppe ist anschließend gestartet; Veröffentlichung und
reale CLI-Nachweise bleiben noch offen. Diese sechs Fälle schließen weder T15
noch T16 vollständig ab.

## Fünf native Paketausführungstests bestanden

Die Ausführungsgruppe besteht mit **5 von 5 Fällen** auf Vollimage `209278d`
mit dem ausschließlich im Testcode korrigierten Build `6946970`. Der Lauf
dauert rund 523 Sekunden. Upgrade und Downgrade erhalten persönliche
Konfiguration, Dateieigentümer und private Paketauswahl. Ein Abgleich ohne
Paketwirkung verändert nur die vollständigen Paketmarkierungen. Die private
Entfernung stellt die gemeinsame Version samt passender Abhängigkeit wieder
her; bei gleicher Version ändert sie das Manifest ohne Paketskripte.

Die Quell- und Helper-Gleichheit ist im Ergebnis dokumentiert. Boot-ID,
SystemServer PID/Startzeit, SELinux Enforcing und CE `[0]` bleiben identisch.
Ergebnis: `out/phase1-dod/209278de/native-execution-components/result.json`,
SHA-256 `ed01f7b71df8d3d58383af426aa61f8adf7e71b3546da3f805ce20d70bc962da`.
Log-SHA-256:
`c162a373646e5066fc3f509fc9ae09f3585d7c7c79f255940508035089529155`.

Die zwei Veröffentlichungstests und der vollständige CLI-Ablauf bleiben offen.
Diese Komponententests bedeuten keine vollständige Abnahme von T15/T16 oder
Phase 1. Es wurden keine gezielten Diagnoseabstürze wieder aufgenommen.

## Passworttransport: Quellenbindung für 209278d

Der frühere Transportnachweis ist mit dem gehashten Quellinventar des aktuellen
Vollbuilds abgeglichen. In den zehn betrachteten Identitätsquellen unterscheidet
sich nur die Paketplananzeige der CLI: Sie beschreibt nun private Entfernung
und erlaubt deren ausdrücklich ausgewiesene wirkungslose Versionsaufhebung.
Passworteingabe, Terminalcode, sensible AIDLs, LockSettings-Transport,
AOSP-Backend und Paket-Adminprüfung sind in dieser Auswahl unverändert.

Die drei früheren `CredentialTransportTest`-Fälle sind über Originalergebnis,
Loghash und identische Transport-/Testquellen weiterhin als ergänzende
Komponentenbelege zuordenbar. Es wird keine neue Ausführung behauptet.
Lokaler Beleg: `out/phase1-dod/209278de/credential-source-continuity.json`,
SHA-256 `8d4eaac1455f840ecfefb05e093b8be619b700c429818c69f02112bc869bf1cf`.
Persönliche Anmeldungen sowie History-, Umgebungs- und relevante Dateiprüfungen
auf dem aktuellen Image bleiben offen; T01 ist damit nicht abgenommen.

## Beide nativen Veröffentlichungstests bestanden

Beide Fälle aus `RuntimeReconciliationPlanning` bestehen auf Image `209278d`
mit dem quellgebundenen Testbuild `6946970`. Der erste führt den signierten
Drei-Ansichten-Plan aus, veröffentlicht ihn und prüft nach erneutem Öffnen
Programm, Bibliothek, private Konfiguration und technischen Dateieigentümer.
Der zweite veröffentlicht die neue gemeinsame Basisbindung bei unveränderter
ausdrücklich gewählter privater Version ohne Paketänderungen.

Der Lauf dauert rund 740 Sekunden. Vorher und nachher stimmen Boot-ID,
SystemServer PID/Startzeit, SELinux Enforcing und CE `[0]` überein.
Ergebnis: `out/phase1-dod/209278de/native-publication-components/result.json`,
SHA-256 `c954cb02547c5c572980972f6239948cb15115891a93d673cbc5a829238e6c28`.
Log-SHA-256:
`e1e2a2be6cbf77b5bb48842d2f4ceb1bffe766234283c92f2e6b29744329b2ff`.
Dies sind Komponentenbelege; frische AOSP-Adminfreigaben und die tatsächliche
Aktivierung im persönlichen CLI-Ablauf bleiben gesondert nachzuweisen.

## Erster persönlicher CLI-/GNU-Ablauf auf 209278d

Die echte CLI legt Alpha als AOSP-Admin `10/10` an. Danach ist CE `[0]`;
auch die Vorbereitung der ersten Anmeldung entsperrt Alpha noch nicht und
legt keinen GNU-Kontext an. Erst das richtige Passwort erlaubt Zugang.
Ohne vorherigen Fehlversuch bestehen die verzögerte Sitzungsprüfung,
Runtime-Start und tatsächliche GNU-Ausführung.

Die zehn anfänglichen HOME-Verzeichnisse gehören `1000:1000` mit Modus `700`.
Debian 13.7, glibc 2.41, bash, apt, dpkg und GNU-Werkzeuge funktionieren.
Die beobachtete Host-Zuordnung lautet `1007500`; Runtime-Domäne, Mountlayout,
schreibgeschützte Basis, leere Capability-Mengen, NoNewPrivs und Seccomp
entsprechen den Grundprüfungen. Dies ist noch kein Zwei-Benutzer-Isolationsbeleg.

Alpha schreibt seine ursprüngliche 1024-Byte-Datei aus GNU und liest sie zurück:
SHA-256 `3eb21b6ca42204a1c533d3f9d7191c06ee67430258477d1ef25ae969e4439750`.
Persönliche Konfigurations-/Testdateien sind ebenfalls angelegt und gelesen.
Nach Shell-Ende bleibt Alpha angemeldet und derselbe Hintergrundprozess
PID `8453`, Startzeit `592964`, schreitet fort. Boot-ID und ursprünglicher
SystemServer bleiben unverändert; SELinux ist Enforcing, CE `[0, 10]`.

Der eingefrorene Beleg enthält die Ereignisse, Eingabenbindung und Hashes:
`out/phase1-dod/209278de/alpha-first-flow/result.json`, SHA-256
`a08f725588f7a841f875b54c3eba4d255d7767b55a3c332e5fe6b3c6b1fce663`.
Die originalen Dateien und der laufende Treiber bleiben für spätere
Persistenz-/Lebenszyklusprüfungen erhalten. Paketinstallation, Beta, Logout,
Neustart und die ergänzenden Pflichtvarianten sind damit nicht abgenommen.

## Gemeinsames jq u3 veröffentlicht, aktiviert und ausgeführt

Der echte CLI-Plan fordert gemeinsam `jq` und `libjq1` jeweils
`1.7.1-6+deb13u3` sowie `libonig5` `6.9.9-1+b1` an. Alphas frische
AOSP-Adminprüfung erlaubt die Veröffentlichung. Die gemeinsame Generation
lautet `9d6005a520d3d932fd2fe7664a405da75d00d06716d60ac060992967ac388cc4`.
Währenddessen bleibt Alphas ursprünglicher Hintergrundprozess mit derselben
Startzeit auf der Werksbasis aktiv; die CLI meldet `activation-pending`.

Nach regulärem `linux stop`/`linux start` zeigt die CLI `packages=current`.
Der tatsächliche Root-Mount gehört jetzt zur veröffentlichten Generation.
Eine echte GNU-Ausführung bestätigt beide u3-Versionen, die passende geladene
Bibliothek und die jq-Berechnung `[1,2,3] | add == 6`. Der vollständige
installierte Bestand mit 81 Paketen ist als Vergleichsbasis eingefroren.
Alphas ursprüngliche Datei und Konfiguration bleiben bytegleich, die alten
persönlichen `/tmp`-/`/run`-Proben sind verschwunden. Der Runtime-Stopp wird
ausdrücklich nicht als Logout gewertet: CE bleibt entsperrt.

Lokale Belege:

- `out/phase1-dod/209278de/shared-u3-before-activation.json`, SHA-256
  `1d746ddd84e61c910c96f323eb05a27b77fb4056e78f575a2bee2096bc7cedd9`.
- `out/phase1-dod/209278de/shared-u3-activation/result.json`, SHA-256
  `2c1029a6d2a63a64aeb3a5440524802e1d0ef7911f53d67da61dc10e129601f9`,
  einschließlich gehashtem Ereignissnapshot und vollständigem dpkg-Status.

Die private u4-Installation für Alpha ist anschließend angefordert. Ihr Plan
und ihre tatsächliche Ausführung bleiben bis zu deren Ergebnis offen.

## Alphas private u4-Version bei unverändertem gemeinsamem u3

Der persönliche Plan ändert ausschließlich `jq` und `libjq1` von
`1.7.1-6+deb13u3` auf `1.7.1-6+deb13u4`. Nach frischer AOSP-Adminfreigabe
gehört die veröffentlichte private Generation
`1fc85751f125f95df24b4c81dc07229a1a9a02b83366b41e0286052fb5728746`
zu Alpha `10/10` und bleibt an die unveränderte gemeinsame Generation gebunden.
Der alte laufende Kontext bleibt zunächst auf gemeinsamem u3;
`activation-pending` wird korrekt angezeigt.

Nach dem regulären Kontextneustart ist das private Image tatsächlich aktiv.
GNU bestätigt u4 für beide Pakete, führt die jq-Berechnung erfolgreich aus
und lädt die passende Bibliothek. Der Vergleich aller 81 installierten
Paketversionen zur gemeinsamen Basis findet genau diese beiden Änderungen.
Alphas ursprüngliche Datei und Konfiguration bleiben unverändert.

Ein zusammengesetzter Diagnosebefehl endete zunächst mit Fehler, weil zusätzlich
die interne `private-choices`-Datei aus dem gewöhnlichen GNU-Kontext gelesen
werden sollte. Dieser Fehlbeleg bleibt erhalten. Der separat ausgeführte
Programm-/Versionscheck besteht. Der Entwicklungsbeobachter bestätigt die
Festlegung auf jq u4 sowie Eigentümer/Modus `1005000:1005000:600`; die
Metadatenrechte wurden nicht verändert. Die interne Auswahl wird nicht als
aus GNU gelesener Beleg dargestellt.

Lokale Belege:

- `out/phase1-dod/209278de/alpha-u4-before-activation.json`, SHA-256
  `2818fabb1f96bda6258c827438419bccb2f808e05988c539207daea79b2ab90f`.
- `out/phase1-dod/209278de/alpha-u4-activation/result.json`, SHA-256
  `3e942b3d1bb018ad1b8a9dad03e9f7a57c3f57c1616825d9d9071736e73080b2`,
  mit Ereignissnapshot, Paketregister und vollständigem Versionsvergleich.

Betas Ausführung, gegenseitige Isolation, Logout und gepaarter Neustart stehen
noch aus. T15 und der vollständige Referenzablauf bleiben offen.

## Zwei angemeldete Benutzer mit tatsächlich verschiedenen Versionen

Beta wird über die CLI mit frischer Adminbestätigung als normaler AOSP-Benutzer
`11/11` angelegt. Vor seiner ersten Passwortübermittlung bleiben nur System
und Alpha entsperrt; Beta besitzt noch keinen GNU-Kontext. Sein erster korrekter
Login ohne vorherigen Fehlversuch, verzögerte Sitzungsprüfung und tatsächliche
GNU-Ausführung bestehen. Die anfänglichen HOME- und Runtime-Grundprüfungen
bestehen ebenfalls.

Beta führt die gemeinsame jq-/libjq1-Version u3 mit passender Bibliothek aus.
Sein vollständiger Bestand entspricht allen 81 Paketen der gemeinsamen Basis.
Alphas privates u4-Image ist gleichzeitig weiterhin aktiv. Beide haben intern
UID/GID 1000, aber Host-UIDs 1007500 und 1107500 und sechs unterschiedliche
Namespace-Identitäten. Alpha bleibt beim Wechsel in den Hintergrund mit
demselben Prozess `10172/748802` aktiv; Betas Prozess ist `12013/791922`
(jeweils PID/Startzeit). Beide Fortschrittszähler steigen. CE ist `[0, 10, 11]`;
Boot-ID, SystemServer und SELinux bleiben unverändert.

Betas ursprüngliche 1024-Byte-Datei hat SHA-256
`e70532731b672a9222adfb11f0394da224873314a893b7c43ad95fd82d2c8793`.
Eigene Konfiguration und temporäre Proben sind ebenfalls angelegt und gelesen.
Der eingefrorene gemeinsame Beleg enthält beide aktiven Imagezuordnungen,
Paketregister, Namespace- und Prozessdaten sowie den gehashten Ereignissnapshot:
`out/phase1-dod/209278de/two-user-version-baseline/result.json`, SHA-256
`7c3ec5ef166622fc6139757b6b90229f1633e479d30d6477ef0ae5187d1b0ba5`.

Dies ist der Ausgangszustand für die gegenseitigen Zugriffstests; deren
Ablehnung wird hier noch nicht behauptet. Logout, Reboot, private Entfernung,
gemeinsames Update und die übrigen Pflichtvarianten bleiben offen.

## Gewöhnlicher Datei-/Prozesszugriff Beta zu Alpha abgewiesen

Aus Betas angemeldeter GNU-Shell liefern die geprüften Pfade zu Alphas
ursprünglicher Testdatei keine Bytes. Alphas Prozess ist in Betas Prozesssicht
nicht zugänglich; das versuchte Stoppsignal wirkt nicht. Der unabhängige
Beobachter bestätigt vor und nach dem Versuch denselben Alpha-Prozess
`10172/748802` mit fortschreitendem Zähler. Beide Kontexte sind dabei entsperrt.

Beleg: `out/phase1-dod/209278de/beta-to-alpha-file-process-proof.json`, SHA-256
`5e40a465cb0080717177a95e309a0fea6d732add53998b71cdd159e313834abd`.
Der Beleg umfasst diese Richtung und diese Datei-/Prozessfälle. Gegenrichtung,
Konfiguration, Paketstore, temporäre Dateien und IPC bleiben gesondert zu
prüfen; T06 ist noch nicht vollständig abgenommen.

## Gegenrichtung und externer Benutzerwechsel bestätigt

Auch Alpha zu Beta besteht den gewöhnlichen Datei-/Prozessfall. Die benannten
fremden Dateipfade liefern keine Bytes; Betas Prozess ist nicht zugänglich,
und der ursprüngliche Prozess `12013/791922` schreitet vor und nach dem
versuchten Signal fort. Beide Richtungen sind zusammen gebunden in
`out/phase1-dod/209278de/reciprocal-file-process-proof.json`, SHA-256
`6b288f84c2af9d8bb3d9c83de80b9f0c29fbdc536bb6b5709f5e0eb121942bc7`.

Der zuvor über eine zweite echte CLI ausgeführte Wechsel Beta zu Alpha ist
ebenfalls eingefroren: frische AOSP-Anmeldung, Widerruf des offenen Beta-GNU-
Terminals und fortbestehender ursprünglicher Beta-Hintergrundprozess.
Beleg: `out/phase1-dod/209278de/external-switch-beta-to-alpha.json`, SHA-256
`006c80836422da783e008346593c4976f61245118a3249b2e4432f498ce770b9`.
Dies ist kein Logout- oder CE-Schlüsselentzugsnachweis.

Für die noch folgenden Temporärdatei-Zugriffstests sind ausschließlich Alphas
flüchtige Proben im aktuellen Kontext neu angelegt. Ihre vorherige Entfernung
nach Kontextneustart bleibt belegt; die ursprünglichen persistenten Dateien
werden nicht neu erzeugt. Die getrennte Belegkette liegt in
`out/phase1-dod/209278de/alpha-ephemeral-renewal.json`, SHA-256
`c291061086d23432dbcfbade2ae57d7e4a07abae0f61783a6ab2194c86cde147`.
