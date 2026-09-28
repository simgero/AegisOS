# Persönliche Sicht auf die gemeinsame Basis

Stand 28. September 2026: Die Komponenten sind kompiliert; im lokalen Gast
bestehen 83 von 86 nativen Tests. Drei Basis-Mount-Tests scheitern, weil
`open_tree(OPEN_TREE_CLONE)` eine im aktuellen Namespace eingehängte Quelle
verlangt. Die echte Debian-ext4-Basis ist gebaut und im Image enthalten,
aber noch nicht als produktive Runtime im Gast eingebunden.

Die neue Korrektur schafft vor dem ersten persönlichen Kontext einen eigenen
privaten Mount-Namensraum des Brokers. Dessen tatsächlich geöffnetes nsfs-FD
und Prozess-ID bleiben gebunden; fremde/geerbte oder nachträglich gewechselte
Namespaces werden abgewiesen. Ein fehlgeschlagener Initialisierungsschritt
versiegelt den Zustand, statt einen schwächeren Start zu erlauben. User-/PID-
Namespace, Root-Kennungen, Einzelthread und Reaper-Bedingungen bleiben geprüft.

Nur dort hängt der Broker seine zuvor verifizierte schreibgeschützte Basis an
`/mnt` ein. Androids Mount-Baum und Dateien werden nicht verändert. Der
Setup-Helfer überdeckt diesen Pfad in seinem eigenen Namespace und entfernt
später die geerbte Android-Wurzel wie bisher. Die Basisreferenz lebt bis zum
Ende des privaten Namespace und aller Mount-/FD-Referenzen. Der noch nicht
aktivierte Broker erhält diesen Schritt ausdrücklich vor seiner Kontextanlage.

Die bestehenden vier Mounttests verwenden nun dieselbe private Einhängung.
Zwei zusätzliche Tests prüfen die verweigerte Namespace-Übernahme sowie
denselben detached Mount vor/nach Einhängung. Fixtures prüfen zusätzlich
Androids unveränderten `/mnt`-Inode und entfernen nur ihre eigene private
Einhängung. Die neue Korrektur benötigt noch Server-Kompilierung und Gasttests;
sie ist kein bereits bestandener Lauf oder fertiger Linux-Start.

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
zunächst detached Tmpfs und drei Textdateien. Sie prüfen die geschlossene
Startfreigabe, Schreibschutz, ausführbare Sicht bei unverändertem `noexec`-
Quell-Mount, identische Inodes mit unterschiedlichen Kennungen für zwei Kontexte
sowie Abweisung unpassender Quellen und beendeter Kinder. Die Fixtures nutzen
[fsopen/fsmount](https://man7.org/linux/man-pages/man2/fsopen.2.html), werden an
keinen Android-Mount-Baum angehängt, sondern nur in die private Broker-Sicht;
sie erzeugen keine AOSP-Benutzer oder CE-Verzeichnisse.

Mit CE-, [Gerätevorbereitung](private-devices.md) und
[Startprotokoll](namespace-setup.md) sind inzwischen **50 native Gerätetests**
vorbereitet. Kompilierung findet auf
`aegis-build` statt, Ausführung ausschließlich im lokalen Android-QEMU mit den
nötigen Kernel- und SELinux-Voraussetzungen. Auch nach Bestehen dieser Tmpfs-
Tests bleiben der konkrete ext4-Basis-Mount, Benutzerprogramme, CE-Isolation,
Paketkoordination und vollständiger Ressourcenabbau separat nachzuweisen.
