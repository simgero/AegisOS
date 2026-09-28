# Passwort- und Verschlüsselungsgrundlage

Stand 28. September 2026; historische Entwicklungsbasis `android-16.0.0_r1`,
Kernel `6.12.18-android16-1-g50eb8d5d443b-ab13257114-4k`.
Die AEGIS-Anbindung delegiert Authentifizierung und Schlüsselverwaltung an
diesen AOSP-Stand. Sie implementiert keine eigene KDF, Passwortdatenbank oder
CE-Schlüsselablage.

Ein lokaler [Plattformtest](identity-platform-test.md) bestätigt Passwortprüfung,
Passwortwechsel und CE-Sperre für einen persönlichen AOSP-Benutzer. Er prüft
weder Neustartpersistenz noch den neuen AEGIS-Adapter.

## Im lokalen Gast festgestellt

Im Lauf `out/qemu-first-boot/mouse-1` meldet Android `ro.crypto.type=file` und
`ro.crypto.state=encrypted`. Die verwendete `fstab.cf.f2fs.hctr2` enthält:

```text
fileencryption=aes-256-xts:aes-256-hctr2:inlinecrypt_optimized
keydirectory=/metadata/vold/metadata_encryption
```

Damit sind für die Dateiverschlüsselung AES-256-XTS für Inhalte und
AES-256-HCTR2 für Dateinamen konfiguriert. Der
[verwendete fscrypt-Kernelcode](https://android.googlesource.com/kernel/common/+/50eb8d5d443b43f38d6e72f005f1b8601ac88a05/fs/crypto/keysetup.c)
verwendet 64 Byte XTS-Schlüsselmaterial (zwei 256-Bit-Schlüssel) und 32 Byte für
HCTR2. Die Zeilen dokumentieren Konfiguration und Algorithmusdefinition; eine
separate Prüfung aller Datei-Policies oder der Metadatenverschlüsselung steht aus.

Das Gastprotokoll meldet beim Anlegen des Testpassworts ausdrücklich, dass kein
Weaver-Dienst vorhanden ist. Der verwendete LSKF-Pfad läuft über Gatekeeper und
den ursprünglichen Cuttlefish-KeyMint-/TPM-Helfer. Die Konfiguration wählt die
Remote-HALs, nicht die nonsecure-Ersatzmodule; siehe
[`secure-env-helper.md`](secure-env-helper.md).

## Gepinnte AOSP-Implementierung

[SyntheticPasswordManager](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/locksettings/SyntheticPasswordManager.java)
legt für neue Passwort-Protectors scrypt mit N=2048, r=8, p=2, einem zufälligen
16-Byte-Salt und 32 Byte Ausgabe fest. Das ist ein Bestandteil des gesamten
Gatekeeper-/Synthetic-Password-Verfahrens, keine eigenständige AEGIS-Passwortprüfung.
Das Synthetic Password ist zufallsbasiert und hat 256 Bit Sicherheitsstärke.
AOSP erzeugt Zufallswerte über `SecureRandomUtils`.

Version 3 leitet zweckgetrennte Unterschlüssel mit
[SP800Derive](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/locksettings/SP800Derive.java)
ab: SP800-108 mit HMAC-SHA-256, explizitem Zweck/Context und 256 Bit Ausgabe.
FBE, Keystore, Gatekeeper und Passwort-Historie verwenden unterschiedliche
Zweckbezeichner. AEGIS erhält daraus weder das Synthetic Password noch CE-Schlüssel.

[SyntheticPasswordCrypto](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/locksettings/SyntheticPasswordCrypto.java)
verwendet für geschützte Blobs AES-256-GCM mit 12-Byte-IV und 16-Byte-Tag sowie
AOSPs Keystore-Protector. Die Ableitung/Blob-Verarbeitung verbleibt vollständig
in AOSP. Ein Passwortwechsel verändert den zugehörigen Schutzpfad; persönliche
Dateien werden nicht durch eine AEGIS-eigene Verschlüsselung ersetzt.

## Grenzen und noch fehlende Nachweise

Der Entwicklungs-TPM läuft als Software auf demselben Mac in einem zweiten
QEMU-Gast. Er bietet keine vom Mac-Eigentümer unabhängige Hardware-Vertrauensbasis.
Android ist ein userdebug-System mit Testschlüsseln; der direkte QEMU-Kernelstart
beweist keinen vertrauenswürdigen Bootloader oder produktionsreifen Secure Boot.

Android-Disk und TPM-Zustand müssen gemeinsam dauerhaft gespeichert werden.
Der aktuelle Testlauf ist flüchtig. Die vorbereitete Persistenz benötigt noch
den neuen Helper-Build und einen echten Neustarttest. Isolation bei gleichzeitig
entsperrten persönlichen Benutzern, Prozess-/Mount-Abbau beim AEGIS-Logout und
die gesamte AEGIS-CLI bleiben separat nachzuweisen.
