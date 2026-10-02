# Abnahme der fünf Terminal-Meilensteine auf dem Buildserver

Stand: 1. Oktober 2026. Der ARM64-Entwicklungsprototyp hat den vollständigen
Zwei-Benutzer-Ablauf direkt auf dem Linux-Buildserver in QEMU/TCG bestanden.
AOSP bleibt die einzige Instanz für Identitäten, Passwortprüfung und CE-Schlüssel.
Builds und Prüfbelege bleiben lokal; sämtliche Änderungen werden lokal
committet. Es wurde kein Build nach GitHub geladen.

[Benutzung und konkreter Startbefehl](terminal-quickstart.md) ·
[Buildverfahren und Diagnoseverlauf](server-development.md) ·
[Wiederholbarer interaktiver Testtreiber](runtime-gnu-test-driver.md)

## Geprüftes System

- Vollimage: `c52657113becde42d735669d4d64405c7740cc74`.
- Lokaler Build: `/srv/aegis/runs/local-20261001T185629Z-c5265711-Ox6o39`,
  `LOCAL_BUILD_VERIFIED`, systemd Exitcode 0.
- AOSP `android-16.0.0_r1`, gepinnter Manifeststand und Kernel-/Runtimebelege
  im Build-Run; Debian 13.7, glibc 2.41.
- ARM64 unter QEMU 10.2.1/TCG, acht Gast-CPUs, 4096 MiB Android-RAM;
  separater KeyMint-Helfer, SELinux Enforcing, `ro.adb.secure=1`.
- Abnahmeprofil: `f450ef38-db1a-47df-b34c-2e723473d0e8`.
- Nachweise: `out/server-stability/candidate-c526571/`.
  `acceptance.json` bindet 25 Prüfbelege mit SHA-256. Die JSON-Dateien und
  Gastlogs liegen ausschließlich auf diesem Server.

## Ergebnisse

| Meilenstein | Tatsächlicher Nachweis | Hauptbelege im Nachweisverzeichnis |
| --- | --- | --- |
| QEMU, Bildschirm, Eingabe, ADB | Sichtbare Tastaturnavigation und Mausklick in Android; 720 × 1280; binärer ADB-Rundlauf mit 262144 identischen Bytes. Authentifiziertes ADB verbindet sich nach Reboot ohne neue Schlüsselfreigabe. | `qmp-ui-proof.json`, `input-display-proof.json`, `adb-binary-proof.json`, `post-reboot-initial.json` |
| AEGIS/AOSP-Anmeldung | Alpha 10/10 als Admin und Beta 11/11 als normaler Benutzer über CLI angelegt. Beide ersten Anmeldungen vor und nach Reboot funktionieren ohne Aufwärmversuch. Vor Passwortprüfung keine neue CE- oder Runtimefreigabe. Passwortwechsel durch AOSP; altes Passwort vor und nach Reboot abgewiesen. | `first-login-proof.json`, `password-private-runtime-proof.json`, `paired-reboot-proof.json`, `identity-test/events.json` |
| Gemeinsame GNU-Basis, private Kontexte | Beide Benutzer führen GNU-Werkzeuge als Runtime-UID 1000 aus; unterschiedliche Host-UIDs und sechs getrennte Namensräume. Private HOME-Dateien, Rechte, Symlink und Konfiguration bleiben erhalten. Terminalgrößenwechsel und Strg+C funktionieren. | `identity-test/events.json`, `two-user-isolation-proof.json` |
| Pakete und Isolation | Gemeinsames `ed=1.21.1-1`, privates Beta-`hello=2.10-5`. Falsche und Nicht-Adminfreigaben veröffentlichen nichts und erhalten beide Originalprozesse. Frische Adminfreigabe für Betas Paket entsperrt Alpha nicht. Alpha erhält auch nach Reboot kein privates `hello`; Beta führt beide Programme aus. Gegenseitige Dateizugriffe, Prozesssignale und fremde POSIX-Mqueues werden aus echten GNU-Prozessen abgewiesen. | `package-negative-proof.json`, `private-package-publication-proof.json`, `two-user-isolation-proof.json`, `paired-reboot-proof.json` |
| Abmeldung und gepaarte Persistenz | Originalprozesse und Kontexte werden entfernt, CE gesperrt, bekannte Dateien und private Paketmetadaten liefern keine Bytes. Android und Helper werden beide vollständig beendet. Nach Start desselben Profilpaars sind die Dateien bytegleich und gemeinsame/private Software unverändert ausführbar. | `identity-test/reboot-checkpoint.json`, `paired-shutdown-1.json`, `post-reboot-initial.json`, `paired-reboot-proof.json`, `private-metadata-after-reboot.json` |

Die Boot-ID wechselt von `18473c21-26de-42f5-8684-9d9183e0c73b` zu
`a67e163c-5c04-41f3-ae78-e31d7bb069cf`. Profilmanifest und Disk-Inodes bleiben
identisch. Es erfolgt keine Profilneuanlage, kein Zurücksetzen und kein erneutes
Schreiben der Testdateien. Beide jeweiligen ersten Post-Reboot-Anmeldungen
werden verzögert nochmals geprüft und führen anschließend echte GNU-Befehle aus.

| Datei | Unveränderter SHA-256 vor und nach Reboot |
| --- | --- |
| Alpha, 1024 Bytes | `caeb5fbd415fc4f31c4c7712552a4d755a725cba96ff05e8ff715e36bb0dcf49` |
| Beta, 1024 Bytes | `678484e6f8e8c6606439831ffe57aca97969a085a2869ccccb82a941226eeb52` |

## Behobene Fehler und Regressionen

Der ursprüngliche CE-Timeout führt jetzt zu einem ausdrücklich ausstehenden
Sperrzustand. Ein offener Test-Dateideskriptor provoziert weiterhin EBUSY;
Logout behauptet keinen Erfolg, ein weiterer Login erhält noch nicht einmal
eine Passwortabfrage oder Runtimefreigabe. Nach Freigabe des Deskriptors wird
der Schlüsselentzug bestätigt, bevor eine neue AOSP-Authentifizierung möglich
ist. Falsches Passwort bleibt negativ, richtiges Passwort liefert dieselbe
GNU-Datei. SystemServer übersteht die gesamte Fehlerfolge unverändert.
Beleg: `ce-fault-recovery.json`.

Die folgenden Aussagen beziehen sich auf die damaligen beiden Abnahme-Boots.
Ein späterer `d149766`-Boot zeigt erneut einen FORTIFY-Absturz trotz Exitcode 0;
siehe [aktuelle vollständige Abnahme](phase-1-acceptance-progress.md).

Die Bootanimation wartet jetzt auf ihren Renderthread und beendet sich in
beiden Boots mit Status 0. QEMU verwendet den gepinnten AOSP-Hardwarezeitfaktor
für nichtnative Emulation; der Watchdog bleibt aktiv. Der CLI-Zustandswechsel
hat ein begrenztes TCG-Budget. Die CE-Eviction-Frist bleibt unverändert.
Paketabbruch erhält ein ausreichendes begrenztes Cleanup-Budget; beide vorher
problematischen Negativfreigaben verlieren im neuen Build weder Kontrollkanal
noch GNU-Kontexte. Die genauen früheren Diagnosegrenzen stehen im Verlauf.

Keine fatalen Signale, FORTIFY-, Watchdog- oder Runtime-Kontrollkanalfehler sind
in den beiden abgenommenen Boots beobachtet. SystemServer bleibt innerhalb
des ersten Boots PID 1373 und im zweiten PID 1166. Drei frühe Upstream-
Einmal-Rückgabewerte sind in `pre-reboot-health.json` ausdrücklich eingeordnet;
es wird kein fehlerfreies Gesamtlog behauptet. Der erste Boot endet nach gut
98 Minuten Kernel-Laufzeit mit Android-Power-down und sauberem Helper-Abschluss.
Auch der zweite Boot endet nach gut 30 Minuten sauber für beide VMs; beide
Launcher liefern Exitcode 0. `final-state.json` bestätigt vor diesem Abschluss
CE `[0]`, nur Systembenutzer 0 gestartet, keine Runtime-Kontexte, beide Dateien
und private Paketmetadaten unlesbar. `paired-shutdown-2.json` bindet die finalen
Logs. Das Profil bleibt erhalten, beide VMs sind gestoppt.

## Komponenten und Grenzen

Der Hostlauf führt 280 Tests aus: 276 bestehen, vier ausschließlich für macOS
werden übersprungen. Im aktuellen Image bestehen **177/177 Java-Gerätetests**.
Drei unveränderte erschöpfende User-ID-Tests bestanden bereits im früheren
vollständigen 180-Test-Lauf; ebenso **44/44 native CE-/Namespace-/Memory-Tests**.
`prior-component-source-binding.json` bindet die unveränderten Quellobjekte
und früheren Logs. Diese früheren Ausführungen werden nicht als neuer nativer
Volltest im aktuellen Image ausgegeben.

Diese Abnahme betrifft die fünf beauftragten Terminal-Meilensteine auf diesem
Server. Sie ist keine Produktions-Sicherheitszertifizierung oder erschöpfende
Syscall-/Nebenläufigkeitsprüfung. Stromausfall, Imagemigration, Mac/HVF,
Hardware-Root-of-Trust sowie die vollständige Update-/Entfernungs-/Benutzer-
ID-Wiederverwendungsmatrix werden nicht als durch diesen Lauf abgenommen
behauptet. Der Software-TPM und Entwicklungs-Rootzugang vertrauen dem Host.

Die synthetischen Passwörter wurden nur im Testprozess gehalten und nicht
ausgegeben. Nach dem finalen Scan aller sechs Android-, Helper- und Logcat-Logs
wurden die Passwortpuffer geleert und der Testtreiber erfolgreich beendet. Kein
vollständiges Testpasswort wurde in diesen Logs gefunden; das sagt nichts über alle
denkbaren Speicherabbilder. Für eigene Benutzer ein getrenntes Profil gemäß
Terminalanleitung anlegen; das Abnahmeprofil bleibt als zusammengehöriges Paar
mit seinen Nachweisen erhalten.
