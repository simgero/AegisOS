# AegisOS

Security-/Privacy-first Betriebssystem auf AOSP-Basis. Der erste Meilenstein ist
ein ARM64-AOSP-Build mit nachgewiesenem Start in QEMU auf dem Mac. Ein erfolgreicher
Build und ein erfolgreicher Boot werden getrennt nachgewiesen.

## Verbindlicher Arbeitsablauf

Aktueller Einrichtungsstand und Befehle: [Entwicklungsablauf](docs/development-workflow.md).

1. Entwicklung lokal in diesem Projekt auf dem Mac.
2. Boot- und Systemtests lokal in QEMU auf dem Mac.
3. AOSP-Builds auf dem per SSH-Alias `aegis-build` erreichbaren Server.
4. Synchronisation und Transport über GitHub: Quellcode über Git, Build-Artefakte
   und zugehörige Prüfsummen über GitHub Releases.

Der Ablauf ist: lokal entwickeln → auf GitHub pushen → auf `aegis-build` den
festgelegten Commit beziehen und bauen → Ergebnisse auf GitHub bereitstellen →
auf dem Mac herunterladen, prüfen und in QEMU testen. SSH dient zur Steuerung
und Diagnose des Builders; Projektänderungen und Build-Artefakte werden über
GitHub übertragen.

Das vorhandene Buildskript zielt noch auf `sdk_phone64_arm64-bp2a-userdebug`
für den Android Emulator. Seine Eignung für den vorgesehenen QEMU-Aufbau ist
noch nicht nachgewiesen; Buildziel und Startkonfiguration müssen dafür
abgestimmt werden. Die folgenden Angaben dokumentieren den bisherigen
Cloud-Builder und sind noch keine fertige Anleitung für `aegis-build` und QEMU.

## Bisheriger temporärer Buildserver

`build.sh` startet einen vollständigen Quellcode-Build auf einem dedizierten,
frischen **Ubuntu-24.04-x86-64-Server mit systemd**, mindestens **64 GB RAM**
und **450 GiB freiem Speicher unter `/srv/aegis`**. Ein formatiertes 500-GiB-Volume mit etwa 471 GiB verfügbarem Platz genügt
für diese Vorprüfung. Größere Volumes geben mehr Reserve. ARM64-Docker auf dem Mac ist kein unterstützter AOSP-Buildhost.

Auf dem Server als `root` in Bash ausführen. Ein kurzlebiger GitHub Fine-grained
Token benötigt Zugriff auf **simgero/AegisOS** mit **Contents: Read and write**.
Das Repository ist öffentlich. Der Token wird für Release-Uploads benötigt.
Den Token nicht in URLs oder das Repository schreiben.

```bash
read -rsp 'GitHub-Token: ' GH_TOKEN; echo
export GH_TOKEN
export AEGIS_REF=codex/aosp-cloud-builder
set -o pipefail
curl -fsSL "https://raw.githubusercontent.com/simgero/AegisOS/$AEGIS_REF/build.sh" | sh
unset GH_TOKEN
```

`AEGIS_REF` kann auch ein Git-Commit oder ein Branch sein. Für wiederholbare
Ausführungen denselben vollständigen Commit verwenden. Der Einstieg lädt seine
weiteren Dateien von einem einmal aufgelösten Commit. Der Download des Startscripts ist öffentlich und benötigt keinen Token.
Schreibzugriff für den späteren Upload bleibt erforderlich.

Nach der anfänglichen Paketinstallation und dem Start des Dienstes kann die
SSH-Verbindung geschlossen werden. Davor muss sie bestehen bleiben. Der
Build läuft als Benutzer `aegis-build`, nicht als root. Systemd stellt den
Token als Credential bereit; er wird nur für GitHub-Aufrufe in deren Umgebung
übergeben. Die temporäre root-Kopie liegt mit eingeschränkten Rechten unter
`/run/aegis-bootstrap/github-token` und verschwindet beim Neustart/Löschen.
Der Dienst übersteht eine SSH-Trennung, wird nach einem Serverneustart aber
nicht automatisch neu gestartet.

```bash
journalctl -fu aegis-build       # Fortschritt
systemctl status aegis-build    # Laufender Dienst
cat /srv/aegis/runs/*/status     # Dauerhafter Status pro Durchlauf
```

## Ablauf und Ergebnisse

1. Host, RAM, freien Speicher und Dateisystem prüfen; doppelte Starts sperren.
2. GitHub-Release-Entwurf erstellen, bevor der große Download beginnt.
3. AOSP und das `repo`-Werkzeug mit den Pins aus `scripts/aosp/config.sh` laden.
4. `sdk_phone64_arm64-bp2a-userdebug` bauen (Android Emulator, kein beliebiges QEMU-Board).
5. Laufzeitdateien aus dem Produktverzeichnis archivieren, in Teile unter 2 GiB
   aufteilen und mit Manifest, Skripten, Paketversionen und Buildlog hochladen.
6. Alle Assets erneut herunterladen und byteweise mit dem Original vergleichen.
7. Erst danach den Release im öffentlichen Repository veröffentlichen und lokal
   **`SAFE_TO_DELETE`** setzen.

Das erste Baseline-Tag ist `android-16.0.0_r1`. Es ist bewusst festgelegt, aber
**kein aktueller Security-Release**. Diese userdebug-Images verwenden AOSP-Testkeys;
sie sind Entwicklungsartefakte. Ein Boot auf Apple Silicon und die genaue
Emulator-Einbindung sind noch zu testen. Kernel und weitere AOSP-Prebuilts bleiben
Teil des unveränderten Upstream-Builds.

`manifest.xml` enthält die aufgelösten Quell-Commits. Die Paketliste dokumentiert
die installierte Umgebung. Ubuntu-Pakete kommen aus den jeweils aktuellen
Paketquellen: Der Ablauf ist automatisiert, **bitidentische Reproduzierbarkeit
wird noch nicht zugesichert**.

## Kosten, Fehler und Löschen

Der Dienst stoppt nach maximal 24 Stunden. **Das löscht den Server nicht und
beendet dessen Abrechnung nicht.** Es gibt keine automatische Serverlöschung.
Bei einem Fehler bleibt der Release ein Entwurf; das Script versucht, das
Fehlerlog zusätzlich hochzuladen. Bei Netzwerkausfall oder hartem Abbruch kann
nur die lokale Diagnose vorliegen. Ein abrupt gestoppter Lauf kann auch im
letzten aktiven Status stehen bleiben. Nur `SAFE_TO_DELETE` bestätigt den Upload.

Nach dieser Meldung kannst du den Server löschen. Separat angelegte Volumes,
Snapshots und Backups müssen bei Bedarf ebenfalls entfernt werden. Bei jedem
frischen Server fallen Download und Vollbuild erneut an. Wiederholtes Starten
auf demselben Server verwendet den Arbeitsordner erneut, beginnt aber einen
neuen Release-Lauf; es ist kein reiner Upload-Resume. Versionswechsel auf einem
frischen Server durchführen.

Archive lokal in ein eigenes Verzeichnis herunterladen und prüfen:

```bash
gh release download RELEASE_TAG --repo simgero/AegisOS --dir download
cd download
shasum -a 256 -c SHA256SUMS       # macOS; auf Linux: sha256sum -c SHA256SUMS
cat images.tar.xz.part-* | tar -xJf -
```

## Validierung

CI prüft Shell-Syntax und den Worker-Ablauf mit lokalen Fixtures: Erfolg,
Buildfehler, Uploadfehler, beschädigte Rückdownloads und Veröffentlichungsfehler.
Diese Tests starten keinen AOSP-Build, erstellen keinen Cloudserver und laden
nichts auf GitHub hoch. Der vollständige Build ist erst auf dem Server zu validieren.

Quellen: [AOSP-Anforderungen](https://source.android.com/docs/setup/start/requirements),
[ARM64-Emulatorprodukte](https://android.googlesource.com/device/generic/goldfish/+/refs/tags/android-16.0.0_r1/AndroidProducts.mk),
[Release-Konfiguration](https://android.googlesource.com/platform/build/release/+/refs/tags/android-16.0.0_r1/release_configs/bp2a.textproto),
[GitHub-Release-Grenzen](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases).

Die Speicherprüfung lässt sich ohne Token, Paketinstallation oder Build ausführen:

```bash
sh build.sh --check-storage
```

Ein zusätzliches Volume muss vor dem Start unter `/srv/aegis` eingebunden sein.
Für einen dauerhaften Mount dessen UUID in `/etc/fstab` verwenden. Der Builddienst
fordert den Mount über systemd an. Die Grenze von 450 GiB ist eine Planungsreserve,
keine Zusicherung des tatsächlichen Platzverbrauchs jedes AOSP-Builds.

## Google-Downloadlimits

Der Quellcode wird mit nur einem Netzwerkjob heruntergeladen (`-j1`), interne
Fetch-Wiederholungen sind abgeschaltet. Beim ersten Fehler stoppt der Sync.
Erkennt das Script HTTP 429, endet der Lauf mit `RATE_LIMITED`; es startet
keinen automatischen Wiederholungsversuch. Ein erneuter manueller Start wartet
vor Google-Zugriffen mindestens bis 30 Minuten nach dem letzten 429
(`RATE_LIMIT_WAIT`). Diese Frist bleibt im Arbeitsverzeichnis erhalten.
Die 30 Minuten sind unsere vorsichtige Mindestpause, keine von Google garantierte
Resetzeit. Git/repo liefert hier keinen zuverlässig auswertbaren Retry-After-Header.
Es werden keine IPs, Konten oder Hosts gewechselt, um Limits zu umgehen.
