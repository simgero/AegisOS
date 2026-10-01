# AegisOS

## Aktueller Entwicklungsstand

**Phase 1 ist teilabgenommen, noch nicht vollständig abgeschlossen.**
Die [Zieldefinition und Definition of Done](docs/architecture/phase-1-dod.md)
beschreibt alle Pflichtkriterien für den Abschluss.

Die fünf beauftragten Terminal-Meilensteine sind direkt auf dem Buildserver
im ARM64-QEMU-Prototyp nachgewiesen: [Abnahme und Grenzen](docs/server-acceptance.md).
[Terminalzugang und konkreter Startbefehl](docs/terminal-quickstart.md) beschreiben
die Benutzung. Builds bleiben lokal, Code wird regelmäßig lokal committet.
Build-Artefakte werden erst für einen ausdrücklich vorgesehenen Mac-Test auf
GitHub bereitgestellt. Ältere Implementierungsstände stehen im
[Entwicklungsverlauf](docs/phase-1-progress.md).

- [Architektur und verbindlicher Entwicklerauftrag](docs/architecture/README.md)
- [Bedrohungsmodell](security/THREAT_MODEL.md)

Security-/Privacy-first Betriebssystem auf AOSP-Basis. Der aktuelle Prototyp ist
ein ARM64-AOSP-Build mit nachgewiesenem Start und Terminalbetrieb in QEMU.
Build-, Boot- und Systemabnahme werden getrennt belegt.

## Aktueller Arbeitsablauf

Seit der Nutzeranweisung vom 1. Oktober 2026 laufen Entwicklung, AOSP-Build und
QEMU-Systemtests direkt auf dem Buildserver. [Serverentwicklung](docs/server-development.md)
beschreibt den lokalen Build aus einem unveränderlichen Git-Snapshot,
Imageprüfung, gepaarte Profile und Nachweise. Ziel bleibt
`aegis_qemu_arm64-bp2a-userdebug`; auf diesem Server ohne KVM wird TCG verwendet.
Mac/HVF ist eine spätere gesonderte Plattformprüfung.

Die nachfolgenden Einrichtungs- und Transporttexte sowie der frühere
[Mac-/SSH-Ablauf](docs/development-workflow.md) sind historische Referenzen;
sie autorisieren keine automatischen GitHub-Build-Uploads.

## Dauerhafter Buildserver

Passwortgeschützter Befehlszugriff über Tailscale: [HTTP-API und Client](docs/build-http.md).

`build.sh` startet einen vollständigen Quellcode-Build auf einem
**Ubuntu-24.04- oder Ubuntu-26.04-x86-64-Server mit systemd**, mindestens
**64 GB RAM** und für den Erstbuild **450 GiB freiem Speicher unter `/srv/aegis`**.
Das separate Build-Volume muss bereits eingebunden sein. Ubuntu 26.04 ist für
unseren ersten Build zugelassen, aber noch nicht durch einen Vollbuild validiert.

Auf dem Server mit dem bereits angemeldeten GitHub-Benutzer ausführen. `COMMIT`
steht für den vollständigen geprüften GitHub-Commit. `build.sh` muss aus genau
diesem Commit stammen. Der Token geht über eine Pipe und erscheint weder in
Befehlsargumenten noch in der Terminalausgabe:

```bash
set -o pipefail
sudo -v
gh auth token --hostname github.com | sudo -n sh build.sh --token-stdin COMMIT
```

Der Token benötigt Schreibzugriff auf Releases von `simgero/AegisOS`.
Bei Fine-grained-Tokens entspricht dies **Contents: Read and write**.
Die Unterdateien des Builders und die Gerätekonfiguration werden über GitHub
vom festgelegten Commit geladen. Der vorhandene AOSP-Checkout wird weiterverwendet.

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
sudo sh -c 'cat /srv/aegis/runs/*/status'     # Dauerhafter Status pro Durchlauf
```

## Ablauf und Ergebnisse

1. Host, RAM, freien Speicher und Dateisystem prüfen; doppelte Starts sperren.
2. GitHub-Release-Entwurf erstellen, bevor der große Download beginnt.
3. AOSP und das `repo`-Werkzeug mit den Pins aus `scripts/aosp/config.sh` laden.
4. `aegis_qemu_arm64-bp2a-userdebug` bauen (experimentelle Geräteintegration).
5. Laufzeitdateien aus dem Produktverzeichnis archivieren, in Teile unter 2 GiB
   aufteilen und mit Manifest, Skripten, Paketversionen und Buildlog hochladen.
6. Alle Assets erneut herunterladen und byteweise mit dem Original vergleichen.
7. Erst danach den Release im öffentlichen Repository veröffentlichen und lokal
   **`UPLOAD_VERIFIED`** setzen.

Das erste Baseline-Tag ist `android-16.0.0_r1`. Es ist bewusst festgelegt, aber
**kein aktueller Security-Release**. Diese userdebug-Images verwenden AOSP-Testkeys;
sie sind Entwicklungsartefakte. Ein Boot auf Apple Silicon und die genaue
QEMU-Einbindung sind noch zu testen. Kernel und weitere AOSP-Prebuilts bleiben
Teil des unveränderten Upstream-Builds.

`manifest.xml` enthält die aufgelösten Quell-Commits. Die Paketliste dokumentiert
die installierte Umgebung. Ubuntu-Pakete kommen aus den jeweils aktuellen
Paketquellen: Der Ablauf ist automatisiert, **bitidentische Reproduzierbarkeit
wird noch nicht zugesichert**.

## Laufzeit und Fehler

Der Dienst stoppt nach maximal 24 Stunden. Server, Quellen und Ergebnisse bleiben
für weitere Builds erhalten. Bei einem Fehler bleibt der Release ein Entwurf;
das Skript versucht, das Fehlerlog zusätzlich hochzuladen. Bei Netzwerkausfall
oder hartem Abbruch kann nur die lokale Diagnose vorliegen. Ein abrupt gestoppter
Lauf kann im letzten aktiven Status stehen bleiben. Nur `UPLOAD_VERIFIED`
bestätigt den vollständigen, durch Rückdownload geprüften Upload.

Ein erneuter Start verwendet den Arbeitsordner weiter und beginnt einen neuen
Release-Lauf; er ist kein reiner Upload-Resume. Die AOSP-Baseline nicht ohne
separate Migrationsplanung wechseln.

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
[ARM64-Gerätebasis](https://android.googlesource.com/device/google/cuttlefish/+/refs/tags/android-16.0.0_r1/vsoc_arm64_only/phone/aosp_cf.mk),
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

Für einen ausdrücklich gewählten Wiederholungsbuild kann
`AEGIS_INCREMENTAL_FROM_RUN=/srv/aegis/runs/aosp-RUN` einen bereits mit
`UPLOAD_VERIFIED` abgeschlossenen Vorgänger benennen. `RUN` ist durch dessen
konkreten Namen zu ersetzen. Dann bleiben mindestens **200 GiB frei** erforderlich.
Der Bootstrap prüft vorhandene Produktimages, den aktuellen Manifest-Commit
und die Belegdateien des Vorgängers. Nach dem Download des neuen Projektstands
muss dessen AOSP-/Produktkonfiguration bytegleich mit der des Vorgängers sein;
andernfalls startet kein Build. `--check-storage` prüft Speicher und vorhandene
Belege; die neue Konfiguration wird erst beim tatsächlichen Start überprüft.

Dieser Modus nutzt den vorhandenen Arbeitsbereich weiter. Er löscht keine
Quellen oder Ergebnisse und ist keine Freigabe für einen zweiten leeren
Checkout oder ein geändertes Buildziel. Eine fehlende oder unpassende Referenz
führt zum Abbruch, nicht zu einer stillen Absenkung der Erstbuild-Grenze.

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
