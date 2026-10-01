# Hardware des lokalen QEMU-Produkts

## Zeitabweichung nach längerem Betrieb, 1. Oktober 2026

Beim Pakettest im unveränderten Gast `d0b866e1` lag die Uhr um etwa
46 Minuten hinter der Mac-Uhr. Android hatte seit dem Start nur eine
NTP-Messung übernommen; der nächste reguläre Abruf war erst nach 18 Stunden
Gastlaufzeit vorgesehen. Das passt zu einer angehaltenen virtuellen Uhr,
beweist aber noch nicht den konkreten Auslöser. Aktuelle Debian-Signaturen
wurden deshalb korrekt als nach dem Prüfzeitpunkt erstellt abgewiesen.

`cmd network_time_update_service force_refresh` bezog erfolgreich eine neue
Messung vom bereits konfigurierten `time.android.com`. Danach betrug die
Abweichung zur Mac-Uhr weniger als eine Sekunde. Weder manuelle Testzeit noch
geänderte Zeitserver oder gelockerte Signatur-/Datumsprüfungen wurden verwendet.
Der ursprüngliche fehlgeschlagene Testlauf bleibt als Beleg erhalten.

Für das nächste Produktimage setzt die QEMU-Ressourcenüberlagerung
`config_ntpPollingInterval` auf 60.000 ms. Androids normaler Zeitdienst soll
so bei verfügbarer Netzwerkverbindung binnen einer Gastminute erneut messen.
Dies begrenzt die Zeitabweichung nach Pausen; es garantiert keine korrekte
Offline-Uhr und ersetzt noch keinen Schlaf-/Aufwachtest des neuen Vollimages.
Der aktuelle Gast verwendet weiterhin den alten Ressourcenstand; die einmalige
Aktualisierung ist kein Nachweis der neuen periodischen Einstellung.

Der [Komponentenbuild `7e49bceb`](https://github.com/simgero/AegisOS/releases/tag/components-20261001T084623Z-7e49bceb-7e49bceb-Z4VqyJ)
ist erfolgreich. `aapt2 dump resources` bestätigt im erzeugten und anschließend
über GitHub bezogenen `framework-res.apk` den Wert 60.000 ms; SHA256:
`199d238de5d1b72e2a938b48e0ba29fd49861c53821bf54c6a6bfc756837e6c2`.
Die 23 übrigen exportierten Module sind identisch zu `58be0749`.
Nachweise: `out/components-7e49bceb/compiled-ntp-resource.{txt,json}`.

Belege: `out/components-58be0749/clock-diagnostic/` mit ursprünglicher und neuer
Zeitdienst-Ausgabe, Host-/Gast-Zeiten und Ergebnis. Der erste Paketlauf liegt
unter `targeted-tests/`, der Lauf nach regulärem Zeitabgleich unter
`targeted-tests-clock-synced/` im selben Komponentenordner.

## Virtuelle Eingabe im Testgast `026665fb`, 29. September 2026

Der separate verwaltete Testgast zeigt Sperrbildschirm, Dialoge und Launcher
vollständig in **720 × 1280 bei 320 dpi**, auch ohne sichtbares Mac-Fenster.
Über QMP gesendete Enter-, Tab- und Leertasten ändern sichtbar den Android-
Zustand und Dialogfokus. Zwei ausstehende ADB-Dialoge gehören nach Vergleich
des angezeigten Fingerprints zu demselben bereits autorisierten Mac; sie
wurden über diesen Eingabeweg geschlossen. ADB bleibt authentifiziert.

Eine anschließend zeitlich begrenzte `getevent`-Erfassung bestätigt den
vollständigen virtuellen Geräteweg: `KEY_LEFTSHIFT` DOWN/UP auf
`QEMU Virtio Keyboard` (`/dev/input/event1`) sowie `REL_X=17`, `REL_Y=-9`
auf `QEMU Virtio Mouse` (`/dev/input/event2`). Die Ereignisse kommen von QMP,
nicht von Androids `input`-Befehl. Die Erfassung endet anschließend planmäßig
durch Timeout; dieser Exitcode ist kein Gerätefehler.

Rohbelege: `out/full-build-026665fb/boot-1/virtual-hid-result.json`,
`virtual-hid-events.log` und die dortigen `hid-*.png`. Das bestätigt virtuelle
Eingabe und Bildausgabe, nicht die physische Mac-Tastatur/Maus im Cocoa-Fenster,
Langzeitstabilität oder GNU-Isolation. Der Broker dieses Images scheitert
weiterhin am separat dokumentierten Basis-Mountfehler. Das normale sichtbare
Profil bleibt vorerst `2a766ab5` und ist auf Wunsch des Nutzers geschlossen.

## Zuletzt sichtbarer Stand `2a766ab5`, 29. September 2026

Der [vollständige Release](https://github.com/simgero/AegisOS/releases/tag/aosp-20260928T224751Z-2a766ab5-ed1329db)
ist nach GitHub-Transport, Prüfsummen- und AVB-Kettenprüfung lokal gebootet.
Alle **111 nativen und 62 Java-Tests** bestehen. Die Product-RRO
`org.aegisos.qemu.hardware.overlay` liefert im tatsächlichen Gast die drei
Telefonie-Booleans als false; die frühere SMS-Abweichung ist behoben.

Profil `out/qemu-profiles/foundation-2a766ab5`, UUID
`cc39ee93-2a53-4420-8285-98c5da21017f`, wurde nach dem Test geordnet beendet
(Android `Power down`, sauberer Helper-Abschluss) und sichtbar neu gestartet.
Der sichtbare Lauf liegt unter
`out/full-build-2a766ab5/interactive-20260929T011639-68287`; der Starter
`scripts/run-local-qemu.sh` öffnet jetzt dieses geprüfte Profilpaar.
Die bestehenden älteren Profile bleiben unverändert verfügbar; keine Migration.

Beide Starts erreichen den Android-Bootabschluss, SELinux Enforcing und den
AEGIS-Anmeldedienst. Die bestehende Mac-ADB-Autorisierung bleibt erhalten;
die erste Konsolenabfrage beim Wiederverbinden lief in ein Timeout, deren
verspäteter erfolgreicher Abschluss wurde gelesen und die Verbindung danach
ohne erneute Schlüsselaufnahme hergestellt. ADB liegt auf `127.0.0.1:15755`.
Der tatsächliche QEMU-Framebuffer zeigt den vollständigen Homescreen in
**720 × 1280 bei 320 dpi**, ohne Größen-/Dichteüberschreibung.

Die AVB-Digest lautet
`ccadc529983ea6b05d9d19e4666ca662d81f9050b3272b014475ff780fd6343c`.
Die realen Verity-Tabellen für `system` und `system_ext` enthalten
`restart_on_corruption`; FBE bleibt aktiv. Im begrenzten ersten Testintervall
trat kein Telefonie-ANR oder Java-Absturz auf, und der Telefonieprozess behielt
seine PID. Das ist kein Langzeit-, physischer Mac-Eingabe- oder
Produktions-SELinux-Nachweis für die weiterhin nicht aktivierte Linux-Runtime.
Rohbelege und Grenzen: [Komponententests](component-tests.md).

## Neuer Gast `bfe90925`, 29. September 2026

Der vollständige Release
`aosp-20260928T210223Z-bfe90925-32d1cb4b` wurde auf dem Mac von GitHub bezogen,
entpackt und mit AVB-Entwicklungsschlüsseln geprüft. Das neue Profil
`out/qemu-profiles/foundation-bfe90925`, UUID
`7422823d-8df5-42af-8cc6-36e6c68f8b05`, bootet vollständig; ADB authentifiziert
denselben Mac, SELinux ist Enforcing und die AEGIS-CLI antwortet. Tatsächliche
`system-verity`- und `system_ext-verity`-Tabellen enthalten weiterhin
`restart_on_corruption`. Kernel und Basis sind unverändert gepinnt.

In der ersten Beobachtung vom Start um 22:09 bis 22:13:42 UTC war kein
Telefonie-ANR, kein Warten auf `IRadioModem/slot1` und keine Java-FATAL-EXCEPTION
im vollständigen Log. `com.android.phone` behielt PID 2780. Das ist ein
begrenztes Intervall, kein Langzeitnachweis. **88/88 native und 52/53 Java-Tests
bestehen.** Der verbliebene Produktfehler ist `config_sms_capable=true`,
nachweislich aus der Vendor-RRO `android.cuttlefish.phone.overlay`.
Der normale Framework-Overlay allein überschreibt diese spätere RRO nicht.
`AegisQemuHardwareOverlay` setzt die drei Telefonie-Booleans zusätzlich in
einer Product-RRO; deren Build und neuer Gastnachweis stehen aus.

Der erste Gast wurde mit Android `Power down` und sauberem Helper-Abschluss
beendet. Ein sichtbarer Neustart desselben Profils wird unter
`out/full-build-bfe90925/visible-1` geführt und bootet ebenfalls vollständig.
Authentifiziertes ADB liegt dort auf Port 15755. Der tatsächliche
QEMU-Framebuffer zeigt den vollständigen Homescreen in **720 × 1280 bei 320 dpi**,
ohne Größen- oder Dichteüberschreibung. Die direkte physische Mac-Eingabe
wurde damit noch nicht gesondert geprüft. Die früheren Profile bleiben
erhalten; es wurde keine Benutzerdatenmigration durchgeführt.

## Früherer Gast `25fde995`

Stand 28. September 2026: Vollbuild `25fde995`, Lauf
`aosp-20260928T193617Z-25fde995-aecfe99a`, ist über GitHub verifiziert und
im lokalen Mac-QEMU vollständig gebootet. `sys.boot_completed=1`, SELinux
Enforcing, authentifiziertes ADB und AEGIS-Binder-Dienst sind bestätigt.
Die drei Audio-Module `default`, `r_submix` und `bluetooth` sind registriert.
Alle 52 Java-Tests einschließlich der vier Produktkonfigurationstests bestehen.
Der Crash-Puffer war bei der anschließenden Kontrolle leer; dies ist eine
Momentaufnahme, kein Langzeitnachweis. Der spätere Komponentenstand `bfe90925`
besteht auf diesem Image in einem separaten Gast **88 von 88 native Tests**,
einschließlich der korrigierten Basis-Mount-Tests (siehe Komponentenbericht).

Der längere [Zwei-Benutzer-CLI-Test](identity-cli-qemu-test.md) zeigt inzwischen
eine weitere offene Störung: `com.android.phone` wartet wiederholt auf den
fehlenden `IRadioModem/slot1`, endet in einem Start-ANR und wird neu gestartet.
Das trat in beiden Testboots auf. Die leere Crash-Puffer-Momentaufnahme oben
erfasst diese ANR-Schleife nicht; dauerhafte Gerätestabilität ist noch nicht erreicht.

Die nächste Quellkorrektur setzt vor der Cuttlefish-Vererbung
`TARGET_NO_TELEPHONY=true` und schließt geerbte Mobilfunkfeatures einschließlich
Telefonie, Daten, IMS und Satellit aus. Der aktuelle Gast meldet elf solche
Features, obwohl kein Modem verbunden ist. `PhoneGlobals.onCreate()` prüft
`FEATURE_TELEPHONY`, sofern `config_force_phone_globals_creation=false`; dieser
effektive Wert wurde im Gast bestätigt. Die Produkt-Overlay legt ihn sowie
Sprach-/SMS-Fähigkeit ausdrücklich auf false fest. Der Produkt-Gerätetest
prüft anschließend die effektiven Ressourcen und alle Telephony-Featurepräfixe.
Diese Korrektur ist noch nicht in einem neuen Image gebootet; insbesondere
ist das Ende der ANR-Schleife damit noch nicht nachgewiesen.

Die sichtbare Instanz bleibt für den Nutzer geöffnet, Profil
`out/qemu-profiles/foundation-25fde995`, ADB `127.0.0.1:15755`.
Android und der tatsächliche QEMU-Framebuffer wurden geprüft. Der neue
Cocoa-Lauf liefert physisch 640 × 480 trotz der angeforderten 720 × 1280.
Eine reine Größenüberschreibung erzeugte eine kleine Darstellung mit alten
Randpixeln. Deshalb bleibt die physische Größe erhalten und die Android-Dichte
ist auf 160 dpi gesetzt. Der vollständige Startbildschirm ist lesbar; ein
QMP-Mausklick öffnete Gallery. Physische Mac-Eingabe wurde nicht gesondert geprüft.

## Gefundene Startfehler und Korrektur

In `336e9275` fehlt die Bluetooth-Audio-Policy wegen
`BOARD_HAVE_BLUETOOTH=false`. Das geerbte XInclude bleibt bestehen; der
Audio-HAL registriert deshalb kein `IModule/default`. In `be1ad9ad` wurde der
Include entfernt. Damit starten `IModule/default` und `IModule/r_submix`,
aber Audioserver wartet nun auf `IModule/bluetooth`: Das unveränderte Audio-APEX
meldet diesen Software-Endpunkt weiterhin im VINTF-Manifest. In beiden Fällen
bleibt SystemServer in `StartAudioService` und `sys.boot_completed` leer.
Kernel, authentifiziertes ADB, FBE und SELinux Enforcing sind erreichbar.
Beide Testprofile wurden mit bestätigtem Android- und Helper-Powerdown beendet.
Logs: `out/full-build-336e9275/boot-1` und `out/full-build-be1ad9ad/boot-1`.

Die in `25fde995` gebootete Korrektur übernimmt wieder Cuttlefishs vollständige Original-Audiopolicy
und kopiert `bluetooth_with_le_audio_policy_configuration_7_0.xml` ausdrücklich
über `LOCAL_AUDIO_PRODUCT_COPY_FILES`. Der Bluetooth-Audio-Endpunkt im Audio-APEX
ist vom nicht vorhandenen HCI-Controller getrennt. Dessen HAL bleibt abgeschaltet;
Bluetooth bleibt für Apps als nicht verfügbar deklariert. Audioserver, VINTF,
SELinux und AVB bleiben unverändert aktiv.

Vor der Paketierung löst `scripts/aosp/check-audio.py` die XIncludes aus der
installierten Vendor-Konfiguration auf und vergleicht alle Modulnamen mit dem
gewählten AOSP-Audio-APEX-Manifest. Vier inerte XML-Regressionen bestätigen:
vollständige Konfiguration akzeptiert, fehlender Include abgewiesen, gültige XML
mit fehlendem Bluetooth-Modul abgewiesen, zusätzlich deklariertes USB-Modul ohne
Konfiguration abgewiesen. Dies ersetzt weder Imageprüfung noch Gastboot.

Der Launcher `scripts/qemu-with-secure-env.py` verbindet die kryptografischen
Kanäle hvc3/4/10/11 mit dem lokalen `secure_env`-Helper. Für die anderen
geerbten Funkkanäle gibt es keine Gegenstelle; insbesondere hvc9/UWB landet
an einem Null-Backend. Die Cuttlefish-Produktvererbung meldet trotzdem NFC,
Bluetooth, UWB und Thread als verfügbar.

Im bisherigen Lauf `mouse-1` wurden NFC-Abbrüche nach Controller-Timeouts
zwischen 01:46:05 und 01:46:53 UTC beobachtet. Bei der späteren Prüfung um
03:50 UTC meldete NFC `mState=turning on`; um 03:57 UTC lief seine Zustandsabfrage
in ein Timeout. Die nativen Funk-HAL-Prozesse hatten dabei unveränderte PIDs.
Das belegt ein NFC-Startproblem, keine zu diesem Zeitpunkt laufende Absturzschleife
aller HALs. Die QEMU-VM lief mit abgeschlossenem Android-Boot und SELinux Enforcing.

## Konfiguration des nächsten Images

- Bluetooth verwendet den vorhandenen Cuttlefish-Schalter
  `BOARD_HAVE_BLUETOOTH := false` vor der Produktvererbung.
- `permissions/unavailable-radios.xml` wird nach
  `/vendor/etc/permissions/aegis-unavailable-radios.xml` kopiert. Es nimmt NFC
  einschließlich `android.hardware.nfc.any` und Kartenemulationsfunktionen,
  UWB und Thread aus den verfügbaren Hardwarefunktionen heraus.
- AOSPs `SystemConfig` erlaubt diese Angaben in der Vendor-Partition und wendet
  sie **nach** dem Einlesen aller Partitionen und APEX-Dateien an. Die Reihenfolge
  der geerbten positiven Feature-Dateien hebt die Ausschlüsse daher nicht auf.
- NFCs Manifest bindet den persistenten Start an `nfc.any`; zusätzlich beendet
  `NfcApplication.onCreate()` seine Initialisierung ohne diese Funktion.
- SystemServer startet den UWB-Dienst nur mit UWB-Feature. Der
  Connectivity-Initializer erstellt den Thread-Dienst nur mit Thread-Feature.

Das entfernt die geerbten NFC-/UWB-/Thread-HAL-APEX-Pakete nicht aus dem Image.
Ihre nativen Dienste können weiterhin registriert sein. Nach dem Build sind
deren Zustand und Logs separat zu prüfen; diese Änderung bestätigt noch keine
vollständige Bereinigung aller virtuellen Gerätedienste. KeyMint, Gatekeeper,
OEMLock, verschlüsselte Benutzerdaten und Secure Element werden nicht verändert.

## Nachweis im neuen Gast

Der Android-Test `ProductConfigurationTest.unsupportedRadiosAreNotAdvertised`
prüft die tatsächliche PackageManager-Featureliste einschließlich geerbter
Unterfunktionen. Er besteht im vollständig gestarteten Image `25fde995`.
Zusätzlich sind in der lokalen QEMU-VM die Abwesenheit der Framework-Dienste
NFC/UWB/Thread, neue Crashmeldungen sowie die Stabilität der verbleibenden HALs
über ein Beobachtungsintervall zu prüfen. Ein erfolgreicher XML-Check oder eine
fehlende Feature-Angabe allein belegt keine Crashfreiheit.

## Geprüfte AOSP-Quellen

Alle folgenden Quellen stammen vom festgelegten Tag `android-16.0.0_r1`:

- [SystemConfig: Einlesen und abschließende Feature-Entfernung](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/SystemConfig.java)
- [NfcApplication: Startbedingung](https://android.googlesource.com/platform/packages/modules/Nfc/+/refs/tags/android-16.0.0_r1/NfcNci/src/com/android/nfc/NfcApplication.java)
- [NFC-Manifest: persistentWhenFeatureAvailable](https://android.googlesource.com/platform/packages/modules/Nfc/+/refs/tags/android-16.0.0_r1/NfcNci/AndroidManifest.xml)
- [SystemServer: UWB-Startbedingung](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/java/com/android/server/SystemServer.java)
- [ConnectivityServiceInitializer: Thread-Startbedingung](https://android.googlesource.com/platform/packages/modules/Connectivity/+/refs/tags/android-16.0.0_r1/service-t/src/com/android/server/ConnectivityServiceInitializer.java)
