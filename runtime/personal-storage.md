# Persönlicher AOSP-CE-Speicher

Stand 29. September 2026: **Im vollständigen `d44ccb33` auf lokalem Mac-QEMU
mit zwei persönlichen Benutzern ausgeführt.** Der Broker verbindet AOSP-Anmeldung,
CE-Prüfung, Mount-Helfer und SELinux. Die Bibliothek selbst ist weiterhin keine
Identitätsautorität. Prüfumfang und noch offener Terminalwiderruf stehen im
[GNU-Test](../docs/runtime-gnu-qemu-test.md).

## Identitätsbindung und Herkunft

Der Namespace-Kontext speichert jetzt bereits bei seiner Erzeugung sowohl
`userId` als auch die nichtnegative AOSP-Seriennummer unveränderlich. Beide
werden dem vertrauenswürdigen Setup-Helfer übergeben. Eine später wiederverwendete
Nummer kann dadurch nicht nachträglich auf einem alten Handle neu gebunden werden.
Der Broker muss trotzdem vor Wiederverwendung alle alten Prozesse, Mounts und
Transaktionen nachweislich abbauen: die Linux-Hostkennung enthält keine Seriennummer.

`aegis_namespace_home_mount` öffnet nach abgeschlossenen Maps und vor Exec-Freigabe
das echte `/data`. Der Broker muss im initialen Android-User- und PID-Namespace
laufen. Zulässig ist ausschließlich der initiale Mount-Namespace oder die
eigene ausdrücklich geprüfte private Kopie; deren Basisanker ersetzt `/data`
nicht. Kein Client kann einen Quellpfad oder eine alternative
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
Löschung oder Logout bleiben Voraussetzungen der Broker-Zulassung.

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
Die zehn [Standardordner](aosp-storage-lifecycle.md#erstmalige-persönliche-home-struktur)
entstehen ausschließlich im unveröffentlichten leeren Home. Eigene spätere
Löschungen, Umbenennungen, Rechteänderungen und Verknüpfungen darunter werden
nicht zurückgesetzt; dies wurde auch nach Reboot tatsächlich geprüft.

Eigentümer, Rechte und Verschlüsselungsrichtlinie werden erneut geprüft; die
offenen Verzeichnisse müssen weiterhin unter denselben Namen/Inodes erreichbar
sein. Diese Prüfungen ersetzen keine AOSP-Lebenszyklus-Synchronisierung.
Eine vom Benutzer geänderte HOME-Wurzel mit nicht mehr passendem Modus wird
abgewiesen, nicht beim nächsten Login heimlich repariert.

## Mount und Abbau

Aus dem geprüften HOME entsteht ein nichtrekursiver detached Mount mit
`nosuid`, `nodev` und privater Mountweitergabe. Er ist beschreibbar. Die aktuelle
SELinux-Policy verbietet direkte Ausführung von Dateien aus beschreibbarem
Home und Scratch, unabhängig von einem fehlenden `noexec`-Mountflag. Ausführbare
private Paketgenerationen benötigen einen gesonderten autorisierten Pfad;
dieser ist noch nicht implementiert. Die Mount-Erstellung ändert die Quelle nicht.
**Keine zweite ID-Zuordnung:** persönliche Dateien tragen bereits die Host-UID/GID
dieses AOSP-Benutzers. Der Shared-Base-Mount hat andere Eigentümersemantik.

Der Aufrufer erhält einen eigenen `CLOEXEC`-Deskriptor. Bei Fehler werden alle
hier geöffneten Referenzen geschlossen. Die Zehn-Sekunden-Startfrist des Kindes
wird nicht verlängert; vor Rückgabe wird sein Zustand nochmals geprüft.
Übergabe an den Mount-Helfer, `/home/user`-Einbindung, Rootwechsel und
Sandbox wurden im persönlichen GNU-Pfad geprüft. Programme aus der geprüften
gemeinsamen Softwarebasis laufen mit internem UID/GID 1000 und getrennten
Hostkennungen; die CE-Prüfung allein garantiert das nicht.

Bei Logout müssen **alle** Prozesse, offenen Dateien und extern gehaltenen
CE-Mount-/Verzeichnisreferenzen enden, bevor AOSP den Schlüssel entfernt. Der
Kernel kann bei noch benutzten Dateien Schlüsselreste behalten. Weder das
Schließen dieses einen Handles noch AOSPs Benutzerstatus allein beweist den
vollständigen Ressourcenabbau und Schlüsselentzug.

## Prüfungen und verbleibende Grenzen

Fünf neue native Tests verwenden ausschließlich detached, unverschlüsselte
Tmpfs-Textfixtures. Sie prüfen strikte Seriennummern, Nummernwiederverwendung,
Symlink-/Pfadabweisung, fehlende Verschlüsselung, ungültige Identitäten und
Deskriptorlecks. Vorhandene Namespace-Tests prüfen zusätzlich Seriennummern-
Übergabe und die Verweigerung vor Maps, nach Prozessende und bei fremdem Besitzer.
Diese und die späteren Layout-, Namespace- und Lebenszyklus-Fixtures sind
Bestandteil der **128/128 nativen Gerätetests** von `d44ccb33`, ausgeführt
im lokalen `6a807692`-Image. Hosttests führen diesen nativen Code nicht aus.
[Komponentenbelege](../docs/component-tests.md).

Erfolgreiche Erststruktur und Wiederöffnung auf echtem AOSP-CE, getrennte Nutzer,
unveränderte GNU-Dateien und Konfiguration nach Neustart sowie bestätigter
Logout mit unlesbaren vorher geschriebenen Dateien sind zusätzlich im Vollbuild
`d44ccb33` geprüft. Die unverschlüsselten Fixtures ersetzen diesen Nachweis nicht.
Fehlerinjektion bei offenen Referenzen, alle konkurrierenden Speicheraktionen,
verwaltete Benutzerlöschung und Pakettransaktionen bleiben ausstehend.

Quellabgleich: Android 16 r1
[UserDataPreparer](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/pm/UserDataPreparer.java),
[Vold FsCrypt](https://android.googlesource.com/platform/system/vold/+/refs/tags/android-16.0.0_r1/FsCrypt.cpp),
[Bionic fscrypt-UAPI](https://android.googlesource.com/platform/bionic/+/refs/tags/android-16.0.0_r1/libc/kernel/uapi/linux/fscrypt.h)
und [Kernel-fscrypt-Dokumentation](https://docs.kernel.org/filesystems/fscrypt.html).
