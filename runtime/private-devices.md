# Privates Geräteverzeichnis der Runtime

Stand: **Quelltext und zwei zusätzliche native Gerätetests vorbereitet, noch
nicht kompiliert, eingehängt oder in der Runtime ausgeführt.** Die bestehende
QEMU-VM läuft weiter mit dem bisherigen Image; ihr Bootabschluss und SELinux
`Enforcing` wurden erneut lesend bestätigt. Der Runtime-Modus bleibt `absent`.

## Vorbereitung durch den Broker

`aegis_namespace_devices_mount` ist nur für den Eigentümer eines vorbereiteten,
noch nicht freigegebenen Namespace-Kontexts zugänglich. Es verwendet dessen
wirklichen User-Namespace und erzeugt ein neues, zunächst detached Tmpfs mit
höchstens 1 MiB und 128 Inodes. Ein Android-Geräteverzeichnis wird nicht kopiert
oder eingebunden. Keine Pfade oder Gerätekennungen werden vom Client angenommen.

Das neue Dateisystem enthält genau folgende Zeichengeräte:

| Name | Major:Minor | Verwendung |
| --- | --- | --- |
| null | 1:3 | Ausgabe verwerfen |
| zero | 1:5 | Nullbytes lesen |
| full | 1:7 | Volles Ausgabegerät |
| random | 1:8 | Kernel-Zufallsquelle |
| urandom | 1:9 | Kernel-Zufallsquelle |
| tty | 5:0 | Kontrollterminal des aufrufenden Prozesses |

Diese Nummern wurden auch mit den bestehenden Knoten im lokalen Android-Gast
abgeglichen. Das bestätigt nicht die Funktion des neuen Geräte-Mounts.
Blockgeräte, Binder, Grafik, Eingabegeräte und echte Host-Terminaldeskriptoren
gehören nicht zum Bestand. `tty` ist der generische Kernelzugang zum jeweiligen
Kontrollterminal, keine Weitergabe eines Android-Terminals.

Daneben entstehen leere Mountpunkte `pts`, `shm` und `mqueue` sowie genau fünf
Links: `ptmx -> pts/ptmx`, `fd -> /proc/self/fd` und die drei Standardstreams
unter `/proc/self/fd/`. Der Dateibestand wird schreibgeschützt und erhält
`nosuid,noexec`, private Mountweitergabe und die ID-Zuordnung dieses Kontexts.
`nodev` ist ausschließlich für diese festgelegte Gerätesicht nicht gesetzt;
andernfalls wären auch die erlaubten Zeichengeräte unbenutzbar.

Inventar, Dateitypen, Rechte, Gerätekennungen, Linkziele, UID/GID, Dateisystem
und Mountflags werden zurückgelesen. Metadaten sind anschließend nicht mehr
beschreibbar; die Zeichengeräte behalten die für ihre Funktion nötigen Zugriffe.
Jeder Aufruf erzeugt eigenen Speicher. Ein Fehler schließt alle hier geöffneten
Referenzen; ein zurückgegebener `CLOEXEC`-Mountdeskriptor gehört dem Aufrufer.
Die bestehende Zehn-Sekunden-Startfrist wird nicht verlängert. Kein Hostpfad
wird durch diese Vorbereitung übermountet.

## Kernel-Abgleich und nächste Einbindung

Im gepinnten Kernel `50eb8d5d443b43f38d6e72f005f1b8601ac88a05` verlangt
[`vfs_mknod`](https://android.googlesource.com/kernel/common/+/50eb8d5d443b43f38d6e72f005f1b8601ac88a05/fs/namei.c)
für diese Gerätedateien `capable(CAP_MKNOD)` und prüft zusätzlich Geräte-Cgroup
und LSM. Der Broker im initialen Android-User-Namespace erstellt sie deshalb
vorab. Ein persönlicher Namespace erhält dadurch keine globalen Geräteprivilegien.
Verweigerte Kernel-/SELinux-Operationen werden nicht umgangen.

[`move_mount`](https://android.googlesource.com/kernel/common/+/50eb8d5d443b43f38d6e72f005f1b8601ac88a05/fs/namespace.c)
prüft Mountrechte im Ziel-Namespace, einen dort liegenden Mountpunkt und eine
eigene oder detached Quelle. Ein vertrauenswürdiger Helfer kann daher prinzipiell
die vom Broker übertragenen detached Mounts übernehmen; SELinux prüft den
Übergang zusätzlich. Das ist ein Quellabgleich, kein gelungener Mountversuch.

Der [Setup-Helfer](namespace-setup.md) ist inzwischen ebenfalls im Quelltext
vorbereitet: eigene Devpts-, Shm- und Mqueue-Dateisysteme, Basis und HOME,
Entfernung der Android-Wurzel vor dem Aufseher-Exec. Kompilierung und echte
Ausführung stehen weiterhin aus. Eigenes Procfs muss aus dem
richtigen Prozesskontext entstehen: Der
[`proc_init_fs_context`](https://android.googlesource.com/kernel/common/+/50eb8d5d443b43f38d6e72f005f1b8601ac88a05/fs/proc/root.c)
verwendet den aktiven PID-Namespace des Aufrufers. Das Procfs des Host-Brokers
wäre deshalb kein Ersatz für die persönliche Prozesssicht.

Der Namespace-Start ruft nun bereits vor dem Setup-Exec `setsid` auf. Damit
werden Sitzung und Prozessgruppe getrennt und kein Kontrollterminal des Brokers
übernommen. Der Probe-Helfer meldet beide IDs. Die spätere Shell erzeugt wie
bisher ihre eigene Sitzung und private PTY erst nach dem Benutzerwechsel.

## Offene praktische Prüfung

Die beiden neuen Tests prüfen im lokalen QEMU-Gast den tatsächlichen frischen
Gerätebestand, Schreibschutz der Metadaten, benutzerspezifische Eigentümer,
harmlose Zugriffe auf `null` und `zero`, getrennte Dateisysteme und FD-Abbau.
Bestehende Tests prüfen zusätzlich Ablehnung vor Maps, nach Kindende und durch
einen fremden Prozess sowie die neue Sitzung/Prozessgruppe beim Exec.
Es werden dabei keine persönlichen AOSP-Benutzer oder CE-Daten geändert.

Mit dem [Startprotokoll](namespace-setup.md) sind jetzt **50 native Gerätetests
vorbereitet**. Sie sind weder auf
dem Mac kompiliert noch außerhalb des lokalen Android-QEMU auszuführen.
Kompilierung auf `aegis-build`, echte Mountübergabe, SELinux-Regeln, Rootwechsel,
PTY-Isolation und vollständiger Runtime-/Logout-Ablauf stehen weiterhin aus.
