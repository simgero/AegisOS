# Phase 1 – Build- und Startinventar b832d6c

Stand: 4. Oktober 2026. Das Inventar ist gegen die vorhandenen Dateien geprüft;
**D1 und Phase 1 sind weiterhin nicht vollständig abgenommen.** Maßgeblich
bleiben [DoD](architecture/phase-1-dod.md), [aktueller Abnahmestand](phase-1-current-status.md)
und [Ergebnisindex](phase-1-result-index.md). Der neue Abgleich ist kein erneuter
Android-Vollbuild und behauptet keine bitgleichen vollständigen Android-Builds.

## Festgehaltene Eingaben

| Bestandteil | Getesteter Stand |
| --- | --- |
| AEGIS-Imagequellen | `b832d6c077baeee4324e00d00dc3618372f3e9d9` |
| AOSP | `android-16.0.0_r1`, Manifestcommit `2e764235335a27ee8cc8efc16baef4c0f6a5b3fe` |
| Aufgelöstes AOSP-Manifest | 987 Projekte; SHA-256 `901ef0ddfe7c9b1b146d87a5c37a82e9cb626fdb1994b977ee3b8f68b5adfd83` |
| Repo-Werkzeug | `97dc5c1bd9527c2abe2183b16a4b7ef037dc34a7` |
| Produktziel | `aegis_qemu_arm64-bp2a-userdebug`, Runtime `managed-v1` |
| Kernelrezept | AEGIS `64e66d772f22465682dd2c41fdb486774cc15a31`; 40 gepinnte Kernelprojekte |
| Kernelrelease | `6.12.18-android16-1-maybe-dirty-4k` |
| Kerneldatei | SHA-256 `148d623ac45b177a1728d97d8178e784a2e746500b1416f6831ee688d355ad6a` |
| Kernelbundle | `84a41f6a0a47edc2ebefeb83a6bcc56ddff64d9d2eeed37fea0f4dd58b40948e` |
| Debian-Basis | Debian 13.7; `debuerreotype/docker-debian-artifacts` Commit `ca011a8b1c3b259e4cbbf83bf6841f1fd5f497c1` |
| Runtime-Generation | 256 MiB; SHA-256 `f0d294fadfa0924489213cd3cb7fe0bf908da30330f1f34bb582dac4aca12514` |
| Helper-Init und Packager | AEGIS `024354c134eb8a59174f2f7796ae0eed4c6ae6c7`; beide Quellen bytegleich zu b832d6c |
| Helper-Archiv | SHA-256 `0d60815c5bdc5c8ae22a60c4cc004fed4c7163698e3c959da4367f1b46126d7d`; Protokoll `persistent-state-v1` |
| Unveränderliche GPT-Basisdisk | SHA-256 `5f32642e3b2ffb7a834236dce72b850925e3518991a81a7f788306300514af89` |
| AVB-Digest | `7a2e84e79ce4e6ab23346ec7e128481bc4fa6c8446cb66e09c93fb7a1bc09470` |
| Beobachtete Hostwerkzeuge | QEMU 10.2.1 (`1:10.2.1+ds-1ubuntu3.2`), Python 3.14.4 |

Das AOSP-Manifest hält Upstream-Revisionen fest. Die zusätzlichen AEGIS-Quellen
und registrierten AOSP-Anpassungen stehen in den separaten Buildbelegen für
Identity, Produkt, Runtime-Policy, Runtime-Storage, Vold, Bootanimation und
EGL-Cache. Die vollständigen Imageprüfsummen und diese 17 Belege liegen unter
`/srv/aegis/runs/phase1-b832d6c0/{images,build-receipts}`. Das historische
AOSP-Manifest des Helper-Exports ist bytegleich zum aktuellen Manifest;
daraus folgt keine Gleichheit aller Android-Images oder AEGIS-Anpassungen.

## Vorhandene abgeschlossene Läufe

| Lauf | Pfad |
| --- | --- |
| Kernel und passende Module | `/srv/aegis/runs/kernel-20260928T170624Z-64e66d77-qW9h9L` |
| Runtimebasis | `/srv/aegis/runs/runtime-base-20261003T230204Z-b832d6c0-TCF6AT` |
| Android-Vollbuild | `/srv/aegis/runs/local-20261003T231351Z-b832d6c0-TSVmQp` |
| Ursprünglicher Helper-Export | `/srv/aegis/runs/secure-env-20260928T134804Z-024354c1` |
| Vorbereitete Factory-Images | `/srv/aegis/runs/phase1-b832d6c0` |
| Synthetisches Abnahmeprofil | `/srv/aegis/runs/phase1-b832d6c0/profile` |

Der Android-Lauf ist `LOCAL_BUILD_VERIFIED`. Der Runtime-Beleg bestätigt
zwei identische Basis-Erzeugungen. Der Kernellauf verwendet den ursprünglichen
Abschlussnamen `BUILT_UNVERIFIED`; seine danach erfolgte und jetzt wiederholte
Eingabeprüfung bestätigt tatsächlich 162 Dateien, die wirksame eingebettete
Konfiguration und passende Modul-Metadaten. Historische Statusnamen werden
nicht nachträglich verändert oder als vollständige Gastabnahme interpretiert.

## Build erneut ausführen

Voraussetzung ist der bereits provisionierte Linux-Buildserver mit dem
gepinnten AOSP-/Repo-Workspace, den Hostabhängigkeiten und dem unveränderten
Quellcheckout. Die folgenden Befehle als `aegis-build` im sauberen Checkout
ausführen. Der vorhandene Build-Lock verhindert gleichzeitige Änderungen
am AOSP-Workspace.

```sh
cd /srv/aegis/work/base-b832d6c-HAhLwO
AEGIS_COMMIT=b832d6c077baeee4324e00d00dc3618372f3e9d9
test "$(git rev-parse HEAD)" = "$AEGIS_COMMIT"
test -z "$(git status --porcelain --untracked-files=all)"
AEGIS_SCRIPT_COMMIT="$AEGIS_COMMIT" \
AEGIS_KERNEL_RUN=/srv/aegis/runs/kernel-20260928T170624Z-64e66d77-qW9h9L \
AEGIS_RUNTIME_RUN=/srv/aegis/runs/runtime-base-20261003T230204Z-b832d6c0-TCF6AT \
bash scripts/aosp/build-local.sh
```

Das Rezept erzeugt ein neues Laufverzeichnis und verwendet die ausdrücklich
ausgewählten, erneut geprüften Kernel-/Runtime-Eingaben. Es lädt keine Builds
hoch. Historische Release-/Exportskripte sind hierfür nicht zu verwenden.

Für eine neue Runtimebasis aus demselben sauberen Checkout:

```sh
bash scripts/runtime/build-base.sh "$AEGIS_COMMIT"
```

Den konkret ausgegebenen erfolgreichen neuen Lauf danach ausdrücklich als
`AEGIS_RUNTIME_RUN` auswählen. Der Kernel wird analog mit
`bash scripts/kernel/build.sh VOLLSTAENDIGER_PROJEKT_COMMIT` im exakt zu diesem
Commit passenden Checkout erzeugt; Fragment, Manifest, GKI und Virtual-Device-
Module müssen zusammenpassen. Siehe [Kernelrezept](../kernel/README.md).

Die lokale Vorbereitung eines **neuen** erfolgreichen Android-Builds erfolgt
mit [prepare-server-build.py](../scripts/prepare-server-build.py):

```sh
python3 scripts/prepare-server-build.py \
  /srv/aegis/runs/NEUER_ERFOLGREICHER_LOCAL_BUILD \
  /srv/aegis/runs/NEUES_VORBEREITUNGSVERZEICHNIS \
  --commit "$AEGIS_COMMIT" \
  --avbtool /srv/aegis/work/aosp/external/avb/avbtool.py
```

Die beiden Platzhalter müssen konkrete Pfade sein; das Ziel darf noch nicht
existieren. Die Vorbereitung prüft Images, Eingangsbelege, eingebetteten
Kernel und AVB und erzeugt die GPT-Basis; sie startet keine VM. Der ausführende
Account braucht Lesezugriff auf seinen Build und Schreibzugriff auf das neue
Ziel. Vor dem Start muss der gewählte QEMU-Account auf dieses Ziel und das
geprüfte Helper-Archiv zugreifen können. Bestehende Konto- und Profilrechte
werden nicht pauschal erweitert.

## Nachgewiesener Start und Persistenzbindung

Die konkreten Befehle für ein eigenes frisches Profil und spätere Starts
stehen in der [Terminalanleitung](terminal-quickstart.md). Der Abnahmelauf
`out/phase1-dod/b832d6c-base/boot-3` verwendet ARM64-TCG, acht Android-vCPUs/
4096 MiB sowie zwei Helper-vCPUs/1024 MiB. Beide VMs gehören zu Profil
`1943dcb7-d438-48de-8e62-d9967b32b9b2`; Android verwendet ADB-Port 15876 und
User-Networking. Exakte QEMU-Argumente, Initrd-Prüfsummen und Bootkonfigurationen
sind im lokalen Inventar enthalten.

Die beobachtete systemd-Unit `aegis-qemu-b832d6c-boot3.service` läuft als
`simeongerodetti`, mit `KillMode=mixed`, 120 Sekunden Stop-Budget und
ausdrücklicher 24-Stunden-Grenze. Dies ist kein Autostart nach Host-Reboot.
Beim erneuten Start desselben Profils `--create-profile` weglassen und ein
neues Laufverzeichnis verwenden. Beide veränderlichen Disks bleiben zusammen;
ihre geordnete Wiederverwendung ist im [Ergebnisindex](phase-1-result-index.md)
separat mit gepaartem Shutdown/Reboot und positiven Datenrücklesungen belegt.

## Umfang des erneuten Inventarabgleichs

Am 4. Oktober wurden alle 20 vorbereiteten Factory-Images, die sieben festen
Profileingaben einschließlich der 16-GB-Basisdisk, 17 ursprüngliche Buildbelege,
162 Kerneldateien, vier Runtime-Artefakte und 36 Helper-Programmdateien gelesen
und per SHA-256 abgeglichen. Die 22 erfassten Build-/Startquellen stimmen mit
dem Image-Commit überein. Das Helper-Archiv und dessen ursprüngliche
Init-/Packagerquellen sind zusätzlich ihrem ursprünglichen Exportcommit
zugeordnet. Keine aktive Nutzerdaten- oder KeyMint-Disk wurde geöffnet.

Zwei reine Leseversuche scheiterten an der Kontotrennung. Anschließend las
jeder Account nur seine vorhandenen Quellen/Belege; die Ergebnisse wurden
getrennt verglichen. Es wurden keine Dateirechte geändert. Diese Fehlversuche
sind im Inventarnachweis erhalten und sind keine fehlgeschlagenen Gasttests.

`out/phase1-dod/b832d6c-base/build-inventory-proof.json`, SHA-256
`0a1ea5043061eff3b6aa8129af1f11af3b37278da110a2467a557a75aef5b3a7`,
bindet die vier lokalen Beobachtungsdateien samt Prüfsummen. Der ursprüngliche
Build-Compilerlauf und die Helper-Kompilierung wurden hierbei nicht wiederholt.
Der dokumentierte Entwicklungsstart mit AOSP-Testschlüsseln und Software-TPM
behauptet keine Hardware-Vertrauenswurzel. Ungeklärte Starthelfer-Ursachen,
vollständige Dienststabilität über alle Pflichtfälle und sämtliche übrigen
offenen DoD-Varianten bleiben Anforderungen. Builds und Rohbelege bleiben lokal.

## Neuer Build mit misctrl-Korrektur ec01e5fa

Der saubere Checkout `/srv/aegis/work/misctrl-ec01e5f-e69witqg` steht auf
`ec01e5fa5f2822da5763ab54c644bc5c5c5ab413`. Er enthält zusätzlich die Shell-
und misctrl-Korrekturen. Sein lokaler Vollbuild ist
`/srv/aegis/runs/local-20261004T132131Z-ec01e5fa-Hp0Ctv`; die geprüfte
Vorbereitung liegt unter `/srv/aegis/runs/phase1-ec01e5fa0`. Ein Profil wurde
dort noch nicht erzeugt. Das oben dokumentierte AOSP-Manifest, Kernelbundle
und die Debian-Basisgeneration gelten unverändert für diese Eingaben.

Der konkrete Buildbefehl im genannten sauberen Checkout als `aegis-build`:

```sh
AEGIS_SCRIPT_COMMIT=ec01e5fa5f2822da5763ab54c644bc5c5c5ab413 \
AEGIS_KERNEL_RUN=/srv/aegis/runs/kernel-20260928T170624Z-64e66d77-qW9h9L \
AEGIS_RUNTIME_RUN=/srv/aegis/runs/runtime-base-20261003T230204Z-b832d6c0-TCF6AT \
bash scripts/aosp/build-local.sh
```

Alle 20 Images sowie nun 18 Buildbelege einschließlich `misctrl-source.json`
sind geprüft. Die GPT-Basis hat SHA-256
`7f942befb9b9f0302f0d3bfb194648359d5953121c890187bcc461926611cbc0`;
der neue AVB-Digest lautet
`8dfbc2ec43f84eb24db0fe6d5eee09cf36315a7235ec6d664f0a374899f00af6`.
Die tatsächliche misctrl-Binärdatei aus `system_a` des ausgelieferten
`super.img` wurde bytegenau mit dem neu kompilierten Buildprodukt verglichen.
Belege, Prüfsummen und Grenzen stehen im [Ergebnisindex](phase-1-result-index.md).
Dies ist weiterhin **Build-/Imagevalidierung ohne Boot oder Gastabnahme**.
