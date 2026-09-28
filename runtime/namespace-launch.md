# Erzeugen persönlicher Namespaces

Stand: **Quelltext und dreizehn Gerätetests vorbereitet, noch nicht kompiliert oder
ausgeführt.** `libaegis-runtime-namespace` ist ein interner Baustein des noch
fehlenden Brokers. Sie aktiviert keine Runtime und hat keinen öffentlichen
Endpunkt. AOSP-Anmeldung, Seriennummer, CE-Zustand, Mounts, SELinux-Übergänge und
Paketkoordination sind weiterhin gesondert zu integrieren.

## Zweistufiger Start

`aegis_namespace_create` erhält die zuvor autorisierte AOSP-Benutzerkennung und
Seriennummer (0 bis INT32_MAX),
einen vorab geöffneten vertrauenswürdigen statischen ARM64-Setup-Helfer und
einen privaten, namenlosen Unix-SEQPACKET-Kanal. Alle Eltern-Deskriptoren und
Verwaltungsobjekte werden vor `clone3` angelegt. Kind und Pidfd entstehen
gemeinsam. Nach erfolgreicher Erzeugung bleibt daher selbst bei einem späteren
Fehler immer ein beobachtbarer Kindprozess-Handle erhalten.

User-, PID-, Mount-, IPC-, UTS- und Netzwerk-Namespace entstehen zusammen.
Das Kind wartet höchstens zehn Sekunden auf die Freigabe. Die Bibliothek ist
für einen eigenen, einzelnen Broker-Thread im Android-Host vorgesehen. Sie
prüft Rootkennungen, Procfs, Übereinstimmung mit den User-/PID-Namespaces von
Androids Init, zusätzlich denselben Mount-Namespace, vollständige Host-ID-Maps
und die tatsächliche Threadzahl.
SIGCHLD-Autoreaping und zusätzliche Gruppen werden abgewiesen. Der Aufrufer
muss die echte Android-Procfs-Sicht erhalten; diese internen Checks sind kein
Schutz gegen einen bereits kompromittierten Host-Rootprozess.

`aegis_namespace_resume` darf nur einmal aufgerufen werden. Unmittelbar davor
muss der Broker die AOSP-Berechtigung und den CE-Zustand erneut prüfen. Er
bleibt exklusiver Reaper. Die Bibliothek überprüft den Pidfd, verankert einmal
das zugehörige Proc-Verzeichnis, schreibt `setgroups=deny` und beide generierten
UID/GID-Maps und liest alles zurück. Erst dann geht die Freigabe zum Kind.
Jeder Fehler schließt den Startkanal und verhindert weitere Startversuche mit
diesem Handle. Bereits eingesammelte Kinder werden nicht erneut per PID gesucht.

Optional führt `aegis_namespace_prepare` die Map-Prüfung getrennt aus und hält
den Startkanal geschlossen. Dazwischen kann der Broker eine
[persönliche Sicht auf die gemeinsame Basis](base-mounts.md) erzeugen. Der dabei
festgehaltene User-Namespace stammt vom tatsächlichen pausierten Kind. Ein
anschließendes `resume` prüft dessen Lebenszustand erneut und gibt es frei;
ohne vorheriges `prepare` erledigt `resume` beide Schritte wie bisher.

Das Kind entfernt geerbte Deskriptoren und Signaleinstellungen, wechselt auf
die zugeordnete Namespace-UID/GID 0 und setzt die Mountweitergabe rekursiv auf
privat. Nach den ID-Wechseln setzt es ein Eltern-Todessignal und prüft einen
stabilen Pidfd des Brokers. Bis zum `execveat` verwendet der rohe Clone-Pfad
nur Syscalls; der Exec initialisiert Bionic neu. Nur der Kontrollkanal als FD 3,
die numerische Benutzerkennung, die bei Erzeugung gespeicherte Seriennummer
und eine feste Umgebung werden übergeben.

Eine erfolgreiche Freigabe beweist **keinen erfolgreichen Exec oder Runtime-
Start**. Ein Fehler im Kind endet mit Status 125; der Broker muss tatsächliche
Exitdaten bzw. später ein geprüftes Setup-/READY-Protokoll auswerten.
`stop` und `wait` verwenden die bestehende Pidfd-Beobachtung. Bloßes Freigeben
des Handles bestätigt weder Prozessende noch CE-Sperrung.

## Grenzen des Setup-Helfers

Der Helfer muss vertrauenswürdiger Systemcode sein. Die Bibliothek prüft unter
anderem statisches ARM64-ELF, Root-Eigentümer, Schreibrechte, fehlende Set-ID-Bits
und Dateicapabilities. Das ersetzt nicht die Herkunftsprüfung durch den Broker.
Beliebige Benutzerprogramme dürfen nicht als Setup-Helfer übergeben werden.

Die anfängliche Wurzel ist noch die geerbte Android-Dateisystemsicht. Der echte
Helfer muss die vorbereitete schreibgeschützte Basis, persönliche CE-Mounts,
eigenes Procfs/Devpts, Ressourcenlimits und SELinux einrichten, die alte Wurzel
vollständig entfernen und erst dann den persönlichen Aufseher starten.
Dieser Helfer und der Broker sind noch nicht implementiert. Der bestehende
`aegis-runtime-init` verlangt bereits fertige Mounts und ist deshalb noch kein
direktes Exec-Ziel dieser Bibliothek.

## Kernel-Abgleich und Tests

Beim kombinierten Clone gehört der neue User-Namespace zu den zuerst erzeugten
Ressourcen und besitzt die weiteren Namespaces. Zusätzliche Gruppen müssen
bereits **vor** dem Clone entfernt sein: vor der GID-Map ist `setgroups` nicht
nutzbar; nach deren Einrichtung kann die dauerhafte Sperre nicht mehr neu
gesetzt werden. Deshalb verändert die Bibliothek die Gruppen des Aufrufers
nicht selbst. Dies folgt aus der
[Linux-User-Namespace-Dokumentation](https://man7.org/linux/man-pages/man7/user_namespaces.7.html).
Ein ID-Wechsel kann das Eltern-Todessignal löschen; die Reihenfolge folgt
[PR_SET_PDEATHSIG](https://man7.org/linux/man-pages/man2/PR_SET_PDEATHSIG.2const.html).
Die rohe ARM64-Signalstruktur wurde mit den gepinnten Android-16-r1-Headern
[ARM64](https://android.googlesource.com/platform/bionic/+/refs/tags/android-16.0.0_r1/libc/kernel/uapi/asm-arm64/asm/signal.h)
und [generische Struktur](https://android.googlesource.com/platform/bionic/+/refs/tags/android-16.0.0_r1/libc/kernel/uapi/asm-generic/signal.h)
abgeglichen. Der statische ELF-Exec über einen CLOEXEC-Deskriptor verwendet
[execveat mit AT_EMPTY_PATH](https://man7.org/linux/man-pages/man2/execveat.2.html).

Die ersten neun Tests in `AegisRuntimeNativeTests` benötigen den neuen Kernel und
passende durchgesetzte SELinux-Regeln. Sie prüfen Startsperre, echte Maps und
Namespaces zweier gleichzeitig lebender Kinder, feste Umgebung, fehlende
FD-Vererbung, Abbruch, Starttimeout mit bereits eingesammeltem Kind, ungültige
Eingaben/FD-Leaks, geerbte Handles, Gruppen, Autoreaping und Mehrfachthreads.
Der statische Probe-Helfer liest Kernelzustand und wartet kurz auf sein Ende;
er startet keine Benutzerprogramme. Die Tests erzeugen keine AOSP-Konten und
beweisen weder CE-Isolation noch vollständigen Logout oder Mountabbau.

Kompilierung erfolgt ausschließlich auf `aegis-build`. Das Komponenten-Skript
sammelt auch den Probe-Helfer. Nach GitHub-Transport werden Test und Helfer
im lokalen Android-QEMU als Root-eigene Dateien mit Modus 0755 installiert.
Nur der dedizierte Testprozess leert seine eigenen Zusatzgruppen. Fehlende
Kernel-/SELinux-Voraussetzungen sind Testfehler, keine übersprungenen Erfolge.
Hosttests führen diesen neuen nativen Code nicht aus.

Die [persönliche CE-Speicheranbindung](personal-storage.md) nutzt inzwischen
dieselbe unveränderliche ID-/Seriennummernbindung für die Prüfung von AOSPs
Speicherwurzeln und die vorbereitete HOME-Sicht. Auch sie ist noch unkompiliert;
die tatsächliche AOSP-Autorisierung und Lifecycle-Sperre bleiben Brokeraufgaben.
