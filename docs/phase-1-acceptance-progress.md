# Vollständige Phase-1-Abnahme: Arbeitsstand

Beginn: 1. Oktober 2026. Ziel ist die vollständige
[DoD](architecture/phase-1-dod.md), einschließlich aller Varianten T01–T17.
**Status: aktiv, keine vollständige Abnahme.** Aktueller Prüfstand ist das
Korrekturimage `f098f439f051c34e92fb1be0b4d908cc542358ea`, Profil
`535c2e93-df64-445b-b9e2-b71e6b403db7`, zweiter Boot
`17a75d6e-75f2-4f18-bf02-ec3d093e57b8`. Seine Ergebnisse sind im Abschnitt
„Zusätzlicher Korrekturstand f098f43“ des [Ergebnisindex](phase-1-result-index.md)
gesondert zugeordnet. Die folgenden älteren Ergebnisse ersetzen keine offenen
Prüfungen auf diesem Korrekturimage.

## Historischer Prüfstand 209278d

Image `209278def7d5bc5612eeb397bdd8ee20ccb16d86`, Profil
`2366ca04-d587-4170-8c56-a63c8a8e1774`, dritter Boot mit ID
`e720d2bf-faef-4af8-87b9-709112eb3c41`.
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
Hintergrundprozesse sind erfasst. Gegenseitige Datei-/Prozess-/Konfigurations-
und POSIX-Mqueue-Prüfungen, beide externen Wechselrichtungen, Bildschirmsperre
und beide regulären Abmeldungen sind belegt. Das Profilpaar ist sauber gestoppt
und erneut gestartet; persönliche Daten sind vor Anmeldung gesperrt. Beide
ersten korrekten Logins danach sowie bytegleiche Originaldateien/Konfigurationen
und tatsächlich getrennte jq-Versionen sind ebenfalls belegt. Gamma wurde
nachträglich angelegt und führte gemeinsame Software aus; seine vollständige
Fremddatenprüfung bleibt offen. Betas Passwortwechsel und beide ursprünglichen
Datensätze sind auch über den zweiten gepaarten Neustart belegt.
Das gemeinsame Update und beide Aktivierungen einschließlich Alphas privater
Versionsbindung bestehen inzwischen auf diesem Image (E31/E32). Die vollständige
ergänzende Matrix bleibt offen.
Der [Ergebnisindex](phase-1-result-index.md) trennt
einzelne aktuelle Varianten von historischen Teilbelegen.

## Persönliche Paketfreigabe für Beta, 3. Oktober 2026

Nach dem gemeinsamen Update beantragt Beta über seine eigene authentifizierte
CLI `jq=1.7.1-6+deb13u3` im Bereich `user`. Der signiert vorbereitete Plan
enthält genau jq und libjq1 von u4 auf u3. Die leere Adminauswahl bricht ihn
um 04:53:44 UTC ohne Veröffentlichung ab (E33); ein einzelnes falsches
Alpha-Adminpasswort wird um 05:00:58 UTC von AOSP abgewiesen (E34).
Beide vollständigen aktiven Kontexte, gemeinsame und Alpha-private Auswahl
bleiben unverändert; Beta hat weiterhin keine ausgewählte private Generation.
Um 05:07:31 UTC wird auch die Freigabe durch Beta abgewiesen (E35); seine
Rolle bleibt normal und die bestehende CLI-Sitzung gültig. Die generische
Fehlermeldung verlangt trotzdem irreführend eine erneute Anmeldung.

Der anschließende Beta-Auftrag erhält um 05:17:14 UTC eine gültige frische
Alpha-Adminfreigabe. Die Ausführung scheitert jedoch um 05:17:40 UTC vor
Veröffentlichung. E36 bestätigt beide unveränderten vollständigen Kontexte
und Auswahlen; Beta hat weiterhin keine private Generation. Dieser Auftrag
ist ausdrücklich kein erfolgreicher Installationsnachweis. Sein inaktiver
Staging-Kandidat wurde regulär bereinigt; der genaue APT-Fehlertext ist aus
diesem CLI-Lauf nicht erhalten.

Der Quellvergleich zeigt einen Unterschied: Explizite Versionsplanung erlaubt
APT den angeforderten Versionsrückgang; die Ausführung übergibt diese Option
bisher nur bei internen Abgleichs-/privaten Entfernungsaktionen. Ein neuer
gewöhnlicher Archive-Regressionstest mit geprüfter 2→1-Auswahl, passender
Bibliothek, privater Auswahl und erhaltener Konfiguration soll den Fehler
zunächst gegen unveränderten Ausführungscode reproduzieren. Die Ursache und
Korrektur sind damit noch nicht durch einen ausgeführten Regressionstest
bestätigt. Das aktive persönliche Profil bleibt unverändert erhalten.

Der Ergebnisindex ordnet außerdem die vorhandenen abgewiesenen Runtime-Starts
aus E23/E24 T04.1 zu. Sie belegen den konkreten CLI-Fall bei gesperrtem CE,
isolieren aber die CE-Voraussetzung wegen zugleich fehlender CLI-Sitzung
nicht. Es wurde dafür kein zusätzlicher Login-Fehlversuch ausgeführt.

## Gemeinsames Update und private Aktivierung, 3. Oktober 2026

Um 04:21:10 UTC meldete der mit frischer Alpha-Adminfreigabe ausgeführte
gemeinsame Updateauftrag die Veröffentlichung. Die gemeinsame Generation ist
jetzt `1a6c3fe780ce47378453186b098964ee79c16c63539b55aa0c0f6d8250525a16`.
Beide laufenden Kontexte behielten zunächst ihre vollständigen Paketbestände,
Prozessstartzeiten, Mounts, Namespace-Zuordnungen und Alphas private Auswahl.
Beide meldeten `activation-pending` und führten ihre bisherigen jq-Versionen
mit den alten Bibliotheksrevisionen weiterhin erfolgreich aus (E31).

Betas eigener Kontextneustart aktivierte die gemeinsame Generation. Alphas
anschließender eigener Neustart führte den privaten Abgleich durch und band die
neue private Generation
`12fee076517596f0005e2a0545446471b55bc9bc5ce4b6100a651f6280af92b9`
an diese gemeinsame Basis. Seine explizite private jq-Festlegung blieb bytegleich.
Jeder Neustart erhielt den jeweils anderen Kontext vollständig unverändert.

Beide tatsächlichen 81-Paket-Bestände entsprechen exakt den geplanten Änderungen:
jq/libjq1 `1.7.1-6+deb13u4`, PCRE2 `10.46-1~deb13u3` und libssl3t64 sowie
openssl-provider-legacy `3.5.7-1~deb13u3`. Beide Statusabfragen melden `current`;
gewöhnliche GNU-Prozesse führen jq aus und bestätigen den passenden libjq-Hash.
Beider ursprüngliche Dateien und Konfigurationen bleiben bytegleich, alte
flüchtige Testdateien sind nicht vorhanden (E32).

Damit ist der zuvor reproduzierte Fall „aktueller Status trotz fehlender
gemeinsamer Bibliotheksupdates“ auf dem aktuellen Image positiv nachgeprüft.
Dieser Lauf führt gemeinsame und private jq-Version auf denselben Wert u4;
er belegt zusätzlich ausdrücklich den Erhalt der privaten Auswahlmetadaten.
Die frühere gleichzeitige Ausführung unterschiedlicher Versionen steht in E02/E04.
Konflikte, Entfernungen, weitere Autorisierungskombinationen und Parallelität
bleiben eigenständige offene Pflichtfälle. Produktcode und Image wurden in
diesem Prüfabschnitt nicht geändert.

## Ergänzung: vorhandene Update-Ablehnungen, 3. Oktober 2026

Im dritten Boot `e720d2bf-faef-4af8-87b9-709112eb3c41` wurde die
gemeinsame Updatefreigabe um 03:52:17 UTC mit einem falschen Adminpasswort
abgewiesen (E29). Um 03:59:14 UTC wurde auch die Freigabe durch den normalen
Benutzer Beta abgewiesen (E30); die anschließende Benutzerliste bestätigt
seine unveränderte Rolle. Zusammen mit dem leeren Adminfeld aus E28 sind die
drei Ablehnungsarten für `update all` belegt. Die Snapshots bestätigen jeweils
unveränderte Paketgenerationen, Paketbestände und aktive Runtime-Kontexte.
Die erlaubte Ausführung sowie Aktivierung und private Versionsbindung nach
dem gemeinsamen Update bleiben offen. Die Belege wurden nach einer gemeldeten
Plattformunterbrechung gelesen und indexiert; die Tests wurden dafür nicht
wiederholt. Buildartefakte und Rohbelege bleiben lokal.

Der konkrete Auslöser der vom Benutzer gemeldeten Security-Meldung ist aus
diesen Projektbelegen nicht feststellbar. Die Meldung wird weder als Beweis
eines Richtlinienverstoßes noch als bestätigter Fehlalarm eingeordnet.
Weitere Arbeit bleibt auf das eigene AEGIS-Testsystem und die vereinbarten
Entwicklungs- und Funktionstests begrenzt. Eine abgewiesene Aktion wird vor
einem weiteren Versuch anhand ihres konkreten Ablehnungsgrundes geprüft;
Schutzmechanismen werden nicht umgangen oder abgeschwächt.

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
| T02 | f098f43: Passwortwechsel, bestätigte Sperrung, altes Passwort abgewiesen; neues Passwort und ursprüngliche Daten/beide privaten Paketbestände auch nach vollständigem gepaarten VM-Neustart bestätigt | Abschließende Zuordnung einschließlich AOSP-Schlüsselverwaltung; eine zusätzliche Ablehnung des alten Passworts im dritten Boot ist noch nicht ausgeführt |
| T03 | UID/GID, sechs getrennte Namespaces, zwei fortlaufende Prozesse | Nachweise zum finalen Versions-/Referenzablauf zuordnen |
| T04 | Admission-/Namespace-/Mapping-Komponententests | Gezielte Gasttests für alle fehlenden Voraussetzungen und fehlende Berechtigung zuordnen/ausführen |
| T05 | GNU-Shell, Exit und Zugriff auf eigene Datei | Ablehnungen für gesperrte/fremde Kontexte und Exit-Lebenszyklus zum finalen Stand binden |
| T06 | Gegenseitige Dateien, Konfiguration, Test-Secrets, temporäre Dateien, Prozesse und POSIX-Mqueues; zusätzlich beide tatsächlich vorhandenen privaten Paketauswahlen auf f098f43 | Alle Varianten zum finalen Image binden; die Prüfung der Auswahldateien belegt nicht sämtliche privaten Paketdateien |
| T07 | Beide ursprünglichen Dateien/Konfigurationen nach Reboot; auf f098f43 außerdem beide privaten jq-Versionen u4/u3 nach Passwortwechsel und erneutem Paarneustart erhalten; alte temporäre Probepfade fehlen | Im finalen Ergebnisindex die getrennten positiven Vorzustände der temporären Proben und die jeweiligen Neustartvarianten zuordnen |
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

## Persönliche Konfiguration, flüchtige Dateien und IPC getrennt

Beide gewöhnlichen GNU-Kontexte können die geprüften persönlichen
Konfigurations-/Testdateien und flüchtigen Dateien des anderen Benutzers weder
lesen noch verändern. Der Beobachter bestätigt jeweils die ursprünglichen
Bytes und den fortschreitenden ursprünglichen Peer-Prozess nach dem Versuch.
Beta zu Alpha umfasst zusätzlich Alphas tatsächlich vorhandenen privaten
Paketstore-Eintrag. Beta besitzt zu diesem Zeitpunkt keinen privaten Store;
dessen fehlender Gegenrichtungsnachweis bleibt offen.

Beleg: `out/phase1-dod/209278de/reciprocal-private-state-proof.json`, SHA-256
`b09a95151346b9c1dc27c94a4516e48be02f8a77d6cc553b57751bb7e7c8c256`.

Die POSIX-Nachrichtenwarteschlangen sind ebenfalls in beiden Richtungen
geprüft: Beide GNU-Benutzer legen den gleichen Queue-Namen an und empfangen
daraus ausschließlich ihre jeweils eigene Nachricht. Die benannte Queue des
anderen Kontexts ist mit ENOENT nicht zugänglich. Beleg:
`out/phase1-dod/209278de/reciprocal-mqueue-proof.json`, SHA-256
`c6761a190c94640c5452d79db494ebdb3a6263e4a47b367ba9f9a2fe859470c3`.
Dies behauptet weder jede mögliche IPC-Schnittstelle noch bereits den
Ressourcenabbau bei Logout.

Der externe Wechsel ist inzwischen auch Alpha zu Beta bestanden: Der jeweils
alte aktive GNU-Kanal wird widerrufen, sein ursprünglicher Hintergrundprozess
läuft weiter. Beide Richtungen sind gebunden in
`out/phase1-dod/209278de/reciprocal-external-switch-proof.json`, SHA-256
`2c6bd123b9b3eff2185562d756644513c022c9701b6f6ccba7a7b9a871982f66`.
Die gesamte T06-/T08-Abnahme bleibt bis zu ihren weiteren Pflichtvarianten offen.

## Bildschirmsperre mit fortlaufender Hintergrundarbeit bestätigt

Der normale Android-Power-Eingang sperrt Betas Bildschirm und widerruft sein
offenes GNU-Terminal. Unabhängige Android-Beobachtung bestätigt `Asleep`,
sicheren angezeigten Keyguard, gesperrte Eingabe und `SCREEN_STATE_OFF` für
Benutzer 11. Das Terminal ist anschließend unauthentifiziert. CE bleibt
ausdrücklich `[0, 10, 11]`; dies ist kein Logout.

Alphas Prozess `10172/748802` und Betas Prozess `12013/791922` behalten ihre
Identität und schreiten bei weiterhin bestätigtem Schlafzustand fort. Boot-ID,
SystemServer und SELinux bleiben unverändert. Belege:

- `out/phase1-dod/209278de/screen-lock-observation/result.json`, SHA-256
  `24d1843bf3f7bb9545308b1aeda62afe61d292c6db2d865789f6923d95527998`,
  mit gehashten Power-/Keyguard-Ausgaben.
- `out/phase1-dod/209278de/screen-lock-background-proof.json`, SHA-256
  `9b100ec38351394a93667898655c346f82eef3b313af1f7004a3cdc02af0f283`,
  mit Terminal-, CE- und beiden Prozessbeobachtungen.

Der Bildschirm ist danach regulär wieder eingeschaltet. Frische Anmeldung und
ausdrücklicher Logout werden als eigene Folgeschritte geprüft.

## Beide persönlichen Benutzer regulär abgemeldet

Beta meldet sich nach der Bildschirmsperre mit frischer AOSP-Prüfung an und
anschließend ausdrücklich ab, ohne vorgeschalteten Runtime-Stopp. Sein
ursprünglicher Prozess `12013/791922` und Kontext sind danach entfernt, CE ist
`[0, 10]`, und seine zuvor geschriebene Datei liefert keine Bytes. Alphas
ursprünglicher Prozess `10172/748802` läuft währenddessen weiter.
Beleg: `out/phase1-dod/209278de/beta-logout-proof.json`, SHA-256
`deba29150aa6e9f3f9e244d4e9ef0cbb7f5beabf1ac76a059d618fe3a5cbaa88`.

Nach frischer Alpha-Anmeldung liest die gewöhnliche GNU-Shell dessen
ursprüngliche Datei und beide persistenten Konfigurations-/Testdateien mit
unveränderten Bytes. Auch Alpha meldet sich anschließend ohne vorherigen
Runtime-Stopp ab. Sein ursprünglicher Prozess und Kontext sind entfernt,
CE ist `[0]`, und der Zugriff auf die bekannte persönliche Datei liefert keine
Bytes. Beide Prozessbeobachtungen liegen vor dem natürlichen Ablauf der
begrenzten Hintergrundproben.
Beleg: `out/phase1-dod/209278de/alpha-logout-proof.json`, SHA-256
`1947ad2d2c8f912e579d747ab4d39acd1b87f19a8e669fc6ad940c425a00578e`.

Dies belegt die regulären Abmeldungen dieses Referenzablaufs. Der gemeinsame
Neustart von Android und KeyMint-Hilfssystem, anschließendes Lesen der
ursprünglichen Daten und weitere T10-/T11-Pflichtvarianten bleiben offen.

Der abgeschlossene Prüfpunkt vor dem Neustart bestätigt zusätzlich ausschließlich
Systembenutzer 0 als gestartet und leere persönliche Runtime-Cgroups. Er bindet
Profil-ID, Image-Commit, bisherigen Boot und ursprüngliche Dateihashes:
`out/phase1-dod/209278de/identity-test/reboot-checkpoint.json`, SHA-256
`40df7073677cda4cf824cec36ecbab9fe5aaa193e254664a140517b5eb2e684a`.
Der Passworttreiber bleibt für die anschließende Anmeldung am Leben.

## Profilpaar sauber beendet und erneut gestartet

Android endet regulär mit `reboot: Power down`; anschließend hängt der Helfer
seinen bestehenden Zustand aus und meldet `AEGIS_HELPER_SHUTDOWN_CLEAN`.
Der gemeinsame Launcher endet mit Code 0. Beleg:
`out/phase1-dod/209278de/paired-shutdown-1.json`, SHA-256
`a1ddc71361415466e697cd6e363b2a36ab42ae84e58f5b6b4435e3d3c7657ed4`.

Die vollständigen Logs dieses ersten Referenzboots enthalten keine beobachteten
fatalen Ausnahmen, fatalen Signale, ANRs, FORTIFY-Abbrüche oder Watchdog-Kills.
Genau ein SystemServer-Start ist protokolliert; unmittelbar vor dem Shutdown
besitzt er weiterhin PID/Startzeit `1368/37230`. Die vor dem Shutdown erfassten
Init-Rückgabewerte entsprechen den bereits begründeten Einmal-/Stopfällen.
Für alle 79 zusätzlichen nichtnull Dienstabschlüsse während des Shutdowns
ist ein vorheriges explizites Stoppsignal an denselben Dienst und dieselbe PID
im Protokoll zugeordnet. Diese Abschlüsse werden nicht ausgeblendet.
Beleg: `out/phase1-dod/209278de/first-reference-boot-health.json`, SHA-256
`b41f371a1c0b92b80eace46ba06021dff01005cbb671f2e4f8ded5bbaebd9553`.

Der zweite Start verwendet dasselbe Profil ohne Neuanlage. Manifest-Hash,
Profil-ID und beide Disk-Inodes stimmen überein; die Boot-ID ist neu:
`2816650c-96bf-4db0-84e6-f169f9dfada9`. ADB authentifiziert sich mit dem
bestehenden Schlüssel. SELinux ist Enforcing, der AVB-Digest unverändert.
Vor persönlicher Anmeldung sind nur Systembenutzer 0 gestartet, CE `[0]` und
keine persönlichen Runtime-Kontexte vorhanden. Beide ursprünglichen
GNU-Dateien und Alphas private Paketmetadaten liefern keine Bytes.
Beleg: `out/phase1-dod/209278de/postboot-locked-baseline.json`, SHA-256
`38868bfb264e865835c1a375c92c6377ef17744a9a1a782e21c31f7b09fc0553`.

Die ersten korrekten persönlichen Anmeldungen und Originaldaten-/Paketprüfungen
nach diesem Boot sind eigene Folgeschritte; sie werden durch den erfolgreichen
Boot allein nicht vorweggenommen.

## Erste Anmeldungen, Originaldaten und Paketversionen nach Reboot erhalten

Alpha und Beta melden sich jeweils beim ersten korrekten Versuch nach dem
zweiten Boot erfolgreich an, ohne vorgeschalteten Fehlversuch. Die
Vorbereitungsbeobachtung bestätigt jeweils weiterhin gesperrtes Ziel-CE und
keinen Kontext vor der Passwortübermittlung. Verzögerte Statusprüfung und
anschließende tatsächliche GNU-Ausführung bestehen für beide Benutzer.

Die ursprünglichen 1024-Byte-Dateien sowie beide persönlichen Konfigurations-
und Testdateien stimmen bytegleich mit den vor dem Reboot geschriebenen Proben
überein. Die alten `/tmp`-/`/run`-Dateien und POSIX-Mqueues fehlen in beiden
neuen Kontexten. Alpha führt privates jq/libjq1 `1.7.1-6+deb13u4` aus, Beta
gemeinsames `1.7.1-6+deb13u3`; beide berechnen tatsächlich `6` aus `[1,2,3]`.
Bibliotheksauflösung, Paketversionen und die ursprünglichen Binary-/Library-
Prüfsummen stimmen. Beide CLI-Statusausgaben melden `packages=current`.

Gemeinsame und private Auswahlmetadaten und die tatsächlichen Root-Backings
sind unverändert. Die vollständigen Paketdatensätze bleiben für beide Benutzer
gleich, jeweils 81 installierte Pakete. Beim ersten Rohbytevergleich bestand
eine einzelne zusätzliche abschließende Newline gegenüber dem früheren
lokalen Beleg; dieser Beobachtungsfehler und die Rohdaten sind erhalten.
Der erfolgreiche Vergleich erlaubt nur diese einzelne Endzeile und verlangt
ansonsten identische Bytes. Produktdaten oder Berechtigungen wurden nicht
geändert. SystemServer bleibt während dieser Prüfung PID/Startzeit `1159/20411`.

Beleg: `out/phase1-dod/209278de/paired-reboot-readback-v2/result.json`, SHA-256
`5f527250f03c94b3677224987d110dd9c8a888f4bb90c79d943db6909db19ab1`.
Er verknüpft den vollständigen Shutdown, die Sperrbeobachtung vor Anmeldung,
die ursprünglichen Dateihashes und tatsächliche Ausgaben aus beiden GNU-Shells.
Für weitere Lebenszyklusfälle sind erst nach diesen Readbacks neue begrenzte
Hintergrundproben angelegt. Sie behaupten kein Prozessüberleben über den Reboot.

Dies schließt den regulären gepaarten Neustart des aktuellen Referenzlaufs.
Passwortwechsel, nachträglicher Benutzer, Löschung/ID-Wiederverwendung sowie
die ergänzenden Paket-, Fehler- und Lebenszyklusvarianten bleiben offen.
Der neue Ergebnisindex führt 71 einzeln bezeichnete Varianten und grenzt
bestandene Teilfälle ausdrücklich von der noch offenen Gesamtfreigabe ab.

## Runtime-Stopp beider Benutzer mit aktivem Gegenbenutzer bestätigt

Beta und Alpha führen jeweils `linux stop` aus, während der andere Benutzer
seinen positiv beobachteten ursprünglichen Hintergrundprozess behält. Eigener
Prozess und Kontext werden entfernt; AOSP-Sitzung, Vordergrundidentität und
CE-Liste `[0, 10, 11]` bleiben erhalten. Der Peer behält jeweils PID, Startzeit
und Namespaces und macht vor/nach dem Stopp weiter Fortschritt. Das ist ein
Runtime-Stopp, ausdrücklich kein Logout oder CE-Schlüsselentzug.

Vor jedem Stopp werden nur die zuvor als leer bestätigten flüchtigen Proben
in `/tmp` und `/run` sowie persönliche POSIX-Mqueues neu angelegt. Persistente
Originaldateien werden nicht überschrieben. Nach erneutem Runtime-Start fehlen
diese flüchtigen Proben und Queues, während die ursprünglichen Dateien und
Konfigurationsbytes unverändert lesbar sind. Beta führt weiterhin jq/libjq1
`u3` aus, Alpha seine private Variante `u4`. Gemeinsame/private Auswahlmetadaten
sind unverändert. Neue Hintergrundproben werden erst nach bestätigtem Ende
der alten Prozesse und erfolgreichem Originaldaten-Readback angelegt.

Beleg: `out/phase1-dod/209278de/reciprocal-runtime-stop-proof.json`, SHA-256
`e6e3763b9ffc5b4b72fd0ccfc8431dd02e956d81d53908d580d8cf1495ba8c85`.
Boot-ID und SystemServer `1159/20411` bleiben unverändert, SELinux Enforcing.
Der nachträgliche Benutzer und AOSPs eigener Ressourcenstopp sind separate
Pflichtfälle; ihr anschließender Teilstand ist unten festgehalten.

## Konfigurations-Hashprüfung im Testwerkzeug präzisiert

Die bisherige private Konfigurationsprüfung verglich den Inhalt per Shell-
Substitution und gab SHA-256 aus, kontrollierte die ausgegebenen Hashzeilen
jedoch nicht automatisch. Damit konnte sie zusätzliche abschließende Newlines
allein über den Shell-Vergleich nicht zuverlässig erkennen. Commit `1ee00e1`
verlangt nun pro geprüftem Pfad genau eine tatsächliche, passende SHA-256-Zeile.

Die geänderte Funktion wurde gegen alle elf bis zum Korrekturzeitpunkt
vorliegenden Originalausgaben ausgeführt: Alle Hashes stimmen. Zusätzliche
Daten-Newline, fehlende Hashzeile und doppelte Hashzeile werden für beide
Benutzer in veränderten Ausgabekopien abgewiesen. Beleg:
`out/phase1-dod/209278de/private-state-hash-verifier-check.json`, SHA-256
`ed64a678635800422ce3d7e00cb512a7acfa21bb2ca95858fa3f55680e1f0239`.

Der lebende Passworttreiber wurde nicht ersetzt und verwendet weiterhin seine
ursprünglich geladene Fassung. Die vollständigen Runtime-Stopp-Belege prüfen
deshalb zusätzlich die tatsächlichen Hashzeilen aus seinen Rohereignissen.
Das Produktimage und die persönlichen Daten bleiben unverändert.

## Nachträglicher Benutzer und AOSP-Ressourcenstopp: Teilnachweis

Gamma wurde über die CLI als normaler AOSP-Benutzer 12/12 angelegt und mit
seinem Passwort angemeldet. Sein GNU-Kontext meldet am 3. Oktober 2026 um
02:17:20 UTC `runtime=ready ce=unlocked`. Die tatsächliche Programmausführung
und der Paketbestand dieses neuen Benutzers sind damit noch nicht abgenommen.

AOSP stoppte beim Wechsel zu Gamma den Hintergrundbenutzer Beta wegen seines
Limits laufender Benutzer. Das AOSP-Protokoll nennt ausdrücklich
`Too many running users (4). Attempting to stop user 11`. Anschließend wurden
Betas ursprünglicher Prozess und Kontext entfernt und sein CE-Speicher
gesperrt. Die bekannte persönliche Originaldatei lieferte danach keine Bytes.
Es wurde hierfür kein zusätzlicher CLI-Logout oder Runtime-Stopp angefordert.

Eine vorzeitig ausgeführte Endzustandsprüfung schlug um 02:14:11 UTC fehl.
Der unabhängig beobachtete Benutzerabbau war damals noch nicht abgeschlossen;
die genaue fehlgeschlagene Assertion wurde nicht protokolliert. Dieser Fehler
bleibt erhalten. Die später bestätigte Sperrung ersetzt ihn nicht rückwirkend.
Die erneute Anmeldung und der Vergleich der ursprünglichen Daten nach diesem
Ressourcenstopp bleiben offen.

Beleg: `out/phase1-dod/209278de/beta-aosp-resource-stop-proof.json`, SHA-256
`1c65561ea6a2461714261f7fe0b709174c20e5c2acc4162c8ccb9a0e25da6ae4`.
Die vollständige Phase-1-Abnahme bleibt ausstehend.

## Gemeinsame Software beim nachträglich angelegten Gamma ausgeführt

Gamma öffnet seine GNU-Shell und erhält vor eigenen HOME-Schreibzugriffen
genau die zehn vorgesehenen Verzeichnisse mit UID/GID 1000 und Modus 0700.
Die tatsächliche Host-Zuordnung ist 1207500, getrennt von Alpha und Beta.
Die GNU-Basisprüfung bestätigt Werkzeuge, Mountlayout, schreibgeschützte Basis
und die vorgesehenen Prozessbeschränkungen.

Am 3. Oktober 2026 um 02:29:22 UTC führt Gamma jq erfolgreich aus. jq/libjq1
haben die gemeinsame Version `1.7.1-6+deb13u3`, libonig5 `6.9.9-1+b1`.
Binary- und Bibliotheks-Hashes entsprechen der bereits nachgewiesenen
gemeinsamen Variante. Das zugrunde liegende Root-Image gehört zu derselben
Paketgeneration wie Betas gemeinsamer Kontext. Alphas private u4-Version wird
nicht übernommen. Gammas `.config` und `.local` sind leer; seine HOME-Struktur
enthält keine ursprünglichen A/B-Dateiproben.

Beleg: `out/phase1-dod/209278de/gamma-shared-program-proof.json`, SHA-256
`76b162a64cd6ea9962e9bda60d5f9412f6da79e927eda4c8ed08e9bffbf0f9a9`.
Dies belegt noch keine expliziten fremden Dateizugriffe aus Gamma. Beta war
dabei nach dem AOSP-Ressourcenstopp gesperrt; eine gleichzeitige Entsperrung
aller drei Benutzer wird nicht behauptet. T14.2 bleibt deshalb teilbelegt.

## Wiederanmeldung nach Betas AOSP-Ressourcenstopp bestätigt

Nach Gammas regulärem Logout bestätigt AOSP CE `[0, 10]`. Beta meldet sich
mit seinem ursprünglichen Passwort erneut an. Vor dessen Übermittlung bleiben
CE gesperrt und der Runtime-Kontext abwesend. Nach erfolgreicher Anmeldung
bestätigt die verzögerte Statusabfrage die Sitzung; der neue GNU-Kontext startet.

Die ursprüngliche 1024-Byte-Datei sowie beide persistenten Konfigurationsproben
stimmen mit ihren ursprünglichen SHA-256-Werten überein. Die Hashzeilen wurden
im Belegsammler ausdrücklich verglichen, zusätzlich zur Prüfung des laufenden
älteren Treibers. jq/libjq1 u3 wird tatsächlich ausgeführt und seine Bibliothek
hat weiterhin den ursprünglichen Hash. Gemeinsame und Alpha-private
Paketgeneration bleiben unverändert.

Alphas Hintergrundprozess behält PID 10354, Startzeit 288155, Host-UID und
Namespaces; sein Zähler schreitet weiter fort. Boot-ID und SystemServer
1159/20411 bleiben ebenfalls erhalten. Betas alte temporäre Dateien und
Mqueues fehlen weiterhin; sie waren schon beim vorausgegangenen Runtime-Stopp
entfernt worden. Dieser Nachweis behauptet deshalb keinen zusätzlichen
IPC-Abbauversuch während des AOSP-Stoppfalls.

Beleg: `out/phase1-dod/209278de/beta-aosp-resource-recovery-proof.json`, SHA-256
`4283d6658f6ca6e6b94cd0422caeed23b8b9931d45ec117e1e364728ec0f1d82`.
Er bindet den ursprünglichen Stoppbeleg und dessen fehlgeschlagene verfrühte
Assertion ein. T08.3 ist für diesen beobachteten AOSP-Limitfall nachgewiesen;
die vollständige Phase-1-Abnahme bleibt offen.

## Passwortwechsel mit erneutem Login und unveränderten Originaldateien

Beta ändert am 3. Oktober 2026 um 02:40:33 UTC sein Passwort über `aegis passwd`.
Die CLI bestätigt den AOSP-Passwortwechsel. Nach regulärem Logout ist CE wieder
gesperrt. Ein einzelner Anmeldeversuch mit dem bisherigen Passwort wird von
AOSP abgewiesen; CE bleibt gesperrt, der persönliche Kontext fehlt, und
`linux start` verlangt eine erneute Anmeldung. Das neue Passwort erlaubt
anschließend eine bestätigte Sitzung und einen neuen GNU-Kontext.

Die ursprüngliche 1024-Byte-Datei und beide persistenten Konfigurationsproben
werden aus Betas GNU-Shell gelesen und behalten ihre ursprünglichen SHA-256-
Werte. Inode, Größe, mtime, ctime, Eigentümer und Modus sind gegenüber der
Beobachtung unmittelbar vor dem Passwortwechsel identisch. Die Dateien wurden
nicht neu erzeugt. Boot-ID und SystemServer 1159/20411 bleiben unverändert.

Beleg: `out/phase1-dod/209278de/beta-password-change-proof.json`, SHA-256
`551d7d9f268a4a3f3c05aed474e5f949f50fe4b25320334b2f4edb61ba54eac1`.
Er bindet die Dateimetadaten vor dem Wechsel, die unabhängige Beobachtung nach
der Passwortablehnung und die ergänzende Quellprüfung ein.

Die gelesenen AOSP-Methoden `setLockCredentialInternal` und
`setLockCredentialWithSpLocked` sind gegenüber dem ausgecheckten AOSP-Basisstand
unverändert. Sie verwenden das bestehende Synthetic Password beim Erstellen
des neuen Passwort-Protectors weiter. Der AEGIS-Adapter ist bytegleich mit dem
getesteten Image-Commit und delegiert an `setLockCredential`. Dies erklärt die
Architektur ohne vollständige Dateineuverschlüsselung; die Metadatenprüfung
allein wäre kein Nachweis sämtlicher kryptographischer Interna. Es wurden
keine geheimen Schlüssel ausgelesen.

T02.1 ist damit belegt. T02.2 bleibt offen, bis der vollständige gepaarte
VM-Neustart mit dem geänderten Passwort und erneutem Datenvergleich erfolgt ist.

## Geändertes Passwort über zweiten gepaarten Neustart bestätigt

Nach regulärer Abmeldung sämtlicher persönlicher Benutzer werden Android und
KeyMint-Helfer geordnet beendet. Android bestätigt Power-down; der Helfer
bestätigt das Aushängen seines persistenten Zustands und ebenfalls Power-down.
Boot 3 startet mit demselben Profilpaar und unverändertem Profilmanifest sowie
denselben Datenträger-Inodes. Die neue Boot-ID lautet
`e720d2bf-faef-4af8-87b9-709112eb3c41`; SystemServer ist 1177/20364.

Vor persönlicher Anmeldung sind nur Benutzer 0 und CE `[0]` aktiv, sämtliche
persönlichen Kontexte fehlen und bekannte Originaldateien liefern keine Bytes.
Betas erster Versuch verwendet direkt das geänderte Passwort und gelingt.
Nach verzögerter Sitzungsprüfung und GNU-Start sind Originaldatei und beide
Konfigurationsproben bytegleich erhalten. Inode, Größe, mtime, ctime, Eigentümer
und Modus entsprechen weiterhin dem Stand vor dem Passwortwechsel. Die
gemeinsame jq/libjq1-u3-Version wird tatsächlich ausgeführt.

Beta meldet sich anschließend regulär ab. Das alte Passwort wird auch in
diesem neuen Boot abgewiesen; CE bleibt `[0]`, ein Runtime-Start wird verweigert,
kein persönlicher Kontext ist vorhanden und die bekannte Datei liefert keine
Bytes. Der erfolgreiche erste Zugang wurde nicht durch einen Fehlversuch
vorbereitet. Die ursprünglichen Dateien wurden nicht neu erzeugt.

Beleg: `out/phase1-dod/209278de/beta-password-reboot-proof.json`, SHA-256
`31db67c68b1fcbbc1bd64480edb2e53abea325290c8aa5fef0dccd2d3b167964`.
Er bindet Shutdown, gesperrten Bootzustand, Dateimetadaten und Quellprüfung ein.
Ein anfänglicher Fehler des Host-Beobachters ist erhalten: Er erwartete eine
nicht vorhandene Zeile statt des tatsächlichen `dumpsys activity users`-Formats.
Die korrigierte Prüfung verlangt weiterhin exakt Benutzer 0 und `[0]` als
gestartete Benutzer. Es gab vor dieser Korrektur keine persönliche Anmeldung.

T02.2 ist damit nachgewiesen. Alphas erneuter Originaldaten-/Privatpaketvergleich
in Boot 3 und die übrigen Pflichtvarianten bleiben gesonderte Arbeiten.

## Boot-2-Gesundheit und zusätzlicher Migrations-Aufräumstatus

Die vollständige Logkopie unmittelbar vor dem zweiten Shutdown enthält keine
Fatal-, ANR-, FORTIFY- oder Watchdog-Abbruchmeldungen und genau einen
SystemServer-Start. HIDL-Allocator-PID 905 erhielt Signal 9 nach seiner expliziten
`hidl_memory.disabled=true`-Stopregel; das ist als regulärer Abbau zugeordnet.

Zusätzlich endet ein einmaliger VirtualizationService-Aufräumbefehl mit Status 1.
Der identische installierte und AOSP-Quellabschnitt beschreibt eine Migration:
alte Dateien der früheren Dienst-UID entfernen, danach ein `system`-eigenes
Verzeichnis erstellen. Das beobachtete Verzeichnis und sein Sticky-Elternordner
gehören bereits `system`, während der Befehl unter der alten UID 1081 läuft.
Dieser Besitzkonflikt erklärt eine fehlende Löschberechtigung als Quell-/Zustands-
inferenz. Der konkrete fehlschlagende Dateipfad und errno wurden nicht erfasst.
Boot 3 zeigt denselben einmaligen Status. Es wurde weder erneut aufgeräumt noch
eine Berechtigung geändert; erfolgreicher Cleanup oder eine Fehlerbehebung wird
nicht behauptet. Der Status bleibt im D1-Audit sichtbar.

Belege: `boot2-pre-shutdown-health.json`, SHA-256
`580f1d6e32b6507fb56803f7dd38b4e43d9746773bb96ab8f51be839058035a1`, und
`boot-migration-cleanup-classification.json`, SHA-256
`c798664aeeebb93514bc66de8927ea754d9dede938598f6792618b758dcce877`,
jeweils unter `out/phase1-dod/209278de/`. D1 und die Gesamtfreigabe bleiben offen.

## Alpha-private Version und beide Originaldatensätze auch in Boot 3 erhalten

Nach Betas abgeschlossenem Passwort-Neustartnachweis meldet sich Alpha korrekt
an. Seine ursprüngliche 1024-Byte-Datei und beide persistenten Konfigurationsproben
behalten ihre bisherigen SHA-256-Werte; alte `/tmp`-/`/run`-Proben fehlen.
jq/libjq1 `1.7.1-6+deb13u4` wird tatsächlich ausgeführt und seine Bibliothek
behält den ursprünglichen privaten Hash. Gemeinsame und Alpha-private
Paketgeneration entsprechen weiterhin dem Ausgangsbestand. Boot-ID und
SystemServer 1177/20364 bleiben während dieser Prüfung unverändert.

Beleg: `out/phase1-dod/209278de/alpha-second-reboot-readback-proof.json`, SHA-256
`b3b4b958c2ff1f32d057290627e3ebd76dd9f420df00b3fb775510cb0873b212`.
Er bindet Betas separaten Neustartbeleg mit ursprünglichen Daten und gemeinsamer
u3-Version ein. Die Benutzer wurden in diesem Boot nacheinander geprüft;
gleichzeitige Entsperrung wird damit nicht behauptet. Paketautorisierung,
gemeinsame Aktualisierung bei privaten Versionen, Entfernung, Parallelität und
die weiteren offenen Lebenszyklusvarianten bleiben erforderlich.

## Gemeinsamer Updateplan ohne Freigabe abgebrochen

Vor dem geplanten gemeinsamen Update laufen Alpha und Beta mit ihren
ursprünglichen 81-Paket-Beständen: Alpha privat jq/libjq1 u4, Beta gemeinsam u3.
Der tatsächliche Updateplan enthält jq/libjq1 u4 sowie neuere Revisionen von
PCRE2, libssl3t64 und openssl-provider-legacy. Die erste Freigabeabfrage wird
mit leerer Adminauswahl abgebrochen; weder Adminname noch Passwort werden
übermittelt. Die CLI bestätigt keine erfolgreiche Veröffentlichung.

Die unabhängige Aufnahme danach ist für beide Kontexte exakt gleich zur
Ausgangslage: Runtime-PID/Startzeit, Namespaces, Root-Mount und Image, gesamte
Paketliste, Hash der dpkg-Datenbank sowie Alphas private Versionsfestlegung.
Gemeinsame/private Auswahlmetadaten, CE `[0, 10, 11]`, Vordergrund und
SystemServer-Identität bleiben unverändert.

Beleg: `out/phase1-dod/209278de/update-all-cancel-proof.json`, SHA-256
`45770e57849e8820b745bc47b06f7a9ad3abaefad14acce93921013e1c91fcc8`.
Die falsche, die Nicht-Admin- und die erlaubte Freigabe sind damit noch nicht
geprüft; ebenso wenig ein Abbruch während der Ausführung.

Eine separate CLI-Schwäche bleibt sichtbar: `linux package status` verlangt
nach dem Abbruch eine erneute Anmeldung, obwohl das allgemeine `status`
Alpha weiterhin als authentifiziert bestätigt. Die Quellprüfung zeigt, dass
die Paketstatusabfrage den bereits gesperrten Auftrag ablehnt. Der Hinweis
auf erneute Anmeldung ist daher irreführend; ein tatsächlicher Sitzungsverlust
oder eine Fristüberschreitung wird nicht behauptet. Die Formulierung ist noch
nicht korrigiert und darf nicht als erledigt gelten.

## Gewöhnlicher privater Versionsrückgang: APT-Fehler reproduziert

Ein separater frischer QEMU-Gast auf Image `209278de` führt den Test
`ReviewedArchiveDowngradePreservesPrivateChoiceConfigurationAndDependency`
aus Commit `065c6e8` aus. Nach erfolgreicher Installation der synthetischen
App/Bibliothek in Version 2 scheitert der geprüfte gewöhnliche Archivplan
für Version 1 bereits in der Simulation mit APT-Status 100:
`Packages were downgraded and -y was used without --allow-downgrades.`
Damit ist die bisherige Quellhypothese am unveränderten Executor reproduziert.
Der ursprüngliche CLI-Fehlerbeleg E36 bleibt separat erhalten; dessen bereinigte
Kandidatenlogs wurden nicht nachträglich wiedergewonnen.

Der Fehlerbeleg liegt lokal in
`out/phase1-dod/209278de/reviewed-downgrade-before-fix/result.json`, SHA-256
`9280d539c42b8683bde43c233c71dcaf99a1fd81bc6383dd11a609a91ac016c6`.
Testausgabe und fehlgeschlagene synthetische Testumgebung bleiben erhalten.
Alle zehn Produkt-Hilfsprogramme sind bytegleich zum bisherigen Image;
Boot, SystemServer, Enforcing und ausschließlich System-CE bleiben stabil.

Die Korrektur setzt APTs explizite Zustimmung zum Versionsrückgang für jeden
vorhandenen geprüften Plan, einschließlich gewöhnlicher Archivinstallation.
Die Simulation muss weiterhin exakt den freigegebenen Vorher-/Nachher-Versionen
entsprechen, bevor Paketskripte ausgeführt werden. Ungebundene Entwicklertests
erhalten diese Zustimmung nicht. Build, erfolgreicher Regressionstest und
der echte CLI-Ablauf auf einem passenden Produktimage stehen noch aus.

### Korrekturimage f098f43 lokal gebaut und vorbereitet

Der vollständige lokale Build des Commits
`f098f439f051c34e92fb1be0b4d908cc542358ea` ist erfolgreich abgeschlossen:
`/srv/aegis/runs/local-20261003T055438Z-f098f439-lvp0t1`.
Der erste Versuch hielt vor der Kompilierung wegen zu wenig verfügbarem RAM an.
Nach geordnetem Stoppen des abgeschlossenen Regressionstest-Profilpaars
bestand dieselbe unveränderte Speicherprüfung mit 51,5 GiB verfügbar.
Die fehlgeschlagene synthetische Testumgebung und ihr Profil bleiben erhalten.

Die neue Vorbereitung `/srv/aegis/runs/phase1-f098f439` verifiziert alle
20 Imageprüfsummen, die festgehaltenen Kernel-/Runtime-Eingaben, die
AVB-Metadaten und eine frisch erzeugte QEMU-Basisdisk. Ihr Beleg
`build-validation.json` hat SHA-256
`42f9c6767390410c3ad3e3ce304bf32659e991f4182b75795941f6ddc77a38b5`;
der vbmeta-Digest ist
`291686e8e3bbbc92e7e0b3ea215e78701b3447d880aebc31ebc7c211ab7e30c5`.
Das ist eine Build-/Vorbereitungsprüfung, noch kein erfolgreicher Gasttest.

Die passend kompilierten nativen Komponenten liegen lokal unter
`out/phase1-dod/f098f439/`. Gegenüber dem roten Regressionstest ändern sich
ausschließlich `aegis-package-execute` und dessen Testvariante; das native
Testprogramm und das Java-Test-APK sind bytegleich. Für den nächsten Lauf
sind der neue Regressionstest sowie fünf vorhandene Kontrollen für gewöhnliche
Installation/Aktualisierung/Entfernung, Konfigurationserhalt, abweichende
Simulation, Abgleich und private Entfernung ausgewählt. Der frische Gaststart
läuft; keiner dieser Wiederholungstests wird bereits als bestanden gewertet.
Buildartefakte bleiben ausschließlich lokal.

### Wiederholung der Paketkorrektur und Bedienung auf f098f43 bestanden

Der frische Gast bootet mit Enforcing, authentifiziertem ADB, passendem
AVB-Digest und ausschließlich System-CE. Alle sechs ausgewählten nativen Tests
bestehen auf dem passenden vollständigen Image. Besonders der mit unveränderten
Testbytes ausgeführte private Versionsrückgang, der vorher APT-Status 100
lieferte, erhält nun App/Bibliothek in der angeforderten älteren Version sowie
Konfiguration, private Auswahl und Abhängigkeitsmarken. Der Kontrollfall mit
abweichender Simulation verweigert weiterhin die Ausführung vor Paketskripten.

Beleg: `out/phase1-dod/f098f439/native-downgrade-controls/result.json`, SHA-256
`e004cf92be2c70defd8e90998ec3bb1e0c2abb83a28a08c290b7d000edfd55e8`.
Boot-ID und SystemServer-Identität bleiben vor/nach den Tests unverändert.
Bildschirm, normale QMP-Tastatur-/Mausnavigation und ein binärer ADB-Rundlauf
sind ebenfalls auf diesem Stand bestätigt und im
[Ergebnisindex](phase-1-result-index.md#zusätzlicher-korrekturstand-f098f43)
mit Grenzen und Prüfsummen erfasst. Der persönliche CLI-Lauf muss die Korrektur
und die vollständigen Pflichtfälle anschließend auf diesem Image nachweisen.

### Erster persönlicher CLI-Ablauf auf f098f43

Auf dem Korrekturimage wurden Alpha (ID/Serial 10/10, Administrator) und
Beta (11/11, Standardbenutzer) über die CLI angelegt. Die Anlage und die
bloße Auswahl des Anmeldeziels entsperrten dessen CE nicht. Alphas erste
Anmeldung blieb anschließend stabil; der persönliche GNU-Prozess führte
Befehle erfolgreich aus. Die zehn anfänglichen Home-Verzeichnisse besitzen
jeweils UID/GID 1000 und Modus 0700. Debian-Werkzeuge, schreibgeschützte
Softwarebasis, Namespaces und Prozessbeschränkungen sind für Alpha geprüft.

Ein tatsächlicher GNU-Prozess schrieb die 1024-Byte-Prüfdatei. Derselbe
Hintergrundprozess (Host-PID 7316, Startzeit 275842) lief mit fortschreitenden
Zählern vor und nach dem Shell-Ende weiter; die AEGIS-Anmeldung blieb gültig.
Nach Betas Anlage waren nur System und Alpha entsperrt (`[0,10]`). Das ist
noch kein Nachweis von Betas Runtime, gegenseitiger Isolation oder Persistenz.

Der lokale Beleg `out/phase1-dod/f098f439/initial-personal-flow.json`, SHA-256
`53ca89bcd135f6b9856448d89692eadae41d46edb370805d9029f38be4c5cdee`,
bindet die ersten 34 Ereignisse des weiterlaufenden Testtreibers über eine
definierte kanonische Prüfsumme. Der gesamte Ablauf bleibt unvollständig.

Ein neuer Statusbeobachter scheiterte zunächst an der legitimen
Paketarbeitsgruppe `p10-s10`, weil er ausschließlich Benutzergruppen `u…`
erwartete. Der Produktcode erzeugt beide Typen ausdrücklich. Die korrigierte
Auswertung erfasst Paketgruppen separat und erhält sämtliche Prüfungen der
Benutzerlaufzeiten. Unbekannte Gruppennamen werden weiterhin abgelehnt.
Die ursprüngliche Fassung und ihre Fehlerklassifikation bleiben lokal erhalten
(`observer-classification-error.json`, SHA-256
`375f7731fa693c4aae91895ec4c24444d2332a649e45838bbb94786d6f166e48`).

Der gemeinsame Installationsplan für `jq` und `libjq1` in Version
`1.7.1-6+deb13u3` sowie `libonig5` in Version `6.9.9-1+b1` wurde am
Adminprompt durch leere Eingabe regulär abgebrochen. Vorher/nachher sind
Laufzeitidentität, Paketdatenbank, ausgewählte Paketgenerationen, CE-Zustand,
Boot und SystemServer exakt gleich. Betas CE bleibt gesperrt. Die CLI meldet
hier ungenau einen fehlgeschlagenen Paketauftrag; eine erfolgreiche
Installation wird nicht behauptet.

Beleg: `shared-install-cancel-proof.json`, SHA-256
`e440ef7c69b981fd7b2a72c9e9608a485259554541820a96a78291f42a7959e1`.
Beide Momentaufnahmen erfolgten nach abgeschlossener Planung und zeigen keine
Paketarbeitsgruppe. Eine anfängliche zusätzliche Vergleichsannahme, vor dem
Abbruch müsse diese Gruppe noch vorhanden sein, schlug fehl und ist im Beleg
erhalten. Ein Entfernen der Gruppe durch den Abbruch ist damit nicht bewiesen.

### Gemeinsame Installation mit falscher Adminbestätigung abgewiesen

Ein erneut vollständig geplanter gemeinsamer jq-u3-Auftrag erreicht die
Adminabfrage. AOSP weist das absichtlich falsche Testpasswort ausdrücklich
zurück. Die CLI bestätigt anschließend weiterhin Alphas bestehende Anmeldung.
Vorher/nachher stimmen ausgewählte Generationen, sämtliche ursprünglichen
Runtime-Metadaten, Paketdatenbank-Bytes, Boot und CE `[0,10]` exakt überein.
Betas persönlicher Speicher bleibt gesperrt.

Beleg: `out/phase1-dod/f098f439/shared-install-wrong-proof.json`, SHA-256
`698f71f2a92aea4decaf6f196c084b5c19f604273be9cec5a4fefe90b5ce3534`.
Die zweite Beobachterfassung erfasst zusätzlich verbleibende
Konfigurationsdatensätze (hier leer) und prüft die ausgewählten Generationen
nochmals am Ende; alle bereits vorhandenen Felder werden exakt verglichen.

Der daraus abgeleitete versionierte Beobachter
`scripts/qemu-package-state.py` und seine
[Aufrufanleitung](runtime-gnu-test-driver.md#paketbestand-vor-und-nach-einer-aktion-beobachten)
binden zukünftige Aufnahmen an Image, Profil und Boot. Die Nachaufnahme mit
dieser Fassung bestand im tatsächlichen Gast; Syntax und Hilfetext sind geprüft.
Sie ersetzt keine Programmausführung oder Zwei-Benutzer-Isolationsprüfung.

Für `install`, `update` und `remove` wurden außerdem jeweils die fehlende
Bereichsangabe und eine unzulässige `--owner 11`-Option über die echte CLI
geprüft. Alle sechs Befehle werden vor einer Planfreigabe abgewiesen;
anschließend stimmen Paketbestand, Kontext, ausgewählte Generationen und CE
exakt mit der vorherigen Aufnahme desselben Beobachters überein.
Beleg: `package-argument-denials-proof.json`, SHA-256
`2551b8fb3f5ef23a35e2ecac4b934bafa06c48df785378baa26947c3401e6939`.
Dies belegt diese CLI-Eingabevarianten von T13.7; beliebige direkte
Binder-Aufrufer sind dadurch nicht geprüft.

Auch Betas korrekt eingegebene Nicht-Adminbestätigung für einen frisch
geplanten gemeinsamen Installationsauftrag wird abgewiesen. Der Vergleich
umfasst Planung und Ablehnung: Paketdatenbank, ausgewählte Generationen,
ursprüngliche Runtime, Benutzer, CE `[0,10]` und Systemidentität bleiben gleich.
Eine anschließende direkte Statusabfrage bestätigt Alphas weiterhin gültige
Anmeldung. Der generische CLI-Hinweis auf eine erneute Anmeldung ist hier
irreführend und bleibt als Bedienungsfehler offen.
Beleg: `shared-install-nonadmin-proof.json`, SHA-256
`c24d2162f05fb0f1ca92944c57f9b9b89a0624e252b51a0f37f1aec33909802f`.

### Bootmeldung des optionalen Tombstone-Exports eingeordnet

Im aktuellen Boot protokolliert `tombstone_transmit` die Meldung
`Port flag is required` mit Logschwere F. Der unveränderte Cuttlefish-Quellstand
`c6a8b05c38d88e8d19b83fd8d47f75c0686f2e69` verwendet an dieser Stelle ausdrücklich
`FATAL_WITHOUT_ABORT` und danach eine Schlafschleife. Im Gast ist die optionale
Portproperty leer; der ursprüngliche Prozess 1186 läuft mit Zustand S und
Startzeit 33285 weiter. Diese konkrete Meldung wird daher als unkonfigurierter
optionaler Host-Diagnoseexport eingeordnet, nicht als nachgewiesener Absturz.

Der lokale Beleg `tombstone-export-observation.json`, SHA-256
`a84b7ae49e2bcda4aaf4cce99af7acd410ea108e66eb4ec679f24e57a4b91070`,
enthält Quellprüfsumme, Vergleich mit dem Quellcommit, Logpräfixbindung und
Gastzustand. Das ist eine begrenzte Quell-/Zustandsinferenz; die vollständige
D1-Dienstprüfung bleibt offen. Es wurde kein Diagnoseabsturz ausgelöst und
keine Dienstkonfiguration verändert.

### Gemeinsame Installation und Aktivierung auf f098f43 nachgewiesen

Nach den drei verweigerten Freigabevarianten erlaubt Alphas frische gültige
AOSP-Adminbestätigung den vollständig angezeigten gemeinsamen jq-u3-Plan.
Die veröffentlichte Generation lautet
`13629e83de8896bf52e5695a071abd67b4232300791fa0f6c3ed484c19f9007e`.
Alphas laufender Kontext behält zunächst exakt seine bisherigen 78 Pakete;
der ursprüngliche Hintergrundprozess 7316/275842 schreitet weiter fort.
Die CLI meldet korrekt `activation-pending`.

Ein regulärer `linux stop` entfernt Kontext und ursprünglichen Prozess bei
weiterhin gültiger Anmeldung und CE `[0,10]`. Dabei wurde irrtümlich die
Logoutprüfung `bg-gone-a` aufgerufen; deren zusätzliche Forderung nach
gesperrtem CE scheitert erwartungsgemäß. Dieser Prüfbedienfehler bleibt im
Ereignisprotokoll. Ein separater Beleg prüft ausdrücklich die vorgesehenen
Runtime-Stoppzustände, ohne Logout zu behaupten:
`shared-activation-runtime-stop.json`, SHA-256
`40a28a7437e23934a1346290f774631edbb45cdd19f5b3daae24f01826bd282c`.

Nach `linux start` zeigt der Status `packages=current`. Der neue Root-Mount
gehört genau zur veröffentlichten Generation. Die vollständige Datenbank
enthält die bisherigen 78 Pakete plus `jq`/`libjq1` jeweils
`1.7.1-6+deb13u3` und `libonig5` `6.9.9-1+b1`. Die normale GNU-Shell mit
UID 1000 führt jq tatsächlich aus, berechnet `6` aus `[1,2,3]` und löst die
erwartete Bibliothek auf. Originaldatei und persönliche Konfiguration bleiben
bytegleich; die ursprünglichen flüchtigen Dateien unter `/tmp` und `/run`
sind verschwunden. Beta bleibt gesperrt.

Der zusammengefasste lokale Beleg `shared-u3-activation-proof.json`, SHA-256
`3bee77ca545d0a6b67e612bed633b34239f6cce7c31860ba421a1cc15a2d8a8b`,
bindet den Ereignispräfix, die Vorher-/Nachher-Aufnahmen, den Stoppbeleg und
die tatsächlich installierte Debian-Quellenkonfiguration. Damit ist die
gemeinsame Installations-Freigabematrix dieses Laufs geschlossen; andere
Aktionen und Bereiche bleiben separat erforderlich. Die private u4-Version
für Alpha ist danach erst angefordert, noch nicht freigegeben oder installiert.

Die ergänzende begrenzte Boot-/Dienstbeobachtung bis zur gemeinsamen
Veröffentlichung enthält einen SystemServer-Start und keine Java-Fatal-,
Fatal-Signal-, ANR-, FORTIFY- oder Watchdog-Abbruchmeldungen. Die drei frühen
Einmalrückgaben sind erneut mit unveränderten Quellen und Gastzustand
abgeglichen; Signal-Exits passen zu den protokollierten Stop-/Restart-Vorgängen.
Beleg: `init-exit-classification.json`, SHA-256
`791bf54516d21d8e161d5b92596933792e1d76835ce73ae3db470eccae8b5314`.
Diese zeitlich begrenzte Beobachtung ersetzt keine spätere Lebenszyklusabnahme.

### Private u4-Version bei unveränderter gemeinsamer u3-Basis

Alphas gültige Adminfreigabe veröffentlicht ausschließlich für Antragsteller
10/10 die private Generation
`19ca0517a7f27e170328b52c64feb07edd5c507ef92ffde0c7beff3769fd316c`.
Die gemeinsame Auswahl bleibt unverändert. Vor der bewussten Aktivierung
besitzt Alphas laufender Kontext weiterhin exakt den gemeinsamen u3-Bestand;
der Status meldet `activation-pending`.

Nach regulärem Stopp/Start liegt der aktive Root unter Alphas CE-Paketstore.
Sein vollständiger 81-Paket-Bestand unterscheidet sich ausschließlich in
`jq` und `libjq1`, jeweils `1.7.1-6+deb13u4`. Die tatsächliche GNU-Ausführung
bestätigt Versionen, Bibliothek und Berechnung. Originaldatei und Einstellungen
bleiben bytegleich; alte flüchtige Dateien sind verschwunden. Das private
Auswahlregister hält ausdrücklich jq u4 fest. Beta bleibt gesperrt.

Beleg: `private-u4-activation-proof.json`, SHA-256
`b14ea49a66246e89b0418b44856509f5bbf5108fe32e4deeda7bc2fa3dd74474`.
Danach wird für die folgenden Wechsel-/Isolationsprüfungen eine neue begrenzte
Hintergrundprobe angelegt; das Ende der ursprünglichen Probe bleibt separat
belegt. Flüchtige Testdateien werden erst nach bestätigter Abwesenheit aus der
eigenen unveränderten synthetischen Konfiguration neu erzeugt.

Zusätzlich sind die gelesenen Paketfreigabequellen einschließlich der tatsächlich
eingebauten LockSettings-Methode gegen die Buildbelege geprüft:
`package-approval-source-binding.json`, SHA-256
`2d4bff84b59172abe80573c56ac16498e6ff13d6479ab484bbff3f84b9cb53a2`.
Die Bestätigung verwendet AOSPs vorhandenen Protector-Prüfpfad ohne die normalen
Benutzer-/CE-Entsperrcallbacks und gibt nur bereinigten Status zurück. Diese
Quellprüfung ergänzt die CE-Beobachtungen; sie ersetzt nicht die verbleibenden
Aktions-, Rollen- und Isolationsprüfungen. Der begrenzte Scan von drei lokalen
Laufprotokollen fand kein vollständiges Testpasswort; T01.5 bleibt insgesamt offen.

### Zwei aktive Benutzer mit privaten/gemeinsamen Versionen auf f098f43

Betas erster korrekter Login bleibt nach sechs Sekunden gültig und führt ohne
vorherigen Fehlversuch tatsächlich GNU-Befehle aus. Die bloße Zielauswahl hatte
vorher weder Betas CE entsperrt noch dessen Kontext angelegt. Danach sind
System, Alpha und Beta entsperrt; die beiden persönlichen Kontexte bestehen
gleichzeitig. Beta führt gemeinsames jq/libjq1 u3 mit passender Bibliothek aus,
während Alphas privater u4-Kontext unverändert bleibt.

Beide normalen GNU-Kontexte verwenden intern UID/GID 1000. Tatsächlich
beobachtete Host-UID/GID-Zuordnungen sind getrennt, ebenso User-, Mount-, PID-,
IPC-, UTS- und Netzwerk-Namespaces. Die normalen Hintergrundprozesse laufen
als Host-UID/GID 1007500 beziehungsweise 1107500. Alphas nach Aktivierung neu
angelegter Prozess 14559/720255 überlebt den Wechsel unverändert und schreitet
fort; Betas Prozess 16811/782059 schreitet ebenfalls fort. Betas ursprüngliche
1024-Byte-Datei hat SHA-256
`e32b6a6650212c74df22976a962bed97babc4fd30dd1fc393948ca1ebdf08175`;
seine getrennten Konfigurations-/flüchtigen Proben sind ebenfalls angelegt.

Beleg: `two-user-version-baseline-proof.json`, SHA-256
`e9069aa25986d82adf291cf05b5eb6f95877c9319e82a1fba8c205999b8e727a`.
Er bindet die tatsächlichen GNU-Ereignisse, beide vollständigen Paketbestände,
die GID-Beobachtung und die öffentliche technische Kontozuordnung ein.
Letztere ist zusätzlich in der [Identitätsgrundlage](identity-crypto-baseline.md)
mit dem Image-Quellstand verknüpft.

Diese gleichzeitige Trennung ist noch kein Nachweis verweigerter gegenseitiger
Zugriffe. Beta hat noch keinen privaten Paketstore. Die gegenseitigen Datei-/
Prozess-/IPC-Prüfungen sowie Logout, gepaarter Neustart und die übrige
Aktions-/Lebenszyklusmatrix bleiben auf diesem Image erforderlich.

### Gegenseitige Zugriffe und Bildschirmsperre auf f098f43

Am 3. Oktober 08:37–08:49 UTC wurden die gegenseitigen Zugriffe aus den
beiden tatsächlichen GNU-Kontexten geprüft. Bekannte Dateien des anderen
Benutzers liefern keine Bytes; dessen Konfiguration, synthetische Test-Secrets
und flüchtige Dateien sind weder lesbar noch veränderbar. Ein unabhängiger
Beobachter bestätigt jeweils deren tatsächliche Existenz und unveränderten
Inhalt. Der fremde begrenzte Testprozess lässt sich nicht adressieren;
derselbe Prozess mit unveränderter Startzeit und Namespace-Zuordnung schreitet
vor und nach den Versuchen fort. Beide Benutzer lesen aus gleichnamigen
POSIX-Nachrichtenwarteschlangen nur ihre eigene Nachricht; die zusätzliche
Warteschlange des anderen Benutzers ist jeweils nicht erreichbar.

Betas Prüfung umfasst Alphas tatsächlich vorhandene private Paketauswahl.
Alpha kann Betas private Pakete noch nicht prüfen, weil Beta bislang keinen
privaten Store besitzt. Dies bleibt eine offene T06-Variante. Der Lauf
beansprucht auch keine vollständige Systemaufrufabdeckung. Alle Felder der
vollständigen Paket-/Kontextaufnahmen vor und nach den Prüfungen sind bis auf
den Erfassungszeitpunkt identisch; der ursprüngliche SystemServer blieb erhalten.

Beleg: `two-user-isolation-proof.json`, SHA-256
`e4ce118967b669b41907a1420cbd4f90218d4e7786a8fb3d617f9c370bdb8283`.
Der lokale Ersteller `record-two-user-isolation.py` prüft die Erfolgsereignisse,
Prozessidentitäten, Fortschritte und Zustandsvergleiche und bindet den festen
Ereignispräfix samt Prüfsummen ein. Rohbelege bleiben auf dem Buildserver.

Die anschließende Android-Bildschirmsperre widerruft Betas offene GNU-Shell;
die CLI meldet `terminal=unauthenticated`. Power und Keyguard bestätigen
unabhängig `mWakefulness=Asleep`, `showing=true` und `mIsShowing=true`.
Die ursprünglichen Prozesse 14559/720255 und 16811/782059 schreiten weiter;
CE bleibt `[0,10,11]`. Boot-ID, SystemServer-PID/Startzeit und Enforcing
bleiben unverändert. Das belegt die vorgesehene Bildschirmsperre und behauptet
keinen CE-Schlüsselentzug.

Beleg: `screen-lock-background-proof.json`, SHA-256
`cdb4ffd32e9b3b4decc6d64d5852d11989266263fb95b90d65b3a7d4139fb380`;
unabhängige Beobachtung `screen-lock-observation/result.json`, SHA-256
`1b8f8bec5395610215b7676a7c3952e359a231377df2c5c67daf6aa50fb3a143`.
Logout, gepaarter Neustart und die verbleibende Pflichtmatrix werden separat
fortgesetzt; die Gesamtfreigabe bleibt offen.

### Regulärer Logout und sauberer Paar-Stopp auf f098f43

Nach dem Aufwecken authentifiziert sich Beta erneut und meldet sich ausdrücklich
ab, ohne vorgeschalteten Runtime-Stopp. Sein ursprünglicher Prozess 16811/782059
und der persönliche Kontext verschwinden; AOSP meldet CE `[0,10]`.
Die zuvor tatsächlich aus GNU geschriebene Datei liefert beim gesperrten Zugriff
keine Bytes. Alphas ursprünglicher Prozess 14559/720255 schreitet unverändert
fort, während der Systembenutzer im Vordergrund steht.

Nach frischer Alpha-Anmeldung bleiben Originaldatei und Konfiguration bytegleich
lesbar. Anschließend meldet sich auch Alpha ausdrücklich ab. Sein ursprünglicher
Prozess und Kontext verschwinden ebenfalls; seine bekannte Datei ist unlesbar.
Die unabhängige Endaufnahme bestätigt CE `[0]`, Vordergrund 0, keine persönlichen
Runtime-Kontexte und keine Paketworker. Beide persönlichen Stores werden wegen
gesperrtem CE nicht gelesen. Die gemeinsame Paketauswahl bleibt bestehen.
Boot-ID und ursprünglicher SystemServer 1404/40097 bleiben unverändert.

Beleg: `both-logout-before-reboot-proof.json`, SHA-256
`1c21ea6ccfa78082147f4882fcab0eada5446650f6e0caa6b01acb5261e6db8d`.
Der Neustart-Checkpoint bestätigt zusätzlich nur System als gestarteten
Benutzer, leere Runtime-Prozessgruppen und die weiterhin unlesbaren Originaldateien.
Die Dienstbeobachtung bis nach beiden Logouts findet weiterhin genau einen
SystemServer-Start und keine der geprüften Java-/Native-Absturz-, ANR-, FORTIFY-
oder Watchdog-Meldungen: `pre-reboot-health.json`, SHA-256
`ef9d70963fded8c8bbee2ed2622e2d32356d9b70c8e1f11c935c369ba88c23d7`.
Die bereits eingeordneten frühen Einmalrückgaben bleiben dokumentiert.

Danach fährt Android über seinen regulären Shutdown-Pfad herunter, während
KeyMint bis zu dessen Ende läuft. Beide ursprünglichen VM-Prozesse und ihr
Starter sind nachweislich beendet. Android protokolliert `reboot: Power down`,
der Helfer `AEGIS_HELPER_SHUTDOWN_CLEAN`; das Profilmanifest ist bytegleich.
Beleg: `paired-shutdown-proof.json`, SHA-256
`4dd9969d3a48f88a47a54be02b157086df5d455af655fcc1d1ba429dbab77fce`.
Der zweite Start verwendet dasselbe Profilpaar und dieselben geprüften Images;
seine Logs liegen separat unter `boot-2`. Bootabschluss, anfängliche CE-Sperrung
sowie Daten-/Versionsreadback nach frischer Anmeldung sind noch nachzuweisen.
Auch Logoutfehler, konkurrierender Start und die übrigen Matrixvarianten bleiben
offen; dieser reguläre Ablauf schließt T10–T12 nicht insgesamt ab.

### Originaldaten und abweichende Versionen nach Paar-Neustart auf f098f43

Der zweite Boot desselben Profils besitzt die neue Boot-ID
`17a75d6e-75f2-4f18-bf02-ec3d093e57b8` und SystemServer 1124/21865.
Vor jeder persönlichen Anmeldung sind nur System-CE und Systembenutzer aktiv,
die persönlichen Runtime-Kontexte fehlen, und beide ursprünglichen Testdateien
liefern keine Bytes. Image-/Profilbindung, authentifiziertes ADB, Enforcing,
FBE und Metadatenverschlüsselung stimmen mit dem ersten Boot überein.
Beleg: `postboot-locked-baseline.json`, SHA-256
`d6e739a9cd01a3e4dd1a287677876a495332c62037b65cf5b68c959670f602c0`.

Alpha und Beta melden sich jeweils beim ersten korrekten Versuch dieses Boots
an; es gibt keinen vorangehenden falschen Versuch zum Aufwärmen. Die verzögerte
Statusprüfung bleibt gültig. Die bloße Zielauswahl entsperrt zuvor weder das
jeweilige CE noch erzeugt sie einen GNU-Kontext. Nach Anmeldung und Runtime-Start
liest jeder Benutzer seine ursprüngliche 1024-Byte-Datei sowie Konfiguration und
synthetische persönliche Testdaten bytegleich. Die alten Proben unter `/tmp`
und `/run/user/1000` und die alten POSIX-Nachrichtenwarteschlangen fehlen.

Die tatsächliche GNU-Ausführung bestätigt bei Alpha weiterhin privates jq/libjq1
`1.7.1-6+deb13u4`, bei Beta gemeinsames `1.7.1-6+deb13u3`, jeweils mit passender
Bibliotheksauflösung, unverändertem libonig5 und erfolgreicher Berechnung.
Beide vollständigen Datenbanken enthalten dieselben jeweils 81 Pakete und exakt
dieselben Rohdaten-Prüfsummen wie vor dem Shutdown; keine Whitespace-Normalisierung
ist erforderlich. Gemeinsame/private Auswahlen, private Versionsfestlegung und
Backing-Image-Pfade sind ebenfalls unverändert. Die beiden neuen Kontexte behalten
getrennte Host-UID/GID-Zuordnungen und Namespaces bei interner UID/GID 1000.

Für spätere Lebenszyklustests werden erst nach Originaldatenreadback neue begrenzte
Hintergrundproben erzeugt. Alphas neue Probe 3660/89824 überlebt den Wechsel zu
Beta; Betas neue Probe ist 5139/121459. Das behauptet kein Überleben der alten
Prozesse über den VM-Neustart.

Zusammenhängender Beleg: `paired-reboot-readback-proof.json`, SHA-256
`aad3626470a13b33ceb4bcac84725d30c2cdc288ae769d921fa9d50b95773683`.
Er bindet die ursprünglichen Daten, erste Anmeldungen, tatsächlichen GNU-Befehle,
vollständigen Paketaufnahmen, GID-Beobachtung und beide Bootidentitäten ein.
Die begrenzte zweite Dienstbeobachtung enthält einen SystemServer-Start und keine
geprüften Java-/Native-Absturz-, ANR-, FORTIFY- oder Watchdog-Meldungen:
`boot2-health-classification.json`, SHA-256
`46ac763c50fe95c3bd5447692022c7dd2efdf1e7f4b64ac639833b9b33394002`.
Eine zusätzliche einmalige Rückgabe der alten VirtualizationService-Aufräumaktion
bleibt sichtbar und ist mit unveränderter init-Quelle sowie tatsächlichem
Verzeichniszustand eingeordnet; sie wird nicht als repariert behauptet.

Die ergänzende [Passwort-/Kryptographiegrundlage](identity-crypto-baseline.md)
enthält neue, ausdrücklich begrenzte Quell- und Konfigurationsbelege. Ein anfänglich
zu pauschaler Vergleich aller Kryptographie-Dateien mit unverändertem Upstream
scheitert an zwei bestehenden AEGIS-Erweiterungen für bestätigte Benutzerlöschung.
Der erhaltene Vergleich wird durch die genaue Bindung ihrer Originaleingänge und
vorbereiteten Ausgänge an den gespeicherten Buildbeleg erklärt; kein Produktcode
und keine Berechtigung wurden dafür geändert.

Damit ist der reguläre Paar-Neustart mit Originaldaten und unterschiedlichen
Paketversionen auf diesem Image nachgewiesen. Passwortwechsel, dritter Benutzer,
vollständige Paketautorisierung, Updates/Entfernung/Konflikte/Parallelität und die
übrigen Pflichtvarianten bleiben offen. Die Phase-1-Gesamtfreigabe bleibt ausstehend.

### Dritter Benutzer mit gemeinsamer Software auf f098f43

Nach dem gepaarten Neustart wird Gamma über die tatsächliche CLI und mit frischer
Adminbestätigung durch Alpha als normaler AOSP-Benutzer `12/12` angelegt.
Die unabhängige Vorher-Aufnahme bestätigt gesperrtes Gamma-CE, keinen Gamma-Kontext
und unveränderte Originaldateien von Alpha/Beta. Auch die Zielvorbereitung vor
der Passworteingabe entsperrt Gamma nicht. Der erste korrekte Login bleibt bei
der verzögerten Statusprüfung gültig; die anschließende GNU-Shell besitzt ein
frisches HOME mit den zehn erwarteten privaten Standardverzeichnissen.

Gamma führt gemeinsames jq/libjq1 `1.7.1-6+deb13u3` und libonig5 `6.9.9-1+b1`
mit erfolgreicher Berechnung und passender Bibliotheksauflösung aus. Seine
vollständige Datenbank mit 81 Paketen und deren Rohdaten-Prüfsumme entsprechen
exakt Betas zuvor verwendeter gemeinsamer Generation. Gamma erhält Host-UID/GID
1207500 bei interner UID/GID 1000; seine sechs beobachteten Namespaces unterscheiden
sich von Alphas aktivem und Betas vorherigem Kontext. Die Basis bleibt
schreibgeschützt, Capabilities sind leer, NoNewPrivs und Seccomp aktiv.

Aus Gammas gewöhnlicher GNU-Shell liefern die bekannten ursprünglichen Dateien,
Konfigurations- und synthetischen persönlichen Testdateien von A/B weder unter
ihren CE-Pfaden noch unter den gleichnamigen eigenen HOME-Pfaden Bytes. Der
Zugriff auf Alphas tatsächlich vorhandene private Paketauswahl wird ebenfalls
verweigert. Gamma legt eigene JSON-Einstellungen mit Modus 0600 an und verarbeitet
sie mit dem gemeinsamen jq. Alphas Konfiguration bleibt nach den Prüfungen
bytegleich; sein ursprünglicher Hintergrundprozess 3660/89824 schreitet weiter.

Dabei beendet AOSP eigenständig Beta, nachdem der Start von Gamma das konfigurierte
Limit laufender Benutzer erreicht. Das Log nennt ausdrücklich „Too many running
users (4)“ und den gewählten Benutzer 11. Betas ursprüngliche Probe 5139/121459
und sein Kontext sind anschließend entfernt, sein CE ist gesperrt; seine bekannte
Originaldatei liefert auch bei gesonderter Prüfung keine Bytes. Es wurde in
diesem Ablauf kein Beta-Logout oder Beta-Runtime-Stopp angefordert. Die Beobachtung
behauptet deshalb keine gleichzeitige Isolation dreier entsperrter Benutzer.
Beta besitzt weiterhin keinen privaten Paketstore; dessen Abwesenheit zählt
nicht als Prüfung eines vorhandenen privaten Bestands.

Beleg: `out/phase1-dod/f098f439/third-user-common-and-isolation-proof.json`, SHA-256
`1af75bc265b6905f516e48c08955bf272984e6e31941aa207d42c7e796784847`.
Er bindet den unveränderlichen Präfix von 266 Treiberereignissen, die vollständigen
Paketaufnahmen, tatsächlich ausgeführten GNU-Befehle, AOSP-Lifecycle-Beobachtung
und den unveränderten SystemServer 1124/21865 ein. Die begrenzte Logaufnahme
enthält keine geprüften Fatal-/ANR-/FORTIFY-/Watchdog-Meldungen. Betas Datenreadback
nach erneuter Anmeldung ist im folgenden gesonderten Wiederanlaufnachweis
erfasst; sämtliche anderen offenen Pflichtvarianten bleiben erforderlich.

### Betas Daten nach AOSP-Ressourcenstopp auf f098f43 erhalten

Gamma wird regulär über die CLI abgemeldet. Vor Betas anschließender frischer
Anmeldung meldet AOSP nur CE `[0,10]`; auch die Zielvorbereitung lässt Betas CE
gesperrt und seinen GNU-Kontext abwesend. Die korrekte Anmeldung bleibt bei
verzögerter Prüfung stabil. Nach dem neuen Runtime-Start liest Beta seine
ursprüngliche Datei und Konfiguration bytegleich, führt gemeinsames jq/libjq1
`u3` mit passenden Abhängigkeiten aus und sieht Gammas eigene JSON-Einstellungen
nicht in seinem HOME.

Die vollständige Paketaufnahme bestätigt dieselben 81 installierten Pakete,
dieselbe Rohdaten-Prüfsumme, gemeinsame/private Auswahlen und dasselbe gemeinsame
Backing-Image wie vor dem Ressourcenstopp. Betas neuer Kontext ist 13017/315337;
seine Namespace-Identitäten unterscheiden sich vom alten Kontext und von Alpha.
Alphas kompletter Kontextdatensatz einschließlich Paketbestand bleibt unverändert,
und die ursprüngliche Probe 3660/89824 schreitet bei Beta im Vordergrund weiter.
Gamma besitzt nach seinem Logout keinen Kontext und bleibt CE-gesperrt.

Erst nach festgestelltem Ende der alten Beta-Probe 5139/121459 und Originaldaten-
Readback wird eine neue begrenzte Probe erzeugt: 13555/331098. Dies zählt nicht
als Überleben der alten Probe. Die kontrollierten alten flüchtigen Dateinamen
waren bereits vor diesem Ressourcenstopp abwesend; ihre erneute Abwesenheit
wird nicht als neuer Nachweis für den Abbau flüchtiger Dateien gewertet.

Beleg: `out/phase1-dod/f098f439/beta-resource-stop-recovery-proof.json`, SHA-256
`bc0b3107b85f0cd631b7e21a27b3fda36fea7c6eb86195b3c9c39da527d8c2e3`.
Der ursprüngliche SystemServer 1124/21865 bleibt erhalten; die eingefrorene
Logaufnahme enthält weiterhin einen Start und keine geprüften Fatal-/ANR-/
FORTIFY-/Watchdog-Meldungen. Der zum Startbeleg bytegleiche Testtreiber prüft
zusätzlich alle sechs Lauf-Logs beider Boots gegen seine vollständigen
synthetischen Testpasswörter und findet keinen Treffer. Dies ersetzt weiterhin
keine vollständige T01.5-Prüfung aller Argumente, Dateien und History-Pfade.

Der aktuelle Referenzschritt 11 sowie dieser AOSP-Ressourcenstopp mit Wiederanlauf
sind damit belegt. Die vollständige Autorisierungsmatrix, Updates, private
Versionsrückgänge/Entfernung/Konflikte/Parallelität und die übrigen Lebenszyklus-
und Identitätsfälle bleiben offen; D1–D7 werden nicht als abgeschlossen markiert.

### Gemeinsames Update ohne Adminauswahl auf f098f43

Beta bleibt als normaler Benutzer angemeldet und beantragt `linux package update
--scope all`. Der Plan nennt jq/libjq1 `1.7.1-6+deb13u3` → `1.7.1-6+deb13u4`,
libpcre2-8-0 `10.46-1~deb13u2` → `10.46-1~deb13u3` sowie libssl3t64 und
openssl-provider-legacy `3.5.7-1~deb13u2` → `3.5.7-1~deb13u3`.
Eine leere Adminauswahl beendet den Auftrag. Die CLI bestätigt keine Veröffentlichung;
Beta ist anschließend weiterhin gültig angemeldet.

Die neuen vollständigen Aufnahmen vor/nach diesem Abbruch sind in allen
Identitäts-, CE-, Auswahl-, Paket- und Kontextfeldern identisch. Alphas privates
u4 und Betas gemeinsames u3 bleiben in ihren bisherigen Kontexten aktiv;
Gamma bleibt gesperrt. Beleg: `out/phase1-dod/f098f439/update-all-cancel-proof.json`,
SHA-256 `5d6531948a7feb12d258029d7dbcfa8ad39974e860149530c5bbe534abbe4bc3`.
Dies ist ein Planabbruch vor Freigabe, kein Abbruch der laufenden Installation
und noch keine vollständige Update-Autorisierungsmatrix.

Das versionierte Werkzeug `scripts/qemu-package-denial-proof.py` prüft einen
einzelnen aufgezeichneten Aktions-/Bereichs-/Ablehnungsfall offline. Es verlangt
den konkreten CLI-Plan, die passende Ablehnung, eine anschließende gültige Sitzung
und exakte Gleichheit der vollständigen Zustandsfelder. Ausgabe und unveränderlicher
Ereignispräfix werden mit Prüfsummen gebunden; bestehende Belege werden nicht ersetzt.
Kontrollen mit ausdrücklich künstlich geändertem CE-Zustand beziehungsweise
Kontextstart werden verweigert. Ihr lokales Ergebnis
`denial-verifier-checks/result.json` hat SHA-256
`9fd0d36d52d70370c2871e39df1deae12a00e156fa6958fec5147d7a6347a919`.

Ein erster Vergleich älterer Installationsaufnahmen wurde wegen unterschiedlicher
Schemas verweigert: `residual_packages` fehlte in der damaligen Vorher-Aufnahme
und war danach als leeres Objekt enthalten. Diese Ablehnung bleibt dokumentiert.
Der Prüfer lockert den Vergleich nicht; die aktuelle Updateprüfung verwendet
das einheitliche vollständige Format. Die künstlichen Verifikatorkontrollen
zählen nicht als zusätzliche Produkt-Abnahmetests.

Ein zweiter tatsächlicher Plan mit denselben fünf Updates erhält anschließend
ein einzelnes synthetisches falsches Alpha-Adminpasswort. AOSP weist es ausdrücklich
ab; die CLI bestätigt keine Veröffentlichung. Beta bleibt angemeldet, und alle
vollständigen Zustandsfelder der beiden Aufnahmen sind exakt gleich. Beleg:
`out/phase1-dod/f098f439/update-all-wrong-proof.json`, SHA-256
`ee220317ebdc6c832065165f024b30f500d86d38b5b53bc5abe6e4ef78166f6d`.

Der dritte Plan erhält Betas korrektes Passwort als Nicht-Adminfreigabe und wird
ebenfalls abgewiesen. Auch hier bleiben alle vollständigen Zustandsfelder
unverändert und die anschließende Statusabfrage bestätigt Betas gültige Sitzung.
Der zusätzliche Hinweis, sich erneut anzumelden, ist weiterhin ungenau; er wird
nicht als tatsächlicher Sitzungsverlust ausgegeben. Beleg:
`out/phase1-dod/f098f439/update-all-nonadmin-proof.json`, SHA-256
`e87848ce3d11c6957398962eb7a7bc3bdde7dda812ae3a1eaae62e45270e899b`.
Damit sind die drei verweigerten Freigabevarianten für `update all` auf diesem
Image belegt. Gültige Veröffentlichung und anschließende Aktivierung werden
separat geprüft.

### Gemeinsames Update veröffentlicht, laufende Kontexte auf f098f43 erhalten

Der vierte Updateplan enthält dieselben fünf Änderungen. Beta stellt weiterhin
den Antrag; Alpha erteilt mit seinem frischen AOSP-Passwort die Adminfreigabe.
Die tatsächliche Worker-Gruppe wird nach der Freigabe unabhängig beobachtet.
Um 10:41:16 UTC bestätigt die CLI die erfolgreiche Veröffentlichung und nennt
ausdrücklich den nötigen eigenen Linux-Neustart zur Aktivierung.

Der gemeinsame Auswahleintrag wechselt von Generation
`13629e83de8896bf52e5695a071abd67b4232300791fa0f6c3ed484c19f9007e`
auf `53e5fddf27e7b22607a60bad9e28a1ad77128d009cb31d0b698329eb1c23bc34`.
Die beiden vollständigen aktiven Kontextdatensätze einschließlich Paketdatenbanken,
Backing-Images und Namespaces bleiben dagegen exakt identisch. Alphas private
Festlegung und ihre Bindung an die bisherige gemeinsame Basis werden nicht
still verändert; Beta besitzt weiterhin keinen privaten Store. CE bleibt
`[0,10,11]`, Gamma bleibt gesperrt, und es ist kein Paketworker mehr vorhanden.

Beta meldet `packages=activation-pending` und führt weiterhin jq/libjq1 `u3`
sowie die bisherigen PCRE2-/OpenSSL-Versionen aus. Nach frischer Alpha-Anmeldung
zeigt auch Alpha die ausstehende Aktivierung und führt weiterhin sein privates
jq/libjq1 `u4` mit den bisherigen übrigen Bibliotheken aus. Beide Berechnungen
und Bibliotheksprüfsummen sind tatsächlich aus GNU-Prozessen erfasst. Die
ursprünglichen Hintergrundproben 13555/331098 und 3660/89824 schreiten weiter.

Beleg: `out/phase1-dod/f098f439/shared-update-publication-proof.json`, SHA-256
`b746d1189753e8df3d83fec3be6ce0b4b95bc743904550a8c15ec8d87a7c5bcc`.
Er verknüpft alle drei Ablehnungsbelege, die gültige Freigabe, Veröffentlichung,
vollständige Zustandsaufnahme und beide bisherigen GNU-Ausführungen. Der
ursprüngliche SystemServer bleibt erhalten; eine zusätzliche begrenzte Logprüfung
enthält keine geprüften Fatal-/ANR-/FORTIFY-/Watchdog-Meldungen. Der Treiber findet
in allen sechs Lauf-Logs weiterhin kein vollständiges synthetisches Testpasswort.

Damit sind die vier Freigabevarianten und die Veröffentlichung für `update all`
sowie der Erhalt laufender Kontexte belegt. Die tatsächliche Aktivierung der
neuen vollständigen Paketstände ist noch erforderlich. Ebenso bleiben der
private Beta-Versionsrückgang mit Alpha-Freigabe und die übrigen Pflichtfälle
offen. Alpha war vor dieser Freigabe bereits CE-entsperrt; Verhalten bei einem
gesperrten freigebenden Administrator wird hier nicht zusätzlich behauptet.

### Gemeinsames Update auf f098f43 bei beiden Benutzern aktiviert

Alpha und danach Beta stoppen jeweils über ihre eigene CLI ausschließlich ihre
Linux-Runtime. Vor jedem Stopp werden zwei neue temporäre Dateien in `/tmp` und
`/run/user/1000` angelegt und aus der gewöhnlichen GNU-Shell positiv gelesen.
Beide Stopps entfernen den eigenen ursprünglichen Kontext und Hintergrundprozess,
erhalten aber die gültige AOSP-Sitzung und CE `[0,10,11]`. Der jeweilige andere
Hintergrundprozess behält PID, Startzeit und Namespaces und schreitet weiter.

Alphas anschließender Start bestätigt um 11:07:44 UTC eine bereite Runtime.
Seine private Generation wechselt auf
`7203ccdcf8b90c0226b6cdff7414e53b20bf8f942215201be56730b64b1fb1dc`,
gebunden an die veröffentlichte gemeinsame Basis `53e5fddf…23bc34`.
Die explizite private jq-Festlegung `1.7.1-6+deb13u4` bleibt bytegleich.
Betas kompletter alter Kontextdatensatz bleibt dabei unverändert. Betas Start
bestätigt um 11:19:58 UTC ebenfalls eine bereite Runtime; er verwendet direkt
das neue gemeinsame Image und besitzt weiterhin keinen privaten Paketstore.
Alphas kompletter neuer Kontextdatensatz bleibt währenddessen unverändert.

Beide führen jq/libjq1 `u4` und die geplanten neuen PCRE2-/OpenSSL-Versionen
tatsächlich aus; jq berechnet das erwartete Ergebnis, die Bibliotheken werden
aufgelöst, UID/GID und getrennte Zuordnungen stimmen. Die vollständigen
81-Paket-Bestände entsprechen exakt dem veröffentlichten Plan, ohne verbliebene
Paketreste. Beide rohen Paketdatenbanken haben SHA-256
`6bcb23b8377daad330bdfe6a0c5595da8989ef5c567581223c608cede0fb6f9b`.
Beide Statusabfragen melden `packages=current`.

Die ursprünglichen persönlichen Dateien und Konfigurationen werden vor und nach
dem jeweiligen Neustart bytegleich gelesen. Die frisch vorbereiteten temporären
Dateien sind danach verschwunden. Ersatz-Hintergrundproben werden erst nach
belegtem Ende der alten Prozesse und Originaldaten-Readback gestartet; sie zählen
nicht als Überleben der beendeten Prozesse. Gamma bleibt CE-gesperrt. Der
SystemServer bleibt 1124/21865, und der Treiber findet in allen sechs Lauf-Logs
kein vollständiges synthetisches Testpasswort.

Beleg: `out/phase1-dod/f098f439/shared-update-activation-proof.json`, SHA-256
`f812ec421300f35c27a2683431fb3e3536d932053f7182b093972e5a0158df13`.
Er bindet den Ereignispräfix, die Veröffentlichung, beide vollständigen
Zustandsaufnahmen, Originaldaten-Prüfungen und tatsächlichen GNU-Ausführungen.
Die Metadatenaufnahmen sind sequenzielle Beobachtungen bei stabilem Zustand.
Dies belegt beide Runtime-Stopprichtungen und die gemeinsame Updateaktivierung
mit erhaltener privater Auswahl. Der private Beta-Versionsrückgang gegen die
neuere gemeinsame Basis und die übrigen Pflichtfälle bleiben offen.

### Nicht bestätigter Alpha-Logout und reguläre Wiederherstellung auf f098f43

Beim folgenden regulären Alpha-Logout tritt ein neuer Fehler auf: Um 11:31:03 UTC
meldet die CLI „Aktion nicht bestätigt“. AOSP hat Alpha gestoppt und zum
Systembenutzer gewechselt, bestätigt aber die CE-Sperrung nicht. Die Terminalsitzung
ist widerrufen. Das Kernel-Log meldet drei noch belegte Inodes, darunter 4589;
StorageManagerService meldet ausdrücklich eine ausstehende CE-Schlüsselsperrung.
Dieser ursprüngliche Logout zählt **nicht als bestanden**.

Die gesonderte Zustandsaufnahme bestätigt den unveränderten SystemServer, Alphas
entfernte Runtime-Gruppe und den entfernten Hintergrundprozess. In der späteren
Loop-Aufnahme ist kein Alpha-privates Image mehr vorhanden. Betas ursprünglicher
Prozess 30976/852342 läuft bei Systembenutzer 0 weiter. Welche Referenz die drei
Inodes zum Fehlerzeitpunkt festhielt und wann sie freigegeben wurde, ist damit
nicht geklärt. Die spätere Abwesenheit darf nicht als Erklärung der Ursache gelten.

Die nächste reguläre Anmeldevorbereitung schließt die ausstehende CE-Sperrung ab,
bevor ein Passwort übertragen wird: CE ist `[0,11]`, Alphas Kontext fehlt, und
seine bekannte ursprüngliche Datei liefert keine Bytes. Erst ein frisch von AOSP
geprüftes Passwort entsperrt ihn wieder. Nach Runtime-Start werden die ursprüngliche
Datei und Konfiguration bytegleich gelesen und privates jq/libjq1 `u4` erfolgreich
ausgeführt. Alle 81 Pakete, der rohe Datenbankhash, die private Auswahl und das
Backing-Image stimmen mit dem Stand vor dem Fehler überein. Betas kompletter
Kontextdatensatz ist unverändert.

Beleg: `out/phase1-dod/f098f439/alpha-pending-logout-recovery-proof.json`, SHA-256
`c7e75df4adbcb90ad9e4b97b2a325dca50b8e07514e43d2bc7523ce2cbfb0f38`.
Ein erster Offline-Vergleich wies die geänderte CE-Reihenfolge zurück. Der
Wiederherstellungsnachweis verlangt jetzt ausdrücklich `[0,11,10]` nach der
erneuten Alpha-Anmeldung; beide Originalaufnahmen bleiben erhalten. Es wurden
keine Produktänderung, kein Framework-Neustart, kein manueller Schlüsseleingriff
und kein Cache-Leeren zur Wiederherstellung eingesetzt.

Dies belegt die sichtbare Ablehnung eines tatsächlichen unvollständigen Logout
und die anschließende reguläre Wiederherstellung. Es ersetzt weder die noch
offene Ursachenklärung noch sämtliche absichtlich ausgelösten Fehler-,
Parallelitäts- und Paketvarianten von T11.

### Private Beta-Paketfreigabe bei gesperrtem Alpha auf f098f43

Nach dem Wiederanlauf wird Alphas Runtime separat gestoppt. Der folgende neue
Logout bestätigt um 11:45:04 UTC tatsächlich die CE-Sperrung; eine unabhängige
Prüfung zeigt nur `[0,11]`. Dieser zweite erfolgreiche Ablauf erklärt den zuvor
fehlgeschlagenen Logout nicht. Beta meldet sich frisch an und behält seinen
laufenden gemeinsamen u4-Kontext; Alpha und Gamma bleiben gesperrt.

Beta beantragt `linux package install --scope user jq=1.7.1-6+deb13u3`.
Der tatsächliche Plan enthält genau jq und libjq1 von `u4` auf `u3`.
Eine leere Adminauswahl bricht den ersten Plan ab. Beim zweiten identischen Plan
weist AOSP ein einzelnes falsches Alpha-Adminpasswort ausdrücklich ab. Beide
anschließenden Statusabfragen bestätigen Betas gültige Sitzung.

Die vollständigen Aufnahmen vor und nach jeder Ablehnung stimmen in allen
Identitäts-, CE-, Auswahl-, Paket- und Kontextfeldern exakt überein. Beta besitzt
weiterhin keinen privaten Store. Die gesperrten Alpha-/Gamma-Stores werden vom
Beobachter nicht geöffnet; er behauptet für sie keinen direkten Inhaltsvergleich.
Belege unter `out/phase1-dod/f098f439/`:

- `private-beta-cancel-proof.json`, SHA-256
  `ed2f532b01bd8cf175b1322be03efbbac08264721a30d90829d7b8df51adf6fb`.
- `private-beta-wrong-proof.json`, SHA-256
  `51cf29a728e834f1c5d78ac6815bc75ddd9147de9e26ec4fe4b5ad2560a6b81b`.

Auch der dritte identische Plan wird mit Betas korrektem Passwort als
Nicht-Adminfreigabe abgewiesen. Sein vollständiger Zustandsvergleich ist erneut
exakt gleich, und Beta bleibt gültig angemeldet. Der generische erneute
Anmeldehinweis der CLI ist weiterhin ein Bedienungsfehler. Beleg:
`private-beta-nonadmin-proof.json`, SHA-256
`c106fa34d1e4bb382be42910073606ad1d100a9ceada0cee3836088126fd2c07`.
Der Treiber findet anschließend in allen sechs Lauf-Logs beider Boots kein
vollständiges synthetisches Testpasswort. Das ersetzt weiterhin keine vollständige
T01-Prüfung aller Argumente, Dateien und History-Pfade.

Gültige Veröffentlichung und tatsächliche private Versionsausführung sind damit
noch nicht nachgewiesen. Die drei verweigerten Varianten für `install user`
sind auf diesem Image belegt; andere Aktionen und Bereiche bleiben getrennt.

### Betas privater Versionsrückgang auf f098f43 tatsächlich ausgeführt

Der vierte identische Plan erhält um 12:17:20 UTC eine gültige frische
Alpha-Adminfreigabe. Um 12:19:01 UTC bestätigt die CLI die erfolgreiche private
Veröffentlichung. Der Auswahleintrag gehört ausdrücklich Beta 11/11; seine
Generation `f38859959a4fcfd655b73c5c9f2e89505a24e76083c893561c0f51a43cea2095`
ist an die unveränderte gemeinsame Basis `53e5fddf…23bc34` gebunden.

Betas vollständiger aktiver Kontext bleibt bis zum bewussten Neustart exakt
unverändert. Er meldet ausstehende Aktivierung, führt weiterhin gemeinsames
jq/libjq1 `u4` aus und behält den ursprünglichen Hintergrundprozess 30976/852342.
Die unabhängigen Aufnahmen vor und nach der Veröffentlichung zeigen CE `[0,11]`;
Alphas und Gammas gesperrte Stores werden nicht gelesen. Alphas zuvor bekannte
persönliche Datei liefert nach der Freigabe weiterhin keine Bytes.

Nach eigenem `linux stop` bleibt Beta gültig angemeldet. Der neue Start bestätigt
um 12:24:48 UTC eine bereite Runtime. Kontext 9084/1222438 verwendet tatsächlich
das private Image unter `/data/misc_ce/11/aegis/packages/store/`. Seine explizite
jq-Auswahl ist `1.7.1-6+deb13u3`. Der vollständige Bestand umfasst weiterhin 81
Pakete: Ausschließlich jq und libjq1 wechseln von `u4` auf `u3`, alle anderen
Versionen bleiben erhalten, insbesondere die neuen PCRE2-/OpenSSL-Versionen.
Die rohe Paketdatenbank hat SHA-256
`73d64ac8c7a9e0f018ee59c2b21965a5aa5c9e8e7aef5bb4187340f39b4ed03e`.

Die gewöhnliche GNU-Shell bestätigt Versionen und Bibliotheksauflösung und
berechnet mit jq das erwartete Ergebnis. Beide ursprünglichen persönlichen
Dateien und die Konfiguration bleiben bytegleich. Zwei unmittelbar vor dem
Stopp positiv gelesene temporäre Dateien sind danach verschwunden. Erst nach
belegtem Ende der alten Probe und erneutem Originaldaten-Readback entsteht die
neue begrenzte Probe 9255/1238958. Der Status meldet `packages=current`.

Die versuchte Live-Worker-Beobachtung besitzt ausdrücklich keinen Erfolgsbeleg:
Beim ersten Versuch endete PID 8145 zwischen Gruppen- und Prozesslesung, beim
zweiten war die Veröffentlichung bereits beendet und die Gruppe entfernt.
Beide Beobachtungsfehler sind lokal festgehalten. Der Auftrag wurde nicht neu
gestartet; ein vollständiger Live-Worker-Nachweis oder eine lückenlose CE-Zeitreihe
wird daraus nicht behauptet.

Beleg: `out/phase1-dod/f098f439/private-beta-downgrade-activation-proof.json`, SHA-256
`e04b127f05db4e84025d693f6c829d684d4a180bf3b79bab8e932a2859bd8811`.
Er bindet die drei Ablehnungsfälle, frische Freigabe, Veröffentlichung,
vollständige Zustandsaufnahmen und tatsächliche GNU-Ausführung an denselben
Image-/Profil-/Bootstand. Der zuvor auf `209278de` fehlgeschlagene gewöhnliche
private CLI-Versionsrückgang ist damit auf dem Korrekturimage `f098f43` bestanden.
Die folgenden Prüfungen ergänzen die gegenseitige Isolation bei gleichzeitig
entsperrten privaten Beständen. Die neue Auswahl über einen gepaarten Neustart
nachzuweisen sowie die übrigen Entfernungs-, Update-, Konflikt- und
Parallelitätsfälle bleiben erforderlich.

### Beide vorhandenen privaten Paketauswahlen gegenseitig geschützt

Am 3. Oktober 12:34–13:04 UTC wird Alpha frisch authentifiziert und sein privater
u4-Kontext gestartet, während Betas privater u3-Kontext erhalten bleibt. Vor der
Passworteingabe bleibt Alphas CE gesperrt. Danach liest Alpha seine ursprüngliche
Datei und Konfiguration unverändert und führt jq/libjq1 u4 tatsächlich aus.
Für die temporären Isolationsproben kopiert jeder Benutzer ausschließlich seine
eigene vorhandene Testkonfiguration in die zuvor fehlenden `/tmp`- und
`/run/user/1000`-Testdateien. Persistente Testdaten werden nicht neu geschrieben.

In beiden gewöhnlichen GNU-Kontexten werden Lese- und Schreibzugriffe auf sieben
positiv vorhandene Gegenstellen abgewiesen: Konfiguration, Test-Secret und zwei
temporäre Dateien über den Peer-Prozess, die beiden persistenten CE-Pfade sowie
die private `packages/store/current`-Auswahldatei. Die separate Entwicklerbeobachtung
bestätigt Existenz und bytegleichen Inhalt vor und nach den Versuchen. Beide
privaten Auswahldateien sind tatsächlich vorhanden; ihre rohen SHA-256-Werte sind
`ac864c4be1ea536636fc10bbb59cc1be79545158e0771637a6139db39b53c883`
(Alpha) und `70819a1f2f7a74e767826c1b0d4bd49c0f98869b33566acd85048eb394543f53`
(Beta). Die Beobachtung mit Entwicklerrechten ersetzt dabei keinen GNU-Zugriffstest.

Zusätzlich verweigern beide GNU-Kontexte den Zugriff auf die bekannte ursprüngliche
Peer-Datei über drei Pfade und ein SIGSTOP an den bekannten Peer-Prozess.
Alphas Probe 11600/1317468 und Betas Probe 9255/1238958 behalten jeweils ihre
Identität und schreiten nach den Versuchen fort. Alle sechs Namespaces sind
getrennt. Beide ursprünglichen 1024-Byte-Dateien und Konfigurationen bleiben
unverändert lesbar im eigenen Kontext.

Die vollständigen Aufnahmen vor und nach beiden Richtungen stimmen außer dem
Erfassungszeitpunkt exakt überein: beide 81-Paket-Bestände samt rohen Datenbankhashes,
Auswahlen, Kontextidentitäten, Mappings, Namespaces, Boot und SystemServer.
CE bleibt `[0,11,10]`, Gamma gesperrt; Paketarbeitsgruppen fehlen. Der lokale
Verifier bindet die Ereignisse ab Index 446 bis einschließlich Präfix 503 und
den vorherigen Beta-Aktivierungsnachweis. Beleg unter
`out/phase1-dod/f098f439/two-private-contexts-isolation-proof.json`, SHA-256
`fd37f26f8b072c53880b4babf9e145d593608e5a329a895fd243d996e0ba58b4`.

Dieser Abschnitt ergänzt T06 um beide vorhandenen privaten Auswahldateien;
er behauptet weder sämtliche Paketdateien/Syscalls noch neue IPC-Abdeckung.
Der frühere POSIX-Mqueue-Beleg bleibt separat. Die Zustandsaufnahmen sind
sequenziell, keine atomare oder lückenlose CE-Beobachtung. Der anschließende Scan
aller sechs Lauf-Logs findet kein vollständiges Testpasswort; die übrigen
T01-Prüfungen und die vollständige Phase-1-Abnahme bleiben offen.

### Passwortwechsel und erneute gesperrte Anmeldung auf f098f43

Am 3. Oktober 13:10:34 UTC bestätigt AOSP den über die CLI ausgeführten
Passwortwechsel von Beta 11/11. Die weiterhin gültige Sitzung liest die
ursprüngliche 1024-Byte-Datei und Konfiguration unverändert und führt privates
jq/libjq1 `1.7.1-6+deb13u3` mit dem erwarteten Rechenergebnis aus. Die vollständige
Paketaufnahme bleibt gegenüber dem vorherigen Isolationsstand in allen Feldern
außer Erfassungszeitpunkt und dem dokumentierten Vordergrundwechsel von Alpha
zu Beta identisch. Beide bisherigen Hintergrundprozesse schreiten fort.

Der reguläre Beta-Logout bestätigt um 13:16:19 UTC den Ressourcenabbau und die
CE-Sperrung. Die unabhängige Kontrolle bestätigt das Ende der ursprünglichen
Probe 9255/1238958, einen fehlenden Beta-Kontext und CE `[0,10]`. Die bekannte
ursprüngliche Beta-Datei liefert keine Bytes. Ein einzelner Versuch mit dem
alten Passwort wird um 13:18:17 UTC ausdrücklich von AOSP abgewiesen; der
Terminalstatus bleibt unauthentifiziert. Die anschließende Aufnahme zeigt
weiterhin gesperrtes Beta-CE, keinen Beta-Kontext und einen vollständig
unveränderten Alpha-Kontext. Auch die zweite Prüfung der Originaldatei liefert
keine Bytes.

Das neue Passwort wird um 13:21:46 UTC akzeptiert. Beide Anmeldevorbereitungen
ließen Beta vor der Passworteingabe gesperrt und ohne Runtime. Der anschließende
reguläre Start erzeugt Kontext 22053/1572254. Die ursprüngliche Datei bleibt bei
SHA-256 `e32b6a6650212c74df22976a962bed97babc4fd30dd1fc393948ca1ebdf08175`,
die Konfiguration bei
`214bf6c06705677aec69726a3273f9e194ad145c9a7051fbcd7fd89409ecfd28`.
Die vor dem Logout vorhandenen temporären Testdateien fehlen. Privates jq/libjq1
u3 wird tatsächlich ausgeführt; alle 81 Paketversionen, der rohe Datenbankhash,
das private Image, die Auswahl und das UID-Mapping stimmen mit dem Vorzustand
überein. Alphas Kontext und ursprüngliche Probe 11600/1317468 bleiben erhalten.
Betas neue begrenzte Probe 22512/1586287 entsteht erst nach belegtem Ende der
alten Probe und erneutem Originaldaten-Readback.

Der lokale Offline-Verifier bindet Ereignispräfix 544, den vorherigen
Isolationsnachweis und die drei neuen Zustandsaufnahmen. Beleg:
`out/phase1-dod/f098f439/beta-password-change-readback-proof.json`, SHA-256
`276ff9864fe81196268aae2ff673156bea885d4a8a781e1b3edab3c276262180`.
Der abschließende Scan der sechs Lauf-Logs findet weder das vollständige alte
noch das neue Testpasswort. Die Testpasswörter bleiben ausschließlich im Speicher
des bestehenden Treibers.

Dies belegt den Passwortwechsel und die erneute Anmeldung nach bestätigter
Sperrung auf dem aktuellen Image. Ein vollständiger gepaarter VM-Neustart mit
dem neuen Passwort und beiden privaten Beständen steht noch aus; T02 ist damit
noch nicht vollständig abgenommen. Die unveränderten Dateiinhalte allein
belegen außerdem keine Details der Schlüsselumhüllung oder Neuverschlüsselung.

### Neues Passwort und beide privaten Bestände über gepaarten Neustart erhalten

Am 3. Oktober wird das laufende Profil nach dem Passwortwechsel erneut vollständig
heruntergefahren und als `boot-3` gestartet. Beta meldet sich um 13:33:15 UTC ab,
Alpha um 13:39:34 UTC. Beide direkten Logouts werden bestätigt; die ursprünglichen
Proben 22512/1586287 und 11600/1317468 sowie beide Runtime-Kontexte sind entfernt.
Die bekannten Originaldateien liefern keine Bytes und AOSP bestätigt CE `[0]`.
Alphas früherer ausstehender Logout bleibt dadurch weder erklärt noch behoben.

Der neue Checkpoint bindet die konkrete Android-/KeyMint-Prozessgruppe und
Profil-ID `535c2e93-df64-445b-b9e2-b71e6b403db7`. Android bestätigt den regulären
Power-down, der Helfer seine saubere Beendigung, der Launcher endet mit Status 0.
Alle aufgezeichneten Prozessidentitäten sind beendet. Das Profilmanifest bleibt
bytegleich; der neue Start verwendet dasselbe Android-Overlay und dieselbe
Helferpartition ohne Neuanlage eines Profils.

Vor jeder persönlichen Anmeldung bestätigt der unabhängige Beobachter die neue
Boot-ID `ea23ac90-af1b-4f26-b027-9085f390f21b`, SystemServer 1103/21177, aktives
SELinux, authentifiziertes ADB, CE `[0]` und fehlende persönliche Kontexte.
Beide bekannten Originaldateien sind weiterhin unlesbar. Der Helfer bestätigt
das vorhandene persistente Profil. Die ADB-Verbindung benötigt nach einem
fehlgeschlagenen ersten Handshake den bereits vorgesehenen zweiten Verbindungsaufbau
mit derselben Host-Identität; es wird kein neuer Hostschlüssel autorisiert.
Beide Ergebnisse bleiben in `boot3-adb-connection.json` erhalten.

Alphas erster korrekter Login gelingt um 13:57:47 UTC, Betas erster Login mit
dem **neuen** Passwort um 14:03:07 UTC. Keinem dieser Logins geht im neuen Boot
ein falsches oder altes Passwort voraus. Die jeweiligen Vorbereitungen lassen
den Zielbenutzer vor der Passwortprüfung gesperrt und ohne Runtime. Danach lesen
beide gewöhnlichen GNU-Kontexte ihre ursprünglichen 1024-Byte-Dateien und
Konfigurationen bytegleich. Alpha führt privates jq/libjq1 `u4`, Beta privates
`u3` mit dem erwarteten Ergebnis aus.

Der vollständige Vergleich gegen den Zustand vor dem Herunterfahren bestätigt
beide 81-Paket-Bestände einschließlich aller Versionen und rohen Datenbankhashes,
privaten Imagepfade, Auswahlmetadaten und UID-Mappings. Die sechs Namespaces der
neuen Kontexte sind untereinander getrennt. Alte temporäre Probepfade fehlen;
Betas ursprüngliche temporäre Dateien waren bereits bei seinem vorherigen
Kontextneustart entfernt worden. Ihre erneute Abwesenheit wird daher nicht als
zusätzlicher Versuch mit unmittelbar positivem Vorzustand ausgegeben.

Beleg: `out/phase1-dod/f098f439/password-paired-reboot-readback-proof.json`, SHA-256
`28244bb5466e7ab96ad56a4ac337d541e12f75d74c2f44af6518b4e33d05f0c8`.
Der Offline-Verifier bindet Ereignispräfix 586, Passwortwechsel, beide Logouts,
gepaarten Shutdown, neue gesperrte Ausgangslage und vollständige Readbacks.
Der Scan aller neun Lauf-Logs findet kein vollständiges Testpasswort.
Die begrenzte Logprüfung `boot3-bounded-health.json`, SHA-256
`aebc15ccd52519bfba81dc13f6ae42bc9d1b74b0636cebd40781dac12b05cead`,
zeigt einen SystemServer-Eintritt und keine der geprüften Kernel-Panik-,
Fatal-, ANR-, FORTIFY- oder Watchdog-Meldungen. Sie ersetzt keine vollständige
Gerätedienstklassifizierung.

Der alte laufende Testtreiber bindet die Erneuerung seiner Hintergrundproben an
seinen ersten `reboot-checkpoint.json`. Diese Grenze wurde vor einer erneuten
Verwendung erkannt. Der ursprüngliche Checkpoint bleibt unverändert; der zweite
Neustart verwendet separate Nachweise und behauptet keine Fortführung alter
Prozessidentitäten. Login-, GNU- und Paketsteuerungen bleiben nutzbar.
Die Ablehnung des alten Passworts ist bislang im zweiten Boot belegt, nicht
zusätzlich im dritten. Weitere Matrixfälle und die vollständige Phase-1-Abnahme
bleiben offen.

### Altes Passwort auch nach dem gepaarten Neustart abgewiesen

Im dritten Boot folgt nach dem ersten erfolgreichen neuen Passwortzugang ein
weiterer regulärer Beta-Logout um 14:14:55 UTC. Die unabhängige Aufnahme bestätigt
das Ende des aktuellen Kontexts 4099/108039, die entfernte Runtime-Gruppe und
CE `[0,10]`. Betas ursprüngliche Testdatei liefert keine Bytes. Der anschließende
einmalige Versuch mit dem alten Passwort wird um 14:18:38 UTC von AOSP abgewiesen.
Bereits die Anmeldevorbereitung lässt Beta gesperrt und ohne Kontext; danach
bleibt das Terminal unangemeldet und die bekannte Datei weiterhin unlesbar.

Der vollständige Zustandsvergleich erhält Alphas Kontext 3553/81468 und seine
Paketdaten unverändert. Beta besitzt keinen Kontext; seine gesperrte private
Paketauswahl wird nicht gelesen. Boot-ID und SystemServer bleiben identisch.
Der Offline-Nachweis bindet den unveränderten gestarteten Treiber, Ereignisse
586–592 und die unabhängigen Vor-/Nachaufnahmen:
`out/phase1-dod/f098f439/boot3-old-password-denial-proof.json`, SHA-256
`59f512b71e2e512ce3a0414d8993af4be0d4e7cca01243ccc65e334a2575c496`.
Damit ist die zuvor ausstehende zusätzliche Ablehnung nach diesem Neustart
belegt. Die übrigen Pflichtfälle und die Gesamtfreigabe bleiben offen.

Nach diesem Negativfall gelingt die frische Anmeldung mit dem neuen Passwort
um 14:33:58 UTC. Der reguläre Runtime-Start erzeugt Beta-Kontext 5565/294246;
Alphas Kontext bleibt vollständig unverändert. Gewöhnliche GNU-Prozesse lesen
die ursprüngliche Datei und beide Einstellungen bytegleich, bestätigen die
Abwesenheit der alten temporären Dateien und führen privates jq/libjq1 `u3`
unter UID/GID 1000 aus. Der vollständige Vergleich erhält alle 81 Paketversionen,
den Datenbankhash, private Auswahl, Imagepfad und UID-Mapping. Betas neue sechs
Namespaces sind von Alpha getrennt. Nachweis mit Ereignispräfix 603:
`boot3-beta-recovery-proof.json`, SHA-256
`33a866cf464e6d07eb06b3870cf4da7b44e8a2513585f6ce9fe02492bf00d064`;
vollständige Aufnahme `boot3-beta-recovery-before-private-removal.json`, SHA-256
`bbea33b11a93f22d738ae2fb43b2298fe8816a5b03ac26b9fb4a24795e88f56f`.

### Offline-Paketnachweise nach Passwortwechsel

Der laufende Testtreiber benennt Betas Freigabeereignis nach einem Passwortwechsel
`package-approve-newbeta`. Der Offline-Prüfer akzeptierte bisher nur den alten
Namen und hätte einen gültigen Nachweis der Nicht-Admin-Ablehnung zurückgewiesen.
Er erkennt jetzt beide Namen als dieselbe Prüfrolle und verlangt weiterhin
genau ein Ergebnis, die ausdrückliche Ablehnung, die gültige Antragstellersitzung
und vollständig unveränderte Zustandsfelder. Zwei Ergebnisse bleiben mehrdeutig
und werden abgelehnt; andere Identitäten oder eine bloße Passwortablehnung
ersetzen keine Nicht-Admin-Prüfung.

Sieben Host-Regressionstests bestehen. Die drei ursprünglichen privaten
Installationsablehnungen wurden mit dem geänderten Prüfer erneut ausgewertet;
Originalbelege und Gastzustand wurden dabei nicht verändert. Lokale Belege:
`denial-verifier-rotated-credential/result.json`, SHA-256
`af80100159e8aef660b6bb01a6f199a38e60deb98f07b3380e8fc2a3486f0234`,
und `denial-verifier-rotated-credential/historical-replay.json`, SHA-256
`2e4c34dabb2e60d3ce98bda876e1629607ebb027a5c39250b5e274be997ce0a9`.
Dies ändert ausschließlich die Offline-Auswertung; ein neuer Gasttest mit
gewechseltem Nicht-Admin-Passwort ist dadurch noch nicht nachgewiesen.

### Private Entfernung: fehlende und falsche Adminfreigabe

Beta beantragt im dritten Boot die Entfernung seiner privaten jq-Auswahl `u3`
bei gemeinsam installiertem `u4`. Beide unabhängig berechneten CLI-Pläne zeigen
ausdrücklich die Aufhebung der privaten Wahl und die Rückkehr zu gemeinsamer
Version `u4`; jq und libjq1 sollen gemeinsam von `u3` auf `u4` wechseln.

Die erste Adminauswahl bleibt leer. Die CLI bestätigt um 14:44:58 UTC das Ende
ohne erfolgreiche Veröffentlichung. Beim zweiten Plan verweigert AOSP um
14:53:25 UTC ausdrücklich das falsche Alpha-Passwort. Nach beiden Ablehnungen
bestätigt ein eigener Statusaufruf Betas gültige Nicht-Admin-Sitzung.

Die vollständigen unabhängigen Vor-/Nachaufnahmen sind in sämtlichen geprüften
Feldern identisch: Image, Profil, Boot, SystemServer, CE `[0,10,11]`, Vordergrund,
Benutzerliste, gemeinsame und private Auswahlen, beide vollständigen Kontexte
einschließlich Paketdatenbanken sowie fehlende Paketworker. Alpha bleibt privat
auf `u4`, Beta auf `u3`; Gamma bleibt gesperrt und seine Auswahl wird nicht gelesen.
Die Aufnahmen sind aufeinanderfolgende Beobachtungen im ruhenden Zustand,
keine lückenlose Beobachtung während der Planung.

Belege unter `out/phase1-dod/f098f439/`:

- `private-remove-beta-cancel-proof.json`, SHA-256
  `142ff9a48c2a06b08bdaa86cd7188c20fd1e4cffea5d98ee69f0b49c15073a8c`,
  Ereignisintervall `[604,607)`.
- `private-remove-beta-wrong-proof.json`, SHA-256
  `66a5a9ea9636c640a95f9d25ac3600eccca46e88bb197d047cad0677626aa23f`,
  Ereignisintervall `[607,610)`.

Dies schließt zwei Varianten von `remove --scope user`. Nicht-Adminfreigabe,
gültige Freigabe und die tatsächliche Aktivierung der gemeinsamen Variante
bleiben in dieser Kombination noch offen. Ein vor der Ausführung abgebrochener
Plan ist kein Nachweis für Abbruch während einer laufenden Installation.

Der dritte gleichartige Plan wird um 15:02:06 UTC mit Betas korrektem **neuem**
Passwort bestätigt. AOSP verweigert die Adminaktion; es handelt sich nicht um
eine Ablehnung des Passworts. Der anschließende Status bestätigt Beta weiterhin
als angemeldeten Nicht-Admin. Alle vollständig verglichenen Zustandsfelder
bleiben gegenüber dem zweiten Ablehnungsfall exakt identisch. Alpha und seine
private Auswahl bleiben erhalten, Gamma bleibt gesperrt.

Beleg: `private-remove-beta-nonadmin-proof.json`, SHA-256
`32855c9cb2a4e9ed4320b72330b61e7af68564b7550dbeadfc41e26bd0018323`,
Ereignisintervall `[610,613)`. Der korrigierte Offline-Prüfer verarbeitet hier
erstmals den tatsächlichen Gastnachweis `package-approve-newbeta`; seine
Zustands- und Rollenanforderungen bleiben unverändert. Der generische
Anmeldehinweis der CLI ist weiterhin irreführend und wird nicht als tatsächlicher
Sitzungsverlust gewertet. Damit sind alle drei verweigerten Freigabevarianten
für private Entfernung belegt; gültige Ausführung und Aktivierung bleiben offen.

### Private Entfernung mit anderer gemeinsamer Version vollständig aktiviert

Der vierte Beta-Plan erhält um 15:10:57 UTC eine frische Alpha-Adminfreigabe.
Die CLI bestätigt um 15:12:58 UTC die Veröffentlichung. Ausschließlich Betas
private Auswahl wechselt auf Generation
`16a0d93cd966f55b4327deff1c50edec7fd476eb7d46ded718422136f8fe6352`, weiterhin
an gemeinsame Generation `53e5fddf27e7b22607a60bad9e28a1ad77128d009cb31d0b698329eb1c23bc34`
gebunden. Beide vollständigen laufenden Kontexte bleiben zunächst unverändert;
Beta meldet ausstehende Aktivierung und führt tatsächlich weiter jq/libjq1 `u3`
aus. Originaldatei und Einstellungen sind bereits hier bytegleich lesbar.

Zwei neue gewöhnliche GNU-Testdateien in `/tmp` und `/run/user/1000` werden vor
dem eigenen Runtime-Stopp erzeugt und anhand ihres Inhalts bestätigt. Der
reguläre Stopp um 15:15:51 UTC entfernt Betas ursprünglichen Init 5565/294246
und die Runtime-Gruppe. Die unabhängige Aufnahme bestätigt Alphas vollständig
unveränderten Kontext 3553/81468, beide Paketauswahlen und CE `[0,10,11]`.
Betas AOSP-Sitzung bleibt gültig; dieser Runtime-Stopp ist kein Logout.

Nach dem bewussten Start führt Beta unter UID/GID 1000 jq und libjq1 `u4` mit
korrektem Ergebnis aus. Alle 81 Paketversionen stimmen mit der vorher
festgehaltenen Erwartung überein; nur jq und libjq1 unterscheiden sich vom
vorherigen Bestand. Die private Auswahlmetadatei enthält keine Festlegung mehr.
Das resultierende persönliche Image verbleibt in Betas CE-Speicher. Der neue
Kontext 7089/553217 besitzt von Alpha getrennte Namespaces; sein Datenbankhash
lautet `6bcb23b8377daad330bdfe6a0c5595da8989ef5c567581223c608cede0fb6f9b`.
Die beiden frisch erzeugten temporären Dateien fehlen, Originaldatei und
Einstellungen sind bytegleich erhalten. Der abschließende Status meldet
`packages=current`.

Der lokale Offline-Verifier bindet alle drei vorherigen Ablehnungen, die
vorab festgelegten Erwartungen, vier vollständige Zustandsaufnahmen, den
gesonderten Prozessabbau und Ereignispräfix 639. Beleg:
`out/phase1-dod/f098f439/private-remove-beta-activation-proof.json`, SHA-256
`0fb703e608d8924a87607351f4fb4f159f4be2430474b0b104935541ed9d7bed`.
Damit sind die vier Autorisierungsvarianten dieser Aktion/Bereich-Kombination
und die private Entfernung mit abweichender gemeinsamer Version nachgewiesen.
Entfernung bei gleicher Version, `update user`, `remove all`, weitere
Fehler-/Parallelitätsfälle und die Gesamtfreigabe bleiben offen.

### Überholte QEMU-Testpaare geordnet beendet

Die weiterhin aktiven Testpaare des Stands `20d7d6d` (ADB 15872) und `209278de`
(ADB 15873) verbrauchten in einer dreisekündigen Stichprobe jeweils etwa zwei
CPU-Kerne. Beide wurden über Androids regulären Shutdown beendet; ihre bereits
laufenden Launcher fuhren anschließend die jeweiligen KeyMint-Helfer sauber
herunter und endeten mit Status 0. Ursprüngliche Prozessidentitäten sind weg,
Profilmanifeste unverändert und beide Datenträger pro Profil weiterhin vorhanden.
Der aktuelle Lauf auf ADB 15874 blieb aktiv. Es wurde kein erzwungenes
Prozesssignal gesendet und kein Profil gelöscht.

Lokaler Beleg: `out/phase1-dod/f098f439/old-qemu-pairs-shutdown-result.json`,
SHA-256 `f9c359474892588f98498b4f1d7c78617ce1d8b3ae6d2af0d8f5564a812b2e72`.
Dies ist die geordnete Freigabe alter Testressourcen, kein zusätzlicher
Persistenznachweis durch spätere Wiederanmeldung in diesen alten Profilen.

### Private Entfernung bei gleicher gemeinsamer Version

Alpha meldet sich im bestehenden dritten Boot regulär an. Die vollständige
Ausgangsaufnahme erhält beide bisherigen Kontexte und Paketbestände; Alpha
besitzt noch die private jq-Festlegung `u4`, während gemeinsam ebenfalls `u4`
installiert ist. Originaldatei und Einstellungen sind bytegleich verfügbar.
Der CLI-Plan zeigt ausdrücklich die Aufhebung dieser privaten Wahl, die Rückkehr
zur gemeinsamen Version `u4` und „Keine Paketversion ändert sich.“

Nach frischer Alpha-Freigabe um 15:35:31 UTC bestätigt die CLI die Veröffentlichung
um 15:36:58 UTC. Ausschließlich Alphas private Auswahl wechselt auf Generation
`0dcb3459ade359d52bd85195ae580f3bfbab9102ee41be1fb7b634070d388760`.
Beide vollständigen laufenden Kontexte bleiben identisch; Alpha führt weiterhin
`u4` aus und meldet ausstehende Aktivierung. Zwei neue temporäre Testdateien
werden unmittelbar vor seinem eigenen Runtime-Stopp erzeugt und gehasht.

Der reguläre Stopp um 15:40:26 UTC entfernt Alphas ursprünglichen Init
3553/81468 und die Runtime-Gruppe. AOSP-Sitzung und CE bleiben erhalten;
Betas vollständiger Kontext 7089/553217 und sämtliche Auswahlen bleiben
unverändert. Nach dem Start führt Alpha unter UID/GID 1000 weiterhin jq/libjq1
`u4` mit korrektem Ergebnis aus. Der neue Kontext 8016/700474 besitzt keine
private Paketfestlegung mehr. Alle 81 Versionen **und die rohe Paketdatenbank**
sind exakt unverändert, SHA-256
`6bcb23b8377daad330bdfe6a0c5595da8989ef5c567581223c608cede0fb6f9b`.
Die neuen temporären Dateien fehlen, ursprüngliche Datei und Einstellungen
sind bytegleich erhalten; der abschließende Status lautet `packages=current`.

Beleg: `out/phase1-dod/f098f439/private-remove-alpha-proof.json`, SHA-256
`790219c2c476e262fee76e7b5feb544a9b338066a3191295cab62d77a14f9bbd`.
Der lokale Verifier bindet Ereignisse ab Index 640 bis Präfix 672, vier
vollständige Zustandsaufnahmen, die vorher festgelegten Erwartungen und den
separaten Prozessabbau. Damit sind private Entfernung und Rückkehr zur
gemeinsamen Variante sowohl bei gleicher als auch bei anderer Version belegt.
Die identische Paketdatenbank ist kein lückenloser Nachweis über sämtliche
eventuellen Paketskriptaufrufe. Andere Pflichtfälle und die Gesamtfreigabe
bleiben offen.

### Privates Update: Ausgangsbestand und Abbruch der Freigabe

Beta installiert jq/libjq1 `u3` über die reguläre CLI mit frischer Adminfreigabe
erneut als privaten Ausgangsbestand. Nach eigenem Runtime-Stopp/Start führt er
diese Version unter UID/GID 1000 aus; Originaldatei und Einstellungen bleiben
erhalten, Alphas Kontext bleibt unverändert. Der lokale Beleg
`private-update-fixture-proof.json` bindet Ereignisse 672 bis Präfix 697 und hat
SHA-256 `8b560cf66a4bbc703c7b971b926ca60ca1ca0a54389992c1eab847568d28e68b`.
Dies ist die Vorbereitung eines echten Updates, kein Update-Erfolg.

Der anschließende Befehl `linux package update --scope user` plant genau den
Wechsel von jq/libjq1 `u3` auf `u4`. Eine leere Adminauswahl bricht den Auftrag
ab. Die anschließende Sitzung bleibt gültig; vollständige Zustandsaufnahmen
vor und nach dem Abbruch stimmen überein. Beleg unter
`out/phase1-dod/f098f439/update-user-cancel-proof.json`, SHA-256
`b50b868a4e14d9f94e1d83fcfe6686c1e8804f187934a5c966175eb1ffeb135c`,
Ereignisse 697 bis Präfix 700. Das belegt nur den Abbruch vor Freigabe;
kein Abbruch während der Ausführung wird daraus abgeleitet.

Auch ein einzelnes falsches Alpha-Passwort und Betas korrektes aktuelles
Passwort ohne Adminrolle verhindern die Veröffentlichung. AOSP weist das
falsche Passwort ausdrücklich zurück. Nach beiden Versuchen bleibt Betas
Sitzung gültig; vollständige Aufnahmen von Identität, CE, ausgewählten
Generationen, laufenden Kontexten und Paketbeständen sind unverändert.
Die missverständliche generische Aufforderung zur erneuten Anmeldung nach
Nicht-Adminfreigabe bleibt als Bedienungsfehler erhalten.

Weitere Belege im selben lokalen Verzeichnis:

- `update-user-wrong-proof.json`, Ereignisse 700 bis Präfix 704, SHA-256
  `5b306c5147db7b697ef6d62b082329ade431e845cd28736b627cc6e363117ba3`.
- `update-user-nonadmin-proof.json`, Ereignisse 704 bis Präfix 707, SHA-256
  `9e9fc7e573fef34902216c38998ab9fa810570b4949b5d98ae673af04e832322`.

Die nachfolgende erfolgreiche private Updateaktivierung ist separat geprüft;
die übrigen Paketvarianten bleiben offen.

### Privates Update: gültige Freigabe und tatsächliche Aktivierung

Der vierte private Updateplan erhält am 3. Oktober um 16:42:12 UTC eine frische
Alpha-Freigabe. Die CLI bestätigt um 16:43:46 UTC die Veröffentlichung.
Ausschließlich Betas ausgewählte private Generation wechselt; beide laufenden
Kontexte, die gemeinsame Auswahl und Alphas private Auswahl bleiben vollständig
unverändert. Beta meldet `activation-pending` und führt weiterhin jq/libjq1
`u3` mit korrektem Ergebnis unter UID/GID 1000 aus. Seine ursprüngliche Datei
und Einstellungen bleiben bytegleich verfügbar.

Betas regulärer Runtime-Stopp um 16:46:24 UTC entfernt den alten Init
8662/811783 und die Kontextgruppe; AOSP-Sitzung und CE bleiben erhalten.
Alphas vollständiger Kontext 8016/700474 bleibt unverändert. Nach dem eigenen
Start führt Beta tatsächlich jq/libjq1 `u4` aus. Der neue Init 10025/1092943
verwendet die private Generation
`64f6efd9c246fea0c66a4859f19e1e82b66e5394de3c3b4ba19a819289a90363`
mit expliziter privater jq-Auswahl `u4`. Genau die beiden geplanten Versionen
ändern sich; alle 81 installierten Pakete sind vollständig geprüft.
Originaldatei und Einstellungen stimmen weiterhin; zwei unmittelbar zuvor
angelegte temporäre Dateien fehlen nach dem Neustart. Der Status lautet
`packages=current`. Alpha bleibt auch in dieser abschließenden Aufnahme
unverändert; Gamma bleibt CE-gesperrt und ohne Kontext.

Beleg: `out/phase1-dod/f098f439/private-update-activation-proof.json`, SHA-256
`a928b7c364beeeb85ecae3a105f3c4e1257c522146f3a51787ccd1d2a7512cc0`.
Er bindet Ereignisse 707 bis Präfix 733, die drei Ablehnungsbelege, vier
vollständige Zustandsaufnahmen, den unabhängigen Prozessabbau und die vorab
festgelegten Erwartungen. Damit ist `update --scope user` einschließlich aller
vier Freigabevarianten und tatsächlicher Aktivierung belegt. Dies ersetzt weder
`remove --scope all` noch die offenen Fehler-/Parallelitätsfälle. Die Aufnahmen
sind einzelne abgeschlossene Beobachtungen, keine lückenlose Überwachung aller
Paketprozesse oder CE-Zugriffe.

### Gemeinsame Entfernung: Abbruch und falsche Freigabe

Nach dem privaten Update beantragt Beta `linux package remove --scope all jq`.
Der Plan entfernt ausschließlich das gemeinsame jq `u4`. Leere Adminauswahl
und ein einzelnes falsches Alpha-Passwort verhindern jeweils die
Veröffentlichung; AOSP bestätigt die Passwortablehnung ausdrücklich.
Betas Sitzung bleibt gültig. Beide vollständigen Kontexte, alle Paketversionen,
gemeinsame und private Auswahlen, CE und Systemidentität bleiben unverändert.

Lokale Belege unter `out/phase1-dod/f098f439/`:

- `remove-all-cancel-proof.json`, Ereignisse 733 bis Präfix 736, SHA-256
  `44aafe8667cd25b6ad7d45af5f74fdf8f5cdd41d81ae657f3a5fa2113a66c1c8`.
- `remove-all-wrong-proof.json`, Ereignisse 736 bis Präfix 739, SHA-256
  `35485f9621a15105c515e9a30f5bd91cc3ab39166227151ad86a3c456e0b0d83`.

Auch Nicht-Adminfreigabe ist anschließend mit unverändertem vollständigem
Zustand geprüft: `remove-all-nonadmin-proof.json`, Ereignisse 739 bis Präfix 742,
SHA-256 `f7b6c35c7374da7ab656c52004c9ff55c2115a4ae964c8fc721fb6c1a155c62f`.
Die Abbrüche vor Freigabe ersetzen keine Ausführungsabbrüche aus T17.

### Gemeinsame Entfernung: Veröffentlichung, Beta erhalten, Alpha-Start fehlgeschlagen

Frische Alpha-Freigabe um 17:12:13 UTC erlaubt die Veröffentlichung der
gemeinsamen jq-Entfernung um 17:13:25 UTC. Beide laufenden Kontexte und privaten
Auswahlen bleiben zunächst unverändert. Betas eigener Stopp/Start gleicht seine
private Generation erfolgreich ab: jq/libjq1 `u4` und libonig5 funktionieren
weiterhin, alle 81 Versionen und seine ausdrückliche private Wahl bleiben
erhalten. Originaldatei und Einstellungen stimmen bytegleich; neue temporäre
Dateien fehlen. Beta meldet `packages=current`; neuer Init 11320/1295664.
Alphas Kontext bleibt währenddessen vollständig unverändert.

Alpha meldet sich regulär an und liest vor seinem Stopp noch jq `u4` sowie die
Originaldaten. Sein geordneter Stopp entfernt Init 8016/700474 und die
Kontextgruppe; Betas vollständiger neuer Kontext bleibt erhalten. Der folgende
Alpha-Start meldet um 17:30:07 UTC jedoch **„Aktion nicht bestätigt“**.
Die anschließenden Statusabfragen zeigen eine gültige AOSP-Sitzung mit
entsperrtem CE, aber `runtime=stopped packages=not-active`. Eine vollständige
Zustandsaufnahme stimmt in allen beobachteten Feldern mit dem Stand vor diesem
Startversuch überein: keine neue private Auswahl, kein Alpha-Kontext, keine
verbliebenen Paketarbeitsprozesse; Beta unverändert. Kein erneuter Startversuch
oder Framework-Neustart wurde zum Verdecken des Fehlers ausgeführt.

Beleg: `out/phase1-dod/f098f439/shared-removal-alpha-failure-proof.json`, SHA-256
`1d5ffac286d8766897491c8962b07afc461108a9ae3012873a27d9b18038eb3f`,
Ereignisse 742 bis Präfix 785. Dies ist ausdrücklich **keine erfolgreiche
gemeinsame Entfernung mit vollständiger Aktivierung**. Der interne Fehlercode
und die Ursache sind in den vorhandenen öffentlichen Ausgaben nicht verfügbar
und noch ungeklärt.

Der zusätzliche native Test
`RuntimeReconciliationPlanning.RemovedSharedRootsExecutePublishAndReopenWithoutPrivateChoice`
erweitert die bisherige reine Planungsprüfung um tatsächliche Ausführung,
Veröffentlichung und erneute Auswahl samt Datei-/Paket- und Konfigurationstest.
Der Test ist in Commit `59752564a98d7eb158dd48d0da3cc6610c288889` ergänzt. Die neue
Gruppe `reconciliation-removal` im Komponententreiber verbindet ihn mit zwei
bestehenden Planungsfällen. Neun Hosttests bestätigen die eingegrenzte
Quellbindung: Nur die benannten Testdateien dürfen abweichen; Produkt-,
Builddefinition- und Inventaränderungen bleiben abgewiesen. Das laufende
Abnahmeimage und seine Produktionshelfer sind unverändert.

Der lokale Komponentenbuild
`identity-20261003T175434Z-59752564-QS2OlS` endet um 18:03:12 UTC erfolgreich
mit `IDENTITY_COMPILED_NOT_INSTALLED`. Der native Test ist damit kompiliert,
aber noch nicht ausgeführt. Die Quellinventare unterscheiden sich ausschließlich
in `runtime/package_planner_test_cases.inc`; alle zehn produktiven Hilfsprogramme
und das Java-Test-APK sind bytegleich mit dem f098f439-Referenzbuild.
Lokaler Beleg: `out/phase1-dod/5975256-removal/native-build-receipt.json`,
SHA-256 `d311e91a93d14fe7b9eaad1872817940aacf46112911e212c75151dac53e05b4`;
Quellinventar `native-sources.json`, SHA-256
`f32abae69f780b561cc2037edbc10c404f9ed5a5e5bcd6d9f791a8009c62c60d`.
Ein gesondertes frisches Android-/KeyMint-Profil wird für den Gasttest gestartet;
das persönliche Abnahmeprofil bleibt erhalten. Kein Build wurde hochgeladen.
Dieser Buildnachweis erklärt oder behebt Alphas Startfehler noch nicht.

Die sechs vorhandenen nativen Auswahlfälle sind anschließend im separaten
Profil `3ce0c9c7-364f-4523-a496-7c785fc2d255` bestanden, ohne übersprungene Fälle.
Boot-ID `e15326ef-e8f3-45f5-b496-1ad2eaca72d6`, SystemServer `1386/37931`,
SELinux Enforcing und ausschließlich System-CE `[0]` stimmen vor und nach dem
Lauf exakt überein. Der Lauf prüft die Auswahl alter/aktueller gemeinsamer und
privater Generationen, die Ablehnung eines unnötigen oder veralteten Abgleichs,
den Rückfall auf die Werksbasis sowie Abbruch und Ressourcenabschluss.
Beleg: `out/phase1-dod/5975256-removal/selection/result.json`, SHA-256
`2c2b21d80782a4422cc27bb0631998ba623ec215976c71b003b32c72e2666723`;
Testlog SHA-256 `c379ea8aa47f3a9f07a1b363897b73a431aede2644659a53f922410382b6ec60`.
Der anschließende Lauf `reconciliation-removal` besteht alle drei Fälle ohne
übersprungene Tests. Insbesondere besteht der neue vollständige Entfernungsfall
mit Veröffentlichung und erneutem Öffnen; die zwei Planungsfälle bestätigen
Entfernung ohne private Wahl beziehungsweise Erhalt einer privaten Wahl samt
Abhängigkeit. Derselbe Systemzustand ist vor/nachher bestätigt. Beleg:
`out/phase1-dod/5975256-removal/reconciliation-removal/result.json`, SHA-256
`cb0f032fb2864ada0d62e1ab7c11d1b14014f3439c7251ffe67261d09f785b75`;
Testlog SHA-256 `f0d92bdfb46bf9912fd6ab45308bf141ef4c07eddbb07a4933f81fdcb0a837e1`.
Alphas tatsächlicher Startfehler ist damit noch nicht reproduziert oder behoben.

Der freigegebene gemeinsame CLI-Plan entfernte nur jq. Die synthetische
Ausgangsvariante entfernt dagegen bereits Anwendung und Bibliothek aus der
gemeinsamen Generation. Deshalb ergänzt
`RemovedSharedRootWithRetainedAutomaticCommonLibraryPublishes` die Variante
mit in der gemeinsamen Basis verbliebener automatischer Bibliothek. Die
private Umgebung ohne private Wahl soll Anwendung und Bibliothek entfernen,
ihre Konfiguration erhalten und die gemeinsame Generation unverändert lassen.
Bei Ergänzung war dieser zusätzliche Test noch nicht kompiliert oder ausgeführt;
der folgende Nachweis hält seinen anschließend abgeschlossenen Lauf fest.
Die neue Gruppe `reconciliation-orphan` prüft ihn zusammen mit dem bisherigen
vollständigen Fall, deren Assertions nun gemeinsam verwendet werden.
`--upload-bundle` erlaubt die ausdrückliche Bereitstellung eines neuen
Testbundles für eine gezielte native Gruppe, ohne die bereits bestandene
Auswahlgruppe allein für den Upload wiederholen zu müssen. Alle vorhandenen
Profil-, Image-, Quell- und Bytegleichheitsprüfungen bleiben erhalten;
vorhandene Bundle-Ziele werden weiterhin abgewiesen. Neun Hosttests bestehen.

Der Komponentenbuild `identity-20261003T185327Z-c1663c3f-AtF89H` aus Commit
`c1663c3fd1d50b31bbabd6c62fad42f2a0b45976` endet erfolgreich. Beleg
`out/phase1-dod/c1663c3-orphan/native-build-receipt.json`, SHA-256
`99bee6600a632573ce2b7719f06f198416625f960ea43d167927593edb4d4ec9`.
Nur die Testquelldatei unterscheidet sich vom Referenzstand; alle zehn
Produkthelfer und das Java-Test-APK bleiben bytegleich.

Die Gruppe `reconciliation-orphan` besteht danach **beide Tests ohne
übersprungene Fälle** im gleichen separaten Systemprofil. Der ursprüngliche
vollständige Fall besteht nach gemeinsamer Nutzung der Assertions erneut;
auch die verbliebene automatische Bibliothek verhindert den vollständigen
synthetischen Abgleich nicht. Boot-ID, SystemServer, SELinux Enforcing und
System-CE `[0]` sind vor/nachher identisch. Beleg
`out/phase1-dod/c1663c3-orphan/tests/result.json`, SHA-256
`78a55279e744c864b1d1a85309633b2bbc8d181fd7d88b4018d594cebe63b0b4`;
Testlog SHA-256 `171a3fae4e29cbd3f93f5ac4ba983160a3878e30602e2bf75ad205d49b0e4f42`.
**Alphas tatsächlicher CLI-Fehler bleibt ungeklärt und ist nicht behoben.**

### Begrenzte Diagnose für fehlgeschlagene Runtime-Starts

Der Broker protokolliert beim erstmaligen Übergang eines vorhandenen
Startvorgangs in den Fehlerzustand die feste Startphase und den numerischen
Fehlercode. In Planung/Ausführung kommen der numerische Worker-Zustand,
Prozessstatus und Worker-Fehler hinzu. Wiederholte Abfragen desselben bereits
fehlgeschlagenen Vorgangs erzeugen keine erneute Meldung. Planner-Freitext,
Identitäten, Paketwahl, Pfade und Zugangsdaten werden nicht ausgegeben.
Fehlerrückgabe, Autorisierung und Ressourcenabbau bleiben unverändert.

Diese Diagnose ist zunächst eine Quelländerung, noch kein gebautes oder im
Gast geprüftes Produkt. Sie erklärt Alphas früheren Fehler nicht rückwirkend.
Ein neues passendes Image und ein regulärer CLI-Versuch in einem gesonderten
Profil müssen den tatsächlichen Fehlerpfad erst sichtbar machen. Das laufende
persönliche Abnahmeprofil und seine bisherigen Belege bleiben erhalten.

Die Gruppe `start-failure` im bestehenden Komponententreiber wählt dafür drei
vorhandene Tests: Ein fehlgeschlagener Selector bleibt fehlgeschlagen;
Fortsetzungen können gestoppte/ersetzte Starts nicht wiederherstellen; auch
ein Stopp ohne Wartebudget hinterlässt keinen fortsetzbaren Start. Die Gruppe
ist vorbereitet, aber noch nicht auf dem Diagnoseimage ausgeführt. Weil die
Diagnose Produktcode ändert, gilt die Ausnahme für reine Testquelländerungen
hier ausdrücklich nicht; die bestehende Image-/Quellprüfung bleibt erforderlich.

Der Diagnosecommit `3fdb058310b75922b17819d47c985bc4eecd09c4` ist anschließend
im Komponentenlauf `identity-20261003T190854Z-3fdb0583-Ba2NhF` erfolgreich
kompiliert (`IDENTITY_COMPILED_NOT_INSTALLED`). Der lokal gesicherte Beleg
`out/phase1-dod/3fdb058-diagnostics/native-build-receipt.json` hat SHA-256
`c7736352d9f7ff0e1e721c6c7e403f7c5117e0d0a86d5a713825879f14c2bab2`.
Gegenüber dem f098f439-Inventar unterscheiden sich `Android.bp`,
`runtime/broker_owner.cpp` und die neue Testdatei; alle zehn Hilfsprogramme
im Testbundle bleiben bytegleich. Das bedeutet ausdrücklich nicht, dass der
geänderte Broker bereits im laufenden Abnahmeimage enthalten wäre.

Das separate System-Testprofil `3ce0c9c7-364f-4523-a496-7c785fc2d255`
ist nach Abschluss seiner Tests regulär heruntergefahren: Android bestätigt
`Power down`, der KeyMint-Helfer seinen sauberen Abschluss, der Launcher
Exitcode 0. Profil und Belege bleiben erhalten. Lokaler Abschaltbeleg
`out/phase1-dod/3fdb058-diagnostics/previous-test-profile-shutdown.json`,
SHA-256 `d01387968db006c71c8c6e2b56a2db2900a9b5705c6248f6288d107a6ef9f48c`.

Der lokale Vollbuild `local-20261003T191808Z-3fdb0583-RnRAcC` ist mit demselben
Quellcommit und den vorhandenen verifizierten Kernel-/Runtime-Eingaben
gestartet. Bei diesem Eintrag läuft er noch; ein neues gebootetes Image und
die tatsächliche CLI-Reproduktion sind damit weiterhin ausstehend.
