# Phase 1 – Ergebnisindex der Referenzläufe

Stand: 4. Oktober 2026. **Unvollständig; keine Gesamtfreigabe.**
Verbindlich bleiben sämtliche Anforderungen der [DoD](architecture/phase-1-dod.md)
und des [Entwicklerauftrags](architecture/phase-1-developer-brief.md).
Dieser Index ordnet einzelne Varianten zu. Ein bestandener Teilfall schließt
weder eine ganze T-Zeile noch ein D-Kriterium. Historische Ergebnisse stehen im
[Arbeitsprotokoll](phase-1-acceptance-progress.md); sie gelten hier nicht
automatisch für das aktuelle Image.

Die [aktuelle Restliste für alle 17 Pflichtbereiche](phase-1-current-status.md)
ordnet den Stand von `b832d6c` einschließlich seiner verbleibenden Lücken zu.
Die weiter unten stehende ältere T01–T17-Tabelle gehört weiterhin zum
historischen Referenzimage `209278de`.

Das [Build- und Startinventar b832d6c](phase-1-build-inventory.md) ordnet
Quellcommits, Manifeste, konkrete Buildläufe und Startbefehle zu. Der erneute
Dateiabgleich umfasst 20 Factory-Images, sieben unveränderliche Profileingaben,
17 Buildbelege, 162 Kerneldateien, vier Runtime-Artefakte und 36 Helper-
Programmdateien. `build-inventory-proof.json`, SHA-256
`0a1ea5043061eff3b6aa8129af1f11af3b37278da110a2467a557a75aef5b3a7`,
bindet die lokalen Beobachtungen. Das ist kein neuer Compilerlauf; D1 und
sämtliche offenen Pflichtvarianten bleiben offen.

## Neuer ec01e5fa-Boot: misctrl, Verity, Bildschirm, Eingabe und ADB

Der spätere Socket-Prüfschritt ist nach erneuter Meldung einer
Security-Unterbrechung unvollständig beendet. Lokaler Beleg:
`out/phase1-dod/ec01e5fa-verity-base/socket-interruption/result.json`, SHA-256
`5a7a28376087ad076f13930d6559e5687071e08eb4a905ebebb8cc8bcbd97a18`.
Der eingefrorene Stand umfasst 449 Ereignisse. Ereignis 444 bestätigt lediglich
vier eigene Socket-Antworten für Alpha. Beta erhielt keinen Socket-Helfer;
gegenseitige Socket-Verbindungen wurden nicht geprüft. Der erste lange
Übertragungsbefehl war abgeschnitten worden; sein Exitcode 0 ist kein
Erfolgsnachweis. Der vollständige Helfer wurde anschließend hashgeprüft.

Der Stopp in Ereignis 445 scheitert am Prozessargumentvergleich; Ereignis 446
enthält den zusätzlichen Treiberfehler. Eine Null-Escape-Sequenz direkt vor
dem numerisch beginnenden Token wird im Perl-Regulärausdruck als Oktal-Escape
interpretiert. Dies ist mit künstlichen Zeichenketten auf dem Host reproduziert.
Ereignis 448 bestätigt die Bereinigung desselben eigenen Helfers nach genauer
Prüfung von PID, Startzeit und sämtlichen Argumenten: SIGTERM, entfernte
Bereitschafts- und Socket-Dateien sowie die Abschlussmeldung des Servers.
Die ursprüngliche Helferfassung und beide Fehlversuche bleiben erhalten.
Der neue Argumentvergleich in `scripts/runtime/ipc-socket-probe.pl` verwendet
exakte Felder statt Regex; Syntaxprüfung bestanden, erneuter Gastlauf offen.
Die lokale Protokollierung enthält keine Entscheidungsbegründung der
Security-Meldung. Ein Zusammenhang mit diesem Test oder dem Stopfehler ist
nicht belegt. T06 und D1–D7 bleiben offen.

Das Image `ec01e5fa5f2822da5763ab54c644bc5c5c5ab413` ist mit der separaten
Startvorbereitung `d9a0d30a48c1f745ffcc02dbcb6b4c15054e89e6` tatsächlich
gebootet. Das neue Profil `f8c09946-d131-40ba-8f30-3c3a4d778b0d` liegt unter
`/srv/aegis/runs/phase1-ec01e5fa-verity0/profile`; ältere Profile wurden nicht
umgebunden. Die Zustandsaufnahme vom 4. Oktober, 14:29:36 UTC bestätigt:

- Boot-ID `7017e5d9-6542-416f-9878-eabf40aa9640`, `sys.boot_completed=1`,
  SystemServer PID/Startzeit `1397/38796`; ausschließlich Systembenutzer 0.
- Authentifiziertes ADB mit `ro.adb.secure=1`, SELinux Enforcing, FBE
  `encrypted`/`file`, CE `[0]`, registrierter Identity-Service und noch nicht
  eingerichtete persönliche AEGIS-Sitzung.
- `misctrl` PID 1101 endet tatsächlich mit Status 0. Der neue Verity-Modus
  lautet `enforcing`; die acht `partition.*.verified`-Werte sind `2`.
  Der gepinnte AOSP-Code ordnet `2` dem Modus `VERITY_MODE_RESTART` zu.
- Die anschließende reine Leseprüfung bestätigt für alle acht Systempartitionen
  aktive Verity-Tabellen mit `restart_on_corruption`, Status `V` und jeweils
  den passenden schreibgeschützten Device-Mapper-Mount. Boot und SystemServer
  bleiben dabei identisch. Bootloader-Lock oder Hardware-Vertrauenswurzel
  werden nicht behauptet; die entsprechenden Boot-Eigenschaften bleiben leer.

Der neue Prüfer aus Commit `cd48514` hat 262144 erzeugte Testbytes über ADB
hin- und zurückübertragen, bytegleich verglichen und sein eigenes temporäres
Gastverzeichnis entfernt. Boot-ID, SystemServer und Profilmanifest blieben
gleich. Die QMP-Aufnahme und Android melden 720 × 1280; die angesehenen
Aufnahmen zeigen zunächst den Hintergrund mit Batterieanzeige und später
den Sperrbildschirm. Die Bildschirmaufnahme allein belegt keine Eingabe.

Die separate Eingabeprüfung öffnet zunächst die Android-Einstellungen für
Systembenutzer 0. QMP-`send-key` mit Tab und Enter öffnet tatsächlich
„Network & internet“. Danach bewegt QMP die relative Maus zum Zurück-Button;
Android meldet die Position `(57.165, 104.121)` innerhalb dessen UI-Grenzen
`[0,48][112,160]`. Ein einzelner linker Mausklick mit getrennten Down-/Up-
Ereignissen im Abstand von 200 ms führt zurück zur Einstellungs-Startseite.
UI-Bäume und angesehene Bilder bestätigen beide Übergänge. Boot-ID und
SystemServer-PID/-Startzeit bleiben unverändert.

Die erste UI-Baumaufnahme beim Tastatur-Seitenwechsel meldete einen leeren
Wurzelknoten; die nachfolgende Aufnahme derselben Seite gelang ohne erneute
Tastatureingabe. Auch das Bild fünf Sekunden nach dem Mausklick zeigte noch
die alte Seite; der folgende UI-Baum und die abschließende Aufnahme bestätigen
die Rückkehr ohne zweiten Klick. Diese Beobachtungsgrenzen bleiben erhalten;
eine Mac-/HVF- oder physische Eingabeprüfung wird nicht behauptet.

Der unveränderte Dienstprüfer zählt einen SystemServer-Start, keine seiner
sechs Fatal-/ANR-Kategorien, keine rückläufige Zeitmarke und keine Signal-
Beendigung ohne passende unmittelbar vorausgehende Steueranweisung.
Sein Ergebnis bleibt `REVIEW_REQUIRED`: `recovery-refresh=254` und
`system_aconfigd_mainline_init=1` bleiben im ursprünglichen Audit erhalten.
Die nachfolgende individuelle Quell-/Logprüfung ordnet ausschließlich den
zweiten Befund als beabsichtigte Mainline-Übergabe ein (siehe unten).
Die spätere Gastbeobachtung
findet pstore eingehängt, aber weder `pmsg-ramoops-0` noch `/dev/pmsg0`.
Das grenzt den Recovery-Befund ein, beweist jedoch nicht rückwirkend dessen
genauen Zustand beim frühen Dienstaufruf.

Der erste Host-Warteprozess für die Aufnahme endete unerwartet mit 143 und
leerem Log, bevor eine Baseline entstand. Ursache unbekannt; VM und separater
ADB-Connector liefen weiter. Nach bestätigtem Boot erfolgte die Aufnahme
erfolgreich, ohne VM-Neustart. Beim ADB-Erstzugang gelang der vorhandene
begrenzte Neuverbindungsversuch mit demselben öffentlichen Hostschlüssel;
die Authentifizierung blieb eingeschaltet. Diese Ereignisse bleiben dokumentiert.

Belege unter `out/phase1-dod/ec01e5fa-verity-base/`:

| Beleg | SHA-256 |
| --- | --- |
| `baseline.json` | `bb3763ac96cd1c8f60eb410d1f12cfde44ed1abc4ec16e07ea959b20d0c79fc9` |
| `first-boot-review/capture.json` | `06b3a0043d2d294d77fd7f445971662af841ad20f87fc43b463389b6ac143ec5` |
| `first-boot-review/service-audit.json` | `9cca25a79f52904b498725434d0ae0bbaa74c1ced9a067a520705ed439c90f94` |
| `active-verity-proof.json` | `417ae50c5963aa91f3f7b83e15a6478535904386a2c9dcc854695e6bec595dc4` |
| `display-adb/result.json` | `93355522fbe92464737e1ddeabd9f8275c277b94d5d891c3c69cce522547e2d6` |
| `ui-input/result.json` | `5f609ffd57252e82ad3d5ce691cdf093c62c4874759f5d88c8f5a37a771b3f70` |
| `capture-observer-termination.json` | `217386839b518b7e0524958f2af822acc543579cc57c2b07cea358342cd13f1d` |
| `system-storage-observation.txt` | `28f313ee3a991321229ea21c8ec4dc540b544d4df0f64dd4737f4bf4e7980916` |

**D1–D7 bleiben offen.** Vollständige Dienststabilität, Neustartverhalten
und die vollständigen persönlichen Abläufe sind auf diesem Image noch nötig.
Historische T01–T17-Ergebnisse werden nicht automatisch übernommen.

## ec01e5fa: individuelle Prüfung der aconfigd-Initialisierung

Der Befund `system_aconfigd_mainline_init`, PID 883, Status 1 ist für diesen
Boot erklärt. Das zum Vollbuild gehörende Manifest pinnt
`system/server_configurable_flags` auf
`3df6afddea58e802591487bba79b362611dafa89`. Die gespeicherten Dateien
`aconfigd/src/main.rs` und `aconfigd/aconfigd.rc` stimmen bytegleich mit diesem
Commit und dem lokalen Buildquellbaum überein. Im Zweig `MainlineInit` führt
`enable_aconfigd_from_mainline()` ausdrücklich zur Meldung
`aconfigd_mainline is enabled, skipping mainline init` und unmittelbar zu
`std::process::exit(1)`.

Genau PID 883 protokolliert diese Meldung, bevor Init ihren Status 1 meldet.
Anschließend startet `mainline_aconfigd_init` als PID 886, initialisiert die
Mainline-Flags und endet mit Status 0. Diese konkrete Folge ist damit eine
beabsichtigte Übergabe und kein ungeklärter Dienstabsturz. Andere Status-1-
Rückgaben werden dadurch nicht freigegeben; weder allgemeine Socket-Gesundheit
noch vollständige Dienststabilität ist damit nachgewiesen.

Der lokale Beleg
`out/phase1-dod/ec01e5fa-verity-base/aconfigd-init-review/result.json`, SHA-256
`6f28a15a34eb396ff9fabae9373f1795cc7cc6366286def7054b39bc5187be27`,
bindet Manifest, Quellprüfsummen, konkrete PID-/Zeilenbezüge und unveränderte
Bootlogs an Image, Profil und Boot-ID. Der ursprüngliche Dienstprüfer und sein
`REVIEW_REQUIRED`-Bericht wurden nicht geändert. `recovery-refresh=254` und
die übrigen D1-Anforderungen bleiben offen.

## ec01e5fa: erste Anmeldung und eigener Shell-Zugang nach Runtime-Stopp

Im selben neuen Profil wurde Alpha als AOSP-Administrator mit ID/Seriennummer
10/10 über `setup` angelegt. Danach blieb CE `[0]`; die Ersteinrichtung wurde
nicht als Anmeldung behandelt. Bei der ersten korrekten Anmeldung wechselte
zunächst nur das Vordergrundziel zu 10: Vor Übermittlung des Passworts blieben
Alphas CE gesperrt und sein Runtime-Kontext abwesend. Nach der Passwortprüfung
war die Sitzung auch sechs Sekunden später gültig. Der ausdrücklich angeforderte
Start und eine echte GNU-Ausführung mit UID/GID 1000 und HOME `/home/user`
bestanden ohne vorherigen Anmelde- oder Fehlversuch.

Die zehn vorgesehenen anfänglichen HOME-Verzeichnisse waren vorhanden und
hatten jeweils Eigentümer 1000:1000 sowie Modus 700. Die GNU-Shell führte die
Identitäts-, Datei- und Statusprüfungen aus und meldete glibc 2.41 und Debian
13.7; Bash, APT, Dpkg und `ls` wurden als vorhandene Befehle aufgelöst. Die
beobachtete gemeinsame Basis war schreibgeschützt; SELinux-Domäne,
Namespace-Zuordnung, leere Capabilities, NoNewPrivs und Seccomp sind erfasst.
Dies sind Einbenutzer-Teilprüfungen, keine vollständige gegenseitige Isolation.

Der gezielte Regressionsablauf bestätigt die Shell-Korrektur:

1. Eine eigene 1024-Byte-Datei wird aus GNU geschrieben und geprüft.
2. Shell-Exit kehrt zur CLI zurück. `linux stop` beendet den ursprünglichen
   Runtime-Init `6146/239891`; Prozess und Kontext-Cgroup sind unabhängig
   beobachtet verschwunden. Alpha bleibt angemeldet, im Vordergrund und
   CE-entsperrt (`[0, 10]`).
3. `shell-stopped` sendet ausschließlich den Shell-Aufruf und Statusabfragen.
   Der Aufruf wird abgewiesen; keine GNU-Shell und kein Kontext entstehen.
   CLI-Sitzung und CE bleiben unverändert. Eine zusätzliche Gastbeobachtung
   bestätigt weiterhin die fehlende Kontext-Cgroup.
4. Erst `linux start` erzeugt den neuen Runtime-Init `6745/269408`.
   Die folgende Shell öffnet regulär; GNU liest die ursprüngliche Datei
   bytegleich. Die alte temporäre Probe unter `/tmp` ist verschwunden.

Die Datei behält SHA-256
`6bf04795ef31925c4798355d6aa7e286bd588fe72f9d60837546e196de1307ac`.
Boot-ID und SystemServer `1397/38796` bleiben unverändert. Der Beleg unter
`out/phase1-dod/ec01e5fa-verity-base/shell-regression/result.json`, SHA-256
`dc2e760e0fb1541fa901c2ee3d9abdfa6dcb0ced04fd39ac0d06b55ba9a2f786`,
bindet den eingefrorenen Präfix mit 37 Treiberereignissen, vier unabhängige
Zustandsaufnahmen und den tatsächlich verwendeten Treiber-Quellhash.

Damit ist **dieser eigene Stopp-/Shell-/Neustartfall auf ec01e5fa bestanden**.
T05 insgesamt bleibt offen, insbesondere fremde/gesperrte Zielkontexte und
die weiteren Terminalfälle. Es ist kein Logout-, VM-Reboot-, Paket- oder
vollständiger IPC-Abbaunachweis. Sämtliche offenen D1–D7/T01–T17-Anforderungen
bleiben bestehen; ältere fehlgeschlagene b832d6c-Ereignisse bleiben unverändert.

## ec01e5fa: zwei persönliche Kontexte und Identität bei Benutzerwechseln

Beta wurde im selben frischen Profil über die CLI als normaler AOSP-Benutzer
11/11 angelegt; Alpha 10/10 erteilte die Adminfreigabe. Vor Betas erstem
Passwort blieben dessen CE gesperrt und Kontext abwesend. Seine erste korrekte
Anmeldung, der ausdrückliche Runtime-Start und tatsächliche GNU-Ausführung
bestanden ohne vorangegangenen Fehlversuch. Auch Betas anfängliche zehn
HOME-Verzeichnisse, GNU-Werkzeuge, schreibgeschützte Basis, Mountlayout,
SELinux-Domäne, leere Capabilities, NoNewPrivs und Seccomp sind geprüft.
APT/Dpkg wurden dabei als vorhandene Befehle aufgelöst, noch nicht als
Paketverwaltung abgenommen.

**T03 ist auf ec01e5fa zugeordnet und bestanden.** Tatsächliche GNU-Prozesse
beider Benutzer melden intern UID/GID 1000 und HOME `/home/user`. UID- und
GID-Maps sind jeweils identisch und umfassen je 1002 Kennungen:

| Benutzer | Interne Kennungen → Host-Kennungen | Runtime-Init PID/Startzeit |
| --- | --- | --- |
| Alpha 10/10 | 0–999 → 1005000–1005999; 1000 → 1007500; 65534 → 1007501 | 6745/269408 |
| Beta 11/11 | 0–999 → 1105000–1105999; 1000 → 1107500; 65534 → 1107501 | 7786/375190 |

Die vollständigen Bereiche überschneiden sich nicht und enthalten weder
Host-Root noch Host-UID/GID 1000. User-, Mount-, PID-, IPC-, UTS- und
Netzwerk-Namespaces unterscheiden sich zwischen beiden Benutzern. Beide
Kontexte verwenden dieselbe unveränderte Debian-Basis.

Beim Wechsel Alpha → Beta bleibt Alphas ursprünglicher GNU-Hintergrundprozess
`6797/343464` mit interner PID 17 erhalten. Nach Betas Shell-Exit und regulärer
AOSP-Anmeldung zurück zu Alpha bleibt Betas ursprünglicher Prozess
`9362/429323`, intern PID 60, erhalten. Positive Fortschrittszähler sowie
unveränderte Eigentümer, Cgroups, SELinux-Domänen und Namespaces belegen die
beiden Beobachtungen. Die gemeinsamen Zustandsaufnahmen vor/nach Beta → Alpha
bestätigen identische Runtime-Inits, Zuordnungen und Paketmetadaten; CE bleibt
`[0, 10, 11]`. Boot-ID `7017e5d9-6542-416f-9878-eabf40aa9640` und
SystemServer `1397/38796` bleiben gleich. Die Hintergrundproben sind begrenzt;
daraus folgt keine Zusage unbegrenzter Laufzeit.

Beide Benutzer haben aus GNU eigene 1024-Byte-Dateien sowie getrennte
synthetische Konfigurations- und flüchtige Proben angelegt. Nach Rückkehr zu
Alpha werden dessen Originaldatei und persistente Konfiguration bytegleich
gelesen. Betas Datei hat SHA-256
`c0f83e390480f013da473f2b806d2d55ac79fbce3edb4d29f1bc6d76840965d9`;
Alphas zuvor dokumentierter Dateihash bleibt unverändert.

Der lokale Nachweis `out/phase1-dod/ec01e5fa-verity-base/two-user-identity/result.json`,
SHA-256 `d5b2ebd5e4962ea153cc4714cb32260f69b0706e5a80abc5320fade20480d1d4`,
bindet 81 eingefrorene Treiberereignisse, beide Zustandsaufnahmen und die
Quellprüfsummen des Treibers, Beobachters und lokalen Auswerters.
Die Aufnahmen heißen `two-user-before-switch.json` und
`two-user-after-switch.json` mit SHA-256
`7862415068f5bc70180843bd0ac8019a3fbea3b5c69f3022f12c14340558946e` und
`8091dea20214f9681154eba834f916fbd0c9f13351b6cbcae7f6bdec3e249511`.

Dieser Nachweis schließt weder die vollständige gegenseitige Zugriffsmatrix
T06 noch Paketverwaltung, Bildschirmsperre, Ressourcenstopp, Logout oder
gepaarten VM-Neustart ab. **D1–D7 und alle übrigen offenen Pflichtfälle bleiben
erforderlich.** Testpasswörter bleiben ausschließlich im ursprünglichen
Treiberprozess; Profile und Rohprotokolle werden nicht veröffentlicht.

## ec01e5fa: gegenseitige Datei-, Prozess- und POSIX-Queue-Prüfungen

Im selben Profil bestehen die folgenden konkreten T06-Teilfälle aus den
gewöhnlichen GNU-Prozessen beider Benutzer, bei gleichzeitig entsperrtem CE
`[0, 10, 11]`:

| Prüfumfang | Beobachtetes Ergebnis in beiden Richtungen |
| --- | --- |
| Originaldatei über CE-, HOME- und Prozesswurzelpfad | Lesezugriffe abgewiesen, keine fremden Bytes; eigener Originalinhalt weiterhin bytegleich |
| Fremder Testprozess | Im persönlichen `/proc` nicht erreichbar; SIGSTOP abgewiesen; derselbe Peer mit gleicher PID/Startzeit vor und nach dem Versuch fortschreitend |
| Konfiguration, synthetische Test-Secret-Datei, `/tmp`, `/run/user/1000` | Je sechs vorhandene Peer-Pfade über Prozesswurzel beziehungsweise CE geprüft; Lesen und Anhängen abgewiesen; sämtliche ursprünglichen Peer-Bytes unverändert |
| POSIX-Mqueues | Beide Benutzer legen denselben Queue-Namen mit unterschiedlichen Nachrichten sowie eigene benannte Queues an; jeder liest seine eigene Nachricht, Öffnen der fremden Queue scheitert mit ENOENT |

Die persönlichen Proben wurden zuvor aus GNU angelegt. Entwicklungs-root
bestätigt ausschließlich Existenz, Inhalt und Prozessfortschritt der positiven
Kontrollen; die tatsächlich abgewiesenen Zugriffe stammen jeweils aus dem
anderen gewöhnlichen GNU-Kontext. Die beiden ursprünglichen Hintergrundjobs
bleiben `6797/343464` und `9362/429323`. Die abschließende unabhängige Aufnahme
bestätigt identische Runtime-Inits, Namespaces, Host-Zuordnungen, CE-Liste,
Paketmetadaten, Boot-ID und SystemServer gegenüber dem Ausgangsstand.

Der lokale Nachweis
`out/phase1-dod/ec01e5fa-verity-base/reciprocal-isolation/result.json`, SHA-256
`ba5d77e12a8df6364b7099076ee5641cbf973cae9f4fc4f050e38b456233306a`,
bindet 127 eingefrorene Treiberereignisse sowie die Prüfer- und Belegquellen.
Die Nachheraufnahme `two-user-after-isolation.json` hat SHA-256
`62d7f4d62154474b92a80259ff6c8c3351fa62012c82d885b8633f90a6218afb`.

**T06 und D5 insgesamt bleiben offen.** Private Paketstores fehlen auf diesem
Image noch und zählen nicht als geprüfte Paketisolation. Die IPC-Aussage gilt
für die ausgeführten POSIX-Queue-Fälle, nicht pauschal für jede Schnittstelle.
Logout, Schlüsselentzug, Ressourcenabbau und VM-Neustart wurden in diesem
Durchgang nicht geprüft. Sämtliche weiteren DoD-Pflichtfälle bleiben bestehen.

## ec01e5fa: erste gemeinsame Paketinstallation und Aktivierung in Alpha

Der reguläre CLI-Auftrag `linux package install --scope all jq=1.7.1-6+deb13u3`
lief unter Alpha 10/10. Der vor der Freigabe gespeicherte Plan enthält exakt
`jq` und `libjq1` in `1.7.1-6+deb13u3` sowie `libonig5` in `6.9.9-1+b1`.
Die unveränderte Quellenpolicy des Image-Commits verwendet die offiziellen
HTTPS-Quellen für Debian trixie, trixie-updates und trixie-security mit
vorgegebenem `signed-by`. Eine zusätzliche Aufnahme der laufenden Policydatei
kam erst nach Ende des Planworkers an und endete mit Status 2; sie bleibt als
Diagnose erhalten und zählt nicht als erfolgreicher Quellen-Livebeleg.

Nach frischer AOSP-Adminfreigabe wurde die gemeinsame Generation
`a1cc551eb2a79434b55e80117164256fc434873203ff382e4298304d8b0b472b`
veröffentlicht. Beide laufenden Kontexte, deren Paketdatenbanken und private
Zuordnungen blieben zunächst unverändert; Alpha meldete ausdrücklich
`packages=activation-pending`. Der eigene reguläre Stopp beendete Alphas
ursprünglichen Init `6745/269408` und Hintergrundprozess `6797/343464` und
entfernte seine Kontext-Cgroup. Alpha blieb angemeldet und CE entsperrt;
Betas ursprünglicher Prozess `9362/429323` arbeitete unverändert weiter.

Erst der ausdrückliche Neustart erzeugte Alphas neuen Init `19895/681341`
und aktivierte die veröffentlichte Generation. Aus dessen gewöhnlicher
GNU-Shell sind die exakten drei Paketversionen geprüft; `jq` verarbeitet
eine JSON-Testeingabe korrekt. Die vollständige Paketdatenbank umfasst nun
81 statt 78 installierte Pakete, mit genau den drei geplanten Ergänzungen und
keinem unvollständigen Restbestand. APT meldet 79 manuelle Pakete und genau
`libjq1`/`libonig5` als automatische Abhängigkeiten. Beta bleibt im unveränderten
78-Paket-Kontext; die gemeinsame Software wurde dort noch nicht ausgeführt.

Alphas ursprüngliche Datei und persistente Konfiguration sind bytegleich;
seine alten `/tmp`-/`/run`-Proben und beide alten POSIX-Queues fehlen im neuen
Kontext. Eine neue begrenzte Hintergrundprobe wurde erst nach bestätigtem
Ende der alten und positivem Dateireadback angelegt. Ihre Identität ist
`20262/699748`, intern PID 47. Boot, SystemServer, CE-Liste `[0, 10, 11]` und
Betas vollständiger Kontext bleiben unverändert.

Der lokale Nachweis
`out/phase1-dod/ec01e5fa-verity-base/shared-u3-activation/result.json`, SHA-256
`ecb3a978d79e8969ba4406d0ae37d8b15f7b9292a41947f748bfe5a23b0d1812`,
bindet 157 eingefrorene Treiberereignisse, den vor Freigabe gespeicherten Plan,
Worker-Beobachtungen sowie Aufnahmen vor Veröffentlichung, nach Veröffentlichung,
nach Stopp und nach Aktivierung. Die aktive Aufnahme `shared-u3-active.json`
hat SHA-256 `b33a89bd3e47bc2e6cef4875da2684ae60c0d5b0bbef40e838081b0218ac05e5`.

Damit ist **ein erlaubtes gemeinsames Installieren samt eigener Aktivierung**
belegt. Die vollständige Autorisierungsmatrix T13, Ausführung bei weiteren
Benutzern T14, private Versionen T15, Updates/Konflikte/Parallelität sowie
Logout und VM-Neustart bleiben offen. Dieser kombinierte Aktivierungsablauf
ist kein Nachweis eines unveränderten Paketbestands über einen reinen
Runtime-Neustart und keine vollständige T09-/Logout-Abnahme.

## ec01e5fa: private jq-Version für Alpha aktiviert

Alpha installiert mit einer erneuten AOSP-Adminfreigabe die persönliche Auswahl
`jq=1.7.1-6+deb13u4`. Der geprüfte Plan ersetzt ausschließlich jq und dessen
passende libjq1-Abhängigkeit von u3 durch u4; libonig5 bleibt `6.9.9-1+b1`.
Die Veröffentlichung um 16:21:58 UTC verändert weder die gemeinsame Auswahl
noch die beiden laufenden Kontexte. Alphas Status zeigt zunächst
`activation-pending`.

Der anschließende eigene Runtime-Stopp erhält Alphas Anmeldung und CE
`[0, 10, 11]`. Sein vorheriger Init `19895/681341` und Hintergrundprozess
`20262/699748` enden; Betas ursprünglicher Prozess `9362/429323` schreitet
davor und danach fort. Erst der ausdrückliche Start aktiviert den privaten
Store im neuen Alpha-Init `24589/817367`; der Status ist nun `packages=current`.

Die tatsächliche GNU-Prüfung liest jq und libjq1 jeweils als
`1.7.1-6+deb13u4` aus Dpkg und berechnet mit jq aus einem JSON-Eingabewert 41
das erwartete Ergebnis 42. Die vollständige Datenbank enthält weiterhin 81
Pakete und unterscheidet sich von Alphas vorherigem Bestand genau in diesen
beiden Versionen. APT meldet 79 manuelle Wurzeln und die beiden automatischen
Abhängigkeiten libjq1/libonig5. Die Ausgabe `jq-1.7` allein unterscheidet die
Debian-Revisionen nicht; Paketversionen und die installierte Bibliotheksdatei werden
deshalb separat erfasst. Deren SHA-256 lautet
`92012c8c198ed5f8e44042a124c3271e89a0a2867fc3344642d9ed391ef75f50`.

Der private Store liegt in Alphas CE-Bereich, ist an `user 10 10` gebunden und
referenziert die unveränderte gemeinsame Generation. Die ursprüngliche
1024-Byte-Datei und beide persistenten Konfigurationsproben sind bytegleich.
Eine neue, zeitlich begrenzte Hintergrundprobe erhält eine eigene Identität;
sie wird nicht als Überleben des beendeten Prozesses gewertet. Die abschließende
Zustandsaufnahme bestätigt Betas vollständig unveränderten Kontext und Bestand
sowie unveränderte Boot-ID, SystemServer-Identität und CE-Zustände.

Der Offline-Prüfer bindet 185 eingefrorene Ereignisse, darunter den identischen
vorherigen 157-Ereignis-Stand, an die Originaldateien unter
`out/phase1-dod/ec01e5fa-verity-base/`:

| Beleg | SHA-256 |
| --- | --- |
| `private-u4-activation/result.json` | `a246770a89bf662a9e5feebc4af2f793185f7c82962d3f52872d29b49d2d3e8a` |
| `private-u4-published.json` | `16b2ac738fb233ecbc5c473927054a3bb9fffeee4d343e1c37b3c222b031f898` |
| `private-u4-alpha-stopped.json` | `b26189ec121a7b99c9bdb20bef75468fcff57bb22d6ab96bc23021ae6673bdb4` |
| `private-u4-active.json` | `09dba2c56ed9a686df7b581cc897b6464f50c9e89911de32066a8a26c6f756d6` |

Beta führt hier weiterhin die ursprüngliche Basis ohne jq aus. Eine
gleichzeitige Ausführung von u3 bei Beta und u4 bei Alpha ist noch nicht belegt.
Alphas ursprüngliche flüchtige Dateien und Queues waren bereits beim vorherigen
gemeinsamen Aktivierungstest entfernt worden; dieser Fall behauptet keinen
erneuten Nachweis ihrer Bereinigung. Private Paketzugriffe, ungültige Versionen,
Entfernung, Updates, Logout und VM-Neustart bleiben erforderlich. T15 insgesamt
und alle D1–D7 bleiben offen.

## ec01e5fa: Beta führt gemeinsame u3 neben Alphas privater u4 aus

Nach erneuter AOSP-Anmeldung als Beta 11/11 meldet dessen noch ursprünglicher
Kontext `packages=activation-pending`. Der eigene reguläre Stopp beendet
Init `7786/375190` und Hintergrundprozess `9362/429323`; eine separate
Leseprüfung bestätigt das Ende des alten Init und der Kontext-Cgroup.
Betas Anmeldung, Vordergrund und CE `[0, 10, 11]` bleiben erhalten. Alphas
Prozess `24890/832130` schreitet vor und nach diesem Stopp unter derselben
Identität und Namespace-Zuordnung fort.

Betas ausdrücklicher Neustart erzeugt Init `28080/916284` und meldet
`packages=current`. Die tatsächliche GNU-Prüfung bestätigt jq/libjq1
`1.7.1-6+deb13u3`, libonig5 `6.9.9-1+b1` und die korrekte JSON-Berechnung
41 → 42. Der vollständige Bestand enthält exakt die vorherigen 78 Pakete
plus diese drei Ergänzungen: 79 manuelle Pakete, zwei automatische
Abhängigkeiten und keine unvollständigen Restpakete. Paketdatenbank und
Backing-Datei stimmen mit der zuvor unter Alpha geprüften gemeinsamen
Generation überein; Beta besitzt keine private Auswahl.

Alphas privater u4-Kontext bleibt in der abschließenden Zustandsaufnahme
vollständig unverändert, einschließlich Init, Namespaces, Paketdatenbank,
CE-Backing-Datei und privater Versionsauswahl. Gemeinsame und private
Generationszuordnungen, Boot-ID, SystemServer und CE-Zustände bleiben gleich.
Die beiden installierten libjq-Dateien unterscheiden sich auch tatsächlich:
u3 hat SHA-256 `58a6c82e3cc0b55f2e11e85ffa487bd2381c4cd30068874504c639c76a3e59d6`,
u4 `92012c8c198ed5f8e44042a124c3271e89a0a2867fc3344642d9ed391ef75f50`.
Die GNU-Aufrufe erfolgten nacheinander in den gleichzeitig bestehenden
persönlichen Kontexten; ein gemeinsamer Programmname oder Versionsbanner
allein dient nicht als Versionsnachweis.

Betas ursprüngliche 1024-Byte-Datei und persistente Konfigurationsproben sind
bytegleich. Seine vor dem Stopp nachweislich vorhandenen `/tmp`-/`/run`-Proben
und beide alten POSIX-Queue-Namen fehlen im neuen Kontext. Der neue begrenzte
Hintergrundprozess `28430/933379`, intern PID 51, wird ausdrücklich als neue
Probe erfasst und macht messbaren Fortschritt.

Belege unter `out/phase1-dod/ec01e5fa-verity-base/`:

| Beleg | SHA-256 |
| --- | --- |
| `both-versions/result.json` | `9bb954b5e7cb81cc3ea1c6aa0454bdabcb2f74496a40fd1dcec1c5fb4811aeb2` |
| `both-versions-active.json` | `9f55e48f1b8b55e8d45f52283404638ce65d6e9146993a2db97fd89e85b9981f` |
| `shared-u3-beta-stopped.json` | `de87f67fbbda8f3fe1f62f3ce489edd68463e2608aa98b891985ddc7ff576b02` |

Der Offline-Abgleich bindet 213 eingefrorene Ereignisse einschließlich des
unveränderten vorherigen 185-Ereignis-Stands. Dieser gültige Versionsfall
schließt T15 nicht ab: nicht verfügbare/unauflösbare Versionen und private
Entfernung bleiben erforderlich. Private Paketzugriffe, vollständiger
Ressourcenabbau, Logout, VM-Neustart und die übrige Matrix bleiben offen.
Alle D1–D7 bleiben offen.

## ec01e5fa: Bildschirmsperre und direkter Beta-Logout

Bei offener Beta-GNU-Shell widerruft die angeforderte Bildschirmsperre den
Terminalkanal. Die CLI meldet `terminal=unauthenticated`; `linux start` und
`linux shell` werden abgewiesen. Beide ursprünglichen Hintergrundprozesse
schreiten bei `mWakefulness=Asleep` mit unveränderten Identitäten und
Namespaces fort. Die vollständige Zustandsaufnahme bestätigt unveränderte
Kontexte, Paketbestände, Generationszuordnungen, Boot-ID, SystemServer und
CE `[0, 10, 11]`. Die Bildschirmsperre wird ausdrücklich nicht als
CE-Schlüsselentzug gewertet.

Nach dem Aufwecken bleiben Terminalstatus und beide Zugangsablehnungen gleich.
Erst Betas frische AOSP-Anmeldung erlaubt wieder eine GNU-Shell im bestehenden
Kontext, ohne `linux start`. Originaldatei und persistente Konfiguration sind
bytegleich. Nach Shell-Exit ist derselbe Beta-Prozess `28430/933379`, intern
PID 51, weiterhin aktiv und Android meldet `Awake`.

Der direkte `logout` ohne vorherigen Runtime-Stopp meldet am 4. Oktober um
16:58:02 UTC bestätigten Benutzerstopp, CE-Sperrung und Runtime-Freigabe.
Unabhängige Leseprüfungen bestätigen das Ende dieses ursprünglichen Prozesses,
des Init `28080/916284` und der Kontext-Cgroup. AOSP meldet CE `[0, 10]` und
Vordergrundbenutzer 0. Genau die zuvor aus GNU gelesene Originaldatei ist
nicht mehr lesbar: Exitcode 1, null Ausgabebytes und `ENOENT` beim bekannten
CE-Pfad. Eine spätere positive Rücklesung bleibt erforderlich, um den
vollständigen Persistenzfall abzuschließen.

Alphas ursprünglicher Prozess `24890/832130` schreitet danach weiter fort;
die abschließende Aufnahme bestätigt seinen vollständig unveränderten
privaten u4-Kontext und unveränderte gemeinsame/private Zuordnungen. Boot-ID
und SystemServer `1397/38796` bleiben gleich. Betas gesperrte Paketmetadaten
werden in dieser Aufnahme nicht gelesen.

Belege unter `out/phase1-dod/ec01e5fa-verity-base/`:

| Beleg | SHA-256 |
| --- | --- |
| `screen-beta-logout/result.json` | `cc6bf2493c3f6aafe7ab0897bb0042729564073cea4908d12658079f1f5b6546` |
| `screen-locked-contexts.json` | `b0f89822f5baccc42e1600ec91fae1e8ddff8a9fb61217dba51c388f35d86edc` |
| `beta-logout-state.json` | `c38c6383531b69274f8550cfbeac00057d4455425cfb8b98c2556248915fb16d` |
| `beta-logged-out-contexts.json` | `1a85a4edc6a166fd62683713097866931754e75385bde158ac1c56aece5c2258` |

Der Offline-Prüfer bindet 239 eingefrorene Ereignisse einschließlich des
identischen vorherigen 213-Ereignis-Stands. Belegt sind die Bildschirmsperr-
Variante und dieser direkte Beta-Logout. Vollständiger Abbau offener Zugriffe,
Mounts und IPC, Konkurrenzfälle, AOSP-Ressourcenstopp sowie erneute Anmeldung
und gepaarter VM-Neustart bleiben erforderlich. T08/T10 insgesamt und alle
D1–D7 bleiben offen.

## ec01e5fa: beide Benutzer abgemeldet, Profilpaar beendet und zweiter Lauf gestartet

Alpha meldet sich nach Betas Logout erneut über AOSP an. Seine ursprüngliche
Datei und persistente Konfiguration sind bytegleich; jq/libjq1 bleiben privat
auf `1.7.1-6+deb13u4`, mit unveränderter libjq-Prüfsumme und erfolgreicher
JSON-Berechnung. Nach Shell-Exit schreitet sein ursprünglicher Prozess
`24890/832130` noch fort. Der direkte Logout ohne vorherigen Runtime-Stopp
meldet am 4. Oktober um 17:06:08 UTC Benutzerstopp und CE-Sperrung.

Unabhängige Prüfungen bestätigen das Ende dieses Prozesses und des Init
`24589/817367`. Beide persönlichen Kontexte fehlen, AOSP meldet nur noch
Systembenutzer 0 als gestartet und CE `[0]`; beide zuvor tatsächlich gelesenen
Originaldateien liefern keine Bytes. Der ursprüngliche Testtreiber bleibt
bestehen und friert den Neustart-Prüfpunkt mit denselben Benutzeridentitäten
und Originaldatei-Prüfsummen ein. Persönliche Testpasswörter bleiben
ausschließlich in seinem Prozessspeicher.

Android erhält anschließend den regulären Shutdown-Auftrag. Sein Log bestätigt
`Power down`; der zugehörige KeyMint-Helfer beendet sich mit `clean`.
Der Launcher-Dienst endet erfolgreich, und alle drei ursprünglichen
Host-Prozessidentitäten sind verschwunden. Unter der Profil-Sperre werden
Manifestbindung, QCOW2-Profilmarkierung, Backing-Disk und Helper-Dateisystem-UUID
geprüft. Beide Disk-Dateien behalten ihre Dateizuordnung; `qemu-img check`
ohne Reparaturoption besteht. Die gestoppten Disk-Prüfsummen bleiben lokal.

Erst danach startet `aegis-qemu-ec01e5fa-verity-boot2.service` dasselbe
Profil `f8c09946-d131-40ba-8f30-3c3a4d778b0d`, mit denselben Image-/Helper-
Eingaben und ohne `--create-profile`. **Dieser Beleg bestätigt den sauberen
Stopp und den erneuten Startauftrag; er bestätigt noch keinen abgeschlossenen
zweiten Boot oder persönlichen Datenzugriff danach.**

Belege unter `out/phase1-dod/ec01e5fa-verity-base/`:

| Beleg | SHA-256 |
| --- | --- |
| `paired-restart/result.json` | `6e0480e0cc10cb44878b71c234d75b7a8c970cbdc12ed76b08b11903f046b2f8` |
| `pre-shutdown-state.json` | `9490e550e9e6e5b1a14f8372f79bd93225cf4c5742732570db817051e1e428e6` |
| `paired-shutdown.json` | `45dd0e6f0de0c310fc2459d8026f79f7207e2e0343d4848e079a17da0e85be91` |
| `boot2-launch.json` | `dfdd3b1afead8f8d48bb5634018ff106395cbc464eefaabbe092f60391f2f283` |

Der Offline-Abgleich bindet 259 eingefrorene Ereignisse und den identischen
vorherigen 239-Ereignis-Stand. Der zusätzliche vollständige Android-Logaudit
mit spätem Logcat-Ausschnitt bleibt `REVIEW_REQUIRED`; seine SHA-256 lautet
`1e971e2dbffb6236f99695ebb6f7e198c4de9093e4036eb42c6c65e861878fe2`.
Neben den bekannten Bootbefunden enthält er Shutdown-Signalbeendigungen,
`mdnsd=4` während des Shutdowns und eine rückläufige Zeitmarke. Der späte
Logcat-Ausschnitt enthält keine frühe SystemServer-Startmeldung; die
identische PID/Startzeit `1397/38796` wurde unmittelbar vor dem Shutdown
separat bestätigt. Keine dieser Auditmeldungen wird pauschal freigegeben.
Bootabschluss, CE-Sperre vor Anmeldung und Originaldaten-/Versionsrücklesung
bleiben ebenso erforderlich wie die übrige Matrix und alle D1–D7.

### Ergänzende individuelle Shutdown-Prüfung dieses ersten Boots

Der Offline-Audit erfasst zusätzlich die Zuordnung zum Shutdown: Dienstname,
PID und Startgeneration, vorheriges tatsächlich gesendetes Signal sowie
Reihenfolge und Zeitmarken. Diese Zusatzinformation unterdrückt keinen
ursprünglichen Befund. Alle 103 Signalbeendigungen und drei nichtnull
Rückgaben bleiben erhalten. Für sämtliche 78 bisher nicht durch einen
kurzen Einzelstopp erklärten Signalbeendigungen liegt nun die passende
Shutdown-Zuordnung vor; der größte Abstand zwischen Signal und protokolliertem
Prozessende beträgt 6,242057 Sekunden. Die 14 Parsertests prüfen unter anderem
falsche PID/Signale, fehlende Starts, veraltete Zuordnungen, PID-Wiederverwendung
und die unverändert erforderliche Prüfung nichtnull Rückgaben.

Für `mdnsd` PID 1139 folgen auf den Shutdown bei 10639,428324 Sekunden
SIGTERM bei 10640,528488 und Status 4 bei 10645,692674. Die Quellen sind
bytegleich mit dem Buildmanifest: `external/mdnsresponder` auf
`6bebf38f17204fb543e41e8fd7c6444e72314ee5`, `system/core` auf
`68be0c2c0006a0740d0b1809abe4717308f90d15` und Bionic auf
`09a271af557444c9a6b3f3146d6d474156fd6cdb`.
`PosixDaemon.c` beendet die Hauptschleife bei SIGTERM/SIGINT mit `EINTR`;
`main` gibt diesen Wert nach der Bereinigung zurück. Bionic definiert ihn
als 4. Dies erklärt diese einzelne langlebige Instanz nach regulärem
Shutdown; andere Rückgaben von `mdnsd` werden dadurch nicht freigegeben.

Die Zeitmarkenabweichung betrifft zwei Mikrosekunden zwischen `apexd`
Thread 5954 und init Thread 1, also die Reihenfolge verschiedener Threads
im gemeinsamen Log. Sie allein belegt keinen zurückgesetzten Gastzeitgeber.
Der frühe Logcat-Auszug ist außerdem ein bytegleicher Präfix der größeren
Bootaufnahme. Diese enthält genau einen SystemServer-Start für PID 1397;
Boot-ID und Prozessstartzeit 38796 stimmen zwischen Anfang und unmittelbar
vor dem Shutdown überein. Keine der sechs erfassten Crash-/ANR-Kategorien
hat Treffer. Das ist kein Nachweis, dass jede mögliche Logmeldung erfasst wurde.

Zusätzliche Belege unter `out/phase1-dod/ec01e5fa-verity-base/`:

| Beleg | SHA-256 |
| --- | --- |
| `boot1-shutdown-correlated-service-audit.json` | `0d8bd6aff7ce45efe45b4210bb94f95750d9f05028384c6447ac828b15b84375` |
| `boot1-complete-capture-service-audit.json` | `a05667d6c6bd8370b67adb736539c093f1818facf87fbece09025300be9a2edd` |
| `shutdown-source-review/result.json` | `c5b4058960345a00f389f2485dde319ed25540e6fbaf7ca4be69496417cf1365` |
| `shutdown-source-review/system-server-continuity.json` | `b68e658bc7c442ca9a8a3b914ddea63bb424e7d438e7add19a2de0fec0163801` |

`recovery-refresh=254` bleibt ungeklärt: Sein Leseweg unterscheidet im
Rückgabewert nicht zwischen fehlenden passenden Daten und bestimmten
Lesefehlern. Der nächste D1-Schritt ist eine Fehlerdiagnose, die diese
Ursachen getrennt sichtbar macht. Die ursprünglichen Audits, D1 und die
Gesamtfreigabe bleiben offen. Diese Auswertung führte keine Gastaktion aus.

Die dafür ergänzte Quelle `scripts/aosp/register-pmsg-diagnostics.py` bindet
`system/logging` auf `d78b713380007d3c0dde14712cbcbec27f491ad9` und den
ursprünglichen Parser-SHA-256
`15c3239d82783d248bf6f229203efb25b1a47d62213f0b9a963e68511119cc54`.
Der Build installiert und verifiziert den Hook; die Vorbereitung verlangt
zusätzlich `pmsg-diagnostics-source.json`. Nicht verwaltete Quelländerungen
und symbolische Links werden nicht überschrieben.
Die neue Meldung `liblog-pmsg: pmsg file read: terminal=… aggregate=… result=…`
enthält ausschließlich numerische Ergebnisse, keine Dateinamen oder Inhalte.
Der bisherige API-Rückgabewert bleibt unverändert. Insbesondere wird weder
Status 254 pauschal akzeptiert noch ein bereits aggregiertes Ergebnis als
Beweis fehlerfreien Lesens behandelt.

Sieben Hosttests kompilieren den gepinnten Rekonstruktionsparser vor und nach
der Änderung. Nur Geräteleser und Ausgabesenke sind durch künstliche Daten
ersetzt: EOF/EAGAIN, ENOENT, EACCES, EIO, EBADF, passende und ausgefilterte
Einträge sowie Callback-Fehler werden geprüft. Eine zunächst falsch gezählte
Länge des synthetischen Texts wurde im Test korrigiert; beide Parser lieferten
dabei bereits identische Daten. Kein pstore-Gerät wird im Hosttest geöffnet.
Die 32 AOSP-Worker-Tests bestehen ebenfalls. Eine tatsächliche Android-
Kompilierung ist inzwischen erfolgt; die Diagnose beim Booten bleibt erforderlich.

Der lokale Diagnosebuild aus `cacbf0d4ccf400155254ed84d0045244a1e4f6a2`
liegt unter `/srv/aegis/runs/local-20261004T193742Z-cacbf0d4-NntCkc` und
endet mit `LOCAL_BUILD_VERIFIED`, Exitcode 0. AOSP kompiliert den geänderten
Parser für ARM64, prüft die ABI und erzeugt die abhängigen Images. Die
Vorbereitung `/srv/aegis/runs/phase1-cacbf0d0` bestätigt 20 Images und
19 Buildbelege einschließlich der neuen Diagnosequelle. AVB-Flags sind null,
der Bootparameter setzt Verity auf `enforcing`; dies ist vorerst ein
statischer Nachweis. Der neue AVB-Digest lautet
`3303e560127bf33fe350736ff3318a22e04485741a8f274a8a5dc35e2f2fd0ef`.

Die aus `system_a` des ausgelieferten `super.img` gelesene ARM64-Bibliothek
stimmt bytegenau mit dem Buildprodukt überein und enthält die neue Meldung.
Ihre SHA-256 ist
`e24ecee948acce14d4d278af11b85f0cd66394461839c394e84c015e013bea40`.
Auch Recovery-Binärdatei und Init-Konfiguration sind bytegleich geprüft;
die Binärdatei importiert `__android_log_pmsg_file_read` aus `liblog.so`.
Ein anfänglicher Zugriff auf die Verifierdatei scheiterte an der fehlenden
Verzeichnistraversierung des Buildkontos, bevor eine Imageprüfung begann.
Danach wurde nur der nicht vertrauliche Verifierquelltext temporär lesbar
bereitgestellt; die Originalverzeichnisrechte blieben unverändert.

Belege unter `out/phase1-dod/pmsg-cacbf0d/`:

| Beleg | SHA-256 |
| --- | --- |
| `build-validation.json` | `957bef1b9850d67daa57991fb853622e83b6b4e5d154182faf864459b1d63b80` |
| `avb-checked.json` | `2e377fb9ee6752522d9392a7929ab1add762c3ea523e3280f0986eb908c71cb1` |
| `pmsg-packaged-image-proof.json` | `aad07a173217b06ad520dcc8ef0426cd40cf62dc2deffe9d1d4a5360fe7f44d8` |

Der überholte dedd1da-Gast mit bestätigt ausschließlich Systembenutzer 0
wurde zuvor sauber samt KeyMint-Helfer heruntergefahren; Profilidentität
und Datenträger blieben erhalten. Der ec01e5fa-Benutzerlauf blieb aktiv.
Am 4. Oktober um 19:54:43 UTC wurde der separate cacbf0d-Diagnosegast
mit frischem Profil `de23866c-db95-46ed-8bbb-748c399262ca`, Netzwerk `none`
und lokalem ADB-Endpunkt `127.0.0.1:15879` gestartet. Der Helfer meldet
Bereitschaft. Der erste ursprüngliche Recovery-Aufruf als PID 404 endet
bei Bootzeit 39,707052 Sekunden mit 254. Der Bootabschluss ist inzwischen
aufgenommen: Boot-ID `f357b68f-d5ea-4168-806e-c55037daf501`, SystemServer
`1299/41148`, ausschließlich Benutzer 0 und CE `[0]`, authentifiziertes ADB,
SELinux Enforcing und dateibasierte Verschlüsselung. Acht Verity-Properties
haben Wert `2`; aktive Device-Mapper-Tabellen sind für dieses Image damit
noch nicht nachgewiesen.

Der ursprüngliche PID-404-Aufruf meldet `terminal=-2`,
`aggregate=9223372036854775807`, `result=-2`. Der unveränderte Aggregatwert
und die gepinnten Pstore-Lese-/Seekpfade grenzen dies auf einen fehlenden
Quellpfad beim Öffnen ein; dies ist eine Quelleninferenz, keine aufgezeichnete
Syscall-Beobachtung. Später im selben Boot ist pstore eingehängt und leer,
`pmsg-ramoops-0` und `/dev/pmsg0` fehlen. Die Gastbibliothek stimmt mit der
geprüften Imagebibliothek überein. Es wurde kein Dienst erneut ausgeführt.
Diese Erklärung gilt für diesen Aufruf und beweist keine historische Ursache
des älteren ec01e5fa-Aufrufs.

Die zwei weiteren Rückgaben sind separat erklärt: Aconfigd PID 776 kündigt
die beabsichtigte Mainline-Übergabe an; Mainline PID 780 endet mit 0.
`rename_eth0` PID 836 liefert laut gepinnter Quelle nur beim fehlenden
Interface den beobachteten Wert -2/254; dieser Gast läuft ausdrücklich mit
Netzwerk `none`. Der ursprüngliche Audit bleibt unverändert
`REVIEW_REQUIRED`, mit drei Rückgaben, einem SystemServer-Start und ohne
Treffer seiner sechs Fatal-/ANR-Marker. Das ist keine pauschale Freigabe.

Der erste ADB-Warteprozess lief vor dem Bootabschluss in sein Zeitlimit;
der Gast lief weiter. Nach bestätigtem Bootabschluss gelang die reguläre
Autorisierung beim zweiten Transportversuch. Eine anschließende Übertragung
von 262144 Testbytes wurde bytegleich zurückgelesen und die eigene temporäre
Datei entfernt. Die QMP-Aufnahme liefert 720 × 1280 Pixel bei unveränderter
Boot-, Profil- und SystemServer-Identität. Dieser Beleg umfasst weder
Tastatur/Maus noch persönliche Benutzer oder Neustartpersistenz.

Zusätzliche lokale Belege unter `out/phase1-dod/pmsg-cacbf0d/`:

| Beleg | SHA-256 |
| --- | --- |
| `boot-observation.json` | `a0e955adc4c1b93678bea36d4ec414d813b9d0460d0a46b82e8d42ad3db2880b` |
| `first-boot-service-audit.json` | `1eb36aa732d8013caef181fb5646ccc88ca9faa1d63d16bcf8c36bf598e79be2` |
| `individual-service-review/result.json` | `53f9061d99d9cc0828378e5d2c246c1b5f2ca12ffd22df7c8c9bef839dc3a03d` |
| `display-adb/result.json` | `d1f650945ecb0ad39534b1d50a25128b64c1c816b03c2a738dd8f4e07808155c` |

Ein anschließender Lauf von `scripts/qemu-verity-test.py` ordnet jede der acht
Systempartitionen über ihren tatsächlichen Mount und Sysfs-Gerätenamen einem
aktiven Verity-Mapping zu. Alle acht liefern Status `V`, enthalten
`restart_on_corruption` und sind als EROFS schreibgeschützt eingehängt.
Boot-ID, SystemServer-Startzeit, Profilmanifest und Mountinventar bleiben
während der Beobachtung unverändert. Der ausschließlich lesende Test erfasst
keine Userdata-Verschlüsselungstabellen und führt keine Beschädigungstests aus.
Beleg: `active-verity/result.json`, SHA-256
`0d6cfaaec3dd384a140c66b66da4717ff0a728cb88fcdf09daa272cbd7174f99`.
Reproduktion auf dem noch laufenden Diagnosegast, mit neuem Ausgabeverzeichnis:

```sh
python3 scripts/qemu-verity-test.py \
  --run out/phase1-dod/pmsg-cacbf0d/boot-1 \
  --prepared /srv/aegis/runs/phase1-cacbf0d0 \
  --commit cacbf0d4ccf400155254ed84d0045244a1e4f6a2 \
  --output out/phase1-dod/pmsg-cacbf0d/active-verity-repeat
```

Die anschließende QEMU-Bedienungsprüfung auf demselben Systembenutzer 0 ist
ebenfalls bestanden. Nach Wecken, Öffnen der Einstellungen und normaler
Wischgeste öffnet einmaliges QMP `send-key` mit `tab`, dann `ret` (je 120 ms)
die Seite „Network & internet“. Ein relativer Mauszug zum Ursprung und
anschließend `(56,104)` landet wegen Mausbeschleunigung zunächst außerhalb
des Zurück-Pfeils. Nach Korrektur `(-28,-53)` bestätigt Android die Position
`(57.165,104.121)` innerhalb der aus der UI-Struktur gelesenen Grenzen
`[0,48][112,160]`. Ein einzelnes linkes Drücken/Loslassen über
`input-send-event` führt zur Einstellungen-Startseite zurück.

Die Seitenwechsel sind sowohl in den UI-Strukturen als auch in visuell
geprüften Screenshots bestätigt. QMP-Aktionen, Positionen und UI-Aufnahmen
liegen unter `ui-input/`; temporäre UI-Dateien im Gast wurden entfernt.
Die erste Aufnahme zeigte noch den Sperrbildschirm. Der erste UI-Dump nach
der Wischgeste meldete trotz Exitcode 0 einen fehlenden Root-Knoten; dieser
Zwischenbefund ist erhalten. Die spätere reine Beobachtung gelang ohne
Wiederholung der Wischgeste. Boot-ID und SystemServer einschließlich Startzeit
blieben unverändert. Keine persönliche Anmeldung ist Bestandteil dieses Tests.

Der Audit der anschließend eingefrorenen Logs enthält dieselben drei bereits
einzeln geprüften Dienst-Rückgaben, genau einen SystemServer-Start und keine
Treffer seiner sechs Fatal-/ANR-Marker. Beide früheren Logaufnahmen sind
bytegleiche Präfixe der späteren Aufnahme. Der Audit bleibt ausdrücklich
`REVIEW_REQUIRED`; dieser begrenzte Zeitraum ersetzt nicht den vollständigen
Referenzablauf oder einen Nachweis lückenloser Protokollierung.

| Zusätzlicher Beleg | SHA-256 |
| --- | --- |
| `ui-input/result.json` | `2146fdf11324edff93dbaee91f9d2ee525bae3f50d4eb81d665a8d5162cccfe1` |
| `post-ui-service-audit.json` | `17f4750da7410f801901f8f5d03c919bad23c7707121bcf734faac95ddab5958` |
| `post-ui-continuity.json` | `6c34f6ac30e237e239e42ffc18a236c15159e488caae56db78d1015c5b5e766a` |

D1 und die vollständige DoD bleiben offen. Die erneute vom Benutzer gemeldete
Plattform-Sicherheitswarnung hat keinen belegten Auslöser in diesen Belegen.
Der zuvor unterbrochene Socket-Test wird nicht automatisch wiederholt;
unvollständige Nachweise bleiben offen und Schutzmechanismen unverändert.

## cacbf0d: Zwei erste Anmeldungen und persönliche GNU-Kontexte

Auf demselben zuvor ausschließlich mit Systembenutzer 0 aufgenommenen Profil
`de23866c-db95-46ed-8bbb-748c399262ca` ist ein neuer, separater Testtreiber
gestartet. Image bleibt `cacbf0d4ccf400155254ed84d0045244a1e4f6a2`, Boot-ID
`f357b68f-d5ea-4168-806e-c55037daf501`, SystemServer `1299/41148`.
Der ältere ec01e5fa-Lauf wurde dafür nicht verändert. Der Treiberquelltext
hat SHA-256 `6c2325afc20f94cd9f4a2c99f646ff38ae46d46068f1378d931c4c8bf7daf074`.

Die echte CLI legt Alpha `10/10` als ersten Administrator und mit erneuter
AOSP-Adminfreigabe Beta `11/11` als normalen Benutzer an. Nach jeder Anlage
bleibt der betreffende CE-Speicher gesperrt. Bei beiden ersten Anmeldungen
ist noch vor der Passwortübermittlung getrennt beobachtet: Ziel im Vordergrund,
CE weiterhin gesperrt, kein GNU-Kontext. Danach gelingen jeweils der erste
korrekte Login ohne vorherigen Fehlversuch, verzögerte Sitzungsprüfung,
ausdrücklicher Runtime-Start und tatsächliche GNU-Ausführung.

Beide führen Debian 13.7/glibc 2.41 mit bash, apt, dpkg und GNU-Werkzeugen aus.
Die anfänglichen zehn HOME-Verzeichnisse gehören intern `1000:1000` und haben
Modus 0700. Die Basis ist schreibgeschützt; Mountsicht, SELinux-Domäne,
leere Capability-Sätze, `NoNewPrivs=1` und Seccomp sind tatsächlich geprüft.
Die Benutzerlisten zeigen Alpha als Administrator und Beta ohne Adminrolle.

Bei CE `[0,10,11]` besitzen beide intern UID/GID 1000, aber je 1002 disjunkte
Host-IDs ohne Host-ID 0 oder 1000 und sechs verschiedene Namespaces.
Alphas Kontext `6156/301392` bleibt nach seinem Shell-Ende und dem Wechsel
zu Beta unverändert; Betas Kontext lautet `8046/357440`. Der Shell-Exitcode 7
beendet die Shell, während Alphas AOSP-Sitzung und CE erhalten bleiben.
Alphas ursprünglicher Testprozess `6544/311909` macht auch mit Beta im
Vordergrund Fortschritt (erste Aufnahme 16→20, spätere 655→659).

Die aus den eigenen GNU-Shells erzeugten 1024-Byte-Dateien sind verschieden:
Alpha SHA-256 `19a41a7777f8ddf48c06c3b9eb633638029fc3a8223b42d7dd33f07238f63827`,
Beta `5dfe1ebda013056b5c66d4bc53ff1c446386cde09e9ebbb6ccff79e3e5332104`.
Separate persönliche Konfigurationen und flüchtige Testdateien sind angelegt.
Diese ursprünglichen Daten sind Ausgangspunkte künftiger Persistenzprüfungen;
ihre Erzeugung allein beweist noch keinen Dateierhalt nach Neustart.

Ereignis 57: Die erste zusätzliche Beta-Hintergrundbeobachtung überschritt
ein Zeitlimit. Der Treiber nennt nur `TimeoutExpired`; genauer Unterbefehl
und Ursache sind unbekannt. Dieser Befund ist mit dem ursprünglichen
58-Ereignis-Präfix eingefroren und zählt nicht als bestandene Messung.
Nach Ende der parallelen Zustandsaufnahme gelingt die reine Beobachtung
des bereits vorhandenen Beta-Prozesses `8545/369467`, Fortschritt 194→197.
Kein Prozess-, Treiber- oder VM-Neustart und keine Zeitlimitänderung erfolgten.
Die zeitliche Abfolge beweist keine Ursache des ersten Timeouts.

Die Offline-Auswertung friert 61 Ereignisse ein und prüft GNU-Erfolgsmarker,
Image-/Profilbindung, Zuordnungen und Prozesskontinuität. Eine separate
Fortschreibung der Dienstlogs enthält dieselben drei bereits einzeln
erklärten Rückgaben, einen SystemServer-Start und keine Treffer der sechs
Fatal-/ANR-Marker. Frühere Logaufnahmen sind bytegleiche Präfixe. Ein Scan
findet keines der vollständigen generierten Testpasswörter in drei lokalen
Bootlogs; dies ersetzt keinen vollständigen T01-Offenlegungsnachweis.

Belege unter `out/phase1-dod/pmsg-cacbf0d/`:

| Beleg | SHA-256 |
| --- | --- |
| `initial-users/result.json` | `b774cf0bbcf90db872ef6afd198cc42ef73020e1d82015d778888f06a2d2c018` |
| `initial-users/events.json` | `bcceae22f651d5fe85a4979781d6cb7ae04f9edb17ed284ca8dcaeb81495fc2b` |
| `alpha-initial-state.json` | `c3549976ad28c1c1029c726e2512c594fe6953816d19632d63a6ffd5869ec687` |
| `both-initial-state.json` | `5b98c08e5dbc8d95698ed29fa8d612f5ac747935a959916b1e761c7e58b8f3d8` |
| `beta-observer-timeout/result.json` | `74a950b1b386c3f8cf0024f5c87cf31b038ff6eae56dce25fb87c5564f3905b7` |
| `beta-observer-timeout/later-observation.json` | `dd2792ad972354a0c88e34e9f0f6a294d4a600d59c3677508d63d439dc3380eb` |
| `initial-users/system-continuity.json` | `19db6132f8c80cfbbfcb365027c605ad7a92890fdfee56e9c4c0782f17a506f0` |
| `initial-users/service-audit.json` | `06a2d77565310f72e5a00284d43b07fd5f4a0335a9e4e7460cb758290be98cd9` |

Die Befehlsfolge entspricht den dokumentierten Erstzugängen in
[runtime-gnu-test-driver.md](runtime-gnu-test-driver.md), ergänzt um die
festgehaltenen persönlichen Dateiproben und Zustandsaufnahmen. Der lokale
Offline-Verifier `record-initial-users.py` ist im Ergebnis mit Prüfsumme gebunden.
Zum Zeitpunkt dieses Erstzugangsbelegs stehen T03 einschließlich Rückwechsel
und die übrigen T01–T17-Varianten noch aus. Der anschließende Rückwechsel ist
nun separat geprüft: Beide vollständigen Kontextaufnahmen sind bis auf den
Vordergrundbenutzer identisch; CE bleibt `[0,10,11]`. Alpha liest seine
ursprüngliche Datei und Konfiguration bytegleich und bestätigt UID/GID 1000
sowie dieselben Maps und Namespaces erneut aus GNU. Betas ursprünglicher
Prozess `8545/369467` bleibt unter Alpha im Vordergrund erhalten und macht
Fortschritt 742→746. Damit ist T03 auf cacbf0d für beide Wechselrichtungen
bestanden; weitere Zugriffs-, Paket- und Lebenszyklusfälle bleiben offen.
Der Beleg `two-user-identity/result.json` bindet 73 Ereignisse und hat SHA-256
`b3a334f3b9d8f212bca231bdbf41e025477672663de9e77578a1c2752f7256fb`.
Die zusätzliche Aufnahme `both-after-reverse-switch.json` hat SHA-256
`06d9af7c6732428ecce279db11c12ff78f633d7ba7680329f88dab5d620d873e`.

Dieser Boot verwendet weiterhin Netzwerk `none`; reguläre
Paketdownloads benötigen einen dokumentierten Start mit `--network user`.
Paket-, Logout- und VM-Neustartbelege älterer Images werden nicht übernommen.
D1–D7 und der vollständige Referenzablauf bleiben offen.

## cacbf0d: Direkter Logout und Passwortwechsel vor dem VM-Neustart

Nach dem Identitätsnachweis wird Alpha ausdrücklich abgemeldet. Der ursprüngliche
Prozess `6544/311909` und sein Kontext verschwinden; CE ist `[0,11]`, seine
bekannte GNU-Datei liefert bei erneutem Leseversuch keine Bytes. Beta behält
seinen ursprünglichen Prozess `8545/369467` und macht Fortschritt 1027→1030.
Nach frischer Beta-Anmeldung ändert `aegis passwd` dessen Passwort über AOSP.
Die folgende direkte Beta-Abmeldung beendet dessen ursprünglichen Kontext und
Prozess; CE ist `[0]`, auch seine bekannte Datei liefert keine Bytes.
Diese Kernfälle schließen offene Zugriffe, IPC, konkurrierende Starts und
Fehlerfälle von T10/T11 noch nicht vollständig ab.

Ein Versuch mit Betas altem Passwort wird abgewiesen. CE bleibt `[0]`, und
`linux start` wird in diesem nicht authentifizierten Terminal verweigert.
Die folgende Anmeldung mit dem neuen Passwort bleibt stabil und entsperrt
ausschließlich Beta neben Systembenutzer 0. Nach ausdrücklichem Runtime-Start
liest die echte GNU-Shell die ursprüngliche Datei und beide persönlichen
Konfigurationsproben bytegleich. Alte Testdateien aus `/tmp` und `/run` fehlen.

Für alle drei persistenten Dateien stimmen vor und nach Passwortwechsel,
Logout und Runtime-Neustart Inhaltsprüfsumme, Inode, Größe, mtime, ctime,
Eigentümer, Gruppe und Modus überein. Das ist ein konkreter Dateinachweis.
Die zusätzliche Quellprüfung erklärt den Kryptographiepfad: Der erfolgreiche
Buildbeleg `runtime-storage-source.json` bindet die tatsächlich angepassten
LockSettingsService-/SyntheticPasswordManager-Dateien. Die Methoden
`setLockCredentialInternal`, `setLockCredentialWithSpLocked` und
`deriveFileBasedEncryptionKey` sind bytegleich zum gepinnten Framework-Commit
`99b01a65cc4c104933788b3143285ab6bae65827`. Der Passwortwechsel ersetzt den
LSKF-Protektor desselben Synthetic Password; die FBE-Ableitung verwendet dieses
weiter. Eine vollständige Neuverschlüsselung persönlicher Dateien ist nicht
Teil dieses normalen Passwort-zu-Passwort-Pfads. Die Klassen sind wegen der
anderen AEGIS-Integrationen ausdrücklich nicht insgesamt unverändert.
Es wurden keine Laufzeit-Schlüssel ausgelesen.

Belege unter `out/phase1-dod/pmsg-cacbf0d/`:

| Beleg | SHA-256 |
| --- | --- |
| `direct-logouts/result.json` (90 Ereignisse) | `88d0e9b5af57e243be383833998a49006d29f1c6ce98ab9288696dd28842c810` |
| `password-source-review/result.json` | `e6763ab3018d6b9bd44b1fdbef4a7438db05fa3b4deef8b3590dc50227b17401` |
| `password-pre-reboot/result.json` (106 Ereignisse) | `5c0f99abfcfb8c58ea96f7a46674e22194664ac84815ddb962ace7e96dde9290` |

Beta ist danach erneut abgemeldet. Der vollständige VM-Neustartteil von T02,
die übrigen Pflichtvarianten und D1–D7 bleiben offen. Das alte Passwort wird
nicht mehr für einen erfolgreichen Beta-Zugang verwendet; der bestehende
Testtreiber hält das neue ausschließlich im Arbeitsspeicher.

## cacbf0d: Gepaarter Shutdown und vollständiger erster Dienstaudit

Am 4. Oktober um 21:37 UTC sind Android und KeyMint-Helfer regulär beendet.
Der Dienst ist inaktiv mit Ergebnis `success`; beide ursprünglichen
QEMU-Prozessidentitäten sind verschwunden. Android meldet `Power down`,
der Helfer `AEGIS_HELPER_SHUTDOWN_CLEAN`. Profil
`de23866c-db95-46ed-8bbb-748c399262ca`, Profilmanifest und Zuordnung beider
persistenten Datenträger bleiben erhalten. Die Prüfung bei gehaltenem
Profil-Lock bestätigt die unveränderlichen Bindungen, Overlay-Markierung
und Helfer-Dateisystem-UUID. `qemu-img check` meldet keine Fehler, beschädigten
Cluster oder Leaks. Die Datenträger wurden nicht repariert oder neu angelegt.

Der unveränderte abschließende Audit bleibt `REVIEW_REQUIRED`. Seine ersten
drei Rückgaben stimmen mit den bereits einzeln geprüften Bootbefunden überein.
Neu ist ausschließlich `mdnsd` PID 1026, Status 4, 4,452801 Sekunden nach dem
an dieselbe Instanz gesendeten Shutdown-SIGTERM. Sechs Quelldateien aus
mdnsresponder, init und bionic sind bytegleich mit den Git-Objekten des
aktuellen Buildmanifests: `MainLoop` beendet sich bei SIGTERM/SIGINT mit
`EINTR=4`, `main` reicht diesen Wert nach dem Aufräumen zurück. Dies erklärt
diese konkrete Rückgabe; andere Status-4-Ereignisse werden nicht pauschal
akzeptiert. Von 105 Signalbeendigungen besitzen 27 einen vorausgehenden
Dienststeuerungsauftrag und 78 eine Shutdown-Zuordnung. Keine bleibt ohne
Zuordnung. Der Mitschnitt enthält einen SystemServer-Start (PID 1299),
keine Treffer der sechs geprüften Fatal-/ANR-Marker, keine ungeparsten
Terminalereignisse und keine Zeitrücksprünge.

Belege unter `out/phase1-dod/pmsg-cacbf0d/`:

| Beleg | SHA-256 |
| --- | --- |
| `pre-shutdown-state.json` | `d118fd5effebb5870e5829514e4f19213d4b6f38de652c83332c55a40ddb6686` |
| `paired-shutdown.json` | `57ed1ec12f6e41bf69cd59f71fd187bee0ce21969404d9a8f7068700e3ec9888` |
| `boot1-final-service-audit.json` | `e2665e373fe149e07aa09fa650b2ed677d8a8c61ce6f37e2b234b292da675a6a` |
| `shutdown-source-review/source-binding.json` | `c2f67eddf0aafc817a7b3612ddfbee841e6d26e26d99b902011d535cfc5af1cb` |
| `shutdown-source-review/result.json` | `2593403a9f79e4edbd998700b9b4c1b64aa7b8d0fe8dda8d022215115008a544` |

Der zweite Boot startet mit demselben Image und Profilpaar sowie regulärem
ausgehendem QEMU-Netzwerk. Der anschließende Passwortnachweis steht im folgenden
Abschnitt. D1–D7 und die gesamte Referenzabnahme bleiben offen.

## cacbf0d: T02 nach gepaartem Neustart bestanden

Boot 2 besitzt die neue ID `48f44f35-222a-437e-9b27-fe04e31ea7bc` und
SystemServer `1017/20295`; Image und Profilpaar sind unverändert. Der bestehende
Hostschlüssel authentifiziert ADB nach dem zweiten Transportversuch; es wurde
kein neuer Schlüssel eingetragen. Vor persönlicher Anmeldung bestätigt die
Aufnahme CE `[0]`, beide vorhandenen persönlichen Benutzer gestoppt, keine
Runtime-Prozesse, SELinux Enforcing und FBE. Beide ursprünglich von GNU
geschriebenen Testdateien liefern in diesem Zustand keine Bytes.

Betas erster korrekter Anmeldeversuch nach diesem Neustart verwendet das neue
Passwort und gelingt ohne vorgeschalteten Fehlversuch. Vor dessen Übermittlung
bleibt Beta CE-gesperrt und ohne Kontext. Die verzögerte Statusprüfung bestätigt
die Sitzung, ausschließlich CE `[0, 11]` ist entsperrt. Der ausdrücklich
gestartete GNU-Kontext liest die ursprüngliche 1024-Byte-Datei sowie beide
persistent gespeicherten 64-Byte-Konfigurationsproben bytegleich. Bei allen
drei Dateien stimmen Inode, Größe, mtime, ctime und Eigentümer/Rechte exakt
mit dem Stand vor dem Passwortwechsel überein. Die ursprünglichen flüchtigen
Dateien sind verschwunden; keine Originaldatei wurde neu geschrieben.

Nach erneutem regulärem Logout bestätigt AOSP wieder CE `[0]`. Ein einzelner
Versuch mit Betas altem Testpasswort wird abgewiesen; CE bleibt unverändert
gesperrt und `linux start` verweigert den nicht angemeldeten Aufrufer.
Die abschließende unabhängige Aufnahme bestätigt dieselbe Boot-ID und
SystemServer-Identität, SELinux Enforcing und keine Runtime-Prozesse.
Zusammen mit dem vorherigen Passwortwechsel und dessen gebundenem AOSP-
Quellpfad ist damit **T02 auf cacbf0d bestanden**. Der vollständige
Referenzablauf, Alphas Rücklesen und private Paketpersistenz sind damit nicht
abgeschlossen. Der frühere Beobachter-Timeout bleibt im Ereignispräfix erhalten;
dieser Fall enthält keinen neuen Treiberfehler.

Belege unter `out/phase1-dod/pmsg-cacbf0d/`:

| Beleg | SHA-256 |
| --- | --- |
| `boot2-baseline.json` | `44641271c5ad03d57b0573c87dd8b7fad4991b3a83fd953e110bcf1b42663bb8` |
| `password-post-reboot/result.json` (137 Ereignisse) | `618649e25c26218e8583b66c977612c5ec7e8380e87fb46610ce97bd5fae30b1` |

Der neue Fallprüfer `record-password-post-reboot.py` und der eingefrorene
Ereignissatz sind im lokalen Ergebnisbeleg mit Prüfsummen gebunden. Die
Testpasswörter verbleiben ausschließlich im ursprünglichen laufenden Treiber.

## cacbf0d: Beide Originaldatenbestände nach Neustart erhalten

Alphas erste korrekte Anmeldung in Boot 2 gelingt ebenfalls ohne vorherigen
Fehlversuch. Vor der Passwortübermittlung bleiben CE und Kontext gesperrt
beziehungsweise abwesend; danach ist ausschließlich CE `[0, 10]` entsperrt.
Die verzögerte Sitzungsprüfung und der ausdrückliche Runtime-Start gelingen.
GNU liest Alphas ursprüngliche 1024-Byte-Datei und die beiden persistenten
Konfigurationsproben bytegleich. Zusammen mit Betas vorherigem Rücklesen sind
damit die ursprünglichen Dateien und Konfigurationen beider Benutzer nach
dem gepaarten Neustart erhalten. Alte `/tmp`- und `/run`-Proben sind abwesend.
Der eingefrorene Satz umfasst 150 Ereignisse und enthält unverändert die
137 Ereignisse des T02-Nachweises. In sechs lokalen Bootlogs findet der
laufende Treiber keines der vollständigen generierten Testpasswörter; dies
ist weiterhin nur eine Teilprüfung von T01.

Die unabhängige Verity-Prüfung bestätigt in Boot 2 alle acht aktiven
System-Mappings mit Status `V`, `restart_on_corruption` und zugehörigen
schreibgeschützten EROFS-Mounts. Profilmanifest, Image, Boot-ID und
SystemServer `1017/20295` bleiben gebunden und unverändert. Zuletzt ist
Alpha in seiner GNU-Shell; Beta ist ausdrücklich abgemeldet und CE-gesperrt.
Private Pakete sind auf diesem Profil noch nicht installiert. Deshalb bleiben
deren Persistenz, weitere T07-/T12-Varianten und der gesamte Referenzablauf offen.

Der Dienstmitschnitt bis nach dem Rücklesen enthält einen SystemServer-Start
und keine Treffer der sechs geprüften Fatal-/ANR-Marker. Sein ursprünglicher
Audit bleibt `REVIEW_REQUIRED`. Die Recovery-Diagnose PID 404 zeigt wieder
`terminal=-2` bei unverändertem Aggregat und ist an denselben pmsg-Quellpfad
gebunden. Aconfigd PID 650 meldet ausdrücklich die Übergabe, der tatsächliche
Mainline-Initialisierer PID 652 endet mit 0. `rename_eth0` PID 672 endet in
diesem Netzwerkmodus ebenfalls mit 0. Der zunächst nicht zugeordnete SIGKILL
an `hidl_memory` PID 827 folgt unmittelbar auf die protokollierte
Property-Aktion `hidl_memory.disabled=true`; die zum Manifest bytegleiche
RC-Datei führt dafür `stop hidl_memory` aus. Die einzige markierte Zeitinversion
beträgt drei Mikrosekunden zwischen Threads 504 und 470 und ist allein kein
Beleg für einen Rücksprung der Systemuhr.

**Ungeklärt bleibt die genaue Ursache des Status 1 von PID 445**, dem
VirtualizationService-Aufräumschritt für alte UID-Zuordnungen. Die originale,
unveränderte Init-Regel ist quellengebunden; die ursprüngliche Fehlerausgabe
und der konkret fehlgeschlagene Pfad/Systemaufruf fehlen jedoch. Dieser
Rückgabewert wird nicht pauschal akzeptiert oder allein aus späteren
Dateirechten erklärt. Keine erneute Aufräumaktion und keine Rechteänderung
wurde für die Einordnung ausgeführt. Eine spätere reine Metadatenaufnahme
findet `/data/misc` als `1000:9998`, Modus `01771`, und das Zielverzeichnis
als `1000:1000`, Modus `0771`. Zusammen mit dem gebundenen Init-UID-/Gruppenpfad
stützt dies eine Rechtehypothese für das Entfernen des Zielverzeichnisses.
Es rekonstruiert weder sämtliche ursprünglichen Operationen noch die fehlende
Fehlerausgabe. Der lokale Zusatzbeleg
`boot2-service-review/migration-later-metadata.json` hat SHA-256
`50e92b603d919b93a051b3180a36d28ea94b7fa00b5ec75b1b37b022d8d1a70b`.
D1 bleibt offen.

Belege unter `out/phase1-dod/pmsg-cacbf0d/`:

| Beleg | SHA-256 |
| --- | --- |
| `paired-readback/events.json` (150 Ereignisse) | `6e920c2b642b2561f34474a8f557511596268c2387ced2dda8923f5ff41127fb` |
| `paired-readback/result.json` | `d70ae3236db51290b319115f2707f7d949fd052f8a7317ded3f778ebdb3763f3` |
| `boot2-active-verity/result.json` | `1bec3452541a8499fbf56ca903a3466dd4c3a3878af37a21eee32bf9ff18f950` |
| `boot2-after-readback-audit.json` | `93f2d3b1df588a3a8db10f7a359e411a82a42247afb06711a087a1c72dbc65fe` |
| `boot2-service-review/result.json` | `c6464ce9739931d3d79468c451a4e35c162c323bbad5e3a3d9892b2190ae8879` |

## cacbf0d: Gemeinsames jq u3 veröffentlicht und in Alpha aktiviert

Der ursprüngliche Treiber beantragt am 4. Oktober um 22:10 UTC über Alphas
angemeldete CLI `linux package install --scope all jq=1.7.1-6+deb13u3`.
Vorher fehlen gemeinsame/private Paketgenerationen; Alpha besitzt unverändert
78 Basispakete, Beta ist CE-gesperrt. Die bytegleich zum Image-Commit gebundene
AEGIS-Paketpolicy verwendet die drei offiziellen HTTPS-Quellen für trixie,
trixie-updates und trixie-security mit festem `signed-by`. Der tatsächliche
Plan enthält ausschließlich jq/libjq1 `1.7.1-6+deb13u3` und libonig5
`6.9.9-1+b1`; er ist vor der frischen AOSP-Adminfreigabe gespeichert.

Die Veröffentlichung erzeugt die gemeinsame Generation
`9eeef520c0366e2aeeb7302cab2ecd8f55e1cc88ee5d9b37d481066cf8b57236`.
Der Vorher-/Nachhervergleich bestätigt denselben laufenden Alpha-Init
`4492/117553`, dieselbe vollständige Paketdatenbank, Namespaces und
private Zuordnungen. Nur die gemeinsame Auswahl ändert sich.
`linux status` meldet ausdrücklich `packages=activation-pending`.
Beta bleibt während des gesamten Falls CE-gesperrt.

Der reguläre eigene Stopp beendet die ursprüngliche Init-Identität und
entfernt ihre Kontextgruppe. Alpha bleibt angemeldet und CE `[0, 10]`
entsperrt. Der anschließende Shell-Aufruf wird abgewiesen; Status vor/nachher
bestätigt weiterhin einen gestoppten Kontext und dieselbe AOSP-Sitzung.
Erst `linux start` erzeugt Init `5417/246350` auf dem veröffentlichten
gemeinsamen Image. Der vollständige Paketbestand umfasst exakt die alten
78 Pakete plus die drei geplanten Ergänzungen, ohne unvollständige oder
verbliebene Konfigurationsdatensätze. Als automatische Pakete sind genau
libjq1 und libonig5 aufgezeichnet.

Aus dem gewöhnlichen GNU-Kontext mit UID/GID 1000 werden alle drei Versionen
geprüft und mit jq die JSON-Berechnung 41 → 42 ausgeführt. Programm- und
Bibliothekshashes sind gespeichert. Alphas ursprüngliche 1024-Byte-Datei und
persistente Konfigurationsproben bleiben bytegleich. Bereits zuvor fehlende
flüchtige Dateien belegen hier keinen zusätzlichen Bereinigungsfall.

Ein Fehler der vorgeschalteten Leseprüfung bleibt ausdrücklich erhalten:
Ereignis 151 folgt auf `cat /etc/apt/sources.list`, obwohl die geprüfte
Debian-Basis `/etc/apt/sources.list.d/debian.sources` verwendet. Der reine
Leseaufruf scheitert vor `apt-cache`; es gab keine Paketänderung. Der spätere
korrekte Deb822-Read liefert keine Paketlisten für jq und wird daher nicht
als Versionsverfügbarkeitsbeleg verwendet. Dafür gelten der konkrete
AEGIS-Plan und die tatsächliche Programmausführung. Seit Beginn des Paketplans
enthält der eingefrorene Abschnitt keinen neuen Treiberfehler.

Belege unter `out/phase1-dod/pmsg-cacbf0d/`:

| Beleg | SHA-256 |
| --- | --- |
| `before-shared-install.json` | `22a235e294a5f7dd20aa9b4759de61c6e4f195d01dd0e6eb323923342dfda575` |
| `shared-jq-u3/plan-before-approval.json` | `ccabb9ff6ad72b07b4f9540f1c23a3259d709c8ab2cfefa7e556a8c972a94116` |
| `after-shared-publish.json` | `3836a64128d54f2e62a4ec425744c4fd469ff93fc8ce2004cefcaeb2428d550c` |
| `shared-jq-u3/stopped-context.json` | `fa5b894b26c2849aee28840847d811dd42a3630e19f0f71530066c21714f4c85` |
| `shared-jq-alpha-active.json` | `599799f0e1af6366829409b73f1b07cef2d9619165a7180e9a398ecd36840afb` |
| `shared-jq-u3/activation-events.json` (173 Ereignisse) | `8ec9a46a67215df1d1bf83e062ceedf1fdf4d786fc8509c1051bb1b0ef725e5a` |
| `shared-jq-u3/result.json` | `359d5107402f62188399e6a6af545779bf3fd922f974908d3ed0255c9bbfb298` |

Dies belegt nur die erlaubte gemeinsame Erstinstallation samt Aktivierung
bei Alpha. Ein gleichzeitig laufender Peer, Betas GNU-Ausführung, die private
u4-Version, weitere T13–T17-Varianten und D1–D7 bleiben offen.

## cacbf0d: Alphas private jq-Version u4 aktiviert

Alphas regulärer Auftrag `linux package install --scope user jq=1.7.1-6+deb13u4`
erzeugt einen Plan mit genau zwei Änderungen: jq und libjq1 wechseln von u3
auf u4. libonig5 bleibt `6.9.9-1+b1`. Der konkrete Plan ist vor der erneuten
AOSP-Adminfreigabe gespeichert. Die Veröffentlichung erzeugt die private
Generation `5a80d92f2dd9609ee085b6718709a331c6e60a6263199bc6a606ce429b261bbd`,
gebunden an Benutzer `10/10` und die unveränderte gemeinsame Generation
`9eeef520c0366e2aeeb7302cab2ecd8f55e1cc88ee5d9b37d481066cf8b57236`.
Der bisherige Init `5417/246350` und seine vollständige Paketdatenbank bleiben
zunächst identisch; Status meldet ausstehende Aktivierung. Beta bleibt CE-gesperrt.

Nach eigenem regulärem Stopp sind ursprüngliche Init-Identität und Kontextgruppe
entfernt, während Alpha angemeldet und CE `[0, 10]` entsperrt bleibt. Der
ausdrückliche Start erzeugt Init `5826/349443`. Sein Root-Dateisystem stammt
aus Alphas privatem CE-Paketimage; die private Versionswahl lautet genau
`jq / arm64 / 1.7.1-6+deb13u4`. Alle 81 Pakete sind vollständig installiert.
Gegenüber dem vorherigen Bestand ändern sich ausschließlich die beiden
geplanten Versionen. Gemeinsame Auswahl, Benutzer-/Boot-/SystemServerbindung
und Betas gesperrter Zustand bleiben erhalten.

Der gewöhnliche GNU-Prozess bestätigt UID/GID 1000, eigenes HOME, die drei
erwarteten Paketversionen und die tatsächliche JSON-Berechnung mit jq.
Die libjq-Prüfsumme ist nun
`92012c8c198ed5f8e44042a124c3271e89a0a2867fc3344642d9ed391ef75f50`
statt `58a6c82e3cc0b55f2e11e85ffa487bd2381c4cd30068874504c639c76a3e59d6`;
libonig bleibt bytegleich. Der jq-Programmhash ist bei diesen beiden Paketen
gleich und wird nicht fälschlich als Versionsunterschied gewertet.
Alphas ursprüngliche Datei und Konfigurationsproben bleiben bytegleich.

Belege unter `out/phase1-dod/pmsg-cacbf0d/`:

| Beleg | SHA-256 |
| --- | --- |
| `alpha-private-jq-u4/plan-before-approval.json` | `bbad2da69c49c03293c7bd928b30088ee97c2c2319f60365206c7844e710e4b9` |
| `after-alpha-private-publish.json` | `75c4074740a7d039d72875ebd9136ff8ab650293a3c653ccb6f71ffea9d44fc3` |
| `alpha-private-jq-u4/stopped-context.json` | `28cd61a2de42149083fcecfcae95257f449259f20958f0c47c2d737748ac2e1c` |
| `alpha-private-jq-active.json` | `08b0ae560dc911d6b95821b8f87062813f55050f5e1d4322216b848c1fc5f964` |
| `alpha-private-jq-u4/activation-events.json` (187 Ereignisse) | `84facb14a83dad44de05fcbae9adb1b59e43ad8ce2dabe4afd15b668a39e4785` |
| `alpha-private-jq-u4/result.json` | `d55ab12b27ff17684031f2563fb0324497c33cdd77880243d2549ffab9cbf4e9` |

Antragsteller und freigebender Administrator sind hier beide Alpha. Dieser
Fall ersetzt weder eine Freigabe durch eine andere Person noch die
Autorisierungs-Negativmatrix. Betas gleichzeitige gemeinsame u3-Ausführung,
ungültige Versionen, private Entfernung, Updates, Konflikte, Parallelität
und die vollständige D1–D7-Abnahme bleiben offen.

## ec01e5fa: Originaldaten und Paketversionen nach gepaartem Neustart erhalten

Der zweite Boot ist mit derselben Profil-ID und demselben Image bestätigt.
Seine neue Boot-ID lautet `f68b515c-075b-4854-8419-fe119c34e193`, der
SystemServer ist `1173/20543`. Der vorhandene Hostschlüssel stellt wieder
authentifiziertes ADB her. Die Aufnahme vor jeder persönlichen Anmeldung
bestätigt SELinux Enforcing, Verity-Modus `enforcing`, die acht
Verity-Statuswerte `2`, CE `[0]`, beide vorhandenen persönlichen Benutzer als
gestoppt und keine persönlichen Runtime-Kontexte. Beide bekannten Originaldateien
liefern in diesem Zustand keine Bytes.

Der ursprüngliche Testtreiber meldet zuerst Alpha und anschließend Beta mit
den unveränderten Testpasswörtern an. Beide ersten korrekten Anmeldungen nach
dem Neustart gelingen ohne vorherigen Fehlversuch; die verzögerte Statusprüfung
bestätigt jeweils eine stabile Sitzung. Vor dem jeweiligen Passwort bleibt
das Ziel CE-gesperrt und ohne Kontext. Nach Alphas Anmeldung ist nur CE
`[0, 10]` offen, nach Betas Anmeldung CE `[0, 10, 11]`.

Beide ausdrücklich gestarteten GNU-Kontexte lesen ihre ursprünglichen
1024-Byte-Dateien und beide persistenten Konfigurationsproben bytegleich.
Alpha führt weiterhin jq/libjq1 `1.7.1-6+deb13u4` aus, Beta
`1.7.1-6+deb13u3`, jeweils mit libonig5 `6.9.9-1+b1` und erfolgreicher
JSON-Berechnung 41 → 42. Die unterschiedlichen libjq-Prüfsummen entsprechen
exakt den Werten vor dem Neustart. Kein Test schreibt die Originaldateien neu.

Der vollständige Zustandsabgleich bestätigt bei beiden Benutzern dieselben
81 installierten Paketversionen, bytegleiche Dpkg-Statusdateien, unveränderte
private Auswahlen und dieselben gemeinsamen/privaten Generationszuordnungen.
Die CE-Backing-Datei von Alphas privatem Store und Betas gemeinsame Backing-Datei
stimmen mit dem vorherigen Stand überein. Neue Kontext-Inits sind
`3621/77659` für Alpha und `4455/104199` für Beta. Ihre sechs Namespaces sind
getrennt, die Host-UID-Zuordnungen unverändert.

Neue begrenzte Hintergrundproben werden erst nach positivem Originaldatei-
Readback angelegt. Alphas `3690/86539` bleibt beim Wechsel zu Beta erhalten
und macht weiter Fortschritt; Beta verwendet `4736/108339`, beide intern
PID 23. Das sind neue Prozesse im neuen Boot, kein behauptetes Überleben
eines VM-Neustarts.

Belege unter `out/phase1-dod/ec01e5fa-verity-base/`:

| Beleg | SHA-256 |
| --- | --- |
| `reboot-readback/result.json` | `11d319045b6380211a513715cfc4ceb62e18d7cc2391523fe9e22c63c816793c` |
| `boot2-baseline.json` | `8637c90dfdf0e89adb2e3084e004692409f927659ec82e0a9e48c3a89af8c5a9` |
| `boot2-both-contexts.json` | `f3d21a7ef81cc921a688ef507d70ccc1ea5adf5e7c45eed0903c21eff77b9d21` |

Der Offline-Abgleich bindet 302 eingefrorene Ereignisse und den identischen
vorherigen 259-Ereignis-Stand. Der gepaarte Neustart mit geschütztem Zustand
vor Anmeldung und Originaldaten-/Versionsrücklesung danach ist damit belegt.
Die ursprünglichen flüchtigen Dateien und Queues waren bereits in früheren
Runtime-Stoppfällen entfernt worden; hier entsteht kein neuer Nachweis ihrer
Bereinigung. Benutzer C, Passwortwechsel, Löschung/ID-Wiederverwendung,
vollständige Logout-Ressourcen-/Konkurrenzfälle, Paketfehler-/Autorisierungsmatrix
und sämtliche weiteren offenen Varianten bleiben erforderlich. D1–D7 bleiben offen.

## ec01e5fa: Gamma nutzt gemeinsame Software; AOSP stoppt Beta wegen Benutzerlimit

Am 4. Oktober wird Gamma nach erneuter AOSP-Anmeldung des Administrators Alpha
regulär als Benutzer/Seriennummer `12/12` angelegt. Vor Gammas erster Anmeldung
ist dessen CE noch gesperrt und kein Runtime-Kontext vorhanden. Nach erfolgreicher
Anmeldung startet Gamma seine Runtime ausdrücklich und führt jq/libjq1
`1.7.1-6+deb13u3` aus. Die vollständige Paketdatenbank mit 81 Paketen, das
gemeinsame Basisimage und der libjq-Hash entsprechen Betas zuvor aktivem
gemeinsamen Kontext. Gamma besitzt keine private Paketauswahl; sein HOME ist
frisch. Seine UID/GID-Zuordnungen sind aus tatsächlichen GNU-Ausgaben geprüft.

Der ActivityManager meldet um **17:44:05 UTC**, während der Vorbereitung des
Vordergrundwechsels, vier laufende Benutzer und den Stopp von Benutzer 11.
Die Zustandsabfrage bestätigt `mMaxRunningUsers:3`, einschließlich Systembenutzer
0. Betas ursprünglicher Init `4455/104199` und Testprozess `4736/108339` sind
danach beendet, sein Kontext fehlt und CE lautet `[0, 10, 12]`. Seine bekannte
Originaldatei liefert im gesperrten Zustand keine Bytes. Dies ist ein
AOSP-Ressourcenstopp; ein direkter Beta-Logout wurde hier nicht ausgeführt.
Alphas vollständiger Kontext bleibt unverändert, sein ursprünglicher Prozess
`3690/86539` macht vor und nach dem Wechsel weiter Fortschritt.

Gamma prüft anschließend als gewöhnlicher GNU-Benutzer zwölf fremde Testpfade:
Originaldateien, Konfigurationen und synthetische Test-Secrets, Alphas vorhandene
private Paketmetadaten und Paketimage sowie ausgewählte Pfade über HOME und
Prozesswurzel. Alle Leseversuche liefern keine Bytes und werden abgewiesen.
Schreibende Öffnungen derselben Pfade sowie der gemeinsamen jq-Datei werden
ebenfalls abgewiesen. Diese Öffnungen verwenden weder Erstellung noch Kürzung
und schreiben keine Nutzdaten. Eigene Lese-/Öffnungskontrollen gelingen.
Unabhängige Vorher-/Nachher-Aufnahmen bestätigen unveränderte Alpha-Testdateien
und private Paketmetadaten. Alpha bleibt dabei entsperrt; Beta ist bereits
gesperrt und hat in diesem Fall keinen privaten Paketstore.

Belege unter `out/phase1-dod/ec01e5fa-verity-base/`:

| Beleg | SHA-256 |
| --- | --- |
| `third-user/result.json` | `dcc97fee41f1c2faa17ba81decde095d27db76c16f36359ddacf209bd8102dee` |
| `third-user-contexts.json` | `355c0e2b56d5e56e56394f5ca90417a2f75d07d9ff83aedd42f74293f22564fe` |
| `gamma-resource-stop.json` | `f48f5eac595d574677af558d89d0dd90eba69174cee45207f8b4053caf723fc2` |
| `gamma-peer-fixtures-before.json` | `c5e0709e31e9f1163e74159538e0df5121a7bab3d2f48928a8053a06c150621e` |
| `gamma-peer-fixtures-after.json` | `abfe0781da5fddb3de542d8666eda233ecba670667dd4aaa9c8f031395ce053c` |

Der Offline-Abgleich bindet 324 eingefrorene Ereignisse und den unveränderten
vorherigen 302-Ereignis-Stand. Betas erneute Anmeldung und positive Rücklesung
der Originaldaten nach diesem Ressourcenstopp stehen noch aus. Dieser Fall
ersetzt weder gegenseitige Prüfungen zweier vorhandener privater Paketstores
noch die vollständigen T06-/T08-/T14-Varianten. D1–D7 bleiben offen.

## ec01e5fa: Beta nach dem Ressourcenstopp mit Originaldaten wieder angemeldet

Nach regulärem Gamma-Logout bestätigt eine unabhängige Abfrage das Ende von
Gammas Init `9778/214259`, den entfernten Kontext und CE `[0, 10]`. Auch Beta
hat zu diesem Zeitpunkt keinen Kontext. Alphas Init und ursprünglicher Prozess
bleiben erhalten. Bei Betas Auswahl als Vordergrundbenutzer bleibt sein CE
bis zur Passwortprüfung gesperrt; die Auswahl erzeugt keine Runtime.

Betas erste korrekte Anmeldung mit seinem unveränderten Passwort gelingt und
bleibt in der verzögerten Statusprüfung gültig. Erst sein ausdrücklicher
Runtime-Start erzeugt den neuen Init `13022/338454`. Die ursprüngliche
1024-Byte-Datei sowie Konfiguration und synthetische Test-Secret-Datei werden
aus der gewöhnlichen GNU-Shell bytegleich rückgelesen. jq/libjq1 u3 und libonig5
werden mit den erwarteten Versionen geprüft, jq tatsächlich ausgeführt und
die libjq-Prüfsumme bestätigt. Der vollständige Bestand mit 81 Paketen,
Dpkg-Status, gemeinsamem Basisimage und privater Auswahl entspricht Betas
Zustand vor dem Ressourcenstopp. Die gemeinsamen und Alpha-privaten
Generationszuordnungen sind ebenfalls unverändert; Gamma bleibt gesperrt.

Der neue Beta-Testprozess `13241/341788` wird erst nach bestätigtem Ende seines
Vorgängers und positiver Originaldatenrücklesung ausdrücklich erzeugt. Dies ist
kein Überlebensnachweis des alten Beta-Prozesses. Alpha behält dagegen seinen
vollständigen Kontext und denselben fortschreitenden Prozess `3690/86539`.
Boot-ID und SystemServer bleiben unverändert.

| Beleg unter `out/phase1-dod/ec01e5fa-verity-base/` | SHA-256 |
| --- | --- |
| `beta-resource-recovery/result.json` | `961c657db93faf42997cb6b6ea7a35bf2980cd622a8f7bd83b772fab57e03214` |
| `beta-resource-recovered-contexts.json` | `c8bf753e7349b9958c0e66f1e6b21d4858c57c41164a83d41b10bc28f93fb303` |
| `gamma-logout-state.json` | `b588fc4dff29afac94296e68cdeb9f92edc5e6d8e3528fbd2aef2805b00eb43e` |

Der Nachweis friert 347 Ereignisse ein; die ersten 324 stimmen unverändert mit
dem Gamma-Nachweis überein. Der erste Offline-Prüfer scheiterte daran, die
vollständigen AOSP-Benutzerlisten einschließlich `running` gleichzusetzen.
Die Identitäten sind unverändert, aber korrekt läuft jetzt Beta statt Gamma.
Originalprüfer und Fehlerbeleg bleiben erhalten und hashgebunden. Der korrigierte
Abgleich prüft Identitäten sowie beide erwarteten Laufzustände ausdrücklich;
Gastaktionen und Rohbelege wurden nicht wiederholt oder verändert.

Damit ist dieser AOSP-Ressourcenstopp samt Wiederanmeldung und Originaldaten-
rücklesung belegt. Alte flüchtige Dateien und Queues fehlten bereits vorher;
hier entsteht kein neuer Bereinigungsnachweis. Vollständige T08-Zuordnung und
die übrigen offenen T01–T17-/D1–D7-Anforderungen bleiben erforderlich.

## ec01e5fa: beide interaktiven Wechselrichtungen und vollständige T08-Zuordnung

Bei offener Beta-GNU-Shell meldet sich Alpha über eine zweite echte AEGIS-CLI
mit AOSP-Passwortprüfung an. Die bisherige GNU-Verbindung wird widerrufen;
das erste Terminal meldet `terminal=unauthenticated`. Sowohl Runtime-Start als
auch Shell-Zugang werden dort abgewiesen. Erst erneute Alpha-Anmeldung am
ersten Terminal erlaubt die Shell im bereits bestehenden Kontext und die
bytegleiche Rücklesung seiner Originaldatei. Der gleiche Ablauf besteht
anschließend in der Gegenrichtung von Alpha zu Beta.

Betas Prozess `13241/341788` und Alphas Prozess `3690/86539` behalten über beide
Wechsel hinweg PID, Startzeit, Host-Identität und Namespaces und machen weiter
Fortschritt. Beide vollständigen Kontexte, Paketbestände und Zuordnungen
stimmen vor/nach diesen Wechseln überein. CE bleibt `[0, 10, 11]`; Gamma bleibt
gesperrt. Es findet kein Runtime-Stopp oder Logout statt, Boot und SystemServer
bleiben unverändert. Der Nachweis friert 385 Ereignisse mit unverändertem
347-Ereignis-Präfix ein.

Der zusätzliche Offline-Abgleich ordnet damit alle T08-Varianten auf Image
`ec01e5fa5f2822da5763ab54c644bc5c5c5ab413` und Profil
`f8c09946-d131-40ba-8f30-3c3a4d778b0d` zu. Ereignisnummern sind nullbasiert.

| T08-Anforderung | Beleg und konkrete Prüfung |
| --- | --- |
| Wechsel beider Richtungen | `interactive-switches/result.json`, Ereignisse 347–384: offene GNU-Kanäle widerrufen, Terminalzugang bis frischer Anmeldung verweigert, Hintergrundidentitäten und vollständige Kontexte unverändert. |
| Bildschirmsperre | `screen-beta-logout/result.json`, Ereignisse 213–232: Kanal widerrufen; Zugang auch nach Aufwecken verweigert; beide Prozesse während bestätigtem `Asleep` aktiv; CE und Kontexte erhalten. Frische Anmeldung stellt den Zugang wieder her. |
| AOSP-Ressourcenstopp | `third-user/result.json`, Ereignisse 302–323: tatsächliche ActivityManager-Meldung zum Benutzerlimit, Betas ursprünglicher Init/Prozess und Kontext beendet, tatsächliche CE-Sperrung; Alpha unverändert. |
| Wiederanmeldung nach Ressourcenstopp | `beta-resource-recovery/result.json`, Ereignisse 324–346: vor Passwortprüfung gesperrt, frische Anmeldung und ausdrücklicher Start, bytegleiche Originaldaten und unveränderte Paketbestände; neuer Beta-Kontext, unveränderter Alpha-Kontext. |

| Beleg unter `out/phase1-dod/ec01e5fa-verity-base/` | SHA-256 |
| --- | --- |
| `interactive-switches/result.json` | `d4b471cce8b89149d7ce2e6d80e887049cb7441873fe6269f69865dc1e35ac99` |
| `interactive-switch-contexts.json` | `57afa608023f70ee1d62eed204dc33aa3657ec876c37af4e4729a439456d529f` |
| `t08-lifecycle-mapping.json` | `1316dd9f89131d47a5e519dc552a61a7ca875c314fa285cd34cc078c4d26a1fd` |

Der Abgleich prüft die referenzierten Prüfsummen und Originalereignisse erneut.
Die Bildschirmsperre stammt aus Boot 1, Wechsel und Ressourcenstopp aus Boot 2
desselben Images und Profilpaares; daraus wird kein Prozessüberleben über
einen VM-Neustart abgeleitet. Das AOSP-Limit schließt Systembenutzer 0 ein,
weshalb kein unbegrenzter Hintergrundbetrieb dreier persönlicher Benutzer
zugesagt wird. **T08 ist auf ec01e5fa belegt.** Die übrigen offenen Pflichtfälle
und alle sieben Gesamtabschlusskriterien bleiben erforderlich.

## ec01e5fa: Beta-eigene private Versionswahl mit Alpha-Adminfreigabe aktiviert

Beta beantragt über seine reguläre CLI `install --scope user jq=1.7.1-6+deb13u3`.
Der tatsächliche Plan hält die bereits gemeinsam vorhandene Version ausdrücklich
privat fest; Abhängigkeitsversionen ändern sich nicht. Die unveränderten
öffentlichen Debian-Quellen sind erneut an den bytegleichen Richtlinienquellcode
des Images gebunden. Alpha bestätigt den Plan mit frischer AOSP-Passwortprüfung.
Die um **18:33:26 UTC** veröffentlichte private Generation
`77965175290febf757d33082d1205cc8842a1c5db709d05b4c0e87485226e7aa`
gehört ausdrücklich `user 11 11`, bleibt an die gemeinsame Generation gebunden
und liegt in Betas CE-Speicher. Alpha wird durch die Freigabe nicht Eigentümer.
Gamma bleibt gesperrt; gemeinsamer und Alpha-privater Paketstand sind unverändert.

Nach Veröffentlichung sind beide vollständigen laufenden Kontexte unverändert;
Beta meldet ausstehende Aktivierung. Vor seinem eigenen Stopp erzeugt und liest
Beta zwei neue Testdateien unter `/tmp` und `/run/user/1000`. Der Stopp beendet
seinen bisherigen Init `13022/338454` und Prozess `13241/341788`; Sitzung und
CE bleiben erhalten. Erst der ausdrückliche Neustart erzeugt Init
`20459/523577` mit dem privaten Beta-Image als Root-Dateisystem. Alle 81
installierten Pakete, Dpkg-Status und Abhängigkeiten bleiben bytegleich zur
vorherigen gemeinsamen Variante. Die zusätzliche private Auswahl lautet jq u3.
Die tatsächliche GNU-Ausführung bestätigt Versionen, jq-Funktion und libjq-Hash;
Originaldatei und Konfiguration bleiben erhalten, die beiden frischen temporären
Dateien fehlen. Alpha behält seinen vollständigen privaten u4-Kontext und
fortschreitenden Prozess `3690/86539`. Betas neue begrenzte Prozessprobe
`20992/542247` ist ausdrücklich von ihrem beendeten Vorgänger getrennt erfasst.

Ein zusätzlicher GNU-Test enthielt zunächst die falsche Erwartung, UID 1000
könne `/var/lib/aegis/private-choices` lesen. Dieser Befehl endete mit Status 1;
Ereignis 409 bleibt als `driver-failure` erhalten. Der bytegleiche Image-Quellcode
in `package_guard.cpp`, Funktion `StoreChoices`, erzeugt das interne Verzeichnis
absichtlich root-eigen mit Modus 0700 und die Datei mit 0600. Der Fehler liegt
in dieser Testerwartung. Die Berechtigungen wurden nicht verändert. Der
separate korrigierte GNU-Befehl, Ereignis 410, prüft Programmausführung und
temporäre Bereinigung erfolgreich; die interne Auswahl wird unabhängig durch
den vorhandenen lesenden Zustandsbeobachter geprüft. Der erste Fehler wird
nicht nachträglich als erfolgreicher GNU-Test gewertet.

Auch eine späte lesende Plan-Workerprobe bleibt mit Exit 1 erhalten: Sie traf
nach erfolgreichem Planabschluss ein, als Worker und Plan-Cgroup bereits
entfernt waren. Die vorherige Beobachtung hatte beide Prozesse bestätigt;
der ursprüngliche Plan wurde nicht wiederholt.

| Beleg unter `out/phase1-dod/ec01e5fa-verity-base/` | SHA-256 |
| --- | --- |
| `beta-private-u3/result.json` | `04c67bcab7a7b659d35e9c79b735695243a304cd62ba29cc0c0cd5a2cdfe04be` |
| `beta-private-u3-install-inputs.json` | `b4ed35c1e6de0173559a568fcd76b70cdd17b15c02635b7d93d58e50dd659617` |
| `beta-private-u3-published.json` | `ff7b8a4a29267e89c62b579fb845daf3311ca131032c3bc277e03600c2fda687` |
| `beta-private-u3-active.json` | `0d13a15294a873d47d21e8c71b94ce92ea22c5a040b022f1a841b3a4128a7d88` |
| `beta-private-u3-stopped.json` | `d65e97de00309038b8162622ab03660146e2f1dc9708d660790d7169ad733c1b` |

Der Nachweis friert 422 Ereignisse einschließlich des exakt eingegrenzten
Fehlers ein; der vorherige 385-Ereignis-Stand und der gesicherte 410-Ereignis-
Fehlerstand stimmen unverändert überein. Beide privaten Stores bestehen jetzt
tatsächlich. Ihre gegenseitigen Zugriffstests sowie die übrigen Paketaktionen,
Autorisierungsvarianten, Updates, Konflikte und Fehlerfälle bleiben erforderlich.
Dieser Fall ersetzt weder deren Abnahme noch einen neuen VM-Neustart oder
eine frische IPC-Bereinigungsprüfung. D1–D7 bleiben offen.

## ec01e5fa: gegenseitige Zugriffe auf beide vorhandenen privaten Paketstores

Nach Aktivierung von Betas privater u3-Auswahl werden Paketstore, Auswahl und
Bibliothek beider Benutzer unabhängig als vorhanden bestätigt. Alpha nutzt
weiterhin private u4, Beta private u3; CE lautet durchgehend `[0, 10, 11]`.
Die gewöhnlichen GNU-Prozesse prüfen anschließend in beiden Richtungen vier
konkrete Pfade: fremde Store-Auswahl `current`, fremdes Paketimage sowie die
interne private Auswahl und libjq über die Prozesswurzel des jeweiligen
fremden Runtime-Init. Alle Leseversuche liefern keine Bytes; alle schreibenden
Öffnungen scheitern. Letztere verwenden ausschließlich `O_WRONLY`, weder
Erstellen noch Kürzen, und schreiben keine Nutzdaten. Eigene Lese- und
Öffnungskontrollen gelingen. Tatsächliche UID-Zuordnungen und eigene
Bibliotheksversionen sind in denselben GNU-Ausgaben bestätigt.

Die vollständigen Kontext-, Paket- und Generationsdaten bleiben unverändert.
Vorher-/Nachher-Prüfungen bestätigen identische Auswahlmetadaten, interne
Versionswahl und Bibliotheksbytes; Existenz, Größe und Rechte der beiden
Paketimages bleiben gleich. Ein vollständiger Bytevergleich der rohen
Paketimages wird dabei nicht behauptet. Alphas Prozess `3690/86539` und Betas
Prozess `20992/542247` behalten ihre Identität und machen vor und nach dem
jeweiligen fremden Zugriff Fortschritt. Der Wechsel zu Alpha erfolgt durch
reguläre AOSP-Anmeldung; beide CE-Speicher bleiben dabei entsperrt.

| Beleg unter `out/phase1-dod/ec01e5fa-verity-base/` | SHA-256 |
| --- | --- |
| `private-store-isolation/result.json` | `eff6aff735b63404df5c49fc4b13f2b96102b9e15b46d6c388f6ee46ac7c9868` |
| `private-store-fixtures-before.json` | `8da1e10403b741a8dbd101133bb647fc39687fc161989f4216511217fe0cfbc2` |
| `private-store-fixtures-after.json` | `ecbd3f69935e79efb2eb03b7921a2d3ddc8acc717824347912482b709a5c89f7` |
| `private-store-isolation-contexts.json` | `d2cb5d906e16a150125c4edc44efe74fa2dba5d6a8b3132db3a54c690fda33cb` |
| `t06-private-package-scope-audit.json` | `55ee9b718f9de61acc43c22fbe05928a3a0c0d85ee7d86a2a173cd5a3f0e1f03` |

Der Offline-Abgleich friert 435 Ereignisse ein und bestätigt das unveränderte
422-Ereignis-Präfix einschließlich des zuvor erklärten Testfehlers. Im neuen
Intervall tritt kein weiterer Treiberfehler auf. Eine lokale Prüfung der
Befehlsvorbereitung hatte wörtliche Zeilenumbrüche vor dem Versand erkannt;
diese wurden für das einzeilige Steuerprotokoll kodiert, ohne dass zuvor ein
Gastbefehl ausgeführt wurde. Die ursprüngliche Vorbereitung bleibt erhalten.

Damit ist die bisher fehlende Prüfung tatsächlich vorhandener privater
Paketstores geschlossen. **T06 insgesamt bleibt offen:** Der Entwicklerauftrag
nennt zusätzlich private und abstrakte Sockets sowie Host-IPC und D-Bus.
Die bisherigen POSIX-Mqueue-Tests und dieser Paketfall belegen diese Wege
nicht. Die noch erforderliche Zuordnung ist im Scope-Audit ausdrücklich
festgehalten; D1–D7 bleiben offen.

## Aktueller Lauf b832d6c: T08 vollständig zugeordnet

Der Offline-Abgleich `t08-lifecycle-mapping.json`, SHA-256
`53b7befe783e1424b5d48c31da14bcdf047cb7064f04de76a13020c8ea10978c`,
prüft 16 bestehende Belegdateien und die folgenden konkreten Anforderungen.
Ereignisnummern sind nullbasiert im eingefrorenen
`shell-after-stop-failure-events.json`; die Präfixe stimmen mit den früheren
Bildschirmsperr- und Ressourcenstopp-Belegen überein.

| Anforderung | Vorhandener Nachweis |
| --- | --- |
| Wechsel Beta → Alpha und Alpha → Beta | Ereignisse 105/106/116/117 und 129/130/137/138; ursprüngliche Prozesse behalten PID, Startzeit, interne/äußere UID, Cgroup, SELinux-Domäne und Namespaces. Fortschritt ist jeweils positiv. Die beobachtete CE-Liste bleibt `[0, 10, 11]`. |
| Bildschirmsperre sperrt interaktive Nutzung | Ereignisse 151–172: tatsächlich offener GNU-Kanal wird widerrufen, Terminal ist unangemeldet und neuer Start abgelehnt; beide ursprünglichen Hintergrundprozesse laufen bei tatsächlichem `Asleep` weiter. Aufwachen allein stellt die Anmeldung nicht wieder her. Frische Anmeldung stellt den Zugang her; beide Kontextaufnahmen stimmen überein. Kein CE-Entzug behauptet. |
| AOSP-Ressourcenstopp baut den betroffenen Kontext ab | ActivityManager 02:59:23 UTC stoppt Benutzer 11 wegen vier laufender AOSP-Benutzer einschließlich Systembenutzer. Ereignis 264 bestätigt das Ende der ursprünglichen PID/Startzeit vor Ablauf der begrenzten Probe, die entfernte Kontext-Cgroup und tatsächlich gesperrtes CE. Nachheraufnahme enthält Alpha/Gamma, keinen Beta-Kontext. Alpha bleibt vollständig erhalten und läuft weiter. |
| Wiederanmeldung nach Ressourcenstopp | Ereignisse 269–287 und drei Zustandsaufnahmen: Beta zunächst gesperrt/ohne Kontext; frische Anmeldung und ausdrücklicher Start erzeugen einen neuen Kontext. Originaldaten sind im bestehenden Ressourcen-Wiederherstellungsbeleg rückgelesen. Alpha und SystemServer bleiben identisch. |

Der bei der damaligen Beobachtung verwendete Treiber ist über die ursprüngliche
Eingangsprüfsumme an b832d6c gebunden. Seine Prozess-/Cgroup-/CE-Prüffunktion
ist gegenüber dem aktuellen Quelltext unverändert. Das bekannte veränderliche
Checkpoint-Ereignis 203 bleibt im Rohprotokoll erhalten; keine Aussage dieser
Zuordnung stützt sich darauf. **T08 ist auf b832d6c belegt.**
Die Zuordnung garantiert weder unbegrenzten Hintergrundbetrieb noch drei
gleichzeitige persönliche Benutzer. Sie ersetzt nicht T05, die vollständige
Ressourcenbewertung von T09, die konkurrierenden Logoutfälle von T10/T11 oder
den Gastnachweis eines neuen Images. Alle D1–D7 bleiben offen.

## Aktueller Lauf b832d6c: fehlgeschlagener Shell-Zugang nach Stopp

Am 4. Oktober 2026 bestätigten `linux stop` um 11:50:54 UTC und
`linux status` um 11:51:04 UTC für Beta einen gestoppten Kontext bei weiter
entsperrtem CE. Das anschließende `linux shell` öffnete dennoch eine GNU-Shell.
Der Treiber erwartete die Rückkehr zum AEGIS-Prompt mit einer Ablehnung und
meldete um 11:54:04 UTC einen `TimeoutError`; die danach gelesene Ausgabe
enthält den tatsächlichen GNU-Prompt. Dies widerspricht der Anforderung im
Entwicklerauftrag, einen fehlenden gestarteten Kontext abzulehnen.

Der bereits laufende Zustandsbeobachter schloss um 11:56:36 UTC ab: Betas
Kontext hat eine neue Init-Identität (PID/Startzeit `25397/3465417` statt
`5423/2926989`). Alphas Kontext ist vollständig unverändert. Image, Profil,
Boot-ID, SystemServer, CE-Zustand, Vordergrundbenutzer, Benutzermetadaten,
Paketauswahlen und Paketgruppen stimmen mit der Vorheraufnahme überein.
Im an das Image gebundenen Servicecode ruft `linuxShell` vor dem Shell-Zugang
`awaitRuntime` auf; dieser Pfad kann den Kontext starten.
**T05 ist auf diesem Image in diesem Teilfall fehlgeschlagen.**

Der lokale Beleg `shell-after-stop-failure-proof.json`, SHA-256
`0d7bb898d1596118b9504a3978a9317a843bb98f79813028c9f24e352d4d2766`,
bindet die unverändert gesicherten 706 Ereignisse, beide Zustandsaufnahmen
und die zum Image identische Servicedatei. Er enthält keine nachträgliche
Gastaktion. Frische IPC-/Dateirücklesungen und das Ende einzelner alter
Hintergrundprozesse sind für diesen zusätzlichen Fall nicht nachgewiesen.
Der technische Fehler erklärt keine gemeldete Plattform-Sicherheitswarnung;
ein solcher Zusammenhang ist nicht belegt. Rohprotokolle bleiben lokal.

Die anschließende Quellkorrektur entfernt Start und Startfortsetzung aus
`linuxShell`. Sie verlangt `READY` durch eine reine Statusabfrage unter
derselben Zugriffssperre wie `EXEC` und prüft Zulassung und Sitzungsbindung
erneut vor der Programmausführung. `linux start` behält seinen ausdrücklichen
Startablauf. Der Treiber erhält den gezielten Befehl `shell-stopped` samt
sofortiger Erkennung eines unerwarteten GNU-Prompts. Elf isolierte Hosttests
prüfen diesen Treiberablauf; drei bestehende Ereignisaufzeichnungstests
bestehen ebenfalls. Das ist noch kein ausgeführter Regressionstest des
korrigierten Android-Service. Der neue Build ist abgeschlossen; der oben
getrennt beschriebene ec01e5fa-Gastlauf bestätigt inzwischen genau den eigenen
Stopp-/Shell-Fall. Der historische b832d6c-Befund und die übrige vollständige
Restliste bleiben gültig.

## Verity-Status: expliziter Modus für die QEMU-Startvorbereitung

Die gepinnte `system/core`-Revision
`68be0c2c0006a0740d0b1809abe4717308f90d15` behandelt einen fehlenden
`androidboot.veritymode` unterschiedlich: `libfs_avb/avb_util.cpp` verwendet
beim Tabellenaufbau bereits `enforcing` mit `restart_on_corruption`, während
`fs_mgr_load_verity_state()` ohne Parameter fehlschlägt. Der Init-Aufruf
`verity_update_state` kann dann die tatsächlichen Partitionsinformationen
nicht veröffentlichen. Die vorhandenen dedd1da-Logs belegen beide Pfade.

[prepare-server-qemu.py](../scripts/prepare-server-qemu.py) übergibt deshalb
nach erfolgreicher AVB-Prüfung ausdrücklich `androidboot.veritymode=enforcing`.
Zusätzlich müssen die Flags aller sechs VBMeta-Header null sein, einschließlich
der über Footer referenzierten Boot-/Init-Boot-Header. Deaktivierte Hashtrees,
deaktivierte Verifikation und unbekannte Flags verhindern die Vorbereitung,
bevor Hilfsdisks oder Bootkonfiguration entstehen. Der Beleg hält gewünschten
Modus und gelesene Flags fest. Die Signaturprüfung bleibt bei `avbtool`.
Der Vorbereiter setzt keinen grünen Verified-Boot-Status und keinen gesperrten
Hardware-Bootloader voraus; das dokumentierte Entwicklungs-Vertrauensmodell
bleibt bestehen. Vorhandene Profile und ihre feste Bootconfig-Bindung werden
nicht verändert.

Fünf [Hosttests](../tests/test_server_qemu_prepare.py) bestehen, darunter
24 Flag-/Header-Kombinationen, fehlgeschlagene Signaturprüfung, verkürzter
Header und fehlerhafte Digest-Ausgaben. Die Fehlerfälle erzeugen keine
Hilfsdisks oder startbare Konfiguration. Diese Tests verwenden lokale
Attrappen und ersetzen keinen Gastnachweis. Die oben dokumentierte neue
ec01e5fa-Beobachtung prüft inzwischen Statusveröffentlichung und aktive
dm-verity-Mounts gemeinsam; D1 insgesamt bleibt offen.

Das Rezept `d9a0d30a48c1f745ffcc02dbcb6b4c15054e89e6` wurde anschließend
auf den unveränderten Imagebuild `ec01e5fa5f2822da5763ab54c644bc5c5c5ab413`
angewendet. Die getrennte Vorbereitung
`/srv/aegis/runs/phase1-ec01e5fa-verity0` endete mit
`LOCAL_BUILD_AVB_AND_DISK_VERIFIED_NOT_BOOTED`. Alle 20 Image-Prüfsummen,
18 Buildbelege und Kernel-/Runtime-Eingaben stimmen mit der vorherigen
Vorbereitung überein. Die offiziellen `avbtool info_image`-Ausgaben bestätigen
unabhängig für sämtliche sechs gelesenen Header Flags gleich null.
Die zusätzliche Bootkonfiguration unterscheidet sich ausschließlich durch die
neue enforcing-Zeile. Die frisch erzeugten Hilfsdisks und GPT-Basis sind eigene
Vorbereitungsartefakte; bestehende Disks oder Profile wurden nicht geändert.

Der lokale Nachweis `out/phase1-dod/verity-mode-fix/preparation-proof.json`,
SHA-256 `67619cc9cffc82208436bd57812859d112739104220eaf7a7f2a034f6275480b`,
bindet Image- und Rezeptcommit getrennt, die drei Vorbereitungsquellen,
unveränderte AOSP-Quellen aus dem Buildmanifest, Headerausgaben und Belegkopien.
Die neue `runtime.bootconfig` hat SHA-256
`dec52186fdff2e3b84678760549e0b43e9b2fc5f5740b04b82aa26b15b5fa2fe`;
`avb-checked.json` hat SHA-256
`1db6a102085dacf29927bbae57549446317c7e83351b1a93c004f24d2013013c`.
Bei dieser Vorbereitung wurde kein persistentes Profil erzeugt und kein Gast
gestartet. Der spätere Bootnachweis steht getrennt am Anfang dieses Index.

## misctrl: Exitcode-Korrektur und Buildbindung

Der unveränderte AOSP-Quelltext `bootable/recovery/bootloader_message/misctrl_main.cpp`
aus Commit `80fbea7e9af1dd883f2046e9b299d6fe45a0f693` hat SHA-256
`50ffb5d5313a18ee6968c08ad8009a514e4bc54b61cf45a3596b738cd67cbcac`.
Er übernimmt den booleschen Rückgabewert von `SetProperty` mit `res |=` in
den Exitcode. Die gepinnte libbase-Implementierung liefert bei erfolgreichem
Setzen `true`. Dadurch meldet misctrl bei ansonsten erfolgreichem Ablauf
Exit 1 und kann umgekehrt einen alleinigen Property-Fehler als Erfolg melden.

[register-misctrl.py](../scripts/aosp/register-misctrl.py) korrigiert diese
Umrechnung und protokolliert den fehlgeschlagenen Property-Aufruf ausdrücklich.
Misc-Lese-/Schreibfehler, unbekannte Seitengrößen und belegter beziehungsweise
nicht lesbarer reservierter Speicher bleiben Fehler. Die Änderung ist an
Originalcommit und Dateihash gebunden und erhält unbekannte lokale Änderungen.
Das Vollbuildrezept wendet sie vor der Kompilierung an und prüft sie danach;
`misctrl-source.json` wird bei der lokalen Imagevorbereitung mitgeführt.
Ältere Builds ohne diesen Beleg werden mit ihrem damaligen Vorbereitungsrezept
verwendet; sie enthalten die Korrektur nicht.

Neun [Hosttests](../tests/test_misctrl_status.py) bestanden. Sie kompilieren
den vollständigen unveränderten und korrigierten C++-Quelltext mit inerten
Android-Schnittstellen und reproduzieren beide vertauschten Originalergebnisse.
Die korrigierte Variante meldet Erfolg und Property-Fehler richtig; sämtliche
anderen genannten Fehlerpfade bleiben nichtnullig. 4-KiB-/16-KiB-Verhalten,
historisches Flag und ungültiger Header bleiben erhalten. Zusätzlich werden
Quellbindung, Wiederholung und Erhalt lokaler Änderungen geprüft.

Dies sind Hosttests ohne Android-Property- oder Gerätezugriff. Der anschließende
ARM64-Vollbuild aus `ec01e5fa5f2822da5763ab54c644bc5c5c5ab413` liegt unter
`/srv/aegis/runs/local-20261004T132131Z-ec01e5fa-Hp0Ctv` und endete mit
`LOCAL_BUILD_VERIFIED`. Das Protokoll enthält tatsächliche neue Kompilierung,
Linken und Installation von misctrl. Alle 20 Factory-Images bestanden die
Prüfsummenprüfung. AOSP-Manifest, Kernelbundle und Debian-Basis blieben
gegenüber dedd1da unverändert; die bestehende RAM-Prüfung bestand mit
51,9 GiB verfügbar. Bestehende VMs mussten dafür nicht gestoppt werden.

Die neue Vorbereitung `/srv/aegis/runs/phase1-ec01e5fa0` bestätigt
AVB, Kernel-/Runtime-Eingaben, 18 Buildbelege und die GPT-Basisdisk. Ihr Status
ist `LOCAL_BUILD_AVB_AND_DISK_VERIFIED_NOT_BOOTED`; ein neues Profil oder ein
Gaststart gehören nicht zu diesem Nachweis. Der AVB-Digest lautet
`8dfbc2ec43f84eb24db0fe6d5eee09cf36315a7235ec6d664f0a374899f00af6`.

Die statische Prüfung extrahiert Slot-A-Partition `system_a` aus dem tatsächlich
ausgelieferten `super.img`, ohne Mounten oder Ausführen von Android-Programmen.
`system/bin/misctrl` und seine Init-Datei sind bytegleich zum Buildprodukt.
Das ARM64-ELF enthält die neue Property-Fehlermeldung; sein SHA-256 lautet
`d2b38ad21d26ffb060f42f3f5da59cdcd14b0143bc895595ddf0c5cb2b926337`.
Quellbeleg, Image-Prüfsumme und Buildcommit sind gegeneinander abgeglichen.

Die folgenden bytegleichen Kopien der Buildbelege bleiben lokal unter
`out/phase1-dod/misctrl-status-fix/`:

| Beleg | SHA-256 |
| --- | --- |
| `build-validation.json` | `62d0962fa24e675312f2c3383a813e8cb6f93182f19acf320340a5ecc5b15a1c` |
| `misctrl-source.json` | `6f7d05667353104dc22390fcb3ed523f17bc52a65348704d817810d20709e150` |
| `misctrl-packaged-image-proof.json` | `a9836fd430647ffbd45ddf36ceb667a53725c9c908c3ae9c3f56c45b2cd0a38c` |
| `verification-summary.json` | `81cd47f726ff595ecae0ab56f4e4cb6f917d5f11892c9e00cd77b58e30493005` |

Der neue ec01e5fa-Boot belegt inzwischen Exit 0 im Gast, wie oben dokumentiert.
Historische Exit-1-Belege werden nicht nachträglich umgedeutet. Die übrigen
Dienstursachen, die Shell-Regression und D1–D7 bleiben offen. Builds, Profile
und Rohprotokolle wurden nicht hochgeladen.

## Shell-Korrektur dedd1da: gebaut und erster Systembenutzer-Boot belegt

Quellcommit: `dedd1dabc45efe080a499b3657b7ee09d2fda5a7`.
Der lokale ARM64-Vollbuild
`/srv/aegis/runs/local-20261004T121842Z-dedd1dab-OeHisa`
ist mit `LOCAL_BUILD_VERIFIED` abgeschlossen; alle 20 Factory-Images haben
die Prüfsummenprüfung bestanden. Das AOSP-Manifest bleibt bytegleich zur
b832d6c-Basis. Kernelbundle und Debian-Basisgeneration sind dieselben
geprüften Eingaben aus dem [Buildinventar](phase-1-build-inventory.md).

Die getrennte Vorbereitung `/srv/aegis/runs/phase1-dedd1da0` erhielt vor dem
ersten Start den historischen Status `LOCAL_BUILD_AVB_AND_DISK_VERIFIED_NOT_BOOTED`.
Dieser Vorbereitungsbeleg bleibt unverändert. Der lokale Beleg
`out/phase1-dod/shell-ready-fix/build-validation.json`, SHA-256
`94d0821653525002b43d7cf2ebafd98c3c84d2933a95d5d79547772f68aab38b`,
bindet die 20 Images, 17 Buildbelege, Kernel- und Runtime-Auswahl.
AVB-Digest: `a28782c74fbce9dc3790ec70cc0f34c23c27f0d5465d1fe6a3ac42fdc77c0977`.

Zusätzlich wurde die Service-JAR aus `system_ext_a` des tatsächlich
ausgelieferten `super.img` extrahiert und statisch disassembliert.
Die kompilierte Shell-Methode enthält Statusprüfung, Zulassungsprüfung und
`EXEC`, aber keinen direkten `START`- oder `awaitRuntime`-Aufruf.
Der Quellbeleg passt zum aktuellen Servicecode; der Imagehash passt zur
Vorbereitung. `shell-packaged-dex-proof.json`, SHA-256
`04b5c54da6dc241a1c8cce427c2899b0a2eec7c478200e9b01573a3738205cbd`,
liegt ebenfalls unter `out/phase1-dod/shell-ready-fix/`. Ein erster lokaler
Textabgleich scheiterte an der Schreibweise von DEX-Klassendeskriptoren;
die Korrektur betraf nur den Prüfer, nicht Produktcode oder Gastzustand.

Der erste Buildlauf `local-20261004T121456Z-dedd1dab-6qJLOT` bleibt als
`FAILED` erhalten: vor der Kompilierung waren 45,5 statt der verlangten
48 GiB verfügbar. Die Speichergrenze wurde nicht verändert. Die bestehende
b832d6c-Konsole wurde geschlossen und ihr VM-Paar geordnet heruntergefahren:
Android-Powerdown, danach `AEGIS_HELPER_SHUTDOWN_CLEAN`, Dienst inaktiv.
Das Profilmanifest blieb unverändert, der ursprüngliche Testtreiber mit
707 Ereignissen blieb erhalten. Der Shutdown-Beleg
`prebuild-shutdown.json` hat SHA-256
`402a922018f4f50622472ac209578cb9571bc5774a7f9cd2162c1db454945f75`.
Mit 51,3 GiB verfügbarem RAM bestand der zweite Lauf die unveränderte Prüfung.

### Erster Boot im frischen Profil

Die vorhandene Aufnahme vom 4. Oktober 2026, 13:05:37 UTC bestätigt
`sys.boot_completed=1`, authentifiziert erreichbares ADB mit `ro.adb.secure=1`,
SELinux Enforcing und FBE-Eigenschaften `encrypted`/`file`. Ausschließlich
Systembenutzer 0 ist angelegt und CE-entsperrt. Der Identity-Service ist
registriert; die CLI meldet eine nicht authentifizierte Sitzung und verfügbare
Ersteinrichtung. Es wurden dabei keine persönlichen Benutzer angelegt und
keine Passwort- oder benutzerübergreifenden Prüfungen ausgeführt.

- Profil: `0282f0b9-a8c4-4dfe-825c-2b230175ee76`.
- Boot: `7f9d7648-d86d-49a7-a246-cae2031de204`.
- Beobachteter SystemServer: PID `1310`, Startzeit `39855`.
- Beleg: `out/phase1-dod/dedd1da-base/baseline.json`, SHA-256
  `af6f61bd7e9d4eee1f4b9fa4fd045d8a7d86ca0837621be28e8c26ad2013d540`.
- Bildschirm: `out/phase1-dod/dedd1da-base/boot-1/screen-boot-complete.png`,
  SHA-256 `8cf1ffea418fde7e1c7bb17251db2313e1e4012b39e4286652cc171cb63fa4d0`.

Der beobachtete `ro.boot.vbmeta.digest` stimmt mit der geprüften Vorbereitung
überein. Die Eigenschaften `ro.boot.verifiedbootstate`,
`ro.boot.vbmeta.device_state` und `ro.boot.veritymode` sind jedoch leer.
Aus dem Digest allein wird daher keine vollständige Durchsetzung des
Integritätsschutzes im laufenden Gast abgeleitet; der D1-Nachweis bleibt nötig.
Die Bildschirmaufnahme zeigt Hintergrund und Batterieanzeige, belegt aber
noch keine funktionierende Eingabe. Die Zustandsaufnahme ist kein Nachweis
langfristiger Dienststabilität oder eines vollständigen Framework-Verlaufs.

### Offline-Prüfung der vorhandenen ersten Bootlogs

Am 4. Oktober um 13:26 UTC wurden die vom Launcher bereits geschriebenen
Android-/Logcat-Dateien unverändert als lokale Präfixkopien gesichert. Dafür
wurden keine weiteren Gastbefehle ausgeführt. Die vorherige Zustandsaufnahme
von 13:05 UTC ist gebunden, aber keine zeitgleiche neue Prozessbeobachtung.

Der unveränderte Dienstprüfer findet einen SystemServer-Start mit PID 1310,
keine Treffer seiner sechs Fatal-/ANR-Kategorien und neun Signal-Beendigungen
mit unmittelbar vorausgehender passender Stop-/Restart-Anweisung: odsign,
hwservicemanager, adbd und sechs idmap2d-Vorgänge. Die drei nichtnulligen
Rückgaben bleiben `recovery-refresh=254`, `system_aconfigd_mainline_init=1`
und `misctrl=1`. Der System-Initializer meldet ausdrücklich das Überspringen;
der tatsächlich zuständige Mainline-Initializer endet mit 0. Der alte
VirtualizationService-Aufräumhelfer endet auf diesem frischen Profil mit 0;
dies erklärt den früheren Fehler auf dem anderen Profil nicht.

Zusätzlich meldet der Prüfer eine rückläufige Zeitmarke. Die konkrete Stelle
sind zwei EXT4-Mountmeldungen verschiedener Tasks, T623 und T622, in der
Reihenfolge `216.215320` und `216.215319`: eine Mikrosekunde Differenz und
keine Init-Dienstmeldung. Der ursprüngliche Befund bleibt erhalten. Wegen
dieser Meldung und der drei nichtnulligen Exits lautet das Ergebnis weiterhin
`REVIEW_REQUIRED` mit Exitcode 2; die Logs werden nicht umsortiert oder bereinigt.

Die Integritätslektüre ordnet acht erzeugte dm-verity-Tabellen den erfolgreichen
Mounts von system, system_ext, product, vendor, odm und ihren drei dlkm-Partitionen
zu. Die Tabellen enthalten `restart_on_corruption`. Fehlgeschlagene Versuche,
unsignierte eigenständige Footer zu laden, und der anschließende Rückgriff auf
die vorhandenen Hashtrees für system/system_dlkm bleiben sichtbar. Der gepinnte
`system/core`-Code liefert ohne Bootparameter `veritymode` beim Statuslesen
`false`; der Parameter fehlt sowohl in der Vendor-Bootkonfiguration als auch
in Zusatzkonfiguration und Kommandozeile. Die Launcherquellen stimmen mit
dem Imagecommit überein und setzen diese Eingaben zusammen. Dies erklärt
den protokollierten `fs_mgr_load_verity_state()`-Fehler, ersetzt aber weder eine
aktuelle Device-Mapper-Abfrage noch einen vollständigen Integritätsnachweis.

Lokale Belege unter `out/phase1-dod/dedd1da-base/offline-boot-review/`:

| Beleg | SHA-256 |
| --- | --- |
| `capture.json` | `67237bbb5ccad434db18fcff77291c45a739f683ceae59d302d015897929b5d7` |
| `service-audit.json` | `595b7ff516d7ede79c08fb62e5e675033760dc525311b23d0d8e13c8388d700c` |
| `integrity-and-clock-review-v2.json` | `0b788814dc9a18e78af6f1be6cfae5557b2402faff15958ee826f3179ea438d5` |

### Recovery-Refresh: Rückgabecode eingegrenzt, Ursache weiter offen

Die zusätzliche Quellprüfung bindet vier unveränderte AOSP-Dateien an das
Manifest des dedd1da-Builds: Recovery-Commit
`80fbea7e9af1dd883f2046e9b299d6fe45a0f693` und Logging-Commit
`d78b713380007d3c0dde14712cbcbec27f491ad9`. `recovery-refresh` liest gespeicherte
Recovery-Logs über `__android_log_pmsg_file_read` und gibt einen negativen
Bibliothekswert unmittelbar aus `main` zurück. `-ENOENT` entspricht dabei
dem beobachteten Prozessstatus 254.

Dieser Status beweist jedoch **keinen harmlosen leeren Erststart**:
`PmsgRead` öffnet `/sys/fs/pstore/pmsg-ramoops-0` und liefert bei Fehlern
`-errno`. Die aufrufende Leseschleife verwirft ihren abschließenden negativen
Rückgabewert. Wenn kein passender Eintrag den Ergebniswert verändert, liefert
die Funktion anschließend `-ENOENT`. Fehlende passende Logs, eine fehlende
Quelldatei und andere Lesefehler können somit zum selben Ergebnis führen.
Zusätzlich reicht der Rotationscallback Fehler beim Schreiben weiter.

Der historische Zustand von pstore ist damit nicht nachgewiesen. Für die
Ursachenklärung fehlen Beobachtungen von Backend, Mount und verfügbaren
Quell-/Zielpfaden zur betreffenden Bootphase; gegebenenfalls sind getrennte
Diagnosen des Lese- und Callbackfehlers nötig. Exit 254 bleibt prüfpflichtig.
Es wurden weder Gastbefehle ausgeführt noch Dienstverhalten oder Fehlerfilter
verändert. D1 bleibt offen.

Der lokale Beleg mit Quellkopien und Prüfsummen liegt unter
`out/phase1-dod/dedd1da-base/offline-boot-review/recovery-refresh-source/review.json`,
SHA-256 `b36c8a88a74f3d69f294f917bcc061e37dd80f44f0f69e7ad3e484365dade8f9`.

**Build, statische Imagevalidierung und dieser erste Boot sind Teilnachweise.**
`shell-stopped` sowie der anschließende ausdrückliche Start und echte
GNU-Zugang müssen auf dem neuen Image ausgeführt werden. Keine bestandene
b832d6c-Variante wird automatisch übertragen; D1–D7 und die übrigen offenen
Pflichtfälle bleiben offen. Images, Profildateien und Rohprotokolle wurden
nicht nach GitHub geladen.

## Aktueller Lauf b832d6c: Paketautorisierung

Der aktuelle Image-Commit ist
`b832d6c077baeee4324e00d00dc3618372f3e9d9`, das Profil
`1943dcb7-d438-48de-8e62-d9967b32b9b2`. Seine Nachweise liegen unter
`out/phase1-dod/b832d6c-base/`. Die folgende Übersicht betrifft ausschließlich
T13 auf diesem Image; sie übernimmt keine bestandenen Varianten aus älteren
Images. T13 als Ganzes sowie D1–D7 bleiben offen.

| Aktion/Bereich | Gültige Freigabe und Ausführung | Ohne Adminauswahl | Falsches Adminpasswort | Nicht-Adminfreigabe |
| --- | --- | --- | --- | --- |
| `install all` | belegt | belegt | belegt | belegt |
| `install user` | belegt für Alpha und Beta | belegt | belegt | belegt |
| `update all` | belegt, beide Benutzer aktiviert; private u3 bleibt | belegt | belegt | belegt |
| `update user` | belegt für Beta mit Alpha-Adminfreigabe | belegt | belegt | belegt |
| `remove all` | belegt, beide Benutzer aktiviert | offen | offen | offen |
| `remove user` | belegt für Alpha mit Versionswechsel und Beta bei gleicher gemeinsamer Version | offen | offen | offen |

Die gültigen Varianten sind an `shared-u3-activation-proof.json`,
`private-u4-activation-proof.json`, `common-update-both-proof.json`,
`personal-update-active-proof.json`, `shared-remove-both-proof.json`,
`private-remove-fallback-proof.json` und `private-remove-same-active-proof.json`
gebunden; die jeweiligen vollständigen
Prüfsummen und Grenzen stehen in den b832d6c-Abschnitten unten. Die drei
Installationsablehnungen bindet `install-all-denials-proof.json`, SHA-256
`a1f4ec9b17bebf4a8d343545ddf8bea70c22d59d16d51ca1ec40b71e1c6ed915`.
Die drei persönlichen Installationsablehnungen bindet
`install-user-denials-proof.json`, SHA-256
`bb052a68caf3fe05bfa0d61c76439ca976ac79385ed2ff6ce5ad978778e7b839`.
Die sechs Update-Ablehnungen bindet `update-denials-proof.json`, SHA-256
`bcebb573e054eb898396b122b087325f69ccb6d08d88b85556f6c3b7aaf5df35`.
Betas gültige private Installation mit Alpha als abweichendem Administrator
ist zusätzlich an `install-user-beta-activation-proof.json` gebunden, SHA-256
`e03a29d00213a56fe3b280e5159f59e9aa99489881a18852d623587c5d6008ba`.
Alphas anschließender Datenvergleich und drei konkrete verweigerte GNU-Zugriffe
auf Betas nun vorhandenen privaten Paketspeicher sind ebenfalls belegt. Die
übrigen T13-Varianten bleiben offen.

### Vorbereitung des gemeinsamen Updates bei privater jq-Version

Am 4. Oktober wurde im laufenden b832d6c-Profil um 08:11:44 UTC erneut
`jq=1.7.1-6+deb13u3` gemeinsam veröffentlicht. Beta beantragte die Installation,
Alpha erteilte die reguläre Adminfreigabe. Der Plan enthielt genau diese eine
Installation. Die neue gemeinsame Generation lautet
`85ccb5402fed0ba7dfdb185b7e9ad136bdb4f8d6fc601e212d7edf2766a1ee73`.

Der vollständige Zustandsvergleich bestätigt unveränderte laufende Kontexte:
Alpha besitzt weiterhin seine 78 Basispakete, Beta 81 Pakete einschließlich
privatem jq/libjq1 u3. Beide Prozessidentitäten mit Startzeiten, Namespaces,
Mounts, Paketdatensätze und privaten Auswahlen stimmen mit dem Zustand vor
der Veröffentlichung überein. Boot, SystemServer, CE-Zustand, Benutzer und
Vordergrund bleiben gleich; kein Paketauftrag ist mehr aktiv. Betas CLI meldet
die ausstehende Aktivierung. Es wurde noch kein Kontext dafür neu gestartet.

`common-update-preparation-proof.json`, SHA-256
`4d303bb6be910f73cd64359a70f6126fced88214d1267eb9ddca8e2933286d7c`,
bindet die 492 Ereignisse, den unveränderten vorherigen Ereignispräfix und den
Zustandsbeleg `common-update-preparation-published.json`, SHA-256
`0560fa704e740c773629bc503db9b06b6980e11ee21dcb26c47eff1a1498fc3a`.
Dies ist der Vorbereitungsnachweis für den anschließend unten belegten
gemeinsamen Update- und Aktivierungsfall mit erhaltener privater u3-Auswahl.
Ein neuer Fortschrittsnachweis der zeitlich begrenzten Hintergrundproben wird
hier nicht behauptet. Alle Belege verbleiben lokal unter dem oben genannten
Verzeichnis; Git enthält ausschließlich diese Ergebnisbeschreibung.

### Gemeinsames Update bei beiden Benutzern, private u3 bleibt erhalten

Beta beantragt `linux package update --scope all`; Alpha bestätigt den Plan
über die reguläre AOSP-Adminprüfung. Der Plan enthält genau fünf Änderungen:
jq/libjq1 von `1.7.1-6+deb13u3` auf u4, libpcre2-8-0 von
`10.46-1~deb13u2` auf u3 sowie libssl3t64 und openssl-provider-legacy von
`3.5.7-1~deb13u2` auf u3. Die Veröffentlichung gelingt um 08:24:04 UTC.
Gemeinsame Generation:
`a9e63e88614b4ddbce55c7710410e3bb1cb00770daed7dc18f21787a6e0489b4`.

Beide bisherigen Kontexte bleiben nach Veröffentlichung vollständig unverändert,
einschließlich Prozessidentität, Namespaces, Mounts und Paketmetadaten. Ihre
gewöhnlichen GNU-Shells bestätigen die jeweiligen alten Bestände; die CLI
meldet die ausstehende Aktivierung. Beta stoppt zuerst ausschließlich seinen
Kontext. Der erste neue Start gelingt um 08:38:28 UTC und aktiviert die neuen
Basisbibliotheken, während die ausdrückliche private jq/libjq-u3-Auswahl
erhalten bleibt. Anschließend stoppt Alpha seinen eigenen Kontext; dessen
erster neuer Start gelingt um 08:58:24 UTC und übernimmt jq/libjq u4 bei
weiterhin leerer privater Versionsauswahl.

Beide GNU-Shells führen jq tatsächlich aus und prüfen Paketversionen,
Bibliotheks-Hashes, Integrität der drei jq-Pakete sowie 79 manuelle und zwei automatische
Pakete. Der vollständige Metadatenvergleich bestätigt jeweils 81 installierte
Pakete. Beide ursprünglichen Dateien und Einstellungen bleiben bytegleich.
Betas neuer Kontext `14330/2308029` bleibt über Alphas Stopp/Start unverändert;
sein Hintergrundprozess `14535/2316113` macht davor und danach Fortschritt.
Alpha verwendet anschließend Kontext `17998/2427632`. Boot, SystemServer und
CE-Zustand bleiben erhalten; beide privaten Generationen binden an die neue
gemeinsame Basis. Beide Benutzer melden nach ihrem Start `packages=current`.

Der Veröffentlichungsbeleg `common-update-publication-proof.json` hat SHA-256
`5729cdea758555d66144ab9133cd5ccd272374c56aad416ac2dd7ae895b8d40c`.
Betas erste Aktivierung bindet `common-update-beta-proof.json`, SHA-256
`29282efc002f9f5e325595f7b62c9ade3329f82442f06f1399e3662d17655e68`.
Der vollständige Beleg `common-update-both-proof.json`, SHA-256
`07eba4383de20c49df82095772f91a0b393841eeefa7bef03f7c3f842f5fc1d3`,
bindet 547 Ereignisse, die unveränderten vorherigen Präfixe, Quellzuordnung,
beide genauen Prozessenden und alle Zustandsaufnahmen. Die abschließende
Aufnahme `common-update-both-active.json` hat SHA-256
`ace38c0b833881dbe49cddea7ef8498292df2ab37442d83d790e615a6c732ccc`.

Damit sind die gültige gemeinsame Updatevariante und dieser konkrete T16-Fall
mit weiterhin unterschiedlicher gemeinsamer/privater Version belegt.
Private Updates, Konflikte und Transaktionsfehler/Parallelität bleiben eigene
Pflichtfälle. Alte temporäre Proben waren schon vorher abwesend und belegen
hier keine neue Reinigung; das Ende zeitlich begrenzter alter Hintergrundjobs
wird nicht dem späteren Runtime-Stopp zugeschrieben. Die gesonderte Prüfung
des jeweiligen ursprünglichen Init-Prozesses bleibt davon unabhängig.

### Persönliches Update und neue flüchtige Beta-Proben

Beta beantragt `linux package update --scope user`; der Plan enthält genau
seine persönlichen jq/libjq1-Änderungen von `1.7.1-6+deb13u3` auf u4.
Alpha erteilt die reguläre AOSP-Adminfreigabe. Die Veröffentlichung gelingt
am 4. Oktober um 09:20:48 UTC. Gemeinsame Auswahl, Alphas private Auswahl und
beide laufenden Kontexte bleiben vollständig unverändert. Betas CLI zeigt
die ausstehende Aktivierung; seine GNU-Shell führt zunächst weiterhin u3 aus.

Für die flüchtige Datenprüfung legt Beta vor dem Update zwei neue synthetische
Dateien exklusiv unter `/tmp` und `/run/user/1000` an, jeweils UID/GID 1000 und
Modus 0600. Beide werden nach Veröffentlichung um 09:23:44 UTC nochmals mit
unveränderten Bytes gelesen. Erst danach beendet der reguläre eigene
`linux stop` Betas bisherigen Init `14330/2308029` und Hintergrundprozess
`14535/2316113`. AOSP-Sitzung und tatsächlicher CE-Zustand bleiben erhalten.
Alphas ursprünglicher Hintergrundprozess `18313/2439313` macht vor/nach dem
Stopp und nach Betas neuer Aktivierung mit unveränderter Identität Fortschritt.
Die verwendeten Prozessproben sind in diesem Fall nachweislich noch nicht
zeitlich abgelaufen.

Betas erster neuer Start gelingt um 09:27:37 UTC. Die gewöhnliche GNU-Prüfung
führt jq u4 tatsächlich aus, prüft libjq1 u4 samt Bibliotheks-Hash, die Integrität
der drei jq-Pakete und die unveränderten Paketmarkierungen. Alle 81 installierten
Paketdatensätze entsprechen dem erwarteten Bestand; ausschließlich jq und
libjq1 haben sich gegenüber Betas altem Kontext geändert. Seine private
Versionsauswahl lautet nun ausdrücklich jq u4 und gehört weiterhin `11/11`.
Die neue private Generation
`d58d80f46af2ca24fc910ea7abaa4317ae0a029dc2de54233cdac6ad3146d45f`
bindet an die unveränderte gemeinsame Generation `a9e63e88614b4ddbce55c7710410e3bb1cb00770daed7dc18f21787a6e0489b4`.

Die ursprüngliche persönliche Datei und Konfiguration bleiben bytegleich.
Beide frisch angelegten flüchtigen Dateien fehlen nach dem Kontextneustart.
Der neue Beta-Kontext `25084/2602870` meldet `packages=current`; Alphas
vollständiger Kontext `17998/2427632`, Boot und SystemServer bleiben gleich.
Beide persönlichen CE-Speicher waren bereits entsperrt; dieser Fall behauptet
keine zusätzliche Prüfung mit gesperrtem Administrator.

`personal-update-publication-proof.json`, SHA-256
`00d2181aaae51dd5c59b29cb01e258777d0d23d37272aabf1f29101bd504c497`,
bindet die Veröffentlichung. Der vollständige Beleg
`personal-update-active-proof.json`, SHA-256
`eb76073b9788d7f1b8139bb1b0b4d59419bdcda47ac99ad6d86a43e8419af3c1`,
bindet 587 Ereignisse, deren unveränderte vorherige Präfixe, alle
Zustandsaufnahmen, die Quellzuordnung der privaten Auswahl und die frischen
flüchtigen Proben. `personal-update-active.json` hat SHA-256
`4278ddfcac4c436eca82c6df9662c983c65eceaa387cd70a79732c88fc3f3fc5`.
Dies belegt das gültige persönliche Update sowie den konkreten Beta-Stopp
mit aktivem Alpha und neuer `/tmp`-/`/run`-Bereinigungsprüfung. Es ersetzt keine
fehlenden IPC-, Logoutfehler-, Konflikt- oder Parallelitätsfälle.

### Reiner Alpha-Runtime-Neustart mit frischen flüchtigen Dateien

Im Anschluss an Betas persönliches Update meldet sich Alpha regulär an und
bestätigt aus seiner GNU-Shell denselben u4-Paketbestand. Er legt zwei neue
synthetische Dateien exklusiv unter `/tmp` und `/run/user/1000` an, liest ihre
Bytes und prüft UID/GID 1000 sowie Modus 0600. Der vollständige Ausgangsvergleich
zeigt gegenüber dem letzten Zwei-Kontext-Nachweis lediglich den erwarteten
Vordergrundwechsel von Beta zu Alpha.

Der reguläre eigene `linux stop` gelingt um 09:40:44 UTC. Alphas ursprünglicher
Init `17998/2427632` und Hintergrundprozess `18313/2439313` sind anschließend
nachweislich beendet, sein Kontext fehlt. AOSP-Sitzung, Vordergrund und
CE-Freigabe bleiben erhalten. Betas Kontext `25084/2602870` bleibt vollständig
unverändert; sein Hintergrundprozess `25379/2613708` macht vor/nach dem Stopp
und nach Alphas neuem Start mit derselben Identität Fortschritt. Die Proben
liegen innerhalb ihrer zulässigen Beobachtungsdauer.

Alphas erster neuer Start gelingt um 09:43:33 UTC. GNU-Ausführung, jq/libjq u4,
Bibliotheks-Hash und Paketmarkierungen stimmen. Beide neuen flüchtigen Dateien
sind abwesend, während die ursprüngliche persönliche Datei und Konfiguration
bytegleich erhalten bleiben. Alle 81 installierten Pakete, die vollständige
dpkg-Prüfsumme, private Auswahl und zugrunde liegendes Paketimage bleiben identisch. Gemeinsame
und persönliche ausgewählte Generationen ändern sich für keinen Benutzer;
es findet keine Paketaktion statt. Der Abschlussstatus ist `packages=current`.

`alpha-restart-active-proof.json`, SHA-256
`cbdacd18d92b0da5f473120b0521012b0c65056c543f0d542d8062ee52a23cea`,
bindet 618 Ereignisse mit unverändertem vorherigem Präfix, die genauen
Prozessenden, frischen Dateiproben und vollständigen Vorher-/Nachher-Aufnahmen.
Die abschließende Aufnahme `alpha-restart-active.json` hat SHA-256
`797682166c1bc0c736c84ad31641b9ff3cea1788a51dd71496de7c4bd88b2833`.
Zusammen mit Betas persönlichem Updatefall sind eigene Stopps mit aktivem
Gegenüber und frische `/tmp`-/`/run`-Bereinigungsprüfungen in beiden Richtungen
belegt. Daraus folgt keine pauschale IPC-, Logoutfehler- oder Parallelitätsabnahme.

### Persönliche Auswahl entfernen, gemeinsame Version bereits gleich

Beta beantragt am 4. Oktober `linux package remove --scope user jq`.
Der Plan um 10:12:29 UTC nennt ausdrücklich die Aufhebung der persönlichen
Versionswahl `1.7.1-6+deb13u4`, die anschließende Verwendung derselben gemeinsamen
Version und das Ausbleiben jeder Paketversionsänderung. Alpha erteilt um
10:13:03 die reguläre AOSP-Adminfreigabe; Veröffentlichung folgt um 10:14:48.
Die ausgewählte persönliche Generation wechselt von `d58d80f4…` auf
`53ff62034ff54335dc367255f127f1dd27f4df19746316be1cc76bedca047948`.
Gemeinsame Generation, Alphas Auswahl und beide laufenden Kontexte bleiben
unverändert. Beta meldet die ausstehende Aktivierung und führt im alten
Kontext weiterhin jq/libjq u4 mit konsistentem Paketbestand aus.

Der reguläre eigene Stopp um 10:18:48 erhält Betas Sitzung und CE-Freigabe.
Die genauen alten Init-/Hintergrundidentitäten sind anschließend beendet;
Alphas ursprünglicher Hintergrundprozess schreitet davor und danach fort.
Der erste neue Start um 10:21:38 gelingt. GNU-Prüfungen um 10:22:12 bestätigen
jq/libjq u4, libonig, Bibliotheksprüfsumme, sämtliche 81 installierten
Paketnamen, 79 manuelle Wurzeln und zwei automatische Abhängigkeiten sowie
tatsächliche jq-Ausführung als UID/GID 1000. Originaldatei und ursprüngliche
Konfiguration werden bytegleich zurückgelesen. Der Endzustand enthält eine
leere persönliche Auswahlliste, dieselbe Paketdatenbank-Prüfsumme und dieselben
81 Paketversionen wie zuvor. Die CLI meldet `packages=current`.

Boot, SystemServer, Benutzer und CE-Zustand bleiben erhalten. Alphas vollständiger
Kontext stimmt mit dem Ausgangszustand überein; derselbe Hintergrundprozess
schreitet auch nach Betas erstem neuen Start fort. Der Prüfer
`record-private-remove-same-active.py` bindet diese Zustände, die regulären
CLI-/GNU-Ereignisse und fünf zum Image identische Quelldateien. Er bestätigt
den unveränderten bisherigen Ereignispräfix und 654 Ereignisse insgesamt;
im neuen Abschnitt gibt es keinen Testtreiberfehler.

Lokale Belege unter `out/phase1-dod/b832d6c-base/`:

| Nachweis | SHA-256 |
| --- | --- |
| `private-remove-same-active-proof.json` | `171b84e65c750dba0e8eb7178c7391778098528f7d74a2ca700af630d33cdfa8` |
| `private-remove-same-active.json` | `fd0741e73890e31cd3ba4af2fbb248452a90cc9512218e26555db96b62b83654` |
| `private-remove-same-retired.json` | `a1d1a8f0969706ddc59d4e73ea5894c8e578b753b6d0da597e4da27ccf9a7bbb` |

Dies schließt den konkreten T15-Fall der Entfernung bei gleicher gemeinsamer
Version. Beide Benutzer waren bereits CE-entsperrt; eine Freigabe mit gesperrtem
Admin wird hier nicht behauptet. Die alten flüchtigen Proben wurden nicht neu
angelegt und begründen keinen zusätzlichen Bereinigungsnachweis. Die sechs
Entfernungsablehnungen sowie offene Versions-, Konflikt- und Parallelitätsfälle
bleiben erforderlich. T13/T15 und D1–D7 sind damit nicht insgesamt abgenommen.

### Gemeinsame Bash, unterschiedliche Einstellungen am selben HOME-Pfad

Am 4. Oktober legt Beta um 10:34:24 UTC und Alpha um 10:35:41 jeweils eine neue
Datei `/home/user/.config/aegis-shared-bash-20261004T1035.conf` im eigenen HOME
an. Exklusive Erstellung verhindert Überschreiben; beide Dateien gehören
UID/GID 1000 mit Modus 0600. Sie enthalten verschiedene synthetische Werte
derselben Bash-Einstellung. Eine neue nichtinteraktive Bash lädt die jeweilige
Datei über `BASH_ENV` und gibt den tatsächlich angewendeten Wert aus.

Beta lädt nach dem Rückwechsel um 10:37:08 wieder seinen ursprünglichen Wert,
Alpha um 10:38:32 ebenfalls. In allen vier Ausführungen stimmen Dateiinhalt,
Prüfsumme und angewendeter Wert. Die gemeinsame Bash-Version ist
`5.2.37-2+b10`; ihre identische Dateiprüfsumme lautet
`ccbd5106945d1095474e5ee47317266f5918aa37fc671829192a007918b1413a`.
Beide vollständigen Kontexte einschließlich Init-Startzeiten, Namespaces,
UID-Mappings, Paketdatenbanken und Auswahlen bleiben unverändert. Geändert
hat sich der Vordergrund von Beta zu Alpha. Boot, SystemServer und CE-Zustand
bleiben gleich; es gab keinen Runtime-Neustart oder Paketauftrag.

`shared-bash-config-proof.json`, SHA-256
`c0eb58f69b5935045660f4a44ac0360dbe6815af37ad1577523babb9004160da`,
bindet die vier tatsächlichen Ausführungen und den Zustandsbeleg
`shared-bash-config-active.json`, SHA-256
`009eb14adcabd980345a839787375a326a34c5373dbb5303e53108a23c3f6cf0`.
Die Ereignisfolge umfasst nun 678 Einträge; der vorherige Präfix ist unverändert.
Das ist ein regulärer Konfigurationsfall aus den eigenen GNU-Kontexten. Daraus
folgt kein neuer Fremdzugriffs-, Logout- oder Passworttransportnachweis.

### Variantenabgleich für Identität, Persistenz und Paketverhalten

Der lokale Prüfer `audit-current-variants.py` gleicht die folgenden Anforderungen
der unveränderten DoD und des Entwicklerauftrags mit tatsächlichen GNU-Ausgaben,
Vorher-/Nachher-Zuständen und ihren Hashbindungen ab. Alle genannten Ergebnisse
gehören zum aktuellen b832d6c-Image und demselben Profil. Die Zuordnung vorhandener
Belege ist keine erneute Ausführung ihrer historischen Testläufe.

| Pflichtvariante | Geprüfter Nachweis und Ergebnis |
| --- | --- |
| T03 interne Identität und Host-Mapping | GNU-Ausgaben nach dem gepaarten Reboot enthalten UID/GID 1000 und vollständige UID-/GID-Maps beider Benutzer. Alle 1002 technischen/normalen Kennungen je Benutzer sind überschneidungsfrei; Host-Root und Host-UID 1000 sind nicht gemappt. |
| T03 Namespaces und Wechsel | Sechs Namespace-Identitäten unterscheiden sich zwischen Alpha/Beta. Alphas Kontext bleibt beim Wechsel zu Beta identisch; die neuen Konfigurationswechsel erhalten beide vollständigen Kontexte. |
| T07 persistente Dateien und Einstellungen | Ursprüngliche, vor dem Reboot angelegte Dateien und Konfigurationen werden nach Runtime-/VM-Neustarts bytegleich aus den eigenen GNU-Kontexten gelesen. Die Werte beider Benutzer bleiben verschieden. |
| T07 private Pakete | Vor/nach VM-Neustart stimmen Generationen, Paketdatenbanken, private Auswahl und alle installierten Versionen überein; private u4 und gemeinsame u3 werden ausgeführt. Betas private jq/libjq u3 bleibt zusätzlich über den Runtime-Neustart beim gemeinsamen Update erhalten. |
| T07 flüchtige Daten | Frische Alpha- und Beta-Dateien unter `/tmp` und `/run` sind vor dem jeweiligen Stopp positiv gelesen, nach dem neuen Start abwesend. Alphas reiner Neustart erhält Paketdatenbank und Generation vollständig. |
| T14 gemeinsame Software | Bestehende Benutzer sowie nachträglich angelegter Gamma führen die gemeinsame Version aus. Für Gamma gilt weiterhin das dokumentierte AOSP-Limit; drei gleichzeitig laufende persönliche Benutzer werden nicht behauptet. |
| T14 persönliche Änderungen | Alpha verwendet private u4, Beta weiterhin gemeinsame u3. Betas späteres persönliches Update verändert weder gemeinsame Auswahl noch Alphas laufenden Kontext; die neue private Version wird tatsächlich ausgeführt. |
| T14 Einstellungen gemeinsamer Programme | Die oben beschriebene identische Bash lädt verschiedene Einstellungen am selben HOME-Pfad; beide Werte bleiben nach Rückwechsel erhalten. |
| T16 erfolgreiche Veröffentlichung | Das gemeinsame Update ändert zunächst keine laufenden Kontexte oder persönlichen Auswahlen; die CLI zeigt ausstehende Aktivierung. |
| T16 erfolgreiche Aktivierung | Beide ersten neuen Starts sind konsistent. Alpha führt gemeinsame u4 aus, Beta behält private u3 mit passenden Abhängigkeiten; beide Generationen binden die neue gemeinsame Basis. |

Damit sind **T03, T07 und T14 belegt**. Die erfolgreiche T16-Kette ist vollständig
zugeordnet; geforderte Konfliktfälle bleiben offen. Fehlende Startvoraussetzungen
(T04), Passwortwechsel (T02), Logoutfehler und Löschung (T10–T12), übrige
Paketautorisierung (T13), unauflösbare Versionen (T15) sowie Fehler/Parallelität
(T17) werden durch diesen Abgleich nicht geschlossen. D1–D7 bleiben offen.

`current-variant-audit.json`, SHA-256
`e04258b81dbf1fe1feb624d01f91820fb3e9502d5ac9195c916f23c74e356aa9`,
bindet 86 lokale Dateien und die zum Image identischen Mapping-/Buildquellen.
Für Reboot und Gamma verwendet der Audit ausdrücklich die bereits dokumentierte
abgeleitete Ereignisdatei mit dem separat erhaltenen Checkpoint für Ereignis 203.
Die Originaldateien bleiben unverändert. Zwei lokale Prüferversuche sind als
`current-variant-audit-first-attempt-failure.json` und
`current-variant-audit-second-attempt-failure.json` erhalten: zunächst eine falsche
Auswahl der historischen Ereignisquelle, danach eine falsche Annahme über die
Struktur einer Zusammenfassung. Die Berichtigung verwendet die ursprünglich
gebundene Ereignisdatei beziehungsweise den bereits hashgebundenen Zustandsbeleg.
Keiner dieser Auditfehler führte eine Gastaktion aus oder änderte einen Testbefund.

## Bisheriger Referenzlauf und Statusregeln

- Produkt-/Image-Commit: `209278def7d5bc5612eeb397bdd8ee20ccb16d86`.
- Profil: `2366ca04-d587-4170-8c56-a63c8a8e1774`.
- Erster Boot: `44dbff5f-3a76-4e97-9334-beb437fcd461`.
- Zweiter Boot: `2816650c-96bf-4db0-84e6-f169f9dfada9`.
- Dritter Boot: `e720d2bf-faef-4af8-87b9-709112eb3c41`.
- Alpha: AOSP-Benutzer/Seriennummer `10/10`, Administrator.
- Beta: AOSP-Benutzer/Seriennummer `11/11`, normaler Benutzer.
- Alle unten bezeichneten Belege liegen lokal unter
  `out/phase1-dod/209278de/`. Die vollständigen Logs liegen zusätzlich unter
  `/srv/aegis/runs/phase1-209278de/`; Belege binden deren Pfade und Prüfsummen.

**Bestanden** bezieht sich nur auf die ausdrücklich beschriebene Variante.
**Teilbelegt** benennt eine konkrete verbleibende Lücke. **Offen** bedeutet,
dass der nötige aktuelle Nachweis noch nicht vollständig erhoben/zugeordnet ist.
Komponententests ersetzen keine geforderten Benutzerabläufe. D1–D7 und alle
T-Gesamtzeilen bleiben bis zum vollständigen Audit offen.
Die folgende T01–T17-Tabelle bezieht sich auf den bisherigen Referenzlauf
`209278de`. Die zusätzlichen Ergebnisse des Korrekturimages `f098f43` stehen
im gesonderten Abschnitt unten; dort sind auch die inzwischen ausgeführten
persönlichen CLI-, Isolations- und Neustartvarianten aufgeführt.

## Pflichtvarianten T01–T17

| Variante | Erwartete Wirkung | Tatsächliches Ergebnis / Status | Beleg |
| --- | --- | --- | --- |
| T01.1 Anlage und Auflistung | Zwei unterschiedliche persönliche AOSP-Identitäten, A Admin, B normal | Bestanden: tatsächliche Anlage und CLI-Auflistung mit Alpha 10/10 als Admin und Beta 11/11 als normalem Benutzer | E01, E02, E19 |
| T01.2 Erster korrekter Zugang A/B | Jeweils erster richtiger Login ohne Aufwärmversuch; nur Ziel-CE neu entsperrt | Bestanden vor Reboot; Vorbereitung und echte GNU-Ausführung erfasst | E01, E02 |
| T01.3 Falsches Passwort | Keine Entsperrung, keine Sitzung, kein Runtime-Start | Bestanden für Betas nach dem Passwortwechsel ungültiges bisheriges Passwort: AOSP verweigert, CE bleibt gesperrt, Kontext fehlt, Runtime-Start abgelehnt | E23 |
| T01.4 Identitätsquelle | Keine parallelen persönlichen Linux-Konten/Passwortspeicher | Offen: vollständige Quell-/Gastzuordnung erforderlich | — |
| T01.5 Passworttransport | Keine Offenlegung in Transport, Argumenten, History, Dateien und Logs | Offen: begrenzte Quellkontinuität allein genügt nicht | — |
| T02.1 Passwortwechsel und erneute Sperre | Neues Passwort erlaubt Zugriff, altes scheitert; Originaldaten erhalten | Bestanden Beta: AOSP-Passwortwechsel, regulärer Logout, altes Passwort verweigert, neues bestätigt; ursprüngliche Datei/Konfiguration und Inode-Metadaten unverändert | E23 |
| T02.2 Passwortwechsel über Reboot | Neues Passwort nach Reboot gültig, altes ungültig; keine vollständige Neuverschlüsselung | Bestanden Beta: gleiches Profilpaar geordnet gestoppt/gestartet, erster neuer Passwortzugang gültig, anschließend altes Passwort verweigert; Originalbytes und Inode-Metadaten erhalten. Quellprüfung bestätigt Weiterverwendung des Synthetic Password | E23, E24 |
| T03.1 Interne/äußere Identität | Intern UID/GID 1000, getrennte Hostbereiche, keine unübersetzte Host-UID 1000 | Bestanden: Host-UIDs 1007500/1107500 | E01, E02 |
| T03.2 Namespaces | Unterschiedliche User-, Mount-, PID- und IPC-Namespaces | Bestanden, zusätzlich UTS/Net getrennt beobachtet | E02 |
| T03.3 Zuordnung bei Wechsel | Ursprüngliche Prozess-/Benutzerzuordnung bleibt erhalten | Bestanden für beide externen Wechselrichtungen | E08 |
| T04.1 Gesperrtes CE | Kein Runtime-Start und kein Ersatzbetrieb | Teilbelegt: nach verweigertem Beta-Login wurde `linux start` ausdrücklich abgewiesen; CE blieb gesperrt und Kontext fehlte. Dieser Fall hat zusätzlich keine gültige CLI-Sitzung und isoliert daher die CE-Startvoraussetzung noch nicht | E23, E24 |
| T04.2 Fehlende Namespaces | Start wird verweigert | Offen: gezielter aktueller Nachweis | — |
| T04.3 Ungültiges Mapping | Start wird verweigert | Offen: gezielter aktueller Nachweis | — |
| T04.4 Fehlende Sicherheitsvoraussetzungen | Jede erforderliche Voraussetzung erzwingen, kein schwächerer Ersatz | Offen: Voraussetzungen und Varianten vollständig zuordnen | — |
| T05.1 Eigene GNU-Shell | GNU-Programme mit eigener Identität und HOME ausführen | Bestanden für A/B | E01, E02 |
| T05.2 Fremder/gesperrter Kontext | Shell-Zugang verweigern | Offen: beide Varianten explizit zuordnen | — |
| T05.3 Shell-Ende | Nur Shell endet; Sitzung und derselbe Hintergrundprozess bleiben | Bestanden für Alpha; ergänzende Exitcode-/PTY-Prüfungen getrennt dokumentieren | E01 |
| T06.1 CE/HOME-Dateizugriff A→B und B→A | Keine fremden Dateibytes | Bestanden für die konkret geprüften Pfade | E05 |
| T06.2 Konfiguration/Test-Secrets A→B und B→A | Kein Lesen/Verändern; ursprüngliche Peer-Bytes erhalten | Bestanden | E06 |
| T06.3 Private Pakete B→A | Vorhandenen privaten Store weder lesen noch verändern | Bestanden für Alphas vorhandenen Storeeintrag | E06 |
| T06.4 Private Pakete A→B | Vorhandenen privaten Store weder lesen noch verändern | Offen: Beta hatte keinen privaten Store; Abwesenheit zählt nicht | — |
| T06.5 Flüchtige Dateien A→B und B→A | Kein fremder Inhalt/Schreibzugriff bei gleichzeitig entsperrten Benutzern | Bestanden; Alphas flüchtige Probe wurde separat nach Kontextneustart erneuert | E06 |
| T06.6 Prozesse A→B und B→A | Kein Zugriff/Stoppsignal; ursprünglicher Peer macht weiter Fortschritt | Bestanden, mit unabhängiger Beobachtung derselben PID/Startzeit | E05 |
| T06.7 IPC A→B und B→A | Gleicher Queue-Name bleibt privat, fremde Queue unzugänglich | Bestanden für POSIX-Mqueues; keine Aussage über beliebige weitere IPC-Schnittstellen | E07 |
| T07.1 Daten nach Runtime-Neustart | Originaldatei, Konfiguration und private Software erhalten | Bestanden für beide Benutzer: Originalbytes, Konfiguration, tatsächliche jq-Versionen und unveränderte Paketgenerationen | E03, E04, E17 |
| T07.2 Flüchtige Daten nach Kontextneustart | Alte `/tmp`- und `/run`-Proben fehlen, persistente Bytes bleiben | Bestanden für beide Benutzer mit unmittelbar vorher neu angelegten flüchtigen Proben | E17 |
| T07.3 Daten/Pakete nach VM-Neustart | Beide Originaldateien/Konfigurationen und A-private/B-gemeinsame Version erhalten | Bestanden: Originalbytes, getrennte Konfiguration, jq/libjq1 u4/u3, unveränderte Paketgenerationen/-datensätze; alte flüchtige Proben fehlen. Originaldaten und unterschiedliche Versionen auch nach zweitem gepaarten Neustart mit Beta-Passwortwechsel bestätigt | E13, E15, E16, E24, E27 |
| T08.1 Wechsel beider Richtungen | Alten Terminalkanal widerrufen, zulässigen Hintergrundprozess erhalten | Bestanden, frische Anmeldung über zweite CLI | E08 |
| T08.2 Bildschirmsperre | Terminal widerrufen, zulässige Arbeit weiterführen; kein behaupteter CE-Entzug | Bestanden: Keyguard/Asleep unabhängig bestätigt, beide Originalprozesse laufen, CE bleibt entsperrt | E09 |
| T08.3 AOSP-Ressourcenstopp | Gestoppten Hintergrundbenutzer samt Runtime-Ressourcen abbauen | Bestanden für den beobachteten AOSP-Limitfall: Beta-Prozess/Kontext entfernt, CE gesperrt, anschließend frische Anmeldung und bytegleiche Originaldaten; Alpha-Prozess bleibt erhalten. Vorzeitige fehlgeschlagene Endzustandsprüfung bleibt dokumentiert | E20, E22 |
| T09.1 Eigener Runtime-Stopp A/B | Nur eigener Kontext/flüchtige Ressourcen enden, Sitzung/CE korrekt | Bestanden in beiden Richtungen: Originalprozess/Kontext weg, Sitzung und CE erhalten; anschließende Daten-/Tmp-/Run-/Mqueue-Prüfungen bestanden | E17 |
| T09.2 Runtime-Stopp mit aktivem Peer | Peer bleibt unverändert arbeitsfähig | Bestanden in beiden Richtungen mit gleicher Peer-PID/Startzeit und fortschreitendem Zähler | E17 |
| T10.1 Regulärer Logout Beta mit Alpha aktiv | Beta-Prozess/Kontext weg, CE gesperrt, bekannte Datei unlesbar; Alpha unverändert aktiv | Bestanden | E10 |
| T10.2 Regulärer Logout Alpha | Originalprozess/Kontext weg, CE gesperrt, bekannte Datei unlesbar, Systembenutzer erhalten | Bestanden | E11, E12 |
| T10.3 Offene Zugriffe, Mounts, IPC | Vollständigen Ressourcenabbau und fehlenden Wiederzugriff zeigen | Teilbelegt: Kontext/Cgroup leer; eigenständiger Mqueue-/Mount-/Zugriffsabbau noch vollständig zuordnen | E12 |
| T10.4 Konkurrierender Start/Vordergrundwechsel | Keine erneute Freigabe während Abmeldung | Offen | — |
| T11.1 Ausstehende CE-Sperrung | Kein falscher Erfolg/Neustart der Runtime, Fehler sichtbar | Offen auf diesem Image | — |
| T11.2 Fehlgeschlagene CE-Sperrung | Kein falscher Erfolg, kein verdeckter Framework-Neustart | Offen auf diesem Image | — |
| T11.3 Wiederherstellung | Nach Beheben sichere Sperrung und frische Authentifizierung | Offen auf diesem Image | — |
| T11.4 Logout bei Paketaktion | Kein unbemerkter persönlicher Zugriff zurückgelassen | Offen | — |
| T12.1 Gepaarter Stopp | Android und KeyMint-Helfer vollständig und sauber beenden | Bestanden, Launcher Exit 0 und Helferzustand ausgehängt | E13 |
| T12.2 Gepaarter Start | Gleiches Profil/Datenträger, neue Boot-ID; beide CE vor Login gesperrt | Bestanden: identisches Manifest und Disk-Inodes, neue Boot-ID, CE `[0]`, Originaldateien und privater Store unlesbar | E15 |
| T12.3 Erster Login nach Reboot | Beide Originaldaten nach jeweils erstem korrektem Login erhalten | Bestanden für A/B ohne vorherigen Fehlversuch; verzögerte Sitzungskontrolle und tatsächlicher GNU-Zugriff | E16 |
| T12.4 CLI-Benutzerlöschung | Schlüsselzugriff, Runtime und privater Zustand entfernt | Offen | — |
| T12.5 Neue Identität und ID-Wiederverwendung | Kein Zugriff auf alte Daten/Zuordnungen, Seriennummer/Lebenszyklus korrekt | Offen; kontrolliertes zusätzliches Profil zulässig | — |
| T13.1 `install all` | Gültige Freigabe erlaubt; fehlende/falsche/Nicht-Adminfreigabe verweigert | Teilbelegt: gültige Alpha-Freigabe und tatsächliche Installation; drei Ablehnungsarten offen | E03 |
| T13.2 `install user` | Gleiche vollständige Autorisierungsmatrix | Teilbelegt: frühere gültige Alpha-Installation und alle drei Ablehnungsarten belegt. Neuer Beta-Auftrag erhält gültige Alpha-Freigabe, scheitert jedoch anschließend beim expliziten Versionsrückgang; keine Veröffentlichung, aktive Bestände unverändert. Fehler offen | E04, E33–E36 |
| T13.3 `update all` | Erlaubte Aktion und alle drei Ablehnungsarten | Bestanden: leere Adminauswahl, falsches Adminpasswort und Nicht-Adminfreigabe verhindern Veröffentlichung; frische Alpha-Freigabe erlaubt genau den geplanten gemeinsamen Updatebestand, anschließend tatsächlich aktiviert und ausgeführt | E28–E32 |
| T13.4 `update user` | Erlaubte Aktion und alle drei Ablehnungsarten | Offen | — |
| T13.5 `remove all` | Erlaubte Aktion und alle drei Ablehnungsarten | Offen | — |
| T13.6 `remove user` | Erlaubte Aktion und alle drei Ablehnungsarten | Offen | — |
| T13.7 Bereich/Eigentümer | Fehlender Bereich und manipulierte Eigentümer ändern nichts | Offen | — |
| T13.8 Antragsteller ≠ Admin | Privater Bestand gehört Antragsteller; Freigabe gewährt Admin keinen persönlichen Lesezugriff | Offen: Beta-Antrag mit frischer Alpha-Freigabe ausgeführt, danach Paketfehler vor Veröffentlichung. Private Zuordnung und anschließende Zugriffsprüfung noch nicht belegt | E36 |
| T13.9 Unbeteiligtes CE | Freigabe entsperrt keinen unbeteiligten Benutzer | Offen: für aktuelle vollständige Aktionsmatrix zuordnen | — |
| T14.1 Gemeinsame Software bestehender Benutzer | Gemeinsames Programm tatsächlich ausführen | Bestanden Alpha vor privater Aktivierung und Beta | E03, E02 |
| T14.2 Nachträglicher Benutzer | C erhält gemeinsame Software ohne A/B-private Daten/Versionen | Teilbelegt: Gamma erhält frisches HOME und führt gemeinsame jq/libjq1 u3 aus, mit passendem Bibliotheks-Hash und gemeinsamer Paketgeneration; eigene Konfigurationsordner leer. Explizite Zugriffsprüfung auf A/B-private Daten aus Gamma noch offen | E21 |
| T14.3 Persönliche Einstellungen | Unterschiedliche private Konfiguration trotz gemeinsamer Basis | Bestanden für erfasste A/B-Proben, einschließlich bytegleicher Wiederherstellung nach Reboot | E02, E06, E16 |
| T15.1 Dasselbe Paket in V1/V2 | Beide Versionen samt passenden Abhängigkeiten tatsächlich ausführen | Bestanden: jq/libjq1 u4 bei A, u3 bei B, gleicher Programmauftrag | E02, E04 |
| T15.2 Vorrang privater Version | Private V2 bestimmt tatsächlichen Programmlauf | Bestanden Alpha | E04 |
| T15.3 Nicht verfügbare Version | Ohne stille Ersetzung ablehnen, Bestand erhalten | Offen auf diesem Image | — |
| T15.4 Unauflösbare Abhängigkeiten | Ohne stille Ersetzung ablehnen, Bestand erhalten | Offen auf diesem Image | — |
| T15.5 Private Entfernung abweichender Version | Rückkehr zur gemeinsamen Variante anzeigen und konsistent ausführen | Offen | — |
| T15.6 Private Entfernung gleicher Version | Private Auswahl tatsächlich aufheben, auch ohne Versionsänderung | Offen | — |
| T16.1 Gemeinsames Update bei privatem Bestand | Aktive Kontexte konsistent erhalten, Aktivierung ausstehend anzeigen | Bestanden: fünf gemeinsame Updates veröffentlicht; beide bisherigen Prozessidentitäten, Mounts und vollständigen Paketbestände erhalten. Beide zeigen ausstehende Aktivierung und führen ihre bisherigen jq-/Bibliotheksversionen tatsächlich aus | E31 |
| T16.2 Aktivierung nach Update | Gemeinsame Updates plus private Auswahl konsistent aktivieren oder Konflikt erklären | Bestanden: beide eigenen Kontextneustarts aktivieren exakt die geplanten Versionen einschließlich PCRE2/OpenSSL; beide vollständigen 81-Paket-Bestände geprüft, jq tatsächlich ausgeführt, Status aktuell. Der jeweils andere Kontext bleibt unverändert | E32 |
| T16.3 Private Version bleibt | Keine stille Überschreibung privater Festlegungen beim Abgleich | Bestanden: Alphas explizite private jq-u4-Auswahl bleibt bytegleich und die neue private Generation bindet an die neue gemeinsame Basis. Das gemeinsame jq wird bei diesem Update ebenfalls u4; der vorherige V1/V2-Unterschied ist gesondert belegt | E04, E31, E32 |
| T17.1 Gleichzeitig gemeinsam/privat | Serialisierung oder sichtbare Ablehnung, kein Teilbestand als Erfolg | Offen | — |
| T17.2 Aktionen verschiedener Benutzer | Eigentum/Autorisierung und konsistenter Bestand bleiben erhalten | Offen | — |
| T17.3 Abbruch | Letzter konsistenter Bestand bleibt erhalten/wird wiederhergestellt | Teilbelegt: Abbruch des gemeinsamen Updateplans und des persönlichen Installationsplans vor Freigabe erhält beide Kontexte und Paketbestände; Ausführungsphasen bleiben offen | E28, E33 |
| T17.4 Installationsfehler | Fehler sichtbar, keine teilweise aktivierte Umgebung als Erfolg | Teilbelegt: tatsächlicher Fehler beim gültigen privaten Versionsrückgang wird als Fehler gemeldet; beide Kontexte und Auswahlen unverändert, kein privater Teilbestand ausgewählt. Weitere Fehlerphasen bleiben offen; gültige Installation selbst muss korrigiert werden | E36 |
| T17.5 Logout während Transaktion | Kein persönlicher Restzugriff, sichtbarer Abschluss/Abbruch/Reparaturbedarf | Offen | — |

## Belegkatalog

Die SHA-256-Werte beziehen sich auf die vollständigen lokalen JSON-Dateien.
Diese enthalten Ereignisse beziehungsweise verknüpfte Rohbelege und grenzen
ihre Aussage ein. Keine Profile, Passwörter oder Buildartefakte werden hochgeladen.

| ID | Lokaler Pfad relativ zum Belegverzeichnis | SHA-256 |
| --- | --- | --- |
| E01 | `alpha-first-flow/result.json` | `a08f725588f7a841f875b54c3eba4d255d7767b55a3c332e5fe6b3c6b1fce663` |
| E02 | `two-user-version-baseline/result.json` | `7c3ec5ef166622fc6139757b6b90229f1633e479d30d6477ef0ae5187d1b0ba5` |
| E03 | `shared-u3-activation/result.json` | `2c1029a6d2a63a64aeb3a5440524802e1d0ef7911f53d67da61dc10e129601f9` |
| E04 | `alpha-u4-activation/result.json` | `3e942b3d1bb018ad1b8a9dad03e9f7a57c3f57c1616825d9d9071736e73080b2` |
| E05 | `reciprocal-file-process-proof.json` | `6b288f84c2af9d8bb3d9c83de80b9f0c29fbdc536bb6b5709f5e0eb121942bc7` |
| E06 | `reciprocal-private-state-proof.json` | `b09a95151346b9c1dc27c94a4516e48be02f8a77d6cc553b57751bb7e7c8c256` |
| E07 | `reciprocal-mqueue-proof.json` | `c6761a190c94640c5452d79db494ebdb3a6263e4a47b367ba9f9a2fe859470c3` |
| E08 | `reciprocal-external-switch-proof.json` | `2c6bd123b9b3eff2185562d756644513c022c9701b6f6ccba7a7b9a871982f66` |
| E09 | `screen-lock-background-proof.json` | `9b100ec38351394a93667898655c346f82eef3b313af1f7004a3cdc02af0f283` |
| E10 | `beta-logout-proof.json` | `deba29150aa6e9f3f9e244d4e9ef0cbb7f5beabf1ac76a059d618fe3a5cbaa88` |
| E11 | `alpha-logout-proof.json` | `1947ad2d2c8f912e579d747ab4d39acd1b87f19a8e669fc6ad940c425a00578e` |
| E12 | `identity-test/reboot-checkpoint.json` | `40df7073677cda4cf824cec36ecbab9fe5aaa193e254664a140517b5eb2e684a` |
| E13 | `paired-shutdown-1.json` | `a1ddc71361415466e697cd6e363b2a36ab42ae84e58f5b6b4435e3d3c7657ed4` |
| E14 | `first-reference-boot-health.json` | `b41f371a1c0b92b80eace46ba06021dff01005cbb671f2e4f8ded5bbaebd9553` |
| E15 | `postboot-locked-baseline.json` | `38868bfb264e865835c1a375c92c6377ef17744a9a1a782e21c31f7b09fc0553` |
| E16 | `paired-reboot-readback-v2/result.json` | `5f527250f03c94b3677224987d110dd9c8a888f4bb90c79d943db6909db19ab1` |
| E17 | `reciprocal-runtime-stop-proof.json` | `e6e3763b9ffc5b4b72fd0ccfc8431dd02e956d81d53908d580d8cf1495ba8c85` |
| E18 | `private-state-hash-verifier-check.json` | `ed64a678635800422ce3d7e00cb512a7acfa21bb2ca95858fa3f55680e1f0239` |
| E19 | `before-third-user.json` | `645ee4ccaccbcd3e04ffa97b5699e1b8c2f940a766f58768a51ed80adea41e6c` |
| E20 | `beta-aosp-resource-stop-proof.json` | `1c65561ea6a2461714261f7fe0b709174c20e5c2acc4162c8ccb9a0e25da6ae4` |
| E21 | `gamma-shared-program-proof.json` | `76b162a64cd6ea9962e9bda60d5f9412f6da79e927eda4c8ed08e9bffbf0f9a9` |
| E22 | `beta-aosp-resource-recovery-proof.json` | `4283d6658f6ca6e6b94cd0422caeed23b8b9931d45ec117e1e364728ec0f1d82` |
| E23 | `beta-password-change-proof.json` | `551d7d9f268a4a3f3c05aed474e5f949f50fe4b25320334b2f4edb61ba54eac1` |
| E24 | `beta-password-reboot-proof.json` | `31db67c68b1fcbbc1bd64480edb2e53abea325290c8aa5fef0dccd2d3b167964` |
| E25 | `boot2-pre-shutdown-health.json` | `580f1d6e32b6507fb56803f7dd38b4e43d9746773bb96ab8f51be839058035a1` |
| E26 | `boot-migration-cleanup-classification.json` | `c798664aeeebb93514bc66de8927ea754d9dede938598f6792618b758dcce877` |
| E27 | `alpha-second-reboot-readback-proof.json` | `b3b4b958c2ff1f32d057290627e3ebd76dd9f420df00b3fb775510cb0873b212` |
| E28 | `update-all-cancel-proof.json` | `45770e57849e8820b745bc47b06f7a9ad3abaefad14acce93921013e1c91fcc8` |
| E29 | `update-all-wrong-password-proof.json` | `9f9643bfb5eb89b1ad733d3c1579b77cf15c8401c0c69ce08b5d688483a32401` |
| E30 | `update-all-nonadmin-proof.json` | `8083b52d469e496c317f9b1bd1259755c89ae2dcd2cdbe03963b3009055e25d6` |
| E31 | `shared-update-publication-proof.json` | `4cda0dc2b398d6c8809a441b63f4c1b924ae25f250d2d683fd06dbd60a0f31ba` |
| E32 | `shared-update-activation-proof.json` | `80ec5a01d0a952c7d1455360b9155d2760c17a08665b5097b36acc48d119b961` |
| E33 | `beta-private-install-cancel-proof.json` | `37174daf62c17c7ccba4843f20d08ed2c1f06419c3d27cc7f3cfdf043fb82093` |
| E34 | `beta-private-install-wrong-proof.json` | `178488713f28a6f2cb220be5c452336b8b5d492ce7a451c91cc7d90288a7b605` |
| E35 | `beta-private-install-nonadmin-proof.json` | `1f07beb76514f6894a69ad79702d3b93a203d25c4d0218128d17bd924e3a16d2` |
| E36 | `beta-private-install-valid-failure-proof.json` | `4625a96c725a71959554368546ac43cb5a62dc3cf5f437dd3724a34d89e223b2` |
| E37 | `reviewed-downgrade-before-fix/result.json` | `9280d539c42b8683bde43c233c71dcaf99a1fd81bc6383dd11a609a91ac016c6` |

E37 ist eine **fehlgeschlagene Regression zur Fehlerreproduktion**, kein
bestandener Abnahmefall. Sie läuft im separaten frischen Profil
`af23f1ac-854c-4883-a499-665ca9494aef` auf Image `209278de`; Testcommit
`065c6e8` ändert ausschließlich den Executor-Test, alle zehn Hilfsprogramme
sind bytegleich. Der normale geprüfte Archivplan für Version 2 → 1 endet
in der APT-Simulation mit Status 100: Die nichtinteraktive Ausführung benötigt
`--allow-downgrades`. Boot-/SystemServer-Identität, Enforcing und CE `[0]`
bleiben unverändert. Die vorbereitete Korrektur erlaubt den Versionsrückgang
bei vorhandenem geprüftem Plan; die exakte Prüfung der simulierten Änderungen
bleibt erhalten. Der Korrekturstand `f098f43` ist inzwischen vollständig lokal
gebaut; 20 Images, Kernel-/Runtime-Eingaben, AVB und die neue Basisdisk sind
geprüft. Boot und alle sechs gezielten Executor-Tests sind inzwischen bestanden,
einschließlich derselben zuvor roten Regression. Auch der echte private
CLI-Versionsrückgang ist inzwischen auf dem Korrekturimage bestanden; sein
gesonderter Nachweis steht in der folgenden Tabelle. Die übrige Abnahme bleibt offen.

## Zusätzlicher Korrekturstand f098f43

Image und native Tests stammen aus
`f098f439f051c34e92fb1be0b4d908cc542358ea`. Das neue Profil ist
`535c2e93-df64-445b-b9e2-b71e6b403db7`. Der erste Boot hat ID
`33e0e0e8-3896-4c42-a19d-270d4665153c`, SystemServer `1404/40097`.
Die späteren, jeweils im Beleg gebundenen Prüfungen nach gepaartem Neustart
laufen im zweiten Boot `17a75d6e-75f2-4f18-bf02-ec3d093e57b8`, SystemServer `1124/21865`.
Die folgenden Belege liegen lokal unter `out/phase1-dod/f098f439/`.

| Nachweis | Ergebnis und Grenze | Beleg / SHA-256 |
| --- | --- | --- |
| Bootkonfiguration | Boot abgeschlossen, authentifiziertes ADB, Enforcing, FBE/Metadatenverschlüsselung, passender AVB-Digest, CE nur `[0]`; begrenzte Beobachtung | `boot-observation.json`, `6178275cc63b98a851415842911019042b72d62e4394e649fbbd676318cf9255` |
| Paketkorrektur | 6/6 Executor-Tests bestanden: gewöhnliche Installation/Aktualisierung/Entfernung; geprüfter Ablauf mit Konfiguration/Abhängigkeitsmarken; gewöhnlicher privater Versionsrückgang; Abgleich mit älterer Basis; private Entfernung mit Rückfall; abweichende Simulation vor Paketskripten verweigert | `native-downgrade-controls/result.json`, `e004cf92be2c70defd8e90998ec3bb1e0c2abb83a28a08c290b7d000edfd55e8` |
| Bildschirm und Eingabe | Einstellungen sichtbar; QMP-TAB/Enter öffnet Netzwerkseite, ein Maus-Klick in unabhängig beobachteten Zurück-Koordinaten führt zur Startseite; Systemidentität unverändert | `qmp-ui-proof.json`, `d6997661ac4a1a639c00af0ea084894f422191f809c7f7bb1fabfaf89425ec9e` |
| Binäres ADB | 262144 synthetische Bytes identisch übertragen/zurückgelesen; eigene temporäre Gastdatei entfernt | `adb-binary-proof.json`, `6c29af2f7ef997b27db191ed58ebffcdad7ba448418b9a2cae8d75eeff78b7b9` |
| Erster persönlicher CLI-Ablauf | Zwei Benutzer angelegt; Alpha erstmals angemeldet, GNU-Befehle und Home-Struktur geprüft, 1024-Byte-Datei geschrieben; ursprünglicher Hintergrundprozess überlebt Shell-Ende. Beta bleibt gesperrt. Noch kein Zwei-Benutzer-/Persistenznachweis | `initial-personal-flow.json`, `53ca89bcd135f6b9856448d89692eadae41d46edb370805d9029f38be4c5cdee` |
| Gemeinsamen Installationsplan abbrechen | Leere Adminauswahl beendet den jq-Plan; Laufzeit, Paketdatenbank, Auswahl und CE unverändert. Generische Fehlermeldung der CLI bleibt ungenau; keine Installation bewiesen | `shared-install-cancel-proof.json`, `e440ef7c69b981fd7b2a72c9e9608a485259554541820a96a78291f42a7959e1` |
| Gemeinsame Installation, falsche Freigabe | AOSP verweigert falsches Adminpasswort; Paketdatenbank, Auswahl, Laufzeit und CE unverändert, Anmeldung weiterhin gültig | `shared-install-wrong-proof.json`, `698f71f2a92aea4decaf6f196c084b5c19f604273be9cec5a4fefe90b5ce3534` |
| Gemeinsame Installation, Nicht-Adminfreigabe | Beta darf nicht freigeben; Planung/Ablehnung erhalten Paketbestand, Kontext und CE, Alpha bleibt angemeldet. Generischer erneuter Anmeldehinweis bleibt ein Bedienungsfehler | `shared-install-nonadmin-proof.json`, `c24d2162f05fb0f1ca92944c57f9b9b89a0624e252b51a0f37f1aec33909802f` |
| Paketbereich und Eigentümerargument | Je drei CLI-Ablehnungen für fehlenden Bereich und unzulässiges Eigentümerargument bei install/update/remove; Paket-/Laufzeit-/CE-Zustand exakt unverändert | `package-argument-denials-proof.json`, `2551b8fb3f5ef23a35e2ecac4b934bafa06c48df785378baa26947c3401e6939` |
| Gemeinsame Installation und Aktivierung | Gültige Freigabe veröffentlicht u3; ursprüngliche Runtime bleibt bis bewusstem Stopp/Start unverändert. Neuer Kontext führt jq mit passender Bibliothek aus; genau drei zusätzliche Pakete, Originaldaten erhalten, alte flüchtige Dateien weg. Falscher Logout-Prüfaufruf separat erhalten | `shared-u3-activation-proof.json`, `3bee77ca545d0a6b67e612bed633b34239f6cce7c31860ba421a1cc15a2d8a8b` |
| Private Installation und Aktivierung | Private jq/libjq1-u4-Generation gehört Alpha 10/10; gemeinsame u3-Auswahl und laufender Kontext bleiben zunächst unverändert. Nach Stopp/Start u4 tatsächlich ausgeführt, Originaldatei/Einstellungen erhalten, alte flüchtige Dateien weg | `private-u4-activation-proof.json`, `b14ea49a66246e89b0418b44856509f5bbf5108fe32e4deeda7bc2fa3dd74474` |
| Zwei Benutzer und abweichende Versionen | Erster Beta-Login stabil und GNU ausgeführt; Alpha privat u4, Beta gemeinsam u3. Getrennte tatsächliche Host-UID/GID-Bereiche und sechs Namespaces, Alphas ursprünglicher aktueller Hintergrundprozess überlebt Wechsel. Gegenseitige Zugriffstests bleiben separat | `two-user-version-baseline-proof.json`, `e9069aa25986d82adf291cf05b5eb6f95877c9319e82a1fba8c205999b8e727a` |
| Gegenseitige GNU-Isolation | Beide Richtungen: bekannte Testdateien unlesbar, Konfiguration und flüchtige Dateien weder lesbar noch veränderbar, fremder Testprozess nicht adressierbar und vor/nachher unverändert fortschreitend. POSIX-Nachrichtenwarteschlangen getrennt. Betas Zugriff auf Alphas private Paketauswahl verweigert; Beta besitzt noch keinen privaten Store | `two-user-isolation-proof.json`, `e4ce118967b669b41907a1420cbd4f90218d4e7786a8fb3d617f9c370bdb8283` |
| Bildschirmsperre | Offene GNU-Shell widerrufen, CLI unangemeldet; Android Asleep/Keyguard bestätigt. Beide ursprünglichen Hintergrundprozesse schreiten weiter, CE `[0,10,11]` bleibt entsperrt, ursprünglicher SystemServer erhalten. Kein Logout-Nachweis | `screen-lock-background-proof.json`, `cdb4ffd32e9b3b4decc6d64d5852d11989266263fb95b90d65b3a7d4139fb380` |
| Regulärer Logout beider Benutzer | Beta zuerst: ursprünglicher Prozess/Kontext weg, CE gesperrt, bekannte Datei ohne Bytes; Alpha schreitet fort und liest Originaldaten unverändert. Danach Alpha ebenso abgemeldet. Nur System-CE und keine persönlichen Kontexte/Paketworker; ursprünglicher SystemServer erhalten | `both-logout-before-reboot-proof.json`, `1c21ea6ccfa78082147f4882fcab0eada5446650f6e0caa6b01acb5261e6db8d` |
| Sauberer Paar-Stopp | Ursprüngliche Android-/KeyMint-Prozesse samt Starter beendet; Android Power down und Helper clean bestätigt, Profilmanifest unverändert. Nachfolgender Boot und Datenreadback separat belegt | `paired-shutdown-proof.json`, `4dd9969d3a48f88a47a54be02b157086df5d455af655fcc1d1ba429dbab77fce` |
| Zweiter Boot vor persönlicher Anmeldung | Gleiches Profil und Schutz-/Imageeigenschaften, neue Boot-ID `17a75d6e-75f2-4f18-bf02-ec3d093e57b8`, SystemServer 1124/21865. Nur System-CE entsperrt, keine persönlichen Kontexte; beide ursprünglichen Testdateien unlesbar. Readback nach Anmeldung separat belegt | `postboot-locked-baseline.json`, `d6e739a9cd01a3e4dd1a287677876a495332c62037b65cf5b68c959670f602c0` |
| Daten und Versionen nach Paar-Neustart | Beide ersten korrekten Anmeldungen stabil; Originaldateien/Konfiguration bytegleich, alte flüchtige Dateien und IPC weg. Alpha führt privat u4, Beta gemeinsam u3 aus. Je 81 Pakete, vollständige Paketdatenbank-Prüfsummen, Auswahlen und private Festlegung exakt unverändert; neue Host-UID/GID-Zuordnungen getrennt | `paired-reboot-readback-proof.json`, `aad3626470a13b33ceb4bcac84725d30c2cdc288ae769d921fa9d50b95773683` |
| Nachträglicher Benutzer, T14.2 / Referenzschritt 11 | Gamma 12/12 über CLI mit Alpha-Adminbestätigung angelegt; vor Passwort gesperrt, erster Login stabil. Frisches HOME, getrennte UID/GID/Namespaces, gemeinsame u3-Software samt Abhängigkeiten und eigenen Einstellungen ausgeführt. Bekannte A/B-Testdateien und Alphas vorhandene private Paketauswahl liefern aus Gamma keine Bytes. Alpha bleibt aktiv; Beta wird regulär durch AOSP gestoppt und gesperrt. Beta besitzt noch keinen privaten Paketstore; dieser fehlende Gegenfall wird nicht als geprüft gezählt | `third-user-common-and-isolation-proof.json`, `1af75bc265b6905f516e48c08955bf272984e6e31941aa207d42c7e796784847` |
| AOSP-Ressourcenstopp und Wiederanlauf, T08 | Nach AOSP-Stopp und bestätigter Beta-CE-Sperre: frische Anmeldung ohne vorherige Entsperrung durch Zielauswahl, Originaldatei/Konfiguration bytegleich, gemeinsame u3-Software tatsächlich ausgeführt. Vollständiger Paketbestand und Auswahlen unverändert; neuer Beta-Kontext und getrennt erfasste neue Hintergrundprobe. Alphas ursprünglicher Kontext und Prozess bleiben erhalten, Gamma ist abgemeldet | `beta-resource-stop-recovery-proof.json`, `bc0b3107b85f0cd631b7e21a27b3fda36fea7c6eb86195b3c9c39da527d8c2e3` |
| Gemeinsames Update ohne Adminauswahl, T13.3-Teilfall | Beta beantragt fünf Updates; leere Adminauswahl beendet den Plan ohne Veröffentlichung. Vollständige Paketdatenbanken, Auswahlen, beide Kontexte und CE exakt unverändert, Betas Sitzung weiterhin gültig. Weitere Freigabevarianten und Aktivierung bleiben separat | `update-all-cancel-proof.json`, `5d6531948a7feb12d258029d7dbcfa8ad39974e860149530c5bbe534abbe4bc3` |
| Gemeinsames Update, falsches Adminpasswort, T13.3-Teilfall | AOSP verweigert das falsche Alpha-Passwort für Betas Updateantrag. Keine Veröffentlichung; vollständige Paket-, Auswahl-, Kontext- und CE-Felder identisch, Beta bleibt angemeldet | `update-all-wrong-proof.json`, `ee220317ebdc6c832065165f024b30f500d86d38b5b53bc5abe6e4ef78166f6d` |
| Gemeinsames Update, Nicht-Adminfreigabe, T13.3-Teilfall | Betas korrektes Passwort erteilt keine Adminfreigabe. Keine Veröffentlichung, sämtliche verglichenen Zustandsfelder unverändert und Beta weiterhin angemeldet. Generischer erneuter Anmeldehinweis bleibt als CLI-Fehler sichtbar | `update-all-nonadmin-proof.json`, `e87848ce3d11c6957398962eb7a7bc3bdde7dda812ae3a1eaae62e45270e899b` |
| Gemeinsames Update mit frischer Alpha-Freigabe, T13.3 / T16.1 | Beta beantragt, Alpha bestätigt; fünf geplante Updates werden gemeinsam veröffentlicht. Beide bisherigen Kontexte samt vollständiger Paketdatenbanken und Alphas privater Festlegung bleiben identisch; ursprüngliche Hintergrundprozesse schreiten fort. Beide melden ausstehende Aktivierung und führen ihre bisherigen Versionen tatsächlich aus. Neue Versionen nach bewusstem Neustart noch separat zu prüfen | `shared-update-publication-proof.json`, `b746d1189753e8df3d83fec3be6ce0b4b95bc743904550a8c15ec8d87a7c5bcc` |
| Gemeinsame Updateaktivierung und beide eigenen Runtime-Stopps, T16.2 / T09 | Beide bewussten Stopps entfernen nur den eigenen Kontext, behalten AOSP-Sitzung/CE und lassen den Peer fortschreiten. Beide neuen Kontexte führen die erwarteten Versionen aus; vollständige 81-Paket-Bestände, private Alpha-Auswahl und Originaldateien geprüft. Frisch vorbereitete temporäre Dateien verschwinden; beide melden aktuellen Paketstand. Privater Beta-Versionsrückgang bleibt separat | `shared-update-activation-proof.json`, `f812ec421300f35c27a2683431fb3e3536d932053f7182b093972e5a0158df13` |
| Tatsächlich ausstehende CE-Sperrung und Wiederherstellung, T11-Teilfall | Alpha-Logout nicht bestätigt: drei belegte Inodes, Sitzung widerrufen, eigener Kontext entfernt, Beta unverändert. Reguläre Anmeldevorbereitung bestätigt Sperrung vor Passwort; frische Authentifizierung stellt ursprüngliche Daten und exakten Paketbestand wieder bereit. Ursprünglicher Logout bleibt fehlgeschlagen, Ursache der belegten Inodes und weitere T11-Varianten offen | `alpha-pending-logout-recovery-proof.json`, `c7e75df4adbcb90ad9e4b97b2a325dca50b8e07514e43d2bc7523ce2cbfb0f38` |
| Private Beta-Installation ohne Adminauswahl, T13.2-Teilfall | jq/libjq1-u3-Plan bei gemeinsamem u4 ohne Freigabe abgebrochen. Betas komplette Zustandsfelder unverändert; Alpha und Gamma CE-gesperrt, deren private Stores nicht gelesen | `private-beta-cancel-proof.json`, `ed2f532b01bd8cf175b1322be03efbbac08264721a30d90829d7b8df51adf6fb` |
| Private Beta-Installation mit falschem Adminpasswort, T13.2-Teilfall | Identischer Plan, ausdrückliche AOSP-Passwortablehnung; vollständiger Vergleich unverändert und Beta weiterhin gültig angemeldet. Keine Veröffentlichung oder private Versionsausführung behauptet | `private-beta-wrong-proof.json`, `51cf29a728e834f1c5d78ac6815bc75ddd9147de9e26ec4fe4b5ad2560a6b81b` |
| Private Beta-Installation mit Nicht-Adminfreigabe, T13.2-Teilfall | Betas korrektes Passwort darf den identischen Plan nicht freigeben; alle Zustandsfelder bleiben exakt gleich, Beta bleibt angemeldet. Alpha/Gamma weiterhin gesperrt. Generischer Anmeldehinweis bleibt als CLI-Fehler dokumentiert | `private-beta-nonadmin-proof.json`, `c106fa34d1e4bb382be42910073606ad1d100a9ceada0cee3836088126fd2c07` |
| Private Beta-Installation mit Alpha-Freigabe und echter Versionsrückgang, T13.2 / T15-Teilfall | Frische Alpha-Freigabe veröffentlicht ausschließlich Betas private Auswahl; vorheriger Kontext zunächst unverändert. Nach Neustart privates jq/libjq1 u3 gegen gemeinsame u4-Basis tatsächlich ausgeführt, alle 81 Pakete und ursprüngliche Daten geprüft, Status aktuell. Alpha/Gamma in den Zustandsaufnahmen gesperrt. Zwei fehlgeschlagene Live-Worker-Aufnahmen explizit erhalten; keine lückenlose CE-Zeitreihe behauptet | `private-beta-downgrade-activation-proof.json`, `e04b127f05db4e84025d693f6c829d684d4a180bf3b79bab8e932a2859bd8811` |
| Private Entfernung ohne Adminauswahl, T13-Teilfall | Plan zeigt Rückkehr von privatem jq/libjq1 u3 zur gemeinsamen u4-Version; leere Auswahl beendet ihn ohne Veröffentlichung. Alle Paket-, Kontext-, Auswahl- und CE-Felder exakt unverändert; Beta weiterhin angemeldet | `private-remove-beta-cancel-proof.json`, `142ff9a48c2a06b08bdaa86cd7188c20fd1e4cffea5d98ee69f0b49c15073a8c` |
| Private Entfernung mit falschem Adminpasswort, T13-Teilfall | AOSP verweigert das falsche Alpha-Passwort für denselben Rückkehrplan. Keine Veröffentlichung; vollständige Zustandsfelder unverändert, Betas Sitzung gültig. Noch keine ausgeführte private Entfernung | `private-remove-beta-wrong-proof.json`, `66a5a9ea9636c640a95f9d25ac3600eccca46e88bb197d047cad0677626aa23f` |
| Private Entfernung mit Nicht-Adminfreigabe, T13-Teilfall | Betas korrektes neues Passwort erlaubt keine Adminaktion. Keine Veröffentlichung, vollständige Zustandsfelder identisch und Beta weiterhin angemeldet. Tatsächliches Ereignis nach Passwortwechsel vom korrigierten Offline-Prüfer geprüft; gültige Ausführung separat | `private-remove-beta-nonadmin-proof.json`, `32855c9cb2a4e9ed4320b72330b61e7af68564b7550dbeadfc41e26bd0018323` |
| Private Entfernung mit Alpha-Freigabe und Rückkehr zu anderer gemeinsamer Version, T13 / T15 / T16-Teilfälle | Beta hebt private u3-Wahl auf, Alpha bestätigt frisch. Veröffentlichung erhält beide alten Kontexte; Beta führt weiter u3 aus. Eigener Stopp entfernt alten Init/Kontext und erhält Sitzung/CE/Alpha. Neuer Kontext führt jq/libjq1 u4 aus, private Festlegung leer, alle 81 Versionen korrekt, Originaldaten erhalten und neue temporäre Dateien entfernt. Status aktuell; gleiche-Version-Fall separat offen | `private-remove-beta-activation-proof.json`, `0fb703e608d8924a87607351f4fb4f159f4be2430474b0b104935541ed9d7bed` |
| Private Entfernung bei gleicher Version, T15 / T16-Teilfälle | Alpha hebt private u4-Wahl bei gemeinsamer u4-Version mit frischer Freigabe auf. Plan zeigt keine Versionsänderung; Veröffentlichung erhält beide Kontexte. Eigener Stopp/Start aktiviert leere Festlegung bei identischen 81 Versionen und bytegleicher Paketdatenbank. Originaldaten erhalten, neue temporäre Dateien entfernt, Beta unverändert, Status aktuell | `private-remove-alpha-proof.json`, `790219c2c476e262fee76e7b5feb544a9b338066a3191295cab62d77a14f9bbd` |
| Beide vorhandenen privaten Paketauswahlen, T06-Teilfall | Beide GNU-Kontexte verweigern Lese-/Schreibzugriffe auf sieben vorhandene Peer-Pfade einschließlich privater Paketauswahl; Bytes unverändert. Bekannte Originaldateien und SIGSTOP in beiden Richtungen abgewiesen, Peer-Prozesse danach mit gleicher Identität fortschreitend. Vollständige Paket-/Kontextaufnahmen exakt unverändert. Keine neue IPC- oder vollständige Paketdateiabdeckung behauptet | `two-private-contexts-isolation-proof.json`, `fd37f26f8b072c53880b4babf9e145d593608e5a329a895fd243d996e0ba58b4` |
| Passwortwechsel und erneute Anmeldung nach Sperrung, T02-Teilfall | AOSP bestätigt Betas Änderung; vollständige Pakete/Kontexte zunächst unverändert. Bestätigter Logout, alte Probe entfernt, bekanntes Original liefert keine Bytes. Altes Passwort einmal abgewiesen, neues akzeptiert; Originaldatei/Konfiguration/private u3 und alle 81 Pakete im neuen Kontext erhalten, alte temporäre Dateien weg. Alpha unverändert. Gepaarter VM-Neustart mit neuem Passwort bleibt offen | `beta-password-change-readback-proof.json`, `276ff9864fe81196268aae2ff673156bea885d4a8a781e1b3edab3c276262180` |
| Neues Passwort und beide privaten Bestände nach gepaartem Neustart, T02 / T07 / T12-Teilfälle | Beide Benutzer regulär abgemeldet und CE gesperrt; Android/KeyMint sauber beendet, dasselbe Profil als boot-3 gestartet. Vor Anmeldung Originaldateien unlesbar. Erste korrekte Logins ohne vorherige Fehlversuche, Beta mit neuem Passwort; Originaldaten/Konfigurationen, beide privaten u4/u3-Ausführungen und vollständige 81-Paket-Bestände erhalten. Alte-Passwort-Ablehnung bleibt separat im vorherigen Boot belegt | `password-paired-reboot-readback-proof.json`, `28244bb5466e7ab96ad56a4ac337d541e12f75d74c2f44af6518b4e33d05f0c8` |
| Altes Passwort nach drittem Boot, T02-Teilfall | Nach erstem korrektem neuen Login und gesondertem bestätigtem Logout: einmalige alte Anmeldung abgewiesen, CE gesperrt, Originaldatei ohne Bytes, Beta-Kontext entfernt. Alphas vollständiger Kontext und Paketbestand unverändert; kein Framework-Neustart | `boot3-old-password-denial-proof.json`, `59f512b71e2e512ce3a0414d8993af4be0d4e7cca01243ccc65e334a2575c496` |
| Neue Anmeldung nach alter Passwortablehnung, T02 / T07-Teilfälle | Neues Passwort angenommen; ursprüngliche Datei und Einstellungen bytegleich, private u3-Ausführung und alle 81 Paketversionen erhalten. Neuer Beta-Kontext mit getrennten Namespaces, Alpha vollständig unverändert | `boot3-beta-recovery-proof.json`, `33a866cf464e6d07eb06b3870cf4da7b44e8a2513585f6ce9fe02492bf00d064` |
| Offline-Prüfer bei gewechseltem Testpasswort | Sieben Host-Tests und erneute Auswertung dreier bestehender Gastablehnungen bestanden. Neuer Ereignisname wird erkannt, Mehrdeutigkeit und Zustandsänderungen bleiben Fehler. Kein neuer Gastfreigabeversuch durch diesen Beleg | `denial-verifier-rotated-credential/result.json`, `af80100159e8aef660b6bb01a6f199a38e60deb98f07b3380e8fc2a3486f0234`; `historical-replay.json` im selben Unterverzeichnis, `2e4c34dabb2e60d3ce98bda876e1629607ebb027a5c39250b5e274be997ce0a9` |
| Dritter Boot, begrenzte Dienstbeobachtung | Ein SystemServer-Eintritt und keine geprüften Kernel-Panik-/Fatal-/ANR-/FORTIFY-/Watchdog-Muster bis nach den ersten Readbacks. Keine vollständige Klassifizierung aller Dienst-Rückgaben behauptet | `boot3-bounded-health.json`, `aebc15ccd52519bfba81dc13f6ae42bc9d1b74b0636cebd40781dac12b05cead` |
| Zweiter Boot, Dienstbeobachtung | Ein SystemServer-Start und keine geprüften Fatal-/ANR-/FORTIFY-/Watchdog-Meldungen bis zu den Datenreadbacks. Zusätzliche einmalige Altbestand-Aufräumrückgabe quellen-/zustandsgebunden eingeordnet, nicht als behoben behauptet | `boot2-health-classification.json`, `46ac763c50fe95c3bd5447692022c7dd2efdf1e7f4b64ac639833b9b33394002` |
| Passworttransport-Quellbindung | Neun Dateien bytegleich zu Imagecommit und gespeichertem Buildmanifest; separate interaktive Eingabe, Echo-Abschaltung, Pufferbereinigung und sensible Binderdeklarationen statisch geprüft. Keine umfassende Laufzeit-Offenlegungsprüfung behauptet | `credential-transport-source-binding.json`, `88e84b5de6d37cd48f6b73683f77800d157c8a2ff279084daa823855e793037d` |
| Aktuelle Verschlüsselungskonfiguration | FBE/Metadatenverschlüsselung und XTS/HCTR2 beobachtet; vier Quellen gegen Upstream beziehungsweise gespeicherte AEGIS-Buildvorbereitung gebunden. Anfänglicher zu pauschaler Upstream-Vergleich erhalten; keine Schlüssel gelesen | `crypto-current-observation.json`, `e154e92235a2d07baf6c6f60d9fbe87eb5156dce5e31c93ae9f457f8da296c6c` |
| Technisches Linux-Konto | A auf Werks-/Privatbasis und B auf gemeinsamer Basis liefern denselben technischen POSIX-Eintrag und bytegleiche öffentliche Kontometadaten; Zuordnung zu AOSP-Identitätsquellen geprüft. Keine Shadow-/Schlüsseldateien gelesen | `two-user-technical-identity-proof.json`, `5843f9d6fe3c4e5e937d010b8d65b727be2e56ca1309d1920b4b00ce71ff71b8` |
| Begrenzte Dienstbeobachtung | Ein SystemServer-Start, keine beobachteten Fatal-Signale/Java-Fatal-/ANR-/FORTIFY-/Watchdog-Abbrüche; frühe Rückgaben und reguläre Signal-Stopps erneut quellengebunden eingeordnet | `init-exit-classification.json`, `791bf54516d21d8e161d5b92596933792e1d76835ce73ae3db470eccae8b5314` |

Vor/nach den sechs Tests bleiben Boot-ID, SystemServer-Startzeit, Enforcing
und CE `[0]` exakt gleich. Die UI-Prüfung erhält drei fehlgeschlagene anfängliche
Automationsabfragen mit leerem Root-Knoten während Seitenübergängen; nur die
Beobachtung wurde wiederholt, die Eingaben wurden jeweils einmal gesendet.
Die erste Bildschirmaufnahme zeigte noch den normalen System-Sperrbildschirm.
Die später bestätigten Einstellungsseiten sind separat erfasst.

Diese Ergebnisse schließen den Executor-Regressionsfall und die bezeichneten
Bedienungsvarianten. Der vollständige persönliche CLI-Referenzablauf,
Paketautorisierung, Persistenz, Isolation und die übrigen D1–D7-/T01–T17-Fälle
des Korrekturstands bleiben erforderlich.

## Abschlusskriterien und Referenzablauf

Für `f098f43` ist zusätzlich der Abbruch eines echten privaten Updateplans
jq/libjq1 `u3` → `u4` bei leerer Adminauswahl geprüft. Die vollständigen
Zustandsaufnahmen bleiben gleich und die Antragstellersitzung gültig.
`out/phase1-dod/f098f439/update-user-cancel-proof.json`, SHA-256
`b50b868a4e14d9f94e1d83fcfe6686c1e8804f187934a5c966175eb1ffeb135c`,
bindet Ereignisse 697 bis Präfix 700. Zusätzlich sind falsches Adminpasswort
und Nicht-Adminfreigabe mit unverändertem vollständigem Zustand belegt:
`update-user-wrong-proof.json`, SHA-256
`5b306c5147db7b697ef6d62b082329ade431e845cd28736b627cc6e363117ba3`,
und `update-user-nonadmin-proof.json`, SHA-256
`9e9fc7e573fef34902216c38998ab9fa810570b4949b5d98ae673af04e832322`.
Der vorausgehende reguläre private Installationslauf stellt nur den
Update-Ausgangsbestand her. Die gültige Alpha-Freigabe und erfolgreiche private
Aktivierung sind anschließend separat belegt:
`private-update-activation-proof.json`, SHA-256
`a928b7c364beeeb85ecae3a105f3c4e1257c522146f3a51787ccd1d2a7512cc0`,
Ereignisse 707 bis Präfix 733. Der laufende Beta-Kontext behält zunächst `u3`;
sein eigener Stopp/Start aktiviert `u4` samt expliziter privater Wahl. Alle 81
Versionen, Originaldaten und der unveränderte Alpha-Kontext sind geprüft.
Damit sind die vier Autorisierungsfälle für `update user` erfüllt;
`remove all` und die übrigen Pflichtvarianten bleiben offen. Einzelheiten im
[Abnahmeabgleich](phase-1-acceptance-progress.md).

Für `remove all` sind anschließend leere Adminauswahl und falsches
Adminpasswort mit gültiger Antragstellersitzung und vollständig unverändertem
Zustand belegt: `remove-all-cancel-proof.json`, SHA-256
`44aafe8667cd25b6ad7d45af5f74fdf8f5cdd41d81ae657f3a5fa2113a66c1c8`,
und `remove-all-wrong-proof.json`, SHA-256
`35485f9621a15105c515e9a30f5bd91cc3ab39166227151ad86a3c456e0b0d83`.
Nicht-Adminfreigabe ist ebenfalls unverändert abgewiesen:
`remove-all-nonadmin-proof.json`, SHA-256
`f7b6c35c7374da7ab656c52004c9ff55c2115a4ae964c8fc721fb6c1a155c62f`.
Gültige Alpha-Freigabe veröffentlicht die gemeinsame Entfernung, und Betas
private Aktivierung erhält alle 81 Pakete sowie Originaldaten. **Alphas
anschließender Start scheitert jedoch ohne bestätigte Aktivierung.** Er bleibt
bei gültiger AOSP-Sitzung gestoppt; ausgewählte Generationen und Betas Kontext
bleiben unverändert. Fehlerbeleg `shared-removal-alpha-failure-proof.json`,
SHA-256 `1d5ffac286d8766897491c8962b07afc461108a9ae3012873a27d9b18038eb3f`,
Ereignisse 742 bis Präfix 785. Die vollständige gemeinsame Entfernung und die
Gesamtfreigabe bleiben damit ausdrücklich offen; Ursache noch ungeklärt.

Der gezielte native Regressionstest aus Commit `5975256` ist lokal erfolgreich
kompiliert und in den unten bezeichneten Gastgruppen geprüft. Der Buildbeleg
`out/phase1-dod/5975256-removal/native-build-receipt.json` hat SHA-256
`d311e91a93d14fe7b9eaad1872817940aacf46112911e212c75151dac53e05b4`.
Nur die Testquelldatei weicht ab; zehn produktive Hilfsprogramme sind bytegleich
mit dem Referenzbuild. Dies ist kein bestandener T13-/T16-Fall und kein Nachweis
einer Fehlerbehebung.

Die sechs vorhandenen Auswahlfälle sind im separaten Profil
`3ce0c9c7-364f-4523-a496-7c785fc2d255` inzwischen bestanden, mit identischer
Boot-ID, SystemServer-Startzeit, Enforcing und System-CE vor/nach dem Lauf.
Beleg `out/phase1-dod/5975256-removal/selection/result.json`, SHA-256
`2c2b21d80782a4422cc27bb0631998ba623ec215976c71b003b32c72e2666723`.
Auch die drei danach ausgeführten Entfernungsfälle bestehen ohne übersprungene
Tests, einschließlich Ausführung, Veröffentlichung und erneutem Öffnen.
Beleg `out/phase1-dod/5975256-removal/reconciliation-removal/result.json`, SHA-256
`cb0f032fb2864ada0d62e1ab7c11d1b14014f3439c7251ffe67261d09f785b75`.
Der Systemzustand bleibt gleich. Alphas CLI-Fehler bleibt ungeklärt: Die
synthetische gemeinsame Generation enthält nach Entfernung keine Bibliothek
mehr, während der freigegebene CLI-Plan nur jq entfernt. Ein zusätzlicher Test
mit verbliebener automatischer gemeinsamer Bibliothek ist in Commit `c1663c3`
kompiliert und zusammen mit dem ursprünglichen vollständigen Fall bestanden
(2/2, keine übersprungenen Tests, identischer Systemzustand vor/nachher).
Beleg `out/phase1-dod/c1663c3-orphan/tests/result.json`, SHA-256
`78a55279e744c864b1d1a85309633b2bbc8d181fd7d88b4018d594cebe63b0b4`.
Auch diese Variante reproduziert den tatsächlichen Alpha-Fehler nicht.
Kein T13-/T16-Gesamtabschluss wird daraus abgeleitet.

## Diagnoseimage 3fdb058

Das Diagnoseimage `3fdb058310b75922b17819d47c985bc4eecd09c4` ergänzt feste
Startphasen und numerische Fehlercodes im Brokerlog. Vollbuild, 20 Images,
AVB und Basisdisk sind lokal geprüft; die Einzelbelege stehen im
[Arbeitsprotokoll](phase-1-acceptance-progress.md#diagnoseimage-erster-boot-und-drei-startfehler-kontrollen-bestanden).
Profil `88e1d3da-7d80-443f-be3f-8f2a8717b0c9` bootet mit Enforcing,
authentifiziertem ADB und allein System-CE. Boot-ID
`68ec879a-f27d-4bef-b8d5-a41c44c2e061`, SystemServer `1339/37416`.

Drei bestehende Startfehler-Kontrollen bestehen auf den exakt passenden
Komponenten, ohne übersprungene Tests und mit identischem Ausgangs-/Endzustand.
Beleg `out/phase1-dod/3fdb058-diagnostics/start-failure/result.json`, SHA-256
`7833c12c26c93e449d650fcaa963acbd045b672b92d3a874f2a9d8109449868d`.
Der neue CLI-Anfang mit zwei angelegten AOSP-Benutzern, Alphas erstem korrekten
Login und echter GNU-Ausführung ist belegt; Beta bleibt dabei gesperrt.
Originaldatei und Konfiguration sind im Anfangszustand erfasst. Beleg
`out/phase1-dod/3fdb058-diagnostics/initial-alpha-proof.json`,
SHA-256 `c8459054840d21f084dab72f412fd96d660c3a9ac2a7a907bb4de3826c3e0d4e`.
Anschließend ist die gemeinsame Installation von jq/libjq1 u3 und libonig5
einschließlich ausdrücklichem Runtime-Neustart, genauer Paketprüfung und
bytegleichem Erhalt der ursprünglichen Alpha-Daten bestätigt. Beleg
`out/phase1-dod/3fdb058-diagnostics/shared-u3-activation-proof.json`, SHA-256
`97ccef9f03226b7f45ceca8bff2b3bc8defc391e14c19449976a180703119644`.
Beta bleibt dabei gesperrt; dies belegt keinen Android-Neustart.
Danach aktiviert Alpha privat jq/libjq1 u4 bei unverändertem gemeinsamen
u3-Bestand und bytegleichem Erhalt seiner ursprünglichen Daten. Beleg
`out/phase1-dod/3fdb058-diagnostics/private-u4-activation-proof.json`, SHA-256
`d51975698692b5fd486f0952c9da27eac2f409dba5dcdcf0d9c828a1d974f5d6`.
Beta besteht seinen ersten korrekten Login und führt die gemeinsame u3-Version
tatsächlich aus; eigene Originaldatei und Konfiguration sind angelegt.
Beide Kontexte besitzen verschiedene Namespaces/Host-Zuordnungen, während
Alphas ursprüngliche Prozessidentität und private Auswahl erhalten bleiben.
Beleg `out/phase1-dod/3fdb058-diagnostics/beta-first-common-u3-proof.json`,
SHA-256 `e864f7691bc6caf7f7481065068ce192812e6bed19b6d257c2b2c957acc472bc`.
Diese Fälle ersetzen weder die vollständige gegenseitige Isolation noch
Logout-, Reboot-, Autorisierungs- und Fehlerfallmatrix.
Ein anschließendes gemeinsames Fünf-Paket-Update ist einschließlich beider
ausdrücklichen Kontextneustarts bestanden. Die laufenden Kontexte bleiben
bei Veröffentlichung unverändert; Alpha behält seine explizite private
jq-u4-Wahl auf der neuen gemeinsamen Basis. Exakte Versionen, unveränderter
übriger Paketbestand und ursprüngliche Daten beider Benutzer sind geprüft.
Beleg `out/phase1-dod/3fdb058-diagnostics/shared-u4-activation-proof.json`,
SHA-256 `c3ac11be9163c989fbaef9f60e6e6f72dc8ca923359bdd6b61b591b9fcd71e47`.
Beta installiert anschließend privat jq/libjq1 u3 mit Alphas Adminfreigabe
und aktiviert diese abweichende Version auf der gemeinsamen u4-Basis.
GNU-Ausführung, unveränderte übrige 79 Pakete und ursprüngliche Beta-Daten
sind bestätigt; Alphas Kontext und Auswahl bleiben unverändert. Beleg
`out/phase1-dod/3fdb058-diagnostics/beta-private-u3-activation-proof.json`,
SHA-256 `57f6a22cea2a0a76cc8e62967aaebd72ed7c214c8b1b64acc2fe28fb9ede36d1`.
Alphas anschließende private Entfernung bei weiterhin gemeinsamer u4-Version
ist ebenfalls aktiviert und geprüft: private Versionsliste leer, alle 81
Paketversionen unverändert, ursprüngliche Alpha-Daten erhalten, Betas
u3-Kontext unverändert. Beleg
`out/phase1-dod/3fdb058-diagnostics/alpha-unpin-activation-proof.json`, SHA-256
`5a334210620c48241aa51669addb677a94fc644bfca321456d3b44c5e04d5088`.
Auch Betas privates Update von jq/libjq1 u3 auf u4 ist aktiviert und geprüft:
die übrigen 79 Paketversionen und ursprünglichen Beta-Daten bleiben gleich,
Alphas Kontext mit leerer privater Versionsliste bleibt unverändert. Beleg
`out/phase1-dod/3fdb058-diagnostics/beta-private-update-u4-activation-proof.json`,
SHA-256 `98cf3351582affbbb4d5e7e6fdf2dc608c854aca19ebaf181e55ac2a40d48c13`.
Nach gemeinsamer jq-Entfernung ist Betas private u4-Auswahl erneut aktiviert:
alle 81 Paketversionen und ursprünglichen Daten bleiben erhalten, Alphas
bisheriger Kontext bleibt unverändert. Beleg
`out/phase1-dod/3fdb058-diagnostics/shared-remove-beta-activation-proof.json`,
SHA-256 `91c3de13536e2e9e9aa87c9f9146cdaadcdd943866d936776154e1f4dda2a1da`.
Alphas Aktivierung dieser Entfernung steht auf dem Diagnoseprofil noch aus.
Alphas einmaliger Start reproduziert anschließend den Fehler. Beleg
`out/phase1-dod/3fdb058-diagnostics/shared-removal-alpha-failure-proof.json`,
SHA-256 `21d31efda67e9b70a09f92b85136592aeef8ddaa84f28c73c3187e59f9a6ece6`:
Planer-Eingabevalidierung meldet `ENODATA`; Zustandsfelder bleiben gegenüber
dem Stopp unverändert. Die Korrektur der automatisch markierten Factory-Basis
besteht 102 Hosttests, benötigt aber einen neuen Basis-/Produktbuild und
Gastnachweis. Die früheren f098f439-Ergebnisse werden nicht automatisch zu
bestandenen Abläufen auf diesem Image; Fehlerabnahme und vollständige DoD
bleiben offen.

Die Ursache ist durch lokale Ausführung der unveränderten Produktparser auf
den drei hashgeprüften Generationen bestätigt: 80 automatische gemeinsame
Pakete und keine private Wahl ergeben null Wurzeln und `ENODATA`. Beleg
`out/phase1-dod/3fdb058-diagnostics/shared-removal-root-cause-proof.json`,
SHA-256 `7aec07ce58bbb7bb37f6781b06233e236ce5fdf4c4768ad93992c651f8b0fcfc`.
Die erfolgreiche Ableitung mit 78 expliziten Basiswurzeln ist ein lokaler
Metadatenvergleich; Gastaktivierung und Persistenz der Korrektur bleiben offen.

Die korrigierte Basis wurde anschließend zweimal bytegleich gebaut und
unabhängig geprüft: unveränderte Paketliste, ausschließlich korrigierte
APT-Markierungen. Beleg `out/phase1-dod/b832d6c-base/base-verification.json`,
SHA-256 `8766c6d38f4573de3b07cba1667e5fb097e5c751c370d8afa8bac3ef68b63412`.
Dies ist ein Basisimage-Nachweis; der passende Produktbuild und Gastnachweis
bleiben erforderlich.

## Kandidat b832d6c mit korrigierter Factory-Basis

Der lokale Vollbuild
`local-20261003T231351Z-b832d6c0-TSVmQp` für
`b832d6c077baeee4324e00d00dc3618372f3e9d9` ist mit
`LOCAL_BUILD_VERIFIED` abgeschlossen. Die getrennte Vorbereitung unter
`/srv/aegis/runs/phase1-b832d6c0` prüft alle 20 Image-Prüfsummen, die
AVB-Kette sowie die ausgewählte Kernel- und Runtime-Basis. Nachweis
`build-validation.json`, SHA-256
`975920cf850003b22f452b0e8956dba08482d140bc867a0b7e4bde88b346cd93`;
AVB-Digest `7a2e84e79ce4e6ab23346ec7e128481bc4fa6c8446cb66e09c93fb7a1bc09470`.
Der passende Komponentenbau ist abgeschlossen; Quellen und Artefaktprüfsummen
sind lokal in `out/phase1-dod/b832d6c-base/native-build-receipt.json` gebunden,
SHA-256 `6b3fd6673d04a3e8b2388c0460ce9cb9f98d6664a717e0d934aa953bc887d0a6`.

Das neue Profil `1943dcb7-d438-48de-8e62-d9967b32b9b2` liegt auf dem
Build-Datenträger. Sein erster Lauf wurde vor Bootabschluss im Host-Launcher
unterbrochen. Android fährt danach normal herunter; der Helfer bestätigt
sauberen Shutdown. Die erste Bildschirmaufnahme zeigt noch keine aktive
Anzeige. Dieser Versuch ist ausdrücklich kein bestandener Boot-, Bildschirm-
oder ADB-Test. Beleg `out/phase1-dod/b832d6c-base/boot-1-interruption.json`,
SHA-256 `e1c6680dcb55b554af40b758ef5144bfb793f7ecdaa1c70164360fef737e5305`.
Der Auslöser des Hostsignals ist nicht geklärt. Dienstbeendigungen während
dieses angeforderten Shutdowns werden nicht als dessen ursprüngliche Ursache
ausgegeben.

Der zweite Lauf verwendet dasselbe erhaltene Profilpaar als transienten
lokalen `systemd`-Dienst `aegis-qemu-b832d6c-boot2.service`. `Restart=no`
verhindert verdeckte Neustarts; `KillMode=mixed` und 120 Sekunden Stopzeit
lassen zunächst den Launcher Android vor KeyMint geordnet beenden. Die
eigentliche Paketregression und die vollständige DoD bleiben offen;
es wurden keine Buildartefakte hochgeladen.

### b832d6c: Boot, Bedienung und authentifiziertes ADB

Der zweite Lauf erreicht am 4. Oktober den Bootabschluss. Boot-ID
`3af1952e-80ab-4ef2-bef4-92a74fb0da75`, SystemServer `1189/29447`, AVB-Digest
wie oben, SELinux Enforcing, `managed-v1`, `ro.adb.secure=1`, ausschließlich
Systembenutzer 0 und CE `[0]` sind vor der persönlichen Benutzeranlage
bestätigt. Die erste ADB-Verbindung scheitert an der Authentifizierung;
der vorgesehene zweite Verbindungsversuch ist erfolgreich. Die
Authentifizierungsanforderung bleibt aktiv.

Einmaliges QMP-TAB/RET öffnet aus den Einstellungen „Network & internet“.
Ein einzelner QMP-Mausklick bei beobachtetem Cursor `(57.2,106.2)` innerhalb
des „Navigate up“-Elements führt zurück. Screenshots und UI-Bäume bestätigen
beide Übergänge. Drei erfolglose frühere UI-Abfragen bleiben erhalten; sie
wurden durch spätere Beobachtungen, nicht zusätzliche Navigationseingaben,
ergänzt. Die erste Mausbewegung landete wegen Beschleunigung außerhalb des
Ziels; vor jedem Klick wurde die Position erneut geprüft und korrigiert.

Ein zufälliger 256-KiB-Testblock kommt über ADB bytegleich zurück. Die einzige
dafür angelegte Gastdatei wird entfernt. Beide abgeschlossenen Belege prüfen
unveränderte Boot-/Framework-Identität, SELinux und CE-Ausgangszustand.
Lokale Belege unter `out/phase1-dod/b832d6c-base/`:

| Beleg | SHA-256 | Reichweite |
| --- | --- | --- |
| `baseline.json` | `ce4ea40a92bae50a7a5154283346d2851f99bff1e33f8c61d364444aba42dbe4` | Systemzustand vor persönlichen Benutzern |
| `qmp-ui-proof.json` | `f4ea8a33e684704347096b89328b16d1ac8170397d7773d01ff7da6af7c21aef` | Tatsächliche Tastatur-/Mausnavigation |
| `adb-binary-proof.json` | `3e5fce603a5d9f10d9b242b0d6274f4d8b303e19ddd41c03ec98d9d1612b516f` | Authentifizierter Binärtransfer und unveränderter Ausgangszustand |
| `boot2-health-observation.json` | `0827efbb6233fab3d5fddf7911b1f077794481ee568c45fe78b205fc7f1126fa` | Begrenzte Logpräfixe: ein Framework-Start, keine gefundenen Java-Fatal-/Fatal-Signal-/Watchdog-Abbruchmeldungen |

Der letzte Beleg erhält auch alle nicht mit Status 0 beendeten Dienste.
Ihre vollständige Zuordnung zum aktuellen Image und die weiteren
Lebenszyklusfälle bleiben erforderlich. Diese Teilbelege schließen weder
D1 noch den persönlichen Referenzablauf oder die Paketkorrektur ab.

### b832d6c: zwei Benutzer und falsches Beta-Passwort

Alpha `10/10` wird über `setup` als AOSP-Administrator angelegt, zunächst mit
gesperrtem CE. Seine erste Anmeldung gelingt ohne vorherigen Fehlversuch;
vor der Passwortübermittlung wechseln lediglich Vordergrund und Zielauswahl.
CE bleibt bis dahin `[0]`, ein GNU-Kontext fehlt. Nach der Passwortprüfung
bleibt die Sitzung auch in der verzögerten Statusprüfung angemeldet.

Alpha legt Beta `11/11` mit eigener frischer Adminbestätigung als normalen
Benutzer an. Die CLI listet beide Identitäten korrekt auf. Ein einzelner
falscher Passwortversuch für Beta wird von AOSP abgewiesen; dessen Vorbereitung
entsperrt ebenfalls nichts. CE bleibt vor und nach dem Versuch `[0,10]`.
Anschließende `linux start`- und `linux shell`-Aufrufe werden abgewiesen und
verlangen erneute Anmeldung. Eine unabhängige Aufnahme bestätigt fehlende
Runtime-/Paketkontexte, keine veröffentlichten Paketgenerationen sowie dieselbe
Boot-/Framework-Identität. Dies isoliert noch nicht die CE-Startvoraussetzung,
weil zugleich eine authentifizierte CLI-Sitzung fehlt.

Beleg `out/phase1-dod/b832d6c-base/two-users-auth-proof.json`, SHA-256
`f56c1e18ee4a39d952acedac8834d04e77cad1027e72f311ab969c574d92e071`,
bindet Ereignispräfix 15 und die unabhängige Zustandsaufnahme. Eine Prüfung
findet keines der vollständigen generierten Passwörter in den sechs lokalen
Bootlogs. Der umfassende T01-Datei-/History-/Argumentaudit bleibt offen;
dieser Beleg enthält noch keine GNU-Ausführung oder Paketregression.

### b832d6c: GNU-Ausführung und korrigierte Basispaketmarkierungen

Nach erneuter regulärer Alpha-Anmeldung startet die Runtime erfolgreich.
Die echte GNU-Shell bestätigt Debian 13.7, glibc 2.41, UID/GID 1000, HOME,
die zehn ursprünglichen privaten Verzeichnisse und die vorgesehenen Mounts,
Namespaces, SELinux-Domäne, fehlenden Capabilities, `NoNewPrivs=1` und
Seccomp-Modus 2. Die gemeinsame Basis verweigert einen normalen Schreibversuch.

`apt-mark showmanual` liefert genau dieselben 78 Paketnamen wie die installierte
dpkg-Datenbank; `apt-mark showauto` bleibt leer. Damit ist die Rezeptkorrektur
erstmals auch aus der laufenden gewöhnlichen GNU-Umgebung beobachtet. Dies
beweist noch nicht den späteren Abgleich nach gemeinsamer Paketentfernung.

Alpha erzeugt seine ursprüngliche 1024-Byte-Testdatei und getrennte Proben für
Konfiguration, synthetisches Test-Secret sowie `/tmp` und `/run/user/1000`.
Nach `exit 7` bleibt die AOSP-Sitzung gültig und die Runtime bereit. Unabhängige
Aufnahmen vor und nach dem Shell-Ende stimmen in allen Feldern außer dem
Erfassungszeitpunkt überein, einschließlich Init-PID/Startzeit `6857/241726`,
78 Paketen, Namespaces, CE `[0,10]` und fehlenden veröffentlichten
Paketgenerationen. Beta bleibt gesperrt. Es wurde noch kein Hintergrundjob
gestartet; daraus folgt kein Nachweis seiner Lebensdauer.

Beleg `out/phase1-dod/b832d6c-base/initial-alpha-proof.json`, SHA-256
`ec1ce281b4fbe5721af6eade24e5b5da0b3ff6282574d6c37f9cf68845d58fc1`,
bindet Ereignispräfix 39, beide unabhängigen Aufnahmen, ursprüngliche
Datei-/Konfigurationsprüfsummen und den Basisbuildbeleg. Pakettransaktionen,
Betas GNU-Kontext, gegenseitige Isolation, Logout und Reboot sind auf diesem
Image noch auszuführen. Die vollständige DoD bleibt offen.

### b832d6c: ergänzende Komponentenidentität

Alle elf nativen Artefakte, das Java-Testpaket und das vollständige
Komponenten-Quellinventar sind bytegleich zum Diagnosebuild `3fdb058`.
Dessen drei tatsächlich bestandene `RuntimeSelectionOwner`-Fälle zur
abgewiesenen Auswahl, zur nicht fortsetzbaren gestoppten/ersetzten Startaktion
und zum Stopp ohne verbleibendes Zeitbudget sind mit Originalreceipt,
Logprüfsumme und unverändertem Systemzustand geprüft. Der neue Beleg
`out/phase1-dod/b832d6c-base/component-continuity.json`, SHA-256
`0b810ab0744531f22c024d02cbb9acdda492618a568cc456016c65602ef9cd0f`,
ordnet genau diese drei früheren Komponentenergebnisse ergänzend zu.
Das geänderte Basisrezept und der tatsächliche neue Paketablauf benötigen
weiterhin eigene Systemnachweise; eine erneute Ausführung dieser drei Fälle
auf `b832d6c` wird nicht behauptet.

### b832d6c: gemeinsame jq-u3-Installation aktiviert

Der reguläre CLI-Plan installiert genau jq/libjq1 `1.7.1-6+deb13u3` und
libonig5 `6.9.9-1+b1`. Nach frischer Alpha-Adminfreigabe wird ausschließlich
die gemeinsame Paketauswahl veröffentlicht; Alphas laufender Factory-Kontext
bleibt unverändert und meldet ausstehende Aktivierung. Beta bleibt gesperrt.

Nach ausdrücklichem Runtime-Stopp ist der ursprüngliche Init `6857/241726`
verschwunden; AOSP-Sitzung und CE bleiben erhalten. Der neue Kontext
`7518/355401` führt jq u3 tatsächlich aus. Versions- und Paketdateiprüfung
bestehen, alle bisherigen 78 Pakete bleiben unverändert. Die 81 installierten
Pakete enthalten jetzt 79 manuelle Wurzeln sowie ausschließlich libjq1 und
libonig5 als automatische Abhängigkeiten. Alphas ursprüngliche Datei,
Konfiguration und synthetisches Test-Secret sind bytegleich; die alten
flüchtigen Proben fehlen. Der Status meldet `packages=current`.

Beleg `out/phase1-dod/b832d6c-base/shared-u3-activation-proof.json`, SHA-256
`18e8332a90f0fb5c022a82547627c450c5b379c5d015a05328f9254f3a69a2b9`,
bindet Ereignispräfix 53, Veröffentlichung, Stopp, neue Aktivierung und
Originaldaten. Gemeinsame Generation:
`37c7f8a9856067f0c3edce8c5a5c0181808cd2d4c5607bcafff25eebeeb447ff`.
Dies ist ein erlaubter gemeinsamer Installationsfall; private Versionen,
vollständige Autorisierungsmatrix und die ursprüngliche Entfernungsregression
sind damit nicht abgenommen.

### b832d6c: private jq-u4-Installation für Alpha aktiviert

Der reguläre private CLI-Plan aktualisiert ausschließlich jq und libjq1 von
`1.7.1-6+deb13u3` auf `1.7.1-6+deb13u4`. Nach frischer Adminfreigabe wird
Alphas private Generation veröffentlicht. Die gemeinsame u3-Auswahl und der
noch laufende Kontext `7518/355401` bleiben unverändert; der Status meldet
ausstehende Aktivierung. Beta bleibt während dieser Aufnahmen gesperrt.

Nach ausdrücklichem Runtime-Stopp ist der bisherige Init nicht mehr vorhanden.
Der neue Kontext `8027/455169` führt jq u4 tatsächlich aus. Versionsprüfung,
Paketdateiprüfung und GNU-Funktionstest bestehen. Von 81 installierten Paketen
bleiben alle außer jq und libjq1 unverändert; weiterhin bestehen 79 manuelle
Paketwurzeln und nur libjq1 sowie libonig5 als automatische Abhängigkeiten.
Alphas ursprüngliche Datei, Konfiguration und synthetisches Test-Secret sind
bytegleich. Der Status meldet `packages=current`.

Beleg `out/phase1-dod/b832d6c-base/private-u4-activation-proof.json`, SHA-256
`5f93c4ba529fc2d81ea38e6f18ff5532082e5be2ad192e532bec3ef268999e3a`,
bindet Ereignispräfix 67 und sechs ursprüngliche Belegdateien. Ihre Prüfsummen
und die kanonische Prüfsumme dieses Ereignispräfixes wurden beim anschließenden
Dokumentationsabgleich erneut bestätigt. Private Generation:
`080ac57d25680167b04a1f3ca833f4ad7968b55fcd752a53a24a50d557965dd5`.
Sie bindet die unveränderte gemeinsame Generation
`37c7f8a9856067f0c3edce8c5a5c0181808cd2d4c5607bcafff25eebeeb447ff`.

Dies belegt einen privaten Installations- und Aktivierungsfall. Die ursprünglichen
flüchtigen Dateien fehlten bereits vorher; daraus entsteht kein neuer Nachweis
ihres Entfernens. Gleichzeitiger Betrieb beider Benutzer, Hintergrundprozesse,
Logout, Reboot, vollständige Autorisierungsmatrix und die tatsächliche
Entfernungsregression sind hiermit nicht abgenommen. Phase 1 bleibt offen.

### b832d6c: Beta mit gemeinsamer Version, Alpha im Hintergrund

Nach dem regulären Wechsel mit AOSP-Passwortprüfung startet Beta seinen eigenen
GNU-Kontext und führt die gemeinsame jq/libjq1-Version `1.7.1-6+deb13u3` aus.
Alphas privater u4-Kontext bleibt vollständig unverändert. Beide Bestände
umfassen 81 Pakete; Beta besitzt keine private Paketauswahl. Seine ursprüngliche
Datei und Konfiguration werden erstmals aus der eigenen GNU-Shell angelegt.

Alphas vorher gestarteter, auf 7200 Sekunden begrenzter GNU-Hintergrundjob
behält über den Wechsel Host-PID `8209`, Startzeit `560968`, Host-UID `1007500`
und alle sechs Namespace-Identitäten. Sein Zähler steigt nach dem Wechsel
weiter. Betas eigener Job läuft unter Host-PID `9681`, Startzeit `596231`
und Host-UID `1107500`, mit anderen Namespace-Identitäten. Beide Kontexte
verwenden intern UID/GID 1000. Nach Betas Shell-Ende meldet seine Runtime
weiterhin `ready` und `packages=current`.

Beleg `out/phase1-dod/b832d6c-base/beta-common-u3-two-contexts-proof.json`,
SHA-256 `afaee50ca23c9b12a6afe747a0a657a5212b6f6839b8fc0ad8f617793ede7e6d`,
bindet Ereignispräfix 100, die positiven Prozessbeobachtungen und die
Zustandsaufnahme mit unveränderter Boot-/Framework-Identität und CE `[0,10,11]`.
Betas erste korrekte Anmeldung folgt dem früheren ausdrücklich falschen
Passwortversuch; ein Erfolg ohne vorherigen Fehlversuch wird hier für Beta
nicht behauptet. Gegenseitige Zugriffsprüfungen, Logout und VM-Neustart sind
eigene, weiterhin erforderliche Nachweise.

### b832d6c: gegenseitige GNU-Datei-, Prozess- und IPC-Prüfungen

Bei gleichzeitig entsperrtem Alpha und Beta werden die vorhandenen synthetischen
Testobjekte aus beiden gewöhnlichen GNU-Kontexten geprüft. Die bekannten
HOME-Dateien liefern über die geprüften fremden Pfade keine Bytes. Der jeweilige
fremde Prozess ist im persönlichen Prozessraum nicht sichtbar und lässt sich
darüber nicht anhalten. Positive Beobachtungen vor und nach jedem Versuch
bestätigen dieselben ursprünglichen Hintergrundprozesse mit weiterlaufenden
Zählern, unveränderten Startzeiten, Host-Zuordnungen und Namespaces.

Lese- und Schreibversuche auf Konfiguration, synthetisches Test-Secret sowie
flüchtige Dateien des jeweils anderen Benutzers werden abgewiesen. Die
Beobachtung vergleicht die tatsächlich vorhandenen Zielbytes vorher/nachher.
Alpha hatte seine flüchtigen Proben nach dem früheren Runtime-Neustart neu aus
seiner unveränderten eigenen Konfiguration angelegt; dieser Schritt ist als
neues Testsetup dokumentiert und kein Überleben der alten flüchtigen Dateien.
Zusätzlich kann Beta Alphas vorhandene private Paketauswahl weder lesen noch
verändern. Beta besitzt selbst keinen privaten Paketstore, weshalb dafür kein
umgekehrter Paketnachweis behauptet wird.

Beide GNU-Kontexte erzeugen eigene POSIX-Nachrichtenwarteschlangen, darunter
eine mit identischem Namen. Jeder empfängt daraus ausschließlich seine eigene
Nachricht; die separate Warteschlange des anderen bleibt unsichtbar.
Die abschließende Zustandsaufnahme bestätigt gegenüber dem vorherigen
Zwei-Kontext-Nachweis identische Benutzer-, CE-, Paket-, Kontext-, Boot- und
Framework-Daten. Beide ursprünglichen Runtime-Kontexte bleiben aktiv.

Beleg `out/phase1-dod/b832d6c-base/two-user-isolation-proof.json`, SHA-256
`bbd0d8315cc127ca7fed6a9bb26f846af3e60d400327668ad333ea47534fd0bc`,
bindet Ereignispräfix 151, Zielprüfsummen, Prozessbeobachtungen und die
Zustandsaufnahme `two-user-isolation-state.json`, SHA-256
`cf0d62dcff81e27978f3a38e7db4d137980f761ac0495ca752de9ba13d6921df`.
Das sind konkrete gegenseitige Zugriffsprüfungen, keine vollständige Prüfung
aller Syscalls, fehlender Startvoraussetzungen oder sämtlicher Paketvarianten.
Logout, Reboot und die übrigen Pflichtfälle bleiben offen.

### b832d6c: Bildschirmsperre und direkter Beta-Logout

Die AOSP-Bildschirmsperre widerruft Betas offene GNU-Terminalverbindung. Status
und ein anschließender Startversuch bestätigen fehlende Terminalautorisierung.
CE bleibt `[0,10,11]`; beide ursprünglichen Hintergrundprozesse laufen bei
`mWakefulness=Asleep` mit unveränderten Identitäten weiter. Das Einschalten
allein stellt keine Anmeldung her. Erst Betas frische AOSP-Passwortprüfung
ermöglicht wieder die GNU-Shell und bytegleiches Lesen seiner Originaldaten.
Die vollständigen Zustandsaufnahmen vor und nach diesem Ablauf sind abgesehen
vom Erfassungszeitpunkt identisch.

Beleg `out/phase1-dod/b832d6c-base/screen-lock-proof.json`, SHA-256
`8637c309e507d8313c14e752a0e6d53914422cc86a669beaa37d605210a92ac6`,
bindet Ereignispräfix 173. Die Bildschirmsperre wird ausdrücklich nicht als
CE-Schlüsselentzug ausgegeben.

Der anschließende reguläre Beta-Logout erfolgt ohne vorherigen `linux stop`.
AOSP bestätigt um 01:56:17 UTC den gestoppten Benutzer und gesperrten CE-Speicher.
Betas ursprünglicher Prozess `9681/596231` und sein Runtime-Kontext fehlen;
CE ist `[0,10]`. Die bekannte, zuvor aus GNU erzeugte Datei liefert keine Bytes.
Runtime-Start und Shell-Zugang werden ohne neue Anmeldung abgewiesen. Alphas
vollständiger privater Kontext, Paketauswahl und ursprünglicher Hintergrundjob
bleiben unverändert; dessen Zähler läuft bei Systembenutzer 0 weiter.

Beleg `out/phase1-dod/b832d6c-base/beta-direct-logout-proof.json`, SHA-256
`adc1435ac5ebe6b7a2cdf63b357dda0e0336ce8352c7c5bdac0c2e74ff5d4178`,
bindet Ereignispräfix 181 und die unabhängige Zustandsaufnahme. Das erneute
bytegleiche Lesen von Betas Datei nach Anmeldung bleibt erforderlich. Der Fall
belegt weder konkurrierende Starts noch Logout während einer Paketaktion oder
die Abmeldung eines Benutzers mit aktivem privaten Paketimage.

### b832d6c: direkter Alpha-Logout und geordneter Profilstopp

Alpha meldet sich nach Betas Logout erneut regulär an und liest seine
ursprüngliche Datei und Konfiguration bytegleich. Sein privater u4-Kontext
meldet weiterhin `ready` und `packages=current`. Der direkte Logout ohne
vorherigen Runtime-Stopp bestätigt um 02:04:13 UTC den gestoppten Benutzer
und gesperrten CE-Speicher. Der ursprüngliche Hintergrundprozess `8209/560968`
und beide persönlichen Kontextgruppen sind anschließend entfernt. CE ist `[0]`,
Alphas bekannte Datei liefert keine Bytes, und neue Runtime-/Shell-Anfragen
bleiben ohne erneute Anmeldung abgewiesen. Boot und SystemServer sind unverändert.

Beleg `out/phase1-dod/b832d6c-base/alpha-direct-logout-proof.json`, SHA-256
`718c42a48a010ee9a0afb5e6dee3f039a461b74e2a70e342c201ce1e643f73f9`,
bindet Ereignispräfix 198. Ein zusätzlicher lesender Loop-Beobachter endet
nach drei Proben mit Fehler: Init verschwindet zwischen Existenzprüfung und
Lesen von `/proc/8027/stat`. Originalskript, Fehler und Teilprotokoll bleiben
erhalten; daraus wird kein vollständiger Loop-Freigabenachweis abgeleitet.
Der frühere nicht bestätigte f098-Logout tritt hier nicht erneut auf; seine
Ursache ist durch diesen erfolgreichen Fall weder geklärt noch als behoben belegt.

Nach dem gesonderten Neustart-Prüfpunkt mit beiden gesperrten Originaldateien
werden Android und anschließend der KeyMint-Helfer geordnet ausgeschaltet.
Beide Poweroff-Marker, `AEGIS_HELPER_SHUTDOWN_CLEAN`, der beendete Dienst und
die Prüfsummen der unverändert zugeordneten bestehenden Profildateien sind in
`out/phase1-dod/b832d6c-base/paired-shutdown-proof.json` festgehalten, SHA-256
`11aee08a34f0d0090a7b8b10196fe3b809ec60186611c53466bc281af4a81b41`.
Profil-ID bleibt `1943dcb7-d438-48de-8e62-d9967b32b9b2`. Der anschließende
erneute Boot und bytegleiche Datenzugriff nach frischer Anmeldung sind gesondert
nachzuweisen; der Shutdown allein erfüllt den Persistenzfall noch nicht.

### b832d6c: gemeinsamer Neustart und ursprüngliche Benutzerdaten

Android und KeyMint-Helfer starten mit demselben bestehenden Profil erneut.
Profil-ID und Zuordnung beider Datenträger bleiben erhalten; die Boot-ID wechselt
auf `588a5da9-c117-4953-973a-36eda11ebb0d`. Vor der ersten Anmeldung ist
ausschließlich CE-Benutzer 0 entsperrt. Beide persönlichen Runtime-Kontexte
fehlen und die bekannten ursprünglichen Benutzerdateien liefern keine Bytes.
ADB verwendet die bestehende Host-Autorisierung; der erste Verbindungsversuch
scheitert, der zweite authentifiziert sich ohne neue Hostfreigabe.

Alpha und anschließend Beta melden sich jeweils beim ersten korrekten
Passwortversuch nach diesem Neustart erfolgreich an. Vor der jeweiligen
Passwortprüfung bleibt der Zielbenutzer gesperrt und ohne Runtime-Kontext.
Nach regulärem Linux-Start lesen beide ihre ursprünglichen Dateien,
Konfigurationen und synthetischen privaten Testdaten bytegleich. Die früheren
temporären Dateien und POSIX-Warteschlangen fehlen wie erwartet.

Alpha führt weiterhin seine private jq-Version `1.7.1-6+deb13u4` aus, Beta
die gemeinsame Version `1.7.1-6+deb13u3`. Tatsächliche Programmausführung,
Paketprüfung und der vollständige Vergleich aller jeweils 81 installierten
Pakete bestätigen die früheren Bestände und unveränderten Paketdatenbanken.
Beide GNU-Shells verwenden intern UID/GID 1000 bei getrennten Host-Zuordnungen
und sechs getrennten Namespaces. Neue begrenzte Hintergrundjobs starten erst
nach nachgewiesenem Ende der alten Prozesse; Alphas neuer Job läuft während
des Benutzerwechsels zu Beta weiter. Ein Überleben von Prozessen über den
VM-Neustart wird nicht behauptet.

Beleg `out/phase1-dod/b832d6c-base/paired-reboot-readback-proof.json`, SHA-256
`a058b0beba2e1ae52c6fc16f7499907cd16ddacc230767452be9931c46f7ef1f`,
bindet Ereignispräfix 249, den geordneten Profilstopp, den gesperrten Zustand
vor Anmeldung und beide Zustandsaufnahmen nach Anmeldung. Die abschließende
Aufnahme `boot3-both-restored-state.json` hat SHA-256
`3594a89d26443e5e036c05bda5a67ecf508b4c4b57167f06d42a6adf857043eb`.

Damit ist für diesen Lauf die Persistenz mit unveränderten Originalpasswörtern
belegt. Passwortwechsel, dritter Benutzer, vollständige Paket- und
Autorisierungsvarianten einschließlich der tatsächlichen gemeinsamen
Entfernungsregression sowie die übrigen Lebenszyklusfälle bleiben offen.
Die Ursache des früheren f098-Logoutfehlers und die vollständige Einordnung
der Dienstabbrüche sind weiterhin ungeklärt. Phase 1 ist nicht vollständig
abgenommen.

### b832d6c: nachträglicher Benutzer und AOSP-Ressourcenstopp

Gamma wird über die echte CLI mit frischer Adminprüfung als normaler Benutzer
`12/12` angelegt. Vor der ersten Passwortübermittlung bleiben sein CE-Speicher
gesperrt und sein Runtime-Kontext abwesend. Nach erfolgreicher Anmeldung und
regulärem Linux-Start erhält er ein eigenes HOME mit den zehn vorgesehenen
Verzeichnissen, jeweils Modus 0700, sowie intern UID/GID 1000 mit eigener
Host-Zuordnung. Er führt die gemeinsame jq-Version `1.7.1-6+deb13u3` mit ihren
passenden Abhängigkeiten aus; alle 81 Paketdatensätze entsprechen Betas zuvor
bestätigtem gemeinsamen Bestand. Gamma besitzt keine private Paketauswahl.
Sieben bekannte synthetische Datei-/Metadatenpfade von Alpha und Beta liefern
aus Gammas gewöhnlicher GNU-Shell keine Bytes. Alphas private Version u4 und
sein vollständiger Runtime-Kontext bleiben unverändert.

Die ursprüngliche Erwartung von drei gleichzeitig laufenden persönlichen
Benutzern trifft nicht zu: AOSP meldet um 02:59:23 UTC ausdrücklich vier
laufende Benutzer einschließlich Systembenutzer und stoppt deshalb Beta.
CE ist anschließend `[0,10,12]`; Betas ursprünglicher Hintergrundjob
`5706/158018` und Kontext fehlen, seine Originaldatei ist unlesbar. Alphas
Hintergrundjob `3753/102888` läuft im Hintergrund weiter. Die fehlgeschlagene
erste Zustandsaufnahme ist in `third-user-observer-failure.json` erhalten,
SHA-256 `9a6e85d430973cea0b51c44e28d86f8aa0001d4254ca4c297df67778c2cb1741`.
Die nachfolgende Aufnahme verwendet den tatsächlich erklärten AOSP-Zustand.
Aus diesem Ablauf wird kein gleichzeitiges Entsperrtsein von Beta und Gamma
während der Zugriffsprüfungen abgeleitet. Betas erneuter Datenvergleich nach
frischer Anmeldung steht an diesem Prüfpunkt noch aus.

Beleg `out/phase1-dod/b832d6c-base/third-user-common-proof.json`, SHA-256
`823d3554edb3a2d8a645411ee0d55d1ad76dd5e5ca616cb036796c0c32177bef`,
bindet Ereignispräfix 268 und die Zustandsaufnahme
`third-user-after-aosp-stop-state.json`, SHA-256
`b679351dd4a9866bbe8cd202aa98acaeeca357b4873b465ce023f1d3a684812d`.

### Korrektur der veränderlichen Testereignisse

Die neue Auswertung erkannte, dass die Anlage von Gamma rückwirkend die
Benutzerliste im Ereignis 203 (`reboot-checkpoint`) geändert hatte. Ursache ist
eine Referenz auf das veränderliche `users`-Dictionary im Host-Testtreiber.
Die separat gespeicherte Prüfpunktdatei ist unverändert. Der Treiber friert
strukturierte Ereignisse nun beim Aufruf von `record` ein. Ein gezielter
Regressionstest scheitert vor der Korrektur; danach bestehen alle drei Tests
einschließlich Terminalbereinigung und Passwort-Echo-Abweisung. Diese Änderung
betrifft den Host-Treiber; das getestete Gastimage bleibt `b832d6c`.

Der bereits laufende Treiber wird nicht neu gestartet oder dynamisch verändert.
Seine Originalereignisse sind in `third-user-events-original.json` eingefroren.
`third-user-events-frozen-checkpoint.json` ist eine explizite Ableitung: Genau
ein Ereignis verwendet den unabhängig gespeicherten, zuvor gehashten Prüfpunkt.
Damit wird der ursprüngliche Präfix-Hash aus dem Neustartnachweis exakt
wiederhergestellt. Der Nachweis `third-user-event-reconstruction.json`, SHA-256
`9a5912c714203426e8728252cfdc094fd7711ce6ed152e4f66ae1d262ba740f4`,
bindet Original, Ableitung, Änderung und frühere Prüfsumme. Der fehlgeschlagene
erste Auswerter und sein Fehler bleiben ebenfalls erhalten. Diese Einordnung
ersetzt keinen fehlenden Systemtest; Phase 1 bleibt teilabgenommen.

### b832d6c: Beta nach dem AOSP-Ressourcenstopp wiederhergestellt

Nach Gammas regulärem Logout meldet sich Beta mit seinem unveränderten Passwort
frisch an. Vor der Passwortübermittlung ist Beta weiterhin gesperrt und ohne
Runtime-Kontext. Nach dem normalen Linux-Start liest er seine Originaldatei
und persönliche Konfiguration einschließlich synthetischer privater Testdaten
bytegleich. jq u3 mit libjq1 u3 und libonig5 funktioniert erneut; sämtliche
81 Paketdatensätze, Paketdatenbank-Bytes und Paketzuordnungen entsprechen dem
Zustand vor dem Ressourcenstopp.

Der neue Beta-Kontext besitzt eine neue Prozessidentität. Erst nach bestätigtem
Ende der alten Probe und erneutem Originaldatenvergleich startet eine neue
begrenzte Hintergrundprobe `14422/407466`. Alphas ursprüngliche Probe
`3753/102888` läuft mit unveränderter Identität weiter; sein vollständiger
Kontext bleibt unverändert. CE ist `[0,10,11]`, Gamma bleibt gesperrt und ohne
Kontext. Die bereits vor diesem Ressourcenstopp fehlenden alten temporären
Dateien werden nicht als neuer Löschungsnachweis gezählt.

Beleg `out/phase1-dod/b832d6c-base/beta-resource-recovery-proof.json`, SHA-256
`cf0d2bea3d27f849c1291beeba17c3c78fb29177fd3a1b0aeeda82dbfa5cf34b`,
bindet Ereignispräfix 288, die oben dokumentierte Ereignisabweichung und den
vollständigen Zustandsvergleich `beta-after-resource-stop-state.json`, SHA-256
`6f0e622e6819f28c6391882b81ea9aae05725506cbd35b193eca79085cea6a23`.
Damit ist dieser natürliche AOSP-Ressourcenstopp samt frischer Anmeldung und
Daten-/Paketerhalt belegt. Die tatsächliche Paketentfernungsregression und die
weiteren Pflichtvarianten der Gesamt-DoD bleiben offen.

### b832d6c: private Entfernung mit ausdrücklichem gemeinsamen Rückfall

Alpha entfernt über die reguläre CLI seine private jq-Auswahl u4. Der Plan
kündigt ausdrücklich die gemeinsame Version u3 und den passenden Wechsel von
libjq1 an. Nach einer AOSP-Adminfreigabe wird die Änderung veröffentlicht.
Beide laufenden Kontexte bleiben unverändert; Alphas Status meldet ausstehende
Aktivierung. Gemeinsame Auswahl und Betas Zustand ändern sich nicht.

Der reguläre `linux stop` beendet Alphas ursprünglichen Kontext samt
Hintergrundjob. Init `3568/82596` ist nachweislich entfernt; AOSP-Anmeldung und
CE-Freigabe bleiben erhalten. Betas ursprünglicher Job `14422/407466` macht
vor und nach dem Stopp sowie nach Alphas neuem Start mit unveränderter
Identität Fortschritt.

Nach `linux start` führt Alpha jq u3 mit libjq1 u3 und libonig5 tatsächlich
aus. Die Bibliotheksprüfsumme stimmt mit der gemeinsamen Version überein;
Paketprüfung, 79 manuell gehaltene Pakete und die beiden automatischen
Abhängigkeiten sind bestätigt. Alle 81 installierten Paketdatensätze entsprechen
dem erwarteten Rückfall. Alphas private Auswahldatei enthält nur noch den
Formatkopf ohne private Versionswahl; die neue private Generation
`8837248f5da960aa110e08e1b1c9a03fa139b9fb09f84a2e93687ded4b702cb7`
ist an die unveränderte gemeinsame Generation gebunden.

Originaldatei, Konfiguration und synthetische private Testdaten bleiben
bytegleich. Alpha erhält eine neue begrenzte Hintergrundprobe `21173/603495`;
Betas vollständiger Kontext bleibt unverändert. Der abschließende Status
meldet `packages=current`.

Beleg `out/phase1-dod/b832d6c-base/private-remove-fallback-proof.json`, SHA-256
`beee43d55e6b950b02318d8a4fe9fc79194d2f4351d2a00ea31d07201645c38a`,
bindet Ereignispräfix 320, die Veröffentlichung, beide Kontextzustände und den
separaten Nachweis des alten Prozessendes. Die aktive Zustandsaufnahme
`private-remove-active.json` hat SHA-256
`b1ca0173a580b95a1dbf74ac84f9bb3027b0faa91b905087bd91e5133af7a5ee`.
Dies belegt einen gültig autorisierten privaten Entfernungsvorgang mit
explizitem Rückfall. Verweigerte Entfernungsvarianten und die tatsächliche
gemeinsame Entfernung einschließlich des früheren ENODATA-Startfehlers bleiben
gesondert nachzuweisen.

### b832d6c: alte UID beim Virtualisierungs-Aufräumauftrag eingegrenzt

Die installierte `/system/etc/init/hw/init.rc` ist bytegleich zur vorhandenen
AOSP-Quelle. Ihr früher Aufräumauftrag ist ausdrücklich eine Migration für die
alte VirtualizationService-UID. Das Bootprotokoll bestätigt dessen Ausführung
als UID 1081 mit GID 1000 und Rückgabe 1. Die beobachteten aktuellen Besitzer
sind UID 1000 für das Verzeichnis und seinen Parent; `/data/misc` hat Modus
1771. Quelle und Zustand stützen damit eine Berechtigungsursache beim Entfernen
des inzwischen systemeigenen Verzeichnisses. Historisches stderr fehlt, weshalb
dies als Quell-/Zustandsinferenz und nicht als unmittelbar beobachteter errno
festgehalten wird. Der Auftrag ist von den separat protokollierten regulären
VirtualizationService-Prozessen zu unterscheiden.

Der ausschließlich lesende Beleg
`out/phase1-dod/b832d6c-base/virtualization-cleanup-source-observation.json`,
SHA-256 `5b2e5ab6a96cf3f64dce2cf17aec55b84d55dc96705cf202073676c26214808d`,
bindet Quell-/Installationsprüfsumme, Besitzdaten und den beobachteten Log-Präfix.
Es wurde weder der Aufräumauftrag erneut ausgeführt noch eine Berechtigung oder
Dienstkonfiguration geändert. Die vollständige Einordnung aller Dienstenden
im gesamten Abnahmezeitraum bleibt gesondert erforderlich.

### b832d6c: erster Start nach gemeinsamer Entfernung, Paketvergleich offen

Nach der gemeinsamen jq-Entfernung meldete der erste Runtime-Start am
4. Oktober 2026 um 04:22:50 UTC `runtime=ready ce=unlocked` für Benutzer 10.
Der anschließende GNU-Vergleich scheiterte um 04:23:54 UTC: Er erwartete,
dass die unbenutzten Abhängigkeiten libjq1 und libonig5 erhalten bleiben.
Die anschließende lesende Diagnose fand stattdessen alle 78 manuellen
Werksbasispakete, keine automatischen Pakete und keines der drei jq-Pakete.
Die Reconciliation in `packages/aegis/identity/runtime/package_resolver.cpp`
verwendet ausdrücklich `--auto-remove`; die ursprüngliche Erwartung passt
damit nicht zu diesem Abgleichpfad.

Der ursprüngliche Fehlversuch bleibt unverändert erhalten in
`out/phase1-dod/b832d6c-base/shared-remove-first-check-failure.json`,
SHA-256 `a3cc403a43ee4fb7d1953524a045e5632cab93f40410e8409c074f9a4267e20d`.
Dies belegt einen erfolgreichen ersten Start und einen fehlgeschlagenen
Testvergleich, noch keine vollständige Abnahme der gemeinsamen Entfernung.
Ein korrigierter Funktionstest, Datenprüfung und Aktivierung beim zweiten
Benutzer standen zu diesem Zeitpunkt noch aus; die folgenden Nachweise
ergänzen diesen Zwischenstand. Aus diesem Testfehler lässt
sich keine Ursache einer separat gemeldeten Plattform-Sicherheitswarnung
ableiten.

### b832d6c: gemeinsames jq bei Alpha aktiviert entfernt

Nach dem erfolgreichen ersten Start wurde die falsche Erwartung zur
Aufbewahrung unbenutzter Abhängigkeiten gesondert korrigiert. Dazwischen gab
es weder eine weitere Paketänderung noch einen zweiten Runtime-Start.
Der neue GNU-Test prüft die exakte Liste aller 78 manuellen Werksbasispakete,
den vollständig dazu passenden installierten Bestand, eine leere automatische
Paketliste und das Fehlen von jq, libjq1 und libonig5. bash, apt, dpkg,
GNU-Werkzeuge und glibc 2.41 funktionieren. Originaldatei und Konfiguration
sind bytegleich erhalten.

Der Zustandsabzug bindet Alphas neue private Generation an die veröffentlichte
gemeinsame Generation. Die private Versionswahlliste bleibt leer; beide
Namespacesätze bleiben getrennt. Betas bisheriger Kontext samt Paketbestand
ist unverändert und sein ursprünglicher Hintergrundprozess arbeitet weiter.
Boot-ID, system_server-Identität und CE-Zustand bleiben gleich.

`out/phase1-dod/b832d6c-base/shared-remove-alpha-proof.json`,
SHA-256 `038f90c9f5439d411815a04f3cebe48a93cc3aff4efe6fbf19c3f7f3898ed664`,
bindet 352 Ereignisse, Originaldaten, Zustand vor/nach Veröffentlichung und
Aktivierung sowie die bytegleiche Paketlogik des Image-Commits. Der ursprüngliche
fehlgeschlagene Vergleich bleibt ausdrücklich als Ereignis 336 enthalten.
Dies ist ein korrigierter Nachweis, kein nachträglich als fehlerfrei ausgegebener
erster Testlauf. Bereits vorher fehlende temporäre Testdateien belegen keine
zusätzliche Bereinigung durch diese Aktivierung.

### b832d6c: gemeinsame Entfernung bei beiden Benutzern aktiviert

Beta meldete vor seinem Kontextneustart korrekt eine ausstehende Aktivierung.
Sein bisheriges jq 1.7.1-6+deb13u3 ließ sich weiterhin tatsächlich ausführen;
die Paketprüfung fand keine veränderten Dateien. Der normale `linux stop`
beendete ausschließlich Betas Kontext. AOSP-Sitzung und CE-Freigabe bestanden
weiter, während Alphas ursprünglicher Hintergrundprozess Fortschritt zeigte.
Betas alte Init-Identität wurde anhand von PID und Startzeit als beendet geprüft.

Der erste anschließende Start gelang. Beta verwendet direkt die neue gemeinsame
Generation: jq fehlt, alle 78 Werksbasispakete sind manuell markiert und die
beiden verbliebenen Bibliotheken libjq1/libonig5 automatisch. Der exakte Bestand
umfasst damit 80 installierte Pakete. Alphas privater Abgleich hatte diese
unbenutzten Bibliotheken mit `--auto-remove` entfernt; Betas gemeinsame
Entfernungsaktion hatte ausschließlich jq vorgesehen. Beide Zustände entsprechen
dem jeweiligen Pfad. Betas GNU-Werkzeuge funktionieren, Originaldatei und
Konfiguration sind bytegleich erhalten. Alpha bleibt mit unverändertem Kontext
und Paketbestand aktiv, Gamma bleibt gesperrt.

`out/phase1-dod/b832d6c-base/shared-remove-both-proof.json`,
SHA-256 `b978afe71419db83a044006fd7c6eb8a0edd8c15d45d47f9ea9d2d01a32c325a`,
bindet 383 Ereignisse und den abschließenden Zustandsabzug
`shared-remove-both-active.json` mit SHA-256
`78033a2d24f1eae29fbac66617170b4da407d1b73a098da114db5084f1777fcf`.
Die Nachweiskette schließt Alphas ursprünglichen fehlgeschlagenen Vergleich
ausdrücklich ein. Es gab keinen verdeckten Framework-Neustart; beide Runtimes
melden den Paketstand als aktuell.

Damit ist diese gültig autorisierte gemeinsame Entfernung einschließlich
Aktivierung bei beiden Benutzern nachgewiesen. Die frühere ENODATA-Regression
tritt auf diesem geprüften Startpfad mit erhaltenen Werkswurzeln nicht auf.
Verweigerte Freigaben, Updates, Transaktionsfehler und die übrigen Pflichtfälle
bleiben eigenständig offen; D6 und die gesamte Phase 1 sind weiterhin nicht
abgenommen.

### b832d6c: drei verweigerte Freigaben für gemeinsame Installation

Beta beantragte jeweils einen neuen konkreten Plan für die gemeinsame
Installation von jq 1.7.1-6+deb13u3. Leere Adminauswahl, ein falsches
Alpha-Passwort und Betas korrektes Nicht-Adminpasswort führten jeweils zu
einer ausdrücklich fehlgeschlagenen Veröffentlichung. Die falsche
Passwortbestätigung wurde ausdrücklich von AOSP abgewiesen.

Nach jedem Fall wurde Betas gültige Sitzung unabhängig abgefragt. Die
vollständigen Zustandsaufnahmen vor/nach jedem Versuch zeigen identische
Paketgenerationen, Paketdatenbanken, Kontexte, Benutzer, Vordergrund und
CE-Zustände; keine Paketgruppe bleibt aktiv. Gamma ist weiterhin CE-gesperrt.
Der Nicht-Adminfehler enthält weiterhin den generischen Hinweis auf erneute
Anmeldung. Dieser Hinweis wird nicht als tatsächlicher Sitzungsverlust
interpretiert: Die anschließende Statusabfrage bestätigt Beta als angemeldet.

| Variante | Lokaler Nachweis | SHA-256 |
| --- | --- | --- |
| Ohne Adminauswahl | `install-all-cancel-proof.json` | `1b57a33595cbf46f52a61d4abef05b72969fca059c074b6e12728dc1e19e67fb` |
| Falsches Adminpasswort | `install-all-wrong-proof.json` | `24474aed23810926b82c5f36c0ec4e2ada984a3c2610e8182c211d776e8ccb16` |
| Nicht-Adminfreigabe | `install-all-nonadmin-proof.json` | `512cce099e8b62a860c86216929241208b1271927e9dbcf1e431efe08a32245a` |

Der Sammelnachweis `install-all-denials-proof.json` bindet alle 396 bisherigen
Ereignisse, die drei Einzelbelege und die zum Image bytegleichen Prüfstellen
für CLI-Argumente, einmalige Planfreigabe und AOSP-Adminprüfung. Sein SHA-256
ist `a1f4ec9b17bebf4a8d343545ddf8bea70c22d59d16d51ca1ec40b71e1c6ed915`.
Die Ereignisse sind zusätzlich unverändert als
`install-all-denials-events-original.json` gesichert.

Die drei CLI-Aktionen ohne Bereich und ein zusätzlicher `--owner 10`-Parameter
wurden jeweils unmittelbar vor einer Planerstellung abgewiesen. Diese vier
konkreten Parserfälle sind ebenfalls gebunden; ihre Zustandsaufnahmen umfassen
zusätzlich den ersten abgebrochenen Plan. Das unbekannte CLI-Eigentümerargument
ersetzt keinen vollständigen Test fremder Binder-Aufrufer oder manipulierter
Identitäten. Die Ablehnungsvarianten für persönliche Installation und Updates
stehen in den folgenden Abschnitten; jene für Entfernungen bleiben offen.

### b832d6c: drei verweigerte Freigaben für persönliche Installation

Beta beantragte für jeden Fall einen neuen persönlichen Installationsplan
für jq 1.7.1-6+deb13u3. Leere Adminauswahl, falsches Alpha-Passwort und Betas
korrektes Nicht-Adminpasswort verhinderten jeweils die Veröffentlichung.
Die vollständigen Zustandsvergleiche bleiben identisch: Beide bisherigen
Kontexte und Paketbestände bestehen fort, Betas private Auswahl bleibt
abwesend, Gamma bleibt gesperrt. Betas gültige Sitzung wurde nach jeder
Ablehnung unabhängig bestätigt.

| Variante | Lokaler Nachweis | SHA-256 |
| --- | --- | --- |
| Ohne Adminauswahl | `install-user-cancel-proof.json` | `aeb22407c9763572c9c05f59c9e571d84937ed8a2b294535276a469d24481746` |
| Falsches Adminpasswort | `install-user-wrong-proof.json` | `990de95e06d1c58b11a4f4934c65dbccead06cec4d25509e28c8f6c43bebd0d1` |
| Nicht-Adminfreigabe | `install-user-nonadmin-proof.json` | `823241be76657cff8f2d7fda52fe54846b77837c568e6c805630e299ce7504c5` |

`install-user-denials-proof.json`, SHA-256
`bb052a68caf3fe05bfa0d61c76439ca976ac79385ed2ff6ce5ad978778e7b839`,
bindet die drei Einzelbelege, unveränderte Zustandsfelder und 405 Ereignisse.
Die vollständige Ereigniskopie liegt in
`install-user-denials-events-original.json`. Der generische Anmeldehinweis
bei Nicht-Adminfreigabe bleibt als solcher erhalten; er ersetzt keine
Sitzungsprüfung. Dieser Abschnitt belegt ausschließlich die drei Ablehnungen;
die anschließende gültige Beta-Installation ist unten gesondert beschrieben.

### b832d6c: private Beta-Installation mit abweichendem Administrator

Nach Alphas regulärer Abmeldung beantragte Beta eine persönliche Installation
von jq 1.7.1-6+deb13u3. Alphas einmalige Adminfreigabe führte zur Veröffentlichung
für Antragsteller und Eigentümer 11/11. Die gemeinsame Auswahl blieb identisch.
Die Zustandsaufnahmen vor und nach der Veröffentlichung sowie eine zusätzliche
Worker-Beobachtung zeigen Alpha weiterhin CE-gesperrt. Dies ist keine
kontinuierliche Aufzeichnung des CE-Zustands. Alphas ursprüngliche Testdatei war
bei den Prüfungen im gesperrten Zustand nicht lesbar. Der Veröffentlichungsbeleg
`install-user-beta-publication-proof.json` hat SHA-256
`f2d1b5a3b5e026db6eb35de580f03bb31125c64d71f315738b5981b7851480ab`.

Betas bisheriger Kontext verwendete bis zum regulären Runtime-Stopp unverändert
den vorherigen Paketbestand. Die alten Init- und Hintergrundprozessidentitäten
wurden danach als beendet bestätigt. Der erste anschließende Runtime-Start
aktivierte die persönliche Generation
`67ea9ded957194fca1350f5991dd10090aaf338f9bae641472ac8d0888022d3b`.
Die gewöhnliche GNU-Ausführung prüfte jq, seine Bibliothek, sämtliche 81
Paketversionen sowie 79 manuelle und zwei automatische Pakete erfolgreich.
Betas ursprüngliche Datei und Konfiguration blieben bytegleich erhalten.

`install-user-beta-activation-proof.json`, SHA-256
`e03a29d00213a56fe3b280e5159f59e9aa99489881a18852d623587c5d6008ba`,
bindet 440 Ereignisse, die Veröffentlichung, den Stopp, die Prozessablösung
und den abschließenden Zustandsabzug `install-user-beta-active.json`, SHA-256
`2e0ad282e7e20979f4221c8203d9d21992d9ce0503aae6608204df340a75f98c`.
Der Paketstand wird als aktuell gemeldet. Alpha und Gamma bleiben in dieser
Aufnahme CE-gesperrt. Die unveränderte Ereigniskopie bleibt lokal in
`install-user-beta-activation-events-original.json`.

Bereits vorher fehlende temporäre Dateien belegen keine neue Bereinigung.
Dieser Teilnachweis schließt weder T13 noch die Gesamtphase ab.

### b832d6c: Alpha nach Betas privater Installation unverändert

Alphas erste korrekte Wiederanmeldung und der erste Runtime-Start waren
erfolgreich. Vor der Passworteingabe war Alphas CE-Speicher weiterhin gesperrt
und sein Kontext abwesend. Die ursprüngliche Datei und Konfiguration wurden
danach aus der gewöhnlichen GNU-Shell bytegleich gelesen. Alpha verwendet
weiterhin seine private Generation `53014efea70172a36aa951724be4a146bc265606df0e218b01074a28a5f455be`
mit leerer persönlicher Paketauswahl und genau 78 Basispaketen; jq fehlt dort.
Betas private Auswahl, laufender Kontext und sämtliche 81 Paketversionen
blieben unverändert. Sein ursprünglicher Hintergrundprozess zeigte weiterhin
Fortschritt. Beide Benutzer waren dabei CE-entsperrt, Gamma blieb gesperrt.

`install-user-alpha-readback-proof.json`, SHA-256
`575951634c37f05c786b82fbc83d10676f6fcdd1bb61948997987a4403a03d7e`,
bindet 459 Ereignisse und `install-user-alpha-readback-state.json`, SHA-256
`add47c3b59988183bfdbd7b5f3afedb07c139e284d03bb8c3c77bbc71ccb270c`.
Zwei gesicherte Offline-Prüferfehler bleiben Bestandteil dieses Nachweises:
Die erste Erwartung berücksichtigte Alphas Wechsel zu `running` nicht, die
zweite verlangte eine sortierte CE-Ausgabe statt der tatsächlichen
Entsperrreihenfolge. Die Korrektur erlaubt ausschließlich den erwarteten
Benutzerstatuswechsel und verlangt weiterhin exakt die CE-Benutzer 0, 10, 11.
Es wurden dafür weder Gastzustand noch Ereignisse verändert.

Der anschließende gewöhnliche GNU-Zugriffstest umfasst Betas ausgewählte
Paketmetadaten, das tatsächlich vorhandene private Paketimage und den Versuch,
eine neue synthetische Testdatei im privaten Paketspeicher anzulegen. Beide
Leseversuche lieferten keine Bytes und scheiterten; auch das Anlegen scheiterte.
Die Existenz der Leseziele wurde vorher unabhängig beobachtet. Nachher waren
die ausgewählten Metadaten, die Image-Dateimetadaten und beide ursprünglichen
Kontextidentitäten unverändert; die Testdatei war weiterhin abwesend. Betas
identischer Hintergrundprozess zeigte vor und nach dem Test Fortschritt.

`beta-private-store-alpha-check-proof.json`, SHA-256
`23695430a9c4105e5404c08f03f18f58ad37214a09a373e1ff3411991631e33a`,
bindet 466 Ereignisse und die identischen Vorher-/Nachher-Beobachtungen mit
SHA-256 `a5719ff74590d2fa7eea265b7f0877a6c2662ab9a3dd7026e9b5e5be4a5076dc`.
Damit ist diese konkrete Richtung bei gleichzeitig entsperrten Benutzern
nachgewiesen: Alphas AOSP-Adminrolle und Paketfreigabe erteilen seinen
gewöhnlichen GNU-Prozessen keinen Zugriff über die geprüften Pfade auf Betas
private Pakete. Entwicklungs-root dient nur als Existenz-/Zustandsbeobachter.
Die Image-Dateimetadaten wurden verglichen, nicht ihr vollständiger Inhalt
gehasht. Weitere Datei-, Prozess- und IPC-Varianten haben gesonderte Belege;
dieser Fall ersetzt weder ihre Prüfung noch die gesamte T13-Abnahme.

### b832d6c: sechs verweigerte Updatefreigaben

Beta beantragte jeweils einen neuen konkreten Updateplan für `all` und `user`.
In beiden Bereichen verhinderten leere Adminauswahl, falsches Alpha-Passwort
und Betas korrektes Nicht-Adminpasswort die Veröffentlichung. AOSP wies die
falschen Passwörter ausdrücklich ab. Nach jeder Ablehnung bestätigte `status`
Betas weiterhin gültige Sitzung; der generische erneute Anmeldehinweis bei
Nicht-Adminfreigabe wird nicht als tatsächlicher Sitzungsverlust ausgegeben.

Jeder gemeinsame Plan enthielt libjq1 u3 → u4 sowie je ein PCRE2-, libssl3t64-
und OpenSSL-Provider-Update. Jeder persönliche Plan enthielt zusätzlich
jq u3 → u4. Es waren damit tatsächlich ausstehende Paketänderungen, keine
leeren Erfolgspläne. Keine dieser Änderungen wurde freigegeben oder veröffentlicht.

| Bereich | Variante | Lokaler Nachweis | SHA-256 |
| --- | --- | --- | --- |
| `all` | Ohne Adminauswahl | `update-all-cancel-proof.json` | `dbe70c0dd26d73848120d5b0829e1793d4e97321832b5cc47acba988b01da5da` |
| `all` | Falsches Adminpasswort | `update-all-wrong-proof.json` | `58bdf0a3d4c5068d5acc90baf870979228dccac78aac2aca6da4f9c433542fd0` |
| `all` | Nicht-Adminfreigabe | `update-all-nonadmin-proof.json` | `e6ecd2e1e358f58b9e108813a4c60e9e778e93294e25587746c2a39dd9714ca5` |
| `user` | Ohne Adminauswahl | `update-user-cancel-proof.json` | `b90907f8a200e60717a804845f2547c4ee5a11eeee882fd857d600be1fc00033` |
| `user` | Falsches Adminpasswort | `update-user-wrong-proof.json` | `a7c8a86fbe50c75a433053db8526767454ecccf874245d6eca56b29c14909430` |
| `user` | Nicht-Adminfreigabe | `update-user-nonadmin-proof.json` | `6eab2e5c96fe802d9d4226ef282a63544b59a42ea9edef3faeb02d0698f164da` |

Der Sammelbeleg `update-denials-proof.json`, SHA-256
`bcebb573e054eb898396b122b087325f69ccb6d08d88b85556f6c3b7aaf5df35`,
bindet 486 Ereignisse, alle sechs Einzelbelege, deren konkrete Pläne und die
vollständigen Zustandsaufnahmen. Elf Felder einschließlich Paketdatenbanken,
Auswahlen, beiden Kontexten, Benutzer-/CE-Zustand und Paketgruppen sind nach
jedem Fall identisch zum Ausgangszustand. Gamma blieb CE-gesperrt. Die Rohkopie
liegt lokal in `update-denials-events-original.json`.

Diese sechs Fälle belegen ausschließlich verweigerte Updatefreigaben vor
Veröffentlichung. Gültige Updateausführung und Aktivierung, Abbruch während
der Ausführung, Konflikte und Parallelität bleiben eigene offene Prüfungen.
T13, D6 und Phase 1 sind weiterhin nicht vollständig abgenommen.

### b832d6c: begrenzte Boot-3-Dienstbeobachtung bis 07:27 UTC

Die am 4. Oktober 2026 eingefrorenen Android-/Logcat-Protokolle enthalten genau
einen SystemServer-Start. Die live geprüfte ursprüngliche Identität bleibt
PID 1175, Startzeit 21073, Boot `588a5da9-c117-4953-973a-36eda11ebb0d`.
Die sechs geprüften Kategorien (`FATAL EXCEPTION`, `Fatal signal`, `am_crash`,
`am_anr`, `FORTIFY:` und Watchdog-Prozessabbruch) haben jeweils null Treffer.
Alle 24 protokollierten Signal-9-Exits sind konkreten Stop-/Restart-Vorgängen
zugeordnet: odsign, hwservicemanager, adbd und 21 idmap2d-Stopps vom ursprünglichen
SystemServer. Ein erster Offline-Prüfer nahm fälschlich nur einen Stop pro
15-Zeilen-Fenster an; er ist gesichert. Die geprüfte Zuordnung verwendet den
jeweils letzten eindeutigen Kontrollvorgang innerhalb einer Sekunde vor dem
Exit beziehungsweise den ausdrücklich erfolgreichen hwservicemanager-Stop.

Die vier nichtnulligen frühen Init-Rückgaben bleiben einzeln erhalten:

- Der System-Mainline-Initializer wird laut Protokoll ausdrücklich übersprungen;
  der aktive Mainline-Initializer endet erfolgreich.
- `misctrl` verknüpft im gepinnten AOSP-Code den erfolgreichen booleschen
  Property-Rückgabewert mit seinem Integer-Exitcode. Die Property ist `0`,
  der reservierte Bereich wird als leer protokolliert, ohne Misc-Lese-/Schreibfehler.
- Recovery-Refresh endet mit 254. Leerer aktueller pstore und der gepinnte
  `-ENOENT`-Rückgabepfad passen dazu; der ursprüngliche errno wurde nicht
  aufgezeichnet. Die Erklärung bleibt eine Inferenz ohne erneuten Aufruf.
- Der bereits dokumentierte alte VirtualizationService-Aufräum-Exec endet mit
  1. Er ist vom eigentlichen Dienst getrennt; die Berechtigungsursache bleibt
  eine Inferenz des vorherigen Quell-/Zustandsbelegs.

`boot3-service-observation.json`, SHA-256
`4c2b2aa33673f659ba56645ec629f07ded54ab7f7624919f08d7433b2ba0105b`,
bindet unveränderte Logkopien, aktuelle öffentliche Verschlüsselungskonfiguration
und die gegen das Buildmanifest gepinnten AOSP-Quellen. Der nachgelagerte Review
`boot3-service-review-proof.json`, SHA-256
`862cf025a7fcf319925ddfd5a2de5b5dba9feeab317f1d0748a7c7942d7267f2`,
prüft diese konkreten Korrelationen und bewahrt die zwei verbleibenden
Erklärungsunsicherheiten. Die vier dokumentierten AOSP-Kryptographiedateien
sind zusätzlich mit `crypto-source-continuity.json`, SHA-256
`ebdd238a56a9189ea3c02bedd5610f372c65871ee5e81024d0b8fb74f9ac8774`,
an das aktuelle Build gebunden; siehe [Kryptographiegrundlage](identity-crypto-baseline.md).
Es wurden keine Dienste verändert, Diagnoseabstürze erzeugt oder Frameworks
neu gestartet. Dies ist eine zeitlich begrenzte Beobachtung, keine vollständige
D1- oder Phase-1-Abnahme.

### b832d6c: erweiterte Dienstbeobachtung um 11:17 UTC

Die neuen lokalen Logkopien vom 4. Oktober, 11:17:25 UTC enthalten die
bisherigen Kopien bis 07:27 UTC bytegleich als Präfix. Boot-ID und ursprünglicher
SystemServer `1175/21073` stimmen vor und nach der Aufnahme bis 11:17:34 UTC
überein. SELinux bleibt Enforcing, die öffentlichen FBE-/ADB-Eigenschaften und
der AVB-Digest stimmen mit dem bisherigen Image überein. Es wurden keine
Gastzustände oder Dienste verändert.

Der neue versionierte [Offline-Prüfer](../scripts/qemu-service-audit.py)
findet weiterhin genau einen SystemServer-Start, keine Treffer der sechs
oben genannten Absturzkategorien und dieselben vier nichtnulligen frühen
Init-Rückgaben. Alle 32 Signal-Beendigungen besitzen eine passende unmittelbar
vorhergehende Stop-/Restart-Anweisung. Die acht hinzugekommenen Fälle sind
idmap2d-Stopps durch den ursprünglichen SystemServer; damit sind es insgesamt
29 idmap2d-Stopps. Die Zuordnung prüft die konkrete Dienst-Prozessgeneration
und verwirft alte Kontrollmeldungen bei erneutem Start, auch bei gleicher PID.
Eine solche Zuordnung erklärt den protokollierten Auslöser, nicht automatisch
die fachliche Berechtigung jedes Stopps.

Der Prüfer gibt bei jedem nichtnulligen Exit weiterhin `REVIEW_REQUIRED`
und Exitcode 2 zurück. Fehlende Init-/Framework-Startbelege, zusätzliche
Framework-Starts, ungeklärte Signale, unbekannte terminale Init-Zeilen und
rückläufige Init-Zeitstempel erfordern ebenfalls Prüfung. Zehn gezielte
[Offline-Tests](../tests/test_qemu_service_audit.py) bestanden. Dies ist kein
neuer vollständiger Systemtest und kein pauschaler Nachweis vollständiger Logs.

Die erneute Quellenlektüre präzisiert den Recovery-Befund: Der übergeordnete
pmsg-Dateileser übernimmt den beendenden Rückgabewert von `PmsgRead` nicht.
Sein unveränderter Anfangswert wird bei fehlenden passenden Datensätzen am
Ende zu `-ENOENT`. Exit 254 identifiziert daher keinen bestimmten
Open-/Read-Fehler und beweist keine beim Boot fehlende pstore-Datei. Der direkte
historische Fehlergrund von Recovery-Refresh und dem alten
VirtualizationService-Aufräumhelfer bleibt offen. Die früheren Belege werden
nicht nachträglich umgeschrieben.

Lokale Belege unter `out/phase1-dod/b832d6c-base/`:

| Beleg | SHA-256 |
| --- | --- |
| `service-followup-capture.json` | `6fe878e0684619766f2e5fab5d25e39466195548adfa1426e5d8eb064702ee1e` |
| `service-audit-original-capture-v2.json` | `453750f6c83d0e124d22f1e1c5c198cc01d871558efd75c0a0c37541a5a14474` |
| `service-audit-followup-v2.json` | `a83f1c558903236e9143fc0a742340b77952e554c316a718e6eafb5d623618fb` |
| `service-followup-proof.json` | `af71bdf7466bafe3d394ab67e60fc768770238e2f4eb158584b35c76494462f8` |

Die beiden ersten Parserberichte ohne `-v2` bleiben zusätzlich erhalten;
die finale Fassung prüft auch einen fehlenden Init-Mitschnitt ausdrücklich.
Reproduktion aus dem Repository-Verzeichnis, mit einem neuen Ausgabepfad:

```sh
python3 -m unittest discover -s tests -p test_qemu_service_audit.py -v
python3 scripts/qemu-service-audit.py \
  --android-log out/phase1-dod/b832d6c-base/service-followup-logs/android.log \
  --logcat-log out/phase1-dod/b832d6c-base/service-followup-logs/logcat.log \
  --expected-system-server 1175 \
  --output out/phase1-dod/b832d6c-base/service-audit-review-copy.json
```

Exitcode 2 ist hier wegen der vier erhaltenen Befunde erwartet. D1 und die
Gesamtfreigabe bleiben offen; Builds, Profile und Rohprotokolle bleiben lokal.

### Historische Restliste des Referenzlaufs 209278de

Die folgende D1–D7-Liste und die anschließende Zuordnung der Referenzschritte
gehören zum früheren Image `209278de`; ihre E-Nummern verweisen auf dessen
Belegkatalog. Sie sind keine aktuelle Restliste für `f098f439`. Insbesondere
sind Betas späterer privater Paketbestand, zusätzliche Autorisierungsfälle und
der dritte Benutzer in den f098f439-Belegen oben dokumentiert. Ein vollständiger
aktueller Variantenabgleich bleibt erforderlich; Alphas fehlgeschlagene
Aktivierung nach gemeinsamer Entfernung bleibt dabei ausdrücklich offen.

| Kriterium | Noch erforderliche vollständige Abnahme |
| --- | --- |
| D1 | Build-/Versionsinventar, Bedienung/ADB und Schutzmechanismen vollständig indexieren; weitere Läufe prüfen. E14 bestätigt den ersten Referenzboot, E25 die begrenzte Boot-2-Beobachtung vor Shutdown. Zusätzlicher Migrations-Aufräumstatus ist in E26 eingeordnet, nicht als behoben behauptet. |
| D2 | Gesamte Benutzerverwaltung, Ablehnungen, Identitätsbindung und Passworttransport einschließlich T01/T02/T12 vervollständigen. |
| D3 | Logoutfehler und Löschung vollständig belegen; Passwortpersistenz ist mit E23/E24 und regulärer Reboot mit E13/E15/E16 nachgewiesen. |
| D4 | Abschließende Zuordnung aller Runtime-Lebenszyklusanforderungen prüfen. AOSP-Ressourcenstopp samt Wiederanmeldung/Datenvergleich ist mit E20/E22 belegt; Runtime-Stopp mit aktivem Peer in beiden Richtungen mit E17. |
| D5 | Fehlende Startvoraussetzungen und Betas privaten Paketbestand prüfen; Rechte-/Adminbegrenzung vollständig zuordnen. |
| D6 | Sechs vollständige Autorisierungskombinationen, Entfernung, Updates, Konflikte und Parallelität nachweisen. |
| D7 | Sämtliche offenen Varianten schließen; Source-/Build-/CLI-/Architektur-/Threat-Model-/Kryptographieübergabe gegen den Entwicklerauftrag prüfen. |

Referenzschritte 1–8 besitzen die oben genannten Teilbelege. Der in Schritt 2
verlangte falsche Passwortversuch ist auf diesem Image noch offen. Schritte 9
und 10 sind mit E13/E15/E16 belegt; die Schritte 11 und 12
bleiben offen. Deshalb wird auch der zusammenhängende Referenzablauf nicht als
vollständig bestanden ausgegeben.

Ein zusätzlicher Prüfkanalfehler bleibt ausdrücklich erhalten: Am 3. Oktober
um 00:11:31 UTC versuchte ein kombinierter GNU-Prüfbefehl nach erfolgreicher
Versionsausführung zusätzlich interne Paketmetadaten zu lesen. Deren Modus
0600 verweigerte diesen Zugriff. Die gesonderte gewöhnliche Programmausführung
bestand anschließend; Berechtigungen wurden nicht erweitert. E04 enthält diese
Einordnung und ersetzt den fehlgeschlagenen Beobachtungsschritt nicht still.

E16 erhält außerdem den ersten fehlgeschlagenen Paketdatenbankvergleich nach
Reboot. Der frühere lokale Beleg endet mit einer einzelnen Newline; der neue
Rohlesevorgang enthält eine zusätzliche abschließende Leerzeile. Der geprüfte
Vergleich erlaubt ausschließlich diese eine zusätzliche Newline, verlangt
ansonsten identische Datensatzbytes und speichert die vollständigen Rohdaten.
Die Produktdateien wurden nicht verändert. Beide Paketdatenbanken umfassen
weiterhin dieselben jeweils 81 installierten Pakete.
