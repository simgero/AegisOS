## Drei geprüfte Generationsansichten: 0fb41856

Der [Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20261001T051630Z-0fb41856-0fb41856-IEaABz)
von `0fb418568c4f35e4b47b81b59637f51910fd5cca` besteht am
**2026-10-01T05:21:19Z alle 302 ausgewählten nativen Tests** in 27 Suiten
im lokalen Mac-QEMU (221.270 ms). Entwicklung lokal, Build auf `aegis-build`,
Transport über GitHub. Die unveränderte Java-Test-App ist byte-identisch zum
unten dokumentierten 54-Test-Nachweis aus `f17a043c`.

Der interne Selektor liefert genau drei schreibgeschützte, nicht ausführbare
Ansichten: private Generation, ihre bisherige gemeinsame Basis und den aktuellen
gemeinsamen Stand. Eine ältere gemeinsame Image-Datei wird anhand ihres
vollständigen Inhalts, Eigentümers, Modus und ihrer Dateidentität erneut geprüft.
Die gepinnte Werksbasis kann ebenfalls die frühere Basis sein. Fehlende oder
veränderte Images führen zu keinem Ersatz durch eine andere Generation.
Metadaten und alle drei Deskriptoren werden erst nach vollständiger Prüfung
übertragen. Abbruch wartet auf das Prozessende und schließt auch bereits
übertragene Ansichten; die bisherige Store-Auswahl bleibt unverändert.

Belege: `out/components-0fb41856/targeted-tests/`, `java-proof-reuse.json`.
Native Log-SHA256: `f19fe7aba4fe9e104fb36f01e57fdb12a57c806f9608186418c4a811e437962e`.
Vorher/Nachher identisch: `7e613ad413f79888442b5ee6fa97976c33c2b9f188901a29451bb06ca60231a5`.
Die sieben neuen Selektorprüfungen verwenden tatsächliche synthetische
Installations-/Updateimages; vier Store-Prüfungen untersuchen historische Images.

**Grenzen:** Dies ist die Eingabestufe der Zusammenführung. Sie löst noch keine
Abhängigkeiten auf, veröffentlicht keine zusammengeführte Generation und macht
einen Start mit veralteter privater Basis nicht zulässig. Das installierte
Vollimage bleibt `d0b866e1`; die vier echten CE-Fixtures bleiben deaktiviert.
Die aktuelle Entwicklungsänderung erweitert als nächsten Schritt den isolierten
Planer um diese drei Ansichten und explizite Paketziele. Ihre neue Testsuite ist
hier noch nicht als bestanden ausgewiesen. Bindung, Ausführung, frische
Autorisierung bzw. abgeleitete Aktivierungsberechtigung und Runtime-Start müssen
anschließend durchgehend verbunden und im tatsächlichen Produkt nachgewiesen werden.

## Explizite private Auswahl einer installierten Version: 2c7bf728

Der [Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20261001T045927Z-2c7bf728-2c7bf728-afFBQL)
von `2c7bf7284c2a5cd22702f9bb96d3a38d964a048d` besteht am
**2026-10-01T05:03:27Z alle 291 ausgewählten nativen Tests** aus 26 Suiten
im lokalen Mac-QEMU (178.968 ms), ohne übersprungene Tests. Entwicklung lokal
im Worktree, Kompilierung auf `aegis-build`, Transport über GitHub.

Eine unveränderte Installation für den privaten Bereich kann jetzt eine
bewusste private Auswahl erzeugen. Der tatsächliche APT-No-op liefert eine
leere Änderungsliste; AEGIS liest Architektur und exakte Version zusätzlich
aus der eingefrorenen, vollständig geprüften Paketdatenbank. Der daraus
abgeleitete Effekt bindet alten/neuen Auswahlbestand, Ausgangsstatus und
Manual-/Automatic-Markierungen in den normalen Freigabeplan. Die CLI zeigt
„privat festhalten, Version … unverändert“. Die Aktion verlangt weiterhin
die normale Adminfreigabe; sie ist keine Installation ohne Autorisierung.

Der neue Ausführungstyp führt weder eine erneute Installation noch
Paket-Skripte aus. Er kontrolliert den Ausgangsbestand unabhängig, markiert
das Paket manuell, prüft den vollständigen Paketbestand und schreibt die
private Auswahl atomar vor der normalen Veröffentlichung. Der tatsächliche
Executor-Test bestätigt unveränderte Paketdateien, Status, Installationsskript-
Protokoll und persönliche Konfiguration sowie persistente Auswahl nach
erneutem Mounten. Ein Hold bleibt erhalten. Manipulierte Registries, falsche
Version/Architektur, unzulässiger Bereich, Replay und fehlende Legacy-Manifeste
werden zurückgewiesen. Gleiche Versionen in einem behaupteten APT-Paketeffekt
berechtigen ausdrücklich nicht zum Überspringen von Archiv-/Skriptprüfungen.

Die beiden vorherigen Planner-Fehler wurden durch Testdaten ausgelöst:
`arm64` im installierten Status passte nicht zum signierten `all`-Archiv,
wodurch APT einen Architekturwechsel plante. Mit korrekten Daten ist der Hook
leer. Eine dafür zwischenzeitlich ergänzte Sonderbehandlung wurde wieder
entfernt. Im anschließenden Lauf `24441e18` bestand dieser Test; eine zeitweise
unerreichbare Debian-Sicherheitsquelle stoppte den Gesamtlauf. Der hier
berichtete Folgelauf prüfte auch den echten HTTPS-/Signaturabruf erfolgreich.

**54 Android-Instrumentierungstests** für Paketprotokoll (18), Transaktion
(14), Freigabe (14) und CLI (8) bestanden bereits mit `f17a043c` am
2026-10-01T04:39:53Z. Das jetzige Test-APK ist nachweislich byte-identisch
(`27a753132d1d286f62cbb3128d67c9ea40b62fc283fba6fc0214e4b117517c4c`); Quelllog und Nachweis
wurden geprüft, die unveränderten Tests nicht wiederholt.

Belege: `out/components-2c7bf728/targeted-tests/` und
`out/components-2c7bf728/java-proof-reuse.json`, ursprünglicher Java-Nachweis
`out/components-f17a043c/java-tests/`. Native Log-SHA256:
`856761ff260b0f2c4b52be5cd30744bf612504efdb1569d8528d56e1400672af`.
Vorher-/Nachherzustand identisch:
`7e613ad413f79888442b5ee6fa97976c33c2b9f188901a29451bb06ca60231a5`.
Boot-ID `18880c8d-4132-48e9-a4de-9bb2823599a6`; Alpha/Beta gestoppt und
CE-gesperrt, keine persönlichen Kontexte, SELinux Enforcing, echter Broker aktiv.
Drei abgeschlossene fehlerhafte synthetische Testimages wurden nach Prüfung
auf Prozess-/Loop-Freiheit entfernt; Logs und Prüfsummen bleiben erhalten.

**Grenzen:** Das installierte Vollimage bleibt `d0b866e1`. Synthetische native
Fixtures und Java-Freigabetests beweisen noch keine tatsächliche neue
AOSP-Passwortfreigabe im installierten Produkt. Vier echte CE-Fixtures bleiben
deaktiviert. Die Zusammenführung neuer gemeinsamer Generationen mit privaten
Versionen, Abhängigkeiten, Konfigurationen und technischen Kennungen,
ausgewiesene Rückkehr zur gemeinsamen Variante nach privater Entfernung,
ausstehende Aktivierung, Konflikt-/Reparaturanzeige und vollständige
Produktabnahme bleiben offen. Die private Beta-Generation mit alter
Gemeinschaftsbasis ist weiterhin kein erfolgreicher Reconciliation-Nachweis.

## Dauerhafte private Paketauswahl: 1fb194c9

Der [Komponentenbuild](https://github.com/simgero/AegisOS/releases/tag/components-20261001T040359Z-1fb194c9-1fb194c9-cF0wUJ)
von `1fb194c9f9454d8eb48cbefb1113c998e47d1489` besteht am
**2026-10-01T04:07:53Z alle 280 ausgewählten nativen Tests** in 26 Suiten
im bestehenden lokalen d0-QEMU-Gast. Entwicklung erfolgte im lokalen Worktree,
Kompilierung auf `aegis-build`, Transport ausschließlich über GitHub.

Eine kanonische, begrenzte Auswahl aus Paketname, Architektur und Version wird
unter `var/lib/aegis/private-choices` innerhalb der privaten Imagegeneration
geführt. Der Planer kopiert sie aus dem schreibgeschützten ausgewählten Image;
Review und Plan-Digest binden alten und neuen Inhalt. Der Ausführungshelfer
prüft beide gegen den vollständigen installierten Paketbestand und schreibt
den neuen Inhalt erst nach erfolgreicher Paketprüfung und Beendigung aller
Paketkinder atomar mit `fsync`. Das Verzeichnis ist `root:root 0700`, die Datei
`0600`. Veröffentlichung und Hash des Gesamtimages umfassen damit auch die
Auswahl. Abhängigkeiten werden nicht allein aufgrund ihrer Installation als
bewusste private Auswahl erfasst; vorhandene Auswahl bleibt bei unabhängigen
Änderungen erhalten. Entfernte gewählte Pakete verschwinden aus der Auswahl.
Eine bestehende private Generation ohne Auswahlmanifest wird ausdrücklich
abgewiesen; ihre Absicht wird nicht aus dem Paketbestand erraten.

Neu geprüft wurden Codec/Framing und Grenzen, Bindung an den Digest,
Übertragung durch den tatsächlichen isolierten Planer, exakte endgültige
Versionszuordnung, Dateirechte und erneutes Öffnen, Symlink-Ablehnung sowie
ein echtes APT-Installationsskript, das die Auswahl zu fälschen versucht.
Der APT-Lebenszyklustest installiert Version 1, aktualisiert auf Version 2 und
entfernt das Paket; nach jedem Schritt wird das Image erneut gemountet und
die erwartete Auswahl gelesen. Eine persönlich geänderte Konfigurationsdatei
bleibt erhalten. Die übrigen betroffenen Planungs-, Ausführungs-, Auswahl-,
Abbruch-, Veröffentlichungs- und Transporttests bestehen ebenfalls.

Der vorherige Lauf `15b80b98` deckte die noch auf 64 KiB begrenzte
Memfd-Übergabe auf. Die erweiterte Beschreibung wurde vor Ausführung mit
`EPERM` abgewiesen. Die gemeinsame Grenze beträgt jetzt begrenzte 128 KiB;
ein neuer Test prüft die tatsächliche Request-Größe, den Grenzwert,
die Ablehnung darüber sowie unverändert schreibgeschützte, versiegelte
Deskriptoren. Der fehlgeschlagene Lauf behielt synthetische Testimages,
füllte dadurch den Gast und verursachte Folgefehler. Seine Nachweise bleiben
erhalten; die 26 nachweislich unbenutzten Testverzeichnisse wurden entfernt.
Der erfolgreiche Folgelauf verwendet Abbruch beim ersten Fehler.

Belege: `out/components-1fb194c9/targeted-tests/{result.json,native.log,before.json,after.json}`.
Log-SHA256: `f9a3e794d7dd1e66c49d352a610bd5bc353bdf172c33e76c7d92c6cb5d85a175`.
Vorher/Nachher identisch:
`7e613ad413f79888442b5ee6fa97976c33c2b9f188901a29451bb06ca60231a5`.
Boot-ID `18880c8d-4132-48e9-a4de-9bb2823599a6`, Benutzer Alpha/Beta
gestoppt und CE-gesperrt, keine persönlichen Runtime-Kontexte, SELinux
Enforcing und echter Broker weiterhin laufend. Die Java-Test-APK ist
byte-identisch zum unten dokumentierten 51-Test-Lauf (`7aafca8b7f749ea9637b0c85d5af5c53ea48ed874f7c9d147a2eebcf76bd2014`);
die unveränderten Java-Tests wurden nicht wiederholt.

**Grenzen:** Dies sind Komponentenprüfungen mit synthetischen Paketimages;
das installierte Produkt bleibt d0. Vier deaktivierte echte CE-Fixtures
bleiben deaktiviert. Der Codec kann eine explizite Auswahl gleicher Version
repräsentieren; der öffentliche Planungs-/Ausführungsweg verwirft einen
solchen No-op weiterhin. Dieser Weg, die Zusammenführung neuer gemeinsamer
Generationen mit privaten Versionen und Konfigurationen, erklärte Konflikte
und ausstehende Aktivierung sowie die vollständige Produktabnahme bleiben
umzusetzen. Der reproduzierte Fehler der privaten Beta-Generation nach dem
gemeinsamen Update ist durch diesen Komponentenlauf noch nicht behoben.

## Java-Freigabe gemischter Paketpläne: 73b2c062

Der [Komponentenbuild](https://github.com/simgero/AegisOS/releases/tag/components-20261001T032720Z-73b2c062-73b2c062-0xzmN8)
von `73b2c062bdd69b3e24a8358a835ae0b2f03bb2ba` korrigiert eine zusätzliche
Integrationslücke nach dem nativen 130-Test-Nachweis: Die Java-Metadatenprüfung
hatte bisher für sämtliche Effekte die Richtung der angeforderten Aktion verlangt.
Sie akzeptiert nun vollständige gemischte Installations-/Entfernungspläne,
prüft die angeforderte Richtung und exakte Version weiterhin am Zielpaket und
verlangt für jeden Installationseffekt eine Repository-Gültigkeit, auch bei
einer angeforderten Entfernung. Sortierung, Eindeutigkeit und unveränderliche
vollständige Reviews bleiben erhalten. Die Freigabeansicht überträgt alle Effekte
und behält Antragsteller und Plan-Digest; eine veränderte UI-Kopie ändert den Plan nicht.

Auf `aegis-build` gebaut und über GitHub mit Prüfsummen übertragen. Am
**2026-10-01T03:28:11Z** bestanden im bestehenden lokalen d0-QEMU-Gast
**51/51 Android-Instrumentierungstests**: `PackageBrokerProtocolTest` 16,
`PackageTransactionTest` 13, `PackageApprovalTest` 14 und `PackageCommandTest` 8.
Darunter sind sieben neue Regressionsfälle für gemischte Effekte, falsche
Zielrichtung/Version, fehlende Gültigkeit sowie die vollständige Freigabeansicht.

Belege: `out/components-73b2c062/java-tests/result.json`, `java.log`,
`before.json` und `after.json`. Log-SHA256:
`d472c386a29ad76e1dba996b9dd470fe7b8a33234cbcdde036ffb72d9227bb9c`.
Beide Zustandssnapshots sind identisch:
`7e613ad413f79888442b5ee6fa97976c33c2b9f188901a29451bb06ca60231a5`.
Boot-ID bleibt `18880c8d-4132-48e9-a4de-9bb2823599a6`; persönliche Benutzer
bleiben gestoppt/CE-gesperrt, Runtime-Kontexte leer und SELinux Enforcing.

**Grenze:** Ausgeführt wurde die Test-APK mit Protokoll-/Koordinator-Fixtures.
Das installierte Produktimage bleibt d0; dies ist kein neuer Nachweis tatsächlicher
AOSP-Passwortfreigabe oder produktiver gemischter Paketinstallation. Die unten
reproduzierte fehlende gemeinsame/private Zusammenführung ist weiterhin offen.
Nach diesem Ersatznachweis wurden 75 obsolete Komponentendateien aus d0,
592866d4 und 7fc2f44a entfernt (424796160 Byte frei geworden). Ihre Metadaten,
Prüfsummen und Testbelege sowie alle drei QEMU-Profile bleiben erhalten.

## Gemischte Pakettransaktionen: 7fc2f44a

Commit `7fc2f44a2e5f8fb4df6b91082ee9db825fd51de7` wurde ausschließlich auf
`aegis-build` kompiliert (Run `identity-20261001T030745Z-7fc2f44a-Cj4uyi`,
Invocation `2cb85f3ef84843d8a0ba3484d6385d7c`). Die
[Komponenten](https://github.com/simgero/AegisOS/releases/tag/components-20261001T030913Z-7fc2f44a-7fc2f44a-yNDT73)
wurden über GitHub verifiziert auf den Mac übertragen.

**130/130 native Prüfungen bestanden**, am **2026-10-01T03:12:14Z**, Laufzeit
103,612 Sekunden. Testgast: Vollimage `d0b866e1`, Profil
`ad828f99-7021-47b9-8645-b9aef2693651`, Boot
`18880c8d-4132-48e9-a4de-9bb2823599a6`, Enforcing und authentifiziertes ADB.
Die neun Suites umfassen 23 Planbindungen, 21 Archivbelege, 19 Ausführungswächter,
zwei mechanische Planprüfungen, 28 Ausführungs-, 15 Vorbereitungs-, zwölf
Transaktions- und zehn Generationsauswahltests.

Die neue vollständige Transaktion installiert zuerst App/Bibliothek Version 1.
Ein zweiter gebundener Vorgang entfernt die App und aktualisiert gleichzeitig
die Bibliothek auf Version 2. Die Entfernung steht vor dem Archiv in der
sortierten Effektliste, während nur ein Archiv-FD übertragen wird. Tatsächliches
APT führt genau beide freigegebenen Effekte aus; die neue Generation lässt sich
über den Store erneut öffnen. Konfigurationsdatei und technische Dateieigentümer
bleiben erhalten, Programmdatei/Paketstatus und automatische Markierung passen
zum Ergebnis. Ein falsches Archiv scheitert bei der Vorbereitung und lässt die
vorherige Generation ausgewählt. Die Wächter verwerfen fehlende oder veränderte
Entfernungseffekte, falsche Suchbegriffe, fehlende Reviews und eingeschleuste FDs.

Benutzerregister, Benutzer-/Seriennummern, CE-/DE-Schlüsselkennungen, gestartete
Benutzer und Runtime-Kontexte sind vor/nach allen Prüfungen bytegleich. Beide
persönlichen Benutzer bleiben CE-gesperrt. Die vier deaktivierten nativen
Real-CE-Fälle wurden nicht aktiviert. Java wurde nicht verändert; vorhandene
Java-Nachweise werden nicht als neue Ausführung gezählt.

Lokale Belege: `out/components-7fc2f44a/targeted-tests/result.json`, `before.json`,
`after.json`, `native.log`. SHA-256 des Logs:
`4c55ce32741cf7bcd9e4a881e1c3ec22486c18bf63589a8fa3054f57f64383af`.
Vor-/Nachzustand: `7e613ad413f79888442b5ee6fa97976c33c2b9f188901a29451bb06ca60231a5`.

**Grenze:** Die neuen Fälle verwenden eigene synthetische Images und den
Entwickler-Testeinstieg. Sie beweisen keine installierte produktive Ausführung
dieses Commits, keine neue AOSP-Passwortfreigabe und noch keine automatische
Zusammenführung gemeinsamer und privater Pakete. Die reproduzierte d0-Sperre
unten besteht weiterhin. Dafür fehlen insbesondere dauerhaft gespeicherte
private Paketentscheidungen, konsistente Auflösung gegen eine neue gemeinsame
Basis, Aktivierungsstatus und Konflikterklärung. Der bisherige Schutz gegen
eine veraltete private Basis bleibt wirksam.

## Private Pakete, Zwei-Benutzer-Neustart und gemeinsamer Bestand: d0b866e1

Der [Vollbuild](https://github.com/simgero/AegisOS/releases/tag/aosp-20261001T014320Z-d0b866e1-5ffa8331)
mit Commit `d0b866e113e4d40a403c7182aa68732b85221fcb` wurde auf `aegis-build`
gebaut, über GitHub verifiziert übertragen und am **2026-10-01T02:08:44Z**
lokal gestartet. Enforcing, FBE, dm-verity, authentifiziertes ADB und die
bytegleichen vier Pakethelfer aus dem 28-Test-Komponentenlauf sind bestätigt.

Die tatsächliche CLI verweigert fehlende Anmeldung, falsches Adminpasswort
und Freigabe durch Nichtadmin Beta ohne Veröffentlichung. Frische Alpha-
Adminfreigabe veröffentlicht um **02:13:35 UTC** `hello` 2.10-5 nur für Beta
(user/serial 11/11); Alpha (10/10) bleibt dabei CE-gesperrt. Nach Betas Runtime-
Neustart laufen das echte Programm und die dpkg-Abfrage unter UID 1000 in
`aegis_runtime_program`. Alpha sieht dieses private Paket nicht.

Beide Benutzer schreiben eigene Dateien aus ihrer tatsächlichen GNU-Shell.
Dateizugriffe und SIGSTOP auf den jeweils anderen Benutzer werden in beiden
Richtungen abgewiesen; die ursprünglichen fremden Prozesse sind davor und
danach nachweislich aktiv und machen Fortschritt. Alle sechs Namespace-
Kennungen und Host-UIDs unterscheiden sich. Bei jedem Logout endet der
ursprüngliche Hintergrundprozess vor seinem natürlichen Ablauf, der Kontext
wird entfernt und AOSP meldet CE gesperrt. Die zuvor geschriebenen Dateien
sind dann nicht lesbar. Resize und Unterbrechung eines Vordergrundprogramms
wurden ebenfalls geprüft. Das ist keine vollständige Syscall-Isolationsprüfung.

Android und KeyMint wurden sauber beendet und mit denselben beiden
Datei-Inodes neu gestartet. Boot-ID vorher:
`3af2980f-f6c9-4b82-9896-af8e34863aa7`, danach:
`18880c8d-4132-48e9-a4de-9bb2823599a6` (**02:32:14 UTC**).
Beide persönlichen CE-Speicher sind zunächst gesperrt. Falsche Passwörter
entsperren keinen Benutzer; frische korrekte Anmeldungen stellen beide
GNU-Dateien bytegenau wieder her. Betas `hello` bleibt ausführbar und für
Alpha unsichtbar. Ein Passwortwechsel über AOSP weist anschließend das alte
Passwort ab; das neue erhält Betas Datei und Programm. In sechs Bootprotokollen
wurden keine vollständigen Testpasswörter gefunden; Passwörter liegen nur im
lebenden Testtreiber, nicht in Belegdateien oder Kommandoargumenten.

**Reproduzierbar offen:** Frische Adminfreigabe veröffentlicht anschließend
`ed` 1.21.1-1 gemeinsam, während Beta CE-gesperrt bleibt. Alphas bereits
laufender Kontext behält seinen alten Bestand; nach einem Runtime-Neustart
führt Alpha `ed` aus und behält seine HOME-Datei. Beta kann sich anmelden,
aber seine vorhandene private Generation referenziert noch die alte gemeinsame
Basis. Sein Runtime-Start und auch `package update --user` scheitern. Die
Prüfsummen beider gespeicherten Images stimmen weiterhin mit ihren Auswahlen
überein. Der Startschutz in `package_prepare_worker.cpp` und die Planbindung
in `package_plan.cpp` weisen diese unterschiedliche Basis ab; eine konsistente
Zusammenführung fehlt. `linux status` zeigt zudem noch keine ausstehende
Aktivierung an. Allgemeine Fehlertexte ersetzen keine Konflikterklärung.
Dieser abgewiesene Start erfüllt die gewünschte gemeinsame/private Aktualisierung
nicht. Beide Benutzer sind nach dem Fall gestoppt/CE-gesperrt, alle Kontexte
und Pakethelfer beendet. Die gespeicherten Generationen bleiben erhalten.

Belege: `out/full-build-d0b866e1/identity-test/package-install-proof.json`,
`persistence-proof.json`, `shared-reconciliation-proof.json`, deren jeweilige
unveränderte Ereignis-Snapshots und die referenzierten Vorher-/Nachher-Dateien.
Die neue kanonische CLI-Schreibweise ist in diesem d0-Vollimage noch nicht
installiert. Gemeinsame/private Zusammenführung, abweichende private Versionen,
Entfernungs-/Aktualisierungseffekte, Aktivierungsstatus und vollständige
Benutzer-Lebenszyklus-/Geräteabnahme bleiben Teil des offenen Gesamtziels.

## Vollständige Paketdateien: Vollimage 8168cf7f

Der [Vollbuild](https://github.com/simgero/AegisOS/releases/tag/aosp-20261001T004850Z-8168cf7f-9bdfd663)
bootet am **2026-10-01T01:12:21Z** lokal mit Enforcing, FBE, dm-verity und
authentifiziertem ADB; Boot-ID `fbd52d89-778d-4be8-b7b5-377d16a865eb`.
Die vier installierten Pakethelfer sind bytegleich mit dem b7-Testkern.
Die echte CLI verweigert fehlende Anmeldung, falsches Adminpasswort und
Freigabe durch Nichtadmin Beta. Aufnahmen des korrekten persönlichen Pfads
`packages/store/current` bestätigen unveränderte Auswahlen; Alpha bleibt gesperrt.

Gültige Alpha-Freigaben führen jetzt `hello` 2.10-5 bis einschließlich Einrichtung,
APT-Abhängigkeitsprüfung und leerem dpkg-Audit aus. Die abschließende
Dateiprüfung lehnt jedoch neu fehlende Dokumentations-/Übersetzungsdateien ab.
Das in der realen GNU-Shell gelesene `/etc/dpkg/dpkg.cfg.d/docker` enthält
die geerbten Slim-Ausschlussregeln. Die vorherige `status-old`-Störung ist behoben.
Es gibt noch keine veröffentlichte Generation oder erfolgreiche `hello`-Ausführung.

Lesende Aufnahmen ausschließlich der synthetischen Kandidaten 6 und 7 erhalten
die Installations- und Prüfprotokolle. Zusatzdeskriptoren sind geschlossen und
temporäre Imagekopien gelöscht. Das ist Diagnose, kein Isolationsbeleg.
Belege liegen unter `out/full-build-8168cf7f/identity-test/`, insbesondere
`events.json`, den Vorher-/Nachher-Aufnahmen und `diagnostic-copy-7.json`.
Ein einzelner Planversuch scheiterte vorher schon in der Update-Phase; ein
neuer normaler Aufruf erreichte wieder die Freigabe. Dessen Ursache ist unbewiesen.

Die Korrektur ergänzt beim kontrollierten dpkg-Aufruf fest
`--path-include=/*`, damit neue Pakete vollständig entpackt werden. Die
[dpkg-Regelreihenfolge](https://manpages.debian.org/trixie/dpkg/dpkg.1.en.html)
wertet die letzte passende Regel aus. Die vollständige Vorher-/Nachher-Prüfung
bleibt bestehen. Zwei neue Ausführungsfälle prüfen das wirkliche Installieren
von Dokumentation/Übersetzungen trotz geerbter Filter sowie das weiterhin
abgewiesene Entfernen einer neuen Dokumentationsdatei durch ein Paketskript.
Commit `d0b866e113e4d40a403c7182aa68732b85221fcb` wurde auf `aegis-build`
kompiliert und über den Release
`components-20261001T013517Z-d0b866e1-d0b866e1-SrpQDF` transportiert.
Am **2026-10-01T01:36:28Z** sind im unveränderten lokalen 8168-Gast alle
**28 Fälle von `RuntimePackageExecutor`** bestanden, einschließlich beider
neuer Fälle. Die Prüfung dauerte 33,932 Sekunden; Nutzer, Schlüsselkennungen,
CE-Zustand und Runtime-Kontexte stimmen davor/danach überein.
Beleg: `out/components-d0b866e1/targeted-tests/result.json` und `native.log`.
Das sind synthetische SU-Ausführungsfälle; der produktive CLI-Test steht aus.
Die vier deaktivierten CE-Fälle bleiben deaktiviert. Java wurde nicht verändert;
die frühere 152-Test-Evidenz wurde nicht als neuer Lauf ausgegeben.

Die im Phase-1-Auftrag verlangte Schreibweise
`linux package install NAME[=VERSION] --scope user|all` wird anschließend
über denselben bestehenden Paket-/AOSP-Freigabeweg ergänzt. Auch `update`,
`remove` und die Auftragssteuerung werden dort angeboten. Fehlende, doppelte,
widersprüchliche oder unbekannte Bereiche sowie zusätzliche Eigentümeroptionen
werden vor dem Dienstaufruf abgewiesen. Commit
`592866d4c1f6ca4d8cbd0e99698d121d08bd22e8` wurde auf dem Builder kompiliert;
der [Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20261001T021040Z-592866d4-592866d4-FhX4NP)
wurde über GitHub verifiziert empfangen. Am **2026-10-01T02:30:46Z** bestehen
im lokalen d0-Gast alle **acht `PackageCommandTest`-Fälle**. Persönliche Benutzer,
CE-/DE-Schlüsselkennungen und Runtime-Kontexte sind davor/danach unverändert;
die persönlichen CE-Speicher bleiben gesperrt. Beleg:
`out/components-592866d4/java-tests/result.json` mit `java.log` und beiden
Zustandsaufnahmen. Das ist ein Parser-Nachweis; die neue installierte öffentliche
CLI benötigt noch ein passendes Vollimage. Der oben dokumentierte tatsächliche
Neustartablauf verwendet die vorherige CLI-Schreibweise. Die gemeinsame/private
Zusammenführung wird durch diese Parser-Ergänzung nicht behoben.

## Produktiver dpkg-Start: Vollimage adee7ad7

Der [Vollbuild](https://github.com/simgero/AegisOS/releases/tag/aosp-20261001T000816Z-adee7ad7-a2241988)
mit Commit `adee7ad70a6731d89962f9cabf4706f388e40f37` ist auf `aegis-build`
gebaut und über GitHub rückverglichen. Der lokale QEMU-Boot ist am
**2026-10-01T00:31:25Z** mit Enforcing, FBE, dm-verity und authentifiziertem ADB
geprüft; Boot-ID `f61f99fc-6fdf-48e4-9ae6-c9c25efdffa6`. Die vier Pakethelfer
sind bytegleich mit dem separat durch 62 native Tests geprüften b7-Kern.
Die unveränderten SU-Suiten wurden für diese Policy-Änderung nicht wiederholt.

Die installierte CLI erstellt Alpha/Beta und verweigert einen nicht angemeldeten
Aufruf, ein falsches Adminpasswort sowie die Freigabe durch den Nichtadmin Beta.
Korrekte frische Alpha-Freigaben um 00:35:15 und 00:40:35 UTC erreichen jetzt
APT und dpkg in deren produktiver Domäne. Die private PTY-Prüfung ist passiert.
dpkg entpackt `hello`, scheitert aber an der Hardlink-Sicherung `status-old`.
Es gibt weiterhin keine veröffentlichte Generation und keinen erfolgreichen
Programmlauf. Alpha bleibt CE-gesperrt. Nach regulärem Logout von Beta sind
beide Testkonten gestoppt/gesperrt und sämtliche Runtime-Kontexte entfernt.

Das Audit-Limit verwirft weitere Meldungen; die sichtbare FIEMAP-Verweigerung
ist laut dpkg-Quelltext nicht fatal. Eine rein lesende Entwicklungsroot-Aufnahme
des zweiten synthetischen Kandidaten bis nach dessen normalem Unlink erhält
das genaue dpkg-Protokoll. Der zusätzliche Deskriptor ist vor Logout geschlossen,
temporäre Imagekopien sind gelöscht. Das ist Diagnose, kein Produkt-Isolationsbeleg.
Die enge Korrektur erlaubt nur dem Paketprogramm `file:link` auf seinem
schreibbaren Kandidatentyp. Keine ioctl-, Capability-, Scratch- oder Store-Rechte
kommen hinzu; ein passendes Vollimage muss die tatsächliche Installation beweisen.

Belege: `out/full-build-adee7ad7/identity-test/`, insbesondere `proof.json`,
`events-accepted.json`, `diagnostic-copy.json`, `diagnostic-aegis-package-5.log`
und `after-diagnostic-correct-store.json`. Der lokale Beobachter wurde berichtigt:
Persönliche Auswahlen liegen unter `packages/store/current`, nicht
`packages/current`. Frühere Aufnahmen des falschen Pfads belegen keine
unveränderte persönliche Auswahl. Die korrigierte Aufnahme bestätigt die
tatsächliche Abwesenheit nach dem Fehler; der nächste Negativtest muss den
richtigen Pfad davor und danach vergleichen. Gemeinsame/private Kohärenz und
der vollständige Zwei-Benutzer-Neustartablauf bleiben offen.

## Öffentliche Paketfreigabe: Vollimage 9b8e5065

Der [Vollbuild](https://github.com/simgero/AegisOS/releases/tag/aosp-20260930T233124Z-9b8e5065-61eda95a)
mit Commit `9b8e5065cf174fe49f4082cc871ee20980595a29` besteht Build- und
Policy-Prüfungen auf `aegis-build`; der GitHub-Upload ist um 23:48:05 UTC
rückverglichen. Der lokale QEMU-Boot ist um 23:54:44 UTC mit Enforcing, FBE,
dm-verity und authentifiziertem ADB geprüft. Boot-ID:
`1b133025-bf4a-44ee-b792-51782a44b2b4`. Die vier Pakethelfer stimmen bytegenau
mit den b7-Komponenten überein; unveränderte SU-Suiten wurden nicht wiederholt.

Die echte CLI zeigt den Plan für `hello` an. Nicht angemeldeter Aufruf,
falsches Alpha-Adminpasswort und Betas fehlende Adminrolle verhindern eine
Veröffentlichung. Alpha bleibt gestoppt/CE-gesperrt; Beta behält seinen
persönlichen Runtime-Kontext. Eine korrekte frische Alpha-Freigabe erreicht
am **2026-10-01T00:00:23Z** den Ausführungshelfer; dessen private PTY-
Metadatenprüfung wird noch von SELinux verweigert. Keine Installation ist
bestätigt. Der folgende Policy-Fix beschränkt die Metadatenfreigabe auf einen
eigenen Geräte-Typ dieses Helfers. Er muss im passenden Vollimage geprüft werden.

Belege: `out/full-build-9b8e5065/identity-test/` mit Ereignissen, allen Vorher-/
Nachher-Aufnahmen, Helper-Bytevergleich und anschließendem Abmelden beider
Testkonten. Paketinstallation, gemeinsame/private Kohärenz und vollständiger
Zwei-Benutzer-Neustartablauf bleiben offen.

## Atomare APT-Planablage: 62 gezielte native Tests

Commit `b7ee7fc836b79d13c9563e40f5fbd9d8c56f5845` besteht am
**2026-09-30T22:50:39Z alle 62 gezielten nativen Tests aus zwei Suiten**,
Exitcode 0, Laufzeit 90991 ms. Die 36 Planungs- und 26 Ausführungstests prüfen
die geänderte APT-Hook-Veröffentlichung mit eigenen synthetischen Images und
Kontrollgruppen. Vier deaktivierte reale CE-Tests bleiben aus. Unveränderte
Java- und Deskriptorprüfungen wurden nicht erneut ausgeführt.

Der [Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T224832Z-b7ee7fc8-b7ee7fc8-Zt29cx)
wurde auf `aegis-build` kompiliert und über GitHub verifiziert übertragen.
Die Ausführung erfolgte ausschließlich im lokalen Mac-QEMU mit Vollimage
`b6e4b93e`, Boot-ID `0b7fe41d-7bcb-415d-8a88-910ca30d4c2b` und SELinux Enforcing.
Benutzer, CE-/DE-Schlüsselkennungen und Runtime-Kontexte sind davor/danach
bytegleich; beide vorhandenen persönlichen Testkonten blieben abgemeldet.

Der echte installierte b6-Paketpfad hatte zuvor das APT-Hook-`link` auf dem
privaten Arbeitsdateisystem verweigert. Die Korrektur verwendet
`renameat2(RENAME_NOREPLACE)` mit den bestehenden Rechten. Das Ziel wird atomisch
sichtbar und niemals überschrieben; zusätzliche Policy-Rechte sind nicht nötig.
**Grenze:** Diese SU-Komponentenprüfung ersetzt nicht den installierten Helfer
und beweist noch keine öffentliche Paketinstallation oder frische AOSP-Freigabe.
Das passende Vollimage wurde über GitHub verifiziert und am 30. September um
23:17:56 UTC mit Enforcing, dm-verity und authentifiziertem ADB gestartet.
Seine vier Pakethelfer sind bytegleich zu den geprüften Komponenten. Der
installierte CLI-Aufruf erreicht den Archivdownload und scheitert danach am
Broker-`setattr` auf `hello_2.10-5_arm64.deb`, vor der Adminabfrage. Der frühere
Hook-`link`-Fehler ist damit im tatsächlichen Produktpfad behoben. Beide
Testbenutzer wurden wieder abgemeldet, CE gesperrt und alle Kontexte abgebaut.
Belege: `out/full-build-b7ee7fc8/identity-test/proof.json` sowie
`helper-byte-comparison.json`. Die gezielte Policy-Korrektur benötigt ein neues
Vollimage; identische SU-Komponententests ersetzen diese Prüfung nicht.
Der gesamte Zwei-Benutzer-Paketablauf bleibt ausstehend.

Nachweise: `out/components-b7ee7fc8/targeted-tests/` und
`out/full-build-b6e4b93e/identity-test/proof.json`.
Native-Log SHA-256: `b67dfbd268320a9a430777dca77e2f9c8b23d1d6d50d5c96e1f15403a5bbb7e4`.
Vorher/Nachher SHA-256: `7e613ad413f79888442b5ee6fa97976c33c2b9f188901a29451bb06ca60231a5`.

## Nur lesbare Paketübergabe: 67 gezielte native Tests

Commit `b6e4b93e95fa5d60e7f90be13409bb1b49e51e2d` besteht am
**2026-09-30T21:54:43Z alle 67 gezielten nativen Tests aus vier Suiten**,
Exitcode 0, Laufzeit 90330 ms. Vier neue Deskriptortests, 36 Paketplanungs-,
26 Ausführungs- und ein Netzwerkregeltest decken die geänderten Übergaben ab.
Vier deaktivierte reale CE-Tests bleiben deaktiviert. Java wurde nicht geändert;
die vorherigen 152 bestandenen Prüfungen wurden nicht wiederholt.

Der [Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T215236Z-b6e4b93e-b6e4b93e-k5kvW3)
wurde auf `aegis-build` kompiliert und ausschließlich über GitHub übertragen.
Ausführung erfolgte im lokalen Mac-QEMU mit Vollimage `c9cd225d`, Boot-ID
`b6e8719e-556a-46b6-9f81-b60c0aaa61bf` und unverändertem SELinux Enforcing.
Die beiden vorhandenen persönlichen Testkonten waren abgemeldet und gesperrt.
Benutzerregistrierung, CE-/DE-Schlüsselkennungen und Runtime-Kontexte sind in den
Vorher-/Nachher-Aufnahmen identisch. Die Fixtures verwenden eigene synthetische
Images und Kontrollgruppen, keine bestehenden persönlichen Stores.

**Grenze:** Der echte c9-CLI-Aufruf hatte zuvor beim Domänenwechsel des
Netzwerkhelfers eine Schreibverweigerung auf dessen versiegeltes, aber noch
`O_RDWR` geöffnetes memfd gezeigt. Die Korrektur öffnet vor exec/SCM_RIGHTS eine
unabhängige `O_RDONLY`-Beschreibung. Diese Komponentenprüfung ersetzt weder den
installierten Broker noch den öffentlichen Binder-/AOSP-Freigabetest; das passende
Vollimage und tatsächliche Paketinstallation stehen noch aus. Der vorherige
Komponentenversuch `cb49ba41` scheiterte an zwei C++-Compilerdiagnosen und führte
keine Laufzeittests aus; diese wurden in `b6e4b93e` behoben.

Nachweise: `out/components-b6e4b93e/targeted-tests/` mit `result.json`,
`native.log`, `before.json` und `after.json` sowie
`out/full-build-c9cd225d/identity-test/proof.json`.
Native-Log SHA-256: `16410832caa4b0f85d13a891b3b29a6da6505958964e6b2e0c267671070df127`.
Vorher/Nachher SHA-256: `7e613ad413f79888442b5ee6fa97976c33c2b9f188901a29451bb06ca60231a5`.

## Öffentliche Paket-CLI: Komponenten geprüft, Vollimage folgt

Commit `7902400f7f345d418c03f2e6b2278243738e00fe` kompiliert auf `aegis-build`
mit dem neuen `IAegisPackage`, der sitzungsgebundenen Dienstanbindung und der
interaktiven Plan-/Passwortabfrage. Am **2026-09-30T20:26:08Z bestehen 400/400
aktivierte native Tests aus 46 Suiten und 152/152 Java-Tests**, jeweils Exitcode 0.
Native Laufzeit: 186791 ms. Vier reale CE-Tests bleiben ausdrücklich deaktiviert.

Der [Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T202213Z-7902400f-7902400f-EFc2cg)
wurde über GitHub verifiziert. Laufzeittests fanden ausschließlich im lokalen
Mac-QEMU mit unverändertem Vollimage `40179351`, SELinux Enforcing und Boot-ID
`82749131-9195-4239-92e9-d3679a98c08a` statt. Die Benutzer-, Schlüssel- und
Kontextaufnahmen davor/danach sind bytegleich. Auch die tatsächlich kompilierte
CLI-Hilfe wurde in diesem Gast ausgeführt und zeigt alle vier Paketbefehlsgruppen.

Die zwölf zusätzlichen Java-Tests prüfen vollständige Review-Anzeige, einmaligen
Handoff, abgelaufene und fremde Pläne, partiellen/verlorenen BEGIN, fehlgeschlagenen
START, unbekannte Jobs, Abbruch vor weiterer Planung, fehlgeschlagene Bereinigung,
Veröffentlichung beim Abbruchrennen und atomaren STOP ohne Warten auf den
Auftragsmonitor. Sie verwenden den produktiven Java-Koordinator mit einem
simulierten nativen Kanal; sie sind keine frische AOSP-Passwortprüfung.

**Grenze:** Der installierte 401-Dienst wurde nicht ersetzt. Eine echte
Paketinstallation über Binder, tatsächliche AOSP-Adminfreigabe und Ausführung in
den vorgesehenen Paketdomänen bleiben zu prüfen. Der kleine Folgecommit
`73c0f3eb2966d248ebcba0fa799359aa3282d09c` ordnet Paketwiderruf/-retirement vor
potenziell fehlschlagendem PTY-Aufräumen an. Er ändert ausschließlich den Dienst;
sein passendes Vollimage wird separat gebaut und geprüft.

Nachweise: `out/components-7902400f/proof.json`, `cli-help-proof.json` und
`targeted-tests/{result.json,native.log,java.log,before.json,after.json}`.
Native-Log SHA-256: `86bd61aa06aea2f4f071b5d77e5cf8ae20db9990f78cb9fb5423712ed649bdf7`.
Java-Log SHA-256: `d1612672c230b116e09e7a48701826b898bc8bd349a4b22df2fc53f1d433eab4`.

## Paketkanal: 400 native und 140 Java-Tests

Komponentencommit `49f038cd0f4f3d116933179525cffcf46de5eb1c` besteht am
**2026-09-30T19:28:05Z alle 400 aktivierten nativen Tests aus 46 Suiten und
alle 140 Java-Tests**, jeweils Exitcode 0. Die Java-Tests wurden neu ausgeführt.
Vier reale CE-Integrationstests bleiben deaktiviert. Native Laufzeit: 194327 ms.

Der [Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T192344Z-49f038cd-49f038cd-lHeHOX)
stammt ausschließlich vom SSH-Builder. Download und Prüfsummen wurden über
GitHub verifiziert; ausgeführt wurde nur im lokalen Mac-QEMU mit Vollimage
`401793518441d671d3f2a67a5488b1faee4978c4`. Dessen unveränderte SELinux- und
Plattformkonfiguration wurde mit dem Komponentenstand verglichen. Das
[Vollimage](https://github.com/simgero/AegisOS/releases/tag/aosp-20260930T184515Z-40179351-fc7aba3a)
hat zuvor seinen strengen Bootcheck und alle eigenen 393 nativen Tests bestanden.

Die neuen Prüfungen decken begrenzte Paketnachrichten, unveränderte Auftragskennung
bei partiellen Fehlern, Ablehnung fremder Deskriptoren, vollständige native Reviews
und strenge Java-Decodierung ab. Veränderte Antwortidentitäten, doppelte JSON-Felder,
fehlende Effekte, ungültige Abhängigkeitsmarkierungen und verfrühte
Veröffentlichungsbehauptungen werden zurückgewiesen. Benutzer-, Schlüssel- und
Kontextnachweise vor und nach dem Lauf sind bytegleich; SELinux bleibt Enforcing.

**Grenze:** Der Produktdienst im 401-Image wurde nicht ersetzt. Die Tests prüfen
native Fixtures und Java-Komponenten, noch keine vollständige Installation über
einen öffentlichen Binder-/CLI-Auftrag mit frischer AOSP-Adminfreigabe. Auch
Produkt-Paketdomänen und der gesamte Zwei-Benutzer-Ablauf bleiben zu beweisen.

Nachweise: `out/components-49f038cd/proof.json` und
`targeted-tests/{result.json,native.log,java.log,before.json,after.json}` sowie
`out/full-build-40179351/proof.json` und `boot-1/boot-health.json`.
Native-Log SHA-256: `0ee4356e5dc850d777e8caaa8cd9149386279aaa5ead7aa8b2ccda2c72af61f7`.
Java-Log SHA-256: `ad221f21fe3b8b80691c2c8676faf25274d32aa56f1de96f5d4b544ee7360772`.

## Beschreibbare Kandidaten: 384 native Tests im passenden Vollimage

Commit `c6f43096e5a686faa8666c560c430b2d3b983bd8` besteht am **2026-09-30T18:11:28Z alle 384
aktivierten nativen Tests aus 44 Suiten**, ohne Überspringen oder Abwahl,
Exitcode 0, Laufzeit 199794 ms. Image und Testmodule stammen vom selben Commit.
Build und Komponentenkompilierung liefen ausschließlich auf `aegis-build`,
Transport über die geprüften [Image-Assets](https://github.com/simgero/AegisOS/releases/tag/aosp-20260930T173838Z-c6f43096-3588256e) und
[Komponenten-Assets](https://github.com/simgero/AegisOS/releases/tag/components-20260930T180710Z-c6f43096-c6f43096-deW8CK),
Ausführung ausschließlich in einer neuen lokalen Mac-QEMU-Instanz.

Sieben Labeltests und zwei zusätzliche Vorbereitungstests bestätigen die
vollständige Kandidatenprüfung, einschließlich tatsächlicher ext4-Kopien mit
fremdem Root- bzw. Symlink-Label. Diese Eingänge werden vor dem Handoff
abgewiesen. Auch die bestehenden Prüfungen für signierte Paketquellen,
Installation/Aktualisierung/Entfernen, Auftragsbindung, Abbruch und sichere
Bereinigung bestehen. Vorher-/Nachher-Nachweise für Benutzer, CE-Schlüssel und
Runtime-Kontexte sind bytegleich. Vier reale CE-Tests bleiben deaktiviert;
der unveränderte Nachweis von 130 Java-Tests wird aus `95f2b925` übernommen,
nicht erneut ausgeführt.

Das neue Vollimage bootet mit SELinux **Enforcing**, authentifiziertem ADB,
FBE, geprüften dm-verity-Tabellen und dem echten init-gestarteten Broker in
`u:r:aegis_runtime_broker:s0`. Alle vier unveränderlichen Pakethelfer sind gepinnt.
Die strenge Startprüfung meldet keine Runtime-/Paket-AVCs und enthält keine
Ausnahme für Beobachterfehler. Profil `26ba98ef-845b-4d51-98a7-a007e2afc0dd`,
Boot `c623189d-91d5-4067-ade3-48ca931842f2`, AVB-Digest `598229848b6879a23c2aa7eb67439ace2160a77f16683ff4870952911c89e72f`.
Die vorherige separate 2f-Test-VM wurde geordnet beendet; Android und KeyMint
bestätigten sauberes Herunterfahren. Beide zugehörigen Zustandsdateien bleiben
erhalten. Die ursprüngliche c740-VM läuft mit unveränderter Bootkennung weiter.

**Grenze:** Interne privilegierte Fixtures belegen noch keine Installation in
Produkt-Paketdomänen. Öffentliche Wire-/Binder-/CLI-Befehle, frische
AOSP-Adminfreigabe und der vollständige Zwei-Benutzer-Paketablauf sind offen.
Die folgenden neuen Worker-/Programm-/Netzwerkdomänen und der feste
Wiedereinstieg gehören zum noch separat zu prüfenden Stand `2308ea46`.
AOSP-Testschlüssel und direkter QEMU-Kernelstart bilden keine Hardware-Vertrauenskette.

Lokale Nachweise: `out/components-c6f43096/proof.json`,
`targeted-tests/{native.log,result.json,before.json,after.json}` sowie
`out/full-build-c6f43096/{proof.json,boot-1/boot-health.json,verified-release.json}`.
Native-Protokoll SHA-256: `2445f163f22545e1928e80af11bfd990ee682b41f21b2223711e07bbac5a6066`.
Bootnachweis SHA-256: `0a150f0929140ef3d40ebccc32251336f86fb88badc5c7ef98d191ce5d75b8f4`.

## Regulärer Brokerstart mit vier fest gepinnten Pakethelfern

Vollimage `2f7b18e2a7da19821f0a602632e0154105d97e14` wurde ausschließlich auf
`aegis-build` gebaut und über den
[verifizierten GitHub-Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260930T164048Z-2f7b18e2-d951591e) übertragen.
Lauf `aosp-20260930T164048Z-2f7b18e2-d951591e`, Invocation `b3abb846eb154a1e91b08b0d14705ba1`.
22 Release-Assets mit zusammen 1933827350 Bytes; das Manifest
bestätigt die Prüfsummen der übrigen 21 Dateien. AVB wurde lokal geprüft,
anschließend erfolgte der Start in einem neuen gepaarten Mac-QEMU-Profil.

Am **2026-09-30T17:11:15.644412+00:00** ist der echte init-gestartete Dienst in
`u:r:aegis_runtime_broker:s0` aktiv, SELinux bleibt **Enforcing**.
`AEGIS_PACKAGE_HELPERS_PINNED` und `AEGIS_RUNTIME_BROKER_LISTENING` stammen
vom tatsächlichen Dienststart. Planung, Netzwerk, Ausführung und Veröffentlichung
liegen unter ihren vier festen Systempfaden mit jeweils eigenem Ausführungstyp,
root:shell, Modus 0755 und Linkzahl 1. Die im Gast gelesenen Hashes stehen im
vollständigen Bootnachweis. Der Startup-Code prüft diese Eingänge vor Übernahme
auf dem schreibgeschützten EROFS; keine Laufzeit-Ersetzung ist aktiviert.

FBE, authentifiziertes ADB, die tatsächlichen dm-verity-Tabellen für `system` und
`system_ext`, die feste verwaltete Runtime-Konfiguration und der Identitätsdienst
sind geprüft. AVB-Digest `263134cee5b03a43a7bfc97485fbdb546a318a2d337967dd2fd11af76955e670`.
Profil `9029a357-cba5-4058-aac3-e07fa70a6278`, Boot `4adbd778-2267-4bd4-b2f1-029d3dd84700`.
Die vorherige c740-Installation läuft weiterhin mit ihrer ursprünglichen
Bootkennung und unverändertem Profilmanifest; das gestoppte 178-Profil bleibt erhalten.

Der erste Beobachter las die Helfermetadaten irrtümlich als normale Android-Shell.
Deren `getattr` auf `/system/bin/aegis-package-plan` wurde korrekt verweigert:
Audit-Ereignis 5, `shell` → `aegis_package_plan_exec`, `permissive=0`.
Der davor gespeicherte reale Dienststart war bereits erfolgreich und ohne
Runtime-AVCs. Die wiederholte reine Metadatenprüfung nutzt den Diagnosezugang.
Der erste Fehlversuch, seine zwei Logcat-Zeilen und die Kernel-Zeile bleiben
unverändert erhalten; ausschließlich diese exakten Zeilen derselben Bootkennung
werden als erwarteter Beobachterfehler zugeordnet. Andere Runtime-/Paket-AVCs
würden weiterhin fehlschlagen; es gibt keine weiteren. Produktcode und Policy
wurden für diese Wiederholung nicht geändert, kein neuer Build war erforderlich.

**Grenze:** Das ist der Nachweis des fest konfigurierten Dienststarts, keiner
Paketinstallation. Produkt-Ausführungsdomänen, korrekte Labels im beschreibbaren
Kandidaten, öffentliche Wire-/Binder-/CLI-Operationen, frische AOSP-Adminfreigabe
und der vollständige Zwei-Benutzer-Ablauf fehlen noch. Die 375 nativen Tests
behalten ihren vorherigen Komponenten-Nachweis `9270ca79`; sie wurden hier
nicht erneut ausgeführt. Vier reale CE-Integrationstests bleiben deaktiviert.
Die Entwicklungs-Testschlüssel und der direkte QEMU-Kernelstart sind kein
Nachweis einer Hardware-Vertrauenskette.

Lokale Belege: `out/full-build-2f7b18e2/proof.json`, `boot-1/boot-health.json`,
`verified-release.json`, `boot-check-first-attempt.log`,
`boot-check-privileged.log` und `expected-observer-denial.json` im primären Workspace.
Bootnachweis SHA-256: `c7d7658dbfa3ad24c505af98ca23b65a0d53ee401dc625e9581e2e2c1765ded9`.
Profilmanifest SHA-256: `0be3676ef2f0407d01929b7aee5f23b58d53e52cd2226f0d549c3b51b2024c27`.
Das erfolgreich entpackte und gestartete lokale Download-Archiv ist danach
entfernt worden; Release, Metadaten, Abbilder und alle sieben Profile bleiben erhalten.

## Auftragsgebundene Bereinigung: 375 native Tests bestanden

Commit `9270ca79a56c3cfa3a2d0bfd7e048f2d6dd914e5` besteht am **2026-09-30T16:27:52Z alle 375 aktivierten
nativen Tests aus 43 Gruppen** (164940 ms, Exit 0). Der Build lief ausschließlich
auf `aegis-build`: `identity-20260930T161636Z-9270ca79-dt5ED6`, Invocation `5c8b2360e02c4faeb46eb0d9ef6b1599`.
Die [geprüften Komponenten](https://github.com/simgero/AegisOS/releases/tag/components-20260930T162432Z-9270ca79-9270ca79-ZOJaMa)
wurden ausschließlich im lokalen Mac-QEMU mit dem unveränderten Vollimage
`178cbb6eb41356c9f624f400b0f55e925e2f9650` getestet. Die neuen Komponenten wurden nicht in das
laufende Produktimage eingebaut; der init-gestartete Broker bleibt unverändert.

Sechs neue Verzeichnisprüfungen bestätigen begrenzte Bereinigung selbst erstellter
Arbeitsverzeichnisse, Erhalt fremder Geschwister, Ablehnung vorhandener Verzeichnisse,
falscher Rechte, unerwarteter Einträge, Hardlinks, Symlinks, Übergröße und ausgetauschter
Verzeichnisse sowie Wiederholbarkeit nach einem Fehler. Drei neue Brokerprüfungen
belegen ausstehende Auftrags-/CE-Verantwortung bei einem Bereinigungsfehler,
Bereinigung vor STOP-Bestätigung und Erhalt eines bereits veröffentlichten Ergebnisses
bei fehlschlagendem Hintergrundabschluss. Die vorhandenen Installations-, Entfernungs-,
Abbruch- und Quellenverlusttests prüfen jetzt zusätzlich, dass kein Arbeitsverzeichnis
zurückbleibt. Ein späterer Versuch verwendet eine neue Kennung.

Der vorherige Hintergrundpfad setzte jeden Abschlussfehler auf `Sealed`. Bei einem
Fehler nach Ende aller Helfer hätte das die nächste Bereinigung überspringen können.
Er behält jetzt den terminalen Zustand und das Ergebnis, bis die Bereinigung gelingt.
Extern übergebene Verzeichnisse werden nicht gelöscht. Wiederanlauf nach einem
Prozessabsturz, Kapazitätsgrenzen, öffentlicher CLI-/Binder-Pfad, echte frische
AOSP-Adminfreigabe und Produktions-SELinux bleiben ausstehend.

Das Profil `cf90cef4-9938-4ade-8a73-3fa8edab317f` und Boot `fb53b06a-0027-4673-89e5-78ce1a68db04` blieben erhalten.
SELinux ist `Enforcing`; der reguläre Broker läuft. Vorher-/Nachher-Snapshots
von Benutzeridentitäten, CE-Zuständen, Schlüsselverzeichnisnamen und Runtime-Kontexten
sind identisch: `0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3`. Es wurden keine realen AOSP-Benutzer
angelegt oder gelöscht. Die vier deaktivierten AOSP-CE-Tests blieben deaktiviert;
130 unveränderte Java-Tests werden mit ihrem Nachweis aus `95f2b925` wiederverwendet,
nicht als erneut ausgeführt gezählt.

Lokale Belege: `out/components-9270ca79/targeted-tests/` und `proof.json`.
SHA-256 des nativen Rohprotokolls: `b9149f067b17ee4f21f8796d838ae2d4f033fefe7ab0ecd9ad98c4e487a160d1`.
SHA-256 des Ergebnis-JSON: `b185495a1e8d7292bc177f60f2683a7f6ea7695a36cf0768eeeb7a60e8d494a0`.

## Konfigurierte Installationsvorbereitung: 366 Tests im passenden Vollimage

Commit `178cbb6eb41356c9f624f400b0f55e925e2f9650` besteht am **2026-09-30T15:52:26Z alle 366 aktivierten
nativen Tests aus 42 Gruppen** (183137 ms, Exit 0). Die Tests laufen ausschließlich
im lokalen Mac-QEMU. [Vollimage und verifizierter GitHub-Transport](https://github.com/simgero/AegisOS/releases/tag/aosp-20260930T152232Z-178cbb6e-00eb24df),
[zugehörige Komponenten](https://github.com/simgero/AegisOS/releases/tag/components-20260930T150808Z-178cbb6e-178cbb6e-zhWsMi).
Image-Lauf `aosp-20260930T152232Z-178cbb6e-00eb24df`, Invocation `273748899d584a188d4f21c01a1aee32`;
Komponentenlauf `identity-20260930T150459Z-178cbb6e-LP5z5O`, Invocation `d691065052d94ad58a64affd61c45e23`.

`BrokerPrepareConfiguredTransaction` leitet Basis, Zielstore und frisches
Arbeitsverzeichnis aus dem zuvor gehaltenen und geprüften Auftrag ab. Es nimmt
keine externen Pfade, Ziel-/Quell-Deskriptoren oder Freigabe-Digests entgegen.
Die Installationshelfer werden beim Start atomar gepinnt; ein späterer Austausch
ist ausgeschlossen. Registrierung und Übernahme des Planungsauftrags gehen den
Dateisystemoperationen voraus. Ein verschwundener Eingang bleibt als Fehler
unter derselben Auftragskennung erhalten.

Fünf neue Prüfungen decken Installation und anschließendes Entfernen aus der
gehaltenen gemeinsamen Generation, einmalige Helper-Konfiguration, notwendiges
Review, Abbruch mit frischem Arbeitsverzeichnis beim Wiederholen sowie eine
nach dem Review verschwundene Quelle ab. Installationsbestand, Abhängigkeit und
APT-Markierungen werden aus der anschließend erneut geprüften Generation gelesen.
Die signierte Offline-Fixture verwendet eine interne Freigabe als Testersatz;
dies ist **kein Nachweis eines öffentlichen CLI-Aufrufs oder frischer AOSP-Adminfreigabe**.

Die erste Ausführung mit `b83ed377` scheiterte an der korrekten SELinux-Sperre
gegen Kernel-Schreibzugriff auf veröffentlichte gemeinsame Dateien. Verbliebene
Fehler-Fixtures füllten danach den alten Testgast; dieser Lauf ist kein Erfolg.
Seine Belege bleiben in `out/components-b83ed377/targeted-tests/`, die drei
identifizierten Fixture-Bäume wurden nach beendetem Test und ohne Mountbindung
gezielt entfernt. Der korrigierte Stand verwendet für die beschreibbare gemeinsame
Vorbereitung den eigenen Typ `aegis_package_staging_file`. Die Kernel-Schreibsperre
für `aegis_package_shared_file` bleibt bestehen. Das Vollimage enthält diese
Regeln; globale Durchsetzung bleibt **Enforcing**. Der reale init-gestartete Broker
läuft in `u:r:aegis_runtime_broker:s0`; dessen Bootprüfung findet keine Runtime-AVCs.

Das neue unabhängige Profil `cf90cef4-9938-4ade-8a73-3fa8edab317f` besitzt nur Android-Benutzer 0;
Boot `fb53b06a-0027-4673-89e5-78ce1a68db04`. Vorher-/Nachher-Snapshot von Identitäten,
CE-Zuständen, Schlüsselverzeichnisnamen und Runtime-Kontexten ist identisch:
`0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3`. ADB authentifiziert diesen Mac;
FBE und tatsächliche dm-verity-Tabellen sind geprüft. Die Entwicklungs-Testschlüssel
und der direkte QEMU-Kernelstart belegen keine Hardware-Vertrauenskette.
Der alte c740-Gast und sein Profilpaar wurden nicht ersetzt.

Basislauf `runtime-base-20260930T152043Z-178cbb6e-r88QIR`, Generation
`5083dea9077e6e99c796e9ae0f77a4fe769e0695db0e9dc6db79971eca8e8c28`: Das geänderte Kennungsrezept erfordert eine
neu erzeugte, wiederholt bytegleiche Basis; Dateieinträge und Paketliste bleiben
gleich. Der erste Vollbuild mit dem alten Basiseingang wurde vor Kompilierung
abgewiesen. [Rezept und Nachweis](../runtime/generations.md).

Native-Log SHA-256: `b4f23e8295f78c1f7c7e89c15ebcd2ddab1d39591b87acb5a1f540ab602746c9`.
Native-Binärdatei SHA-256: `d56aaabcc2b8809fddc3a63740d386d2686eac796ea46f48d8db0c1fafbbb4a9`.
Lokale Belege im primären Workspace: `out/components-178cbb6e/proof.json`,
`out/components-178cbb6e/targeted-tests/` und `out/full-build-178cbb6e/`.
Die unveränderten 130 Java-Tests verwenden den früheren Nachweis `95f2b925` und
wurden hier nicht erneut ausgeführt. **Vier reale CE-Tests bleiben deaktiviert.**

Offen sind insbesondere begrenztes Aufräumen der Arbeitsverzeichnisse, die
produktive Aktivierung der Pakethelfer mit ihren SELinux-Grenzen, öffentliche
CLI/Binder-Befehle und frische AOSP-Adminbestätigung. Der vollständige
Zwei-Benutzer-Ablauf mit gemeinsamer und persönlicher Paketverwaltung ist noch
nicht abgeschlossen.

## Konfigurierter Paketauftrag bis zur signierten Planung: 361 Tests bestanden

Stand **cfbaf37a711c523ce0c8a06461c202f07c3a4d3a** besteht am
**2026-09-30T14:28:56Z alle 361 aktivierten nativen Tests aus 42 Gruppen**
(140355 ms, Exit 0). Kompiliert auf `aegis-build` in 35 Sekunden; Lauf
`identity-20260930T142439Z-cfbaf37a-KyFhHU`, Invocation
`0d3cae85976944ee8fa5869e980b8280`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T142603Z-cfbaf37a-cfbaf37a-bK3ecx).
Transport ausschließlich über GitHub, Laufzeittests ausschließlich im bestehenden
lokalen Mac-QEMU mit unverändertem Produktimage `c7401f60`.

`BrokerBeginConfiguredPackage` nimmt als Paketwunsch nur Aktion, Paket,
optionale Version und Zielbereich an. Die frisch von AOSP bestätigte Identität
wird getrennt übergeben. Der Broker speichert den Wunsch vor der Auswahl und
vor privaten CE-Zugriffen. Der spätere Übergang
`BrokerContinueConfiguredPackagePlanning` verwendet nur Identität, Seriennummer,
Auftrags-ID und neue Frist. Quelle, Vertrauensdaten, Helfer, Internetmodus und
Store-Anlage werden aus gehaltenem Produktzustand abgeleitet. Die gleiche ID
bleibt über Auswahl, Planung und das vorhandene Review erhalten.

Eine konfigurierte Auswahl kann nicht durch den allgemeinen FD-basierten
Übergang übernommen werden; ein allgemeiner interner Planauftrag wird auch
nicht als konfigurierter Auftrag akzeptiert. Wiederholte Fortsetzung startet
keinen zweiten Planer. Nach bestätigtem Abbruch erzeugt die alte ID kein neues
Werk. Teilfehler bleiben unter ihrer bereits verbrauchten ID im Besitzer.

Die Anlageentscheidung unterscheidet geprüfte Abwesenheit von einem bereits
initialisierten leeren Store. Beide können dieselbe Factory-Generation wählen,
benötigen aber unterschiedliche veröffentlichungsgebundene Pläne. Ein
beschädigter vorhandener Store scheitert; fehlende persönliche CE wird nicht
erstellt. Die neue Helferübernahme ist atomar, dupliziert die geprüften
Dateideskriptoren und lässt nach dem Start keine Ersetzung zu.

Die vier neuen Prüfungen bestehen, einschließlich eines echten HTTPS-/Signatur-
Abrufs von `hello` über die fest gepinnten Debian-Quellen (5966 ms). Der Aufrufer
ändert nach Registrierung Paket, Aktion und Zielbereich; das Review bleibt bei
dem ursprünglichen gemeinsamen Installationswunsch und dem erwarteten Archiv.
Weitere Prüfungen vergleichen fehlenden und initialisierten leeren Store,
beschädigen nur einen neu angelegten isolierten Fixture-Store, prüfen falsche
Identität/Seriennummer, ungültige Anfragen, atomare Helferübernahme, FD-Freigabe
und die Ablehnung einer nicht vorhandenen persönlichen CE ohne Benutzeranlage.
[Auftragsvertrag und verbleibende Produktanbindung](package-network.md).

**Grenze:** Native Fixtures rufen die neue interne Konfiguration auf. Der
Produkt-Broker aktiviert die Planer-Helfer noch nicht; deren feste Bereitstellung
und SELinux-Einbindung müssen folgen. Die verbleibende FD-freie Übergabe zur
Installationsvorbereitung, öffentliche Wire-/Binder-/CLI-Operationen, frische
AOSP-Adminbestätigung und ein neues vollständiges Systemimage sind noch offen.
Diese Tests belegen weder eine öffentlich verfügbare Paketinstallation noch
den vollständigen Zwei-Benutzer-Lebenszyklus. Vier deaktivierte reale CE-Tests
bleiben ungetestet. Die 130 unveränderten Java-Tests behalten ihren früheren
Nachweis `95f2b925`; kein erneuter Java-Lauf.

Profil `39d29ee1-7587-4223-8e5d-f9872c910554`, Boot
`6c6dc3df-cf70-4c1b-8f16-f131b5a981a5`, SELinux Enforcing, Broker läuft.
Benutzer-/Schlüssel-/CE-Snapshot vor und nach dem Lauf identisch:
`427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.

| Nachweis | SHA-256 |
| --- | --- |
| Native Testdatei | `52e0b8254ad7afba8ae140375aaff985c5e0e9497cbd4630ac8378c9c6fa5d2a` |
| Vollständiges Protokoll | `aeb86a47c8d23d201fc0c61aead0e806dddd48fe8000184708a70b8fe387ecb2` |
| Vollständiges Ergebnis | `2e5797bb9adae4fbc465234d4282b2b8c0c66b28110d82e91b6453aae40b1c1b` |

Lokale Belege im primären Workspace: `out/components-cfbaf37a/proof.json`
und `out/components-cfbaf37a/targeted-tests/` (Filter `*`, vollständige aktivierte Suite).

## Feste Debian-/AOSP-Vertrauensdaten: 357 native Tests bestanden

Stand **e26cfd9004e89542bb3021d4429f0c3be6a267d7** besteht am
**2026-09-30T14:12:53Z alle 357 aktivierten nativen Tests aus 42 Gruppen**
(171173 ms, Exit 0). Kompiliert auf `aegis-build` in 20 Sekunden; Lauf
`identity-20260930T140829Z-e26cfd90-w8DVEH`, Invocation
`a4dfc37f04144d5c9ced85629f76d496`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T140924Z-e26cfd90-e26cfd90-JBq8Tz).
Alle Laufzeittests fanden im bestehenden lokalen Mac-QEMU mit Produktimage
`c7401f60` statt. Der Produktbroker wurde dabei nicht ersetzt.

Der neue Startbaustein liest ausschließlich die drei festen Debian-13-
Archivschlüssel aus der geprüften, schreibgeschützten Basis und die CA-Zertifikate
aus dem signierten, schreibgeschützten Conscrypt-APEX. Die Quellenliste ist fest
im Produkt: HTTPS für `trixie`, `trixie-updates` und `trixie-security`, jeweils
`main`. Mounttyp/Schreibschutz, Dateityp, Eigentümer, SELinux-Typ und Größen werden
geprüft; die Auflösung erfolgt relativ zu gehaltenen Verzeichnissen ohne
Symlink-/Mountübergänge unterhalb dieser Anker.

Quellen, Signaturschlüssel und TLS-Bündel werden gemeinsam als drei readonly
memfds mit vollständigen Schreib-, Größen- und weiteren Seal-Sperren übernommen.
Der Broker-Startcode bindet sie vor dem ersten Auftrag; nachträgliche Ersetzung
und teilweise Übernahme werden abgewiesen. Die interne Dateiprüfung akzeptiert
weiterhin reguläre vertrauenswürdige Fixture-Dateien und ist kein Herkunfts-
oder Autorisierungsnachweis. [Datenweg und Produktgrenzen](package-network.md).

Die drei neuen Tests prüfen den echten Abruf über alle drei Quellen mit
Signaturen und vollständigen Indexhashes, verweigerte Schreib-/Größenänderungen,
unversiegelte Eingaben, ungültige Basis-FDs, atomare Übernahme ohne FD-Leck und
unveränderliche Startkonfiguration. Der positive Lauf benötigt 6576 ms und liefert
drei authentifizierte Repository-Belege. `hello_2.10-5_arm64.deb` hat 52.660 Bytes
und SHA-256 `7a917c7f44fbd3373dff0f35a0b6bdf8ef564ff90579d8b130ff52fbf33fce1f`.
Die ausgewählte Paketdatenbank und automatische Installationsmarkierungen bleiben
unverändert. Dieser Abruf installiert das Paket nicht.

Der erste Stand `7a1088b9` hatte zwei Compilerfehler. Nach deren Korrektur
kompilierte `c95a2c4f`, wurde aber bei der neuen Verzeichnisübernahme von Bionics
fdsan abgebrochen. Die Korrektur gibt den `unique_fd` vor `fdopendir` frei und
schließt den unübernommenen Deskriptor ausdrücklich bei Fehler. Die Aufzeichnungen
beider Versuche bleiben erhalten; beim Laufzeitabbruch waren Benutzer-/CE-
Bestand und Broker unverändert. Erst `e26cfd90` hat den vollständigen grünen Nachweis.

Die SELinux-Bauprüfungen einschließlich Kontext-, Kompatibilitäts- und Neverallow-
Prüfungen bestanden im Komponentenbuild `c95a2c4f`; die anschließende Korrektur
ändert keine Policy. Das beweist noch nicht den tatsächlichen neuen Brokerstart:
Vollimage und Boot mit diesen Startzugriffen sind weiterhin erforderlich.
Öffentliche Paket-CLI/Binder, konfigurierte komplette Auftragspipeline, frische
AOSP-Adminbestätigung und der vollständige Zwei-Benutzer-Ablauf bleiben offen.
Die 130 Java-Tests behalten ihren unveränderten Nachweis `95f2b925`; sie wurden
hier nicht erneut ausgeführt. Vier deaktivierte echte CE-Tests bleiben ungetestet.

Profil `39d29ee1-7587-4223-8e5d-f9872c910554`, Boot
`6c6dc3df-cf70-4c1b-8f16-f131b5a981a5`, SELinux Enforcing, Broker läuft.
Benutzer-/Schlüssel-/CE-Snapshot vorher und nachher identisch:
`427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.

| Nachweis | SHA-256 |
| --- | --- |
| Native Testdatei | `d99821d98409381d49d2f837b1d5b86b1e209e785379469324c7c04ec1b98d3e` |
| Vollständiges Protokoll | `b5570e9bcaeee4c4f87d4636fd32282d6943e5dfc6054e333fc7c540e3554d7e` |
| Vollständiges Ergebnis | `efdbc8f72a85483ca1ff9b58aa0dbb11847ada4a2e2503fd9a479d9e992b1f75` |
| Produktquellen-Beleg | `16d2424f28a60c376061f51cb954bc22853d20642987100a961187ae77810968` |

Lokale Belege im primären Workspace: `out/components-e26cfd90/proof.json`,
`product-policy-receipt.json` im selben Verzeichnis sowie `targeted-tests/`
(Filter `*`, vollständige aktivierte Suite).

## Paketaufträge neben laufender Runtime: 354 native Tests bestanden

Stand **5427b78903742d88fdb0d36278bdfabd44d28c11** besteht am
**2026-09-30T13:42:01Z alle 354 aktivierten nativen Tests aus 42 Gruppen**
(136833 ms, Exit 0). Kompiliert auf `aegis-build` in 36 Sekunden; Lauf
`identity-20260930T133714Z-5427b789-sMjebv`, Invocation
`b4931a388b354488b8a3eb30ae7084d8`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T133832Z-5427b789-5427b789-M2HBGI).
Tests ausschließlich im unveränderten lokalen Mac-QEMU, Produktimage `c7401f60`.

Die bisher gemeinsame Benennung der cgroups verhinderte Paketarbeiten neben
einer Runtime desselben Benutzers. Runtime-Prozesse verwenden weiter `u<id>-s<serial>`,
Pakethelfer einschließlich Generationenauswahl nun `p<id>-s<serial>`.
Beide zählen weiterhin zum unveränderten gemeinsamen Limit von 16 Blättern und
2 GiB. Die Wiederanlaufprüfung akzeptiert genau beide kanonischen Formen;
fremde beziehungsweise fehlerhafte Namen bleiben ein Fehler. Neue Helper und
Broker-Wiederanlaufcode müssen gemeinsam im nächsten Vollimage ausgeliefert werden.
Die produktiven Prozesse wurden nicht durch Komponenten ersetzt.

Die Auswahl speichert nun ihren Zweck (Runtime, gemeinsame oder persönliche
Pakete). Ein Paketplan darf weder eine Runtime-Auswahl übernehmen noch den Scope
nachträglich ändern. Bereits aktivierte Runtime-Metadaten belegen keinen weiteren
Paketauftrag; ausstehende und fehlgeschlagene Paketaufträge bleiben bis zur
bestätigten Freigabe auf einen pro Benutzer begrenzt. Ein eigener Abbruch schließt
nur den passenden Paketauftrag. STOP/HELLO/Verbindungsabbruch besitzen weiterhin
alle Ressourcen bis zum vollständigen Aufräumen.

`BrokerPrepareConfiguredPackageSelection` verwendet ausschließlich beim Start
gepinnte Factory-, Helper-, cgroup- und Zustandsdeskriptoren. Persönliche Auswahl
öffnet den geprüften CE-Pfad nach Registrierung; gemeinsame Auswahl greift nicht
auf den persönlichen Store zu. Der spätere Systemdienst muss für beide Fälle
vor dem Aufruf frische AOSP-Anmeldung, Benutzer/Seriennummer und CE-Zulassung prüfen.

Die sechs neuen Prüfungen belegen Scope-/Zweckbindung, genaue Abbruchzuordnung,
fehlende CE ohne stillen Fallback, beide Wiederanlauf-Namensformen sowie
Paketvorbereitung und Abbruch neben einem tatsächlich laufenden isolierten
Namespace beziehungsweise Prozess derselben Identität. Diese Fixtures ändern
keine realen AOSP-Benutzer und ersetzen keinen angemeldeten Produkt-End-to-End-Test.

Benutzer/Schlüssel/CE-Snapshot vorher und nachher identisch:
`427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.
Profil `39d29ee1-7587-4223-8e5d-f9872c910554`, Boot
`6c6dc3df-cf70-4c1b-8f16-f131b5a981a5`, SELinux Enforcing, Broker läuft.
Native-Log SHA-256:
`eb04914df7669de604cc1ffc2a474081428a0866a302b8807b72aa39fdab3163`.
Binärdatei SHA-256:
`745c1c9512fe3a482fcc0da2aa5e0d2ad1ea365ea169155e8158582fe05afdca`.
Lokale Belege im primären Workspace: `out/components-5427b789/proof.json`
und `out/components-5427b789/targeted-tests/` (Filter `*`, vollständige aktivierte Suite).
Die 130 Java-Tests verwenden ihren unveränderten früheren Nachweis; sie wurden
hier nicht erneut ausgeführt. Vier deaktivierte reale CE-Tests bleiben offen.
Produkt-CLI/Binder, frische Adminbestätigung, Trust-Inputs/SELinux, Vollimage und
vollständiger Zwei-Benutzer-Ablauf sind noch nicht abgeschlossen.

## Kontrollierter Debian-Internetabruf: 54 und 348 Tests bestanden

Komponentenstand **83744d58352dbcca34e3944efd53d1d1f522a1f7** besteht im lokalen Mac-QEMU
**54/54 gezielte Tests** (2026-09-30T13:15:43Z) und anschließend **348/348 aktivierte
native Tests aus 42 Gruppen** (2026-09-30T13:18:17Z, 122120 ms).
Buildlauf `identity-20260930T131320Z-83744d58-QUY3vp`, Invocation `55861933f27946d7bf0bad6216a9ba97`,
kompiliert auf `aegis-build` in 44 Sekunden. [Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T131447Z-83744d58-83744d58-0dhN2v).
Entwicklung lokal, Kompilierung auf dem SSH-Builder, Transport über GitHub und
sämtliche Gerätetests im vorhandenen lokalen QEMU.

Der Planer erhält einen eigenen, vom Broker gehaltenen Netzwerkhelfer im selben
begrenzten Auftrag. Nur der private Planer kann dessen Listener erreichen;
persönliche Shells und Installationsprogramme behalten getrennte Netzwerkbereiche.
Der feste Helfer lässt ausschließlich die beiden Debian-Hosts auf Port 80/443
zu, weist numerische/unzulässige Ziele und nichtöffentliche Zieladressen ab und
wird bei Abbruch beziehungsweise Abschluss mitsamt seinen Verbindungen beendet.
Der bestätigte positive Abruf verwendet HTTPS mit einem festen Zertifikatsbündel;
Signatur-, Gültigkeits-, vollständige Index- und Archivhashprüfungen bleiben aktiv.
[Besitz, Grenzen und Datenweg](package-network.md).

Der echte Abruf liefert `hello_2.10-5_arm64.deb` mit **52.660 Bytes**, SHA-256
`7a917c7f44fbd3373dff0f35a0b6bdf8ef564ff90579d8b130ff52fbf33fce1f`. Der vollständige authentifizierte Paketindex
hat 56.162.922 Bytes und den Hash
`f97011263173eaccb1a215a28a6ec7ba1f6e5cebd8bb68e8fa0475b1d4021161`. Seine längste `Provides`-Zeile
ist 75.649 Bytes lang; die frühere 64-KiB-Grenze blockierte deshalb korrekt einen
noch nicht unterstützten Index. Jetzt werden bis zu 128 KiB pro Zeile bei
weiterhin 1 MiB pro Absatz akzeptiert. Grenztests und abweichende Hashes bleiben
negativ. Die Diagnosemessung aus einem separaten öffentlichen Abruf ist erst
durch Übereinstimmung mit diesen authentifizierten Hashes dem Testindex zugeordnet.

Bestätigt sind außerdem Ablehnung ungültiger TLS-Vertrauensanker und unerlaubter
Ziele, vollständig eingesammelte Abbrüche, die Übergabe genau sechs
Richtlinieneingaben ohne Deskriptorleck und die bisherigen signierten
Offline-Paketabläufe bis zur veröffentlichten, erneut überprüften Generation.
Statisches Bionic konnte Androids DNS-Proxy nicht verwenden; der Netzwerkhelfer
nutzt nun dynamisches Bionic und die reservierte Systemdienstkennung 7502
außerhalb aller persönlichen UID-Abbildungen. HTTPS-Prüfung wurde nicht abgeschaltet.

**Grenzen:** Dies ist der echte Netzabruf im internen Komponentenpfad. Öffentliche
CLI/Binder-Verbindung, Produktbereitstellung von Quellen/Schlüsseln/TLS-Bündel,
SELinux-Domäne des Netzwerkhelfers und frische AOSP-Adminfreigabe sind noch nicht
aktiviert. Die vier deaktivierten echten AOSP-CE-Tests wurden nicht ausgeführt.
130 unveränderte Java-Tests behalten ihren früheren Beleg `95f2b925`; kein erneuter
Java-Lauf. Der vollständige Zwei-Benutzer-Produktablauf bleibt offen.

Gast `c7401f60`, Profil `39d29ee1-7587-4223-8e5d-f9872c910554`, Boot `6c6dc3df-cf70-4c1b-8f16-f131b5a981a5`:
SELinux Enforcing, authentifiziertes ADB, laufender Broker, Benutzer 0/10/11,
CE-/DE-Schlüsselkennungen und leere Runtime-Kontexte bleiben vor/nach beiden
Durchläufen unverändert. Kein Produktimage oder Benutzerprofil wurde ersetzt.

Nachweise: `out/components-83744d58/{targeted-tests,native-regression}/` und
`online-receipt.json` im primären Workspace. Der erste Versuch des vorherigen
Komponentensatzes hielt vor dem Teststart wegen Platzmangels an. 14 ausschließlich
zu `c33c05d9` gehörende, inaktive Kopien der Testbasis wurden nach Prüfung von
Prozessen und Loop-Zuordnungen entfernt (3,5 GiB im Gast); Nachweis
`out/components-c33c05d9/cleanup-fixtures.json`. Benutzerdateien und Schlüssel
waren nicht betroffen.

| Datei | SHA-256 |
| --- | --- |
| Gezieltes Protokoll | `478c618b6c77308c39d3858d3cfc9b5475900fc631d581679f4e395c66f70ccc` |
| Vollständiges Protokoll | `8d73e4f4be76c875b8e504e0c7b99f9f4c40a1480afe77818e4e3a6977798795` |
| Vollständiges Ergebnis | `29729e907fe205e9de79dcd8cda6be95ca4c43d4845c1bd1b14711fa1da004a3` |
| Zustand vor/nach Tests | `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a` |
| Native Testdatei | `b6e9c01f6d99d46db09851f49aa778827f41fae2f47a7c735bfa2df7e76994ce` |

## Paketplan aus signierter Quelle bis zur veröffentlichten Generation: 46 und 340 Tests bestanden

Komponentenstand **b827ba82e6576c0cc08121a3a862c767f5fcb737** besteht im lokalen Mac-QEMU
**46/46 gezielte Prüfungen** (2026-09-30T11:54:15Z, 15,602 Sekunden) und **340/340
aktivierte native Tests aus 41 Gruppen** (2026-09-30T11:56:33Z, 110500 ms).
Buildlauf `identity-20260930T114712Z-b827ba82-u1BhQf`, Invocation `c3483bb3d8e54ffa9f73a450b8c06736`,
kompiliert auf `aegis-build` erfolgreich in 4:04 Minuten einschließlich neu
erstellter Buildkonfiguration. [Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T115154Z-b827ba82-b827ba82-QZpb8I).
Die erste lokale Releaseabfrage war unmittelbar nach Veröffentlichung noch ohne
Treffer; erneute Abfrage und Prüfsummenprüfung bestätigen denselben Release.
Es wurde kein zweiter Build oder Upload gestartet.

Der isolierte Planer übergibt jetzt begrenzte, kanonische Metadaten aus seiner
signierten Quellenprüfung direkt an den registrierten Brokerauftrag. Die
geprüfte Generationsauswahl enthält auch Größe und Hash ihrer gemeinsamen Basis.
Antragsteller, persönlicher/gemeinsamer Bereich und Store-Anlage stehen vor der
Auflösung fest. Der Broker ergänzt diese behaltene Identität und Quellenauswahl
zum vollständigen Review; der Aufrufer kann keine Ersatzversionen, Quellenbelege,
Dateipfade oder Hashlisten einspeisen. Die Review-Operation gibt nur Metadaten aus.

`BrokerPreparePlannedTransaction` übernimmt denselben Auftrag in die vorhandene
Vorbereitung und Ausführung. Archive stammen ausschließlich aus dem privaten
Planerverzeichnis nach bestätigter Beendigung aller Arbeiter. Der kurze Übergang
prüft Dateityp/Eigentümer/Größe und übernimmt nur diese temporären Archive; Kopieren
und erneute Hashprüfung bleiben im asynchronen Vorbereiter. Gemeinsame und private
Zielregeln bleiben getrennt; ein privates Ziel verwendet weiterhin die bestehende
CE-Verankerung. Ein persönlicher Ausgangsstand kann nicht zum gemeinsamen Ziel
umgedeutet werden. Vor Review, Übergang und tatsächlichem Ausführungsstart wird
die Repository-Gültigkeit geprüft; eine abgelaufene Vorbereitung startet nicht.

Der neue vollständige native Fall verwendet die echte Werksauswahl und eine
signierte Offline-Quelle. Er plant App und Abhängigkeit, weist einen anderen
Freigabe-Digest schon vor der Kandidatenanlage ab, bereitet unter derselben
Auftragskennung vor, weist eine veränderte Startfreigabe erneut ab und führt
anschließend APT, unabhängige Bestandsprüfung und Veröffentlichung aus. Er öffnet
die veröffentlichte Generation über den echten Selektor, prüft beide installierten
Versionen 2 und die automatische Markierung der Abhängigkeit. Dieser Store liegt
nur im exklusiven Testverzeichnis. Ein weiterer Fall verlangt eine überprüfte
Quellenauswahl und bestätigt, dass STOP auch ein bereits geprüftes Review entfernt.
Das Protokoll weist überzählige Deskriptoren, nichtkanonische Texte und unerwartete
Restdaten ab, ohne ein vorheriges Ergebnis zu überschreiben.

**Grenzen:** Der bestätigte Ablauf ist intern; der Testaufruf an die Ausführung
steht für die spätere frische AOSP-Adminfreigabe und beweist keine ausgeführte
Passwortabfrage. Produktiver Netzwerkabruf, öffentlicher CLI/Binder-Paketweg und
Produkt-SELinux-Aktivierung bleiben offen. Die vier deaktivierten echten
AOSP-CE-Integrationstests wurden nicht aktiviert. Die 130 unveränderten Java-
Prüfungen behalten ihren früheren Beleg `95f2b925`; kein erneuter Java-Lauf.
Der vollständige Zwei-Benutzer-Produktablauf ist damit weiterhin nicht abgeschlossen.

Laufender Gast: Image `c7401f60`, Profil `39d29ee1-7587-4223-8e5d-f9872c910554`, Boot
`6c6dc3df-cf70-4c1b-8f16-f131b5a981a5`. SELinux Enforcing, authentifiziertes ADB und Broker laufen;
Benutzer/Seriennummern 0/10/11, CE-/DE-Schlüsselkennungen und leere Runtime-Kontexte
sind vor/nach beiden Durchläufen unverändert. Benutzer 10/11 bleiben gesperrt.
Kein produktives Abbild, Profilpaar oder sichtbarer Launcher wurde ersetzt.

Nachweise: `out/components-b827ba82/{targeted-tests,native-regression}/`
im primären Workspace; jeweils Protokoll, Ergebnis und Vorher-/Nachherzustand.

| Datei | SHA-256 |
| --- | --- |
| Gezieltes Protokoll | `47fa589e335aca649ed83581f000de74670086597626f6e31b87ed7c0f64df16` |
| Vollständiges Protokoll | `70c57e4b96dd6a0d10939f01cef40ad0b394976aac82409d507cee464e6eaf46` |
| Vollständiges Ergebnis | `27d7e3a8f7bd50587445acba6e2228771e9ee24c28ba37af78942836a7c61d66` |
| Zustand vor/nach Tests | `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a` |
| Native Testdatei | `6c9690d16071c27a0398c437eeb33b6edea0a2172c088765bbaa623ffe2e9e66` |

## Eigener Planungsauftrag mit Abbruchverwaltung: 43 und 337 Tests bestanden

Komponentenstand **33482ef26d714e9e8265c20f6e3d81e629e7cfa2** besteht am 30. September 2026
im lokalen Mac-QEMU **43/43 gezielte Prüfungen** (2026-09-30T11:27:25Z, 11,737 Sekunden)
und anschließend **337/337 aktivierte native Tests aus 41 Gruppen**
(2026-09-30T11:33:10Z, 131,647 Sekunden).
Buildlauf `identity-20260930T112502Z-33482ef2-JWjK0R`, Invocation
`1120b20af1eb4b0c98c07a7074034d0e`, kompiliert inkrementell in 34 Sekunden auf
`aegis-build`. [Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T112621Z-33482ef2-33482ef2-IoSOhn).

Der Broker registriert den Planungsauftrag vor dem Start des Arbeiters. Eine
bereits geprüfte Generationsauswahl geht direkt unter derselben Auftragskennung
an den Planer; Quellenidentität und Mount bleiben intern gebunden. Kopieren der
Paketmetadaten und APT laufen im isolierten Kind außerhalb der kurzen Zulassung.
Der Planer verwendet Werksprogramme, feste Quellen/Schlüssel und ausschließlich
den Paketstatus der ausgewählten Generation. Eine abweichende geerbte umask
ändert die benötigten Mountberechtigungen nicht.

Poll liefert nur Status. Auch ein eingesammeltes Ergebnis (`Collected`) bleibt
als privates Verzeichnis im Auftrag registriert und sperrt einen zweiten Auftrag
desselben Antragstellers. Bei STOP_USER, Abbruch, HELLO und Verbindungsverlust
behält der Broker Arbeiter, Cgroup und Ergebnis bis zur bestätigten Beendigung. Ein abgelaufenes
Zeitbudget darf die Ressourcen nicht vorzeitig freigeben. Identität und
Seriennummer werden für Poll und Abbruch gemeinsam geprüft; Auftragsnummern
werden innerhalb desselben Besitzers nicht wiederverwendet. Das begrenzte interne
Protokoll weist zusätzliche Dateideskriptoren ohne Leck ab.

Eine abgetrennte Generationsansicht ist nur für einen Startversuch verwendbar:
Nach temporärem Einhängen/Aushängen muss ein Folgeauftrag eine neu geprüfte
Ansicht beziehen. Der Wiederholungstest prüft diese Neuauswahl nach STOP und eine
höhere Auftragskennung. Die produktive Helferdatei verlangt ihren vorgesehenen
SELinux-Kontext; der getrennte Testhelfer behauptet diese Produktzulassung nicht.

Die neun zusätzlichen Tests prüfen tatsächliches signiertes Offline-APT,
Metadatenbegrenzung, Abbruch, Teilfehler, Fristablauf, falsche Identität,
STOP nach Ergebnis, direkte Auswahlübergabe und Deskriptorbereinigung.
**Grenze:** Der private Netzwerkraum bleibt ohne produktiven Internetzugang.
Öffentliche CLI/Binder-Operationen, Produkt-SELinux-Einbindung und die Verbindung
des intern behaltenen Ergebnisses mit erneuter Gültigkeitsprüfung, gebundenem Plan,
frischer AOSP-Adminfreigabe und Ausführung fehlen weiterhin. `Collected` ist keine
Freigabe oder Installation. Der vollständige Zwei-Benutzer-Produktablauf ist offen.

Gast bleibt Produktimage `c7401f60`, Profil `39d29ee1-7587-4223-8e5d-f9872c910554`, Boot-ID
`6c6dc3df-cf70-4c1b-8f16-f131b5a981a5`. SELinux Enforcing, authentifiziertes ADB, laufender Broker,
Benutzer/Seriennummern 0/10/11, CE-/DE-Schlüsselkennungen und leere Runtime-Kontexte
sind vor und nach beiden Läufen unverändert. Benutzer 10/11 bleiben gesperrt.
Vier ausdrücklich deaktivierte AOSP-CE-Integrationstests wurden nicht aktiviert.
Die 130 unveränderten Java-Prüfungen verwenden ihren früheren Beleg `95f2b925`;
sie wurden hier nicht erneut ausgeführt. Kein Produkthelfer oder Launcher ersetzt.

Nachweise im primären Workspace: `out/components-33482ef2/` mit
`targeted-tests/` und `native-regression/`, jeweils Ergebnis, Protokoll und Zustand
vor/nach dem Lauf. Alle zehn nativen Testhelfer wurden vor Ausführung gehasht.

| Datei | SHA-256 |
| --- | --- |
| Gezieltes Protokoll | `06511945b0009c51d1cb3b973fe75c16c10f3dfde40e99de86a1b2f3344e6052` |
| Vollständiges Protokoll | `17ba7c5273e2eddcbed299b6d52aaa85832cbac019b0d8ae485a687d7d9f32c3` |
| Vollständiges Ergebnis | `756a6c9cce2ab7dd2d78f706db99c34bbf30affc96fad3b6ace5551a0f0c27ed` |
| Zustand vor/nach Tests | `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a` |
| Native Testdatei | `43cb574d2811af2d886c29e0be45f06288ac209ecbd22128d498485cd1a8180d` |

## Paketplan mit authentifizierten Index- und Archivbelegen: 34 und 328 Tests bestanden

Komponentenstand **9e705281d73bc08d9ae1df4e9f58cc68203d102c** besteht am
30. September 2026 im lokalen Mac-QEMU **34/34 gezielte Prüfungen**
(10:37:02 UTC, 13,146 Sekunden) und anschließend **328/328 aktivierte native
Tests aus 39 Gruppen** (10:39:16 UTC, 103,151 Sekunden).
Buildlauf `identity-20260930T103450Z-9e705281-fgRXtC`, Invocation
`0d37a8821c4548e7b44586c30418312e`, kompiliert in 25 Sekunden auf `aegis-build`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T103602Z-9e705281-9e705281-1Yv5KD).
Der vorherige Lauf `1bbb5658` scheiterte an einem Variablennamen im neuen Test;
getestet und übernommen wurden ausschließlich die korrigierten Komponenten.

Der unveränderliche Planer verknüpft jetzt seinen erfolgreichen APT-Abruf mit
nativen Repository- und Archivbelegen. `PackageCollectAptEvidence` läuft nach
dem Ende aller APT-Kinder im privaten, unveränderten Auftragsverzeichnis. Es
prüft die exakten Indexziele und deren Dateinamen, den vorgesehenen Schlüsselpfad,
den vollständigen unkomprimierten Index gegen die authentifizierte Release-
Prüfsumme und jedes ausgewählte Archiv gegen Größe und SHA-256. Doppelte,
komprimierte oder unerwartete Ziele werden abgewiesen. Die Receipt enthält
Repository-Identitäten, Release-/Index-Digests, Gültigkeit, Archivdaten und einen
separaten Digest der festen Konfiguration, Quellen und Schlüssel.

**Die OpenPGP-Authentifizierung bleibt Aufgabe des unveränderlichen APT mit
festen Schlüsseln.** Der native Release-Parser ist kein Signaturprüfer und
`Trusted: yes` allein kein Vertrauensnachweis. Die Bibliothek darf keine
beliebigen Verzeichnisse oder vom Aufrufer gelieferte Metadaten als authentifiziert
annehmen. Der echte Resolver führt vorher den geprüften APT-Update-/Abrufablauf
aus und lässt keine Programme oder Konfigurationen der ausgewählten Generation zu.
Die spätere produktive Besitzverwaltung muss Herkunft, unveränderliche Mounts,
CE-Zulassung, Fristen und Abbruch bis zum Schließen aller Referenzen sichern.

Native Gültigkeitsprüfung und feste APT-Konfiguration begrenzen die Lebensdauer
auch ohne `Valid-Until` auf **120 Tage ab Release-Datum**; ein früheres
`Valid-Until` gewinnt. Das entspricht der
[Max-ValidTime-Verknüpfung in APT 3.0.3](https://github.com/Debian/apt/blob/3.0.3/apt-pkg/deb/debmetaindex.cc).
Ungültige Kalenderdaten, widersprüchliche Wochentage, zukünftige Daten,
doppelte Felder/Indexnamen und übergroße Eingaben werden abgewiesen.
Clear-signed InRelease und getrennt signierter Release-Text werden unterstützt;
die reine Envelope-Parserprüfung behauptet keine Signaturgültigkeit.

Die echten isolierten APT-Fixtures für Installation, Update und abhängige
Entfernung führen die gewonnenen Belege anschließend in `PackageBindAptArchives`.
Dabei bleiben alle ausgewählten Versionen und automatischen/manuellen
Abhängigkeitsmarkierungen erhalten. Nachträglich geänderte, gleich große
Index-/Archivdateien werden ebenfalls abgewiesen; unveränderte Kontrollfälle
bestehen. Auch ein Plan ohne Änderungen prüft noch sämtliche gelieferten Indizes;
er erzeugt dadurch keine Installation oder Freigabe. Die Kontext- und
Quellabbildangaben dieser Bindungsprüfung sind interne Testeingaben, kein Nachweis
einer öffentlich autorisierten Produkttransaktion.

Gast: unverändertes Produktimage `c7401f60`, Profil
`39d29ee1-7587-4223-8e5d-f9872c910554`, Boot-ID
`6c6dc3df-cf70-4c1b-8f16-f131b5a981a5` in
`out/qemu-network-20260930/boot-2`. SELinux Enforcing, authentifiziertes ADB,
Benutzer/Seriennummern 0/10/11, CE-/DE-Schlüsselkennungen und leere Runtime-
Kontexte sind vor/nach beiden Prüfungen identisch. Benutzer 10/11 bleiben gesperrt.
Vier deaktivierte AOSP-CE-Integrationstests sind ausdrücklich nicht ausgeführt;
der unveränderte Java-Code behält seinen früheren 130-Test-Beleg aus `95f2b925`.

Nachweise: `out/components-9e705281/{targeted-tests,native-regression}/` mit
jeweils `result.json`, `native.log`, `before.json` und `after.json`.

| Datei | SHA-256 |
| --- | --- |
| Gezieltes Protokoll | `cc35a05005e127122b7576be78425b66908a9191aa8be46c117c0d3f04045205` |
| Vollständiges Protokoll | `bc869518a1539237dde49d57aaf5ef06a0c0458258a011092a924f4d3a844660` |
| Vollständiges Ergebnis | `2dc0dc9c6d0b814e2d310664e4103d504a6ce73a8eb47f025ddb0b1670451c67` |
| Zustand vor/nach Tests | `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a` |

**Weiter offen:** kontrollierter produktiver Netzwerkabruf und registrierter
Planungsauftrag mit CE-/Abbruchlebenszyklus, Wiederprüfung vor Genehmigung,
öffentliche CLI/Binder-Operationen und frische AOSP-Adminfreigabe. Diese Tests
nutzen weiterhin eine feste signierte Offline-Quelle. QEMU-Netzwerkzugang allein
aktiviert keine persönliche Netzwerkfreigabe oder öffentliche Paketverwaltung.
Der vollständige Zwei-Benutzer-Produktablauf mit Paketverwaltung bleibt offen.

## Unveränderlicher Paketplaner: 9 gezielte und 322 native Prüfungen bestanden

Komponentenstand **8e500d7e9825a7d8f4e05a95c45d5997639d4fc2** besteht im
lokalen Mac-QEMU am **2026-09-30T09:45:44Z** alle **9/9 gezielten Prüfungen**
in 5,457 Sekunden und am **2026-09-30T09:47:40Z** anschließend **322/322
aktivierte native Tests aus 38 Gruppen** in 95,777 Sekunden.
Der Komponentenbuild auf `aegis-build` dauert 27 Sekunden; Lauf
`identity-20260930T094357Z-8e500d7e-bx1OYQ`, Invocation
`b5fb404a5c0a4e1988e58bab84ee1ccf`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T094508Z-8e500d7e-8e500d7e-qD5ERk).

`PackageResolverRun` führt APT-Update, strukturierte Simulation, reinen
Archivabruf und Indexermittlung unter der geprüften unveränderlichen
Debian-Werksbasis aus. Die ausgewählte Generation liefert ausschließlich eine
separate schreibgeschützte Kopie von dpkg-Status und automatischen Markierungen.
Feste Konfiguration, Quellen, Schlüssel und Hook liegen in einem getrennten
schreibgeschützten Mount. Der Test verändert absichtlich APT und dessen
Konfiguration im ausgewählten Abbild; die Planung gelingt trotzdem anhand der
richtigen Metadaten. Diese ausgewählten Programme werden nicht eingebunden.

Der Planer vergleicht sämtliche Simulationseffekte mit dem Downloadlauf,
bewahrt automatische Markierungen und bestätigt den unveränderten Eingangszustand.
Er ruft dpkg nicht auf. Seine Phasen und Status unterscheiden Fehler von einem
vollständig eingesammelten Ergebnis; `Collected` ist keine Freigabe oder Installation.

- Echte Installation, Aktualisierung und abhängige Entfernung ergeben die
  erwarteten App-/Bibliotheksversionen und Markierungen. Archivbytes entsprechen
  den auf dem Build-Server erzeugten signierten Testdaten.
- Unsigned- und abgelaufene signierte Quellen werden im Update abgewiesen.
  Je ein verändertes Byte im authentifizierten Index bzw. Archiv verhindert das
  Ergebnis. Die konkrete fehlgeschlagene URI, Phase, Exitstatus 100 und fehlende
  endgültige Datei werden geprüft; ein gültiger positiver Kontrolllauf besteht.
  APTs `copy:`-Methode meldet bei diesen Hashfehlern nur „Undetermined Error“.
- Eine veränderte feste Konfiguration wird schon vor APT abgewiesen. Pfade,
  Optionen und mehrdeutige APT-Suffixoperatoren werden nicht als Paketnamen akzeptiert.
- Isolierte PID1-/Benutzer-/Mount-/Netzräume, feste UID-Abbildung, Capability- und
  Seccomp-Begrenzung; die Testhülle besitzt eine 1-GiB-Cgroup und begrenzte Tmpfs-
  Ablagen. Kinder werden vor dem Lesen der Ergebnis-FDs beendet und eingesammelt.

**Grenze:** Der Engine-Kern ist im getrennten Entwicklerroot-Gerätetest geprüft.
Der Test verwendet ein tatsächlich signiertes lokales `copy:`-Repository,
keinen produktiven Netzwerkabruf. Die produktive Hülle muss Mount-/Quellen-
Provenienz, Fristen, Abbruch und CE-Lebenszyklus besitzen sowie die verifizierten
Release-/Index-/Archivbefunde an `PackageBindAptArchives` anbinden. Diese Verbindung,
öffentliche CLI/Binder-Operationen und frische AOSP-Adminbestätigung fehlen noch.
Kein Produkt-Paketendpunkt wurde aktiviert. Die vier explizit deaktivierten
AOSP-CE-/Paket-CE-Prüfungen sind nicht Teil der 322 aktivierten Tests.
130 unveränderte Java-Prüfungen behalten ihren älteren Beleg `95f2b925`;
sie wurden in diesem Lauf nicht erneut ausgeführt.

Produktgast `c7401f60`, Profil `39d29ee1-7587-4223-8e5d-f9872c910554`, Boot
`112b706c-2e59-4845-8c90-376f1e7fc048` und AVB-Beleg bleiben gleich.
Enforcing/Broker laufen; Benutzer, Seriennummern, CE-Sperren, Schlüsselverzeichnisse
und Runtime-Cgroups sind vor/nach beiden Läufen bytegleich. Kein Benutzer wurde
entsperrt oder gelöscht; keine Produkthelfer und kein sichtbarer Launcher ersetzt.

Belege im primären Workspace unter `out/components-8e500d7e/`:

- `targeted-tests/result.json`: `4a9bbceeafe8d8b169be31aadbc689ebfbdbeaaf2fbb729342476e0523c12728`
- `targeted-tests/native.log`: `67bfcbdd19ecbf18846c9039068bca1ad1cf6e648792df612d6faeb67f283c1c`
- `native-regression/result.json`: `c9cdb85826a1326fdbe1b63a40b7b09ba9316c4fa6cbf7293a61208f18b1a764`
- `native-regression/native.log`: `b78dee765333c448b80d8e6260e5b5d3ed70f9d8ca5ef3dc4060f9b24341e364`
- native Testdatei: `d170f201c7cd61acac38e22d657cd0eac1f5f9b7d82c18807f3053d2a5691377`
- Namespace-Probe: `b30473f314a345153cc12f204f9c2c46553de4bd695544e0e1984f09defadc17`
- Vorher/Nachher-Register: `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`

## Ausführungsprüfung cacb1718: 23 gezielte und 313 native Tests bestanden

Komponentenstand **cacb171824b9586bc3a63dc99368f0f5a3b3b1c0** besteht im lokalen
Mac-QEMU am **2026-09-30T09:04:18Z alle 23 gezielten Prüfungen** (9,308 s) und
am **2026-09-30T09:07:39Z alle 313 aktivierten nativen Tests** aus 36 Gruppen
(98,190 s). Kein Test wurde innerhalb dieser Läufe übersprungen. Die vier
bereits ausdrücklich deaktivierten Fälle `DISABLED_RuntimeCeAosp` (3) und
`DISABLED_RuntimePackageCe` (1) wurden nicht aktiviert und sind kein Bestandteil
dieses Nachweises. **Keine öffentliche Paketverwaltung oder Produktaktivierung.**

Build ausschließlich auf `aegis-build`: Lauf
`identity-20260930T090140Z-cacb1718-HloAVV`, Invocation
`81307238b29842bba6b139747e44c915`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T090248Z-cacb1718-cacb1718-uiwrSV).
Entwicklung lokal im Arbeitsbaum, Quell- und Artefakttransport über GitHub.

Die vorherigen drei Fehler sind behoben. Diagnosebuild `92811c94` erhält die
begrenzte strukturierte APT-Rückmeldung in der exklusiven Kandidatenkopie; sein
Log belegt `search-terms: []` bei ausschließlich lokalen Debianarchiven. Der
Arbeiter verlangt dafür jetzt genau diese leere Liste. Entfernung verlangt
weiterhin die exakten Paketnamen. Vollständige Effekte, Zielversionen,
Architekturen, Anfangsstatus und automatische Markierungen werden weiterhin
unabhängig mit dem versiegelten Plan verglichen. Fünf neue direkte Gegenproben
prüfen leere Archivsuchlisten, unerwartete Suchbegriffe, falsche Version/Architektur,
fehlende/zusätzliche Effekte und exakte Entfernungsnamen. Der öffentliche
Planungsparser behält seine exakte ursprüngliche Anforderung bei.

Die gebundene Testtransaktion verwendet für ihre separate Ausgangsaufnahme eine
positive Vorbereitungs-ID und kehrt danach zur brokervergebenen Auftrags-ID
zurück. Tatsächlich bestätigt sind nun Installation, Update und Entfernung mit
erhaltenen Konfigurationsänderungen, automatischer Bibliothek und manuellem
Hauptpaket, Ablehnung eines anderen simulierten Versionsziels vor Skriptstart,
Schreibschutz von Hook/Konfiguration gegen reale Maintainerskripte sowie die
registrierte Vorbereitung/Ausführung/Publikation mit unverändertem Freigabedigest.
Die 313er-Regression umfasst auch bestehende Namespace-/Prozessisolation,
Abbrüche und Ressourcenbesitz, Paketgenerationen, private Auswahl, vorbereitete
Quellen und die Signatur-/Index-/Archivfixtures.

Produktgast `c7401f60`, Profil `39d29ee1-7587-4223-8e5d-f9872c910554` und Boot
`112b706c-2e59-4845-8c90-376f1e7fc048` bleiben unverändert. SELinux Enforcing,
Broker aktiv, nur Benutzer 0 entsperrt; Benutzer/Seriennummern, CE-Sperren,
Schlüsselverzeichnisse und persönliche Runtime-Kontexte sind vorher/nachher
bytegleich. Keine Produktbinärdateien ersetzt, kein Neustart, kein sichtbares
Fenster geöffnet. Die 130 unveränderten Java-Prüfungen wurden nicht erneut
ausgeführt; ihr gesonderter Nachweis `95f2b925` wurde auf Hash und unveränderte
Java-Quellen geprüft.

Belege im primären Workspace unter `out/components-cacb1718/`:

- `targeted-tests/result.json`: `be4c68c4093329143965f118f17daf21587ea8fc1b96e3c2cfc49e9346623225`
- `targeted-tests/native.log`: `dfb5e6435aa787b715cca593b63aaa5e68d9ef356f1d6af0a5494cbd0f992c17`
- `native-regression/result.json`: `a34b2acadc4936220ba533081f73a8b59dc9a09ab39898ec5ffc07fb632e0d3d`
- `native-regression/native.log`: `d0fb6b0e2ab14eb4c90240205e161352ecca16edb22dd38eed9419301e6e875b`
- Native ELF: `33615bfad5d28af8d666f0bdcefc580faafe770fc8eef2f3097e187265967a1b`
- Beide Vorher-/Nachher-Paare: `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`

**Nächster Integrationsschritt:** den unveränderlichen produktiven Planer samt
signaturgeprüfter Quellen-/Archivbeschaffung, Auftragsbesitz und Ablaufprüfung
an den vorhandenen Ausführungsweg anschließen. Danach sessiongebundene CLI/Binder-
Operationen und frische AOSP-Adminbestätigung verbinden und den gesamten
Zwei-Benutzer-/Logout-/Neustartablauf mit gemeinsamen und privaten Paketaktionen
im gebauten Produktimage prüfen. Entwicklerroot-Fixtures ersetzen diese Prüfung
nicht. Die folgenden Einträge sind historische Belege ihres jeweiligen Stands.

## Ausführungsprüfung 207d7aa7: kompiliert, 15/18 Tests, noch nicht freigegeben

Stand **207d7aa7f24e06ad1b02fe44dcfa1598eb21f892** wurde auf `aegis-build`
übersetzt: Lauf `identity-20260930T075949Z-207d7aa7-i0zgnp`, Invocation
`ffcb3d5f38e84305a500f065f8cc456e`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T080415Z-207d7aa7-207d7aa7-PSIMik).
Der lokale QEMU-Lauf am **2026-09-30T08:08:03Z** besteht **15 von 18 Prüfungen**
in 4,088 Sekunden. **Kein vollständiger Erfolgsnachweis, keine Produktaktivierung.**

Der neue Arbeiter erhält die versiegelten erwarteten Paketversionen und
Abhängigkeitsmarkierungen über Ausführungsprotokoll 2 / Vorbereitungsprotokoll 3.
Ein separates schreibgeschütztes Tmpfs enthält den statischen APT-Hook und die
feste Konfiguration. PID1 soll anfänglichen Status/automatische Markierungen,
strukturierte Offline-Simulation und vollständigen resultierenden installierten
Paketbestand unabhängig vergleichen. Der Produkt-Einstieg verweigert fehlenden
Review; nur die Entwickler-Testprobe unterstützt die alten mechanischen Fixtures.

Die zwölf direkten nativen Bestandsprüfungen bestehen: exakter Effekt,
unerwartetes zusätzliches Paket, veränderte fremde Version, entfernter Hold,
Zwischenzustand, verlorene automatische Markierungen, Duplikate, gleich große
anfängliche Manipulation, Symlink und FIFO. Drei reale Negativfälle bestehen:
veralteter Status-/APT-Beleg und andere simulierte Zielversion. Die letzte
Ablehnung ist bis zur Korrektur des positiven Simulationspfads kein isolierter
Beweis für genau diese Versionsabweichung.

Offene Fehler:

- Der reale positive Install-/Update-/Remove-Test und der Test gegen Austausch
  von Hook/Konfiguration enden bereits nach erfolgreicher APT-Simulation mit
  `ESTALE (116)`, bevor das Installationsskript läuft. Die strukturierte
  Simulation muss diagnostiziert und gegen den tatsächlich normalisierten
  APT-Aufruf geprüft werden. Markierungsübernahme und Schreibschutzangriffe
  wurden damit noch **nicht** erreicht und sind noch nicht nachgewiesen.
- Die gebundene Publikationsfixture scheitert in ihrer neuen vorbereitenden
  Ausgangsaufnahme mit `EINVAL`: die Transaktionsfixture setzt `job=0`, während
  der direkte Vorbereiter eine konkrete positive ID benötigt. Für diese
  Aufnahme eine getrennte gültige Fixture-ID verwenden und anschließend zum
  brokervergebenen Auftrag zurückkehren.

Die ersten beiden Builds wurden ebenfalls nicht als Erfolg gewertet:
`a01dafc4` fehlte eine statische Android-Variante von `libcrypto`, `50e3c3f6`
fehlte die direkte Metadatenbibliothek am Testprogramm. Die korrigierte
Buildintegration nutzt `libcrypto_static` ausschließlich für Paket-SHA-256;
eine enge Visibility-Ergänzung ist an BoringSSL-Revision
`ecc1358826150d6a1851c517c325b0e6c0e1b8be` und Original-Builddatei-SHA
`c64b09f64b9fb5a2ba7836c7b964895db74edeef404a8380202bc429f0738430`
gebunden. Vorher/nachher-Prüfung im Builder; keine Änderung an AOSP-
Anmeldeverschlüsselung oder Laufzeit-Sicherheitsrichtlinien.

Produktgast, Boot-ID, Enforcing, aktiver Broker und gesperrte Benutzer bleiben
unverändert. Vorher-/Nachher-Dateien haben weiterhin SHA-256
`427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.
Alle Fehlerbelege bleiben unter `out/components-207d7aa7/targeted-tests/`:

- `native.log`: `78adaae9df3091a1329014e30bb548528019384bd03db458a2463959e2a59641`
- `result.json`: `7a93c721fc43949a4451aa3604140eec50b1c9e341eef65c7c3369ee53edeb03`
- Native ELF: `9b1be0b26ea147624d58a113270f48c1a1b7a208fe65f7a1d87a5bc10ed6d67c`

Die vollständige native Regression sowie Java wurden hier nicht wiederholt.
Öffentlicher Paketbefehl, unveränderlicher produktiver Planer/Netzabruf und
frische AOSP-Adminfreigabe bleiben offen. Der folgende 76/76-Nachweis gehört zum
älteren Stand 6caa5bf2, nicht zum neuen Ausführungsarbeiter.

## Vollständiger interner Freigabeplan: 76 gezielte Gerätetests

Stand **6caa5bf266edd7471c2feb33476fc7ad1a4e079f** besteht am
**2026-09-30T06:53:22Z** im lokalen Hintergrund-QEMU **76/76 Tests**
aus fünf Gruppen, Laufzeit 14,287 Sekunden. Der Komponentenbuild auf
`aegis-build` kompiliert in 32 Sekunden; Lauf
`identity-20260930T065040Z-6caa5bf2-HjNp8P`, Invocation
`75dd0ad29d7641c6b9ba6bfcb9f68717`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T065151Z-6caa5bf2-6caa5bf2-S9zIKh).

Die neue Version-2-Bindung verlangt für jede Änderung ausdrücklich Manual oder
Automatic und für den ursprünglichen APT-Zustand ausdrücklich Absent oder Present.
Unbekannte Zustände werden abgewiesen. Eine vorhandene leere Datei ist nicht
dasselbe wie bestätigte Abwesenheit; Größe/Hash und Markierungen beeinflussen den
Digest. Die unabhängige Python-Referenzcodierung ergibt
`e2481eca1b14ae560b109c61f22eb9599f4ad8d909ddac6a626c832b69e59949`.
Der vollständige validierte Plan bleibt für die spätere Prüfung erhalten.

Der echte APT-Test kopiert den tatsächlichen anfänglichen automatischen Paketstatus
aus seiner geprüften Basis in den getrennten Planer. Simulation und reiner
Archivabruf lassen diesen Zustand unverändert. Die signiert bezogenen App-/Lib-
Archive fließen durch `PackageBindAptArchives` in den vollständigen Review und
die gemeinsame Vorbereitungs-/Publikationskennung. Die automatische Bibliothek
bleibt erfasst; das Ändern nur dieser Markierung erzeugt einen anderen Digest.

- 19 Archiv-/Bindungsprüfungen, davon fünf neue: vollständige Übernahme,
  konkurrierende Änderungsliste/FD-Anzahl, vertauschte oder gleich groß veränderte
  Dateien, abgelaufene Quellen vor IO und Entfernung ohne eingeschleuste Archive.
- 22 Planprüfungen, davon vier neue: fehlende Markierungen, unterschiedliche
  Abwesenheit/Leerzustand, Zustandsgrenzen und vollständiger Review-Erhalt.
- 10 APT-JSON-Parserprüfungen und 24 Namespaceprüfungen bestehen erneut, inklusive
  der tatsächlichen Signatur-/Fristen-/Index-/Archivablehnungen und der neuen Bindung.
- Der echte registrierte Vorbereitung-/APT-/Publikationslauf verweigert jetzt
  zusätzlich einen Start mit geändertem Abhängigkeitsgrund. Der Auftrag bleibt
  vorbereitet; der ursprüngliche Digest startet weiterhin erfolgreich.

**Grenze:** Das ist ein interner Entwicklerroot-Komponentennachweis, kein
öffentlicher Installationsbefehl und keine tatsächliche AOSP-Passwortfreigabe.
Der mechanische Paketarbeiter ist bytegleich und übernimmt automatische
Markierungen noch nicht. Sein unabhängiger Vergleich mit den erwarteten Effekten,
produktiver unveränderlicher Planer mit Netzbeschaffung und Auftragslebenszyklus
sowie öffentliche CLI/Binder-Autorisierung fehlen weiterhin. Der erfolgreiche
Publikationstest verwendet weiterhin seine getrennten synthetischen Archive.
Eine geprüfte Planbindung allein autorisiert oder implementiert diese Schritte nicht.

Nur Testbinärdatei und Namespace-Probe unterscheiden sich vom 67er-Stand;
alle sechs mechanischen Helfer sind bytegleich. Produktgast `c7401f60`, Profil
`39d29ee1-7587-4223-8e5d-f9872c910554` und Boot
`112b706c-2e59-4845-8c90-376f1e7fc048` bleiben gleich. Enforcing und Broker aktiv;
Benutzer, Seriennummern, CE-Sperren, Schlüsselverzeichnisse und Runtime-Cgroups
sind vorher/nachher bytegleich. Keine Benutzer entsperrt/gelöscht, keine Produkt-
helfer ersetzt, kein sichtbares Fenster oder Launcherwechsel. Der ältere
239er-Nativlauf und 130 unveränderte Java-Prüfungen bleiben separate frühere Belege.

Belege im primären Workspace unter `out/components-6caa5bf2/targeted-tests/`:

- `result.json`: `dae6c9fc788abfd39beacd2c29f9fb8787db2bc4e5d94fba8e97f895d39003fe`
- `native.log`: `4a9491d4d4e0be8cc484af2c061a239aca9630e858fae42c576961ac498e4c5c`
- native Testbinärdatei: `4191c21816ddddea8786f97cbba744d6f631cff1c1089224292eb6148f7cc4c5`
- Namespace-Testprobe: `9f336e33e4daa8d0026dedfe371adabeab94010ff223fce59af1e8969b310d00`
- `before.json` und `after.json`: `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`

## Signatur bis zu den Archivbytes: 67 gezielte Gerätetests

Stand **d414758476d32e27d6fe6bc2b092da5f87e54319** besteht am
**2026-09-30T06:37:35Z** im lokalen Hintergrund-QEMU **67/67 Tests**
aus fünf Gruppen, Laufzeit 14,181 Sekunden. Kompiliert ausschließlich auf
`aegis-build`, Lauf `identity-20260930T063544Z-d4147584-T1BDCy`, Invocation
`4ca126e3520b4e678dba0df6a51113a6`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T063638Z-d4147584-d4147584-IgvXgE).

Die neue Prüfung verwendet echte Debianarchive aus dem separat signierten
[Testrepository](https://github.com/simgero/AegisOS/releases/tag/apt-fixture-20260930T060011Z-2d618d0d).
APT bestätigt Release-Signatur, Frist und Packages-Index. Eine inhaltlich
veränderte App mit **identischer Dateigröße** scheitert beim Abruf. Nach Rücknahme
ausschließlich dieser Byteänderung übernimmt APT die Originaldatei erfolgreich.
Der native Adapter liest danach den vollständigen Index gegen den authentifizierten
SHA-256, ordnet exakt App 2 und Bibliothek 2 zu und verifiziert beide heruntergeladenen
Archive unabhängig. Die Bibliothek bleibt als automatische Abhängigkeit erfasst.

- 14 neue Index-/Archivprüfungen decken falsche Indizes, fehlende exakte Version,
  doppelte und gefaltete Identitätsfelder, Pfadausbruch, Größen-/Fristenlimits,
  widersprüchliche Quellen, ungeeignete FDs sowie veränderte/gekürzte Archive ab.
- 10 APT-JSON-Parserprüfungen und 18 Prüfungen der bisherigen Paketplanbindung
  bestehen erneut.
- 24 Namespaceprüfungen schließen den tatsächlichen signierten Metadaten- und
  Archivabruf ein. Unsignierte, verfälschte und abgelaufene Releases, geänderte
  Indizes und fehlende Wunschversionen scheitern. Die Simulation lässt den
  Archivcache leer, Paketstatus unverändert und führt keine Paketskripte aus.
- Ein registrierter Vorbereitung-/Offline-APT-/Publikationslauf mit abgewiesenem
  falschem Freigabe-Digest besteht weiterhin. Die separate tatsächliche
  Install-/Upgrade-/Purge-Prüfung bestätigt Skripte, Konfigurationserhalt und UID 42.

**Grenze:** Der Abruf erfolgt über APTs lokale `copy:`-Quelle im ausschließlich
für diesen Test erzeugten Image, als Entwicklungsroot. Es ist kein produktiver
Netzabruf, keine öffentliche Paketaktion und keine frische AOSP-Adminfreigabe.
Die Skript-/Publikationsfixtures bleiben getrennte Ausführungsbelege; sie
installieren noch nicht über den öffentlichen Weg die gerade signiert bezogenen
Archive. Produktiver unveränderlicher Planer, Quellenbeschaffung und deren
Auftragslebenszyklus, Bindung/Übernahme automatischer Markierungen samt ursprünglichem
APT-Zustand sowie unabhängiger Vergleich der tatsächlich ausgeführten Änderungen
fehlen weiterhin. [Testdaten, Protokoll und Grenzen](../packages/aegis/identity/runtime/package-apt-fixture.md).

Die vorangegangenen Stände werden nicht als Erfolge umgedeutet: `1f910fb8`
scheiterte beim Kompilieren an einer Einrückungswarnung. `4d040072` und
`56b13494` bestanden jeweils 66/67 Tests; ihr Test erwartete bei einer korrekt
abgewiesenen Hashänderung einen spezifischen Fehlertext, den APTs `copy:`-Methode
nicht ausgibt. `0acd86af` bestand ebenfalls 66/67 Tests; dort brach die Simulation
wegen `--no-download` und noch leerem Archivcache vor der positiven Prüfung ab.
Der korrigierte Stand verwendet APTs Simulation mit unabhängig geprüftem leerem
Cache. Die Ablehnungen, Vertrauensregeln und Inhaltsprüfungen wurden beibehalten.
Fehlgeschlagene Logs und ihre eindeutig zugeordneten Fixture-Images bleiben erhalten.

Der laufende Produktgast bleibt `c7401f60`, Profil
`39d29ee1-7587-4223-8e5d-f9872c910554`, Boot
`112b706c-2e59-4845-8c90-376f1e7fc048`, Enforcing und Broker aktiv.
Benutzer, Seriennummern, laufende Benutzer, CE-Sperren, Schlüsselverzeichnisse und
Runtime-Cgroups sind vorher/nachher gleich. Persönliche Benutzer bleiben gesperrt;
kein Produkthelfer wurde ersetzt. Android-/KeyMint-Paare und sichtbarer Launcher
bleiben erhalten, das Fenster geschlossen. Der ältere 239er-Nativlauf und 130
unveränderte Java-Prüfungen sind **getrennte frühere Belege**, kein wiederholter
Gesamtlauf dieser Komponentenrevision.

Belege im primären Workspace unter `out/components-d4147584/targeted-tests/`:

- `result.json`: `bbb307160176194f4ab833d7254a0517b69bf41318fc45ab67809af085dfefa4`
- `native.log`: `86493821b92d8610fd707fe096bb7b79cb27f58ccfd1e9be659cc353f2043179`
- native Testbinärdatei: `e884f7aa382a48f951538c700afb871381c5f7ace3520300af6bfeeaa666a2e2`
- Namespace-Testprobe: `b39f60387009900d3f136d2d51f3eb231d22711cff2b7e448871082558e67a6e`
- `before.json` und `after.json`: `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`

## APT-Auflösung mit signierten Testmetadaten: 53 gezielte Gerätetests

Stand **322ac8a2e5eacc59553d1cc665cda7aa46bf4cb5** besteht am
**2026-09-30T05:52:41Z** im lokalen Hintergrund-QEMU **53/53 Tests**
aus vier Gruppen, Laufzeit 14,580 Sekunden. Die Kompilierung auf `aegis-build`
dauerte 6:28 Minuten; Lauf `identity-20260930T054425Z-322ac8a2-VuHGXs`,
Invocation `4abda1fc9633426385f885601e742dea`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T055130Z-322ac8a2-322ac8a2-7saoCF).

- 10 neue Parserprüfungen: ausgewählte statt angebotener Version, genaue
  Abhängigkeiten, automatische Markierung, Upgrade/Downgrade, abhängige Entfernung,
  unveränderte No-op-Ergebnisse sowie Ablehnung widersprüchlicher, doppelter,
  fremder, unvollständiger und übergroßer Protokolldaten.
- 24 Namespaceprüfungen einschließlich der erweiterten echten APT-Prüfung.
- 18 erneut bestandene Prüfungen der eindeutigen Paketplanbindung.
- Ein vollständiger registrierter Vorbereitung-/Offline-APT-/Publikationslauf,
  der einen abweichenden Freigabe-Digest vor der Ausführung ablehnt.

Der echte APT-Lauf akzeptiert das mit einem weggeworfenen Testschlüssel signierte
Release und den passenden Packages-Index. Frische, getrennte Indexverzeichnisse
verhindern bei den negativen Fällen einen Erfolg aus alten Daten. Fehlende
Signaturen werden abgewiesen; veränderte Release-Bytes erzeugen die bestätigte
sqv-Meldung „Message has been manipulated“. Ein korrekt signiertes abgelaufenes
Release scheitert an seiner Frist, ein veränderter Packages-Index am SHA-256.
Die nicht vorhandene Version 999 scheitert ohne fertige Planmeldung.

Bei der Installation wird ausschließlich `aegis-probe-app=2` angefordert. APT
wählt selbst `aegis-probe-lib=2` als automatische Abhängigkeit. Ein späteres
Upgrade plant beide Pakete von Version 1 auf 2; das Entfernen der Bibliothek
plant auch die abhängige App ein. Alle drei strukturierten Meldungen werden vom
neuen Adapter gelesen. Während der Simulation bleibt der Paketstatus bytegleich
und kein Installationsskript läuft. Im getrennten tatsächlichen Offline-APT-Test
bestehen Installation, Upgrade, Konfigurationserhalt, Skriptaufrufe, technische
UID 42 und Purge weiterhin.

**Grenze:** Dies ist ein isolierter Komponententest als Entwicklungsroot mit
separaten Testquellen. Die Metadaten sind wirklich signiert, ihre Archiveinträge
haben aber Platzhalter-Prüfsummen und werden nicht heruntergeladen. Damit ist
noch keine Signatur-bis-Archiv-Prüfkette im Produkt nachgewiesen. Unveränderlicher
produktiver Planer, vertrauenswürdige Quellenbeschaffung, Auftrag/Lebenszyklus,
Bindung und Übernahme automatischer Paketmarkierungen, unabhängiger Vergleich
der tatsächlichen Effekte sowie öffentliche CLI mit frischer AOSP-Adminfreigabe
fehlen weiterhin. Kein produktiver Paketendpunkt wurde aktiviert.
[Testdaten, Protokoll und Grenzen](../packages/aegis/identity/runtime/package-apt-fixture.md).

Der Gast bleibt auf `c7401f60`, Profil `39d29ee1-7587-4223-8e5d-f9872c910554`, Boot
`112b706c-2e59-4845-8c90-376f1e7fc048`, Enforcing und Broker aktiv. Benutzer,
Seriennummern, laufende Benutzer, CE-Sperren, Schlüsselverzeichnisse und
Runtime-Cgroups sind vorher/nachher gleich. Beide persönlichen Benutzer bleiben
gesperrt. Die vorhandenen Android-/KeyMint-Paare und der sichtbare Launcher
bleiben erhalten. Nur Testbinärdatei und Namespace-Testprobe unterscheiden sich
vom vorherigen Komponentenlauf. Die übrigen sechs mechanischen Helfer sind
bytegleich. 130 unveränderte Java-Prüfungen und der frühere 239er-Nativlauf sind
**separate ältere Belege**, kein hier wiederholter Gesamtlauf.

Belege im primären Workspace unter `out/components-322ac8a2/targeted-tests/`:

- `result.json`: `132a48c55854b3c420b7861176508b65b533a2aad0a55c43afe5a146655a85bb`
- `native.log`: `f30d1a64fd7a3814b316b8eb7e101726180b1db677d599fd2d7ad1f788c1fc1e`
- native Testbinärdatei: `306c6bdc0204788d09a8e3056db43f856e0d85b78a106a80374475f6c89027c8`
- `before.json` und `after.json`: `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`

## Aufgelöster Paketplan: 19 gezielte Komponententests

Stand **e9361b4ecbc39d8ff69f937b4d1bf55612bced66** besteht am
**2026-09-30T05:19:21Z** im lokalen Hintergrund-QEMU **19/19 gezielte Tests**.
Kompiliert ausschließlich auf `aegis-build`, Lauf
`identity-20260930T051134Z-e9361b4e-8FvYqf`, Invocation
`e8a22662349e43e6a24ae7f669e843b0`.
[Verifizierter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260930T051829Z-e9361b4e-e9361b4e-4greeJ).

18 Prüfungen decken die eindeutige Planbindung und ihre Ablehnungsfälle ab:
Antragsteller/Seriennummer, Bereich, Ausgangs-/Zielgeneration, ursprüngliche
Versionsanforderung, Quellenrichtlinie, Repository-/Status-/Archivbelege und
geplante Änderungen gehören zu demselben Digest. Falsche Wunschversion,
abgelaufene Metadaten, veränderte gemeinsame Basis, fremde Architektur,
Pfad-/Optionsinjektion, doppelte/unsortierte Einträge, unzulässige Größen und
nicht darstellbare gemischte Aktionen scheitern ohne Ausgabeänderung. Eine
unabhängig in Python codierte Version-1-Prüfzahl stimmt überein; der Epochenteil
wird in APT-Archivnamen korrekt als `%3a` abgebildet.

Der 19. Test führt die abgeleiteten Vorbereitung-/Veröffentlichungsdaten durch
den wirklichen nativen Besitzer. Eine veränderte Versionsanforderung erzeugt
einen anderen Digest; dessen Startversuch scheitert mit ESTALE und lässt den
vorbereiteten Auftrag bestehen. Der richtige Digest startet anschließend echtes
Offline-APT mit Test-App und exakter Bibliotheksabhängigkeit. Vollständige
Publikation und tatsächlicher Datei-/Konfigurationsinhalt werden geprüft. Alle
zugehörigen temporären Ressourcen werden regulär freigegeben.

**Grenze:** Repository-Belege dieses Fixtures sind synthetische Metadaten. Es
prüft keine Signaturen, keine tatsächliche Repository-Auflösung und keine frische
AOSP-Adminfreigabe. Der Test läuft als Entwicklungsroot; kein öffentlicher
Paketendpunkt oder produktiver Paketarbeiter wurde aktiviert. Die Paketplan-
Bindung ersetzt nicht den noch fehlenden unabhängigen Vergleich der geplanten
mit den durch APT ausgeführten Effekten.

Der laufende Produktstand bleibt `c7401f60`, Profil
`39d29ee1-7587-4223-8e5d-f9872c910554`, Boot
`112b706c-2e59-4845-8c90-376f1e7fc048`, Enforcing und Broker weiter aktiv.
Vor-/Nachvergleich der Benutzer, Seriennummern, laufenden Benutzer, CE-Sperren,
Schlüsselverzeichnisse und Runtime-Cgroups ist bytegleich. Kein Benutzer wurde
angelegt, gelöscht oder entsperrt; Profilpaare und sichtbarer Launcher bleiben
erhalten. Sieben mechanische Helfer sind bytegleich mit dem früheren
239-Test-Stand. Die 239 nativen Basistests und 130 unveränderten Java-Tests sind
**getrennte frühere Belege**, kein hier wiederholter 258er-Gesamtlauf.

Belege im primären Workspace unter `out/components-e9361b4e/targeted-tests/`:

- `result.json`: `e2c5ff863fc42e791a97e86a9dad7cc11cc8a186f179e0fcb8cb3c7b336fbbaf`
- `native.log`: `1780584e7820f93bd7a22f298552dfcfb40b8d0b1d7215e590ce1aee72b5e2c6`
- native Testbinärdatei: `92007cb62c2c35c42423cc8e44b4eedf333b43799dfcb1388d9a8785fec40251`
- `before.json` und `after.json`: `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`

## Vollimage c7401f60: private Generationen im echten Zwei-Benutzer-Ablauf

Am **2026-09-30T04:53:00Z** ist die integrierte Auswahlprüfung von
`c7401f60f2266c1fdeff5a92003b92d9ddf789f2` im lokalen Mac-QEMU abgeschlossen.
Der [Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260930T041126Z-c7401f60-433faf9c)
gehört zum Lauf `aosp-20260930T041126Z-c7401f60-433faf9c`, Invocation
`964c7c2285c648f4b0e2ae4a5427d20c`. Upload und alle 21 lokalen Assets sind
verifiziert. Die gezielte mounton-Korrektur behebt den unten dokumentierten
Startfehler, ohne SELinux abzuschalten oder die laufende Policy zu verändern.

Das eigenständige Profil `39d29ee1-7587-4223-8e5d-f9872c910554` wurde mit demselben
Android-/KeyMint-Paar neu gestartet. Boot-IDs:
`84d48333-63ec-4b9c-9063-0e7a9632c998` und
`112b706c-2e59-4845-8c90-376f1e7fc048`; AVB-Digest in beiden Boots:
`0c6c6537e71732a510eb04a150b5fe11fc4e4b7015951ea1fba3a796c57e9797`.
Beide Boots bestätigen tatsächliches dm-verity, FBE, authentifiziertes ADB,
Enforcing, Brokerdomäne und Cgroup-Delegation. In beiden vollständigen
Laufzeit-Logs und im abschließenden Kernelpuffer wurden keine Runtime-/Paket-AVCs
oder Runtime-Fehlermeldungen beobachtet.

Nachgewiesen sind:

- Alpha 10/10 und Beta 11/11: erste echte AOSP-Anmeldung ohne vorherigen
  Fehlversuch, stabile Sitzung und tatsächlicher GNU-Start. Pro Kontext gelten
  eigene UID-Zuordnung, Namensräume, HOME, schreibgeschützte Softwarebasis,
  entfernte Capabilities und Seccomp. GNU/glibc, Bash und APT sind vorhanden.
- Private vollständige Testgenerationen im jeweiligen CE-Speicher werden vom
  produktiven Start ausgewählt. Acht unabhängige Beobachtungen verbinden
  Backing-Datei, Loopgerät und tatsächlichen GNU-Mount vor und nach Reboot
  beziehungsweise bestätigen vollständige Freigabe nach Logout.
- Gegenseitige Datei-/proc-Leseversuche und SIGSTOP werden aus den echten
  GNU-Kontexten abgewehrt, während der fremde Prozess vorher und nachher lebt.
  Ein Wechsel über die zweite CLI widerruft die erste aktive GNU-PTY; ihr
  Hintergrundprozess arbeitet weiter. Alphas Logout beendet nur Alphas Kontext;
  Betas Hintergrundprozess arbeitet danach nachweislich weiter. Beide Logout-
  Prüfungen liegen deutlich vor dem natürlichen 30-Minuten-Ende der Testprozesse.
- Logout entfernt Prozesse, Kontext und private Backing-Loops und sperrt AOSP-CE.
  Nach gepaartem Neustart lassen sich beide GNU-geschriebenen Dateien bytegleich
  lesen. Falsches Passwort erlaubt weder CE-Zugriff noch Runtime-Start.
- AOSP-Passwortwechsel für Beta: altes Passwort nach Logout abgewiesen, CE bleibt
  gesperrt; neues Passwort öffnet dieselbe Generation und unveränderte Datei.
  Terminal-Resize und Vordergrund-Unterbrechung funktionieren. In sechs Bootlogs
  wurde kein vollständiges Testpasswort gefunden; der Testtreiber ist beendet.

**14/14 zusätzliche native CE-Probeaufrufe** bestehen. Sie prüfen Publikation,
Wiederöffnung, fremde Seriennummer, registrierte Vorbereitung/Abbruch und wirklich
gesperrte Schlüssel. Diese Proben veröffentlichen Testabbilder als Entwicklungsroot;
sie sind **kein öffentlicher Paketbefehl und kein Nachweis frischer Adminfreigabe**.
Die 239 nativen und 130 Java-Komponentenbelege wurden nicht redundant wiederholt:
die betreffenden Quellen sind gegenüber den bereits geprüften Ständen unverändert.

Belege im primären Workspace: `out/full-build-c7401f60/integration-result.json`,
SHA-256 `a4dcbf1a154a3f76387dab1bdd7b80e17074dc6dcf48b423a834107851b58593`.
Der Beleg enthält Prüfsummen der Ereignisse, Bootprüfungen, CE-Proben und
Mount-Beobachtungen. GNU-Dateihashes: Alpha
`5ebf706722861c13173547a2195fe89ed729a1449445d596dfc8cac57531132b`, Beta
`961282b42166b5851486fcaf9aaabe40704e3a35f6b7f7398488edd3051dcf09`.
Am Ende sind beide persönlichen Benutzer gesperrt, keine persönlichen Kontexte
oder privaten Backing-Loops vorhanden und ausschließlich Benutzer 0 entsperrt.
Alle bisherigen Profilpaare bleiben erhalten; das QEMU-Fenster bleibt geschlossen.

**Noch offen:** öffentlicher Install-/Update-/Remove-Kanal, vertrauenswürdige
Repository-/Abhängigkeitsplanung, frische AOSP-Adminfreigabe für beide Bereiche,
produktive Paketskript-Domäne und APT-Abbruch während AOSP-Logout. Gemeinsame/private
Paketänderungen, Konkurrenz, Aktivierungskonflikte und die vollständige Phase-1-
Abnahme stehen aus. Auch eine Imageaktualisierung eines bestehenden persönlichen
Profils ist hiermit nicht nachgewiesen; der sichtbare Launcher wird nicht umgestellt.
Dieser integrierte Lauf ist ein Teilnachweis, kein Abschluss des Gesamtauftrags.

## Vollimage 6473cf51: Boot bestätigt, erster GNU-Start bewusst nicht abgenommen

Der [vollständige Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260930T033608Z-6473cf51-27531d4b)
ist hochgeladen und durch Rückdownload bytegenau geprüft. Lauf
`aosp-20260930T033608Z-6473cf51-27531d4b`, Invocation
`97e6119b88d14026991b3844a20ecdf7`. Lokal sind alle 21 Assets und AVB-Metadaten
geprüft. Das alte ab38-Profil wurde mit bestätigt sauberem Android-/Helper-Ende
beendet; beide Datenträger bleiben erhalten.

Das neue separate Profil `008b9efe-663a-448b-a3f8-ab42128e1b83` bootet ohne Fenster,
Boot-ID `1eea1952-9fbd-4c72-8086-653b3b947d86`, AVB-Digest
`bd7f16b202eba8125c0aee85bf22d8fd1fb35e977c21e6adbdfcba8e13ef73e5`.
Tatsächliche dm-verity-Tabellen, verschlüsseltes FBE, authentifiziertes ADB,
Enforcing, init-gestarteter Broker und dessen Cgroup-Delegation bestehen die
Startprüfung. Das bestätigt noch keine GNU-Sitzung.

Alpha 10/10 wurde über die echte CLI/AOSP angelegt. Der erste Login hielt CE bis
zur Passwortübermittlung gesperrt, entsperrte nur Alpha und blieb sechs Sekunden
später authentifiziert. **Der anschließende erste Linux-Start scheiterte**:
SELinux verweigerte dem Broker `dir mounton` auf `/mnt` mit Label
`aegis_runtime_base_file`. Dort liegt bereits die globale geprüfte Werksbasis.
Die neue Auswahl muss ihre geprüfte schreibgeschützte Generation kurz über diesem
Anker im privaten Broker-Mount-Namensraum einhängen, klonen und wieder lösen.
Die native Komponentenprüfung lief als Entwicklungsroot und konnte diese
fehlende produktive Regel nicht nachweisen.

Die Korrektur ergänzt ausschließlich Broker -> Basisverzeichnis `mounton`;
Init/Programme erhalten dafür ein ausdrückliches neverallow. Unverändert bleiben
Pfad, Herkunfts-, Mount-ID- und Namensraumprüfungen, schreibgeschützte Generationen
und durchsetzendes SELinux. Kein Live-Policy-Patch und kein permissiver Ersatz.
Die Korrektur benötigt einen neuen vollständigen Build und den realen Starttest.

Auch der Fehlpfad wurde tatsächlich geprüft: `logout` stoppte Alpha und sperrte
CE; der Kontext verschwand, Cgroup `populated 0`, nur Benutzer 0 blieb gestartet
und entsperrt. Genau ein absichtlich global gehaltener Werksbasis-Loop blieb;
die ausgewählte zusätzliche Referenz wurde freigegeben. Der native Opt-in-Test
`LockedAospKeyCannotOpenOrProvisionPackageStorage` bestand mit Alpha im wirklich
gesperrten Zustand. Die vollständigen Testpasswörter wurden in keinem der drei
Bootlogs gefunden; der Treiber wird nach dieser Fehlprüfung beendet.
Keine Benutzerlöschung oder Promotion des sichtbaren Launchers.

Belege: `out/full-build-6473cf51/` im primären Workspace.
`integration-rejection.json` SHA-256 `e73e240f5a3d5765e301c3f494a0de4e1b16ed34b20b9e918b112afbacc3e1a2`;
Diagnose `e091e9be1b320013eb9be095ba433c98beb6fb4899e4af90d5bb22086144cdbd`;
Ereignisse `efe840fe2617d50762e3966b6e36002d0b1b908be145cb13cf371cab44fcc4c4`;
gesperrter CE-Test `b15841e92c8bb3f369b1925831ff7b49575af6b62e0a6c063ebc131cd6973745`.
Die Produktintegration bleibt nicht abgenommen; persönliche Generationen,
GNU-Ausführung und Persistenz über Reboot stehen für den korrigierten Stand aus.

## Produkt-Bootstrap registriert die Generationenauswahl: 6473cf51

Stand `6473cf51871f3bc73eb8afe979fa3bbff82ede3d` wurde auf `aegis-build`
kompiliert und über [GitHub](https://github.com/simgero/AegisOS/releases/tag/components-20260930T033157Z-6473cf51-6473cf51-JznVgr)
übertragen. Lauf `identity-20260930T033047Z-6473cf51-nyakjv`, Invocation
`b384c12f6c7649669873d1107ad1aeab`. Am **2026-09-30T03:34:11Z bestehen
239/239 native Tests**, 32 Suiten in 82.113 Sekunden, ohne ausgelassenen aktiven
Test. Die vier CE-Opt-in-Tests wurden hier nicht aktiviert. Die Java-Quellen sind
gegenüber `95f2b925` unverändert; dessen 130 Tests wurden nicht erneut ausgeführt.

Das Bootstrap hält nun den exakt aus dem unveränderlichen `system_ext` geprüften
Image-FD samt Receipt, den fest installierten Auswahlhelfer und das exklusiv
gehaltene Zustandsverzeichnis. Der erste START registriert die Auswahl vor
CE-Zugriff und Prozessstart. Fortsetzungen setzen ausschließlich diese Auswahl
fort. Der optionale gemeinsame Store ist ausschließlich `shared-packages` unter
diesem Verzeichnis; nur tatsächliche Abwesenheit erlaubt die Systembasis.
Symlinks, falscher Modus, falsches Label und beschädigte vorhandene Metadaten
werden nicht als Abwesenheit behandelt. Persönliche Auswahl erfolgt anhand der
AOSP-Identität und des tatsächlich entsperrten CE-Schlüssels. Der Helfer liest
Store und Image; sein Lock-FD benötigt kein Schreibrecht.

Produktkonfiguration, feste Helfer-/Store-Labels und präzise SELinux-Regeln sind
kompiliert. Der Auswahlhelfer läuft als vertrauenswürdiger Hash-/Mountprozess im
Brokerbereich; dadurch werden keine Paketprogramme oder Maintainer-Skripte zur
Ausführung freigegeben. Der Ereignisloop reapet registrierte Arbeit einmal pro
Durchlauf auch ohne abfragenden Client. Die fünf neuen Tests prüfen Bootstrap-
FD-Eigentum, einmalige Konfiguration, Registrierung vor fehlendem CE, keine
stillschweigende Basiswahl bei unsicherem gemeinsamem Store und Unveränderlichkeit
nach registrierter Arbeit.

Der ursprüngliche Stand `549d6daa` scheiterte an einer Variablenkollision beim
Kompilieren. `3c09511c` kompilierte und bestand 238/239 Tests: ein Test erwartete
ELOOP, während der Kernel `O_DIRECTORY|O_NOFOLLOW` mit ENOTDIR ablehnte. Die
Korrektur akzeptiert beide Ablehnungen und prüft zusätzlich den tatsächlich
registrierten Failed-Zustand mit demselben Fehler. Kein Lauf schwächte die
Pfadprüfung oder aktivierte einen Fallback.

Belege im primären Workspace: `out/components-6473cf51/targeted-tests/`.
Ergebnis SHA-256 `1cdda1effabec2bb2b773e23a47a89d9dbec8742e8f2ab304248d8bf7a2cca15`;
Native-Log `75c32daf99f7e172e21bc50de9c36a58464640ad4a2d07bf507c6920ca48ac38`;
Native-Binary `caa45c5f0b36a5a97aa0168355aec21c4fe42c4c577a045bccc230917926cd1c`.
Vorher-/Nachherzustand identisch:
`427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.
Das bestehende Profil `0bbb6cf5-951e-43b4-9e08-ec952ab1b6e4` bootet weiterhin
`ab38cf24`; Benutzer Alpha/Beta bleiben gesperrt und nur Benutzer 0 entsperrt.
Der Test ersetzte weder Broker noch Service oder Daten-/KeyMint-Paar.

**Offene Integration:** Das neue vollständige Abbild muss noch im lokalen
Hintergrund-QEMU booten. Tatsächlicher erster START, ausgewählte private
Generation, GNU-Ausführung sowie CE-Sperre/Logout und gepaarter Neustart unter
Enforcing sind mit diesem Bootstrap noch nicht nachgewiesen. Öffentlicher
Paketkanal, vertrauenswürdige Repository-Planung und frische AOSP-Adminfreigabe
für gemeinsame und persönliche Änderungen bleiben zusätzlich offen.
Der sichtbare Launcher bleibt bis zur integrierten Abnahme unverändert.

## Startfortsetzung bleibt an denselben Auftrag gebunden: 95f2b925

Komponentenstand `95f2b925b7c2814cf4dd970ec4f6b7569bd6fe48` wurde auf `aegis-build`
kompiliert und über [den verifizierten GitHub-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260930T030515Z-95f2b925-95f2b925-jt1XQd) bezogen.
Lauf `identity-20260930T030420Z-95f2b925-JaTeKI`, Invocation `996251ed7ced4cb2bc40e6c3f8b89c3b`.
Am **2026-09-30T03:07:33Z bestehen 234/234 native Tests** aus 32 Suiten
in 82.169 Sekunden und **130/130 Java-Tests** in 8.854 Sekunden. Kein aktiver
Fall wurde übersprungen. Die vier grundsätzlich deaktivierten CE-Opt-in-Fälle
wurden in diesem Lauf nicht aktiviert; die beiden gesperrten CE-Prüfungen aus
dem vorherigen Stand bleiben gesonderte Belege.

Der interne Brokerkanal hat nun **Version 3**: START liefert eine 40-Byte-Antwort.
EAGAIN darf nur eine positive, weiterhin registrierte Auswahlkennung begleiten.
CONTINUE_START trägt diese Kennung und ist streng an Benutzer, Seriennummer und
bestehenden Auftrag gebunden. STOP, HELLO oder eine spätere Auswahl machen eine
alte Fortsetzung ungültig. Sie darf weder neue Arbeit registrieren noch auf eine
andere Generation oder die Systembasis zurückfallen. Die Kennung ist reine
Korrelation, keine Anmeldeberechtigung. Jede Anfrage benötigt weiterhin die
frische AOSP-Zulassung. Fehler behalten unvollständige Ressourcen beim Besitzer;
sie dürfen keine erfolgreiche Bereinigung melden. Protokolländerungen gehören
in ein zusammen gebautes Vollimage, nicht in ein teilweise ersetztes laufendes System.

Der AEGIS-Service nutzt diesen Ablauf für `linux start` und vor einer Linux-Shell.
Er kontrolliert vor jedem Versuch außerhalb des Gates den tatsächlichen AOSP-
Benutzerzustand und innerhalb des Gates dieselbe ursprüngliche Sitzung/Binding.
Er wartet zwischen Versuchen außerhalb der CE-Serialisierung. Nur EAGAIN mit der
gebundenen Kennung ist fortsetzbar; native Fehler und Kanalfehler werden nicht
als neuer START wiederholt. Das Warten hat ein Gesamtbudget von zwei Minuten;
jeder native Aufruf bleibt zusätzlich an die kurze Zulassungsfrist gebunden.
Timeout und Unterbrechung behaupten keine Bereinigung. Die bestehende native
Logout-/STOP-Verantwortung bleibt bestehen.

Vor EXEC werden die ursprüngliche Auswahl, Anmeldung und Terminalkapazität
nochmals unter Zulassung geprüft. Ein zwischenzeitlicher STOP darf keinen neuen
Kontext erzeugen. Ein schon bestehender Kontext ohne registrierte Auswahl liefert
weiterhin READY mit Kennung 0; für ihn wird vor EXEC nur STATUS geprüft. Dieser
Übergangspfad ist noch nötig, bis das Produkt-Bootstrap die Auswahl registriert.

Die fünf zusätzlichen nativen Tests prüfen echte SEQPACKET-Antworten, genaue
Paketgrößen/Identität/Kennung, eine tatsächlich eingefrorene Auswahl, falsche
Benutzer/Seriennummern, STOP, HELLO und Wiederanlage unter einer neuen monotonen
Kennung. Ein fehlgeschlagener Hash wird niemals als wartender Start interpretiert.
Die elf zusätzlichen Java-Tests prüfen Parser und Warteablauf, einschließlich
Zugriff auf das reale Serialisierungsgate während der Pause, Widerruf während
der Pause, späterer Anmeldung, unverändertem Auftrag, Gesamtlaufzeit, verspätetem
READY, Interrupt und nicht wiederholtem AOSP-Fehler. Diese Java-Tests injizieren
native Antworten und Anmeldungen; sie ersetzen **keinen integrierten AOSP-/CE-
oder produktiven Binder-/SELinux-Test**.

Belege im primären Workspace: `out/components-95f2b925/targeted-tests/`.
Ergebnis SHA-256 `019b81a1537cc99b59741a613b678961eb593df380bf6888ee299810d871889e`;
Native-Log `c3479a821e0185553c3f0261596d3eca9c0faa543bf3a2b0653ebad864435c37`;
Java-Log `9c522b2164a459de7b8fde7bfd37084b8339d14398e71d0299ddab7a05e55458`;
installiertes Test-APK `950f08706f48acbc090e6a1a39e1c2b3c4843569dca5296be59491b0a56a134d`.
Vorher-/Nachherzustand identisch:
`427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.
Der vorausgehende Stand `4d7c5298` wurde erfolgreich gebaut und transportiert,
aber nicht separat ausgeführt; `95f2b925` ergänzt die Terminalprüfung nach dem Warten.

**Nächste tatsächliche Integration:** Das Produkt-Bootstrap muss den fest
verifizierten Systemabbild-FD samt Receipt und Auswahlhelfer halten, den optionalen
festen gemeinsamen Store sicher auflösen und die CE-Auswahl vor dem ersten START
registrieren. Helferinstallation, präzise SELinux-Rechte und durchgehende
Arbeitsprozess-/Cgroup-Verantwortung sind im Vollimage zu prüfen. Anschließend
müssen echte entsperrte CE-Positivfälle, START/READY und GNU-Ausführung mit gewählter
Generation sowie Logout und Neustart unter realen AOSP-Benutzern bestehen.
Öffentlicher Paketkanal, vertrauenswürdige Repository-Planung und frische
AOSP-Adminfreigabe für gemeinsame **und** private Änderungen bleiben zusätzlich offen.

Das produktive Image samt Broker/Service bleibt `ab38cf24`, Profil
`0bbb6cf5-951e-43b4-9e08-ec952ab1b6e4`, Boot `6e287d8c-a75c-47c1-ba45-b72b96b2122e`.
Alpha 10/10 und Beta 11/11 bleiben gesperrt; nur Benutzer 0 ist entsperrt.
Keine Benutzeranlage, Löschung, Anmeldung, Schlüsselmutation oder Profilmigration.
SELinux bleibt Enforcing, das sichtbare QEMU-Fenster geschlossen und der sichtbare
Launcher bei 2a766ab5. Die Komponentenprüfung ersetzt noch kein lokales Gesamtupdate.

## Registrierte Generation bis START und vollständigem Abbau: 93270f09

Komponentenstand `93270f09d249882c8db7e245ce3ca868009787cf` wurde auf `aegis-build`
gebaut und über [den verifizierten GitHub-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260930T024124Z-93270f09-93270f09-mLlmAo) bezogen.
Lauf `identity-20260930T024005Z-93270f09-wDwLks`, Invocation `312f78f272fe4f0c82d1050d840fd813`.
Am **2026-09-30T02:43:27Z bestehen alle 229/229 standardmäßig aktivierten nativen
Gerätetests**, 31 Suiten in 82.102 Sekunden. Zusätzlich bestehen je eine explizite
Prüfung der tatsächlich gesperrten AOSP-CE-Speicher von Alpha 10/10 und Beta 11/11.
Die unveränderten 119 Java-Tests wurden in diesem Lauf nicht wiederholt.

Der native Besitzer registriert die Auswahl samt Antragsteller, Seriennummer und
monotoner Auftragskennung **vor** CE-Zugriff oder Prozessstart. Statusabfragen
geben ausschließlich Metadaten zurück. Ein fertiger Mount bleibt bis zur Übernahme
oder zum bestätigten Abbau im Register. STOP_USER, HELLO, Verbindungsverlust und
Abschaltung versiegeln auch laufende Auswahlprozesse und behalten die Zuständigkeit
bis zum tatsächlichen Reaping und Schließen aller Mountreferenzen. Ein Timeout
behauptet weder Abwesenheit noch erfolgreiche Schlüsselsperre.

Ein nachfolgend frisch zugelassener nativer START übernimmt die registrierte
Generation. Solange die Auswahl läuft, liefert er EAGAIN; eine gescheiterte Auswahl
darf niemals die Systembasis starten. Bestehende Kontexte behalten ihren Stand.
Für die Kerneloperation open_tree(CLONE) hängt der Broker den ausgewählten Mount
kurz in seinem privaten Namensraum ein, klont die feste Benutzerabbildung und
hängt den temporären Anker vor weiteren Kontextschritten wieder ab. Tatsächliche
Kernel-Mount-IDs verhindern Ersetzung oder Abhängen eines fremden Mounts. Teilstarts
und fehlgeschlagenes Abhängen bleiben ebenfalls bis zum vollständigen STOP registriert.

Neun neue Besitzertests prüfen fertige und eingefrorene laufende Auswahl, fremde
Benutzer/Seriennummern, monotone Aufträge, STOP, abgelaufenen Verbindungsabbau, HELLO,
fehlende CE-Daten, gescheiterte Auswahl und vorübergehende Mountanker. Ein Teilstart
klont den echten ausgewählten Mount und entfernt den Anker, bevor er erwartungsgemäß
an den fehlenden CE-Daten eines unbenutzten Testbenutzers scheitert. Das ist **noch
kein positiver vollständiger START/READY-Nachweis mit dieser Generation**.

Die neue nur lesende CE-Auflösung unterscheidet geprüfte Abwesenheit von beschädigten
privaten Ablagen. Sie verlangt zuvor AOSP-Identität, passende fscrypt-Policy und einen
vorhandenen CE-Schlüssel. Sie erstellt und repariert nichts. Die beiden zusätzlichen
Prüfungen bestätigen wiederholt ENOKEY bei real gesperrten Benutzern und unveränderte
Deskriptorzahlen. Positivfälle für fehlende/pristine Ablagen unter entsperrtem echtem
CE sowie die produktive Auswahl bleiben Teil der noch offenen Integration.

Belege: `out/components-93270f09/targeted-tests/` im primären Workspace.
Ergebnis SHA-256 `b5add8fb1bb30193d8e59746347542c38fd7ea58912642046a1f38c8af5dd62b`;
Rohlog `c9744adffa7f5c9569dd443e05653bf165aff3d3a239ef968edda84e12c09cfc`;
CE-10 `af60f475da4e3b3162f239843382fb6d29340ff50b2b0804855b9fb06ded880a`;
CE-11 `b94dffa63cf0c054c3c44cc2ca95e8308a6d5dac28d863dfb8229cf6c2b1af2f`.
Vorher-/Nachherzustand identisch:
`427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.

**Nächster Schritt:** Auswahl im Produkt-Bootstrap registrieren und den Java-/CLI-Start
so verbinden, dass langsame Hasharbeit außerhalb der kurzen AOSP-Zulassung wartet,
aber jede erneute Anfrage Benutzer, Sitzung und CE frisch prüft. Öffentlicher
Paketkanal, vertrauenswürdige Repository-Planung, frische Adminfreigabe für beide
Bereiche und produktive SELinux-/Zwei-Benutzer-/Logout-/Reboot-Abnahme bleiben offen.
Der Produkt-Broker ab38cf24, Profil `0bbb6cf5-951e-43b4-9e08-ec952ab1b6e4`, Boot
`6e287d8c-a75c-47c1-ba45-b72b96b2122e`, alle drei Benutzer und deren Schlüsselzustand
sind unverändert. SELinux bleibt Enforcing. Das sichtbare QEMU-Fenster bleibt
geschlossen; der sichtbare Launcher bleibt beim geprüften Stand 2a766ab5.

## Schreibgeschützte Paketstände asynchron auswählen: c60d6ea2

Komponentenstand `c60d6ea29d4d25745a38bf21c117521c893e24ae` wurde auf `aegis-build`
kompiliert und über [den geprüften GitHub-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260930T022308Z-c60d6ea2-c60d6ea2-aGt2y4) bezogen.
Lauf `identity-20260930T022200Z-c60d6ea2-4VtflP`, Invocation `5e2310af69c7497c8cbdfb4fe9c30ba7`.
Am **2026-09-30T02:25:18Z bestehen 86/86 native Gerätetests**, zehn Suiten
in 69.723 Sekunden, ohne übersprungene Fälle. Darunter sind die bisherigen
76 Paket-/Rechteprüfungen und zehn neue Fälle zur Runtime-Auswahl.

`PackageRuntimeSelectionStart` verwendet den vorhandenen beaufsichtigten
Vorbereitungsprozess mit einem getrennten, strikt codierten Auftrag. Der Broker
muss ihm vertrauenswürdig verankerte Store-Dateireferenzen und die festgelegte
Systembasis übergeben; es gibt keine CLI-Dateipfade oder übernommene Adminrechte.
Lange Hashprüfungen und Mount-Erstellung laufen im Kindprozess. Rückgaben werden
auf Antragsteller, Seriennummer, Auftrag, Basiskennung, erlaubten Bereich und
Mount-Eigenschaften geprüft. Das private Vorbereitungsprotokoll ist Version 2;
Publisher- und öffentlicher Brokerkanal ändern sich nicht.

Die Auswahl folgt persönlich -> gemeinsam -> unveränderliche Systembasis.
Nur nachgewiesene Abwesenheit oder ein vollständig initialisierter leerer Store
erlauben einen Rückfall. Fehlende Metadaten eines vorhandenen Stores, fremde
Identität, Sperrkonflikte, Prüfsummenfehler oder ein fehlendes ausgewähltes Abbild
werden abgewiesen. Letzteres liefert jetzt `ESTALE` statt des für eine leere
Auswahl reservierten `ENOENT`. Eine persönliche Generation muss exakt zur
aktuellen gemeinsamen Basishash passen; ein Konflikt verlangt eine spätere
explizite Neubasierung, keinen stillen Verlust privater Programme.

Das ausgewählte Abbild wird ohne Kopie direkt als readonly/autoclear-Loop und
readonly/nosuid/nodev/noexec-ext4 eingebunden. `noload` und eine zusätzliche
[Prüfung des sauberen ext4-Abschlusses](https://docs.kernel.org/filesystems/ext4/super.html)
verhindern eine unbemerkte Journal-Wiederherstellung; das ersetzt kein fsck oder
die vorausgehende Paketvalidierung. Der originale abgetrennte Mount-Deskriptor
wird erst nach tatsächlich abgeholtem Kind und geleerter/entfernter Cgroup
übergeben. Abbruch verwirft auch einen bereits in der Antwortwarteschlange
liegenden Mount und bestätigt keine Runtime-Aktivierung.

Die zehn neuen Prüfungen belegen tatsächlich:

- Systembasis bei fehlenden Stores; unvollständiger Store wird abgewiesen,
  korrekt initialisierter leerer Store darf auf die Systembasis zurückfallen.
- Echte gemeinsame Installation und Update: ein offener schreibgeschützter
  Mount zeigt weiter Version 1, eine neue Auswahl Version 2. Schreiben scheitert
  mit `EROFS`; erneute vollständige Hashprüfung bestätigt unveränderte Abbilder.
- Echte private Installation, Update und Entfernung durch den vorhandenen
  registrierten APT-/Publisher-Ablauf, jeweils anschließende readonly-Auswahl.
  Fremder Antragsteller oder falsche Seriennummer werden abgewiesen.
- Persönliche Version 2 hat Vorrang vor gemeinsamer Version 1 mit exakt
  gebundener Basisherkunft. Ohne persönlichen Store bleibt Version 1 sichtbar.
- Falsche persönliche Basisherkunft, ein tatsächlich umbenanntes/fehlendes
  ausgewähltes Testabbild sowie ein von einem anderen Store-Halter gehaltener
  Lock führen zu Fehlern und niemals zu stiller Fallback-Auswahl.
- Abbruch nach beobachtetem Kindende verbraucht den wartenden Mount, schließt
  alle gehaltenen Deskriptoren und hinterlässt eine leere Cgroup. Ungültige
  Identität, Auftrag, Hash, Größe und Store-Deskriptor starten keinen Arbeiter.

Belege im primären Workspace: `out/components-c60d6ea2/targeted-tests/`.
Ergebnis SHA-256 `d8db04a4e6e4e3bdfdded2a18923c2a39974cc38ad79ed90e42f960046312f11`;
Rohlog `2467c9cebf6e2c65d508af90bd35878541f0356decf0af5344a1da93f60ebaeb`;
identischer Vorher-/Nachherzustand `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.

**Grenzen und nächster Schritt:** dies sind native Entwickler-root-Tests in
neuen Testablagen; sie ersetzen keine produktive CE-/Adminprüfung. Die Auswahl
ist noch nicht an `Broker START`, dessen STOP/HELLO-Ressourcenregister oder die
öffentliche CLI angebunden. Vor Aktivierung muss der Broker die Auswahl vor dem
CE-Zugriff registrieren, fehlende private Stores ohne Mutation sicher erkennen,
die Mount-Referenz bei STOP/Logout vollständig schließen und bei einem neuen
Start genau die geprüfte Generation übernehmen. Laufende Kontexte behalten ihre
Generation bis zum kontrollierten Neustart. Frische AOSP-Adminprüfung für beide
Paketbereiche, vertrauenswürdige Repository-/Planauflösung, Arbeiterdomänen und
die vollständige Zwei-Benutzer-/Logout-/Reboot-Abnahme bleiben erforderlich.

Produkt-Broker/Image ab38cf24, Boot `6e287d8c-a75c-47c1-ba45-b72b96b2122e`, Profil
`0bbb6cf5-951e-43b4-9e08-ec952ab1b6e4`, Benutzer 0/0, Alpha 10/10 und Beta 11/11 sowie
Schlüsselzustand sind unverändert; nur Benutzer 0 ist entsperrt. SELinux bleibt
Enforcing. Das sichtbare QEMU-Fenster bleibt geschlossen, der sichtbare Launcher
wird nicht durch diesen Komponentenstand ersetzt. Die übrigen nativen/Java-Suiten
wurden mangels Änderungen in ihren Bereichen nicht wiederholt.

## Vorbereitung, APT, Hashbildung und Veröffentlichung in einem Auftrag: 89adbb55

Komponentenstand `89adbb5534ddeb03196687dabc64fe0f288caeff` wurde auf `aegis-build`
gebaut und über [GitHub mit geprüften Prüfsummen](https://github.com/simgero/AegisOS/releases/tag/components-20260930T020048Z-89adbb55-89adbb55-rzCGqf)
bezogen. Lauf `identity-20260930T015937Z-89adbb55-VUPqV9`, Invocation
`87d71c15765441ac8cf1b3f8a508bdb9`. Am **2026-09-30T02:02:33Z bestehen 76/76 native
Gerätetests**, neun Suiten in 49.754 Sekunden, ohne übersprungene Fälle.

Die Auswahl umfasst 21 Executor-, 13 Vorbereitungs-, neun neue Transaktions-,
neun Publisher-, zehn Store-, acht Broker- und vier Antwortprotokolltests sowie
zwei Rechte-/Aufruferprüfungen. Es läuft weiterhin das lokale Hintergrund-QEMU
mit Vollimage ab38cf24, Profil `0bbb6cf5-951e-43b4-9e08-ec952ab1b6e4` und
Boot `6e287d8c-a75c-47c1-ba45-b72b96b2122e`. Die Komponenten werden als
Entwicklungs-root in neuen, ausschließlich test-eigenen Ablagen ausgeführt.

`BrokerPrepareTransaction` bindet den späteren Store, Herausgeber-Helfer,
Bereich, Antragsteller, Seriennummer, Plan, Abbildgröße und erwarteten bisherigen
Stand bereits vor dem Ausführungsstart. Ein Kandidatenhash darf dort noch nicht
vorgegeben sein. Für bestehende Generationen muss der genehmigte Ausgangsstand
mit der tatsächlich zu kopierenden Generation übereinstimmen. Das Ziel wird
tief kopiert; spätere Änderungen am Aufruferobjekt verändern es nicht.
`BrokerPreparePersonalTransaction` löst den privaten CE-Bereich ausschließlich
über die feste AOSP-Kennung samt Seriennummer des Antragstellers auf und
registriert den Auftrag vor dem ersten CE-Zugriff. Die frische AOSP-Freigabe
bleibt Pflicht des künftigen öffentlichen Aufrufers beim Start.

Nach erfolgreichem APT einschließlich Paket-/Konten-/Dateiprüfung und bestätigtem
Arbeiterabbau startet derselbe Slot den Publisher. Das Abbild stammt nur aus
`candidate.ext4` im weiterhin gehaltenen Arbeitsverzeichnis; CLI-Pfade oder eine
nachträglich gelieferte Datei werden nicht akzeptiert. Der separate Prozess
prüft Eigentümer, Größe und sauberen ext4-Abschluss, berechnet SHA-256 und lässt
den Store beim Kopieren erneut Inhalt und unveränderte Quelle prüfen. Die
[ext4-Abschlussmerkmale](https://docs.kernel.org/filesystems/ext4/super.html)
sind eine zusätzliche Zustandsprüfung, kein Ersatz für Dateisystemprüfung oder
vorangegangene Paketvalidierung. Keine dieser langen Arbeiten läuft innerhalb
der kurzen AOSP-Startzulassung. Alte Generationen bleiben erhalten.

Erst bestätigte Auswahl, tatsächliches Reaping, leere/entfernte Cgroup und
Freigabe aller übernommenen Dateien ergeben `Published` mit gebundener
Generationsbeschreibung. Das private Helferprotokoll verwendet dafür Version 2;
Größe, Basiskennung und gegebenenfalls vorher bekannter Hash werden auch beim
Empfang abgeglichen. Fehlende, unpassende oder unvollständige Antworten bleiben
`Unconfirmed`. Der unveränderte öffentliche Brokerkanal hat weiterhin keinen
Paketendpunkt.

Die neun neuen Transaktionstests belegen:

- Gemeinsame Installation, Update und Entfernung durch den vollständigen
  registrierten Ablauf. Nach jedem Schritt wird die tatsächlich ausgewählte
  Generation erneut gehasht, in eine eigene Prüfarbeitskopie übernommen und auf
  Programmversion beziehungsweise Entfernung geprüft. Konfiguration bleibt bei
  `remove` erhalten. Eine alte geöffnete Generation und ihr Store-Inode bleiben
  nach der nächsten Auswahl identisch. Auftragskennungen steigen weiter.
- Private Installation mit unverändertem Antragsteller und gemeinsamer
  Basisherkunft, obwohl das ursprüngliche Zielobjekt nach Registrierung auf
  einen anderen Benutzer/Bereich umgestellt wird. Der resultierende private
  Store weist andere Eigentümer, Seriennummern und gemeinsamen Zugriff ab.
- Unpassende Identität, Plan, Größe, vorgegebener Kandidatenhash oder falscher
  Ausgangsstand werden vor Vorbereitung und ohne neue Dateien abgewiesen.
- Benutzerstopp vor Ausführung schließt Arbeits- und Store-Referenzen, entfernt
  den vorbereiteten Mount und veröffentlicht nichts. Eine echte nachgelagerte
  Kontenverletzung durch ein Paketskript initialisiert den Zielstore nicht.
- Ein tatsächlicher Auswahlkonflikt nach APT liefert `ESTALE` und belässt die
  vorherige Auswahl unverändert.
- Benutzerstopp und Abbruch erfassen einen nachweislich lebenden Publisher.
  Nur dessen eigene Cgroup wird zur kontrollierten Beobachtung eingefroren;
  `populated 1`, `frozen 1` und noch fehlende Auswahl sind bestätigt. Anschließend
  verschwinden Kind, Gruppe und gehaltene Store-Referenzen. Fremde Abbruchkennung
  greift nicht ein. Ein getöteter Publisher liefert `Unconfirmed`; die separat
  nachgeprüfte fehlende Auswahl wird nicht als allgemeine Rollbackgarantie
  ausgegeben.
- Fehlender echter AOSP-CE-Bereich verbraucht genau eine Auftragskennung,
  hinterlässt aber weder neue Benutzerpfade noch offene Referenzen/Arbeiter.

Benutzer-/Schlüsselzustand davor und danach ist bytegleich: 0/0, Alpha 10/10,
Beta 11/11; nur 0 ist gestartet und CE-entsperrt. Runtime-Kontexte bleiben leer,
SELinux Enforcing, Boot-ID und Produkt-Broker bleiben bestätigt. Keine Anmeldung,
Kontenlöschung, Profilmigration oder Launcher-Ersetzung wurde durchgeführt.

Belege: `out/components-89adbb55/targeted-tests/` im primären Workspace.
Ergebnis SHA-256 `061b8ff78702c114f1be37a83ba8665a58d2ee711edfa1b188d9c520cca99276`;
Rohlog `b2802d9ad141364d1e4d7978afef34362f077dbadb1947dbc4a020efa8ecab67`;
identischer Vorher-/Nachherzustand `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.

**Offen:** öffentliche Paket-CLI und Binder-/Broker-Anbindung mit frischer
AOSP-Adminprüfung für beide Bereiche, vertrauenswürdige Repository-/Planauflösung,
produktive Arbeiterdomänen und Auswahl dieser Generationen beim Runtime-Start.
Diese Tests beweisen noch keine private produktive APT-Transaktion während
AOSP-Logout und keinen Benutzerwechsel/Reboot mit neu aktivierten Generationen.
Die unveränderten übrigen nativen/Java-Suiten wurden nicht wiederholt. Private
Updates/Entfernung und die vollständige Zwei-Benutzer-Abnahme müssen im
integrierten Produkt folgen. Der sichtbare QEMU-Launcher bleibt bis dahin beim
bekannten Stand.

## Paketkonsistenz und AOSP-Kontengrenzen nach realem APT: ef8242a1

Stand `ef8242a19e3c08a04dd254cb6fc49d3b27204952` wurde auf `aegis-build`
kompiliert und über den [geprüften Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260930T014238Z-ef8242a1-ef8242a1-uxM88Q)
bezogen. Buildlauf `identity-20260930T014142Z-ef8242a1-dD01wl`, Invocation
`1ea0cf140cd9417e972aedbcb30d904d`.

Am **2026-09-30T01:43:49Z bestehen 44/44 gezielte native Gerätetests**, fünf
Suiten in 31.489 Sekunden, ohne Skip. Darunter sind 21 Executor-, 13 Vorbereitungs-
und acht Brokerfälle sowie zwei gezielte Rechte-/Hostaufruferprüfungen.
Ausführung nur im lokalen Hintergrund-QEMU, Vollimage ab38cf24, Profil
`0bbb6cf5-951e-43b4-9e08-ec952ab1b6e4`, Boot `6e287d8c-a75c-47c1-ba45-b72b96b2122e`.

Der feste Offline-APT-Auftrag prüft nun nach erfolgreicher Ausführung:

- APT-Abhängigkeiten mit `apt-get check`, dpkg-Metadaten mit `dpkg --audit`
  (auch Diagnosen bei Exit 0 führen zur Ablehnung) und Paketdateien mit
  `dpkg --verify --verify-format=rpm`. Veränderte Konfigurationsinhalte dürfen
  nach `--force-confold` erhalten bleiben. Fehlende/geänderte Programmdateien,
  neue fehlende Dokumentation und beschädigte Paketmetadaten werden abgewiesen.
- Native AEGIS-Vorgaben für Konten, gesperrte Linux-Passwörter, reine `files`-
  Namensauflösung, technische IDs, Dateieigentümer, Setid, Capabilities, ACLs
  und besondere Dateitypen. Die begrenzte Dateibaumprüfung folgt keinen Links
  und öffnet keine FIFOs. Sie läuft vor den nachgelagerten Debian-Prüfungen und
  erneut danach. Ein realer Test ersetzt `/usr/bin/ls` durch eine FIFO; der
  Auftrag endet mit `EPERM`, bevor die Paketdateiprüfung startet.
- Tatsächliches Beenden und Abholen aller Nachkommen, bevor die Prüfung beginnt.
  Ein Paketskript startet einen beobachteten Hintergrundprozess unter UID 42.
  Nur der vertrauenswürdige Namespace-PID1 behält `CAP_KILL` in seinem aktuellen
  Effective-/Permitted-Satz; Bounding, Inheritable und Ambient schließen es aus.
  Ausgeführte Paketskripte behalten nachweislich die bisherigen sechs Rechte
  (`CapEff=0xdb`). Sie erhalten keine zusätzlichen Host- oder Mountrechte.

Der Vorlauf b69d6608 bestand 36/36 Fälle ohne Paketdatei-Abgleich. Die erste
Erweiterung 2fafc631 bestand nur **28/40**: Die gepinnte Debian-slim-Basis lässt
Dokumentation, Übersetzungen und Cache-Unterverzeichnisse absichtlich weg,
während dpkg sie teilweise noch verzeichnet. Diese Herkunft wurde auch im
unveränderten, gepinnten Rootfs-Archiv bestätigt. Es wird keine pauschale
Ausnahme für beliebige fehlende Dateien oder ganze Verzeichnisse verwendet.

Die Korrektur erfasst vor APT ausschließlich bereits fehlende Einträge in
begrenzten Slim-Pfadfamilien und hält die exakten Zeilen im Speicher des
vertrauenswürdigen PID1. Der Nachhervergleich liest keine vom Paketskript
veränderbare Ausnahmeliste zurück. Fehlende Programme werden bereits vorher
abgewiesen; danach ist keine neue Auslassung erlaubt. Tests entfernen eine
zuvor vorhandene Dokumentationsablage, fälschen gleichzeitig das Vorher-Log
und bestätigen trotzdem Ablehnung. Beibehaltene Copyright-Dateien bleiben
verpflichtend. Der Korrekturstand e7728958 bestand 43/43; ef8242a1 ergänzt
anschließend den FIFO-Fall und prüft alle 44 Fälle erneut.

Das entspricht der dokumentierten [Slim-Aufbereitung des Basisprojekts](https://github.com/debuerreotype/debuerreotype/blob/master/scripts/debuerreotype-slimify).
Der [dpkg-Dateivergleich](https://manpages.debian.org/trixie/dpkg/dpkg.1.en.html)
ist eine Konsistenzprüfung anhand vorhandener Paketmetadaten und **kein
Authentizitätsnachweis**. Signierte Repository-Auswahl und der endgültige
SHA-256-Nachweis des vollständigen Abbilds bleiben eigene Anforderungen.

Vorher und nachher sind AOSP-Benutzer 0/0, Alpha 10/10 und Beta 11/11 identisch.
Nur Benutzer 0 ist gestartet/CE-entsperrt, alle persönlichen Kontexte bleiben
leer. Die AVB-Digest entspricht dem gepinnten Vollimage; SELinux Enforcing,
unveränderte Boot-ID und laufender Produkt-Broker sind bestätigt. Kein Konto wurde gelöscht oder angemeldet,
kein Profil migriert und kein sichtbares QEMU-Fenster geöffnet.

Belege im primären Workspace: `out/components-ef8242a1/targeted-tests/`.
Ergebnis SHA-256 `05440a0fcbada1bb65d37bd77b01bd1b124c00821a3052da537fad0775561093`;
Rohlog `7f2a73a6a28df46e0d711c4b494e69d6235c7e3ae60ce5252d6d739b74392b02`;
identischer Vorher-/Nachherzustand `427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.
Die früheren 28/40- und 43/43-Belege bleiben unter
`out/components-2fafc631/targeted-tests/` beziehungsweise
`out/components-e7728958/targeted-tests/` erhalten.

**Grenzen:** Entwicklungskomponenten in privaten Root-Testfixtures; der laufende
Produkt-Broker bleibt ab38cf24. Erfolgreiches APT und diese Prüfungen liefern
weiterhin nur `NeedsValidation` und dieselbe gehaltene Staging-Referenz im
registrierten Auftrag. Der Übergang zur endgültigen Hashbildung,
Veröffentlichung und Aktivierung ist noch nicht verbunden. Es gibt weiterhin
keinen öffentlichen Paketbefehl mit frischer AOSP-Adminbestätigung, keinen
vollständigen Repository-Planer und keinen neuen produktiven SELinux-/CE-
Nachweis während APT. Die übrigen nativen und Java-Suiten wurden für diese
begrenzte Änderung nicht erneut ausgeführt. Der bekannte sichtbare Launcher
bleibt bis zur integrierten Abnahme unverändert.

## Paketablage bleibt nach APT dem Auftrag zugeordnet: 8ef04ca6

Der Komponentenstand `8ef04ca63cb2cd660bf29d9702bc9baada8d4258` wurde auf
`aegis-build` erfolgreich kompiliert und am 30. September 2026 um 00:57:16 UTC
als [Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260930T005654Z-8ef04ca6-8ef04ca6-28pGaF)
verifiziert veröffentlicht. Lauf `identity-20260930T005535Z-8ef04ca6-6D0Apr`,
Invocation `42db13853beb45afbb727d8a768a6871`.

Am **2026-09-30T01:04:46Z bestehen 29/29 gezielte Gerätetests**, drei Suiten
in 10.440 Sekunden: acht `RuntimePackageExecutor`, dreizehn
`RuntimePackagePreparation` und acht `RuntimePackageBroker`. Kein Skip.
Ausführung ausschließlich im lokalen Hintergrund-QEMU, Vollimage ab38cf24,
Profil `0bbb6cf5-951e-43b4-9e08-ec952ab1b6e4`, Boot
`6e287d8c-a75c-47c1-ba45-b72b96b2122e`.

Nach erfolgreichem APT hält derselbe registrierte Auftrag die exakte
Staging-Verzeichnisreferenz im Zustand `AwaitingValidation`. Wiederholtes
Abfragen konsumiert den Auftrag nicht. Der Test schließt die ursprüngliche
Aufruferreferenz und zählt anschließend genau eine verbliebene Brokerreferenz
zu demselben Inode, auch nach sechzehn Statusabfragen. Freigeben des Besitzers,
zweiter Auftrag und erneuter Start bleiben blockiert. Fremde Identität,
Seriennummer oder Plan werden abgewiesen. Abbruch schließt die letzte Referenz
und liefert einen einmal abholbaren Fehler. Vorbereitung, echtes Offline-APT
und Warten auf Validierung behalten dieselbe Auftragskennung; `STOP_USER`
schließt die Referenz und bestätigt `ABSENT`. Der Schreibmount und sein
Loopgerät sind zu diesem Zeitpunkt bereits vollständig freigegeben.

Vorher-/Nachherabgleich bestätigt unveränderte AOSP-Identitäten 0/0, Alpha
10/10 und Beta 11/11; nur Benutzer 0 ist gestartet und CE-entsperrt. Persönliche
Kontexte bleiben leer, Enforcing und der produktive Broker laufen weiter.
Die getestete Besitzerbibliothek wird direkt im Entwickler-Testprozess
aufgerufen; der laufende Produkt-Broker bleibt ab38cf24. Es wurde weder ein
Benutzer angemeldet/gelöscht noch ein Profil migriert oder der Launcher ersetzt.

Der zunächst angehaltene Empfang benötigte mehr lokalen Speicher. Sechs
ungenutzte komprimierte Downloadkopien wurden erneut anhand gleicher SHA-256
in den weiterhin veröffentlichten GitHub-Assets und fehlender Dateizugriffe
verifiziert und entfernt (10.78 GiB). Alle 31 Profilmanifeste, Daten-/KeyMint-Paare,
extrahierten Images, Helper und Belege blieben erhalten. Danach waren tatsächlich
23.48 GiB frei. Der vorhandene Release wurde empfangen, ohne neu zu bauen oder
erneut zu veröffentlichen. Die erste lokale Testvorprüfung hielt vor Gaständerung
an, weil `dumpsys user` den Systembenutzernamen als `null` ausgibt, während
`pm list users` ihn als `Owner` darstellt. Nur diese Vorprüfung wurde an die
beobachtete Darstellung angepasst; IDs und Seriennummern blieben exakt geprüft.

Belege: `out/components-8ef04ca6/targeted-tests/` im primären Workspace.
Ergebnis SHA-256 `0db9d1c0d590005809cf4288497536fc6bc40e40bbe7d9daca972772ac334624`;
Rohlog `6ea91e861619dadf84042ed2b0478d53f087af6cd2362722c9897add9fa43178`;
identischer Vorher-/Nachherzustand
`427588a420e87667361ec5b55db00c561070d70a17fdf2064dda31e9bb30c95a`.
Die Cacheprüfung ist unter `out/download-cache-20260930-verified.json` erhalten.

**Grenzen:** Die vollen 187 nativen und 119 Java-Tests wurden für diese Änderung
nicht wiederholt. Registrierte semantische Validierung, Veröffentlichung und
Aktivierung sind weiterhin nicht miteinander verbunden. Es gibt damit noch
keinen öffentlichen Paketbefehl mit frischer AOSP-Adminfreigabe und keinen
Nachweis produktiven privaten APTs während echter AOSP-Abmeldung. Die
Komponentenprüfung ersetzt diese Integration und einen neuen Vollimage-Test nicht.

## Reale private CE-Paketablage mit Anmeldung, Logout und Reboot: ab38cf24

Der [Vollbuild ab38cf24](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T225939Z-ab38cf24-844b2f1e)
läuft im separaten lokalen Mac-QEMU-Profil `runtime-ab38cf24`. Die produktive
Runtime legt den privaten Paketbereich jetzt innerhalb derselben AOSP-Zulassung
wie HOME an. Dabei erhält er tatsächlich den SELinux-Typ
`u:object_r:aegis_package_private_file:s0`; gewöhnliche GNU-Prozesse erhalten
keinen Zugriff auf das Verwaltungsverzeichnis. Enforcing, FBE, authentifiziertes
ADB sowie tatsächliches dm-verity für system und system_ext bestehen vor und
nach dem Neustart.

Am **30. September 2026 bis 00:40:39 UTC bestehen 16 gezielte native Aufrufe**
aus vier ausdrücklich aktivierten Integrationstests. Die beiden persönlichen
Benutzer Alpha (10/10, Admin) und Beta (11/11, regulär) wurden über die echte
AEGIS-CLI neu angelegt und angemeldet. Ihre ersten Anmeldungen funktionieren
direkt; nach jeweils sechs Sekunden besteht die Sitzung weiter und führt GNU
aus. Die Passwörter bleiben im Arbeitsspeicher des Treibers und werden bei
dessen bestätigtem normalem Ende verworfen. Beide Testkonten bleiben erhalten
und sind abschließend gesperrt; es wurde keine Benutzerlöschung aktiviert.

Für beide Benutzer wurde im tatsächlich von AOSP entsperrten CE-Speicher:

- die vollständige, geprüfte 256-MiB-Debian-Basis im internen privaten Store
  veröffentlicht, wieder geöffnet und anhand Größe sowie Hash verifiziert;
- eine falsche Seriennummer beziehungsweise gemeinsame Eigentümerzuordnung
  abgewiesen; wiederholte Ablehnung hinterlässt keine offenen Datei-FDs;
- ein privater Kandidat durch den registrierten Testbesitzer vorbereitet;
  `STOP_USER` schließt dessen gehaltene Dateien und Mountreferenzen, bevor
  anschließend die tatsächliche AOSP-Abmeldung erfolgt;
- nach Logout das Öffnen und Anlegen mit `ENOKEY` verweigert, auch wiederholt;
- nach einem geordneten Neustart desselben Android-/KeyMint-Paars zunächst
  erneut Zugriff verweigert und nach echter Anmeldung dasselbe vollständige
  Paketabbild gelesen. Neue Kandidatenvorbereitung und erneuter Logout bestehen
  ebenfalls. Beide privaten Stores werden nach dem Reboot nicht neu veröffentlicht.

Die vier Tests ergeben je Benutzer vor Reboot vier Aufrufe und danach vier
weitere (gesperrt, Wiederöffnung, Vorbereitung, erneut gesperrt). Jeder Aufruf
bestätigt unveränderte AOSP-Benutzer-/CE-/Vordergrund- und Kontextzustände
zwischen Eintritt und Ende. Die native Store-/Besitzerprüfung läuft als
Entwickler-Root, **nicht über einen öffentlichen Paketbefehl oder mit frischer
AOSP-Adminfreigabe**. Die Vorbereitung wird vor Logout gestoppt; gleichzeitiges
APT im produktiven Broker während AOSP-Logout ist damit nicht belegt.

Unabhängig davon bleiben die durch echte GNU-Prozesse geschriebenen und gelesenen
Dateien nach Reboot bytegleich (je 1.024 Bytes):

- Alpha: `6bf7f9f104a98c56e31755fc4b27aad3fe5e2ce93780f477e39f2f7597fcdca3`.
- Beta: `fd154421de5a6c10d852f009ad08c0908b692644035fa9e1b647da13acd5e244`.

Aus Alphas GNU-Kontext scheitern tatsächliches Lesen von Betas Datei und SIGSTOP
gegen dessen unabhängig beobachteten Hostprozess; derselbe Prozess schreitet
danach weiter. Er übersteht Alphas Logout und wird bei Betas eigenem Logout
entfernt. Alphas zeitlich begrenzter Hintergrundjob konnte während einer Pause
wegen des Nutzungslimits natürlich auslaufen und wird nicht als Logout-Beweis
verwendet. Der umfassende frühere Zwei-Richtungs-Test bleibt separat dokumentiert.

Der erste neue Probeaufruf scheiterte vor Store-Zugriff: Der Testleser erwartete
Text-XML, AOSP speichert die Benutzerdateien als ABX. Die reine Testkorrektur
`76983c0844265bf954daa8c5a1205a203c640182` verwendet AOSPs eigenen Konverter
mit begrenztem Puffer, geprüftem Exit und abgeholtem Kindprozess. Sie verändert
keine AOSP-Metadaten. Sie wurde auf `aegis-build` kompiliert und über den
[korrigierten Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260930T003147Z-76983c08-76983c08-dhbh8R)
bezogen; das laufende Produktimage bleibt ab38cf24. Der fehlgeschlagene Ausgangslog
ist erhalten. Der ursprünglich vorgesehene Treiber-Checkpoint verlangt außerdem
einen rechtzeitig beobachteten Alpha-Hintergrundjob; für diesen langen Lauf
bestätigt ein unabhängiger Readback stattdessen beide tatsächlich gesperrten
CE-Bereiche, ausschließlich Benutzer 0 gestartet und vollständig leere Kontexte.
Es wird kein zusätzlicher Prozess-Beendigungsnachweis daraus abgeleitet.

Profil-ID `0bbb6cf5-951e-43b4-9e08-ec952ab1b6e4`; Boot vorher
`a8555e55-e590-4193-98d1-860b559dee6c`, danach
`6e287d8c-a75c-47c1-ba45-b72b96b2122e`. AVB-Digest unverändert
`b6e974f6612d5810c1fa395286db6d4146bfa28287cc1ad765880d0716ff4a86`.
Der vorherige Testgast 927cf51d wurde geordnet beendet; sämtliche früheren
Profile bleiben erhalten. Das sichtbare QEMU-Fenster blieb geschlossen und
der bekannte Launcher wurde nicht ersetzt.

Belege: `out/full-build-ab38cf24/ce-user-test/`, `corrected-ce-tests/` und
beide `boot-*/boot-health.json`. `SHA256SUMS` bindet Rohlogs, Eingaben,
Testtreiber, Vorher-/Nachherzustand und Ergebnis. Ergebnis-JSON SHA-256:
`bf3fb27466054e89061ae9849a95f26d71fd7f36f3b07d06200e988373591810`;
Ereignislog: `523da7a58326f32eab013b15c4af19e00c0acda42cf5f09facf38aef1cc17cb6`.
Die gesonderte `driver-exit.json` bestätigt anschließend Exit 0 und verworfene
Credential-Puffer. Korrigierte native Testdatei SHA-256:
`f478b78b43a664ec93cda81f48a655a564038fbe8296ac58135b5c4dde41e45d`.

Vor diesem Vollimage bestanden die **187/187 Standardtests** des Komponentenstands
ab38cf24 am 29. September um 22:57:30 UTC auf Gast 927cf51d; die vier opt-in Tests
waren dort deaktiviert. Log unter `out/components-ab38cf24/component-tests/`,
SHA-256 `dcadccd33ff11bb7d2314faf82a237aff24c28588c14c0086cf6ecd8335d1582`.
Diese 187 Tests und die unveränderten 119 Java-Tests wurden nach der reinen
ABX-Testkorrektur nicht nochmals ausgeführt.

**Weiterhin offen:** produktive Paketarbeiter-/Cgroup-/SELinux-Anbindung,
vertrauenswürdige Paketplanung, frische Adminfreigabe mit Java-/CLI-Aufruf,
semantische Validierung, Auswahl vollständiger Generationen und echte
Installation/Update/Entfernung gemeinsam und privat. Der neue Nachweis ersetzt
weder diese Integration noch die vollständige Phase-1-Abnahme.

## Private Paketablage an AOSP-CE gebunden: a08e3d7d

Der auf `aegis-build` kompilierte [Komponentenstand](https://github.com/simgero/AegisOS/releases/tag/components-20260929T224118Z-a08e3d7d-a08e3d7d-spwnQv)
aus `a08e3d7df753ae9f14586e2f58c047e2ca098c5e` besteht am **2026-09-29T22:43:37Z
alle 187/187 nativen Gerätetests**, 28 Suiten in 65.077 Sekunden, ohne Skip.
Der Serverbuild hat außerdem `selinux_policy` einschließlich Neverallow- und
Dateikontextprüfungen erfolgreich abgeschlossen. Diese neue Richtlinie ist
noch nicht im laufenden Gast installiert.

`BrokerPreparePersonalCandidate` und `BrokerPreparePersonalPublication`
registrieren ihre Aufträge vor dem ersten privaten Zugriff. Anschließend
öffnen sie ausschließlich das feste `/data` in den von init gepinnten
Host-Namespaces. ID und Seriennummer gehören zum Antragsteller; eine separate
Administrator-Zielkennung oder ein vom Client gelieferter Speicherpfad ist
nicht vorgesehen. Fehler nach Registrierung verbrauchen die Auftragskennung,
schließen alle übernommenen Referenzen und bleiben passend abholbar.

Die CE-Auflösung verwendet dieselben bestehenden Prüfungen wie HOME:
`system_ce` als alleinige Seriennummernautorität, identische fscrypt-v2-Policy
in `system_ce` und `misc_ce`, vorhandener Schlüssel, exakte Eigentümer und
keine ACL, Symlinks oder Mountwechsel. Der neue Bereich
`/data/misc_ce/<id>/aegis/packages` liegt neben HOME. Er und seine Unterordner
`store` und `staging` bleiben root:root 0700 mit unveränderlicher
ID-/Seriennummer-Zuordnung. Neue Bereiche werden zuerst vollständig angelegt
und synchronisiert, dann ohne Überschreiben umbenannt. Unterbrochene Bereiche
werden weder übernommen noch gelöscht. Auftragsablagen erhalten zusätzlich
einen zufälligen Namensanteil, damit auch nach Broker-Neustart kein alter
Auftrag übernommen wird. Es werden keine AOSP-Schlüssel oder -Seriennummern
angelegt oder verändert.

Vier zusätzliche Gerätetests prüfen konkret:

- Plausible Eigentümermetadaten auf unverschlüsselter tmpfs ermöglichen weder
  Store-Zugriff noch Ablagen; vorhandene Testdateien bleiben unverändert.
- Ungültige Benutzer, Seriennummern und Auftragskennungen erzeugen keine Ablage.
- Fehlendes echtes AOSP-CE erzeugt beim privaten Vorbereitungspfad einen
  gescheiterten registrierten Auftrag, ohne offene Referenzen oder Kindprozess.
  Eine fremde Seriennummer kann dessen Ergebnis nicht abholen.
- Private Veröffentlichung lehnt gemeinsamen Bereich vor Registrierung ab;
  fehlendes AOSP-CE schließt alle Referenzen und liefert ein gebundenes Fehlerergebnis.

Der bestehende Test gegen unverschlüsselte nachgeahmte AOSP-Verzeichnisse
prüft zusätzlich den neuen Einstieg. Alle bisherigen Kopier-, APT-,
Veröffentlichungs- und Abbruchtests bestehen weiterhin.

**Grenzen:** Dies ist noch kein erfolgreicher privater CE-/APT-Gesamtnachweis.
Es wurden keine persönlichen AOSP-Testbenutzer angelegt. Die neue Ablage muss
mit tatsächlicher Anmeldung, privater Veröffentlichung, Abmeldung und erneutem
Entsperren geprüft werden. Die produktiven Helferdomänen, AOSP-/Java-/CLI-Aufruf,
Repository-Planung, frische Adminfreigabe, semantische Validierung und Auswahl
kompletter Generationen sind noch zu verbinden. Der neue Dateityp trennt
Paketablagen von gewöhnlichem HOME; seine Laufzeitwirkung ist noch nicht
gebootet. Die 119 unveränderten Java-Tests wurden nicht wiederholt.

Gast `927cf51d`, Profil `d68845b3-62a9-4181-a7cd-c0f0a8e7d316`,
Boot-ID `984f23bd-607e-4a6d-8c08-7ae91bccd4f5`. Benutzer-/CE-/Schlüsselbestand bleibt bei 0,
Kontexte leer, Enforcing und bestehender Broker aktiv. Kein sichtbares
QEMU-Fenster wurde geöffnet, kein Startprofil ersetzt.

Build `identity-20260929T223818Z-a08e3d7d-fvJ8p4`, Invocation `15ee4dfe201c4aaf843b5f3b79606a28`.
Belege: `out/components-a08e3d7d/component-tests/`; `native.log` SHA-256
`91661d85c19b977555ffd30ef1223e0bca11d7d58eb90399d5e105fcbef9f646`; Vorher-/Nachher jeweils
`0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3`.

## Registrierte Paketvorbereitung bis APT: adce0475

Der auf `aegis-build` kompilierte [Komponentenstand](https://github.com/simgero/AegisOS/releases/tag/components-20260929T221859Z-adce0475-adce0475-VgK5Ma)
aus `adce04750acbe4cbac10a72a5c1a4f513890ec8c` besteht am **2026-09-29T22:22:09Z
alle 183/183 nativen Gerätetests**, 28 Suiten in 72.830 Sekunden, ohne Skip.
Elf neue Vorbereitungs-/Übergabetests und eine Prüfung der begrenzten
Eingabedaten ergänzen den zuvor nachgewiesenen Stand mit 171 Tests.
Transportprofil `aegis-qemu-arm64-components-v4` enthält acht native Programme;
neu ist der feste vertrauenswürdige Kopierhelfer `aegis-package-prepare`.
Die lokalen Transportprüfungen bestanden mit neun aktiven und sieben
plattformbedingt übersprungenen Tests.

`BrokerPrepareCandidate` registriert die Arbeit vor dem Start im bestehenden
Ausführungsslot. Benutzer, Seriennummer, monotone Auftragskennung und Planhash
bleiben durch Vorbereitung und APT unverändert. Die gemeinsame Kapazität und
die Grenze eines nicht abgeholten Auftrags je Antragsteller gelten weiter.
Auch ein fehlgeschlagener Teilstart bleibt als konkreter Auftrag besessen.
Es existiert kein neuer öffentlicher Socket- oder CLI-Endpunkt.

Kopieren, Hashen, Mounten und Archivvorbereitung laufen in einem eigenen,
cgroup-begrenzten Kind außerhalb der kurzen AOSP-Zulassung. Der feste Helfer
führt keinen Paketcode aus. Er verlangt ein neues leeres root:root-Verzeichnis
mit 0700 und ohne ACL, kopiert den gepinnten vollständigen Quelldatenträger
und prüft Größe, SHA-256 sowie stabile Quellmetadaten. Nur seine neue Kopie
wird über ein geprüftes autoclear-Loopgerät als getrenntes ext4-Dateisystem
bereitgestellt. Vorhandene Ablagen werden weder übernommen noch repariert.

Jedes Archiv erhält eine eigene Größen-/Hashbindung. Kopiert wird nur in den
Cache des Kandidaten. Ein vorhandenes gleichnamiges Archiv wird ausschließlich
nach erneuter Prüfung sowohl des Eingangs als auch der Cachedatei akzeptiert;
abweichende Inhalte, Links oder ungeeignete Eigentümer werden abgewiesen.
Die Namen müssen der von APT erwarteten Cacheform entsprechen, beispielsweise
`paket_1_all.deb`. Der spätere vertrauenswürdige Planer muss die Zuordnung aus
Paket-/Versions-/Architekturmetadaten liefern; ein Hash-Dateiname allein reicht
bei `--no-download` nicht aus. Hashbindung ersetzt keine Repository-Signatur.

Die erfolgreiche Antwort enthält genau einen getrennten Mount-FD. Solange er
in der privaten Antwortwarteschlange liegt, besitzt ihn der registrierte
Kanal. Nach Reaping und leerer/entfernter Cgroup übernimmt derselbe Slot den
Mount und behält ihn mitsamt der Ablage im Zustand `Prepared`. Erst der
bestehende Startpfad darf ihn unter frischer AOSP-Freigabe und Sitzungsprüfung
an APT weitergeben. Hier wurde dieser interne Pfad direkt geprüft; die reale
AOSP-Freigabe ist weiterhin nicht angeschlossen. Kopie oder Hashprüfung
verbrauchen keine vorab gespeicherte Adminfreigabe.

Tatsächlich im lokalen QEMU nachgewiesen:

- Vollständige Basiskopie, exakte Archive, echte APT-Installation der synthetischen
  Anwendung samt Abhängigkeit, installierte Dateien, Paketskript und technischer
  Eigentümer 42:42 in der Benutzerabbildung. Der separate Broker-Test behält
  dieselbe Auftragskennung bis zum Ergebnis `NeedsValidation`.
- Erneute Verwendung einer bereits vorbereiteten Kopie mit identischen Cache-
  Archiven und anschließender APT-Installation. Ein verändertes Cachearchiv
  sowie falsche Image-/Archivhashes erzeugen keinen übernehmbaren Mount.
- Eine bereits belegte Ablage bleibt unverändert. Der Helfer setzt seine eigene
  umask auf 0022, damit die reale Broker-Voreinstellung 0077 keine Cacheverzeichnisse
  mit falschen Rechten erzeugt. Geprüft sind 0755/0644 im Kandidaten, 0600/0400
  für Image/Auftragsdaten und die unveränderte umask des Elternprozesses.
- `STOP_USER` beendet eine nachweislich unvollständige Kopie. Dafür friert der
  Test ausschließlich seine eigene Cgroup nach einer tatsächlichen Änderung
  von `candidate.ext4` ein und bestätigt eine Größe zwischen 0 und 512 MiB.
  Erst danach erfolgt der Abbruch über den echten Besitzerpfad.
- `STOP_USER` schließt sowohl einen noch wartenden Antwort-Mount als auch einen
  schon im Slot vorbereiteten Mount. Die Tests identifizieren das zugehörige
  Loopgerät anhand Gerät/Inode der eigenen Kandidatendatei und bestätigen,
  dass danach kein passendes Loopgerät mehr konfiguriert ist. Laufende Besitzer
  lassen sich vorher nicht freigeben. FD-Zählung bestätigt die Übernahme genau
  eines Mounts und das Schließen der übrigen eigenen Referenzen.

**Grenzen:** Testablagen liegen ausschließlich unter `/data/local/tmp`; es
wurden keine neuen AOSP-Benutzer, persönlichen CE-Stores oder Adminpasswörter
verwendet. Aufrufer-eigene FDs bleiben deren Verantwortung. Die drei neuen
Arbeiterpfade sind nicht in den laufenden Daemon/dessen SELinux-Domänen
installiert. Privater CE-Store und AOSP-Zulassung, Repository-/Abhängigkeitsplaner,
semantische Gesamtprüfung, Veröffentlichung/Startauswahl, private/gemeinsame
Updates und Konfliktbehandlung sowie tatsächliche Abmeldung und Reboot bleiben
zu verbinden und im Vollimage nachzuweisen. `Prepared` ist keine Freigabe;
`NeedsValidation` ist keine aktivierte Paketgeneration. Die unveränderten
119 Java-Tests wurden nicht erneut ausgeführt.

Gast `927cf51d`, Profil `d68845b3-62a9-4181-a7cd-c0f0a8e7d316`, Boot-ID
`984f23bd-607e-4a6d-8c08-7ae91bccd4f5`. Benutzer/CE/Schlüsselverzeichnisse bleiben
bei 0, Runtime-Kontexte leer, Enforcing und der bestehende Broker aktiv.
Sichtbarer Launcher und sämtliche Daten-/KeyMint-Paare bleiben erhalten.
Erfolgreiche eigene Fixtures werden nach geschlossenen Referenzen entfernt;
fehlgeschlagene bleiben erhalten.

Build `identity-20260929T221753Z-adce0475-VOux1C`, Invocation `6aa14806bc1a441c84e27365a9579de5`.
Belege: `out/components-adce0475/component-tests/`; `native.log` SHA-256
`a011a21dca1cf90198ccb822138426e5f35b75257732e978af6cea9e4f9b5a17`; Vorher-/Nachher jeweils
`0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3`.

Vorherige Belege bleiben erhalten: `c8f707d5` bestand den Build, aber nur 1/2
Smoke-Tests. APT meldete einen nicht erkannten kanonischen Cachepfad; Ursache
war der anders benannte Testarchivname. Die geschützte eigene Testkopie
`/data/local/tmp/aegis-preparation-FgD4iN` wurde nur lesend ausgewertet; ein
normaler `adb pull` war an ihren Rechten gescheitert, ohne sie zu verändern.
Der begrenzte Root-Lesezugriff und die Diagnose sind unter
`out/components-c8f707d5/preparation-smoke/diagnostic.json` dokumentiert.
`d5132eb9` bestand danach 2/2 Smoke- und 182/182 Gesamttests um
2026-09-29T22:15:38Z. Beim Vergleich mit der realen Broker-umask wurde zusätzlich
der neue Rechtefall ergänzt; der aktuelle Lauf umfasst ihn und alle früheren Tests.

## Begrenzter Paketabschluss bei fehlender Antwort: 1719eebd

Der [Komponentenstand](https://github.com/simgero/AegisOS/releases/tag/components-20260929T214339Z-1719eebd-1719eebd-Hh238A)
aus `1719eebd57f3f07c97d3deb2ebe89de7f3eeefc7` besteht am **2026-09-29T21:45:18Z
alle 171/171 nativen Gerätetests**, 26 Suiten in 44,809 Sekunden, ohne Skip.
Vier zusätzliche Tests prüfen den tatsächlich vom Publisher verwendeten
privaten Abschlussdecoder. Bei fehlgeschlagenem Empfang oder EOF konnte dessen
Ancillary-Schleife zuvor eine Headerlänge von null wiederholt auswerten.
Der Empfangsfehler wird jetzt vorher verworfen; jede Headerlänge wird vor dem
Weiterschalten begrenzt. Fehlende Antworten bleiben ausdrücklich `Unconfirmed`.

Die Regression prüft offene leere und geschlossene Kanäle in jeweils einem
eigenen Kindprozess mit pidfd und Zweisekundenfrist. Ein Hänger würde nur
diesen eigenen Prüfprozess beenden und den Test fehlschlagen lassen.
Zusätzlich geprüft: exakte Auftrags-/Planbindung, unzulässige Resultate,
leere/kurze/übergroße Pakete und mitgesendete FDs einschließlich Kontrollpuffer-
Trunkierung und leerer Nutzlast. Alle übernommenen FDs sind danach geschlossen;
die Originale des Senders bleiben offen. Die vier Tests dauerten zusammen 4 ms.

Der Decoder alleine bescheinigt kein Prozessende. Der Publisher ruft ihn erst
nach Reaping des Kindes und Entfernung der leeren Cgroup auf. Die übrigen
167 Tests einschließlich echter APT-Ausführung und Abbruch bestehen weiterhin.
Die Produktionsanbindung und Grenzen des folgenden Nachweises bleiben offen;
keine Paket-CLI, AOSP-CE-Abmeldung oder neue Vollimage-Integration wird damit
behauptet. Die Java-Quellen sind seit `09b10fd7` unverändert; deren 119 Tests
wurden nicht erneut ausgeführt. Transportprofil v3 bleibt unverändert.

Buildlauf `identity-20260929T214231Z-1719eebd-abAgmi`, Invocation `ffbf07f8abe54dc68b3f48264d3ddd80`.
Ausführung ausschließlich im lokalen Mac-QEMU mit bestehendem Gast `927cf51d`,
Profil `d68845b3-62a9-4181-a7cd-c0f0a8e7d316`, Boot-ID `984f23bd-607e-4a6d-8c08-7ae91bccd4f5`.
Benutzer-/CE-/Schlüsselbestand bleiben identisch bei Benutzer 0; Runtime-Kontexte
leer, Enforcing und der bestehende Broker aktiv. Sichtbarer Launcher und
sämtliche Daten-/KeyMint-Paare bleiben erhalten. Nur die eigenen neuen
Komponententestdateien wurden in den laufenden Hintergrundgast übertragen.

Belege: `out/components-1719eebd/component-tests/`; `native.log` SHA-256
`35351e308118da1355c984c9771c1b87e798ab5a6f1042519de05af2ca4cd533`; Vorher-/Nachher jeweils
`0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3`.

## Brokerverwalteter APT-Arbeiter: 90b9732d

Der auf `aegis-build` kompilierte und über GitHub verifizierte
[Komponentenstand](https://github.com/simgero/AegisOS/releases/tag/components-20260929T213008Z-90b9732d-90b9732d-ncIcPj) aus Commit `90b9732dc6e2528e23837870267c8256a327a7da`
besteht am **2026-09-29T21:31:27Z alle 167/167 nativen Gerätetests**
aus 25 Suiten in 18.864 Sekunden, ohne übersprungene Tests.
Dazu gehören acht neue Executor-/Broker-Tests und eine Prüfung begrenzter
Auftragsfelder. Der aktuelle Testtransport heißt
`aegis-qemu-arm64-components-v3` und umfasst sieben native Programme,
einschließlich getrennter Produktions- und Test-Einstiege des APT-Arbeiters.

Der neue Arbeiter erhält ausschließlich eine intern vorbereitete eigene
ext4-Kopie, einen begrenzten Auftrag und eine private Geräteansicht. Er prüft
Namespaces, UID/GID-Abbildungen und Enforcing, hängt Androids Wurzel ab,
prüft die vollständige Mountliste und begrenzt seine Rechte vor Debian-Code.
Er startet ausschließlich festes `apt-get` mit vorbereiteten Archivnamen bzw.
Paketnamen, leeren Quellen und `--no-download`. Keine Shellbefehle, Hostpfade
oder frei gewählten APT-Optionen kommen aus dem Auftrag. Die Auftragsdaten
liegen in einem vollständig versiegelten memfd; Antworten binden Benutzer,
Seriennummer, Auftragsnummer und Planhash. Die Echtheit und vollständige
Auflösung der Archive bleiben Aufgaben des noch fehlenden Planers.

Tatsächlich ausgeführt und bestätigt:

- Installation zweier synthetischer Pakete mit exakter Versionsabhängigkeit,
  Update beider, technische Eigentümer 42:42 und Paketskripte. Die geänderte
  Konfiguration bleibt erhalten. `remove` entfernt die Paketdateien und lässt
  die Konfiguration bewusst bestehen; der ältere separate Purge-Test besteht
  ebenfalls weiterhin.
- Alle vom Executor übernommenen Deskriptoren sind nach bestätigtem Ende
  geschlossen. Bestätigung setzt tatsächliches PID1-Reaping, eine leere und
  entfernte Cgroup und das Schließen eigener Mount-/Stagingreferenzen voraus.
  Die externen Prüf-FDs und der Test-Loop werden separat vom Fixture geschlossen.
- Abbruch bei einem nachweislich wartenden echten `postinst`: Prozesse enden,
  Cgroup wird leer, ein vorheriger nicht blockierender Wait behält den Besitzer.
  Ein getöteter Auftrag liefert `Unconfirmed`, keine behauptete Rückabwicklung.
- Abbruch nach natürlichem APT-Ende und nach internem Reaping verweigert ebenfalls
  die Weitergabe des Kandidaten. Eine frühere erfolgreiche Ausführung wird
  dadurch nicht zur Aktivierungsfreigabe.
- Der reale Broker-`STOP_USER`-Pfad beendet den laufenden APT-Auftrag des
  Antragstellers und erhält die Vorbereitung eines anderen Benutzers.
  Falsche Seriennummer/Planhash, doppelte Starts und fremder Abbruch scheitern.
  Veröffentlichung und APT teilen Kapazität, IDs und eine ausstehende Arbeit
  je Antragsteller; ein konkurrierender Veröffentlichungseintrag wird abgewiesen.
- Ein Teilstart behält seine Cgroup/FDs bis zum Aufräumen. Der Produktionseinstieg
  verweigert die Entwickler-Testdomäne; sein früher Tod wird ohne Warten auf
  das gesamte neunsekündige Test-Startbudget erkannt.

`NeedsValidation` bedeutet ausschließlich: APT meldete Exit 0, es kam eine
passende Abschlussantwort und die Ressourcen sind freigegeben. Der Kandidat
ist damit weder semantisch validiert noch veröffentlicht oder aktiviert.
Der Produktionshelper verlangt zusätzlich exakt `aegis_package_worker` als
SELinux-Domäne. Der separat benannte Testhelper verwendet dieselbe Ausführung
und dieselben Namespace-/Enforcing-Prüfungen, ohne diesen Domänennachweis zu
behaupten. Es wurde keine permissive Richtlinie oder produktive Ausnahme aktiviert.

**Nachweisgrenzen:** Der neue Besitzer wird direkt im nativen Test aufgerufen,
nicht in den laufenden Daemon installiert. Es fehlen die produktive
Cgroup-/SELinux-Einrichtung, lifecycle-eigene Vorbereitung/Kopie/Downloads,
vertrauenswürdige Repository-/Versionsauflösung, privater CE-Store, reale frische
AOSP-Freigabe samt Java-/CLI-Verbindung, semantische Generationsprüfung und
Auswahl beim Runtime-Start. Reale AOSP-Abmeldung/CE-Sperrung während APT,
konkurrierende persönliche Paketbereiche, gemeinsames Update/Rebase und
Neustart mit integrierter Paketverwaltung sind damit noch nicht nachgewiesen.
Die zuvor getesteten 119 Java-Tests sind für unveränderte Quellen übernommen,
nicht erneut ausgeführt. Die lokalen Transporttests bestanden mit neun
aktiven Prüfungen und sieben plattformbedingt übersprungenen Prüfungen.

Gast `927cf51d`, Profil `d68845b3-62a9-4181-a7cd-c0f0a8e7d316`,
Boot-ID `984f23bd-607e-4a6d-8c08-7ae91bccd4f5`. Benutzer/CE/Schlüsselverzeichnisse bleiben
bei 0, persönliche Runtime-Kontexte bleiben leer. Enforcing und der bestehende
Broker sind unverändert aktiv. Sichtbarer Launcher und sämtliche Daten-/KeyMint-
Profilpaare wurden nicht ersetzt. Erfolgreiche eigene Testkopien werden erst
nach dem Schließen ihrer Referenzen entfernt; fehlgeschlagene bleiben erhalten.

Buildlauf `identity-20260929T212909Z-90b9732d-H0ZbNL`, InvocationID `f36e8ec964144fa3b32c385bb263eb94`.
Belege: `out/components-90b9732d/component-tests/`. `native.log` SHA-256
`926fd41df7bd9a00c078199f5335b2dc95004c14f2ff5a7b8991c22062dcf6e7`; identische Vorher-/Nachherdateien
`0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3`.

Vorherige Versuche bleiben dokumentiert: `8c3cac8e` bestand den Build, scheiterte
vor Arbeiterstart am noch fehlenden Archivcache im Fixture; `82c9240e` wurde
beim absoluten Mountziel korrekt durch `RESOLVE_BENEATH` abgewiesen.
`bcb6ac2b` bestand den echten Install-/Update-/Remove-Smoke-Test und 166/167
Gesamttests. Der eine Fehler war ein Test-`STOP_USER` mit Seriennummer 42 statt
des vorgeschriebenen Werts 0. Das Protokoll wurde beibehalten, der Test korrigiert.
Zusätzlich wurde die elterliche Kopie des Kind-Sockets früh geschlossen, damit
ein abgewiesener Helper sofort als beendet beobachtet wird. Ein erster lokaler
Smoke-Aufruf verwendete versehentlich den falschen ADB-Port und startete keinen Test.
`014d217d` legte nach Freigabe des Kind-Sockets eine Endlosschleife im neuen
Ancillary-Parser bei EOF offen. Ausschließlich der anhand seines exakten
Programmpfads geprüfte Test-PID 8524 wurde beendet; sein Kind war bereits
beendet. Beide Prozesse waren danach verschwunden, und die nachweislich leeren
eigenen Test-Cgroups wurden entfernt. Der separate Aufräumbeleg liegt unter
`out/components-014d217d/component-tests/cleanup.json`; die Imagekopie bleibt.
Die Korrektur prüft jede Headerlänge vor dem Weiterschalten und verwirft
fehlgeschlagene Empfangsaufrufe vor der Auswertung. Der aktuelle vollständige
Durchlauf enthält den zuvor hängenden Fehlerfall samt Zeitbegrenzungsprüfung.

## Echter Offline-APT-Durchlauf: fddf9563

Der auf `aegis-build` kompilierte und über GitHub verifizierte
[Komponentenstand](https://github.com/simgero/AegisOS/releases/tag/components-20260929T202813Z-fddf9563-fddf9563-PEiJmo)
aus Commit `fddf956306c05bc556c5ec54c56909d041d8500b` besteht am
29. September 2026 um **20:29:33 UTC alle 158/158 nativen Gerätetests** aus
23 Suiten in 21,213 Sekunden, ohne übersprungene Tests. Der neue tatsächliche
APT-Durchlauf benötigt 6,462 Sekunden.

Die Probe kopiert die im gestarteten System verifizierte Debian-Basis in ein
neues eigenes ext4-Abbild unter `/data/local/tmp`. Hash und Größe müssen zum
unveränderlichen Systembeleg passen. Kopie und beschreibbarer Mount entstehen
vor dem begrenzten Namespace-Start. Nur der explizite Paketkontext kann diesen
noch unverbundenen Kandidaten auf seine eigenen UID/GID-Bereiche abbilden.
Ein normaler Runtime-Kontext darf das nicht; der abgewiesene Versuch lässt den
Kandidaten unverändert. Paketkontexte dürfen keinen persönlichen HOME-Mount
anfordern. Die unveränderliche Basis wird nicht beschreibbar gemacht.

Nach Abhängen der Android-Wurzel läuft das echte **APT 3.0.3 / dpkg 1.22.22**
mit den begrenzten sechs Capabilities. Der Test erzeugt zwei synthetische lokale
Pakete mit einer genauen Versionsabhängigkeit, stellt ihre Archive im eigenen
Cache bereit und verwendet ausschließlich leere Paketquellen sowie
`--no-download`. Er bestätigt:

- Installation und tatsächliche Ausführung von Version 1 samt Bibliotheksdatei.
- Gemeinsames Update beider Pakete auf Version 2 mit passenden dpkg-Versionen.
- Erhalt einer absichtlich geänderten Konfigurationsdatei mit `--force-confold`.
- Ausführung von preinst, postinst, prerm und postrm in den erwarteten Phasen.
- Technischen Dateibesitz 42:42 innerhalb des Kandidaten, außerhalb korrekt als
  1005042:1005042 sichtbar. Der vom Skript erzeugte Datenordner bleibt eigens für
  diese Eigentümerprüfung erhalten; Purge ist keine Löschung sämtlicher Appdaten.
- Purge beider Pakete, verschwundene Programm-/Bibliotheks-/Konfigurationsdateien
  und einen leeren `dpkg --audit`-Befund.

APT benötigt technische Zusatzgruppen auch beim Prüfen lokaler Archive.
Dafür erhält ausschließlich die explizite Paketvorbereitung `setgroups=allow`
mit denselben drei begrenzten GID-Abbildungen. Der reale Rechte-/Exec-Test
bestätigt Gruppen 42 und 65534; GID 1001 ist nicht abgebildet und wird abgewiesen,
ohne die vorherige Gruppenliste zu ändern. Nach UID-Wechsel fehlen auch die
Rechte für weitere Gruppenänderungen. Normale Runtime-Vorbereitung und -Sitzung
behalten `setgroups=deny`; ihre bisherigen Tests bestehen weiterhin. Die sechs
Capability-Mengen, No-new-privileges, NOROOT-/Ambient-Sperren und Mount-/Namespace-
Filter werden nicht erweitert. Der Paketarbeiter erhält keine zusätzliche
Host-Benutzergruppe. [APT-Quellstelle](https://raw.githubusercontent.com/Debian/apt/3.0.3/apt-pkg/contrib/fileutl.cc).

**Nachweisgrenze:** Dies ist eine native Entwicklerprobe mit einer eigenen
Imagekopie und synthetischen Paketen, noch kein produktiver Paketendpunkt.
Es gibt keine Repository-/Signaturprüfung, echte AOSP-Adminbestätigung, private
CE-Paketablage, produktive Paketarbeiter-SELinux-Domäne, brokerregistrierte
APT-Ressourcen oder Veröffentlichung/Aktivierung dieses Kandidaten. Der aktuelle
native Besitzer verwaltet separat getestete Veröffentlichungsprozesse; der
APT-Arbeiter muss noch denselben Abmelde-/Abbruchlebenszyklus erhalten.
Zwei persönliche Paketbereiche, gemeinsame Updates/Rebase, reale Abmeldung
während APT und ein neu gestartetes integriertes Vollimage bleiben offen.

Der lokale Gast bleibt `927cf51d`, Profil `d68845b3-62a9-4181-a7cd-c0f0a8e7d316`, Boot-ID
`984f23bd-607e-4a6d-8c08-7ae91bccd4f5`. Benutzer-/CE-/Schlüsselverzeichnis-/Kontextbestand sind identisch;
Enforcing, Broker und dieselbe Boot-ID sind bestätigt. Der erfolgreiche eigene
Kandidat wird erst nach Kindende und Schließen aller eigenen Mountreferenzen
entfernt; drei frühere fehlgeschlagene Fixture-Abbilder bleiben als Belege stehen.
Sichtbarer Launcher und sämtliche Daten-/KeyMint-Profilpaare werden nicht ersetzt.
Die 119 Java-Tests von `09b10fd7` gelten für unveränderte Quellen und wurden nicht
wiederholt. Buildlauf `identity-20260929T202723Z-fddf9563-z85XvY`, InvocationID `d8e6588aabdd45cb84833feceb123c85`.

Frühere Durchläufe: `be0a9ace` scheiterte an CLOEXEC auf bereits passend
nummerierten Standarddeskriptoren; `140ff98e` erreichte die echte APT-Auflösung
und zeigte die benötigten Gruppenwechsel; `85a2cb2d` bestand die neue Rechteprobe,
benötigte für `--no-download` aber bereits gefüllte Archivcache-Dateien.
`b3479b93` wurde kompiliert, nach Fund einer falschen erwarteten Eigentümerzahl
jedoch nicht ausgeführt. Diese Zahl ist nun aus der festen Abbildung berechnet.
Es wurde keine Sandbox deaktiviert und kein fehlgeschlagener Test ausgelassen.

Lokale Belege unter `out/components-fddf9563/component-tests/`:

| Beleg | SHA-256 |
| --- | --- |
| `native.log` | `68e3d978b4aa5bad347f0c9c94826ebc40935960ecafcc4b1da02732e426d328` |
| identische `before.json` / `after.json` | `0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3` |

## Rechte des Paketarbeiters: ac01f261

Der auf `aegis-build` kompilierte und über GitHub verifizierte
[Komponentenstand](https://github.com/simgero/AegisOS/releases/tag/components-20260929T195146Z-ac01f261-ac01f261-1JWDvU)
aus Commit `ac01f26105e3348199f08faa5e7275e26cc077d2` besteht am
29. September 2026 um **19:53:31 UTC alle 157/157 nativen Tests** aus
23 Suiten in 15,248 Sekunden, ohne übersprungene Tests.

Ein tatsächlicher Namespace-Kindprozess wechselt in eine eigene leere tmpfs,
hängt die Android-Wurzel ab, begrenzt Rechte und startet den statischen
Testhelfer erneut. Danach besitzen sämtliche fünf Capability-Mengen exakt
`0xdb`: CHOWN, DAC_OVERRIDE, FOWNER, FSETID, SETGID und SETUID. No-new-privileges,
Seccomp und gesperrte NOROOT-/Ambient-Erweiterungsbits sind aktiv. Der Prozess
kann eine eigene Datei einem technischen Benutzer zuordnen und trotz Modus 000
bearbeiten. Neue Mounts, chroot, Namespace-Erzeugung, ptrace und zusätzliche
Capabilities werden verweigert. Nach Wechsel auf UID/GID 42 verschwinden
Permitted/Effective/Ambient-Rechte; eine Rückkehr auf UID 0 scheitert.
Eine falsche Benutzerkennung sowie direkte Aufrufe aus dem Host-Testprozess
scheitern vor einer Privilegänderung.

Dies ist **noch kein APT- oder produktiver Paketarbeiternachweis**. Die Probe
nutzt nur ihre eigene tmpfs und führt keine Paketskripte aus. Ein vollständiger
beschreibbarer Kandidat, produktive SELinux-/CE-Anbindung und Paket-CLI fehlen.
Die neue interne Funktion wird vom installierten System noch nicht aufgerufen.
Der unveränderte Gast `927cf51d`, dieselbe Boot-ID und derselbe Benutzer-/CE-/
Schlüssel-/Kontextbestand sind bestätigt; Enforcing und Broker bleiben aktiv.
Die 119 Java-Tests von `09b10fd7` werden für unveränderte Quellen nicht wiederholt.
Das sichtbare Fenster bleibt geschlossen; keine Profilpaare werden ersetzt.

Buildlauf `identity-20260929T194439Z-ac01f261-jw7b9y`, InvocationID
`17693a3c3ee24b06ad05f215582a410d`. Lokale Belege unter
`out/components-ac01f261/component-tests/`:

| Beleg | SHA-256 |
| --- | --- |
| `native.log` | `2d4c650ea56ee874b50eebe008bdcf0bd590de7a4f48d1466bc160e4295dc319` |
| identische `before.json` / `after.json` | `0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3` |

## Brokergebundene Paketaufträge: 74bb0db9

Der auf `aegis-build` kompilierte und über GitHub verifizierte
[Komponentenstand](https://github.com/simgero/AegisOS/releases/tag/components-20260929T192740Z-74bb0db9-74bb0db9-J9n2Lj)
aus Commit `74bb0db9e8b2793b8d9be1b4d12d5ec9dc292f96` besteht am
29. September 2026 um **19:29:19 UTC alle 155/155 nativen Gerätetests**
aus 22 Suiten in 15,194 Sekunden, ohne Abwahl oder übersprungene Tests.
Die acht neuen Broker-Besitzertests benötigen zusammen 160 ms; die neun
Publisher-Tests einschließlich der zusätzlichen Dateirechteprüfung 1,101 Sekunden.

Der native Besitzer registriert vorbereitete und gestartete Paketaufträge in
seinen bestehenden Benutzerstopp-, HELLO- und globalen Aufräumpfaden. Geprüft
sind genaue Bindung an Antragsteller, Seriennummer und Plan; nur ein Start;
nicht wiederverwendete Auftragskennungen; eigene FD-Kopien; Aufräumen ohne
Client-Poll; erhaltene Verantwortung bei Teilstart oder Aufräumfehler;
Fortsetzung des Aufräumens anderer Aufträge trotz eines solchen Fehlers;
Kapazitätsgrenzen und Ablehnung im fremden Prozess. Fertige, noch nicht abgeholte
Antworten belegen höchstens einen Platz je Antragsteller. Ein einzelner Benutzer
kann damit nicht alle globalen Plätze durch fertige Antworten belegen.

Der zusätzliche Publisher-Test kopiert ausschließlich seinen eigenen Helfer
in ein neues Testverzeichnis. Ein nicht gruppenschreibbarer `root:shell`-Helfer
veröffentlicht erfolgreich; eine andere Gruppe sowie `shell`-Gruppenbesitz an
Quelle oder Store werden abgelehnt. Das installierte Systemabbild wird dabei
nicht verändert. Seine Herkunfts-/EROFS-/Labelprüfung bleibt Aufgabe des
vertrauenswürdigen Aufrufers.

**Direkte native Besitzertests, keine produktive Paketinstallation:** Die Tests
verwenden inerte Dateien unter `/data/local/tmp` und eigene Cgroups. Sie rufen
die tatsächlichen nativen Besitzerpfade auf, installieren aber keinen neuen
Daemon im laufenden Vollimage. Private CE-Stores, reale AOSP-Abmeldung während
APT, frische echte Adminpasswörter, Paketauflösung, Paketskripte und die öffentliche
CLI sind weiterhin nicht durch diesen Nachweis abgedeckt. Die produktive
Cgroup-/SELinux-/CE-Anbindung und Verbindungswiderruf der Java-Vorbereitungen
fehlen noch. [Implementierungsstand](../runtime/package-transactions.md).

Vorgänger `113aa930` scheiterte beim Linken des Brokers an der fehlenden direkten
Publisher-Bibliothek und beim Kompilieren des Tests an einer einschränkenden
Ganzzahlinitialisierung. `02cf1ab4` wurde deshalb nie gestartet. `44419a93`
korrigierte beide Fehler und bestand 154/155 Tests. Sein Störfalltest versuchte,
eine eigene Cgroup v2 umzubenennen; der verwendete Kernel verbietet das. Der
jetzige Stand verändert nur diesen Test: Er setzt vorübergehend eine fremde
Gruppenkennung an einer eigenen Test-Cgroup, prüft erhaltenen Besitz und die
Bereinigung des zweiten Auftrags, stellt die ursprüngliche Kennung wieder her
und verlangt danach vollständigen Abbau. Der Produktcode wurde für diese
Testkorrektur nicht verändert. Beide Fehlbelege bleiben erhalten.

Der lokale Gast verwendet weiterhin Vollimage `927cf51d`, Profil
`d68845b3-62a9-4181-a7cd-c0f0a8e7d316` und Boot-ID
`984f23bd-607e-4a6d-8c08-7ae91bccd4f5`. Benutzer-, CE-, Schlüsselverzeichnis-
und Runtime-Kontextbestand sind vorher/nachher identisch; Enforcing, der
laufende Broker und dieselbe Boot-ID sind bestätigt. Der 119er-Java-Nachweis
von `09b10fd7` bleibt für unveränderte Java-Quellen erhalten und wurde nicht
wiederholt. Der sichtbare Launcher bleibt geschlossen und unverändert;
sämtliche bisherigen Daten-/KeyMint-Profilpaare bleiben erhalten.

Bestätigter Buildlauf `identity-20260929T192639Z-74bb0db9-ITOjva`, InvocationID
`ddff76e85868485cb904ab611c840a36`. Belege unter
`out/components-74bb0db9/component-tests/`:

| Beleg | SHA-256 |
| --- | --- |
| `native.log` | `fcf22045b657cf341e9e8e6c3550cbc12414c0977103863c8d60c7159c862e63` |
| identische `before.json` / `after.json` | `0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3` |

Zusätzlich bestehen 10 lokale Quellregistrierungs- und 9 Archivtests; sieben
Linux-Export-Fixtures sind auf dem Mac ausgelassen. Diese Hosttests kompilieren
oder starten keinen Android-Code.

## Eigener Veröffentlichungsprozess: d4fdb778

Der auf `aegis-build` kompilierte und über GitHub verifizierte
[Komponentenstand](https://github.com/simgero/AegisOS/releases/tag/components-20260929T184758Z-d4fdb778-d4fdb778-gyOJTW)
aus Commit `d4fdb7786769e6443eb90ad73b7dda5708313d20` besteht am
29. September 2026 um **18:49:41 UTC alle 146/146 nativen Gerätetests**
aus 21 Suiten in 11.375 ms, ohne Abwahl oder übersprungene Tests. Die acht
neuen Tests für den eigenen Veröffentlichungsprozess benötigen 332 ms.

Diese Tests starten tatsächlich den neuen ARM64-Helfer über pidfd und eigene
Cgroups. Sie prüfen Veröffentlichung und Rücklesen, geschlossene eigene FDs,
weiter gültige eigene Referenzen nach Schließen der Caller-FDs, private
Zuordnung/Seriennummer, konkurrierenden Start ohne Beeinflussung des ersten
Auftrags, Hashablehnung, unzulässige Eingaben und prozessgebundenen Besitz.

Der Abbruchtest verwendet eine sparse 512-MiB-Quelldatei mit bekanntem Hash.
Nach einer tatsächlich beobachteten Änderung im Store wird ausschließlich
die eigene Test-Cgroup eingefroren. Eine vorhandene, noch unvollständige
Kopierdatei wird unabhängig geprüft. Ein unmittelbarer Wait liefert Timeout,
behält den Besitzer und verändert das Ergebnis nicht. Die anschließende
Zwangsbeendigung bestätigt Reaping, leere/entfernte Cgroup und geschlossene
eigene Referenzen; die Veröffentlichung bleibt ausdrücklich unbestätigt.
Das separate Rücklesen bestätigt die vorherige vollständige Auswahl.
Eigene erfolgreiche Fixtures werden anschließend entfernt. Dieser Test ist
kein physischer Stromausfall und keine Aussage über Rollback nach bereits
erfolgter Auswahl.

**Inerte Dateien und eigene Test-Cgroups, keine APT-/CE-Lifecycle-Prüfung:**
Der Helfer führt ausschließlich vertrauenswürdigen Kopier-/Speichercode aus.
Keine Paketskripte, echten Adminpasswörter oder privaten CE-Stores werden
verwendet. Der installierte Broker enthält den neuen Prozessbesitzer noch
nicht; AOSP-Quieszenz, produktive SELinux-Anbindung, APT und CLI sind offen.
[Implementierung und nächste Integration](../runtime/package-transactions.md).

Der lokale Gast verwendet weiterhin Vollimage `927cf51d`, Profil
`d68845b3-62a9-4181-a7cd-c0f0a8e7d316`, Boot-ID
`984f23bd-607e-4a6d-8c08-7ae91bccd4f5`. Vorher-/Nachherbestand von Benutzern,
Schlüsselverzeichnissen, CE und Runtime-Kontexten ist identisch; Enforcing,
der laufende Broker und dieselbe Boot-ID sind nachher bestätigt. Der
119er-Java-Nachweis von `09b10fd7` bleibt für unveränderte Java-Quellen erhalten;
die Java-Suite wurde hier nicht wiederholt. Der sichtbare Launcher bleibt
unverändert; alle bisherigen Profilpaare bleiben erhalten.

Buildlauf `identity-20260929T184045Z-d4fdb778-M0dGUI`, InvocationID
`410554fe90ab4198a800e14ba2478432`. Belege unter
`out/components-d4fdb778/component-tests/`:

| Beleg | SHA-256 |
| --- | --- |
| `native.log` | `adfd4f2ac14b1ffa90cdc07735b7fafa2c4daf9801bf85d6153ab6ab9bfdd9a0` |
| identische `before.json` / `after.json` | `0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3` |

Zusätzlich bestehen 10 lokale Quellregistrierungs- und 9 Archivtests; sieben
Linux-Export-Fixtures sind auf dem Mac ausgelassen. Das neue Transportprofil
v2 inventarisiert auch den Helfer und prüft ihn vor der Gastübertragung.

## Einmalige Paketbestätigung und Übergabe: 09b10fd7

Der auf `aegis-build` kompilierte und über GitHub verifizierte
[Komponentenstand](https://github.com/simgero/AegisOS/releases/tag/components-20260929T182444Z-09b10fd7-09b10fd7-Yk6W4s)
aus Commit `09b10fd73bd980c9134ba250cc42e8ac6e6ca378` besteht am
29. September 2026 um **18:27:09 UTC alle 119/119 Java-Gerätetests** in
9,183 Sekunden, ohne Abwahl oder übersprungene Tests. Alle bisherigen
105 Tests und die 14 neuen Koordinatortests wurden ausgeführt.

Geprüft sind die unveränderliche Zuordnung zum Antragsteller statt zum
bestätigenden Admin, expliziter privater/gemeinsamer Bereich, frische Prüfung
aller sechs Aktions-/Bereichskombinationen, Passwortablehnung und Sperrfrist,
Widerruf vor/während/nach Bestätigung sowie geänderter Antragstellerzustand.
Eine Vorbereitung lässt sich nicht zweimal übernehmen. Ein paralleler
Verlierer kann den Auftrag des Gewinners nicht abbrechen. Unklare Antworten
nach begonnener Übergabe bleiben ausdrücklich unbestätigt; der zugehörige
Abbruch wird angefordert, ohne erfolgreichen Abbau oder Rollback zu behaupten.

**Kontrollierte Authority-/Handoff-Fixtures, keine Paketinstallation:**
Die Tests führen weder echte Adminpasswortprüfung noch APT aus und starten
keinen nativen Paketarbeiter. Der echte `AospPackageAuthority`-Adapter ist
kompiliert, aber noch nicht im installierten Dienst verbunden. Der Gast
verwendet weiterhin Vollimage `927cf51d`; der neue LockSettings-Pfad läuft
noch nicht in dessen Systemserver. Kein neuer CE-Unlock-/Worker-Abbruchnachweis.
[Implementierung und offene Integration](../runtime/package-transactions.md).

Profil `d68845b3-62a9-4181-a7cd-c0f0a8e7d316`, unveränderte Boot-ID
`984f23bd-607e-4a6d-8c08-7ae91bccd4f5`. Vorher und nachher ausschließlich
Benutzer und Schlüsselverzeichnisse 0, CE 0 entsperrt, keine persönlichen
Kontexte; Enforcing und laufender Broker sind erneut bestätigt. Native Quellen
sind seit `8e1c2228` unverändert und behalten dessen 138er-Nachweis. Der
sichtbare Launcher und sämtliche bisherigen Profilpaare bleiben unverändert.

Bestätigter Lauf `identity-20260929T181947Z-09b10fd7-r9WPkv`, InvocationID
`b745d032ad554041a782568769fb257a`. Die erste lokale Release-Abfrage schlug
nach bestätigt abgeschlossenem Export fehl. Derselbe veröffentlichte Release
wurde anschließend mit passendem Commit und vollständigen Prüfsummen geladen;
Build und Export wurden nicht wiederholt.
Belege unter `out/components-09b10fd7/component-tests/`:

| Beleg | SHA-256 |
| --- | --- |
| `java.log` | `7094d0114034b43203388dae46fe3877a9b372af807d78ddae1f3d16de89ddfa` |
| identische `before.json` / `after.json` | `0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3` |
| installiertes APK | `0bf75fd6f2718459ecb41f0aa19ea4305f6476ca104f034a2a6a656ebc0f29b1` |

## Interne Paket-Passwortprüfung: 28811521

Der auf `aegis-build` kompilierte und über GitHub verifizierte
[Komponentenstand](https://github.com/simgero/AegisOS/releases/tag/components-20260929T175837Z-28811521-28811521-NvDO6t)
aus Commit `28811521b6c0b034a0e8cfa7b0cc8e8b715c0ff5` besteht am
29. September 2026 um **17:59:39 UTC alle 105/105 Java-Gerätetests** in
10,040 Sekunden, ohne Abwahl oder übersprungene Tests. Die 14 neuen Tests
betreffen Adminstatus, Typ, ID/Seriennummer, Paketbeschränkungen vor und nach
Verifikation, wiederholte Passwortpflicht, Ablehnung, Sperrfrist, fehlenden
Provider und Entfernung von HAT und Passwort-Handle aus der Antwort. Der
öffentliche Klassen-Eingang lehnt den App-Prozess ab und wischt sein Credential.
Alle bisherigen 91 Java-Tests wurden ebenfalls erneut ausgeführt.

**App-lokale Policy-Fixtures mit inerten Passwortantworten:** Es werden keine
echten Adminpasswörter geprüft. Der neue LockSettings-Pfad ist im übertragenen
`services.jar` kompiliert, läuft aber nicht im Systemserver des Vollimages
`927cf51d`. Der fehlende zusätzliche CE-Unlock bei echter Bestätigung ist noch
nicht im Vollsystem nachgewiesen. Aktion/Plan, Antragsteller, privates Ziel,
Lebenszyklus und Paketarbeiter sind weiterhin nicht verbunden.
[Details und Grenzen](../runtime/package-transactions.md).

Profil `d68845b3-62a9-4181-a7cd-c0f0a8e7d316`, unveränderte Boot-ID
`984f23bd-607e-4a6d-8c08-7ae91bccd4f5`. Vorher und nachher nur Benutzer und
Schlüsselverzeichnisse 0, CE 0 entsperrt, keine persönlichen Kontexte.
Enforcing und laufender Broker sind nachher erneut bestätigt. Die unveränderten
nativen Quellen behalten den 138er-Nachweis von `8e1c2228`, ohne erneuten Lauf.
Der sichtbare Launcher bleibt unverändert.

Der erste Lauf `3f79bdbe` bestand 104/105 Tests: Das Gast-Fixture setzte nur
`FLAG_GUEST`, behielt aber `FULL_SECONDARY` als Benutzertyp. Android16 ermittelt
`isGuest()` aus `userType`. Der korrigierte Test setzt `USER_TYPE_FULL_GUEST`
und prüft diese Voraussetzung. Nur Testcode änderte sich; der Fehlbeleg unter
`out/components-3f79bdbe/component-tests/` bleibt erhalten.

Bestätigter Lauf `identity-20260929T175632Z-28811521-Ounrcj`, InvocationID
`c0400492ff414ae3b1164a9999bb4d2f`. Belege unter
`out/components-28811521/component-tests/`:

| Beleg | SHA-256 |
| --- | --- |
| `java.log` | `05e32e57ceee75fa9651be132c50792287736c5c777728c1ff010a1d3ad34225` |
| identische `before.json` / `after.json` | `0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3` |
| installiertes APK | `b79a68d754352ab9cdab9468ab94f772b5e00b87db9fb1fcc5af0529b5e32800` |

Zusätzlich bestehen lokal 24 Quellintegrations-, 10 Identitätsregistrierungs-
und 9 Archivtests; sieben Linux-Export-Fixtures sind auf dem Mac ausgelassen.

## Paket-Store: 8e1c2228

Der [Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260929T162856Z-8e1c2228-8e1c2228-OrbqxJ)
aus Commit `8e1c222825b48a6167d174d68e4afae111487e72` besteht am
29. September 2026 um 16:31:01 UTC im lokalen `927cf51d`-Gast **138/138
native Tests** aus 20 Suiten in 15.076 ms, ohne Abwahl oder übersprungene Tests.
Alle zehn neuen Tests für atomare Paketgenerationen bestehen (119 ms).
Der vollständige bisherige native Umfang wurde erneut ausgeführt.

Geprüft werden vollständige Auswahl, erhaltene offene alte Referenzen,
Reopen, private Basisbindung, falsche Eigentümer/Seriennummern, bereits vor
Beginn gesetztes Abbruchsignal, beschädigte Quellen/Metadaten, konkurrierende
Schreiber, Symlink-/Hardlink-Ablehnung, unausgewählte Reste und Prozessbindung.
Die Fixtures enthalten kleine inerte Textdateien in `/data/local/tmp`.
APT, persönliche CE-Paketstores, Adminfreigaben, Abbruch während des Kopierens,
fsync-Fehler nach Umbenennung und Stromausfall sind nicht damit nachgewiesen.
Der Baustein ist noch nicht in den installierten Broker eingebunden.

Profil `d68845b3-62a9-4181-a7cd-c0f0a8e7d316`, Boot-ID
`984f23bd-607e-4a6d-8c08-7ae91bccd4f5`; Vorher-/Nachherzustand des echten
Benutzer-, Schlüsselverzeichnis- und Runtime-Bestands ist identisch.
Enforcing bleibt aktiv. Der erste Versuch endete vor Testbeginn wegen einer
ADB-Staging-Berechtigung; nur die lokale Übertragungsvorbereitung wurde
korrigiert, ohne Neubuild oder Lockerung der Produkt-Policy.

Belege: `out/components-8e1c2228/component-tests-attempt2/`, Log-SHA-256
`71fb61d07b4ad333690ad61be630877150fc78ad345a369078cf7f484490067d`;
Vorher und nachher jeweils
`0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3`.
Die unveränderten Java-Quellen behalten den separaten 91er-Nachweis unten.
Weitere Integration: [Pakettransaktionen](../runtime/package-transactions.md).

## AOSP-Allocator im lokalen Gast: be9d0d54

Der auf `aegis-build` kompilierte und über GitHub geprüfte
[Komponentenstand be9d0d54](https://github.com/simgero/AegisOS/releases/tag/components-20260929T160444Z-be9d0d54-be9d0d54-9d2hw4)
besteht am 29. September 2026 um 16:05:32 UTC **91/91 Java-Tests** in
12,518 Sekunden, ohne Abwahl oder übersprungene Tests. Der lokale Mac-QEMU
verwendet das [Vollimage 927cf51d](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T153553Z-927cf51d-17d08c57),
Profil `d68845b3-62a9-4181-a7cd-c0f0a8e7d316`, Boot-ID
`984f23bd-607e-4a6d-8c08-7ae91bccd4f5`. Enforcing, FBE, authentifiziertes ADB
und tatsächliches dm-verity für System und System-Extension sind bestätigt.

Das APK bindet erstmals `services.core` ein. Die drei zusätzlichen Tests
verwenden den tatsächlich integrierten `UserManagerService` in einem eigenen
App-Cache und dessen prozesslokale Tabellen. Eine freie Kennung bleibt
verfügbar; ein erschöpfter Nummernraum und das Überschreiten der bisherigen
Liste zuletzt gelöschter Kennungen führen zu keiner Wiedervergabe.
Wiederholte Vergabeversuche bauen die Reservierungen nicht ab.
Der vollständige bisherige Java-Umfang wurde wegen der geänderten APK-Bindung
ebenfalls neu ausgeführt.

Dies verändert nicht den echten Benutzerbestand des Systemservers.
Vorher und nachher bestehen ausschließlich Benutzer/CE-Schlüsselverzeichnisse
0, nur Benutzer 0 läuft, und es gibt keine privaten Runtime-Kontexte.
Das Plattform-Quellinventar des Komponentenrelease stimmt exakt mit dem
Vollimage überein. Die unveränderten nativen Quellen behalten ihren separat
geprüften 128er-Nachweis; sie wurden hier nicht erneut ausgeführt.

Belege unter `out/components-be9d0d54/component-tests/`:

| Beleg | SHA-256 |
| --- | --- |
| `java.log` | `e63a631bda6d1ae04400e0f1eb50bc4bc6f8760453726337e56c60f125757fb4` |
| identische `before.json` / `after.json` | `0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3` |
| installiertes Test-APK | `9eee6a2cd7f657174df4e2a138118ff60e4db0f2803088456119d41f1462361f` |

Das ist ein Test des echten Allocatorcodes in isolierten Fixtures, kein
erschöpfter produktiver Benutzerbestand. CLI-Löschberechtigungen und
Pakettransaktionen sind dadurch nicht geprüft. Der sichtbare Launcher wurde
nicht geändert; der neue Gast läuft ohne Fenster, bisherige Profilpaare
bleiben erhalten.

## Anmeldevorbereitung: 2f29f0ac

Der [Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260929T104228Z-2f29f0ac-2f29f0ac-QdCsAG)
des Commits `2f29f0ace2a6164980621b87037f20df344a569f` besteht im lokalen
QEMU-Image `d44ccb33` **68/68 Java-Tests**, ohne Abwahl oder übersprungene
Tests. Beide persönlichen Testbenutzer waren vorher abgemeldet, CE gesperrt
und ihre Kontexte unabhängig bestätigt entfernt; die Tests laufen in Benutzer 0.

Die sechs neuen Tests betreffen die einmalige Anmeldungsvorbereitung:
Identität einschließlich Seriennummer, geänderte Sperr-/Benutzer-Epochen,
Abbruch und konkurrierende Verwendung. Die unveränderten nativen Quellen
behalten den unten dokumentierten 128/128-Nachweis; sie wurden nicht erneut
ausgeführt. Der installierte Dienst behält noch die alte Reihenfolge.
Ein vollständiges neues Image und tatsächliche erste Anmeldung ohne vorherigen
Aufwärmversuch, falsches Passwort, Benutzerwechsel und Bildschirmsperre
bleiben für die Dienstkorrektur erforderlich.

Belege: `out/components-2f29f0a/component-tests/result.json`, `java.log`
(SHA-256 `505ce3ae28d79fbc6a1681c535ffecc704991aec442ec92980dd4374a38b7e63`)
und `guest.txt`
(`1e68b3808f92fb66318f06637902c734d3bd624e5a2c4889a361ac848f936320`).

## Private Home-Erststruktur: d44ccb33

Der auf `aegis-build` kompilierte Stand
`d44ccb3389889740f19373969a00216807b400a7` wurde über den verifizierten
[Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260929T095219Z-d44ccb33-d44ccb33-8tJnOs)
auf den Mac übertragen. Im lokalen QEMU-Image `6a807692`, nach vollständiger
Abmeldung beider Testbenutzer und unabhängig bestätigter CE-Sperre, bestehen
**128/128 native Tests aus 19 Suiten** in 10.884 ms, ohne Abwahl oder
übersprungene Tests.

Die vier zusätzlichen Home-Tests prüfen die genaue Erststruktur und gemappte
Eigentümer, Ablehnung vorhandener Dateien und Verknüpfungen ohne Überschreiben,
ungültige Identitäten sowie falsche Staging-Metadaten. Wiederholte Anlage
setzt private Änderungen nicht zurück. Die Quellen für Java/JNI sind gegenüber
`026665fb` unverändert; deren separater 62/62-Nachweis wurde nicht neu ausgeführt.

Diese Root-Fixtures verwenden unverschlüsseltes Tmpfs. Der installierte
Broker in `6a807692` enthält die Home-Erweiterung noch nicht. Reale erstmalige
CE-Provisionierung und GNU-Zugriff wurden anschließend im vollständigen
`d44ccb33` separat im [Zwei-Benutzer-Test](runtime-gnu-qemu-test.md) nachgewiesen.
Die tatsächlichen GNU-/Logout-/Persistenztests von `6a807692` sind separat
im [Zwei-Benutzer-Test](runtime-gnu-qemu-test.md) dokumentiert.

Belege: `out/components-d44ccb33/component-tests/result.json`, `native.log`
(SHA-256 `2d414ed52823bfa0a5555e461fbb942c8dea6be58025f9e709f15bfd4ed1f431`)
und `guest.txt`
(`8287f250f529f940326af62f01d1d96f789c56f1bdc0165cb84bb444bc70cec7`).

## Geprüfter Namespace-Komponentenstand c0d8c16c

Der Komponentenlauf `identity-20260929T064403Z-c0d8c16c-pQQoXM` ist kompiliert
und über den GitHub-Release
`components-20260929T065410Z-c0d8c16c-c0d8c16c-Dd980X` verifiziert übertragen.
Im lokalen QEMU-Image `ebf3610` bestehen **124/124 native Tests** aus 19 Suiten
in 10.909 ms. Die beiden neuen Tests bestätigen die Rücksetzung geerbten
OOM-Schutzes vor Exec und wiederholte Vorbereitung/Abbruch ohne FD-Verlust.
Auch die vollständigen UID-/GID-Maps und getrennten Namespaces bestehen.

Der native Elternprozess erhält feste Proc-/NSFS-Referenzen vom eigenen noch
gesperrten Clone. Die zusätzliche Ptrace-Freigabe ist entfernt; die AOSP-
Neverallow- und Kompatibilitätsprüfungen bestehen im Komponentenbuild. Die
Root-Fixtures ersetzen weiterhin keinen produktiven SELinux-Startnachweis.
Die unveränderten Java-/JNI-Quellen behalten den separaten 62/62-Nachweis.

Belege: `out/components-c0d8c16c/component-tests/result.json`, `native.log`
(SHA-256 `85bc9b6ab74169f4d8a94b200b10c66bd91e12a7e5babd0769a3611b763238e2`)
und `guest.txt`. GNU-Ausführung über AOSP-Anmeldung sowie ein vollständiger
neuer Boot sind noch offen.

# Komponentenläufe in lokalem QEMU

## Private Cgroup-Delegation: ebf3610

Am 29. September 2026 bestehen **122/122 native Tests aus 19 Suiten**
des Commits `ebf3610430ac22b98b515528edf156dd9e2c3465` im lokalen
QEMU-Image `d308ea6a`, ohne Filter oder übersprungene Tests. Der
[Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260929T052635Z-ebf36104-ebf36104-hF9HrH)
ist über GitHub geprüft empfangen. Die sechs zusätzlichen Tests prüfen
die private Delegation mit getrenntem Broker-Zweig, falsche Platzierung und
Grenzen, fremde Zweige, fehlende Delegation sowie die Lebensdauer des
gehaltenen `cgroup.procs`-Deskriptors.

Der bestehende AOSP-Testbenutzer 10 war dabei gestoppt und CE gesperrt;
der bisherige Runtime-Kontext war entfernt. Die Tests verwenden eigene
temporäre Cgroups und keine produktiven Benutzerkontexte.
**Diese Root-Fixtures beweisen noch nicht die neue Init-Platzierung oder
SELinux-Policy im vollständigen Image und keine GNU-Ausführung.** Die
62/62 Java-Tests aus `026665fb` werden für unveränderte Java-/JNI-Quellen
weiterverwendet. Der nachfolgende vollständige Build
`aosp-20260929T053136Z-ebf36104-d60e8839` ist inzwischen mit
`UPLOAD_VERIFIED` veröffentlicht und im eigenen lokalen Profil gebootet.
Die private Cgroup-Delegation ist tatsächlich eingerichtet. Der persönliche
Start scheitert nach erfolgreicher AOSP-Anmeldung an einer namespacelokalen
Capability-Prüfung; fünf Init-AVCs bleiben offen. Der komplette Befund steht
in der [Runtime-Policy](../runtime/selinux-integration.md). Dies ist noch kein
GNU-Ausführungs- oder Isolationsnachweis.

- Rohbelege: `out/components-ebf3610/component-tests/`.
- Native-Log SHA-256: `500c2814b41901fcf6a694018a0e1f0ff02a4c6038328a246051432dfb6c1fb9`.
- Gastbeleg SHA-256: `02f8c665bb9d0c4d44f124eb977bc5d8183fbb526a7a53620138ab12f1390182`.

## Mount-Besitz nach vollständigem Boot von a187a309

Build [`aosp-20260929T034544Z-a187a309-0129b0ef`](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T034544Z-a187a309-0129b0ef)
ist veröffentlicht und nach Asset-/AVB-Prüfung im eigenen gekoppelten Profil
gestartet. Enforcing, FBE, authentifiziertes ADB, dm-verity und die drei
Telefonie-Overrides sind bestätigt. Die vorherige ext4-Kontextverweigerung
ist überwunden. Der Broker scheitert nun mit `private base anchor errno=22`;
die aufgezeichneten Runtime-AVCs sind leer. Beleg:
`out/full-build-a187a309/boot-1/boot-health.json`.

Der gepinnte Kernel `50eb8d5d443b43f38d6e72f005f1b8601ac88a05`
markiert die `fsmount`-Dateibeschreibung mit `FMODE_NEED_UNMOUNT`.
Ihr letztes Schließen ruft `dissolve_on_fput()` auf, auch wenn eine separat
geöffnete Wurzel noch einen Pfadverweis hält. `move_mount()` weist diesen
nicht mehr eingebundenen Mount anschließend mit `EINVAL` zurück. Der
Basisöffner muss deshalb den ursprünglichen Mount-Deskriptor zurückgeben;
der lesbare Deskriptor dient nur der unveränderten Labelprüfung.

Zwei neue native Gerätetests bestehen: ein tatsächlich abgetrennter
tmpfs-Mount reproduziert den Verlust beim Ersetzen des Besitzers; ein Test
des echten Basisöffners prüft die unveränderliche ext4-Datei, private
Einbindung, persönlichen ID-Mount und abgewiesenen Schreibzugriff. Stand
`d308ea6a` besteht **116/116 native Tests in 18 Suiten**, ohne Filter oder
übersprungene Tests, im bestehenden lokalen Image `a187a309` (10.819 ms).
Die Quellen wurden auf `aegis-build` kompiliert und als
[geprüfter Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260929T042716Z-d308ea6a-d308ea6a-ASJL50)
übertragen. Der erste Versuch `88eb5509` scheiterte an einem vorzeichenbehafteten
GTest-Vergleich; korrigiert wurde dessen Typ, nicht die geprüfte Bedingung.

Die 62 Java-Tests des früheren Komponentenstands werden für unveränderte
Java-/JNI-Quellen beibehalten. Die nativen Tests laufen als Entwicklungs-root;
sie ersetzen nicht den echten Init-Start in der Broker-SELinux-Domain.
Vollbuild `aosp-20260929T042930Z-d308ea6a-57b5567f` läuft für diesen Nachweis.
Kein persönlicher AOSP-Benutzer wurde im neuen Gast angelegt und GNU-Programme
laufen weiterhin nicht.

- Rohbelege: `out/components-d308ea6a/component-tests/`.
- Native-Log SHA-256: `106dd974c145ed18e75ea7cd0653918050285d592efa55cdc2db6537112cd958`.
- Gastbeleg SHA-256: `e3f7eeac7187d76ec0142cbe1c294ba93dca5ba040819d87808b6ab6d14aee89`.

## Terminal- und Basisvorbereitung: 026665fb auf Image 030dd177

Der Komponentenlauf `identity-20260929T030627Z-026665fb-TMiVTr` kompiliert
CLI, Dienst, JNI, Broker und Policy einschließlich der Verbotsprüfungen.
Die Korrekturen betreffen den öffentlichen widerrufbaren Terminalkanal,
ueventds asynchrone Loop-Gerätedatei und den Kernel-Lesezugriff auf die
unveränderliche Basis. Der vorherige Lauf `05bdef02` ist wegen des unter
Bionic fehlenden `explicit_bzero` fehlgeschlagen; der Ersatz verwendet
`memset_explicit` aus dem gepinnten Android.

Nach [GitHub-Export und Verifikation](https://github.com/simgero/AegisOS/releases/tag/components-20260929T030923Z-026665fb-026665fb-sJgvbF)
bestehen im separaten lokalen Profil `runtime-030dd177` **114/114 native
Tests in 18 Suiten und 62/62 Java-Tests**, ohne Filter oder übersprungene
Tests. Der Gast verwendet Enforcing, FBE und authentifiziertes ADB. Nur die
Testprogramme und das Test-APK wurden in den Entwicklungsbereich übertragen;
Systemdienst, JNI und Policy des alten Images wurden nicht ersetzt.

Der Broker dieses alten Images scheitert weiterhin beim Öffnen der Basis.
Diese Komponentennachweise ersetzen daher weder den neuen Produktionspfad
noch Passwort-/Rohmodusprüfungen der öffentlichen CLI, GNU-Ausführung oder
Zwei-Benutzer-Isolation.

- Rohbelege: `out/components-026665fb/component-tests/`.
- Native-Log SHA-256: `fb7e22c651f451849b305e42ea46a76b875f80a216bde0936d67a7fc1fe63fc4`.
- Java-Log SHA-256: `2d0664d0f8f39d609b3fedfdda32c1d8a4baa96170a48646b8934a0f7acc952b`.
- Gastbeleg SHA-256: `e1b02a074ce729bd0c9240e94e6e5fa7ff478243b2243fb81b3f677f4e055387`.

Der anschließende vollständige Build
[`aosp-20260929T031210Z-026665fb-02ff1cf0`](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T031210Z-026665fb-02ff1cf0)
ist mit `UPLOAD_VERIFIED` abgeschlossen. Nach Prüfung aller 21 Assets und
der AVB-Kette bootet das neue Profil `runtime-026665fb` mit Enforcing,
FBE, authentifiziertem ADB, tatsächlichen Verity-Tabellen und allen drei
Telefonie-Booleans auf false. Der alte Testgast wurde geordnet mit bestätigtem
Android-Powerdown und sauberem KeyMint-Helper beendet; seine Daten bleiben erhalten.

Der neue Basis-Wartepfad erreicht beim ersten Start die ext4-Superblock-Erzeugung,
ohne den früheren Kernel-FD-AVC. Dort scheitert er mit `errno=13` und
`aegis_runtime_broker -> aegis_runtime_base_file:filesystem relabelfrom`.
Im gepinnten Kernel `50eb8d5d443b43f38d6e72f005f1b8601ac88a05` setzt
`selinux_set_mnt_opts()` erst die Superblock-SID aus `context=`, anschließend
prüft `may_context_mount_inode_relabel()` erneut `relabelfrom` auf dieser SID.
Die bereits vorhandene AOSP-Regel `allow fs_type self:filesystem associate`
deckt die folgende Zuordnungsprüfung ab. Deshalb wird ausschließlich die
fehlende Berechtigung für Broker und Basis-Dateisystem ergänzt. Kein permissiver
Betrieb und keine Regeländerung im laufenden Gast. Die neue Policy braucht
einen weiteren vollständigen Build und Bootnachweis; Native-/Java-Quellen und
Tests bleiben identisch. Rohbeleg: `out/full-build-026665fb/boot-1/boot-health.json`.

Im laufenden vollständigen Image `026665fb` wurden zusätzlich die tatsächliche
CLI und JNI-Passworteingabe geprüft. Ein unbekannter persönlicher Benutzer
bleibt abgewiesen und das Terminal unauthentifiziert; der zufällige Testwert
wird nicht zurückgeschrieben und fehlt in den drei erfassten Gastlogs.
`linux start`, `status`, `stop` und `shell` werden ohne Anmeldung zurückgewiesen.
Ein separater Strg+C-Test an der aktiven Passwortabfrage bestätigt identische
Termios-Werte vor und nach der CLI sowie deren Exitcode 130. Der umgebende
su/ADB-Prozess erhält dasselbe PTY-Signal und endet ebenfalls mit 130; die
erste Testfassung hatte dort fälschlich 0 erwartet. Der korrigierte Test prüft
weiterhin den separat gemeldeten CLI-Exitcode und die exakte Wiederherstellung.
Es wurden keine persönlichen Benutzer angelegt. Das sind öffentliche
Negativ-/Passwortmodus-Tests, kein AOSP-Passwortnachweis und kein GNU-Rohmodus-,
Terminalwiderrufs- oder Isolationsnachweis. Belege:
`out/full-build-026665fb/identity-test/negative-result.json` und
`interrupted-password.json` im selben Verzeichnis.

## Init-Namespace-Übergabe: Komponenten 3b350e74 auf Image 4e53dc18

Am 29. September 2026 bestehen **114/114 native Tests in 18 Suiten**, ohne
Filter oder übersprungene Tests. Lauf `identity-20260929T014019Z-3b350e74-FRvHfp`
kompiliert die Komponenten und die NSFS-Policy erfolgreich; der
[Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260929T014924Z-3b350e74-3b350e74-d9cLz4)
wurde mit geprüften Artefakten über GitHub übertragen. Die neue Prüfung
weist falsche, vertauschte oder ausgetauschte Namespace-Handles zurück und
prüft die Freigabe temporärer Deskriptoren. Die positive wiederholte Übergabe
derselben Identität besteht; der vorhandene Fork-Test weist eine erneute
Besitzbindung im Kind zurück.

Der Test lief im separaten lokalen Profil `runtime-4e53dc18`, mit Bootabschluss,
authentifiziertem ADB und SELinux Enforcing. Eigene Test-Cgroups bleiben nicht
zurück. Es gibt nur Systembenutzer 0; der produktive Broker dieses alten
Images ist weiter gestoppt. Entwicklungs-root-Fixtures beweisen weder die
neue NSFS-Policy im Gast noch den Init-gestarteten Dienst oder eine GNU-Sitzung.
Unveränderte Java-Tests wurden nicht erneut ausgeführt.

- Rohbelege: `out/components-3b350e74/native-tests/`.
- Native-Log SHA-256: `5a9a08697c5b62c027d776dbcee4261623d7b2132055c0628ae7f57b73f13412`.
- Gastbeleg SHA-256: `3ac12c40d8263be284d58182c688263fa969cea44f32191a184388727dbbc419`.

Der folgende Image-Commit `afaf6462` ändert gegenüber diesen getesteten
Komponenten ausschließlich die benannte Cgroup-Type-Transition samt
Dateisystemzuordnung und deren Dokumentation. Native Quellen und Tests sind
identisch. Die zusätzliche Policy muss im vollständigen Build kompiliert
und mit dem tatsächlichen Dienst geprüft werden; sie ist kein Ergebnis
dieses Komponentenlaufs.

## Komponentenstand 4e53dc18 auf dem verwalteten Testimage a8d38b97

Am 29. September 2026 bestehen **113/113 native Tests** aus
`4e53dc180da1129a06b7b670f364c5ab0cf90e72`, ohne Filter oder übersprungene
Tests. Komponentenlauf `identity-20260929T005029Z-4e53dc18-mPGAUM` ist
erfolgreich kompiliert; sein
[Release](https://github.com/simgero/AegisOS/releases/tag/components-20260929T005737Z-4e53dc18-4e53dc18-3W0vyB)
wurde über GitHub mit geprüften Artefakten übertragen. Das lokale Profil
`runtime-a8d38b97` bootet mit authentifiziertem ADB, FBE, Verity und SELinux
Enforcing. Es enthält keine persönlichen Benutzer; sein produktiver Broker
ist nach dem bereits dokumentierten Startabbruch gestoppt.

Die Tests prüfen weiterhin Entwicklungs-root-Fixtures mit eigenen temporären
Ressourcen. Sie bestätigen auch nach Ergänzung der phasenbezogenen Diagnose
das bisherige native Verhalten, nicht den Start des echten Dienstes, die
Helferprüfung im Broker oder eine persönliche GNU-Sitzung. Das unveränderte
Image enthält die neuen Broker-Diagnosen noch nicht. Java wurde in diesem
Lauf nicht erneut getestet.

- Rohbelege: `out/components-4e53dc18/native-tests/`.
- Native-Log SHA-256: `56254b4ff8ae0b836193567f908409a7a267b4e4fabac352125822fe352c949f`.
- Gastbeleg SHA-256: `38be475a91518d789ae8308dd1da275899c73b03d05df4898a265b22ea678051`.

## Komponentenstand a8d38b97 auf dem geprüften Image 2a766ab5

Am 29. September 2026 bestehen **113/113 native Tests**, ohne Filter oder
übersprungene Tests, im lokalen Mac-QEMU mit SELinux Enforcing und
authentifiziertem ADB. Die auf `aegis-build` erzeugten Komponenten stammen aus
`a8d38b97b8e3c8fb15365327fdadb04506baa019`, Lauf
`identity-20260929T000603Z-a8d38b97-WVXn2V`. Der
[Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20260929T001323Z-a8d38b97-a8d38b97-RodhFL)
wurde über GitHub übertragen und vor Ausführung auf seine Prüfsummen geprüft.
Das gebootete Image bleibt unverändert `2a766ab5`; keine Framework-Dateien
wurden ersetzt. Dieser Lauf enthält keine erneute Java-Testausführung.

Die zwei neuen Cgroup-Tests bestätigen, dass Androids Elternbereich
`system:system 0775` akzeptiert wird, private Gruppen weiterhin zwingend
`root:root 0700` verlangen und abweichende Eigentümer oder zu offene Modi vor
dem Anlegen eines Runtime-Unterbaums scheitern. Die Tests verändern nur ihren
eigenen temporären Unterbaum, nicht den wirklichen Android-Cgroup-Elternbereich.

- Rohbelege: `out/components-a8d38b97/native-tests/`.
- Native-Log SHA-256: `131fa1d21bf9831afc6185109164cad3f9e2b2454b9d21089b1a237171bab761`.
- Gastbeleg SHA-256: `ae923c374233a284952958c950f4958e07066e8b45c64405d887642ec3f88845`.

Diese Entwicklungs-root-Fixtures beweisen weiterhin keine Ausführung in den
neuen produktiven SELinux-Domänen. Das gebootete Produkt bleibt `absent`;
der verwaltete Dienststart und eine persönliche GNU/Linux-Sitzung sind offen.

## Vollständiges Image und Komponentenstand 2a766ab5

Am 29. September 2026 bestehen im neuen lokalen Mac-QEMU **111/111 native
und 62/62 Java-Tests**, ohne Abwahl. Image und Komponenten stammen beide aus
`2a766ab5d6a6ef5ed4c01fd87fa4e97bb2e2961d`. Der
[vollständige Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260928T224751Z-2a766ab5-ed1329db)
wurde nach `UPLOAD_VERIFIED` über GitHub bezogen und lokal auf Prüfsummen,
Eingangsbelege und AVB-Ketten geprüft.

Die Product-RRO ist tatsächlich aktiv: `config_sms_capable`,
`config_voice_capable` und `config_force_phone_globals_creation` sind false.
Der zuvor fehlgeschlagene, unveränderte Produktkonfigurationstest besteht nun.
Bootabschluss, authentifiziertes ADB, SELinux Enforcing, FBE und tatsächliche
dm-verity-Tabellen mit `restart_on_corruption` sind bestätigt. Die installierte
AEGIS-CLI antwortet und verweigert alle drei Linux-Lebenszyklusbefehle, solange
der Produktmodus `absent` ist.

- Profil: `out/qemu-profiles/foundation-2a766ab5`.
- Bootbelege: `out/full-build-2a766ab5/boot-1/`.
- Vollständige Testergebnisse: `out/full-build-2a766ab5/component-tests/`.
- Native-Log SHA-256: `e6e81922c8b930e2703611f76264d464d6045a8e7578f59250118201a1db389c`.
- Java-Log SHA-256: `7a526a3fef77b23f93a5f2cd0452b640fdcf51873a9b78439fe4598325d23fd5`.

Diese Komponentenprüfung führt weiterhin keine produktive persönliche
GNU/Linux-Sitzung aus. Die nativen Tests laufen als Entwicklungs-root;
produktive Runtime-SELinux-Domänen, Paketverwaltung und der gesamte
Zwei-Benutzer-Linux-Ablauf bleiben unbewiesen.

## Komponentenstand 2a766ab5 auf bfe90925

Am 29. September 2026 wurden die auf `aegis-build` kompilierten Komponenten
`2a766ab5d6a6ef5ed4c01fd87fa4e97bb2e2961d` im getrennten lokalen
`runtime-bfe90925`-QEMU geprüft: **111/111 native, 61/62 Java-Tests**.
Die Sitzung verwendet SELinux Enforcing, authentifiziertes ADB und unveränderte
Images von `bfe90925`. Es wurde kein Test abgewählt. Die einzige Java-Abweichung
ist `config_sms_capable=true` aus der geerbten Vendor-RRO dieses Images.

Der unmittelbar vorhergehende Komponentenstand `ac87df1f` hatte 106/110 native
Tests bestanden. Vier positive Terminaltests öffneten Androids Tmpfs-`/dev/ptmx`
statt des vom echten Supervisor verwendeten Devpts-Knotens. Die Fixture wurde
korrigiert und ein zusätzlicher Test für die fortgesetzte Legacy-Ablehnung
hinzugefügt; der produktive Terminalprüfer wurde nicht gelockert.

Drei neue Java-Tests bestätigen, dass widerrufene interne Sitzungsbindungen auch
nach einer anderen Anmeldung ungültig bleiben, mehrere gültige Anmeldungen
bis zum Widerruf bestehen können und fremde Gate-/geschlossene Scope-Bindungen
abgewiesen werden. Dabei kommt ein simulierter Quiescer zum Einsatz.

- Build: `identity-20260928T223826Z-2a766ab5-sutWPl`.
- [Geprüfter Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T224549Z-2a766ab5-2a766ab5-u7YQ7B).
- Lokale Rohbelege: `out/components-2a766ab5/component-tests/`.
- Native-Log SHA-256: `7e01d37b9e025b3d579233329e932063adac73f12ef5fa19db837cbc698f5ee2`.
- Java-Log SHA-256: `cb46cc4cb258be79e7fff0e43b150af201439bdf3b819632163688f72f9809a6`.

Die tatsächlich angebundenen verwalteten Dienstpfade sind kompiliert, aber im
Produkt weiterhin deaktiviert. Diese root-/Komponententests beweisen weder
produktive SELinux-Domänen noch persönliche Debian-Ausführung, Paketverwaltung
oder vollständigen CE-Ressourcenabbau. Ältere Ergebnisse unten gelten nur für
ihre jeweils genannten Stände.

Stand: 28. September 2026. Einzelne Module und der separate CLI-Ablauf sind
geprüft; eine persönliche GNU/Linux-Sitzung ist noch nicht integriert.

## Korrigierter nativer Lauf `bfe90925`

Im späteren vollständigen Image desselben Commits, Lauf
`out/full-build-bfe90925/boot-1`, bestehen erneut **88 von 88 native Tests**.
Von **53 Java-Tests bestehen 52**. Die zusätzliche Prüfung
`productDoesNotForceCellularInitializationWithoutAModem` findet
`config_sms_capable=true`: Die aktivierte Vendor-RRO
`android.cuttlefish.phone.overlay` überschreibt den bereits auf false gesetzten
Framework-Wert. Die beiden anderen Telefonie-Booleans sind false und sämtliche
Telefonie-Features fehlen wie vorgesehen. Es wurde kein Test abgeschwächt.
Eine gezielte Product-RRO ist vorbereitet, noch nicht im Image gebaut oder
im Gast geprüft. Nachweise: `out/full-build-bfe90925/component-tests/` und
`out/full-build-bfe90925/boot-1/sms-overlay.txt`.

Alle **88 nativen Tests bestehen**, ohne Abwahl, im lokalen Mac-QEMU-Lauf
`out/components-7c09d1f/boot-1` auf dem vollständigen Image `25fde995`.
Die geprüften ARM64-Module stammen aus Commit
`bfe90925aa1873761f091a74a70e2ee1cf7b8bf9`, Builderlauf
`identity-20260928T205138Z-bfe90925-XDN6PJ`, InvocationID
`ab79d182a8684ad4bd5797097c4d4174`. Der
[Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T205942Z-bfe90925-bfe90925-LhHoqY)
wurde über GitHub transportiert und auf dem Mac geprüft.

Die drei zuvor fehlgeschlagenen Mounttests bestehen nach der privaten
Broker-Einhängung. Zwei zusätzliche Tests bestätigen die Namespace-Bindung
und den zunächst abgewiesenen detached Mount, der erst nach der kontrollierten
Einhängung geklont werden kann. Androids eigener Mount-Baum bleibt unverändert.
Bootabschluss, authentifiziertes ADB, Kernel und SELinux Enforcing wurden vorab
geprüft. Nachweise: `out/components-7c09d1f/native-tests/` mit Gastzustand,
vollständigem Rohlog und SHA-256-Ergebnisdatei.

Dies sind Entwicklungs-root-Tests mit einer kleinen Tmpfs-Basis. Sie beweisen
weder die produktiven SELinux-Domänen noch einen tatsächlichen Debian-Start,
persönliche Linux-Isolation oder Paketinstallation. Die neue Telefonie-
Produktprüfung ist in diesen Komponenten enthalten, verlangt aber das neue
vollständige Image; Java wurde deshalb hier nicht erneut ausgeführt.

## Vollständiger Komponentenlauf im gebooteten Image `25fde995`

QEMU-Lauf `out/full-build-25fde995/boot-1` meldet `sys.boot_completed=1` und
SELinux Enforcing. Derselbe Komponentenstand `336e9275` wurde erneut gegen das
vollständig gestartete Produkt geprüft: **52 von 52 Java-Tests und 83 von 86
nativen Tests bestanden**. Die drei unten beschriebenen Basis-Mount-Fehler
bleiben unverändert. Die vier bisher ausgenommenen Produktkonfigurationstests
bestätigen nun die installierten Kennungsregister, die genau einmal konfigurierte
AEGIS-Dienstklasse, mindestens vier Benutzerplätze und die ausgeschlossenen
Funkfunktionen. Nachweise: `out/full-build-25fde995/component-tests/`.

Zusätzlich erreichen die installierten CLI-Befehle `aegis user list` und
`aegis status` den echten Dienst: keine persönlichen Benutzer, Terminal nicht
angemeldet, Ersteinrichtung verfügbar, Runtime nicht installiert. Das ersetzt
keine interaktive Passwortprüfung. Der spätere, separate
[CLI-Gasttest](identity-cli-qemu-test.md) bestätigt inzwischen zwei persönliche
Benutzer samt Passwortwechsel, Abmeldung und geordnetem Neustart.

## Früherer vollständiger nativer Lauf mit dem neuen Kernel

Im Image `be1ad9ad` wurden alle **86 nativen Tests des Komponentencommits
`336e9275`** ausgeführt: **83 bestanden, drei fehlgeschlagen, keiner abgewählt**.
Lokaler Mac-QEMU, Kernel `6.12.18-android16-1-maybe-dirty-4k`, authentifiziertes
ADB und SELinux Enforcing. Androids Framework-Boot bleibt wegen der separat
beschriebenen Audiokonfiguration unvollständig; hier wurden ausschließlich
native Kernel-/Dateisystembausteine geprüft, keine Java- oder Anmeldetests.

Neu positiv belegt sind die tatsächlichen Namensräume, getrennte Host-IDs
zweier gleichzeitig lebender Kinder, geschlossene Startfreigabe, private
Geräteansichten, reservierte Bionic-Kennungen und der kombinierte Speichergruppen-/
Namespace-Start. Alle fünf CE-Negativtests bestehen nun einschließlich der
Tmpfs-Seriennummernattribute. Dies sind Tests als Entwicklungs-root, kein Nachweis
produktiver SELinux-Domänen oder persönlicher Linux-Sitzungen.

Drei Basis-Mount-Tests scheitern weiterhin mit `EINVAL`:

- `PreparedMappingsKeepExecBlockedWhileBaseMountIsBuilt`
- `TwoViewsKeepSharedInodesWithSeparateUserOwnership`
- `WritableOrNonDirectorySourcesAreRefusedWithoutMutationOrLeaks`

Der gepinnte Kernel ruft bei `open_tree(OPEN_TREE_CLONE)` `__do_loopback` auf,
das über `check_mnt` einen Quellmount im aktuellen Mount-Namespace verlangt.
Die Fixtures liefern einen detached `fsmount`; auch `aegis_base_open` liefert
gegenwärtig einen solchen Mount. Die Besitz-/Namespace-Anbindung muss vor der
Runtime-Aktivierung korrigiert werden. Prüfungen wurden nicht gelockert und
kein alternativer `chroot`-Betrieb eingeführt.

Nachweise: `out/full-build-be1ad9ad/native-tests-before-boot/` mit Rohlog,
Gastzustand, Ergebnis und SHA-256. Android und der gepaarte Schlüssel-Helper
wurden anschließend mit bestätigtem Powerdown beendet.

Die folgenden Abschnitte halten frühere, eingeschränkte Läufe fest.

## Herkunft

- Buildcommit: `9d3295106d7c6f4841ab5db1bf29a8cd085cccf3`.
- Builderlauf: `identity-20260928T143037Z-9d329510-HLLRGJ`,
  InvocationID `2d1f199b35a0432287e727df2208c4eb`.
- AOSP meldete nach 3:14 Minuten erfolgreichen Abschluss. Status:
  `IDENTITY_COMPILED_NOT_INSTALLED`; 16 Artefakte wurden eingesammelt.
- [Geprüfter GitHub-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T143513Z-9d329510-9d329510-RPcQHk).
  Exporter und Mac haben Transport, Manifest und Datei-Prüfsummen geprüft.
- QEMU-Lauf `out/qemu-first-boot/components-9d32951`, authentifiziertes ADB
  auf `127.0.0.1:15655`, Bootabschluss `1`, SELinux `Enforcing`, FBE `file`.
  Weiterhin das bisherige Android-Image und dessen Kernel; kein Austausch
  von Framework-JARs im laufenden Gast.

## Tatsächliche Ergebnisse

| Gruppe | Ausgeführt | Bestanden | Fehlgeschlagen | Nicht ausgewählt |
| --- | ---: | ---: | ---: | ---: |
| Java/Android | 40 | 40 | 0 | 4 |
| Native ARM64 | 35 | 32 | 3 | 16 |

Die Java-Auswahl umfasst `CredentialTransportTest`, `InitialAdminRecoveryTest`,
`RuntimeUidMapTest`, `RuntimeAdmissionTest` und `AegisRuntimeStorageTest`.
Sie prüft unter anderem Parcel-Kopien, Wiederaufnahme, Zuordnungsrechnung,
Serialisierung und Fehlerbehandlung innerhalb der Testanwendung. Vier
`ProductConfigurationTest`-Tests verlangen die Integration im neuen Image
und wurden ausdrücklich nicht ausgewählt.

Die nativen Tests prüfen Kontrollnachrichten, Descriptor-Übergaben, das
Abweisen direkter Helferstarts, Seccomp, Kindprozessbeobachtung sowie
Speicher- und Setup-Fehlerpfade. Die 15 Namespace-Tests und der Test der
installierten AOSP-Kennungsregister wurden nicht ausgewählt; ihnen fehlen
im alten Image die benötigten Voraussetzungen.

Drei ausgeführte `RuntimeCe`-Tests scheitern beim Anlegen beziehungsweise
Lesen ihrer Tmpfs-Seriennummernattribute mit `EOPNOTSUPP`:

- `MissingOrNonCanonicalSerialIsNeverRepairedOrAccepted`
- `ReusedNumberWithAnotherSerialIsRejectedWithoutMutation`
- `PlausibleDirectoryNamesAndSerialDoNotSubstituteForEncryption`

Der Gast bestätigt `# CONFIG_TMPFS_XATTR is not set`. Der geplante Kernel
fordert diese Option nun ausdrücklich an; die drei Tests müssen danach
erneut bestehen. Ihre Voraussetzungen werden nicht durch Überspringen,
Lockerung der Prüfungen oder Ausschalten von SELinux ersetzt.

## Nachvollziehen und Grenzen

Die lokalen Nachweise liegen unter `out/component-tests-9d329510/`:
`java.log`, `native.log`, `guest.txt`, `result.json`. Letzteres enthält Zähler,
Commit, Release, Grenzen und SHA-256 der drei Rohprotokolle.

```sh
adb -s 127.0.0.1:15655 shell am instrument -w -r \
  -e class org.aegisos.identity.CredentialTransportTest,org.aegisos.identity.InitialAdminRecoveryTest,org.aegisos.identity.RuntimeUidMapTest,org.aegisos.identity.RuntimeAdmissionTest,com.android.server.aegis.AegisRuntimeStorageTest \
  org.aegisos.identity.tests/androidx.test.runner.AndroidJUnitRunner
adb -s 127.0.0.1:15655 shell su 0 \
  /data/local/tmp/aegis-components-9d329510/AegisRuntimeNativeTests \
  '--gtest_filter=-RuntimeNamespace.*:RuntimeRegistry.*'
```

Der Testgast und sein KeyMint-Hilfssystem wurden danach geordnet beendet;
beide Dateisysteme sind ausgehängt. Vor einer Wiederholung muss dasselbe
Profil erneut gestartet und ADB verbunden werden.

Dies belegt weder den Systemserver-Dienst noch AEGIS-Passwortanmeldung,
Runtime-Start, SELinux-Übergänge oder Isolation zweier persönlicher Benutzer.
Diese Nachweise folgen mit dem vollständig integrierten Image.

## Zweiter Lauf: Speichergruppen und Ressourcenbesitz

Commit `481f738c7417f644c01d08f5a1cb4426aa761eaa` wurde im Lauf
`identity-20260928T154101Z-481f738c-NDVInX` erfolgreich kompiliert
(InvocationID `3fdadaffe02b4620943459ee1a881045`, Buildzeit 7:43 Minuten).
Die nativen Module einschließlich F2FS-Korrektur, Speichergruppen und
zusammengesetztem Kontextbesitzer umfassen jetzt **63 kompilierte Tests**.
Der [zweite Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T154958Z-481f738c-96f9ed6b-WR0G9Q)
wurde vom Exporter zurückgeladen und verglichen sowie auf dem Mac geprüft
heruntergeladen und entpackt.

Im lokalen QEMU-Lauf `out/qemu-first-boot/components-481f738` wurden **elf neue
native Tests ausgeführt; alle elf bestehen**. Drei davon prüfen den Abbau
abgewiesener Starts, erhaltene Aufrufer-Deskriptoren, den Schutz vorhandener
Gruppen und Kontextbesitz nach direktem `clone3`. Acht prüfen tatsächliche
Speicher-Cgroup-Dateien und Grenzen, einmalige Freigabe, falsche Identität,
fehlende Controller, manipulierte Limits und getrennten Stopp zweier Kinder.
Eine leere Gruppe wird dabei ausdrücklich von einem verbrauchten Pidfd-Exit
unterschieden. Die direkten Clone-Tests erfassen Bionics geerbten PID-Cache.

```sh
adb -s 127.0.0.1:15655 shell su 0 \
  /data/local/tmp/aegis-native-481f738/AegisRuntimeNativeTests \
  '--gtest_filter=RuntimeMemoryGroup.*:RuntimeContext.*-RuntimeMemoryGroup.NamespaceIsAlreadyBoundedWhileExecGateIsClosed'
```

Dieser Lauf verwendet weiterhin den bisherigen Kernel. Der kombinierte neue
Namespace-/Cgroup-Test wurde deshalb nicht ausgewählt. Insgesamt sind in
diesem Lauf 52 native Tests nicht ausgewählt; die Java-Tests wurden nicht erneut
ausgeführt. Es wurde kein tatsächlicher CE-HOME-Start und kein produktiver
SELinux-Übergang geprüft. Das ist kein Speicherdruck-/OOM-Nachweis und keine
Abnahme persönlicher GNU/Linux-Sitzungen oder des vollständigen Logouts.

Der Gast meldete Bootabschluss `1`, `ro.adb.secure=1`, FBE `file`, globales
SELinux `Enforcing` und den Cgroup-Speichercontroller. Root-Gerätetests allein
belegen keine durchgesetzte Produktionspolicy des künftigen Brokers.
Alle eigenen Testgruppen wurden entfernt; Android und KeyMint-Helfer wurden
geordnet beendet. Android bestätigte das Aushängen von `/data` und `/metadata`,
der Helper `AEGIS_HELPER_SHUTDOWN_CLEAN`.

Lokale Nachweise: `out/component-tests-481f738/{native-memory.log,guest.txt,cleanup.txt,result.json}`.
SHA-256 des nativen Rohprotokolls:
`e657c2d6ecd9ce0fa79d1d893bef3ab63eb7d39d02303075a45cd77ebb8551b6`.
Das Ergebnis-JSON enthält auch die Hashes der Gast- und Bereinigungsprotokolle.

## Dritter Lauf: Brokerprotokoll, Wiederherstellung und Basisparser

Commit `878fc970ace1a19a7a6f637e8e7df3763536b649` wurde im Lauf
`identity-20260928T174855Z-878fc970-sllLJ3` vollständig gebaut und über den
[geprüften Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T175708Z-878fc970-878fc970-sYlkeV)
bezogen. Im lokalen QEMU bestehen **48 von 48 ausgewählten Java-Tests**
(vier Produktkonfigurationstests nicht ausgewählt). Die Auswahl enthält jetzt
auch `RuntimeBrokerProtocolTest`. Von **23 ausgewählten nativen Tests bestehen
22; einer schlägt fehl**. 63 weitere native Tests wurden nicht ausgewählt.

Bestanden haben neun Protokolltests, drei Besitztests, fünf echte Cgroup-
Wiederherstellungstests, ein Fristtest und vier der fünf Basisparser-Tests.
`RuntimeBaseImage.DuplicateUnknownAndTrailingDataCannotAuthorizeMount` nahm
eine unzulässige Variante an. Die gepinnte JsonCpp-Version überspringt an
bestimmten Objektpositionen Kommentare trotz `allowComments=false`. Commit
`336e9275` weist daher Schrägstriche außerhalb von JSON-Zeichenketten bereits
in der begrenzten lexikalischen Vorprüfung ab. Er ergänzt mehrere Kommentar-
positionen und einen zulässigen String mit URL/Kommentarsyntax als Regression.
Der korrigierte Komponentenlauf
`identity-20260928T181002Z-336e9275-y2tzSs` endete um 18:10:51 UTC erfolgreich;
im gesonderten Gast-Nachtest bestehen **alle fünf Parser-Tests**. Der
[neue Komponenten-Release](https://github.com/simgero/AegisOS/releases/tag/components-20260928T181151Z-336e9275-336e9275-V8Iq0V)
ist geprüft. 81 weitere native Tests und unveränderte Java-Tests wurden in diesem
gezielten Nachtest nicht wiederholt. Nachweis: `out/component-tests-336e9275/`,
einschließlich `native-base.log`, `guest.txt` und Hash-/Ergebnisdatei `result.json`.

Nachweise des fehlgeschlagenen Ausgangslaufs liegen unverändert in
`out/component-tests-878fc970/{guest.txt,native.log,java.log,cleanup.txt,result.json}`.
Die eigenen Cgroups wurden entfernt. Nach dem gezielten Parser-Nachtest
wurden Android und KeyMint-Helfer geordnet beendet: `/data` und `/metadata`
ausgehängt, Android Power-down und bestätigter sauberer Helper-Abschluss. Authentifiziertes ADB und SELinux Enforcing
sind bestätigt. Root-Komponententests belegen weiterhin weder Produktions-
SELinux-Regeln noch den aktivierten Broker, echte Basismounts oder persönliche
GNU/Linux-Sitzungen. Der neue Kernel ist noch nicht in diesem Gast gestartet.
