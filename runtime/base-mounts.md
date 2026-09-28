# Persönliche Sicht auf die gemeinsame Basis

Stand: **Quelltext und vier zusätzliche Gerätetests vorbereitet, noch nicht
kompiliert oder ausgeführt.** Die echte Debian-ext4-Basis ist weiterhin weder
gebaut noch im Gast gemountet. Es gibt keinen aktivierten Runtime-Modus.

`aegis_namespace_prepare` kann die UID/GID-Maps jetzt getrennt einrichten und
den Exec-Kanal geschlossen halten. Dabei speichert die Bibliothek den wirklichen
User-Namespace-Deskriptor des Kindes aus dessen verankertem Proc-Verzeichnis.
Der Aufrufer kann keinen fremden Namespace als Zuordnung übergeben. Vor weiteren
Operationen muss genau dieses Kind noch leben und abwartbar sein.

`aegis_namespace_base_mount` übernimmt eine bereits geöffnete, vom Broker
geprüfte Basiswurzel. Der noch fehlende Broker muss zuvor Herkunft, Image-Hash,
Generationsbindung und unveränderliches Backing prüfen und das Dateisystem im
initialen Android-User-Namespace schreibgeschützt bereitstellen. Root-Eigentümer
und ein Readonly-Flag beweisen diese Herkunft nicht.

Die neue Implementierung prüft Verzeichnistyp, Root-Eigentümer, Modus 0755 und
Schreibschutz. Sie klont nur dieses Dateisystem als noch nirgendwo eingehängten
Mount, setzt die ID-Zuordnung des Kindes, `readonly`, `nosuid`, `nodev` und
private Mountweitergabe. Die geprüfte Software-Sicht ist ausführbar, auch wenn
die Quelle `noexec` ist; Quellflags und gespeicherte Dateieigentümer bleiben
unverändert. Tatsächliche Wurzel-Eigentümer, Geräte-/Inode-Identität,
Dateisystemtyp, Mountflags und Quellmetadaten werden anschließend geprüft.

Der zurückgegebene `CLOEXEC`-Deskriptor überdeckt keinen Hostpfad. Der Clone ist
nicht rekursiv: zusätzliche Host-Mounts unterhalb der Basis werden nicht
übernommen. Fehler schließen die neue Mountreferenz und erhalten die Quelle.
Der Broker muss alle ausgegebenen Referenzen bei Abbruch und beim späteren
Abbau schließen. Das Ende des Kindes allein gibt externe Referenzen nicht frei.
Die Startfrist bleibt zehn Sekunden ab Erzeugen des Kindes. Fehlgeschlagene
Vorbereitung erteilt keine Startbereitschaft; der Broker kann innerhalb der
Frist korrigieren oder abbrechen.

Der echte Setup-Helfer muss die Basis über einen geprüften privaten FD-Kanal
erhalten, persönliche Mounts ergänzen und die alte Host-Wurzel vollständig
entfernen. Erst danach dürfen Benutzerprogramme starten. Diese Übergabe, der
Helfer und die SELinux-Integration fehlen noch. Persönliche CE-Verzeichnisse
haben bereits benutzerspezifische AOSP-Hostkennungen und dürfen nicht nochmals
als gemeinsame Basis ID-gemappt werden. Ihre Herkunfts-, Seriennummern-, CE-
und Rechteprüfung sind inzwischen als [CE-Baustein](personal-storage.md) im
Quelltext vorbereitet. Echte Einbindung, AOSP-Koordination und Gastnachweise
stehen weiterhin aus.

Grundlagen sind die [Kernel-ID-Mappings](https://docs.kernel.org/filesystems/idmappings.html),
[open_tree](https://man7.org/linux/man-pages/man2/open_tree.2.html) für den detached
Clone und [mount_setattr](https://man7.org/linux/man-pages/man2/mount_setattr.2.html)
für dessen Attribute. Es gibt keinen Fallback auf `chown`, fehlende
User-Namespaces oder bloßes `chroot`.

Vier zusätzliche Tests verwenden echte Kernelaufrufe mit einem kleinen,
vollständig detached Tmpfs und drei Textdateien. Sie prüfen die geschlossene
Startfreigabe, Schreibschutz, ausführbare Sicht bei unverändertem `noexec`-
Quell-Mount, identische Inodes mit unterschiedlichen Kennungen für zwei Kontexte
sowie Abweisung unpassender Quellen und beendeter Kinder. Die Fixtures nutzen
[fsopen/fsmount](https://man7.org/linux/man-pages/man2/fsopen.2.html), werden an
keinen Gastpfad angehängt und erzeugen keine AOSP-Benutzer oder CE-Verzeichnisse.

Mit CE-, [Gerätevorbereitung](private-devices.md) und
[Startprotokoll](namespace-setup.md) sind inzwischen **50 native Gerätetests**
vorbereitet. Kompilierung findet auf
`aegis-build` statt, Ausführung ausschließlich im lokalen Android-QEMU mit den
nötigen Kernel- und SELinux-Voraussetzungen. Auch nach Bestehen dieser Tmpfs-
Tests bleiben der konkrete ext4-Basis-Mount, Benutzerprogramme, CE-Isolation,
Paketkoordination und vollständiger Ressourcenabbau separat nachzuweisen.
