# Kernel für die gemeinsame GNU/Linux-Runtime

Stand 28. September 2026: **Buildrezept vorbereitet, noch nicht ausgeführt.**
Kein neuer Kernel ist in die Android-Images integriert oder in QEMU abgenommen.

## Festgelegte Grundlage

Der laufende Entwicklungsgast meldet
`6.12.18-android16-1-g50eb8d5d443b-ab13257114-4k`.
Das [offizielle Manifest dieses Builds](https://ci.android.com/builds/submitted/13257114/kernel_aarch64/latest/manifest_13257114.xml)
legt alle 40 Quellprojekte einschließlich Toolchains und Treibern auf Commits fest.
Die Herkunft stimmt mit dem
[Prebuilt-Eintrag unseres AOSP-Stands](https://android.googlesource.com/kernel/prebuilts/6.12/arm64/+/0af99653adede9524c7dbb36ba901d22e6266404/prebuilt-info.txt)
überein.

- Kernel: `50eb8d5d443b43f38d6e72f005f1b8601ac88a05`
- Virtual-Device-Module: `b972a07579955c4a6757e46899040922f60ef06c`
- Kleaf: `a02488f06e024e940862a398ad7c38ea1279014e`
- SHA-256 des unveränderten heruntergeladenen Manifests:
  `23a76af3c306287c5b6cc0ad9e4b9d64bcc1946888cfa703628945d6dea70bd4`

`manifest.xml` übernimmt sämtliche Projekt-Pins und Linkfiles daraus. Nur der
bewegliche Superproject-Verweis und die nicht benötigte Default-Revision sind
entfernt; die XML-Formatierung ist normalisiert. Das ist die historische
Entwicklungsbasis, kein aktueller Sicherheitsstand.

## Konfiguration und Build

`aegis_runtime_defconfig` ergänzt User-/PID-/IPC-Namespaces und System-V-IPC.
Die Optionen für SELinux, Signaturen und andere Android-Sicherheitsmechanismen
werden nicht abgeschaltet. `CONFIG_VIRTIO_NET` wird nicht erzwungen: Der
Virtual-Device-Build erzeugt bereits das im bisherigen Gast geladene Modul.

Die gepinnte [Kleaf-Dokumentation](https://android.googlesource.com/kernel/build/+/a02488f06e024e940862a398ad7c38ea1279014e/kleaf/docs/kernel_config.md#defconfig_fragment-flag)
unterstützt ein zusätzliches Fragment über `--defconfig_fragment`. Das Rezept
wendet dasselbe Fragment auf den GKI-Build **und** den Build aller passenden
Virtual-Device-Module an. Die vorhandenen Build-/ABI-Prüfungen bleiben aktiv.
Ein Fehler ist zu untersuchen; er berechtigt nicht dazu, alte Module mit dem
geänderten Kernel zu mischen.

Nach Commit und GitHub-Sync führt der unprivilegierte Build-Account auf
`aegis-build` aus dem exakt passenden Projektcheckout aus:

```sh
bash scripts/kernel/build.sh VOLLSTAENDIGER_PROJEKT_COMMIT
```

Voraussetzungen sind das vorhandene gepinnte Repo-Werkzeug und Schreibrechte
für `/srv/aegis/work` und `/srv/aegis/runs`. Die Quellen liegen separat unter
`/srv/aegis/work/kernel`; die bestehende AOSP-Quelle wird nicht verändert.
Der gemeinsame Build-Lock verhindert parallele AOSP-/Helper-/Kernel-Builds.
Der Prozess muss für den langen Lauf auf dem Server verwaltet gestartet werden;
das Rezept selbst richtet keinen Dienst ein.

Vor dem Compilerstart prüft das Rezept alle 40 tatsächlichen Git-Checkouts auf
den erwarteten Commit und lokale Änderungen. Es speichert Quellmanifest,
Projektcommit, Fragment, Sync- und Buildlogs. Ergebnisse landen getrennt in
`gki/` und `virtual-device/` eines neuen Laufverzeichnisses.
`BUILT_UNVERIFIED` bedeutet ausschließlich, dass beide Buildbefehle erfolgreich
waren. Das Rezept lädt nichts hoch und installiert oder startet keine Images.

## Noch notwendige Integration und Abnahme

1. Das Rezept auf dem erreichbaren Builder ausführen. Im Ergebnis die tatsächlich
   wirksame Konfiguration sowie Herkunft und Versionsdaten aller Module prüfen.
2. Kernel, GKI-Module und Virtual-Device-Module gemeinsam in den AOSP-Build
   übernehmen. Die gepinnte Cuttlefish-Boardkonfiguration verwendet dafür
   `TARGET_KERNEL_PATH`, `SYSTEM_DLKM_SRC` und `KERNEL_MODULES_PATH`. Boot-,
   Vendor-Boot-, DLKM- und VBMeta-Images konsistent neu erzeugen; keine einzelnen
   Dateien in das alte, signierte Image austauschen.
3. Geprüfte Images und Prüfsummen über GitHub veröffentlichen und auf dem Mac
   beziehen. Das bisherige QEMU-Profil behalten; eine Profilmigration ist noch
   nicht implementiert.
4. In lokalem QEMU Boot, Modulladen, Grafik, Eingaben, ADB, SELinux und
   Verschlüsselung prüfen. User-/PID-/Mount-/IPC-Isolation praktisch ausführen,
   einschließlich UID-Mappings und negativer Zugriffsversuche.

Das Fragment allein ist kein Isolationsnachweis. Laufzeitverwaltung,
SELinux-Regeln und AOSP-vermittelte Benutzerautorisierung fehlen weiterhin.
