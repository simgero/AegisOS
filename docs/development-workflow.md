# Lokale Entwicklung, SSH-Build und QEMU

Verbindlicher Ablauf: Entwicklung in diesem Projekt auf dem Mac, AOSP-Build auf
`ssh aegis-build`, Quellcode und Artefakte über GitHub, Boot- und Systemtests in
QEMU auf dem Mac. SSH ist Steuerungs- und Diagnosekanal, kein Dateitransport.

## Verifizierter Stand vom 27. September 2026

Update 28. September: Vollbuild, `UPLOAD_VERIFIED` und lokaler Android-Boot
sind erfolgreich. Im QEMU-Lauf `secure-env-7` wurden `sys.boot_completed=1`,
beendete Bootanimation, sichtbarer Sperrbildschirm, SELinux `Enforcing` und
dateibasierte Verschlüsselung bestätigt. KeyMint läuft im separaten lokalen
ARM64-Hilfsgast. Start: `bash scripts/run-local-qemu.sh`.
Der Test bleibt flüchtig; noch keine Bestätigung für persistente Daten,
vollständige Hardwarefunktionen oder Produktionssicherheit.

Die folgende Bestandsaufnahme dokumentiert die frühere Servereinrichtung:

- Mac: ARM64; QEMU 11.1.1 und GitHub CLI vorhanden; GitHub-Anmeldung funktioniert.
- Builder: x86-64, Ubuntu 26.04.1, 93 GiB RAM, 12 CPU-Threads.
- Platte: 1 TiB; 100 GiB Root-LV und separates 800-GiB-LV `ubuntu-vg/aegis-build`.
- `/srv/aegis` ist als ext4 eingebunden, mit 779 GiB freiem Platz. Der dauerhafte
  fstab-Eintrag verwendet UUID `7bd5bedd-3b18-402d-bbee-776c2be150b5`.
- Build-Pakete sind installiert und per Paketdatenbank verifiziert. Der separate
  Benutzer `aegis-build` besitzt `/srv/aegis/home`, `/srv/aegis/work` und
  `/srv/aegis/runs`. Ein vollständiger AOSP-Build auf Ubuntu 26.04 bleibt ungeprüft.
- `sudo -n` verlangt weiterhin interaktive Authentifizierung. GitHub CLI ist auf
  dem Server installiert; die Anmeldung als `simgero` wurde vom Benutzer bestätigt.
- Quellcode-Sync ist mit `SOURCES_READY` abgeschlossen (52 Minuten Laufzeit).
  Etwa 120 GiB belegt, 660 GiB frei. Noch kein veröffentlichter Vollbuild.
- Der Bootstrap akzeptiert Ubuntu 24.04 und 26.04 und verwendet für den
  dauerhaften Server `UPLOAD_VERIFIED` als Erfolgsstatus. Ein echter Vollbuild
  auf Ubuntu 26.04 steht noch aus.

## Prüfung und Transport

Auf dem Mac, im Projekt:

```sh
bash scripts/check-environment.sh
python3 scripts/fetch-release.py RELEASE_TAG downloads/RELEASE_TAG
```

Die Umgebungsprüfung liest nur. Sie meldet fehlenden Build-Speicher als Fehler.
Der Download verlangt einen ausdrücklich gewählten veröffentlichten Release,
ein neues Zielverzeichnis und prüft alle Assets gegen `SHA256SUMS`. Fehlende,
zusätzliche, beschädigte oder nicht fortlaufende Archivteile werden abgelehnt.
Es erfolgt weder automatisches Entpacken noch ein VM-Start. Bei Fehlern bleiben
die heruntergeladenen Dateien zur Diagnose erhalten und gelten als ungeprüft.

## Administrative Einrichtung

Der Vollbuild vom 27. September brach nach 6:45 Stunden bei Trustys `nsjail`
ab: AppArmor verweigerte den privaten Mount-Namespace (`mount ... MS_PRIVATE:
Permission denied`). `build.sh` lädt deshalb vor dem Start über
`scripts/aosp/setup-sandbox.sh` ein persistentes Profil für genau
`/srv/aegis/work/aosp/prebuilts/build-tools/linux-x86/bin/nsjail`.
Es erlaubt diesem Programm User-Namespaces; die globale Ubuntu-Einschränkung
bleibt aktiv. Der Build-Benutzer kontrolliert diesen Pfad und muss daher als
vertrauenswürdig behandelt werden. Ein abweichendes vorhandenes Profil wird
nicht überschrieben. Die Kompilierung prüft den tatsächlichen Sandbox-Start
als Build-Benutzer vorab. Die Wirksamkeit muss beim nächsten Serverlauf bestätigt
werden; vorhandene Build-Ausgaben werden wiederverwendet.

Hintergrund: [Ubuntu AppArmor User-Namespaces](https://documentation.ubuntu.com/security/security-features/privilege-restriction/apparmor/).

Die Speicherbereitstellung ist abgeschlossen und per SSH verifiziert. Den
Formatierungsblock nicht erneut ausführen. Auch Build-Pakete und separater
Build-Benutzer sind eingerichtet. Der folgende Block dokumentiert die bereits
ausgeführte Einrichtung; eine erneute Ausführung ist aktuell nicht erforderlich:

```sh
sudo bash <<'EOF'
set -euo pipefail
mountpoint -q /srv/aegis
apt-get update
apt-get install -y ca-certificates curl git gh python3 build-essential flex bison \
  zip unzip zlib1g-dev libc6-dev-i386 libx11-dev lib32z1-dev libgl1-mesa-dev \
  libxml2-utils xsltproc fontconfig rsync libncurses-dev libssl-dev file xz-utils
if ! id aegis-build >/dev/null 2>&1; then
  useradd --system --user-group --create-home \
    --home-dir /srv/aegis/home --shell /bin/bash aegis-build
fi
install -d -o aegis-build -g aegis-build -m 755 /srv/aegis/work /srv/aegis/runs
echo 'Build-Pakete und Arbeitsverzeichnisse eingerichtet.'
EOF
```

Danach den dauerhaft nutzbaren Builddienst und GitHub-Release-Zugriff für diesen
Host einrichten; keine Abschalt-/Löschanweisung für den lokalen Server übernehmen.
Der SSH-Schlüssel für Git ersetzt keine GitHub-API-Anmeldung für Release-Uploads.

## Quellen vor dem Build vorbereiten

`scripts/prepare-builder.sh FULL_GITHUB_COMMIT` wird auf dem Builder mit sudo
gestartet. Das Skript bezieht die Worker-Dateien von genau diesem GitHub-Commit
und startet einen systemd-Dienst als Benutzer `aegis-build`. Es installiert keine
Pakete, benötigt keinen GitHub-Token und startet weder Compiler noch Upload.
Ubuntu 26.04 wird für diesen Download-Schritt nicht ausgeschlossen; die
Buildkompatibilität wird damit noch nicht bestätigt.

Der Download verwendet dieselben festgelegten AOSP-/repo-Commits wie der bisherige
Builder, einen Netzwerkjob und die bestehende HTTP-429-Abbruch-/Wartefrist.
Er darf nach Trennung der SSH-Verbindung weiterlaufen. Nach 24 Stunden endet der
Dienst; nach einem Serverneustart wird er nicht automatisch neu gestartet.
Ein manueller Neustart nutzt die bereits geladenen Quellen weiter. Die Prüfung
auf 450 GiB freien Platz gilt auch bei einem Neustart.

Status und Protokoll:

```sh
systemctl status aegis-build
journalctl -fu aegis-build
sudo sh -c 'cat /srv/aegis/runs/*/status'
```

`SOURCES_READY` bestätigt nur den vollständigen Quellcode-Sync und das aufgelöste
Manifest. Es bestätigt keinen Build und keinen Boot. Daten bleiben auf dem
dauerhaften Server erhalten. Der angepasste `build.sh`-Vollbuild erstellt nun das experimentelle Produkt
`aegis_qemu_arm64-bp2a-userdebug`. Start siehe README; ein QEMU-Bootnachweis folgt
erst nach dem Build und der noch offenen Startdisk-/Hostdienst-Integration.

## Verbindliches QEMU-Buildziel

Am 27. September 2026 bestätigt: **`aegis_qemu_arm64-userdebug`**.
Die Entscheidung ist verbindlich. Eine erste Produktdefinition unter
`device/aegis/qemu_arm64` erbt die ARM64-only-Cuttlefish-Basis des festgelegten
AOSP-Tags. Ein erfolgreicher vollständiger Build oder Android-Boot ist noch
nicht nachgewiesen.

| Eigenschaft | Festlegung |
| --- | --- |
| AOSP-Produkt | `aegis_qemu_arm64` |
| Buildvariante | `userdebug` |
| Architektur | ARM64 |
| Virtuelle Maschine | QEMU `virt` auf dem Apple-Silicon-Mac |
| Beschleunigung | HVF als Ziel; mit Kernel und Startkonfiguration zu verifizieren |
| Ausgangsbasis | `android-16.0.0_r1`, bestehender Manifest-Pin aus `scripts/aosp/config.sh` |
| Bedienung im ersten Meilenstein | ADB und Entwicklungs-Shell; keine eigene Oberfläche |
| Build und Transport | `aegis-build`; Quellcode und Artefakte über GitHub |

Die konkrete Lunch-Konfiguration lautet `aegis_qemu_arm64-bp2a-userdebug`.
`bp2a` übernimmt den bisherigen Release-Konfigurationsstand. Die Auswertung
durch das vollständige AOSP-Buildsystem steht noch aus. Der Compiler registriert
die zum Projekt-Commit gehörende Produktdefinition als verwalteten Symlink im
AOSP-Baum und prüft vor dem Build den tatsächlich ausgewählten Produktnamen.
Unbekannte bestehende Gerätedateien werden nicht überschrieben.

Der Vollbuild-Bootstrap lädt neben den Skripten auch die Produktdefinition vom
gleichen GitHub-Commit. Die Paketierung verlangt Kernel, boot, init_boot,
vendor_boot, super, userdata und vbmeta; fehlende Dateien verhindern einen
erfolgreichen Upload. Diese Dateien sind noch keine fertige QEMU-Startdisk.

### Lokaler Hardware- und Kerneltest

```sh
python3 scripts/qemu-kernel.py --probe
python3 scripts/qemu-kernel.py --kernel /absoluter/pfad/zum/kernel --dry-run
python3 scripts/qemu-kernel.py --kernel /absoluter/pfad/zum/kernel
```

`--probe` wurde auf dem Mac mit QEMU 11.1.1 erfolgreich ausgeführt: eine pausierte
ARM64-VM mit `virt-11.1`, GICv3 und HVF wurde erstellt und geschlossen. Das ist
kein Kernel- oder Android-Boot. Der Kernelmodus akzeptiert ein unkomprimiertes
ARM64-Linux-Image und startet ohne Netzwerk und ohne Disk. Ohne Root-Dateisystem
ist ein Mountfehler zu erwarten; dieser Modus dient nur der Kernel-Diagnose.
Er deaktiviert keine SELinux- oder Verschlüsselungsregeln des Android-Produkts.

Erste Abnahmekriterien:

1. Das eigene Produkt lässt sich auf `aegis-build` vollständig bauen; Projekt-
   und Quellstände sowie Build-Protokoll sind dokumentiert.
2. Die erforderlichen Images, Kernel und Startinformationen werden mit
   Prüfsummen über GitHub auf den Mac übertragen.
3. Android startet lokal in QEMU vollständig (`sys.boot_completed=1`);
   ADB-Verbindung und eine bedienbare Entwicklungs-Shell sind nachgewiesen.
4. Ein erneuter Start mit derselben dokumentierten Konfiguration gelingt.

Benutzerverwaltung, FBE und Runtime-Isolation erhalten anschließend eigene
Abnahmetests. Ein erfolgreicher Boot belegt diese Sicherheitsfunktionen nicht.

### Noch zu implementierende Geräteanpassung

Das bisherige Ranchu-Produkt wurde durch die eigene Produktregistrierung ersetzt.
Die Vererbung von Cuttlefish ist ein Ausgangspunkt, keine fertige Portierung:
Bootconfig, passende Ramdisk und GPT-/AVB-Partitionslayout sowie die
Gegenstellen für bisherige Hostdienste müssen noch integriert und getestet werden.
Insbesondere KeyMint/Gatekeeper dürfen nicht stillschweigend durch schwächere
Implementierungen ersetzt werden, um einen erfolgreichen Boot vorzutäuschen.

Für ein ARM64-`virt`-Ziel müssen Kernel und Module, Partitionen und fstab,
Ramdisk/Bootconfig sowie die benötigten Android-Hardwaredienste zusammenpassen.
Die AOSP-Cuttlefish-Gerätekonfiguration dient als mögliche technische Referenz;
sie ersetzt nicht die Vorgabe, lokal mit QEMU auf dem Mac zu testen. Ihr
QEMU-Manager enthält zwar HVF-Unterstützung, aber auch Hostdienste und
vhost/vsock-Anbindungen. Ein bloßer Austausch des Produktnamens reicht nicht.

Nächste Abnahme: dokumentiertes Buildziel, vollständiges Image, lokaler QEMU-Start,
erfolgreicher Android-Start und protokollierter Zugriff auf die Entwicklungs-Shell.
Ein Kernel-Start allein gilt nicht als vollständiger AOSP-Boot. FBE und
Benutzerisolation werden danach separat geprüft.

Referenzen:

- [QEMU ARM virt](https://www.qemu.org/docs/master/system/arm/virt.html)
- [AOSP 16 Cuttlefish BoardConfig](https://android.googlesource.com/device/google/cuttlefish/+/refs/tags/android-16.0.0_r1/shared/BoardConfig.mk)
- [AOSP 16 QEMU-Manager](https://android.googlesource.com/device/google/cuttlefish/+/refs/tags/android-16.0.0_r1/host/libs/vm_manager/qemu_manager.cpp)
