# Gemeinsame Basis aus dem Systemimage

`base_image.cpp` ergänzt die native Vorbereitung für den künftigen Broker.
**Noch nicht im Produkt installiert oder im Gast ausgeführt.** Es gibt keine
CLI-Mountfunktion und keinen vom Benutzer wählbaren Pfad.

Die Eingaben sind ausschließlich `/system_ext/etc/aegis/runtime/base.ext4`
und `generation.json`. Die Systempartition muss ein schreibgeschütztes EROFS
sein. Pfadkomponenten dürfen weder Links noch weitere Mounts enthalten;
Dateien müssen root gehören, dürfen keine ausführbaren/Set-ID-Bits oder
Gruppen-/Fremdschreibrechte tragen und benötigen das dedizierte SELinux-Label
`aegis_runtime_image_file`. Die SELinux-Anbindung dieses Typs steht noch aus.
Die vorhandenen Android-Systemimages und deren durchgesetzte Richtlinie sind
die Vertrauensbasis; eine selbstkonsistente Prüfsumme ersetzt keine Bootkette.
Die Grenzen unseres direkten QEMU-Kernelstarts bleiben unverändert.

Ein begrenzter, strikt gelesener JSON-Nachweis bindet Schema, Generation,
SHA-256, 256-MiB-Größe, Rezept-/Werkzeugnachweise und UUID. Doppelte oder fremde
Felder, Kommentare, zusätzliche JSON-Werte, Fließkommazahlen für Kennungen/
Größen und übermäßige Verschachtelung werden abgewiesen. Erst nach dem Hashen
der tatsächlichen unveränderlichen Imagedatei wird ein Loop-Gerät zugeordnet.
SHA-256 kommt aus AOSPs BoringSSL; hier entsteht keine neue Kryptographie.

Die Loop-Zuordnung erfolgt atomar mit `LOOP_CONFIGURE`, ausdrücklich readonly
und autoclear. Belegte Geräte werden weder übernommen noch geleert. Gerätetyp,
Backing-Inode, Größe, Flags und Block-Schreibschutz werden zurückgelesen.
Das ext4-Dateisystem wird über die
[Kernel-Mount-API](https://docs.kernel.org/filesystems/mount_api.html) ohne
Journalschreiben erzeugt. Sein Mount ist readonly/nosuid/nodev/noexec, erhält
den festen SELinux-Kontext `aegis_runtime_base_file` und bleibt außerhalb der
Android-Mountstruktur. Nur ein eigener Deskriptor wird zurückgegeben.

Die vorhandene Namespace-Vorbereitung kann daraus die persönliche, idmapped
und ausführbare Sicht klonen. Nach Schließen sämtlicher Mount-/Klonreferenzen
endet die autoclear-Loop-Zuordnung. Der Code führt kein pfadbasiertes Unmount
und kein pauschales Löschen alter Ressourcen aus. Jede Fehlerstelle schließt
ihre eigenen Deskriptoren; bestehende fremde Loop-Geräte bleiben erhalten.

Fünf Gerätetests für das Receipt-Parsing sind vorbereitet. Ausführung erfolgt
ausschließlich im lokalen Android-QEMU. Positive Mountprüfung, tatsächliche
SELinux-Regeln, Verhalten nach Prozessabbruch, Ressourcenrückgewinnung,
Broker-Start und Zwei-Benutzer-Isolation stehen weiterhin aus.
