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
