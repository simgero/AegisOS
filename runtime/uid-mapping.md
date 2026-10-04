# Kennungen für persönliche Linux-Kontexte

Stand 4. Oktober 2026: Die Zuordnung ist im ARM64-QEMU-Image
`b832d6c077baeee4324e00d00dc3618372f3e9d9` integriert und aus gewöhnlichen
GNU-Prozessen beider persönlicher Benutzer beobachtet, auch nach dem gepaarten
VM-Neustart. Die [aktuelle Abnahmeübersicht](../docs/phase-1-current-status.md)
und der [Ergebnisindex](../docs/phase-1-result-index.md) benennen Nachweise und
verbleibende Pflichtfälle. Phase 1 besitzt weiterhin keine Gesamtfreigabe.

## Eine AOSP-Identität, unterschiedliche Kernel-Kennungen

`uid-map.json` ist die gemeinsame Quelle für die Android-Ressourcenreservierung
und die Java-Zuordnung. `scripts/runtime/uid_layout.py write` erzeugt daraus
`runtime-ids.fs` im QEMU-Produkt, `RuntimeUidLayout.java` im Identitätsmodul und
`uid_layout.h` für die native Runtime.
`check` weist abweichende oder durch Links ersetzte erzeugte Dateien zurück.

| Kennung innerhalb von Linux | Reservierte AOSP-App-ID | Hostkennung bei AOSP-Benutzer 10 | Hostkennung bei AOSP-Benutzer 11 |
| --- | --- | --- | --- |
| Technische UID/GID 0–999 | 5000–5999 | 1005000–1005999 | 1105000–1105999 |
| Normale UID/GID 1000 | 7500 | 1007500 | 1107500 |
| `nobody`/`nogroup` 65534 | 7501 | 1007501 | 1107501 |

UID und GID verwenden dieselben Zahlen in ihren getrennten Kernel-Namensräumen.
Die Hostkennung ergibt sich aus `AOSP-userId * 100000 + appId`. Technisches Linux-
Root wird damit nicht Host-Root. Persönliche Anmeldedaten werden nicht in Debian
angelegt: AOSP bleibt für Person, Passwortprüfung und Administratorstatus zuständig.

`RuntimeUidMap` prüft die Grenzen gegen `UserHandle` des gebauten AOSP-Stands,
verwendet überlaufgeprüfte Berechnung und erzeugt die drei vorgesehenen
`uid_map`-/`gid_map`-Zeilen. Die Klasse schreibt kein procfs, setzt keine Kennung
und startet keinen Prozess. Nicht zugeordnete IDs, zum Beispiel 1001 oder 64055,
werden abgewiesen. Paketstaging muss auch neu hinzugefügte technische Konten
und Dateieigentümer kontrollieren; die Basisprüfung allein genügt nicht.

## Reservierung im Android-Produkt

Alle 1002 gemappten App-IDs stehen ausdrücklich in `runtime-ids.fs`. Zusätzlich
reserviert das Register App-ID 7502 für den Paket-Netzwerkhelfer. Diese Kennung
liegt außerhalb sämtlicher persönlicher Runtime-Mappings. Das Register enthält
damit 1003 Einträge. Das eigene QEMU-Produkt beansprucht den gesamten
Vendor-OEM2-Bereich 5000–5999 sowie drei System-Ext-IDs.
Es installiert die von AOSP erzeugten `passwd`-/`group`-Register beider Partitionen.
Das sind numerische Ressourcenbezeichnungen; sie schaffen keine persönlichen
AOSP-Benutzer, Login-Passwörter, Capabilities oder SELinux-Freigaben.

Beide Buildwege lesen nach der Produktauswahl die vollständige wirksame Liste
`TARGET_FS_CONFIG_GEN`. Vor dem Compilerstart prüft der Check diese Liste
mit AOSPs eigenem Parser auf Überschneidungen und erlaubte Bereiche. Header und
Parser sind mit SHA-256 auf unseren Android-16.0.0-r1-Stand festgelegt. Geänderte
Upstream-Definitionen erfordern eine Überprüfung und brechen den Build ab.
Auch ein fehlendes, doppeltes oder vom Projekt abweichendes eigenes Register
führt zum Abbruch. Kein fremder Eintrag wird verdrängt oder umnummeriert.

Auf späterer Hardware können Vendor-Kennungen bereits belegt sein. Dann ist eine
neue, gemeinsam überprüfte Zuordnung samt Datenmigration nötig; dieser QEMU-
Bereich ist keine universelle Zusage für beliebige Geräte.

Referenzen des verwendeten AOSP-Stands:

- [Reservierte Kennungsbereiche](https://android.googlesource.com/platform/system/core/+/refs/tags/android-16.0.0_r1/libcutils/include/private/android_filesystem_config.h)
- [AOSPs Parser und Registergenerator](https://android.googlesource.com/platform/build/+/refs/tags/android-16.0.0_r1/tools/fs_config/fs_config_generator.py)
- [Register-Buildmodule](https://android.googlesource.com/platform/build/+/refs/tags/android-16.0.0_r1/tools/fs_config/Android.bp)
- [Bionic-Namensauflösung](https://android.googlesource.com/platform/bionic/+/refs/tags/android-16.0.0_r1/libc/bionic/grp_pwd.cpp)
- [AOSP-UserHandle](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/core/java/android/os/UserHandle.java)

## Nachweise und Grenzen

`python3 scripts/runtime/uid_layout.py check` bestätigt am aktuellen Stand,
dass Android-Register, Java-Zuordnung und nativer Header genau zur gemeinsamen
Quelle passen. Das ist eine Quellenprüfung und schreibt keine Prozesskennungen.

Im aktuellen Profil `1943dcb7-d438-48de-8e62-d9967b32b9b2` lesen gewöhnliche
GNU-Prozesse ihre tatsächlichen `uid_map`- und `gid_map`-Dateien. Beide zeigen
die drei obigen Zeilen; `id` meldet intern UID/GID 1000. Getrennte User-, Mount-,
PID-, IPC-, UTS- und Netzwerk-Namespaces sowie Host-UIDs 1007500/1107500 sind
beobachtet. Nach dem gepaarten Neustart enthält
`out/phase1-dod/b832d6c-base/paired-reboot-readback-proof.json` denselben
Mapping-Nachweis zusammen mit ursprünglichen Daten und Paketversionen.
Seine SHA-256 lautet
`a058b0beba2e1ae52c6fc16f7499907cd16ddacc230767452be9931c46f7ef1f`.
Die im Ergebnisindex erklärte Rekonstruktion des historischen Ereignisses 203
bleibt Bestandteil dieser Belegkette; das Originalprotokoll wurde nicht ersetzt.

Native Testprogramme und Java-Test-APK wurden für b832d6c gebaut;
`native-build-receipt.json` bindet deren Prüfsummen. Ein erfolgreicher Build
belegt keine Ausführung sämtlicher Tests. Der vorhandene native Test
`RuntimeRegistry.InstalledNamesAndIdsRoundTripThroughBionic` prüft die direkte
Bionic-Auflösung der 1002 gemappten Kennungen; die zusätzliche Netzwerkkennung
ist nicht Teil dieses Tests. Die hier genannten GNU-Belege ersetzen weder
die offenen Startfehlerfälle noch Benutzerlöschung und ID-Wiederverwendung.

## Lebenszyklusanforderungen an den Runtime-Koordinator

- Unmittelbar vor jedem Start den frisch autorisierten AOSP-Eigentümer, dessen
  Seriennummer, CE-Entsperrung und Lebenszykluszustand prüfen. Eine Kennung oder
  ein `RuntimeUidMap`-Objekt ist keine Berechtigung.
- Metadaten nach Benutzer-ID **und** Seriennummer trennen (`u10-s10` gegenüber
  `u10-s99`). Eine wiederverwendete AOSP-Benutzernummer führt zur gleichen
  numerischen Hostkennung. Deshalb müssen alte Prozesse, Mounts, IPC-Ressourcen,
  Transaktionen und private Dateien vor einer Wiederverwendung nachweislich
  bereinigt sein; ein neuer Verzeichnisname allein schützt nicht davor.
- UID/GID-Maps, Zusatzgruppen und Capabilities kontrolliert einrichten und im
  Namespace tatsächlich messen. Fehlerfälle dürfen keinen schwächeren Start
  erlauben; ihre vollständige Abnahme bleibt T04 zugeordnet.
- Die gemeinsame schreibgeschützte Basis muss für jeden Kontext passende
  Dateieigentümer zeigen. Die Zuordnung allein erledigt das nicht. Wiederholtes
  Umchownen derselben gemeinsamen Basis auf unterschiedliche Personen ist keine
  Lösung. Der aktuelle Gast verwendet schreibgeschützte idmapped Mounts;
  deren beobachtete Kontexte stehen in den verknüpften Zustandsbelegen.
- CE-Verzeichnisse, persönliche Paketgenerationen und temporäre Dateien mit den
  geprüften Eigentümern verbinden. Keine fremden Hostpfade, Prozesse oder IPC-
  Objekte zugänglich machen. SELinux, `nosuid` und `no_new_privs` bleiben nötig.
- Logout erst bestätigen, wenn persönliche Prozesse beendet und Mounts/IPC
  abgebaut sind und AOSP den Benutzer gestoppt sowie den CE-Speicher gesperrt hat.

Der aktuelle Gast meldet `ro.aegis.runtime.mode=managed-v1`.
