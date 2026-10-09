# AEGIS – Übergabe dieses Threads an den Cloud-Task

Stand: **1. Oktober 2026, 17:51 Uhr Europe/Berlin**. Diese Zusammenfassung ersetzt
keine Prüfung des aktuellen Checkouts und der laufenden Systeme. Sie trennt
implementierten Code, tatsächlich ausgeführte Tests und offene Anforderungen.
Sie enthält keine Passwörter oder Tokens.

## 1. Sofortüberblick

AEGIS ist ein experimentelles ARM64-AOSP-System für lokalen Mac-QEMU mit einer
persönlichen GNU/Linux-Runtime. CLI, AOSP-Anmeldung, getrennte Linux-Kontexte
und gemeinsame/persönliche Paketinstallationen sind implementiert und in
mehreren realen Gästen teilweise nachgewiesen. **Das Gesamtziel ist nicht
abgenommen.** Insbesondere der Wiederanlauf nach einer verzögerten CE-Sperre
hat einen reproduzierten Fehler bis zum Systemserver-Absturz.

Die drei bisherigen Entwicklungszweige wurden in `main` zusammengeführt und
gepusht. Danach entstand separat ein HTTP-Zugangsbranch für den Buildserver.
Die GitHub-CI von `main` ist inzwischen **fehlgeschlagen**; die letzte kurze
Chatantwort hatte sie noch als laufend bezeichnet.

Bei der aktuellen Übergabeprüfung laufen auf dem Mac **keine QEMU- oder
Testtreiberprozesse mehr**. Die beiden bisherigen ADB-Adressen sind nicht
verbunden. Die gespeicherten Profile sind vorhanden. Alte Prozess-, Port- und
Tool-Sessionangaben aus diesem Thread dürfen daher nicht als live gelten.

## 2. Unverändertes Gesamtziel

1. **QEMU dauerhaft stabil:** Bildschirm, Bedienung, ADB und Gerätedienste;
   Android-Datenpartition und KeyMint-Helferzustand gemeinsam erhalten.
2. **Benutzer und Anmeldung:** `aegis`-CLI für Benutzerverwaltung, Passwort,
   Anmeldung, Wechsel, Abmeldung. AOSP bleibt die einzige Identity Authority.
3. **GNU/Linux:** gemeinsame Debian-/Ubuntu-Basis, persönliche isolierte
   Laufzeitumgebung und Dateien; Phase 1 vollständig über Terminal.
4. **Pakete und Isolation:** gemeinsame und persönliche Softwareverwaltung;
   frische AOSP-Adminfreigabe; keine Zugriffe auf fremde Dateien oder Prozesse.
5. **Ende-zu-Ende-Nachweis:** zwei Benutzer, Passwortwechsel, Programme,
   Wechsel/Logout/Neustart, persistente Daten, Schutz ohne richtige Anmeldung,
   Löschung und sichere Wiederverwendung einer Kennung.

Zusätzlich dokumentiert: eigener AEGIS-Desktop und Terminal-App; langfristig
ein Wayland-Frontend für Linux-Apps mit SurfaceFlinger als gemeinsamer
abschließender Kompositionsinstanz. Das ist eine Architekturrichtung,
**keine implementierte Grafikkompatibilität**. Sie ersetzt nicht die offene
Terminal-Abnahme. Siehe die Architekturunterlagen unten.

## 3. Arbeitsregeln und Cloud-Grenze

- Bisher: Entwicklung im lokalen Projekt auf dem Mac. Der Nutzer möchte die
  weitere Arbeit jetzt ausdrücklich im Cloud-Task fortsetzen.
- **AOSP-/Kernel-/Runtime-Builds auf `aegis-build`.**
- **Boot- und Systemtests weiterhin lokal in QEMU auf dem Mac.** Ein Linux-
  Cloud-Container oder der Buildserver ersetzt diese Abnahme nicht.
- **Quellcode und Build-Artefakte über GitHub transportieren.**
- **Buildserver über die neue HTTP-API im Tailscale-Netz steuern und prüfen.**
  Der Cloud-Task benötigt dafür keine SSH-Sitzung. Befehle laufen mit
  `sudo -n`, falls erhöhte Rechte erforderlich sind; keine Passwörter im Chat.
- Keine globale SELinux-Abschwächung, zweite Passwortdatenbank oder fingierte
  Entsperrflags. Laufzeitstatus allein ist kein Sicherheitsnachweis.
- Höchstens zwei gleichzeitig laufende QEMU-Paare. Ein Paar besteht aus
  Android-Gast und KeyMint-/Secure-Environment-Helfer.
- Keine neuen Subagenten, Chats oder Automationen ohne entsprechende Vorgabe.
- Fehler-/Zeitüberschreitungsbeobachtung bedeutet nicht, dass ein Job beendet
  ist. Vor jedem Ersatzstart den ursprünglichen Job tatsächlich prüfen.

Ein frischer Cloud-Task erhält nicht automatisch den Mac-Dateibaum, dessen
Tailscale-Zugang, HTTP-Passwortdatei, laufende Tool-Sessions oder Geheimnisse.
Zunächst die dort verfügbaren Werkzeuge, Netzwerkregeln und Zugänge prüfen.
Für Mac-QEMU-Tests wird weiterhin ein tatsächlich erreichbarer Mac-Ausführer
benötigt. Keine alten `write_stdin`-Sessionnummern übernehmen.

## 4. Git-Stand und Arbeitsverzeichnisse

Repository: <https://github.com/simgero/AegisOS>

| Stand | Commit | Bedeutung |
| --- | --- | --- |
| `main` und `origin/main` | `d1e1b0dd4b77f1a034ecd4e67813e058bb2c1d56` | Zusammengeführter Entwicklungsstand, bei Übergabe per GitHub geprüft |
| `codex/package-transactions` | `2292c68cfc24ea7755d9ce686e943812ccf41141` | Letzter bisheriger Entwicklungs-Worktree; vollständig in main enthalten |
| letztes gebautes Produkt | `1b1a0a9a3fa4d17b868087065d23df0ebe39c555` | Bestätigter Kernel-Schlüsselentzug; tatsächlicher Timeout-Wiederanlauf fehlerhaft |
| aktueller lokaler Branch `codex/build-http` | `da8582fb7c038efa50e4e604353222c03762fd55` | Nachträglicher Builder-HTTP-Zugang, auch auf origin; noch nicht in main |
| installierter HTTP-Quellstand laut Betriebsdokument | `efe5dc9925cb7495051b4c4b48a666e739adb790` | Zweiter Commit ist Dokumentation des Deployments |

Die bisherigen Branches `codex/aosp-cloud-builder`,
`codex/package-transactions` und `docs/aegisos-foundation` sind in main
enthalten. Der lokale Sicherungscommit `bf9741b` ebenfalls. Bei den Konflikten
wurden die neueren Paket-/Löschfunktionen erhalten; alte Architekturtexte
stehen als ausdrücklich historische Grundlage im Repository.

Lokale Verzeichnisse:

- Primär: `/Users/simeongerodetti/AegisOS`
- Bisheriger Entwicklungs-Worktree:
  `/Users/simeongerodetti/.codex/worktrees/package-transactions/AegisOS`
- `/Volumes/AegisOS-Dev` war zuletzt nicht eingebunden.
- Im primären Checkout lagen vor Erstellung dieser Datei nur die unversionierten
  Ordner `output/` und `website-brand-20260928/`. Das sind Design-/Website-
  Ergebnisse. Sie wurden beim Merge weder gelöscht noch aufgenommen.
- Diese Übergabedatei wird als lokale Markdown-Datei bereitgestellt; ihre
  Erstellung allein macht sie noch nicht in GitHub verfügbar. Im Cloud-Task
  anhängen oder anschließend gezielt versionieren.

Nicht blind auf den alten Worktree wechseln oder zurücksetzen: `main` enthält
zusätzliche Dokumentation und der HTTP-Branch zusätzliche neue Arbeit.

## 5. Aktueller CI-Befund – zuerst bereinigen

[GitHub-Lauf 36884005970](https://github.com/simgero/AegisOS/actions/runs/36884005970)
prüfte `d1e1b0d` und endete mit **failure**.

- Lokal auf macOS: 260 Tests, erfolgreich, **30 übersprungen**. Das entspricht
  nicht einer vollständigen Linux-Prüfung.
- GitHub Ubuntu: 260 Tests, **4 failures, 4 errors, 4 skipped**.
- Vier Audio-Prüfungen scheitern an `FileNotFoundError: xmllint`. In der
  Workflow-Paketinstallation fehlt derzeit das dazugehörige Werkzeug
  (`libxml2-utils` auf Ubuntu). Abhängigkeit ergänzen und erneut prüfen.
- Vier Erfolgsfälle in `tests/test_aosp_worker.py` scheitern beim Kopieren von
  `runtime-policy-source.json` aus dem synthetischen Run-Verzeichnis:
  `test_verified_success`, `test_selected_kernel_receipt_is_published_and_verified`,
  `test_selected_base_receipts_are_published_and_verified` und
  `test_kernel_and_base_receipts_survive_together`.
  Worker-/Compile-Vertrag und Fixture abgleichen; fehlende Belege nicht im
  Produktionspfad ignorieren, nur um den Test grün zu bekommen.
- Shell-/Helper-Syntaxprüfungen bestanden im CI-Lauf.
- Die Warnung zu `actions/checkout@v4`/Node 20 war nicht die Fehlerursache.

Lokale Logs: `out/main-merge-20261001-tests.log`,
`out/main-merge-20261001-ci.log`, `out/main-merge-20261001-ci-failed.log`.

## 6. Buildserver über HTTP steuern

**Regulärer Zugang für den Cloud-Task: `http://100.122.101.48:8787`.**
Der Builder heißt `devserversg` (bisheriger SSH-Alias `aegis-build`) und ist
eine Ubuntu-x86_64-VM unter Hyper-V. Zuletzt vom Nutzer auf 70 GB RAM gestellt;
vor einem neuen Build aktuell prüfen. Ein separates 800-GiB-LVM-Volume unter
`/srv/aegis` ist eingerichtet.

Die HTTP-Schnittstelle ist laut `docs/build-http.md` bereits installiert.
Für diese Umstellung der Übergabe ist keine erneute Serverinstallation nötig.
Der Client liegt in `tools/build-http/client.py`, derzeit auf
`codex/build-http`, noch nicht auf dem oben genannten main-Stand. Diesen
Quellstand im Cloud-Checkout verfügbar machen, bevor die Beispiele ausgeführt
werden. Implementierung und Deployment-Nachweise: `docs/build-http.md`.

### Zugang und Arbeitsverzeichnisse

- API-Dienst: `aegis-build-http.service`; Bindung nur an die Tailscale-IP.
- Befehle laufen als `simeongerodetti` mit dessen passwortlosen sudo-Rechten.
  Die API ist keine Sandbox; ihr Passwort gewährt entsprechend weitreichenden Zugang.
- AOSP: `/srv/aegis/work/aosp`; Runs/Status/Logs: `/srv/aegis/runs/`.
- Bootstrap: `/home/simeongerodetti/aegis-bootstrap/<voller Commit>/`.
- Builddienste: `aegis-build.service`, `aegis-components.service`.
- AOSP-Git-Operationen als Eigentümer `aegis-build`, z.B. `sudo -n -u aegis-build`.

Der Cloud-Ausführer braucht Tailscale-Erreichbarkeit und eine sicher
bereitgestellte Passwortdatei mit Modus `0600`. HTTP selbst hat kein TLS;
der Transport wird hier durch Tailscale geschützt. Keine öffentliche Freigabe
als Ersatz für fehlenden VPN-Zugang einrichten.

Passwortdateien, **nur Pfade, niemals Inhalte übernehmen oder ausgeben**:
Mac `out/build-http/password`, Server `/etc/aegis-build-http/password`.
Der Client verwendet ohne Option `~/.config/aegis-build-http/password`.
Für den Cloud-Task die Datei über den vorgesehenen geheimen Zugang bereitstellen;
sie wird nicht über GitHub oder diese Markdown-Datei übertragen.

### Prüfung und Diagnose über den Client

Die folgenden Beispiele verwenden den Standardpfad für das Passwort und
setzen den Repository-Checkout als lokales Arbeitsverzeichnis voraus. Auf dem
Mac stattdessen vor dem Unterbefehl `--password-file out/build-http/password`
angeben. `health` prüft nur die API, nicht den AOSP-Build.

```bash
python3 tools/build-http/client.py health
python3 tools/build-http/client.py run \
  'systemctl show aegis-build aegis-components -p Id -p ActiveState -p SubState -p InvocationID -p ExecMainStatus; df -h /srv/aegis; free -h' \
  --timeout 30
python3 tools/build-http/client.py run \
  'journalctl -u aegis-build -n 100 --no-pager' --timeout 30
python3 tools/build-http/client.py run \
  'journalctl -u aegis-components -n 100 --no-pager' --timeout 30
python3 tools/build-http/client.py list
```

Vor jedem Buildstart zusätzlich den jüngsten Run und dessen Statusdatei
prüfen. Journale dem aktuellen systemd-InvocationID zuordnen; historische
Erfolgsmeldungen nicht dem neuen Lauf zuschreiben. Diese Übergabe hat keinen
neuen Build gestartet und den aktuellen Server-Jobzustand nicht erneut abgefragt.

### Builds starten und weiterverfolgen

Quellcode zuerst über GitHub bereitstellen und auf dem Builder den exakten
vollen Commit beziehen. Vorhandene, zum Commit passende Bootstrap-Skripte
verwenden. **HTTP ersetzt den Steuerungsweg, nicht GitHub als Transport.**
Keine Skripte oder Images als Befehlsinhalt am GitHub-Weg vorbeischieben.

Für längere Befehle `run … --detach --request-id …` verwenden. Der Client gibt
seine Job-ID vor dem Absenden aus. Die folgende Vorlage erst nach Prüfung des
Bootstrap-Pfads verwenden; beide Platzhalter durch konkrete Werte ersetzen:

```bash
python3 tools/build-http/client.py run \
  'set -e; gh auth token --hostname github.com | sudo -n bash /home/simeongerodetti/aegis-bootstrap/FULL_COMMIT/build.sh --token-stdin FULL_COMMIT' \
  --cwd /srv/aegis --detach --request-id UNIQUE_REQUEST_ID
python3 tools/build-http/client.py get JOB_ID
python3 tools/build-http/client.py follow JOB_ID
```

Die API führt Bash mit `pipefail` aus. Das GitHub-Token wird dabei ausschließlich
auf dem Builder über stdin weitergereicht; nicht ausgeben oder in den
Befehlstext einsetzen. Für Komponenten den tatsächlichen Bootstrap-Aufruf des
gewählten Commits verwenden, nicht blind die Vollbuild-Vorlage übernehmen.

**HTTP-Job und systemd-Buildlauf sind unterschiedliche Vorgänge.** Ein
Startskript kann erfolgreich enden, während der Builddienst weiterarbeitet.
Dann Dienst, Journal und Run-Status mit weiteren HTTP-Befehlen beobachten.
`finished` allein reicht auch beim HTTP-Job nicht: dessen Exitcode prüfen.
Erst `UPLOAD_VERIFIED` und der passende verifizierte GitHub-Release belegen
den abgeschlossenen Artefakttransport.

Bei unklarer POST-Antwort zuerst die ausgegebene Job-ID abfragen; einen Retry
nur mit derselben Request-ID und identischem Inhalt senden. Keinen zweiten
Build mit neuer ID starten, solange der erste nicht geklärt ist. Abbruch des
Clients trennt nur die Verbindung; der Serverjob läuft weiter. Direkte
HTTP-Jobs überstehen keinen API-/Serverneustart (Status `interrupted`);
separat gestartete systemd-Builds haben ihren eigenen Lebenszyklus.
Das Abbrechen eines HTTP-Startjobs stoppt nicht automatisch den Builddienst.

Laut Betriebsdokument bestanden acht Integrationstests auf Mac und Builder
sowie echte HTTP-Smoke-Tests. Diese wurden in diesem Übergabeschritt nicht
wiederholt. Ein echter Serverneustart ist dort noch nicht getestet.

## 7. Wichtigster Produktfehler: CE-Timeout und Wiederanmeldung

### Hintergrund und Implementierung

Im älteren f2-Image meldete Logout Erfolg, obwohl vold die noch beschäftigten
verschlüsselten Inodes erst danach bereinigte. Ein gecachtes
`isCeStorageUnlocked=false` reichte daher als Abschlussnachweis nicht.

Produktcommit `1b1a0a9a` ergänzt:

- `packages/aegis/identity/runtime/fscrypt_eviction.h` und native Tests:
  Erfolg ausschließlich nach tatsächlichem `ABSENT` desselben fscrypt-Keys.
- `scripts/aosp/register-vold-eviction.py` und
  `runtime/aosp-vold-hooks.json`: gepinnte Integration in vold; Policy-Metadaten
  bleiben bei unvollständigem Entzug erhalten; pending CE blockiert erneutes
  Entsperren/Vorbereiten. Vollständiges Image notwendig, nicht nur APK-Austausch.
- `packages/aegis/identity/platform/com/android/server/aegis/AegisCeLock.java`:
  nur EBUSY wird maximal zehn Sekunden wiederholt; andere Fehler werden
  weitergegeben. Erfolg erst nach normalem bestätigtem vold-Return.
- `scripts/aosp/register-runtime-storage.py`: Framework-Einbau unter der
  bestehenden Runtime-/Storage-Lease; Framework-Cache erst nach Erfolg ändern.
- Neun native und sechs Java-Prüfungen bestanden im lokalen f2-Gast.
  Diese Tests allein beweisen nicht das Verhalten der installierten Plattform.

### Tatsächliche Tests im neuen 1b-Gast

1. Vollimage über GitHub geladen, geprüft und lokal gebootet: SELinux Enforcing,
   FBE, dm-verity, authentifiziertes ADB, erwarteter Runtime-Broker.
2. Alpha 10/10 angelegt. Aus echter GNU-Shell eine private 1024-Byte-Datei
   geschrieben. Normale Abmeldung, unlesbare Datei nach Abmeldung,
   falsches Passwort abgewiesen, korrekte Anmeldung und bytegleicher Inhalt
   bestätigt.
3. Dieselbe Datei diagnostisch mit einem Entwickler-root-Prozess außerhalb
   der persönlichen Runtime acht Sekunden offen gehalten. Während des
   gehaltenen FD noch kein Logout-Erfolg; danach vold-Bestätigung und Erfolg.
   Erneute Sperr-/Passwort-/Dateiprüfung bestanden. Dies ist kontrollierte
   Fehlerauslösung, kein unprivilegierter Isolationstest.
4. Datei 45 Sekunden offen gehalten. Framework endet nach seiner Frist mit
   `CE key eviction remains pending`. Runtime-Cgroup leer; CLI meldet
   `Aktion nicht bestätigt`, keine erfolgreiche Sperre. Danach verbleibt
   `CE unlocked users: [0, 10]`; erneutes Logout aus der inzwischen
   unangemeldeten CLI wird abgewiesen.
5. Nach Freigabe des FD erneute Anmeldung versucht: Dienstkontakt bricht ab.
   **Systemserver stirbt tatsächlich**, Kernel-Boot-ID bleibt gleich.

Bei der Übergabe zusätzlich im gespeicherten Log konkret gefunden:

```text
10-01 14:32:41.103  902  902 E AndroidRuntime:
*** FATAL EXCEPTION IN SYSTEM PROCESS: main
RuntimeException: Error receiving broadcast ... android.intent.action.USER_UNLOCKED
in com.android.server.content.SyncManager$5
Caused by: SQLiteCantOpenDatabaseException:
/data/system_ce/10/accounts_ce.db ... SQLITE_CANTOPEN ... No such file or directory
```

Aufrufkette: `SyncManager.onUserUnlocked` → `AccountManagerService` →
`AccountsDb.attachCeDatabase`. Danach neuer `system_server` PID 7046 statt 902.
**Das ist der beobachtete Absturzpfad, noch keine vollständig bewiesene
Ursachenanalyse.** Die naheliegende Ursache ist die widersprüchliche
CE-/User-Unlock-Veröffentlichung nach dem unvollständigen Entzug.

Quellpfade für die nächste Untersuchung:

- `AegisIdentityService.logout()`: widerruft Sitzungsautorität vor Stop/Lock;
  bei Fehler bewusst keine weiter gültige Anmeldung.
- `AospIdentityBackend.stopAndroidUserAndLock()`, `selectLoginTarget()`,
  `authenticate()`/`verify()`.
- Gepatchte `StorageManagerService.lockCeStorage` und Unlock-/Cache-Pfade,
  `UserController.dispatchUserLocking`, LockSettings-/USER_UNLOCKED-Abfolge.
- vold-Pending-Zustand und erneuter bestätigter Entzug vor einem neuen Unlock.

Noch **keine Korrektur dieses Timeout-Wiederanlaufs implementiert**. Nicht mit
Wartezeitverlängerung, pauschalem Exception-Schlucken oder manuellem Setzen
von CE-Flags überdecken. Erforderlich sind ein sicherer, begrenzter Abschluss-
und Wiederholungsweg sowie frische AOSP-Passwortprüfung vor neuer Autorität.

## 8. Weitere belegte Funktionen und offene Punkte

- Ältere echte Zwei-Benutzer-Durchläufe im 020-Gast belegen getrennte GNU-
  Dateien, Passwortwechsel und Persistenz über einen gepaarten Neustart.
  Das ist keine pauschale Abnahme jeder neuen Plattformversion.
- Im f2-Gast: gemeinsame Installation `ed=1.21.1-1`, persönliche Installation
  `hello=2.10-5`, frische Adminfreigabe und Ablehnungen falscher/Nicht-Admin-
  Bestätigung; neue Benutzer übernehmen die gemeinsame Generation.
- Verwaltete CLI-Löschung: vier Ablehnungsfälle und tatsächliche Entfernung
  von Beta einschließlich geprüfter Schlüssel-/Datenpfade und Originalprozess;
  Alpha blieb erhalten. Vollständige ID-Wiederverwendung nach Neustart sowie
  alle Fehler-/Abbruchfälle bleiben separat nachzuweisen.
- Abstrakte Unix-Stream-Sockets: beide Richtungen fremder Verbindungen
  abgewiesen, eigener Ziel-Listener vor und nach jedem Versuch erreichbar.
  Listener anschließend gezielt beendet. Keine allgemeine D-Bus-/IPC-Abnahme.
- **POSIX-Mqueue offen:** normaler GNU-Perl-Prozess erhält bei `mq_open`
  EACCES; AVC `{ search }` von `aegis_runtime_program` auf `mqueue:dir`.
  Gezielte SELinux-Produktkorrektur nötig, danach eigene Queue-Nutzung,
  gegenseitige Isolation und Abbau testen.
- System-V-IPC ist absichtlich deaktiviert (`CONFIG_SYSVIPC` aus, Android-FCM-
  Vorgabe); ENOSYS ist kein bestandener Isolationstest. IPC_NS/POSIX_MQUEUE an.
- Paket-Update, Paketentfernung und vollständige Konkurrenz-/Abbruchmatrix
  sind durch die erfolgreichen Installationen nicht automatisch abgenommen.
- Gerätedienste/UI/ADB und vollständiger Dauerbetrieb müssen am finalen Stand
  erneut geprüft werden. Aktueller Systemserver-Absturz widerspricht Stabilität.
- AOSP-Entwicklungskeys und QEMU-Helfer sind keine Hardware Root of Trust,
  kein produktionsreifes Security-Release und kein Schutz vor dem VM-Host.

## 9. Artefakte und Nachweise finden

Letzter Vollrelease:
<https://github.com/simgero/AegisOS/releases/tag/aosp-20261001T134005Z-1b1a0a9a-98335bc1>

Komponentenrelease:
<https://github.com/simgero/AegisOS/releases/tag/components-20261001T133453Z-1b1a0a9a-1b1a0a9a-zSMhW1>

Weitere gepinnte Eingänge:

- Kernelrun `kernel-20260928T170624Z-64e66d77-qW9h9L`
- Runtimebasis `runtime-base-20260930T152043Z-178cbb6e-r88QIR`
- Basisgeneration `5083dea9077e6e99c796e9ae0f77a4fe769e0695db0e9dc6db79971eca8e8c28`
- KeyMint-Helfer `secure-env-20260928T134804Z-024354c1`

Lokale Pfade relativ zu `/Users/simeongerodetti/AegisOS`:

| Beleg | Pfad |
| --- | --- |
| 1b Boot/Plattformprüfung | `out/full-build-1b1a0a9a/boot-check.log` |
| Vollständiges Gastlog inkl. Absturz | `out/full-build-1b1a0a9a/boot-1/logcat.log` |
| Normaler CE-Zyklus | `out/full-build-1b1a0a9a/identity-test/normal-ce-cycle/events.json` |
| Kurzer FD-Halt | `out/full-build-1b1a0a9a/identity-test/held-short-1/` |
| Timeout/Wiederanmeldefehler | `out/full-build-1b1a0a9a/identity-test/held-long-1/` |
| Live-Treiber-Ereignisdatei bis Prozessende | `out/full-build-1b1a0a9a/identity-test/events.json` |
| 9 native + 6 Java-Prüfungen | `out/components-1b1a0a9a/completion-tests-2/result.json` |
| f2 Pakete/Löschung | `out/full-build-f2d1d0e7/identity-test/product-proof.json` |
| 020 Persistenz/Isolation/Shared-Stand | `out/full-build-020ae750/identity-test/{persistence-proof,two-user-isolation-proof,shared-reconciliation-proof}.json` |
| Abstrakte Unix-Sockets und Mqueue-Befund | `out/full-build-020ae750/identity-test/ipc-socket-proof.json` |

`out/` und `downloads/` sind Git-ignoriert und nicht automatisch in einem
Cloud-Checkout. Raw-Images, Nutzerzustände und KeyMint-Dateien nicht pauschal
als Übergabe hochladen. Benötigte Diagnosebelege gezielt auswählen, auf
vertrauliche Inhalte prüfen und über den vereinbarten Transport bereitstellen.

Wichtige 1b-Identifikatoren:

- Android-Raw-SHA: `3a086cccc234a08fa5309fdc09fd9242f507c11a28cb6450de197736b35688ec`
- vbmeta-Digest: `dced4bd72f74e199f12b947c8f3881467cab9c593da9b6301000e3ee0cb465c6`
- installierter vold-SHA: `9a0fa161683d005e30bcf585d27d1728f6d9cc625daf0a89a00f452e77ed4f5d`
- bisherige Boot-ID: `237369ef-a309-4f38-8174-3d68508c9b0f`
- Profil-ID: `c82f0f24-9cf1-45f8-8693-a8739c35c323`

## 10. Lokale Profile und nicht übertragbare Testzugänge

Gespeicherte Profile unter `out/qemu-profiles/`:
`foundation-2a766ab5`, `runtime-c7401f60`, `runtime-d0b866e1`,
`runtime-020ae750`, `runtime-f2d1d0e7`, `runtime-1b1a0a9a`.

Letzte Zuordnung, **aktuell nicht laufend**:

| Profil | bisheriges ADB | letzter bekannter Inhalt |
| --- | --- | --- |
| `runtime-1b1a0a9a` | `127.0.0.1:15868` | Alpha 10/10; Timeout-/Absturzfall; kein Beta angelegt |
| `runtime-f2d1d0e7` | `127.0.0.1:15867` | Alpha erhalten; Beta im echten Löschtest entfernt |
| `runtime-020ae750` | früher 15868, dann wiederverwendet | Zwei-Benutzer-Persistenz-/IPC-Nachweise; vor 1b-Start sauber gestoppt |

Die Testtreiber erzeugten Passwörter ausschließlich im Prozessspeicher.
Jetzt sind keine Treiberprozesse mehr nachweisbar; deshalb **keine verfügbare
Fortsetzung dieser Credentials annehmen**. Alte Tool-Sessions sind keine
dauerhaften Geheimnisspeicher. Für neue Passworttests gegebenenfalls ein
frisches, eindeutig benanntes synthetisches Profil verwenden; vorhandene
Profile zur Diagnose erhalten, nicht stillschweigend überschreiben.

Ein Profil besteht aus der Android-QCOW2-/Datenlage, dem passenden
`secure-env.ext4`, `profile.json` und den gebundenen Basis-/Kerneldateien.
Diese gehören zusammen. Ein bloßes Kopieren oder Zurücksetzen einer Hälfte
kann persistente Schlüssel-/Datenzuordnung unbrauchbar machen. Bei jetzt
nicht laufenden Profilen ist ein sauberer letzter Shutdown nicht für alle
Profile bewiesen; Wiederstart entsprechend behandeln.

Es wurden auf Nutzerwunsch bereits überflüssige Build-/Testdateien und
redundante Downloadarchive entfernt. Zuletzt rund 2 GB doppelte 1b-Archive;
entpackte Dateien, Profilpaare, Quellcode und Nachweise wurden behalten.
Nicht erneut große Archive herunterladen oder Profile löschen, ohne den
tatsächlichen Bedarf und freie Kapazität zu prüfen.

## 11. Empfohlene Fortsetzung in konkreter Reihenfolge

1. **Cloud-Arbeitsstand und Zugänge feststellen.** Main-Commit prüfen; den
   zusätzlichen HTTP-Branch für den Client verfügbar machen. Keine unbeabsichtigte
   Rückkehr auf einen älteren Entwicklungsstand. Tailscale und HTTP-Passwortdatei
   einrichten, `health` und Builder-Status über HTTP prüfen. Mac-Testausführung
   separat klären; ein erreichbarer Builder ersetzt keinen Mac-Ausführer.
2. **CI reparieren.** `xmllint`-Abhängigkeit und Worker-Fixtures korrigieren;
   vollständigen Linux-Testlauf ausführen. Produktionsbelegpflicht erhalten.
3. **Timeout-Wiederanlauf untersuchen und korrigieren.** Gesicherten
   USER_UNLOCKED-/AccountsDb-Absturz mit den exakten gepatchten AOSP-Pfaden
   abgleichen. Pending-Entzug, gecachter CE-Zustand, Android-Userstart und
   frische Authentifizierung müssen konsistent sein. Noch kein Fix vorhanden.
4. **Gezielte Regressionen hinzufügen.** Normales Logout, kurz offenes File,
   Fristüberschreitung, Freigabe danach, erneuter Abschluss/Login; keine
   Erfolgsmeldung oder Runtimefreigabe bei unbestätigtem CE-Zustand. Kein
   `USER_UNLOCKED`, wenn CE tatsächlich nicht nutzbar ist; kein Systemserver-
   Neustart. Falsches Passwort weiterhin negativ, richtige Datei bytegleich.
5. **Auf dem Builder kompilieren.** Commit zuerst über GitHub bereitstellen;
   exakten Commit über den HTTP-Client starten und den systemd-Lauf über HTTP
   verfolgen; Logs und GitHub-Release verifizieren. Bei Änderungen an
   vold/Framework/SELinux braucht es ein passendes Vollimage für Systemtests.
   Komponenten-/Mocktests sind sinnvolle Vorstufen, aber kein Ersatz.
6. **Auf dem Mac erneut real prüfen.** Frische synthetische Testidentitäten,
   eindeutige Profile, gepaarte Persistenz. Bestehende Belege nicht überschreiben.
7. **Restliche Isolation/Pakete schließen.** POSIX-Mqueue gezielt freigeben
   und testen; Dateisystem-Sockets/D-Bus entsprechend tatsächlichem Ziel;
   Update/Entfernung/Konkurrenz/Abbruch und Kennungswiederverwendung nachweisen.
8. **Gesamtabnahme.** Zwei Benutzer, neue/alte Passwörter, Linux-Programme,
   Shared-/Privatpakete, beide aktiven Kontexte, Logout, echter gepaarter
   Neustart, Löschung/neue Identität, stabile Dienste/UI/ADB. Jede Behauptung
   an konkrete Version, Privilegien und positiven/negativen Beleg binden.

Nicht das Ziel auf die bereits bestandenen Teiltests reduzieren. Der letzte
Entwicklungsabschnitt brachte konkrete neue Fehlernachweise; es besteht kein
belegter externer Blocker, der die Produktarbeit grundsätzlich unmöglich macht.
Ein Cloud-Task ohne Mac-Zugang kann Quellarbeit und Hosttests leisten, die
abschließenden Mac-Systemtests aber nicht als erledigt melden.

## 12. Zuerst zu lesende Repository-Dokumente

- `docs/architecture/phase-1-developer-brief.md` – verbindliche Anforderungen.
- `docs/phase-1-progress.md` – datierte Fortschritte und offene Befunde;
  ältere Abschnitte sind historische Zustände.
- `docs/component-tests.md` – genaue Testumfänge und Grenzen.
- `runtime/aosp-storage-lifecycle.md` – Storage-/Removal-/CE-Integration.
- `docs/runtime-gnu-test-driver.md` – echter CLI-Testtreiber.
- `docs/persistent-qemu.md`, `docs/development-workflow.md` – Betrieb/Transport.
- `docs/build-http.md` – nur im zusätzlichen HTTP-Branch vorhanden.
- `docs/architecture/desktop-and-cli.md`,
  `docs/architecture/shared-display-study.md` – spätere Oberfläche/Grafik.

Frühere `continuation.json`-Dateien unter `out/` können veraltet sein. Beispielsweise
meldet die 1b-Datei noch eine ausstehende Boot-Transition, obwohl Boot und
Timeout-Test bereits erfolgt sind. Diese Übergabe korrigiert das; trotzdem
immer aktuelle Dateien, Prozesszustände und Logs als maßgebliche Belege lesen.
