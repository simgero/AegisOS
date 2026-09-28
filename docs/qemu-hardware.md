# Hardware des lokalen QEMU-Produkts

Stand 28. September 2026: Die Vollbuilds `336e9275` und `be1ad9ad` wurden
gebaut und über GitHub verifiziert. Beide lokalen Erststarts zeigten
Audio-Konfigurationsfehler; die zweite Korrektur ist vorbereitet, noch nicht
gebaut oder gebootet.

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

Die neue Korrektur übernimmt wieder Cuttlefishs vollständige Original-Audiopolicy
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
Unterfunktionen. Er ist kompiliert, aber noch nicht im neuen Gast ausgeführt.
Danach sind in der lokalen QEMU-VM zusätzlich die Abwesenheit der Framework-Dienste
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
