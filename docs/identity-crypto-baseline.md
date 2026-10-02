# Passwort- und Verschlüsselungsgrundlage

Stand: 2. Oktober 2026. Der aktuelle Serverlauf verwendet das Image
`d1497661b70e301aec6c92190f2bb5ec58401caf`, AOSP `android-16.0.0_r1` und den
gepinnten Android-16-Kernel 6.12.18 mit dokumentierten AEGIS-Anpassungen.
Die AEGIS-Anbindung delegiert Authentifizierung und Schlüsselverwaltung an
AOSP. Sie implementiert keine eigene KDF, Passwortdatenbank oder CE-Schlüsselablage.

Die [Server-Teilabnahme](server-acceptance.md) belegt Anmeldung, Passwortwechsel,
CE-Fehlerwiederanlauf und gepaarte Persistenz bereits im Image `c526571`.
Die unten beschriebenen AOSP-Dateien sind im aktuellen Checkout gelesen und
mit SHA-256 dokumentiert; die aktuelle Gastkonfiguration ist separat beobachtet.
Die [vollständige Phase-1-Abnahme](phase-1-acceptance-progress.md) bleibt offen.

Ein lokaler [Plattformtest](identity-platform-test.md) bestätigt Passwortprüfung,
Passwortwechsel und CE-Sperre für einen persönlichen AOSP-Benutzer. Er prüft
den neuen AEGIS-Adapter nicht. Ein separater [Persistenztest](persistent-qemu.md#tatsächlicher-neustarttest)
bestätigt inzwischen den Erhalt passwortgeschützter Daten nach einem geordneten
vollständigen Neustart.

## Im lokalen Gast festgestellt

Im aktuellen Lauf `out/phase1-dod/d149766/boot-1` meldet Android
`ro.crypto.type=file`, `ro.crypto.state=encrypted` und
`ro.crypto.metadata.enabled=true`. Die aus dem installierten Image gelesene
`fstab.cf.f2fs.hctr2` enthält:

```text
fileencryption=aes-256-xts:aes-256-hctr2:inlinecrypt_optimized
keydirectory=/metadata/vold/metadata_encryption
```

Damit sind für die Dateiverschlüsselung AES-256-XTS für Inhalte und
AES-256-HCTR2 für Dateinamen konfiguriert. Der
[verwendete fscrypt-Kernelcode](https://android.googlesource.com/kernel/common/+/50eb8d5d443b43f38d6e72f005f1b8601ac88a05/fs/crypto/keysetup.c)
verwendet 64 Byte XTS-Schlüsselmaterial (zwei 256-Bit-Schlüssel) und 32 Byte für
HCTR2. Der aktuelle Bootlog bestätigt die Verwendung von AES-256-HCTR2 und
AES-256-XTS sowie den Metadatenverschlüsselungspfad beim Mounten von `/data`.
Das dokumentiert Konfiguration und konkrete Kernelbeobachtung; es ist keine
Einzelprüfung sämtlicher Inode-Policies oder aller Metadatenschutzpfade.

Der Beleg `out/phase1-dod/d149766/crypto-current-observation.json` enthält
Image-, Profil- und Bootbindung, Kernelzeilen, den Hash der gelesenen fstab und
Hashes der vier unten beschriebenen AOSP-Quelldateien. Die fstab hat SHA-256
`50ff9f6fa265b68b4e392e892e98f4eafe3574e656f92ecd8856b74c83a71677`.

Die bisherigen Gastprotokolle melden beim Anlegen der Testpasswörter ausdrücklich,
dass kein Weaver-Dienst vorhanden ist. Der verwendete LSKF-Pfad läuft über Gatekeeper und
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

Der Entwicklungs-TPM läuft als Software in einem zweiten QEMU-Gast auf
demselben Linux-Buildserver; frühere Mac-Läufe verwenden dasselbe Prinzip.
Er bietet keine vom kontrollierenden Host unabhängige Hardware-Vertrauensbasis.
Android ist ein userdebug-System mit Testschlüsseln; der direkte QEMU-Kernelstart
beweist keinen vertrauenswürdigen Bootloader oder produktionsreifen Secure Boot.

Android-Disk und TPM-Zustand müssen gemeinsam dauerhaft gespeichert werden.
Die Server-Teilabnahme belegt bereits geordnete gepaarte Neustarts, Isolation
bei gleichzeitig entsperrten Benutzern und bestätigten CE-Entzug nach dem
AEGIS-Logout. Der neue vollständige Versions-/Lebenszykluslauf bleibt separat
abzunehmen. Stromausfall und Migration sind bisher nicht geprüft; sie ersetzen
keinen der Pflichtfälle der Phase-1-DoD.
