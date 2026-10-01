# HTTP-Befehlszugriff auf aegis-build

Der Dienst führt frei formulierte Bash-Befehle auf dem Build-Server aus.
Adresse: `http://100.122.101.48:8787`, ausschließlich am Tailscale-Interface.
Der Transport zwischen Tailscale-Geräten ist verschlüsselt; der Dienst selbst
terminiert kein TLS. Keine öffentliche Freigabe, kein Funnel und keine
Firewallöffnung werden eingerichtet. Zugriff erfordert zusätzlich ein zufällig
erzeugtes Passwort (256 Bit); HTTP Basic mit Benutzer `codex` oder Bearer.

Der Dienst läuft als bisheriger SSH-Benutzer `simeongerodetti`, einschließlich
seiner vorhandenen passwortlosen sudo-Rechte. **Das API-Passwort gewährt damit
administrativen Zugriff auf den Build-Server.** Die API ist keine Sandbox.
Tailscale-Zugriffsregeln gelten zusätzlich. Keine interaktive TTY; `sudo -n`
verwenden. Auch SSH zu anderen Hosts ist als Shell-Befehl möglich, sofern der
Dienstbenutzer dafür bereits Schlüssel und bestätigte Hostkeys besitzt.

## Verwendung

Das Passwort liegt auf dem Server in `/etc/aegis-build-http/password` (root,
0600). Nicht in Git, Chat, Shell-Argumenten oder Logs speichern. Der Installer
legt zusätzlich `/etc/aegis-build-http/client.curl` für lokale root-Aufrufe an.
Auf dem Mac kann eine private Kopie in `out/build-http/password` verwendet werden.

```bash
python3 tools/build-http/client.py --password-file out/build-http/password run \
  'id; sudo -n id; df -h /srv/aegis'

python3 tools/build-http/client.py --password-file out/build-http/password run \
  'journalctl -u aegis-build -n 100 --no-pager'

python3 tools/build-http/client.py --password-file out/build-http/password run \
  'sudo -n bash scripts/start-components.sh FULL_COMMIT' --cwd /PATH/TO/CHECKOUT --detach
```

Quellen und Build-Artefakte werden weiterhin über GitHub transportiert.
Ein Vollbuild kann wie bisher den vorhandenen GitHub-Zugang nutzen:
`gh auth token --hostname github.com | sudo -n sh build.sh --token-stdin COMMIT`.
Die API führt Bash mit `pipefail` aus; für Abbruch beim ersten Skriptfehler
explizit `set -e` verwenden. Kein Loginprofil wird geladen.

Der Client unterstützt `--cwd`, `--env KEY=VALUE`, `--stdin`, `--timeout`,
`--detach` und `--request-id`. Weitere Aktionen: `health`, `list`, `get ID`,
`follow ID`, `cancel ID`, `delete ID`. `run` wartet standardmäßig, streamt beide
Ausgaben durch Polling und übernimmt den Exit-Code. Strg-C trennt den Client;
der Job läuft weiter. `follow` liest ab Anfang. Die ID wird vor dem POST
angezeigt; bei unklarem Netzwerkausgang dieselbe ID und denselben Auftrag erneut
senden, um Doppelstarts zu vermeiden. Die Garantie gilt, solange der Job nicht
gelöscht wurde.

## HTTP-Vertrag

Alle Endpunkte verlangen Authentifizierung. POST-Jobdaten sind JSON:

```json
{
  "command": "printf 'hello'; printf 'diagnostic' >&2; exit 7",
  "cwd": "/srv/aegis",
  "env": {"EXAMPLE": "value"},
  "stdin": "",
  "timeout": 3600,
  "wait": 10,
  "request_id": "0123456789abcdef0123456789abcdef"
}
```

| Methode / Pfad | Funktion |
| --- | --- |
| `GET /health` | Authentifizierter Gesundheitscheck |
| `POST /v1/jobs` | Start; HTTP 200 bei Abschluss während `wait`, sonst 202 |
| `GET /v1/jobs` | Jobinventar mit Status |
| `GET /v1/jobs/ID` | Status, Exit-Code, stdout/stderr |
| `GET /v1/jobs/ID?stdout_offset=N&stderr_offset=M` | Nächste Ausgabe ab Byteposition |
| `POST /v1/jobs/ID/cancel` | Prozessgruppe abbrechen; Abschluss anschließend abfragen |
| `DELETE /v1/jobs/ID` | Abgeschlossenen Job und seine Logs löschen |

Antwortfelder: `id`, `state`, `exit_code`, Zeitstempel, `stdout`, `stderr`,
`stdout_next`, `stderr_next` und die jeweilige `_size`. Jede Antwort enthält
höchstens 64 KiB pro Stream. Für verlustfreie Bytes gibt es zusätzlich
`stdout_base64` / `stderr_base64`; Textfelder ersetzen ungültiges UTF-8.
Abschlusszustände: `finished` (auch bei Exit-Code ungleich null), `failed`,
`timed_out`, `cancelled`, `output_limit`, `interrupted`.

Grenzen: 4 gleichzeitig laufende Jobs, 32 HTTP-Handler, 1 MiB Anfrage,
16 MiB Ausgabe pro Stream, 24 Stunden pro Job, höchstens 30 Sekunden `wait`,
1000 gespeicherte Jobs. Standard-Timeout: eine Stunde. Bei Ausgabelimit wird
der Job beendet. Abgeschlossene Jobs explizit löschen, bevor Speicherlimits
erreicht sind (HTTP 507); volle Parallelität liefert HTTP 429.

Jobs und Ausgaben liegen unter `/var/lib/aegis-build-http` (0700). Befehlszeilen
und Ausgaben werden gespeichert; Geheimnisse deshalb möglichst über `stdin`
oder vorhandene Credential-Dateien zuführen und nicht ausgeben. `stdin` und
zusätzliche Umgebungsvariablen werden nicht als Klartext in Jobmetadaten
persistiert. Das API-Passwort wird nicht an Befehle vererbt; die Serverkonfiguration
enthält ausschließlich seinen SHA-256-Hash (für das zufällige starke Passwort).

Jobs überstehen HTTP-/Client-Abbrüche. Ein Dienst-/Serverneustart beendet direkte
Jobprozesse; zuvor laufende Jobs erhalten `interrupted`. Mit `systemd-run`
gestartete separate Builddienste haben ihren eigenen Lebenszyklus. Ein Abbruch
des API-Startjobs stoppt solche Dienste nicht: dafür explizit z.B.
`sudo -n systemctl stop aegis-components` aufrufen. Absichtlich aus der
Prozessgruppe gelöste Prozesse sind ebenfalls separat zu verwalten.

## Installation und Betrieb

Aus einem über GitHub bezogenen Checkout auf dem Build-Server:

```bash
sudo -n bash tools/build-http/install.sh simeongerodetti 100.122.101.48
systemctl status aegis-build-http --no-pager
sudo -n journalctl -u aegis-build-http -n 50 --no-pager
```

Der Installer prüft die lokale Tailscale-Adresse, verwendet vorhandenes Python
und systemd, generiert das Passwort nur beim ersten Mal und aktiviert Autostart.
Für ein Update zunächst laufende API-Jobs prüfen; danach explizit mit
`sudo -n env AEGIS_HTTP_RESTART=1 bash tools/build-http/install.sh ...` installieren.
Unabhängig gestartete Builddienste bleiben bei einem API-Neustart bestehen.
Deaktivierung: `sudo -n systemctl disable --now aegis-build-http`.

Tests: `python3 -m unittest discover -s tests -p test_build_http.py -v`.

## Verifizierte Inbetriebnahme am 1. Oktober 2026

Installierter Quellcommit: `efe5dc9925cb7495051b4c4b48a666e739adb790`.
`aegis-build-http.service` ist aktiviert, der Listener bindet ausschließlich
`100.122.101.48:8787`. Acht Integrationstests bestehen sowohl auf macOS als auch
auf dem Ubuntu-Builder. Vom Mac über Tailscale wurden zusätzlich HTTP 401 ohne
Passwort, normaler Benutzer und `sudo -n id` als root, stdin/Umgebungsvariablen,
getrennte Ausgaben, Exit-Code 7, idempotente Wiederholung sowie Abbruch und
Timeout von `sudo -n sleep` bestätigt. Die entsprechenden Smoke-Testjobs wurden
gelöscht. Keine echten Builds wurden für diese Prüfung gestartet.

Die lokale Passwortkopie liegt in `out/build-http/password` (0600, Git-ignoriert).
Ein echter Serverneustart wurde nicht durchgeführt; der systemd-Autostart ist
konfiguriert und als `enabled` geprüft.
