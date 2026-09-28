# Aufbau der persönlichen Dateisystemsicht

Stand: **Helfer kompiliert und acht Protokoll-/Aufruftests im lokalen QEMU
bestanden; erfolgreiche Mounts und Produktaktivierung stehen aus.** Siehe
[erster Komponentenlauf](../docs/component-tests.md). Die danach ergänzte
F2FS-HOME-Korrektur ist im Komponentenlauf `481f738c` kompiliert; der positive
Mount-Nachweis im Gast steht weiterhin aus.
`aegis-runtime-setup` ergänzt den Namespace-Launcher und den Prozessaufseher.
Der AOSP-Broker, dessen Lebenszyklus-Sperre, die Produktanbindung der Ressourcen-Cgroups und die konkreten
SELinux-Typen/Übergänge und ein ausführbarer Zwei-Benutzer-Ablauf fehlen weiter.
`ro.aegis.runtime.mode` bleibt `absent`.

## Private Übergabe

Nach dem Anlegen des Namespace-Kinds schreibt der Broker die UID/GID-Maps,
bereitet Basis-, CE-HOME- und Geräte-Mount vor und authentifiziert den statischen
Aufseher aus dem schreibgeschützten Systemimage. Nur der Broker hält den anderen
Endpunkt des privaten, namenlosen Unix-SEQPACKET-Paars. Er hält außerdem die
AOSP-Lebenszyklus-Sperre über Vorbereitung und Freigabe des Kindes hinweg.

`aegis_send_setup` sendet fünf feste Datensätze in dieser Reihenfolge:

1. schreibgeschützter, ID-gemappter ext4-Basis-Mount;
2. beschreibbarer CE-HOME-Mount ohne zweite ID-Zuordnung;
3. vorbereitetes, begrenztes Geräte-Tmpfs;
4. authentifizierter statischer ARM64-Aufseher;
5. Abschlussdatensatz ohne Dateideskriptor.

Jeder Datensatz bindet die unveränderliche AOSP-ID **und Seriennummer**.
Die ersten vier übertragen jeweils genau einen Deskriptor. Es gibt keine
Dateipfade, Passwörter, freien Optionen oder Befehle in diesem Protokoll.
Zuordnung und Transport ersetzen keine AOSP-Autorisierung; Dateideskriptoren
von CLI-Clients dürfen hier nie ungeprüft übernommen werden.

Die Übertragung erfolgt vor `aegis_namespace_resume` und verlängert dessen
Zehn-Sekunden-Frist nicht. Der Sender behält seine Referenzen. Bei einem
Sendefehler muss der Broker den Kontext abbrechen; ein teilweise gesendeter
Auftrag wird nicht wiederholt. Der Empfänger hat insgesamt höchstens fünf
Sekunden und veröffentlicht erst nach dem Abschlussdatensatz vier neue
`CLOEXEC`-Deskriptoren ab FD 4. Fehler schließen die bereits empfangenen
Referenzen; der Aufrufer muss auch den fehlerhaften Kanal schließen, damit
noch in dessen Warteschlange liegende Referenzen freigegeben werden.
Ein toter Peer wird auch bei bereits wartenden Datensätzen abgewiesen.

## Voraussetzungen und Mounts

Vor jeder Änderung an Mounts, FDs oder Arbeitsverzeichnis verlangt der Helfer
PID 1, unsichtbaren Elternprozess, neue Sitzung/Prozessgruppe, richtige Maps,
leere Zusatzgruppen, verweigerte `setgroups`, den SELinux-Kontext
`u:r:aegis_runtime_setup:s0` und aktive SELinux-Durchsetzung. Die Kernelobjekte
von PID-, Mount-, IPC-, UTS- und Netzwerk-Namespace müssen dem eigenen
User-Namespace gehören. Er benötigt dafür keinen Ptrace-Zugriff auf Androids
Init-Prozess. Ein direkter Aufruf im Android-Host endet mit Status 78.

Der Helfer prüft Typ, Rechte, Eigentümer und Flags der drei Mountreferenzen.
Die gemeinsame Basis ist fest ext4. Für das bereits durch den Broker auf
Identität und fscrypt geprüfte CE-HOME sind ext4 und F2FS zulässig: Der reale
QEMU-Gast verwendet F2FS für `/data`. Der Helfer merkt sich den Typ der
übergebenen Referenz und verlangt exakt denselben Typ nach dem Rootwechsel.
Andere Dateisystemtypen werden abgewiesen; eine Typprüfung ersetzt keine
Passwortprüfung oder CE-Autorisierung.
Die Identität/Herkunft der Basis und des CE-HOME bleiben Aufgaben des Brokers.
Der Aufseher muss ein schreibgeschütztes ausführbares ARM64-ELF ohne Interpreter,
Set-ID-Bits oder File-Capabilities mit dem exakten SELinux-Dateityp
`aegis_runtime_init_exec` sein. Es gibt keine alternative Shell oder dynamische
Ladeumgebung als Ersatz.

Eine eigene kleine Tmpfs-Arbeitsfläche über `/mnt` entsteht nur im neuen
Mount-Namespace. Darin wird die Basis eingehängt und folgende Sicht aufgebaut:

| Ziel | Herkunft / Grenze |
| --- | --- |
| `/` | Geprüfte gemeinsame ext4-Basis, readonly/nosuid/nodev |
| `/home/user` | Zugeordnetes ext4-/F2FS-CE-HOME, UID/GID 1000, 0700, nosuid/nodev |
| `/dev` | Vorbereitetes privates Geräte-Tmpfs, readonly/nosuid/noexec |
| `/proc` | Aus diesem PID-Namespace erzeugt, `hidepid=2,subset=pid`, nosuid/nodev/noexec |
| `/dev/pts` | Neue Instanz, höchstens 128 PTYs, Slave-Modus 0600, nosuid/noexec |
| `/dev/mqueue` | Sicht des eigenen IPC-Namespace, nosuid/nodev/noexec |
| `/dev/shm` | Eigenes Tmpfs, 64 MiB / 8192 Inodes, 1777, nosuid/nodev/noexec |
| `/tmp` | Eigenes Tmpfs, 128 MiB / 16384 Inodes, 1777, nosuid/nodev |
| `/run` | Eigenes Tmpfs, 16 MiB / 4096 Inodes, nosuid/nodev/noexec |

`/run/user/1000` gehört UID/GID 1000 mit Modus 0700. `/sys` bleibt ein geprüft
leeres Verzeichnis in der schreibgeschützten Basis. Der Host-Sysfs-Baum wird
nicht übernommen. Die PTY-UID entsteht erst beim Öffnen durch den jeweiligen
Shell-Prozess; die GID ist innerhalb dieses Kontexts 1000. Diese begrenzten
Tmpfs-Größen ersetzen keine vollständigen Prozess-/RAM-/CPU-Cgroups.

## Wechsel der Wurzel und Fehlerpfad

Der Helfer verwendet feste, mit `openat2` ohne Symlinks oder Pfadausbruch
aufgelöste Mountpunkte und `move_mount`. Nach Aufbau der Sicht schließt er
sämtliche Directory-/Mountreferenzen, wechselt mit `pivot_root(".", ".")`
auf die neue Wurzel und trennt die alte mit `umount2(..., MNT_DETACH)` ab.
Es gibt keinen Chroot-Ersatzpfad.

Anschließend liest er Typen/Flags, private Verzeichnisrechte, PTMX-Gerät,
PID-1-Procfs, fehlendes `/proc/sys` und die Mountliste zurück. Genau neun
Mountpunkte, keine geteilte Weitergabe und keine zusätzliche Android-Mountsicht
sind zulässig. `/..` muss dieselbe Wurzel bezeichnen. Nur FD 3 für den Broker
und der reine, authentifizierte Code-Dateideskriptor für `execveat` bleiben;
dieser schließt beim Exec. Ein ELF-Code-Mapping ist keine Directory-Referenz
und erlaubt keinen Weg aus der neuen Wurzel.

Der Aufseher prüft danach seinen eigenen SELinux-Kontext und die bestehenden
Startbedingungen, entfernt Capabilities, setzt die Syscall-Grenzen und meldet
erst dann `READY`. Der Setup-Helfer meldet selbst keine Betriebsbereitschaft.
Er hat zusätzlich einen Zehn-Sekunden-Alarm, der nicht in den Aufseher übernommen
wird. Ein im Kernel blockierter Vorgang kann dennoch eine bestätigte externe
Beendigung erfordern. Fehler beenden PID 1; der Broker muss den tatsächlichen
Exit abwarten und **seine** CE-/Mount-/Socketreferenzen schließen. Erst danach
darf er AOSPs bestätigten CE-Schlüsselentzug als Logoutabschluss melden.

Der Kernel-Abgleich beruht auf dem festgelegten Stand
[`50eb8d5d…`, Mounts und pivot_root](https://android.googlesource.com/kernel/common/+/50eb8d5d443b43f38d6e72f005f1b8601ac88a05/fs/namespace.c),
[Procfs](https://android.googlesource.com/kernel/common/+/50eb8d5d443b43f38d6e72f005f1b8601ac88a05/fs/proc/root.c),
[Devpts](https://android.googlesource.com/kernel/common/+/50eb8d5d443b43f38d6e72f005f1b8601ac88a05/fs/devpts/inode.c)
und [Mqueue](https://github.com/aosp-mirror/kernel_common/blob/50eb8d5d443b43f38d6e72f005f1b8601ac88a05/ipc/mqueue.c).
Mqueue wird an den IPC-Namespace gebunden; seine Kernel-Wurzel entsteht bereits
bei dessen Erstellung. Deren Eigentümer darf deshalb nicht aus dem sichtbaren
UID-0-Wert einer späteren Mountoperation hergeleitet werden.

## Noch ausstehender Nachweis

Acht neue native Tests prüfen die echte FD-Übertragung, Eigentumserhalt,
falsche ID/Seriennummer, beschädigte Reihenfolge/Frames/Ancillary-Daten,
unvollständige Übertragung, Peer-Verlust, ungültige Parameter und den direkten
Host-Aufruf. Diese acht Tests bestehen im ersten Komponentenlauf; insgesamt
sind inzwischen **63 native Tests** kompiliert. Diese Protokolltests allein beweisen weder
erfolgreiche Mounts noch Isolation oder Verschlüsselung.

Der Komponenten-Build erzeugt und sammelt den Helfer zusätzlich, aktiviert ihn
aber nicht. Kompilierung ausschließlich auf `aegis-build`; sämtliche nativen
Tests, Rootwechsel-/FD-Ausbruchversuche, getrennte Prozess-/IPC-/PTY-Sichten,
CE-Lock und Zwei-Benutzer-Ablauf ausschließlich im lokalen Android-QEMU mit
neuem Kernel, echter Basis und durchsetzbarer SELinux-Policy.
