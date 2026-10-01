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
`scripts/prepare-server-qemu.py DIR --avbtool PATH/avbtool.py` prüft die
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
