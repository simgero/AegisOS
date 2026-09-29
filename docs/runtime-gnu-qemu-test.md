# GNU-Laufzeiten: Zwei-Benutzer-Test in lokalem QEMU

Am 29. September 2026 wurde erstmals die tatsächliche GNU-Ausführung über
die installierte AEGIS-CLI in zwei persönlichen AOSP-Kontexten nachgewiesen.
Der Ablauf umfasst Benutzerwechsel, Bildschirmsperre, Logout, Passwortwechsel
und den Neustart desselben Android-/KeyMint-Paars. **Dies ist eine begrenzte
Funktionsabnahme; Phase 1 ist noch nicht vollständig implementiert.**

## Ergänzung: private Home-Struktur im Vollbuild d44ccb33

Der [Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260929T101135Z-d44ccb33-f8bf03a3)
des Commits `d44ccb3389889740f19373969a00216807b400a7` wurde am selben Tag
im eigenen lokalen Profil `runtime-d44ccb33` geprüft. UUID:
`a42780d7-97ad-4ae2-ace2-dff7b4c9d634`; AVB-Digest:
`1208721a78ece6f49323c2b364a806d419fa9fb7506cb5aecdaa82600eb96e78`.
Der folgende Nachweis ergänzt den historischen 6a-Durchlauf weiter unten.

Die beiden neuen AOSP-Testbenutzer `10/10` und `11/11` sehen beim ersten
GNU-Zugang jeweils genau die zehn vorgesehenen Home-Verzeichnisse, ohne
Verknüpfungen, mit internem Eigentümer `1000:1000` und Modus `0700`.
Die Prüfungen erfolgen um 10:38:23 und 10:44:30 UTC aus ihren tatsächlich
angemeldeten GNU-Shells auf AOSP-CE, nicht aus unverschlüsselten Root-Fixtures.

Alpha ändert `Books` auf `0750`, entfernt `.cache`, benennt `Downloads` um,
legt an dessen Stelle eine relative Verknüpfung an und schreibt eine
synthetische Konfiguration. Diese eigenen Änderungen bleiben nach
`linux stop`/`linux start`, Benutzerwechsel und vollständigem Neustart
unverändert. Es erfolgt kein nachträgliches Auffüllen oder Zurücksetzen.
Betas Verzeichnisse bleiben separat unverändert; Alphas Konfiguration und
Umbenennung sind dort nicht vorhanden. Die vorherige private `/tmp`-Probe
verschwindet beim Kontextneustart beziehungsweise Reboot.

Auch die gegenseitigen GNU-Datei-/SIGSTOP-Verweigerungen, unterschiedliche
Namespaces und fortlaufenden ursprünglichen Hintergrundprozesse wurden
erneut geprüft. Wechsel über eine zweite authentifizierte CLI und
Bildschirmsperre widerrufen die aktive GNU-PTY. Bei `Asleep` und sicherem
Keyguard bleiben beide CE-Bereiche für ihre eigenen Hintergrundprozesse
entsperrt. Logout beendet hingegen jeweils den ursprünglichen Prozess,
entfernt den Kontext und sperrt CE; Betas Logout lässt Alpha weiterlaufen.

Nach geordnetem Android-/Helper-Stopp bootet dasselbe Profil mit neuer
Boot-ID `3e098bae-7bb4-49e1-808e-69e84f6722bb` und zunächst CE `[0]`.
Enforcing, FBE, authentifiziertes ADB und tatsächliches dm-verity bestehen.
Beide zuvor von GNU geschriebenen Dateien sind bis zur eigenen Anmeldung
unlesbar und anschließend bytegleich. SHA-256 der jeweils 1.024 Bytes:

- Alpha: `af67f767d143d76a797f89ffb7faae09319f6fc12a241776b5fd3c264656ce97`.
- Beta: `3a8c01e79a0ec5cdd2c061013e66e4ccc4889653f8f7633bf487d82301ed8421`.

**Der Anmeldefehler bleibt reproduzierbar:** Betas bestätigte Anmeldung
um 10:51:07 und `runtime=ready` um 10:51:08 werden von einer Shell-Ablehnung
um 10:51:09 gefolgt. Eine frische zweite Anmeldung ermöglicht um 10:51:24
den identischen Dateizugriff. Die neue Anmeldereihenfolge aus `2f29f0ac`
ist in diesem Image nicht enthalten und damit hier nicht abgenommen.

Der korrigierte lokale Resize-Treiber liefert `SIGWINCH` ausdrücklich an
seinen eigenen ADB-Kindprozess und wartet auf einen eindeutigen GNU-Exit-
Nachweis statt auf eine beliebige Prompt-Neuzeichnung. Die tatsächlichen
Größen `36 104` und `28 92` bestehen. Ctrl-C erhält die benutzbare Shell;
eine vollständige Vordergrundprozessgruppen-Prüfung ist damit nicht behauptet.

Abschließend sind beide Benutzer abgemeldet, CE `[0]`, keine persönlichen
Kontexte und `populated 0` unabhängig bestätigt (10:52:22). Die CLI ist
geschlossen. Nach separaten Java-Komponententests wurde das Paar um
10:54:18 sauber heruntergefahren und erhalten; keine Launcher-Promotion.
Passwortwechsel wurde hier nicht erneut geprüft. Pakettransaktionen,
verwaltete Löschung und umfassende IPC-/Systemaufrufprüfungen bleiben offen.

Belege unter `out/full-build-d44ccb33/identity-test/`, einschließlich
`reboot-checkpoint.json`, `reboot-readback.json`, `final-cleanup.json`,
`screen-lock-readback.json` und `SHA256SUMS`:

- `events-accepted.json`: `8fe2ac87a3858f8e05a8da78fb3aa66ffcebfc537525bf0081202c483c2c1521`.
- `result.json`: `fbd17c32ef7858127b5d9a0a4da81e18a65ab96e300e9a17305a7e00fc38e112`.

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
