# Entwicklung direkt auf dem Buildserver

Die [abgeschlossene Abnahme der fünf Terminal-Meilensteine](server-acceptance.md)
fasst Ergebnis und Grenzen zusammen. [Terminalanleitung](terminal-quickstart.md)
enthält den konkreten Startbefehl. Die folgenden Einträge dokumentieren den
chronologischen Diagnose- und Reparaturverlauf einschließlich früherer Fehler.

Ab 1. Oktober 2026 autorisiert der Nutzer Entwicklung und QEMU-Systemtests
direkt auf `devserversg`. Die früheren Mac-/HTTP-Vorgaben gelten hierfür nicht.
ARM64 bleibt zunächst das Produktziel: Quellen, Kernel und Runtimebasis liegen
bereits vor. Der x86_64-Server besitzt kein zugängliches KVM; QEMU benutzt TCG.
Ein späterer Mac-Test bleibt eine gesonderte Plattformprüfung.

Build-Artefakte bleiben auf dem Server. **Keine automatischen GitHub-Releases
oder Artefaktuploads**; diese werden erst für einen ausdrücklich vorgesehenen
Mac-Test verwendet. Code regelmäßig committen. `worker.sh`, `build.sh` und die
Exportskripte besitzen historische Releasepfade und sind hierfür nicht die
Build-Einstiegspunkte.

`scripts/aosp/build-local.sh` kompiliert als `aegis-build` unter der vorhandenen
Workspace-Sperre. Ein neuer, unveränderlicher Quell-Snapshot aus einem Commit,
`AEGIS_SCRIPT_COMMIT`, `AEGIS_KERNEL_RUN` und `AEGIS_RUNTIME_RUN` sind erforderlich.
Das Skript kontrolliert den AOSP-Manifest-Pin, verwendet die bestehenden
Quell-/Image-Prüfungen, kopiert die fertigen Images in ein neues Run-Verzeichnis
und prüft deren SHA256SUMS. Es verwendet weder GitHub noch dessen Credentials.
Erfolg heißt `LOCAL_BUILD_VERIFIED`; dies ist ausdrücklich keine Gastabnahme.

Für QEMU werden die Images unter einem neuen Verzeichnis `images/` abgelegt.
`scripts/prepare-server-qemu.py DIR --avbtool PATH/avbtool.py --commit COMMIT` prüft die
AVB-Kette mit den eingebetteten Entwicklungsschlüsseln und erstellt die
Bootkonfiguration sowie neue Metadata-/Misc-/FRP-Partitionen. Anschließend
erstellt `scripts/make-qemu-disk.py DIR` die unveränderliche GPT-Basisdisk.
Der bestehende gepaarte Launcher verwaltet weiterhin Android-Overlay,
KeyMint-Zustand, Eingabeprüfsummen und exklusive Profilsperre gemeinsam.

Linux verwendet QEMU `virt-10.2`, Apple Silicon weiterhin `virt-11.1`/HVF.
Beide vollständigen Aufrufe werden pro Lauf gespeichert. Bestehende Profile
werden nicht automatisch migriert. Der Linux-Standard ist bildschirmlos;
Bildschirm und Eingabe lassen sich über QMP prüfen, optional ist GTK verfügbar.

Die erste CI-Korrektur besteht 270 Hosttests (13 übersprungen vor Installation
der QEMU-/Archivwerkzeuge). Die neue CE-Reparatur ergänzt einen expliziten
ausstehenden Sperrzustand: keine USER_UNLOCKED-Veröffentlichung während einer
unbestätigten Sperre, erneuter bestätigter Entzug vor Benutzerwiederstart und
weiterhin frische AOSP-Passwortprüfung. Ein Framework-Neustart darf aus volds
aufbewahrten Eviction-Metadaten keinen entsperrten Benutzer rekonstruieren.
Die echten Gastregressionen hierzu stehen bei Erstellung dieses Eintrags aus.

## Lokaler Build- und Hosttestnachweis

Commit `993f1e8b4fca34c5f5aaea8c2df3a63eab9b6466` wurde im Run
`/srv/aegis/runs/local-20261001T162620Z-993f1e8b-g1seMf` vollständig gebaut.
Status `LOCAL_BUILD_VERIFIED`, Dienst-Exitcode 0; 14 Minuten 48 Sekunden.
Kernel-/Runtime-/Framework-/vold-/SELinux-Quellbelege und unveränderliche
Imagekopien mit SHA256SUMS liegen im Run. Kein Release oder Upload wurde erzeugt.
Die zusätzlichen AegisIdentityTests und AegisRuntimeNativeTests wurden ebenfalls
lokal kompiliert; ihre tatsächliche Gastausführung steht noch aus.

Der gemeinsame Hosttestlauf nach den Serverergänzungen besteht **276 Tests**,
vier ausschließlich für macOS vorgesehene Clone-Tests sind übersprungen.
Lokaler Beleg: `out/server-stability/host-tests-final.log`.
Die Gastprotokolle und Profilpaare liegen unter `out/server-stability/`.
Die frühen Diagnosestarts haben zwei Fehler im neuen Linux-Startweg gezeigt:
hohe PCI-ECAM-Adresse und unvollständig berechnete VBMeta-Gesamtgröße. Beide
sind korrigiert; der folgende Diagnosegast erreicht die Android-Paketinitialisierung
und zeigt die AEGIS-Bootanimation in 720 × 1280. Das ist noch kein Bootabschluss
und noch kein Nachweis der CE-Wiederanlaufkorrektur.

## TCG-Startdiagnose

Die ersten vollständigen ARM/TCG-Starts mit vier Gast-CPUs sind **nicht stabil**:
Sowohl das ältere Vergleichsimage als auch `993f1e8` erreichen SystemServer,
werden aber beim Erststart vom unveränderten Android-Watchdog beendet.
Beim Vergleichsimage wartet die Hauptschleife auf ActivityManager, beim neuen
Image auf den Berechtigungscache. Die gesicherten Watchdog-Daten zeigen starke
CPU-Wartezeiten; beim Vergleich standen noch rund 3 GiB RAM zur Verfügung.
Das ist keine nachgewiesene CE-Regression: Es waren noch keine persönlichen
AEGIS-Benutzer angelegt. Die Ursache ist noch nicht abschließend geklärt.

Die Rohbelege einschließlich Thread-Dumps liegen in
`out/server-stability/{baseline/boot-3,fixed-993f1e8/boot-1}/`.
Beide Android-Gäste wurden anschließend über `sys.powerctl shutdown` beendet;
beide Hilfssysteme bestätigen `AEGIS_HELPER_SHUTDOWN_CLEAN`.
Der folgende Lauf benutzt dasselbe neue Profil mit `--cpus 8`.
Der Launcher erlaubt nun ausdrücklich 1–16 Gast-CPUs und mit `--memory-mib`
2048–32768 MiB RAM; Standardwerte bleiben vier CPUs und 4096 MiB.
Diese Zuteilung verändert keine Profilbindung, Passwörter oder Bootprüfung.

Der Acht-CPU-Lauf (`fixed-993f1e8/boot-2`) erreicht `sys.boot_completed=1`,
authentifiziertes ADB mit `ro.adb.secure=1`, SELinux Enforcing und CE `[0]`.
Anschließend stirbt SystemServer erneut: UI-Thread für 89 Sekunden blockiert.
Damit ist eine höhere CPU-Zahl allein keine Lösung. Der APK-Installationsversuch
wird dabei mit `Broken pipe` abgebrochen; Gerätetests sind noch nicht bestanden.

Die anschließende Quellprüfung findet einen konkreten fehlenden Bootparameter:
Der **gepinnt vorhandene** Cuttlefish-Launcher setzt in
`host/commands/assemble_cvd/bootconfig_args.cpp` für eine fremde Zielarchitektur
`androidboot.hw_timeout_multiplier=50`, für native VMs `3`.
`shared/config/init.vendor.rc` überträgt dies nach `ro.hw_timeout_multiplier`;
`Build.HW_TIMEOUT_MULTIPLIER` und Watchdog verwenden diesen AOSP-Mechanismus.
In unseren beiden bisherigen Gast-Properties war der Wert leer.
`qemu-init.py` ergänzt deshalb ab `04d2c8f` genau dieses Hardwarebudget:
50 unter TCG, 3 mit HVF/KVM. Es schaltet den Watchdog nicht aus; die AEGIS-
CE-Eviction-Frist bleibt unverändert. Dieser Startweg muss noch durch reale
CLI-Antwortzeiten und alle Schutztests bestätigt werden. Die Tests prüfen
weiterhin Systemserver-Neustarts und melden verzögerte/fehlgeschlagene Aktionen.

Der dritte Start von `993f1e8` bestätigt beide Timeout-Properties als 50,
Bootabschluss, SELinux Enforcing und authentifiziertes ADB mit dem bereits
persistierten Hostschlüssel. Die QMP-Tastatur erzeugt im Gast `KEY_SPACE` DOWN/UP
und öffnet den sichtbaren Sperrbildschirm. SystemServer bleibt PID 1117.
Nach Korrektur der Eigentümer der ausschließlich per ADB installierten
Testhelfer bestehen **44/44 native Gerätetests** aus FscryptEviction,
RuntimeNamespace und RuntimeMemoryGroup in 228,841 Sekunden. Der vorherige
Lauf mit Shell-Eigentümern wurde als ungültige Testinstallation abgebrochen.
Rohbelege und SHA256-Querverweise: `fixed-993f1e8/component-progress.json`.

Der echte CLI-Lauf legt den ersten Administrator als AOSP-Benutzer 10 an,
setzt sein Passwort und bestätigt anschließend CE `[0]` (Benutzer 10 gesperrt).
Der erste Anmeldevorbereitungsschritt scheitert jedoch geschlossen: Der
langsame Android-Benutzerwechsel überschreitet AEGIS' feste 30-Sekunden-Frist;
es wird noch kein Passwort angenommen. Ab `c989a7d` berücksichtigen deshalb
auch die AEGIS-Wartefristen für Android-Zustandswechsel den Hardwarefaktor,
mit einer absoluten Obergrenze von 120 Sekunden. Identitäts-/Zustandsprüfungen,
frische Passwortprüfung und die separate zehnsekündige CE-Eviction bleiben
unverändert. Der lokale Folgebuild und die endgültige Gastabnahme stehen aus.

## Nachgewiesene CE-Fehlerbehandlung im ersten korrigierten Image

Die Java-Gerätesuite besteht inzwischen **180/180 Tests** (963,552 Sekunden
unter TCG). Der Hostlauf mit den weiteren Launcher-/Shutdown-Änderungen besteht
**280 Tests**, vier macOS-Tests übersprungen. Der lokale Build von `c989a7d`
ist `LOCAL_BUILD_VERIFIED`; ein weiterer Build enthält nun zusätzlich einen
gepinnten Bootanimation-Shutdown-Fix. Dieser wartet auf den Animationsthread,
bevor der Hauptprozess gemeinsamen Zustand zerstört. Anlass ist eine während
des normalen Prozessendes beobachtete FORTIFY-Meldung über einen zerstörten
Mutex; die Behebung muss noch in wiederholten Gaststarts bestätigt werden.

Im weiterhin laufenden Image `993f1e8` ist folgender echter Ablauf bestanden:
AOSP-Anmeldung, GNU-Shell als UID/GID 1000 in privaten Namespaces, alle
Capability-Sätze leer, Seccomp und NoNewPrivs aktiv, gemeinsame Basis nur lesbar,
persönliches Home in CE. GNU schreibt eine Datei mit 1024 Bytes. Ein kontrollierter
Root-Testhalter hält genau diese Datei offen. Logout beendet die Runtime,
bestätigt die noch ausstehende CE-Eviction nicht und entzieht die Autorität.
Eine sofortige Neuanmeldung wird ohne Passwortannahme abgewiesen; SystemServer
bleibt PID 1117. Nach Schließen des Halters wird die Sperre abgeschlossen.
Ein falsches Passwort wird von AOSP abgewiesen, CE bleibt `[0]` und die zuvor
geschriebene Datei ist nicht lesbar. Eine frische richtige Anmeldung startet
GNU wieder und liest dieselben Bytes mit identischer SHA-256-Prüfsumme.

POSIX-Mqueue-Erzeugung durch unprivilegiertes GNU-Perl besteht ebenfalls;
nach Runtime-Abbau und Neuanmeldung sind die alten Warteschlangen verschwunden.
Die beidseitige Isolation zweier Benutzer und der Neustartnachweis stehen noch
aus. Belege: `fixed-993f1e8/{ce-fault-recovery.json,component-progress.json,
identity-test/events.json}`. Die Belege benennen auch zwei korrigierte Fehler
im Hosttreiber (legitimer CE-Sperrabschluss während Vorbereitung sowie
Texteingabe beim Schließen einer möglicherweise noch offenen Passwortabfrage).

## Neuer lokaler Abnahmelauf, Image `7fb41f57`

Der vollständige lokale Run
`/srv/aegis/runs/local-20261001T175333Z-7fb41f57-MCSAuT` ist
`LOCAL_BUILD_VERIFIED`; zusätzliche Java-/Native-Testartefakte sind ebenfalls
kompiliert. Das neue Profil liegt unter
`out/server-stability/candidate-7fb41f5/profile`, Bootprotokolle unter `boot-1`.
AVB-Digest: `47b71e2e2ba8e58bc47ce561411c397a332f066344ae789c45d1e6861b4fde63`.

Dieser frische Gast erreicht am 1. Oktober 18:22 UTC Bootabschluss,
authentifiziertes ADB, SELinux Enforcing und Hardwarefaktor 50. Bootanimation
endet mit Status 0; bis einschließlich erster Benutzeranlage fehlen die zuvor
beobachteten FORTIFY-/Watchdog-Abstürze. Tastatur, relative Mausbewegung und
Mausklick sind über QMP und tatsächliche `getevent`-Ausgaben bestätigt;
720×1280-Bildausgabe ist gespeichert (`input-display-proof.json`).

Alpha (10/10, Administrator) wird mit AOSP-Passwort angelegt und zunächst
CE-gesperrt. Die **erste** Anmeldung besteht jetzt ohne Aufwärmversuch:
Vor dem Passwort weiterhin CE `[0]`, kein GNU-Kontext; anschließend bestätigte
stabile Sitzung und echte GNU-Ausführung. Private Home-Rechte, Debian 13.7,
Seccomp, leere Capability-Sätze, sechs Namespaces und nur lesbare gemeinsame
Basis sind nachgewiesen. Beta (11/11, kein Administrator) ist nach frischer
AOSP-Adminprüfung ebenfalls angelegt und zunächst gesperrt. Dessen erste
Anmeldung, beidseitige Isolation und Persistenz werden im selben laufenden
Testtreiber geprüft; sie sind mit diesem Zwischenstand noch nicht abgenommen.

Im gleichen Lauf bestehen inzwischen auch Betas allererste Anmeldung und
beide GNU-Isolationsrichtungen. Beide Originaljobs (Host-PIDs 5949/7619,
Host-UIDs 1007500/1107500) laufen vor und nach den Negativproben weiter.
Lesen der fremden GNU-Datei über Home, Android-CE-Pfad und `/proc/.../root`
sowie SIGSTOP auf den fremden Prozess scheitern aus den unprivilegierten
Kontexten. Alle sechs Namespace-Kennungen sind verschieden. Gleichnamige
POSIX-Mqueues liefern nur die eigene Nachricht; fremde Queue-Namen sind
nicht erreichbar. Beleg: `candidate-7fb41f5/two-user-isolation-proof.json`.
SystemServer bleibt bis 18:40 UTC PID 1322, ohne beobachtetes FORTIFY-,
Fatal-Signal-, Java-Fatal- oder Watchdog-Kill-Ereignis. Paket-/Neustartabnahme
läuft anschließend weiter.

### Paketabbruch: weiterer Fehler unter TCG

Falsche Adminfreigabe und korrektes Nicht-Adminpasswort werden im Image
`7fb41f57` beide abgewiesen, ohne gemeinsame Generation zu veröffentlichen.
Nach dem zweiten Abbruch bleibt jedoch die Bestätigung des Aufräumens aus.
Die gemeinsamen Staging-Dateien sind entfernt, beide GNU-Originalprozesse
beendet und die Kontextgruppe leer; Linux-Status und neue Paketaufträge
scheitern weiterhin. SystemServer bleibt PID 1322. Dies ist **keine bestandene
Paketabnahme** und kein erfolgreicher Benutzer-Logout.

Quellprüfung und Ablauf deuten auf die feste Zwei-Sekunden-Frist der
Aufräumaufrufe hin: ein verspäteter Kontrollkanal-Reply vergiftet die gemeinsame
Verbindung, deren Schließung alle nativen Kontexte sicher beendet. Der alte
Stand protokolliert die Transportursache nicht, daher bleibt diese Zuordnung
bis zur gezielten Wiederholung eine Diagnose. `c526571` gibt ausschließlich
bestätigendem Aufräumen acht Sekunden (weiter unter der unveränderten
Zehn-Sekunden-Protokollgrenze). Bei Kanalfehlern werden nun Operationsnummer,
Exception-Klasse und abgelaufene Frist protokolliert, ohne Inhalte oder Secrets.

Beleg: `candidate-7fb41f5/package-negative-failure.json`. Der Credential-Scan
findet keine vollständigen Testpasswörter in den drei Bootlogs. Der Testtreiber
verwirft seine Passwortpuffer. Android und KeyMint-Helfer sind anschließend
geordnet heruntergefahren (`reboot: Power down`, Helper `clean`); das Paar
bleibt zur Diagnose erhalten. Ein neuer lokaler Vollbuild und frischer
Gesamttest folgen. Kein Build- oder Quellupload ist erfolgt.

Der Folgebuild `c52657113becde42d735669d4d64405c7740cc74` ist im Run
`/srv/aegis/runs/local-20261001T185629Z-c5265711-Ox6o39` inzwischen
`LOCAL_BUILD_VERIFIED` (systemd Exitcode 0). Der erneute Hostlauf besteht
280 Tests, vier Mac-Clone-Tests übersprungen; Beleg
`out/server-stability/host-tests-c526571.log`. Seine neue lokale QEMU-Kopie
liegt unter `candidate-c526571`; eine erfolgreiche Wiederholung des zuvor
fehlgeschlagenen Paketabbruchs ist damit noch nicht behauptet.

Der frische `c526571`-Gast bootet mit AVB-Digest
`90fe54bc5405a87535a0dcb47a6bd08d8e6714f78bb1999798e1131b2781bfb2`,
SELinux Enforcing, Hardwarefaktor 50 und authentifiziertem ADB. Die
Bootanimation endet erneut mit Status 0. Tastatur-/Mausereignisse und
Framebuffer sind in `candidate-c526571/input-display-proof.json` gebunden.
**177/177 ausgewählte Java-Gerätetests** bestehen (JUnit 59,113 Sekunden);
SystemServer bleibt PID 1373, persönliche Benutzer fehlen weiterhin.
Die drei unveränderten erschöpfenden User-ID-Tests wurden aus diesem Lauf
bewusst ausgelassen; sie bestanden bereits im früheren vollständigen
180-Test-Lauf. APK-, Log-Hash und Klassenauswahl stehen in
`candidate-c526571/java-177-result.json`. Die neue echte CLI-Abnahme folgt.

Im frischen `c526571`-Profil bestehen inzwischen beide ersten persönlichen
Anmeldungen (Alpha 10/10, Beta 11/11) ohne vorherigen Fehlversuch. Vor der
Passworteingabe bleiben Ziel-CE und GNU-Kontext unverändert gesperrt/abwesend.
Beide führen anschließend Debian 13.7 mit glibc 2.41 als Runtime-UID 1000 aus,
mit schreibgeschützter Basis, eigenen Namensräumen, leerem Capability-Satz,
Seccomp und NoNewPrivs. Zwei unterschiedliche 1024-Byte-Dateien werden aus
GNU geschrieben, geprüft und mit Hash festgehalten; Beleg
`candidate-c526571/first-login-proof.json`. Alpha besteht außerdem tatsächliche
Terminalgrößenwechsel und Strg+C. Paketabbruch, adversariale Isolation und
gepaarter Neustart sind in diesem neuen Lauf weiterhin ausstehend.

### Paketabbruch im korrigierten Image bestätigt

Im `c526571`-Gast sind jetzt **beide tatsächlichen Negativfreigaben bestanden**:
falsches Alpha-Passwort und korrektes Beta-Passwort ohne Adminrolle.
Die CLI bestätigt jeweils den fehlgeschlagenen Auftrag ohne Veröffentlichung;
anschließend meldet `linux status` weiterhin `runtime=ready packages=current`.
Alle drei Auswahlsnapshots bleiben `NO_SHARED_SELECTION`. Die Originalprozesse
Alpha PID 6317/Start 146366 und Beta PID 8236/Start 204322 schreiben nach beiden
Ablehnungen weiter; SystemServer bleibt PID 1373. Kein Kontrollkanalfehler wird
protokolliert. Ein weiterer Paketplan lässt sich starten.
Beleg: `candidate-c526571/package-negative-proof.json` und die beiden darin
gebundenen Prozessbeobachtungen. Der frühere Fehler reproduziert sich damit
nach der Cleanup-Budgetkorrektur nicht; die genaue damalige Transportursache
bleibt mangels damaliger Diagnoseausgabe eine begründete Zuordnung.
Positive Paketveröffentlichung und gepaarter Neustart folgen gesondert.

Authentifiziertes ADB besteht zusätzlich einen bytegenauen binären
262144-Byte-Rundlauf (`adb-binary-proof.json`); `ro.adb.secure=1` bleibt gesetzt.
Die aktuelle Bedienungsanleitung steht in [Terminalzugang](terminal-quickstart.md).

### Gegenseitige Isolation und CE-Fehlerwiederanlauf im aktuellen Gast

`candidate-c526571/two-user-isolation-proof.json` bestätigt beide Richtungen:
echte GNU-Prozesse können fremde Dateien über CE-/Home-/Proc-Pfade nicht lesen,
keine Bytes erhalten und den fremden Originalprozess nicht mit SIGSTOP
anhalten. Beide Originalprozesse sind davor und danach identisch und machen
Fortschritt. Sechs Namensräume sind jeweils verschieden. POSIX-Mqueue erlaubt
in beiden Kontexten die eigene Nachricht unter gleichem Queue-Namen und weist
den jeweiligen fremden Queue-Namen zurück. Das ist keine erschöpfende
Syscall-/IPC-Matrix.

Der echte CE-Fehlerfall besteht ebenfalls: kontrollierter Root-Dateideskriptor
auf Alphas existierender GNU-Datei, unbestätigter Logout, verweigerter Login
noch vor Passwortabfrage und ohne neuen Kontext. Nach Freigabe wird die
anhängige Eviction abgeschlossen, bevor die neue Passwortabfrage erfolgt.
Ein falsches Passwort entsperrt nichts; selbst der bekannte Dateipfad liefert
keine Bytes. Frische korrekte Anmeldung und tatsächlicher GNU-Lesezugriff
liefern anschließend denselben SHA-256. Der Originalprozess ist entfernt,
SystemServer bleibt PID 1373. Beleg: `candidate-c526571/ce-fault-recovery.json`.

Nach dem neuen Kontextstart sind eigene Home-Rechte, Symlink und Konfiguration
erhalten, die alten persönlichen Mqueues dagegen verschwunden. Das zuvor mit
frischer Alpha-Adminfreigabe gemeinsam veröffentlichte `ed` (Paketversion
`1.21.1-1`) führt nun einen echten Editierauftrag aus. Vor dem Kontextneustart
war es trotz Veröffentlichung bewusst noch nicht verfügbar; `linux status`
meldete die ausstehende Aktivierung. Persönliche Paketinstallation und
vollständiger gepaarter Neustart bleiben die folgenden Abnahmeschritte.

### Persönliches Paket und Passwortwechsel bestätigt

Beta veröffentlicht `hello=2.10-5` als persönlichen Paketbestand, gebunden an
Benutzer/Seriennummer 11/11 und die gemeinsame `ed`-Generation. Alpha gibt den
Plan frisch über AOSP frei, bleibt dabei aber CE-gesperrt und ohne Kontext;
die gemeinsame Auswahl und Vordergrundidentität 11 ändern sich nicht.
Beleg: `private-package-publication-proof.json` unter `candidate-c526571`.

Nach AOSPs Passwortwechsel macht Betas Originalprozess weiterhin Fortschritt.
Logout entfernt ihn und den Kontext; CE ist `[0]`. Die bekannte GNU-Datei und
die zuvor veröffentlichte private Paketmetadatei liefern auch Gast-root keine
Bytes. Das alte Passwort scheitert, CE bleibt gesperrt. Mit dem neuen Passwort
startet die private Generation: dieselbe GNU-Datei und Paketmetadaten bleiben
identisch, `hello` und `ed` führen echte Programme aus, `packages=current`.
Auch Betas alte Mqueues fehlen im neuen Kontext. Belege:
`password-private-runtime-proof.json`, `private-package-locked-proof.json`,
`private-metadata-readback.json`. Der vollständige Neustart beider VMs und
der anschließende Zugriff beider Benutzer sind damit noch nicht vorweggenommen.

### Sichtbare Bedienung und erster gepaarter Abschluss

`candidate-c526571/qmp-ui-proof.json` ergänzt die Rohereignisprüfung um eine
sichtbare Bedienfolge: QMP-TAB/RET öffnet in Android die Seite „Network &
internet“; die virtuelle relative Maus klickt anschließend auf Zurück und
öffnet wieder die Einstellungsübersicht. Screenshots und UI-XML stimmen überein.
Drücken und Loslassen müssen in getrennten QMP-Eingabeframes erfolgen; ein
anfängliches gemeinsames Event-Batch aktivierte den Klick nicht. Kein
physischer Mac-/HVF-Test wird daraus abgeleitet.

Der Abschluss vor Reboot bestätigt beide ursprünglichen Prozessidentitäten
entfernt, keine persönlichen Kontexte, CE `[0]` und nur Systembenutzer 0 gestartet.
Beide bekannten GNU-Dateien bleiben unlesbar. SystemServer war durchgehend
PID 1373; keine fatalen Signale, FORTIFY-, Watchdog- oder Kontrollkanalfehler.
`pre-reboot-health.json` hält drei frühe Upstream-Einmal-Rückgabewerte getrennt
fest: Recovery-Refresh ohne vorherige pstore-Protokolle, ausdrücklich übersprungener
System-Mainline-Initializer (der aktive Mainline-Initializer endet erfolgreich)
und `misctrl`, dessen gepinnter Quellcode das erfolgreiche boolesche
`SetProperty` in den Exitcode OR-verknüpft. Die gesetzte Eigenschaft wurde mit
Gast-root verifiziert. Diese Meldungen werden nicht als verschwundene Fehler
oder als abstürzende Dauerdienste umgedeutet.

Nach gut 98 Minuten Kernel-Laufzeit endet Android mit `reboot: Power down`;
der Helfer hängt seine bestehende UUID-Disk aus und meldet
`AEGIS_HELPER_SHUTDOWN_CLEAN`. Der Launcher endet mit Code 0. Der erste
vollständige Abschluss ist in `paired-shutdown-1.json` gebunden. `boot-2`
startet anschließend exakt dasselbe Profilpaar ohne Neuanlage; die erneute
Passwort-/Datei-/Paketprüfung steht noch aus.


### Vollständiger gepaarter Neustart und Abschluss bestätigt

Der zweite Boot verwendet dasselbe Profil `f450ef38-db1a-47df-b34c-2e723473d0e8`
mit unverändertem Manifest und denselben Disk-Inodes. Der Helfer lädt seinen
bestehenden Secure-Deletion-Zustand; ADB authentifiziert sich ohne neue
Schlüsselfreigabe. Die Boot-ID ändert sich, SystemServer bleibt während aller
Prüfungen PID 1166. Vor Anmeldung sind beide bekannten GNU-Dateien und Betas
private Paketmetadaten unlesbar, CE ist `[0]`.

Beide jeweils ersten korrekten Anmeldungen funktionieren ohne vorherigen
Aufwärmversuch. In echten GNU-Kontexten stimmen beide 1024-Byte-Dateien exakt
mit ihren früheren SHA-256 überein. Alpha behält Home-Rechte, Symlink und
Konfiguration und führt gemeinsames `ed` aus; Betas privates `hello` fehlt bei
Alpha sowohl als Programm als auch in der Paketdatenbank. Beta verwendet das
neue AOSP-Passwort, erhält identische private Paketmetadaten und führt `hello`
und `ed` aus. UID 1000, leere effektive Capabilities, NoNewPrivs, Seccomp,
SELinux-Domäne und schreibgeschützte Softwarebasis werden erneut bestätigt.
`paired-reboot-proof.json` bindet diese Schritte.

Auch nach diesem Neustart weist AOSP das alte Beta-Passwort ab, ohne CE zu
entsperren. Eine abschließende korrekte Anmeldung und Abmeldung hinterlässt
nur Benutzer 0 gestartet, CE `[0]`, keine Runtime-Kontexte und keine lesbaren
persönlichen Testdateien oder Paketmetadaten. `final-state.json` hält diesen
Zustand fest. Android und KeyMint-Helfer enden anschließend sauber; beide
Launcher-Abschlüsse haben Exitcode 0. Der zweite Boot erreicht gut 30 Minuten
Kernel-Laufzeit, die Bootanimation endet wiederum mit Status 0. Auch die
finalen Logs enthalten keine beobachteten fatalen Signale, FORTIFY-, Watchdog-
oder Runtime-Kontrollkanalfehler. Die eingeordneten Upstream-Einmalmeldungen
werden weiterhin ausdrücklich nicht verschwiegen.

Nach Scan aller sechs Gastlogs ohne vollständiges Testpasswort leert der
Testtreiber seine Passwortpuffer und endet erfolgreich. Beide VMs bleiben
heruntergefahren; das synthetische Profilpaar bleibt samt Nachweisen erhalten.
`candidate-c526571/acceptance.json` bindet 25 Belegdateien mit SHA-256.
[Abnahme und Grenzen](server-acceptance.md) sowie die
[Anleitung für ein eigenes Profil](terminal-quickstart.md) sind der aktuelle
Einstieg. Keine Builds wurden auf GitHub geladen.
