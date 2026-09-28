# Kennungen für persönliche Linux-Kontexte

Stand 28. September 2026: **Zuordnung und Buildprüfung im Quelltext; noch kein
laufender Namespace und kein Isolationsnachweis.** Der aktuelle QEMU-Gast nutzt
diese Zuordnung nicht. Die AEGIS-Anmeldung bleibt unkompiliert.

## Eine AOSP-Identität, unterschiedliche Kernel-Kennungen

`uid-map.json` ist die gemeinsame Quelle für die Android-Ressourcenreservierung
und die Java-Zuordnung. `scripts/runtime/uid_layout.py write` erzeugt daraus
`runtime-ids.fs` im QEMU-Produkt und `RuntimeUidLayout.java` im Identitätsmodul.
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
werden abgewiesen. Paketstaging muss später auch neu hinzugefügte technische
Konten und alle Dateieigentümer kontrollieren; die Basisprüfung allein genügt nicht.

## Reservierung im Android-Produkt

Alle 1002 App-IDs stehen ausdrücklich in `runtime-ids.fs`. Das eigene QEMU-Produkt
beansprucht den gesamten Vendor-OEM2-Bereich 5000–5999 sowie zwei System-Ext-IDs.
Es installiert die von AOSP erzeugten `passwd`-/`group`-Register beider Partitionen.
Das sind numerische Ressourcenbezeichnungen; sie schaffen keine persönlichen
AOSP-Benutzer, Login-Passwörter, Capabilities oder SELinux-Freigaben.

Beide Buildwege lesen nach der Produktauswahl die vollständige wirksame Liste
`TARGET_FS_CONFIG_GEN`. Vor dem Compilerstart prüft der neue Check diese Liste
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

Lokal wurden die 38 unterschiedlichen Kennungen der erneut hashgeprüften echten
Debian-Basis abgeglichen; sie sind vollständig abgedeckt. Der originale gepinnte
AOSP-Parser akzeptiert unser Register und weist eine zusätzlich eingebrachte
Kollision mit Kennung 5000 zurück. Das ist eine Metadatenprüfung, kein Android-
Build und noch keine Prüfung aller tatsächlich geerbten Builder-Konfigurationen.

Elf Hosttests prüfen unter anderem Bereichsgrenzen, doppelte Kennungen/Namen,
fehlende Pflichtkennungen, geänderte Quellen/Parser, ungültige Produktpfade und
nicht abgedeckte Debian-Datei-, Benutzer- und Gruppenkennungen.

Fünf vorbereitete Android-Tests vergleichen Zuordnung, Grenzen und Wiederverwendung
mit der tatsächlichen Plattform-API. Ein weiterer Produkt-Test fragt alle 1002
Ressourcennamen über `Process.getUidForName` und `getGidForName` ab; diese APIs
delegieren an Bionic. Der zusätzliche native Test
`RuntimeRegistry.InstalledNamesAndIdsRoundTripThroughBionic` prüft direkte
`passwd`-/`group`-Auflösung und die kanonischen Rückwärtsnamen aller 1002 Einträge.
`Os.getpwnam` und `StructPasswd` gehören nicht zur stabilen libcore-API des
Testmoduls; der erste Java-Build hat diese falsche Annahme aufgedeckt.
Die korrigierten Gerätetests sind **noch nicht kompiliert oder in QEMU ausgeführt**.
Sie ersetzen keine praktischen Zugriffsversuche zwischen zwei Benutzern.

## Vor Verwendung im Runtime-Koordinator

- Unmittelbar vor jedem Start den frisch autorisierten AOSP-Eigentümer, dessen
  Seriennummer, CE-Entsperrung und Lebenszykluszustand prüfen. Eine Kennung oder
  ein `RuntimeUidMap`-Objekt ist keine Berechtigung.
- Metadaten nach Benutzer-ID **und** Seriennummer trennen (`u10-s10` gegenüber
  `u10-s99`). Eine wiederverwendete AOSP-Benutzernummer führt zur gleichen
  numerischen Hostkennung. Deshalb müssen alte Prozesse, Mounts, IPC-Ressourcen,
  Transaktionen und private Dateien vor einer Wiederverwendung nachweislich
  bereinigt sein; ein neuer Verzeichnisname allein schützt nicht davor.
- UID/GID-Maps, Zusatzgruppen und Capabilities kontrolliert einrichten und im
  Namespace tatsächlich messen. Der benötigte Kernel und Broker fehlen noch.
- Die gemeinsame schreibgeschützte Basis muss für jeden Kontext passende
  Dateieigentümer zeigen. Die Zuordnung allein erledigt das nicht. Wiederholtes
  Umchownen derselben gemeinsamen Basis auf unterschiedliche Personen ist keine
  Lösung; Mount-/Dateisystemintegration ist noch zu implementieren und zu testen.
- CE-Verzeichnisse, persönliche Paketgenerationen und temporäre Dateien mit den
  geprüften Eigentümern verbinden. Keine fremden Hostpfade, Prozesse oder IPC-
  Objekte zugänglich machen. SELinux, `nosuid` und `no_new_privs` bleiben nötig.
- Logout erst bestätigen, wenn persönliche Prozesse beendet und Mounts/IPC
  abgebaut sind und AOSP den Benutzer gestoppt sowie den CE-Speicher gesperrt hat.

`ro.aegis.runtime.mode=absent` bleibt bis zur tatsächlichen Integration gesetzt.
