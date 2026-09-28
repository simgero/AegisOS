# Persönlicher Prozessaufseher

Stand: 28. September 2026. **Quelltext und Gerätetests vorbereitet, noch nicht
kompiliert oder ausgeführt.** Dies ist ein Bestandteil der künftigen Runtime,
keine bereits startbare Linux-Umgebung. Der Identitätsdienst bleibt im Modus
`ro.aegis.runtime.mode=absent`; weder Produktpakete noch ein automatischer
Systemstart aktivieren den neuen Aufseher.

## Zuständigkeit und noch fehlender Aufrufer

`packages/aegis/identity/runtime/` enthält `aegis-runtime-init`: ein statisch
gelinktes ARM64-Programm als PID 1 **innerhalb** eines persönlichen PID-Namespace.
Es besitzt keine Anmeldung, Passwortdatenbank, Adminrechteprüfung oder
öffentliche Socket-Adresse. Der noch zu implementierende AOSP-Broker muss vor
jedem Start den authentifizierten AOSP-Benutzer mit Seriennummer, gültiger
Sitzung und entsperrtem CE-Speicher prüfen. Kennungen im Startaufruf oder
Protokoll sind technische Zuordnungen und erteilen keine Berechtigung.

Der Broker muss vorher insbesondere:

1. separate User-, Mount-, PID-, IPC- und Netzwerk-Namespaces einrichten; der
   User-Namespace liegt unmittelbar unter dem Android-Host-Namespace;
2. die [festgelegten UID/GID-Maps](uid-mapping.md) schreiben, ergänzende Gruppen
   entfernen und `setgroups` dauerhaft verweigern;
3. die gemeinsame Softwaregeneration schreibgeschützt mit `nosuid,nodev`
   einbinden, die alte Host-Wurzel vollständig entfernen und persönliche
   persistente Mounts dem aktuellen CE-Eigentümer zuordnen;
4. ein zum PID-Namespace gehörendes Procfs, eine eigene Devpts-Instanz ohne
   erzwungene fremde PTY-Eigentümer sowie private `/tmp`- und `/run`-Tmpfs-Mounts
   bereitstellen; HOME und `/run/user/1000` gehören UID/GID 1000 mit Modus 0700;
5. begrenzte Gerätedateien, Netzwerkzugriff und Speicher-/Prozessressourcen
   festlegen und SELinux-Übergänge einschließlich der Domain
   `u:r:aegis_runtime_init:s0` implementieren;
6. ausschließlich einen Endpunkt eines namenlosen Unix-`SOCK_SEQPACKET`-Paares
   als FD 3 übergeben. Der andere Endpunkt bleibt beim autorisierten Broker;
   Shells, CLI-Clients und fremde Prozesse erhalten ihn nicht.

Der Aufseher prüft PID/Eltern-PID, reale/effektive/gespeicherte IDs, fehlende
Zusatzgruppen, tatsächliche UID/GID-Maps, Procfs-Sicht, `setgroups`, seine
SELinux-Domain sowie ausgewählte Dateisystemtypen, Mountflags und
Verzeichnisrechte. Direkter Aufruf in Androids Host-PID-Namespace endet mit
Status 78. Es gibt keinen Testschalter zur Umgehung dieser Bedingungen.
Diese Prüfungen ersetzen weder den Broker noch eine vollständige Kontrolle
aller Mounts, der aktiven SELinux-Durchsetzung, CE-Herkunft oder Ressourcen.

Die C-Kennungsdefinition wird gemeinsam mit Java und AOSPs Kontenregister aus
`runtime/uid-map.json` erzeugt. Abweichende erzeugte Quellen verhindern den
vorbereiteten Komponenten-Build.

## Shells und Prozessende

Nach der Vorbereitung entfernt PID 1 die geerbte Umgebung und alle zusätzlichen
Dateideskriptoren. Es setzt `no_new_privs`, leert Capability-Bounding-, Ambient-
und Inheritable-Sets und behält selbst ausschließlich die für Benutzerwechsel
und gezieltes Beenden benötigten Namespace-Capabilities. Normale Kinder erhalten
UID/GID 1000 ohne Capabilities, eine feste Umgebung und HOME `/home/user`.
Set-ID-Dateien erteilen keine AOSP-Berechtigung. Zusätzlich gelten zunächst
256 offene Deskriptoren und ein Prozesslimit von 256 je zugeordneter Real-UID
sowie deaktivierte Core-Dumps. Das ist kein vollständiges Speicher-/CPU-Budget.

Ein Startauftrag enthält höchstens 32 getrennte, NUL-terminierte Argumente in
einem Paket von maximal 8192 Bytes. Das Programm muss als absoluter Pfad
angegeben sein; der Aufseher fügt weder Shell-Auswertung noch PATH-Suche hinzu.
Die Argumente gehen direkt an `execve`. Shell-Auswertung ist möglich, wenn der
autorisierte Client ausdrücklich Bash mit entsprechenden Argumenten startet.

Jedes Kind erzeugt **nach** dem UID-Wechsel eine eigene PTY und Sitzung. Nur der
neu erzeugte Master geht an den Broker; keine Host-Terminal-FDs gelangen ins
Kind. Terminalgröße, interaktive Weiterleitung und Wiederherstellung des
CLI-Terminals bleiben Aufgaben der noch fehlenden Anbindung. Ein privater
Startkanal meldet Fehler, begrenzt die Wartezeit auf fünf Sekunden und schließt
beim erfolgreichen `execve`. `STARTED` gibt PTY und Prozesszuordnung zurück,
garantiert aber weder anhaltende Aktivität noch den fachlichen Erfolg des
Befehls. Der tatsächliche Exitstatus folgt separat.

Bis zu 32 angeforderte Shells werden gleichzeitig verfolgt. Verwaiste Prozesse
werden ebenfalls eingesammelt. Eine beendete Shell entfernt nur ihren Eintrag;
Hintergrundprozesse und AOSP-Sitzung dürfen weiterbestehen. Bash-`exit` ist kein
Logout.

Bei STOP, einem Verwaltungssignal oder verlorenem Kontrollkanal endet PID 1.
Auch beschädigte/wiederholte Aufträge sowie ein nicht mehr beschreibbarer
Antwortkanal beenden den Kontext. Nach dem dokumentierten
[Linux-Verhalten von PID-Namespaces](https://man7.org/linux/man-pages/man7/pid_namespaces.7.html)
beendet der Kernel beim Tod von PID 1 sämtliche Prozesse dieses Namespace.
Der Broker muss dennoch das tatsächliche Ende abwarten, seine PTY-/Mount- und
Namespace-Referenzen schließen und erst dann den AOSP-Logout samt bestätigter
CE-Sperrung abschließen. Der Aufseher meldet niemals selbst „abgemeldet“ oder
„Speicher gesperrt“.

## Bestätigung des Prozessendes auf der Verwaltungsseite

`child.c` und `child.h` ergänzen `libaegis-runtime-child` für den noch fehlenden
Broker. Der vertrauenswürdige Launcher muss das Kind und einen Prozess-Pidfd
gemeinsam mit `CLONE_PIDFD` erzeugen und exklusiv verwalten. Vom Client
übergebene PIDs oder aus altem Zustand neu geöffnete Prozesse sind kein Ersatz.
Die Bibliothek hat keinen öffentlichen Endpunkt und keine AOSP-Autorisierung.
Der Broker muss den Kontext dem authentifizierten Benutzer samt Seriennummer
zuordnen und Starts sowie Paketoperationen vor einer Abmeldung sperren.

Die Bibliothek dupliziert nur einen abwartbaren Kindprozess-Pidfd mit `CLOEXEC`,
ohne einen vorhandenen Exitstatus zu verbrauchen. Beenden erfolgt mittels
`pidfd_send_signal`, niemals über eine numerische PID. Erst ein Exitereignis aus
`waitid(P_PIDFD)` bestätigt das Prozessende. Signalanforderung, Socket-EOF,
Poll-Bereitschaft oder `ESRCH` reichen nicht aus. Ein an anderer Stelle
verbrauchter Status bleibt mit `ECHILD` ein unbestätigter Abschluss.

Warten hat eine monotone Frist von höchstens 60 Sekunden pro Aufruf. Ein Timeout
behält die Referenz für eine erneute Beobachtung; bestätigte Exitdaten bleiben
für weitere Abfragen erhalten. Das Freigeben einer Referenz beendet oder reapet
den Prozess nicht und darf weder `STOPPED` noch Ressourcen-Wiederverwendung
erlauben. Zugriffe auf einen Handle müssen im Broker serialisiert werden.
Die Eigentümerprüfung fragt die Kernel-PID direkt ab: Der gepinnte
[Bionic-Code für getpid](https://android.googlesource.com/platform/bionic/+/refs/tags/android-16.0.0_r1/libc/bionic/getpid.cpp)
verwendet einen TLS-Cache, den ein direkter `clone3`-Aufruf zunächst erbt.
Ein Kind darf seinen geerbten Beobachter nicht zum Signalisieren eines anderen
Kontextes verwenden. Diese Prüfung ersetzt keine AOSP-Autorisierung.

Die Bibliothek ist **noch nicht kompiliert oder im Gast ausgeführt** und
aktiviert keinen Dienst. Launcher, Namespaces, Mounts, PTY-Verwaltung, Broker
und AOSP-Koordination fehlen weiterhin. Ein beliebiges beendetes Kind beweist
keine Namespace-Bereinigung. Auch beim echten Runtime-PID-1 bleiben offene
Verwaltungsreferenzen, Paketoperationen und tatsächliche CE-Sperrung gesondert
zu behandeln. Die Kernel-Semantik wurde anhand von
[pidfd_open](https://man7.org/linux/man-pages/man2/pidfd_open.2.html),
[pidfd_send_signal](https://man7.org/linux/man-pages/man2/pidfd_send_signal.2.html)
und [waitid](https://man7.org/linux/man-pages/man2/waitid.2.html) abgeglichen;
die gepinnten Android-Header enthalten `P_PIDFD` und die verwendeten Aliase.

## Noch notwendige AOSP-Stoppkoordination

Im gepinnten
[UserController](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/am/UserController.java)
ruft `finishUserStopping` die Dienstcallbacks auf. Später verarbeitet
`finishUserStopped` die Stoppbestätigung; `dispatchUserLocking` fordert den
Schlüsselentzug auf dem Vordergrund-Handler an. Der
[SystemServiceManager](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/SystemServiceManager.java)
fängt Ausnahmen der Dienstcallbacks ab und setzt den Ablauf fort.

Daraus folgt: `onUserStopping` kann Bindungen sofort widerrufen, bietet aber
keine bestätigte asynchrone Barriere für den Runtime-Abbau. Eine dort geworfene
Ausnahme verhindert den weiteren Stopp nicht. Der Koordinator muss Logout,
Benutzerlöschung und externe AOSP-Hintergrundstopps abdecken, ohne auf dieselbe
Operationssperre zu warten, die der AOSP-aufrufende Binder-Worker hält. Nach
Timeout dürfen neue Starts oder wiederverwendete Benutzerkennungen keine
verbliebenen Ressourcen übernehmen. Dieser Integrationspfad ist noch nicht
implementiert; der Identitätsdienst bleibt im Modus `absent` aktivierbar.

## Kontrollkanal und Syscall-Grenzen

Das private Little-Endian-Protokoll steht in `control.h`. Aufträge verwenden
streng steigende IDs ungleich null. Nur `STARTED`-Antworten transportieren
einen Deskriptor. Bei abgeschnittenen oder unerwarteten Ancillary-Daten werden
alle empfangenen Deskriptoren geschlossen; Broker-Aufträge dürfen keine
Deskriptoren enthalten. Antworten blockieren nicht und verwenden `MSG_NOSIGNAL`.
Das Protokoll ist keine Authentifizierung.

Ein vererbter ARM64-Seccomp-Filter sperrt unter anderem Namespace-/Mount-
Änderungen, Schlüsselbundzugriff, fremde Prozessinspektion, Kernelmodule, BPF,
Perf und io_uring. `clone3` liefert `ENOSYS`, damit libc auf das überprüfbare
`clone` zurückfallen kann; dort sind neue Namespace-Flags gesperrt. Gewöhnliche
Prozesse und Threads sollen weiter möglich sein. Die tatsächliche glibc-/
Programmmischung ist erst im Gast zu prüfen.

Die zusätzliche Sperrliste ist **keine vollständige Syscall-Allowlist**. Neue
Kernel-Schnittstellen und Programmkompatibilität müssen beim Aktualisieren
erneut geprüft werden. Wie in der
[Linux-Seccomp-Dokumentation](https://man7.org/linux/man-pages/man2/seccomp.2.html)
beschrieben, ersetzt ein Filter keine vollständige Isolation. Dateisystemsicht,
UID-Zuordnung, SELinux und autorisierter Lebenszyklus bleiben notwendige Grenzen.
Paketoperationen mit Adminfreigabe sind hier noch nicht implementiert.

## Build und noch ausstehende Tests

GitHub-Quelltransport und Paketregistrierung nehmen das ganze Verzeichnis mit.
`scripts/aosp/check-identity.sh` baut auf `aegis-build` zusätzlich
`aegis-runtime-init` und `AegisRuntimeNativeTests`. Der Test ist ein ARM64-
Gerätetest ohne Hostvariante. Das gepinnte
[Soong-Testmodul](https://android.googlesource.com/platform/build/soong/+/refs/tags/android-16.0.0_r1/cc/test.go)
legt Test und `data_bins` unter `data/nativetest64/AegisRuntimeNativeTests/` ab.
Beide Dateien werden mit Prüfsummen gesammelt. Der Builder führt sie nicht aus.

Die ersten zwölf vorbereiteten Tests prüfen Argumentgrenzen und Replay-Abweisung, echte
Unix-Socket-FD-Übertragung mit `CLOEXEC`, Deskriptorverluste bei abgeschnittenen
Nachrichten, volle/geschlossene Kanäle, verweigerte direkte Ausführung und
ausgewählte Filterregeln samt Vererbung. Sie sind noch nicht kompiliert oder
ausgeführt. Hosttests prüfen nur Kennungsdateien und den bestehenden Transport.

Zehn weitere Gerätetests verwenden echte `clone3`-Kinder und Pidfds: normaler
Exit, bereits beendetes Kind, gezieltes Beenden, Timeout mit späterer Bestätigung,
Stopp/Weiterlaufen, anderweitig verbrauchter Exitstatus, ungültige/fremde
Deskriptoren samt Leakprüfung, geerbte Beobachter und explizite Referenzfreigabe.
Auch diese Tests sind **noch nicht kompiliert oder ausgeführt**. Die Fixtures
haben keine Runtime-Namespaces und beweisen keinen vollständigen Logout.

Nach Server-Kompilierung und GitHub-Transport sind die nativen Tests ausschließlich
in lokalem Android-QEMU auszuführen. Positive Aufsehertests benötigen außerdem
den neuen Kernel und tatsächlichen Broker samt SELinux-/Mount-Integration:
interaktive PTY, mehrere Shells, Shell-Ende mit weiterlaufendem Hintergrundprozess,
Brokerverlust, volle Kanäle, Startabbruch, STOP, reale Prozessbeendigung,
Mountabbau, zwei gleichzeitige Benutzer und AOSP-Logout mit CE-Sperrung.
Ein grüner Protokolltest beweist diese Abläufe ausdrücklich nicht.
