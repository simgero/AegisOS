# Persönlicher AOSP-CE-Speicher

Stand: **Quelltext vorbereitet, noch nicht kompiliert oder im Gast ausgeführt.**
Die Runtime bleibt deaktiviert. Diese Bibliothek ersetzt weder AOSP-Anmeldung
noch den fehlenden Broker, Mount-Helfer oder die SELinux-Integration.

## Identitätsbindung und Herkunft

Der Namespace-Kontext speichert jetzt bereits bei seiner Erzeugung sowohl
`userId` als auch die nichtnegative AOSP-Seriennummer unveränderlich. Beide
werden dem vertrauenswürdigen Setup-Helfer übergeben. Eine später wiederverwendete
Nummer kann dadurch nicht nachträglich auf einem alten Handle neu gebunden werden.
Der Broker muss trotzdem vor Wiederverwendung alle alten Prozesse, Mounts und
Transaktionen nachweislich abbauen: die Linux-Hostkennung enthält keine Seriennummer.

`aegis_namespace_home_mount` öffnet nach abgeschlossenen Maps und vor Exec-Freigabe
das echte `/data`. Der Broker muss im initialen Android-User-, PID- **und**
Mount-Namespace laufen. Kein Client kann einen Quellpfad oder eine alternative
Datenwurzel an diesen Einstiegspunkt übergeben. Unterhalb des offenen `/data`
werden Verzeichnisse mit `openat2` verankert; Symlinks, magische Links, Ausbrüche
und Mountwechsel werden abgewiesen. Vorerst wird nur interner Speicher unterstützt.

Die Prüfung verwendet zwei AOSP-Verzeichnisse:

| Verzeichnis | Prüfung |
| --- | --- |
| `system_ce/USER_ID` | AOSP-Eigentümer/Modus, vorhandenes `user.serial` exakt als kanonische Dezimalzahl, fscrypt-v2-Richtlinie und vorhandener Schlüssel |
| `misc_ce/USER_ID` | AOSP-Eigentümer/Modus, dieselbe vollständige fscrypt-v2-Richtlinie einschließlich Schlüsselkennung |

AOSP versieht **system_ce**, nicht misc_ce, mit dieser Seriennummer. Vold weist
beiden Verzeichnissen im internen Speicher dieselbe CE-Richtlinie zu. Fehlende,
falsch formatierte oder abweichende Seriennummern werden abgewiesen; AEGIS ergänzt
oder repariert weder AOSPs Attribut noch seine Verschlüsselungsrichtlinie.

Die Implementierung liest ausschließlich Richtlinie und Kernel-Schlüsselstatus.
Sie erzeugt, exportiert, setzt oder entfernt keine CE-Schlüssel. Nur `PRESENT`
wird akzeptiert, nicht `INCOMPLETELY_REMOVED`. **Schlüsselpräsenz beweist keine
Anmeldung.** Frische AOSP-Authentifizierung, aktueller Benutzer plus Seriennummer,
bestätigte CE-Entsperrung und eine Sperre gegen konkurrierenden Benutzerstopp,
Löschung oder Logout bleiben Voraussetzungen des noch zu implementierenden Brokers.

## Kontrollierte Erstanlage

Unter `misc_ce/USER_ID/aegis` liegt ein Root-eigenes Verzeichnis mit Modus 0700.
Sein Verwaltungsattribut `user.aegis.owner` enthält ausschließlich Schemafassung,
AOSP-Benutzerkennung und Seriennummer (`1:USER_ID:SERIAL`), keine Passwörter oder
Schlüssel. `home` darunter gehört der zugeordneten gewöhnlichen Host-UID/GID
(Runtime-UID 1000) und hat ebenfalls Modus 0700. ACLs werden an beiden Stellen
abgewiesen. HOME ist vom Benutzer beschreibbar; die darüberliegende Verankerung
kann er nicht ersetzen. Private Paketgenerationen sind damit noch nicht eingerichtet.

Bei ausdrücklicher Erstanlage entsteht zunächst `.aegis-preparing`. Beide
Verzeichnisse müssen AOSPs CE-Richtlinie geerbt haben. Nach Rechte-, Zuordnungs-
und Schlüsselprüfung sowie `fsync` wird die vollständige Struktur mit
`RENAME_NOREPLACE` sichtbar und das Elternverzeichnis synchronisiert. Vorhandene
fremde oder unvollständige Daten werden niemals still übernommen, umbenannt,
nachträglich umgeordnet oder gelöscht. Ein verbliebener Vorbereitungsordner
führt zu einem sichtbaren Fehler und braucht eine gesonderte Wiederherstellung.
Ein gültig abgeschlossener Bestand wird bei Wiederöffnung nur geprüft.

Eigentümer, Rechte und Verschlüsselungsrichtlinie werden erneut geprüft; die
offenen Verzeichnisse müssen weiterhin unter denselben Namen/Inodes erreichbar
sein. Diese Prüfungen ersetzen keine AOSP-Lebenszyklus-Synchronisierung.
Eine vom Benutzer geänderte HOME-Wurzel mit nicht mehr passendem Modus wird
abgewiesen, nicht beim nächsten Login heimlich repariert.

## Mount und Abbau

Aus dem geprüften HOME entsteht ein nichtrekursiver detached Mount mit
`nosuid`, `nodev` und privater Mountweitergabe. Er ist beschreibbar und erlaubt
später eigene ausführbare Linux-Programme. Die Quelle bleibt unverändert.
**Keine zweite ID-Zuordnung:** persönliche Dateien tragen bereits die Host-UID/GID
dieses AOSP-Benutzers. Der Shared-Base-Mount hat andere Eigentümersemantik.

Der Aufrufer erhält einen eigenen `CLOEXEC`-Deskriptor. Bei Fehler werden alle
hier geöffneten Referenzen geschlossen. Die Zehn-Sekunden-Startfrist des Kindes
wird nicht verlängert; vor Rückgabe wird sein Zustand nochmals geprüft.
Die Übergabe an den echten Mount-Helfer, `/home/user`-Einbindung, Rootwechsel und
Sandbox bleiben offen. Erst danach dürfen persönliche Programme ausgeführt werden.

Bei Logout müssen **alle** Prozesse, offenen Dateien und extern gehaltenen
CE-Mount-/Verzeichnisreferenzen enden, bevor AOSP den Schlüssel entfernt. Der
Kernel kann bei noch benutzten Dateien Schlüsselreste behalten. Weder das
Schließen dieses einen Handles noch AOSPs Benutzerstatus allein beweist den
vollständigen Ressourcenabbau und Schlüsselentzug.

## Noch ausstehender Nachweis

Fünf neue native Tests verwenden ausschließlich detached, unverschlüsselte
Tmpfs-Textfixtures. Sie prüfen strikte Seriennummern, Nummernwiederverwendung,
Symlink-/Pfadabweisung, fehlende Verschlüsselung, ungültige Identitäten und
Deskriptorlecks. Vorhandene Namespace-Tests prüfen zusätzlich Seriennummern-
Übergabe und die Verweigerung vor Maps, nach Prozessende und bei fremdem Besitzer.
Zusammen mit der [Gerätevorbereitung](private-devices.md) sind inzwischen
42 native Gerätetests vorbereitet, **keiner davon hier neu kompiliert
oder ausgeführt**. Hosttests führen diesen nativen Code nicht aus.

Erfolgreiche Anlage und Wiederöffnung auf echtem AOSP-CE, zwei getrennte Nutzer,
falsche Passwörter, Schlüsselentzug bei offenen Referenzen, Abbrüche, Logout und
Neustart müssen nach dem Server-Build im lokalen Android-QEMU geprüft werden.
Die unverschlüsselten Negativfixtures beweisen diese Abläufe ausdrücklich nicht.

Quellabgleich: Android 16 r1
[UserDataPreparer](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/pm/UserDataPreparer.java),
[Vold FsCrypt](https://android.googlesource.com/platform/system/vold/+/refs/tags/android-16.0.0_r1/FsCrypt.cpp),
[Bionic fscrypt-UAPI](https://android.googlesource.com/platform/bionic/+/refs/tags/android-16.0.0_r1/libc/kernel/uapi/linux/fscrypt.h)
und [Kernel-fscrypt-Dokumentation](https://docs.kernel.org/filesystems/fscrypt.html).
