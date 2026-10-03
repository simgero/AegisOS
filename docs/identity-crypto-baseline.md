# Passwort- und Verschlüsselungsgrundlage

Stand: 3. Oktober 2026. Der aktuelle Korrekturlauf verwendet das Image
`f098f439f051c34e92fb1be0b4d908cc542358ea`, AOSP `android-16.0.0_r1` und den
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

## Quellbindung und Gastzuordnung auf f098f43

Für das lokal gebaute Korrekturimage `f098f43` sind acht relevante Quelldateien
gegen die tatsächlich gespeicherten Image-/Runtime-Buildbelege geprüft.
`out/phase1-dod/f098f439/identity-source-binding.json` hat SHA-256
`e5bcd26a334d45c6043d61a8758636156a38f7e27c093f7807db0a93ac8b168f`.
Die sechs Identitäts-/Runtime-Dateien stimmen mit dem Image-Buildmanifest
überein; die beiden Basisgenerator-Dateien sind bytegleich zum festgehaltenen
Runtime-Buildercommit `178cbb6e`.

Die gelesene Implementierung ordnet die Verantwortlichkeiten wie folgt zu:

- `AospIdentityBackend` legt persönliche Benutzer über AOSPs `UserManager`
  an und prüft die Anmeldung über `ILockSettings`. Die aufgelöste Identität
  enthält Benutzer-ID und Seriennummer; ein ersetzter oder gelöschter Benutzer
  wird bei erneuter Prüfung abgewiesen. Namen dienen der Auswahl, nicht der
  Speicherzuordnung.
- `RuntimeUidMap` bildet diese AOSP-Identität auf reservierte Hostbereiche ab.
  Sein Speicherschlüssel enthält ID und Seriennummer. Die interne normale
  GNU-Identität bleibt UID/GID 1000.
- Der Basisgenerator ergänzt ausschließlich das gemeinsame technische Konto
  `runtime` mit HOME `/home/user` und gesperrtem lokalen Anmeldeeintrag.
  Die Namensauflösung verwendet lokale technische Kontometadaten. Es entstehen
  dadurch keine nach persönlichen AOSP-Benutzern benannten Linux-Konten.
- Der native Shellstart wechselt vor der Programmausführung zu UID/GID 1000,
  entfernt die verbleibenden Capabilities und setzt das feste technische HOME.
  Er führt keine weitere Linux-Anmeldung aus.
- Die Paketvalidierung verlangt weiterhin das feste technische Konto,
  zulässige ID-Bereiche, gesperrte lokale Anmeldeeinträge und konsistente
  Kontometadaten. Diese Prüfung ist Bestandteil der Kandidatenvalidierung.

Dieser Beleg bestätigt Quellbindung und die gelesenen Zuständigkeiten.
Die zusätzliche Gastzuordnung ist inzwischen für Alpha 10/10 auf Werksbasis
und privatem jq-u4-Stand sowie Beta 11/11 auf gemeinsamem jq-u3-Stand erfasst.
Alle drei gewöhnlichen GNU-Abfragen liefern denselben technischen POSIX-Eintrag
`runtime:x:1000:1000:AEGIS runtime:/home/user:/bin/bash` und bytegleiche
öffentliche Dateien `/etc/passwd`, `/etc/group` und `/etc/nsswitch.conf`.
Persönliche Anmeldung und Speicherfreigabe erfolgten zuvor durch AEGIS/AOSP;
eine zusätzliche Linux-Anmeldung findet nicht statt.

Beleg: `out/phase1-dod/f098f439/two-user-technical-identity-proof.json`, SHA-256
`5843f9d6fe3c4e5e937d010b8d65b727be2e56ca1309d1920b4b00ce71ff71b8`.
Er bindet die tatsächlichen GNU-Ereignisse an die oben geprüften Quellen.
Es wurden nur öffentliche technische Kontometadaten gelesen, keine
Shadow-Einträge, Passwörter oder Schlüssel. T01.5 zur umfassenden
Offenlegungsprüfung bleibt davon getrennt und weiterhin offen.

## Beobachtete Konfiguration auf 209278d

Der erste Boot des neuen Profils `2366ca04-d587-4170-8c56-a63c8a8e1774`
mit Boot-ID `44dbff5f-3a76-4e97-9334-beb437fcd461` bestätigt FBE, den Zustand
`encrypted` und aktivierte Metadatenverschlüsselung. Die installierte fstab
ist bytegleich zum unten dokumentierten Stand. Der Kernel protokolliert
AES-256-HCTR2 und AES-256-XTS; SELinux ist Enforcing. Vor der Anlage persönlicher
Benutzer ist nur CE `[0]` entsperrt. Die vier unten genannten AOSP-Quelldateien
wurden erneut gehasht und sind unverändert.

Aktueller Beleg: `out/phase1-dod/209278de/crypto-current-observation.json`, SHA-256
`4e35d69d8b48a5376766c80b12a3b408ef65203be12d66121ef356e3021b1479`.
Der darin gebundene Quellvergleich `crypto-source-continuity.json` hat SHA-256
`4ff1e1ab6c675492ead7589964c0a175385fc8619dba27e648188c3ee53e88af`.
Dies bestätigt Konfiguration und Quellkontinuität; die persönliche Anmeldung,
einzelne CE-Policies und gepaarte Neustarts des neuen Profils sind damit noch
nicht abgenommen.

## Vorheriger Wiederholungsstart auf 3155115

Im früheren Wiederholungsstart `out/phase1-dod/3155115/boot-2` meldet Android
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
HCTR2. Der zweite Start bestätigt dieselben beiden fscrypt-Modi; die oben genannten
Properties bestätigen weiterhin FBE und aktivierte Metadatenverschlüsselung.
Das dokumentiert Konfiguration und konkrete Kernelbeobachtung; es ist keine
Einzelprüfung sämtlicher Inode-Policies oder aller Metadatenschutzpfade.

Der Beleg `out/phase1-dod/3155115/crypto-current-observation.json`, SHA-256
`eca6d359a1d92882fbecfc75960718dbed9ae0c9b10c04b7abaa0307f64787d7`, enthält
Image-, Profil- und Bootbindung, Kernelzeilen und den Hash der gelesenen fstab.
Die fstab bleibt bytegleich mit SHA-256
`50ff9f6fa265b68b4e392e892e98f4eafe3574e656f92ecd8856b74c83a71677`.
Der darin gebundene Beleg `crypto-source-continuity.json` bestätigt die
unveränderten Prüfsummen der vier unten beschriebenen AOSP-Quelldateien.
`boot2-before-login.json`, SHA-256
`4158d4313255f54b2fd09bb69e705110dd6f8d58a3eac1998f6440a68e6ab854`,
bindet die genannten Properties an denselben Boot. Vor der persönlichen
Anmeldung ist ausschließlich CE des Systembenutzers 0 entsperrt.

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
