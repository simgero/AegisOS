# GNU-Laufzeiten: Zwei-Benutzer-Test in lokalem QEMU

Am 29. September 2026 wurde erstmals die tatsächliche GNU-Ausführung über
die installierte AEGIS-CLI in zwei persönlichen AOSP-Kontexten nachgewiesen.
Der Ablauf umfasst Benutzerwechsel, Bildschirmsperre, Logout, Passwortwechsel
und den Neustart desselben Android-/KeyMint-Paars. **Dies ist eine begrenzte
Funktionsabnahme; Phase 1 ist noch nicht vollständig implementiert.**

## Festgehaltener Stand

- Image-Commit: `6a807692c20e1b277e2369cdbf7841cb98e3b293`.
- [Verifizierter GitHub-Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T092225Z-6a807692-0febd375).
- Profil: `out/qemu-profiles/runtime-6a807692`, UUID
  `8b1e5ec2-3bae-4601-b8ef-eb8f156867d4`.
- Raw-Image SHA-256:
  `1a2f64da6e2eea9a682c2f74dc62d3d01b10630d186add3b4df5f808e06349b5`.
- AVB-Digest:
  `2c5d71f2ebfdbe202bb6181c2f9890b88eeeb084207bfbb4aca5254786dd08ff`.
- Android-Kernel `6.12.18`, Debian `13.7`, glibc `2.41`; keine zusätzliche
  Linux-VM innerhalb Androids.

Kompilierung erfolgte ausschließlich auf `aegis-build`, Artefakttransport
über GitHub und Ausführung ausschließlich im lokalen Mac-QEMU. Der Kandidat
lief ohne sichtbares Fenster. Enforcing, FBE, sichere ADB-Anmeldung und
tatsächliche dm-verity-Tabellen wurden vor und nach dem Neustart geprüft.
Der sichtbare Launcher bleibt unverändert bei `foundation-2a766ab5`.

## Beobachteter Ablauf

Alle Zeiten sind UTC. Passwörter wurden zufällig im Speicher des lokalen
Testtreibers erzeugt und ohne Echo an die interaktive CLI übergeben. Die
Aufzeichnungen enthalten Identitäten und synthetische Dateiproben, keine
gespeicherten Passwörter.

| Prüfung | Ergebnis |
| --- | --- |
| AOSP-Benutzer | Alpha: ID/Seriennummer `10/10`, Administrator. Beta: `11/11`, kein Administrator. Anlage und Passwortprüfung ausschließlich über AOSP. |
| Falsche Anmeldung | Abgewiesen. Unabhängiger CE-Status vor und nach dem Versuch unverändert. |
| Tatsächliche GNU-Shell | Alpha ab 09:46:44, Beta ab 09:53:48. Beide intern UID/GID `1000`, HOME und Arbeitsverzeichnis `/home/user`, Programmdomäne `aegis_runtime_program`. |
| Kontextgrenzen | Verschiedene User-, Mount-, PID-, IPC-, UTS- und Netzwerk-Namespaces. Persönliche Host-UIDs `1007500` und `1107500`. Keine effektiven, erlaubten oder begrenzenden Capabilities; `NoNewPrivs=1`, `Seccomp=2`. Gemeinsames `/usr` schreibgeschützt, private `/tmp` und `/run`, kein zugängliches Host-Cgroup-Dateisystem. |
| Gegenseitiger Dateizugriff | Tatsächliche GNU-Prozesse können die bekannte Datei des anderen weder über dessen AOSP-CE-Pfad, das eigene HOME noch `/proc/<fremde Host-PID>/root` lesen. Beide Eigentümer lesen ihre eigene Datei bytegenau. |
| Gegenseitiger Prozesszugriff | Fremde Prozesse sind in der persönlichen Proc-Sicht nicht sichtbar; `SIGSTOP` an deren Host-PID scheitert. Ein unabhängiger Host-Readback bestätigt denselben tatsächlich weiterlaufenden Prozess vor und nach jedem Versuch. |
| Terminal schließen | GNU-Exitcode `7` erreicht den CLI-Prozess. Alphas ursprünglicher Hintergrundprozess läuft nach geschlossenem und neu geöffnetem Terminal weiter. |
| Aktiver Benutzerwechsel | Frische Anmeldung in einer zweiten CLI wechselt den Vordergrund. Die zuvor aktive GNU-PTY wird widerrufen, während der ursprüngliche Hintergrundprozess weiterläuft. In beide Richtungen geprüft. |
| Bildschirmsperre | Um 10:00:33 wird der offene Terminalkanal widerrufen. Keyguard meldet `showing=true`, `secure=true`, Power `Asleep`; beide Hintergrundprozesse laufen weiter. CE bleibt `[0, 10, 11]`. Keine Behauptung eines Schlüsselentzugs durch Bildschirmsperre. |
| Vollständige Abmeldung | Ohne vorgeschaltetes `linux stop`: Betas Logout um 10:01:37 und Alphas um 10:02:13 beenden die ursprünglichen Hintergrundprozesse, entfernen die jeweiligen Kontext-Cgroups und sperren CE. Betas Abmeldung lässt Alphas ursprünglichen Prozess weiterlaufen. |
| Passwortwechsel | Um 10:03:22 über AOSP. Nach Logout wird das alte Passwort abgewiesen; CE bleibt gesperrt. Das neue Passwort erlaubt unveränderten GNU-Dateizugriff. Auch nach Neustart bleibt das alte Passwort ungültig. |
| Persistenz | Geordneter Android-/Helper-Stopp und Start desselben Profils. Neue Boot-ID, identischer AVB-Digest, beide persönlichen Speicher zunächst gesperrt. Alpha liest seine ursprüngliche Datei um 10:06:37, Beta um 10:07:18 bytegenau aus tatsächlichen GNU-Prozessen. |

Die beiden Probedateien enthalten jeweils 1.024 Bytes. SHA-256:

- Alpha: `c6dfca46ac8fd29ceb2f40001415eeed5b9da41144b69fa063a9c7a8c27f8fef`.
- Beta: `a95da576904a6fdc0402d1e6fa9bbd113e297abfa3f2ac7aaa853da71c66bb0b`.

Nach beiden Bootdurchläufen wird die Unlesbarkeit **derselben vorher von GNU
geschriebenen Dateien** bei gesperrtem CE geprüft. Der anschließende identische
Inhalt nach eigener Anmeldung verhindert, dass ein nie vorhandener oder
gelöschter Dateipfad als Verschlüsselungsnachweis missverstanden wird.

Der abschließende unabhängige Readback um 10:08:17 bestätigt ausschließlich
Systembenutzer 0 gestartet und CE-entsperrt, keine persönlichen Kontextgruppen
und `populated 0`. Die CLI ist geschlossen; das Profil bleibt erhalten.

## Offene Fehler und Grenzen

Nach manchen Wechseln zu einem vorher gestoppten Benutzer wird die soeben
bestätigte Terminalanmeldung nachträglich widerrufen. Drei beobachtete
`linux start`-/Shell-Ablehnungen sind erhalten. Eine frische zweite Anmeldung
funktioniert. Zeitgleiche verspätete Keyguard-/User-Switch-Meldungen und der
globale Widerruf bei Keyguard-Sperre sind ein Untersuchungsansatz, noch kein
abschließend nachgewiesener Auslöser. Ein zuverlässiger Ablauf mit nur einer
Anmeldung ist damit **noch nicht abgenommen**; die Widerrufsprüfung wurde
nicht abgeschwächt.

Der Resize-Testtreiber besitzt keine steuernde Host-PTY und lieferte zunächst
kein `SIGWINCH` an ADB. Nach gezieltem Signal an den überprüften ADB-Kindprozess
zeigen GNU-Readbacks `36 104` und `28 92`. Zusätzlich störte eine asynchrone
Prompt-Neuzeichnung den Testparser. Der unveränderte Resize-Test gilt nicht
als bestanden. Interaktives Ctrl-C und Weiterbenutzung der Shell wurden
beobachtet; das ist keine vollständige Prüfung jeder Vordergrundprozessgruppe.

Diese Tests decken keine Pakettransaktionen, privaten Paketversionen, verwaltete
Benutzerlöschung, sämtliche IPC- oder Systemaufrufangriffe ab. Die später in
`d44ccb33` implementierten Standardordner sind in diesem Image noch nicht
enthalten. Root-Komponententests ersetzen deren reale CE-Provisionierung nicht.
AOSP-Testschlüssel und virtuelles KeyMint sind keine Hardware-Vertrauenswurzel.

## Lokale Belege

`out/full-build-6a807692/identity-test/` enthält `events-accepted.json`, `result.json`,
`reboot-checkpoint.json`, `reboot-readback.json`, `final-cleanup.json`,
`screen-lock-readback.json`, `reentry-rejection.json`, `resize-harness.json`
und `SHA256SUMS`. Die unveränderten Bootlogs liegen in `boot-1/` und `boot-2/`.

- `events-accepted.json`: `dacba4ef4739c42eef0fb1dc9412b0a7182ac2dadadfce897ab9667efadd2ba7`.
- `result.json`: `f1224d9d0ea018102de19c8e5448d491e55b8b65f33e08dea9732aa7e202f88b`.

Die unveränderliche Kopie `events-accepted.json` bezeichnet den abgeschlossenen
obigen Durchlauf; eine spätere Fortsetzung des Testtreibers schreibt nur
`events.json` weiter.
