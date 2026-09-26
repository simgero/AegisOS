# Lokale Entwicklung, SSH-Build und QEMU

Verbindlicher Ablauf: Entwicklung in diesem Projekt auf dem Mac, AOSP-Build auf
`ssh aegis-build`, Quellcode und Artefakte über GitHub, Boot- und Systemtests in
QEMU auf dem Mac. SSH ist Steuerungs- und Diagnosekanal, kein Dateitransport.

## Verifizierter Stand vom 27. September 2026

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
- GitHub enthält vier ältere Release-Entwürfe, keinen veröffentlichten Build.
- Das vorhandene Bootstrap-Skript verlangt Ubuntu 24.04. Es ist noch nicht für
  den dauerhaften Ubuntu-26.04-Builder angepasst oder dort validiert.

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
dauerhaften Server erhalten. Der bisherige `build.sh`-Vollbuild bleibt bis zur
Anpassung von Host und QEMU-Ziel ungeeignet für den vereinbarten Ablauf.

## QEMU-Buildziel: noch offen

Das bisherige Produkt `sdk_phone64_arm64-bp2a-userdebug` ist für Androids
Ranchu-Emulator konfiguriert. Die lokale QEMU-Maschinenliste enthält `virt`, aber
kein `ranchu`. Das Produkt unverändert als QEMU-kompatibel zu markieren wäre
deshalb kein belastbarer Nachweis.

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
