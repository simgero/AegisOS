# Hardware des lokalen QEMU-Produkts

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
