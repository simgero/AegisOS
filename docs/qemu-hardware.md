# Hardware des lokalen QEMU-Produkts

Stand 28. September 2026: Produktänderung in Vollbuild `336e9275` erfolgreich
gebaut und über GitHub verifiziert. Der lokale Erststart hängt jedoch an einer
inkonsistenten Audio-Konfiguration. Der zur Ansicht geöffnete ältere Gast
enthält diese Änderung nicht.

## Gefundener Startfehler und Korrektur

Im neuen Gast fehlt `/vendor/etc/bluetooth_with_le_audio_policy_configuration_7_0.xml`
wegen `BOARD_HAVE_BLUETOOTH=false`; Cuttlefishs übernommene
`audio_policy_configuration.xml` enthält weiterhin ein XInclude auf diese Datei.
Der AIDL-Audio-HAL registriert `IConfig/default`, aber kein `IModule/default`.
SystemServer bleibt in `StartAudioService`; `sys.boot_completed` bleibt leer.
Kernel und authentifiziertes ADB funktionieren mit SELinux Enforcing. Logs:
`out/full-build-336e9275/boot-1`, Hashes in dessen `../progress.json`.

Die Korrektur nutzt `LOCAL_AUDIO_PRODUCT_COPY_FILES` vor der Produktvererbung.
Die eigene Audiopolicy lässt ausschließlich den Bluetooth-Include weg; primäre
Audio-/Submix-Konfiguration, Effekte und Lautstärketabellen bleiben erhalten.
Ein Neubau mit anschließendem QEMU-Boot muss die Korrektur noch bestätigen.
Der Fehler wird nicht durch Abschalten von Audioserver, SELinux oder AVB umgangen.

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
