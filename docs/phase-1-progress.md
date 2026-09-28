# Phase 1: Implementierungsstand

Stand: 28. September 2026. Keine Abnahme des Gesamtziels.

| Anforderung | Nachweis / verbleibende Arbeit |
| --- | --- |
| Lokaler Android-Start | Bootabschluss und sichtbare Oberfläche bestätigt, siehe `qemu-first-boot.md`. |
| ADB und Bildschirm | Authentifizierte Verbindung, Dateiübertragung und Bildschirmaufnahme geprüft; siehe `local-adb.md`. |
| Bedienung | Virtuelle Tastatur schreibt den vollständigen Testtext; relative Maus öffnet mit linkem Klick eine Einstellungsseite. Native Mac-Fensterbedienung noch prüfen; Mac beim Versuch gesperrt. |
| Dauerhafte Daten und Schlüssel | Neuer Helper auf dem Server gebaut und über GitHub geprüft bezogen. Vollständiger QEMU-Neustart mit persönlichem Passwort: falsches Passwort abgewiesen, CE gesperrt, richtiges Passwort liefert dieselben 4096 Bytes. Doppelstart, fehlende/fremde Disk und verlorener TPM-Zustand auf einer Kopie abgewiesen. Stromausfall- und Migrationsnachweis offen; siehe `persistent-qemu.md`. |
| Gerätedienste | Bluetooth-Abstürze und NFC-Controller-Timeouts im bisherigen Image dokumentiert; zuletzt keine laufende Absturzschleife aller HALs belegt. Bluetooth-Schalter sowie fehlende NFC-/UWB-/Thread-Funktionen im nächsten Produktstand konfiguriert. Noch ungebaut; Framework-Start, verbliebene native HALs und weitere geerbte Geräte im neuen Gast prüfen. Siehe `qemu-hardware.md`. |
| AEGIS-Identität/CLI | AOSP-Adapter, prozessgebundener Binder-Dienst und interaktive CLI einschließlich Ersteinrichtung und Benutzeranlage/-löschung mit frischer Adminprüfung im Quelltext. Produktpakete, Systemserver-Classpath, Bootressource und SELinux-Zuordnung ergänzt; alles noch unkompiliert und nicht im Gast. Server-Check baut auch Ressourcen/Policy, Kennungsregister, die betroffenen Framework-Dienste und 44 vorbereitete Android-Tests. Der neue lesende Dienstcheck weist die fehlende Integration im bisherigen Image korrekt zurück. Runtime-Koordination fehlt. Siehe `identity-cli.md`. |
| AOSP-Passwortgrundlage | Ein persönlicher Testbenutzer: falsches Passwort abgewiesen, CE-Sperre nach Benutzerstopp bestätigt, richtiges Passwort stellt Dateizugriff wieder her. Nach Passwortwechsel wird das alte Passwort abgewiesen; das neue erhält dieselben Daten. Anschließend Plattformlöschung und Abwesenheit von acht Schlüssel-/Datenpfaden bestätigt. Tests über AOSP-Dialoge und Plattformbefehle, noch nicht über AEGIS; siehe `identity-platform-test.md`. |
| GNU/Linux-Runtime | Offizielle Debian-13.7-ARM64-Basis festgelegt und als unverändertes Archiv importiert/geprüft: 78 Pakete, darunter glibc, Bash und apt. Keine Extraktion oder Ausführung. UID/GID-Zuordnung und AOSP-Registerprüfung vorbereitet; alle 38 Basiskennungen abgedeckt, aber noch keine laufenden Maps. Laufzeitverwaltung, AOSP-Autorisierung, Mounts und SELinux-Integration fehlen. Der aktuelle Kernel erfüllt die notwendigen Namespace-Anforderungen nicht. Siehe `../runtime/README.md` und `../runtime/uid-mapping.md`. |
| Pakete und Isolation | Noch zu implementieren und mit zwei AOSP-Benutzern praktisch zu prüfen. |
| Vollständiger Ablauf | Noch kein Nachweis für Login, Wechsel, Logout mit CE-Sperrung und Neustart mit zwei passwortgeschützten Benutzern. |

Die ausdrückliche Fortsetzung einer protokollierten Ersteinrichtung ist mit
`setup --resume NAME` im Quelltext angebunden. Vorhandene Passwörter bleiben
erhalten und müssen über AOSP bestätigt werden; der Abschluss verlangt einen
gestoppten Benutzer und gesperrten CE-Speicher. Zwölf zusätzliche Android-Tests
sind vorbereitet. **Kompilierung, Ausführung der neuen Gerätetests sowie echte
Abbruch-/Neustartversuche in QEMU stehen aus.** Nicht eindeutig zugeordnete oder
partielle AOSP-Konten werden dabei nicht automatisch übernommen.

Als nächster Runtime-Baustein liegt jetzt der
[persönliche Prozessaufseher](../runtime/process-supervisor.md) mit privaten
Kontrollkanälen, Shell-PTYs, UID/GID-Wechsel und Prozessende bei Brokerverlust
im Quelltext vor. Die native Kennungsdatei wird mit Java und AOSP-Register
gemeinsam erzeugt. Der Komponenten-Build umfasst zusätzlich den Aufseher und
vorbereitete ARM64-Gerätetests. **Noch nicht kompiliert, nicht aktiviert und
nicht im Gast getestet.** Broker, Mounts, SELinux-Anbindung, Paketverwaltung
und vollständige AOSP-Logout-Koordination bleiben offen.

Die Verwaltungsseite besitzt zusätzlich eine interne Bibliothek zur Bestätigung
des tatsächlichen Kindprozessendes über eine stabile Kernel-Prozessreferenz.
Timeouts behalten die Referenz; anderweitig verbrauchte Exitdaten bestätigen
keinen Erfolg. Zehn zusätzliche Gerätetests sind vorbereitet. **Auch dieser
Code ist unkompiliert und nicht im Gast geprüft.** Der Abgleich mit AOSP zeigt,
dass `onUserStopping` allein keine bestätigte Barriere für den Runtime-Abbau
bietet; die vollständige Stopp-/CE-Koordination bleibt zu implementieren.

Der [Namespace-Start](../runtime/namespace-launch.md) ist jetzt ebenfalls im
Quelltext vorbereitet: gemeinsam erzeugte Namespaces/Pidfd, begrenztes Warten
auf beide geprüften UID/GID-Maps, keine geerbten Zusatzgruppen, feste Umgebung
und FD-Übergabe sowie beobachtbares Prozessende auch nach Startfehlern. Neun
zusätzliche native Gerätetests sind vorbereitet; damit sind es insgesamt
zunächst 31 native Tests. **Noch nicht kompiliert, aktiviert oder in QEMU ausgeführt.**
Der neue Code ersetzt weder den fehlenden Broker noch den Mount-Helfer,
SELinux-Integration, AOSP-Sitzungsprüfung oder den Zwei-Benutzer-Nachweis.

Die Maps können inzwischen ohne Exec-Freigabe vorbereitet werden. Darauf baut
eine [persönliche, schreibgeschützte Basis-Sicht](../runtime/base-mounts.md) auf:
derselbe Dateibestand erhält unterschiedliche Mount-Eigentümer, ohne die Quelle
umzuschreiben. Vier weitere native Tests sind vorbereitet, insgesamt jetzt 35.
**Auch diese Erweiterung ist unkompiliert und nicht im Gast geprüft.** Die Tests
verwenden ein inertes Tmpfs; echter ext4-Basis-Mount, CE-Einbindung, Rootwechsel
und ausführbarer Runtime-Kontext bleiben gesondert offen.

Die [persönliche Speicheranbindung](../runtime/personal-storage.md) ist jetzt
ebenfalls im Quelltext vorbereitet: unveränderliche ID-/Seriennummernbindung,
AOSP-Seriennummer und gleiche CE-Richtlinie, kontrollierte HOME-Erstanlage,
strikte Wiederöffnung und detached Mount ohne doppelte ID-Zuordnung.
Fünf weitere native Negativtests sind vorbereitet, insgesamt jetzt 40.
**Unkompiliert und nicht im Gast geprüft.** AOSP-Lebenszyklus-Sperre, echter
Mount-Helfer, SELinux, private Paketbestände und Zwei-Benutzer-Nachweis fehlen.

Das [private Geräteverzeichnis](../runtime/private-devices.md) ist ebenfalls
im Quelltext vorbereitet: begrenztes frisches Tmpfs, sechs festgelegte
Zeichengeräte, leere Terminal-/IPC-Mountpunkte, feste Links und geprüfte
schreibgeschützte Metadaten mit persönlicher ID-Zuordnung. Der Namespace-Start
trennt zusätzlich Terminalsitzung und Prozessgruppe vom Broker. Zwei weitere
native Tests sind vorbereitet, insgesamt 42. **Unkompiliert und nicht im Gast
ausgeführt.** Die echte Mountübergabe, private Prozess-/Terminal-Dateisysteme,
Rootwechsel und durchgesetzte SELinux-Regeln bleiben offen.

Der [statische Mount- und Starthelfer](../runtime/namespace-setup.md) ist jetzt
ebenfalls im Quelltext vorhanden: feste, an ID und Seriennummer gebundene
FD-Übergabe, eigene Procfs-/Devpts-/IPC-/Tmpfs-Sichten, Rootwechsel mit Abtrennen
der Android-Wurzel und anschließendes Rücklesen der Mounts. Acht weitere native
Tests sind vorbereitet, insgesamt 50. **Noch nicht kompiliert oder im Gast
ausgeführt.** Broker, Lebenszyklus-Sperre, Ressourcen-Cgroups, SELinux-Typen und
Übergänge sowie der praktische Rootwechsel-/Isolationsnachweis fehlen weiterhin.

Die [vorgeschaltete Speicherkoordination](../runtime/aosp-storage-lifecycle.md)
ist jetzt an fünf AOSP-Schlüsseloperationen im Quelltext angebunden. Der
fehlende Controller muss den Ressourcenabbau bestätigen, bevor Schlüssel
entzogen werden; Fehler dürfen keine erfolgreiche Sperrbestätigung erzeugen.
Acht zusätzliche Java-Tests sind vorbereitet, insgesamt 32. Der Komponentenlauf
baut nun auch die betroffenen Framework-Dienste. **Noch nicht auf dem Server
kompiliert oder im Gast getestet; Runtime-Modus weiterhin `absent`.**

Die [Zugangsserialisierung](../runtime/admission.md) liegt ebenfalls als
Quelltext vor: einmalige Anmeldeversuche, getrennte Benutzersperren,
Seriennummernbindung und Widerruf vor/nach destruktiven Speicheroperationen.
Zwölf weitere Android-Tests sind vorbereitet, insgesamt 44. **Unkompiliert,
ungetestet und noch nicht aktiviert.** Native Abbaubestätigung, vollständige
AOSP-Lebenszyklus-Anbindung und Pakettransaktionen fehlen weiterhin.

Auch die [gemeinsame Softwaregeneration](../runtime/generations.md) besitzt nun
ein Buildrezept: technischer NSS-Benutzer, explizite Datei-Eigentümer, Entfernen
der Set-ID-Bits, wiederholte ext4-Erzeugung und Lesen des tatsächlichen
Image-Inhalts. Die echte Debian-Basis ergibt im Plan 78 Pakete und 3.271
Einträge. Vierzehn Hosttests mit inerten Archiv-/Dateisystemfixtures prüfen
Metadaten und Fehlerpfade. **Das echte Image wurde noch nicht auf dem Server
gebaut, hochgeladen oder in QEMU eingebunden.**

Die explizite Übernahme eines abgeschlossenen Basislaufs in den vollständigen
Systembuild ist ebenfalls im Quelltext angebunden: Rekonstruktion des Plans
aus dem Originalimport, erneute Inhaltsprüfung, Erhalt einer ausgewählten
Kernelkonfiguration, Produktdateien und Release-Nachweise. Elf Metadatentests,
vier Worker-Transporttests und eine Pfadprüfung wurden ergänzt. **Server-Build,
Prüfung der tatsächlichen Partitionsimages, Gast-Mounts und Runtime-Start stehen
weiterhin aus.** Die Ablage von Basisdateien aktiviert keinen Runtime-Modus.

Der Vollbuild enthält jetzt zusätzlich einen vorbereiteten Prüfschritt für die
Basisdateien innerhalb der tatsächlich ausgelieferten `super.img`: Slot-A-
Partition lesen, EROFS-Inhalte prüfen und Größen/Hashes abgleichen. Der zugehörige
Bericht ist für jeden Release erforderlich und wird nach dem Upload zurückgelesen.
Fehlerfalltests verwenden inerte Dateien; Linux-CI prüft zusätzlich echte kleine
EROFS-Textfixtures. **Mit dem neuen echten AOSP-Image und dessen Werkzeugen ist
dieser Schritt noch nicht ausgeführt; er bestätigt keinen Boot oder Runtime-Start.**

## Bestätigte Kernel-Lücke

Der laufende Kernel 6.12.18 meldet in `/proc/config.gz`:

```text
# CONFIG_SYSVIPC is not set
# CONFIG_USER_NS is not set
# CONFIG_PID_NS is not set
# CONFIG_VIRTIO_NET is not set
CONFIG_UTS_NS=y
CONFIG_NET_NS=y
CONFIG_EXT4_FS=y
CONFIG_OVERLAY_FS=y
```

Die Zeile zu `CONFIG_VIRTIO_NET` beschreibt nur die GKI-Konfiguration: Im
laufenden Gast ist `virtio_net` als separates, passendes Virtual-Device-Modul
bereits geladen (`/proc/modules`, Lauf `mouse-1`). Daraus folgt kein fehlender
Netzwerktreiber. Der Launcher richtet bislang keine Gast-Netzwerkkarte ein.

Wegen der fehlenden User-/PID-Namespaces und System-V-IPC ist für die
beauftragte gemeinsame GNU/Linux-Runtime ein gezielter
Kernel-Build einschließlich passender Module erforderlich. User-, PID-, Mount-
und IPC-Isolation müssen anschließend tatsächlich funktionieren. Ein chroot
allein oder eine zusätzliche Linux-VM erfüllt den Auftrag nicht. Der genaue
Kernel-Quellstand, Konfiguration, Module und Android-Integration sind vor dem
Build abzugleichen; das bloße Hinzufügen von Konfigurationszeilen reicht nicht.

Die passenden 40 Kernel-Quellprojekte sind inzwischen aus dem offiziellen
Manifest des laufenden Builds `13257114` festgelegt. Ein Buildrezept mit
gemeinsamem Namespace-Fragment für Kernel und Module liegt unter
[`kernel/`](../kernel/README.md). Die Übernahme eines ausdrücklich gewählten
Laufs in den vollständigen AOSP-Build ist inzwischen im Quelltext vorbereitet:
eingebettete Kernelkonfiguration, Modulversionen, Boot-Treiber, gemeinsame
Pfadauswahl und der resultierende Kernel werden geprüft, der Eingabenachweis
über GitHub mitgeliefert. Das Rezept und die Übernahme sind noch nicht mit einem
neuen Kernel auf dem Builder ausgeführt; tatsächliche Images und Gasttests fehlen.

## Reihenfolge

1. Erreichten geordneten Neustartnachweis um Absturz- und Migrationsfälle
   ergänzen; gekoppelte Android-/Helper-Profile beibehalten.
2. QEMU-Hardwarekonfiguration und Bedienung bereinigen; Kernel-Build vorbereiten.
3. AOSP-vermittelte AEGIS-Anmeldung und Benutzerlebenszyklus integrieren.
4. Runtime und Paketoperationen mit AOSP-Adminautorisierung integrieren.
5. Alle Anmelde-, Daten-, Prozess- und Logout-Anforderungen mit zwei Benutzern
   einschließlich Fehlerfällen prüfen.

Der Komponentencheck für `5447a968d00f63aa7cb700d868508c437d89980c` wurde am
28. September auf `aegis-build` gestartet. Der erste Lauf endete um 12:41 UTC
durch OOM beim Soong-Buildplan: nur 14,7 GiB Gast-RAM statt der ursprünglichen
rund 94 GiB, 8 GiB Swap nahezu ausgeschöpft. AOSPs Parser akzeptierte zuvor
die 1.002 vorbereiteten Runtime-Kennungen; das belegt keine laufenden UID-Maps.

Nach der vom Nutzer vorgenommenen Hyper-V-Anpassung meldet Ubuntu rund 65 GiB
RAM und 62 GiB verfügbar. Der Ersatzlauf startete um 12:48 UTC mit derselben
Revision und InvocationID `5b747d59dd5947a4b8b0ca30ddf703c3`. Soong schloss
seine Analyse ab; der Speicherhöchststand lag bei rund 37,8 GiB. Um 12:54:52 UTC
brach Kati nach 5:42 Minuten an der Artefaktgrenze von `generic_system.mk` ab:
CLI, Dienst-JAR und zugehörige Dex-Artefakte wurden vom Geräteprodukt
fälschlich in `system` installiert. Das Journal belegt den Fehler, auch wenn
der inzwischen eingesammelte Dienst `inactive` und `ExecMainStatus=0` meldet.
Der Lauf `identity-20260928T124851Z-5447a968-1zAsj2` enthält die
32 Java-Tests dieser Revision und die 50 nativen Tests; die später vorbereitete
Zugangsserialisierung ist darin noch nicht enthalten. Kein Gasttest läuft auf
dem Server. Die neue RAM-Vorprüfung und bereinigte Skript-Fehlerbehandlung sind
für einen folgenden Quellstand vorbereitet. Die Partitionskorrektur setzt CLI
und Dienst auf `system_ext` und ergänzt den nötigen expliziten CLI-Klassenpfad;
Details in [Identitätsintegration](identity-cli.md#vorbereitete-produktintegration).
**Weiterhin kein bestätigter Komponentenabschluss oder Gasttest.** Der
fehlgeschlagene Checkout bleibt erhalten. Ein um 13:18 UTC gestarteter weiterer
Lauf verwendete nochmals die alte Revision `5447a96` und endete um 13:24:45 UTC
mit demselben Partitionsfehler; er prüfte die Korrektur noch nicht.

Der [Komponenten-Transport](component-transport.md) ist jetzt ebenfalls
vorbereitet: erfolgreicher Buildstatus und vollständige Modulprüfsummen als
Voraussetzung, getrennte Build-/Export-Commits, GitHub-Entwurf, Rückdownload
aller Assets und geprüfte lokale Extraktion. Die Transportfixtures führen
keinen neuen Android-/Runtime-Code aus. **Noch kein echter Komponentenexport:**
Ein erfolgreicher Lauf mit dem korrigierten System-Ext-Profil ist weiterhin
Voraussetzung. Die späteren Exportwerkzeuge verändern die mit `283452a`
zu kompilierenden Komponenten nicht.

Nach der Mitteilung des Nutzers wurde der passwortlose `sudo -n`-Zugriff
erfolgreich geprüft. Am 28. September um 13:29:07 UTC wurde der **korrigierte**
Stand `283452ae7ce82344bd94fc865e35abe58b1241b6` direkt gestartet:
InvocationID `7b0d8b055ab94cafb892e6c424f8e1f1`, Lauf
`identity-20260928T132908Z-283452ae-N06WMX`. Quellcode und Startskript kamen von
GitHub. Dieser Lauf passierte die Artefaktgrenze; die erzeugten Modulregeln
verweisen korrekt auf `system_ext`. Um 13:35:20 UTC endete er nach 5:57 Minuten
an der anschließenden Systemserver-Dex-Prüfung, die noch `system/framework`
erwartete. Die Produktliste erhält deshalb zusätzlich den in AOSPs
`ConfiguredJarList` vorgesehenen Präfix `system_ext:aegis-identity-service`.
Kompilierabschluss und Gasttests bleiben offen. Die Linux-CI des separaten
Exportwerkzeugs `f234349` besteht mit 190 Tests ohne Auslassungen; davon prüfen
16 neue Tests ausschließlich Transport und Veröffentlichungsablauf mit
inerten Dateien und einer GitHub-Fixture.

Der Lauf `identity-20260928T133821Z-024354c1-4sBywQ` passierte am
28. September beide Partitionsprüfungen und begann die native Kompilierung.
Er endete um 13:46:09 UTC nach 7:31 Minuten mit Status `FAILED`: Bionics
ARM64-`statfs.f_type` ist vorzeichenlos, der erwartete Dateisystemtyp in
`sandbox.c` war dagegen `long`. Die Erwartungen in Sandbox und Mount-Helfer
werden deshalb einheitlich als `uint64_t` geführt; die Prüfwerte und
Compilerwarnungen bleiben unverändert. Mehrere native Quellen und Tests
kompilierten bereits, aber kein vollständiger Modulabschluss ist bestätigt.
Die Linux-CI von `024354c` besteht mit 190 Tests ohne Auslassungen.

Der persistente KeyMint-Helfer wurde anschließend aus `024354c` auf dem
Server kompiliert und als `secure-env-20260928T134804Z-024354c1` über GitHub
veröffentlicht. Das Exportskript hat das Archiv zurückgeladen und bytegenau
verglichen. Lokale Prüfung und tatsächliche Wiederentschlüsselung nach
einem Neustart wurden anschließend bestätigt; siehe `persistent-qemu.md`.

Der nächste Komponentenlauf `identity-20260928T135507Z-d8cfe22b-ZoTz0n`
kompilierte die korrigierte Dateisystemprüfung. Er endete um 14:03:17 UTC nach
7:53 Minuten: `MQUEUE_MAGIC` fehlt in Bionics UAPI-Headern. Der Mount-Helfer
erhält den Wert `0x19800202` aus `ipc/mqueue.c` des exakten Kernel-Pins
`50eb8d5d443b43f38d6e72f005f1b8601ac88a05` unter einem eigenen Konstantennamen.
Die Prüfung des Message-Queue-Dateisystems bleibt erhalten. Der weitere
`sizeof`-Fehler war eine Folge des ungültigen Tabelleninitialisierers.

Der Lauf `identity-20260928T140941Z-5e045c4e-i5Skvo` kompilierte und linkte
die nativen Init-/Setup-/Probe-Binaries. Auch CLI, Identitätskern und
Speichergate erreichten die Java-Kompilierung. Der Gesamtlauf endete nach
9:32 Minuten mit `FAILED`: `ProductConfigurationTest` verwendete
`Os.getpwnam`, `getpwuid` und `StructPasswd`, die in der stabilen libcore-API
des Testmoduls nicht enthalten sind. Die Vorwärtsprüfung verwendet nun
`Process.getUidForName`/`getGidForName`; ein zusätzlicher nativer Test prüft
kanonische Vorwärts-/Rückwärtsnamen über Bionic. Damit sind 44 Java- und
51 native Gerätetests vorbereitet. Ihr vollständiger Build und ihre Ausführung
bleiben offen; der Dienst-/Framework-Build ist noch nicht abgeschlossen.

Unveränderte Produkt-/Identitätsdateien behalten nun ihre Zeitstempel, damit
kleine Änderungen nicht allein wegen erneuter Quellkopien den gesamten
Make-/Soong-Buildplan invalidieren. Die Linux-CI von `13485f1` besteht mit
195 Tests ohne Auslassungen. Eine tatsächliche Zeitmessung folgt mit dem
nächsten Komponentenlauf.
