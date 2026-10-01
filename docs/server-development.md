# Entwicklung direkt auf dem Buildserver

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
