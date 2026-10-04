# Passwort- und Verschlüsselungsgrundlage

Stand: 4. Oktober 2026. Der unten dokumentierte frühere Korrekturlauf verwendet das Image
`f098f439f051c34e92fb1be0b4d908cc542358ea`, AOSP `android-16.0.0_r1` und den
gepinnten Android-16-Kernel 6.12.18 mit dokumentierten AEGIS-Anpassungen.
Die AEGIS-Anbindung delegiert Authentifizierung und Schlüsselverwaltung an
AOSP. Sie implementiert keine eigene KDF, Passwortdatenbank oder CE-Schlüsselablage.

Der aktuelle Abnahmelauf verwendet inzwischen
`b832d6c077baeee4324e00d00dc3618372f3e9d9`; seine Teilnachweise stehen im
[Ergebnisindex](phase-1-result-index.md). Die folgenden Quell- und
Konfigurationsbelege behalten ihre ausdrücklich angegebenen Imagebindungen.
Ihre vollständige Zuordnung zum aktuellen Image bleibt vor D7-Abschluss zu
prüfen; frühere Beobachtungen werden nicht automatisch als aktuelle übernommen.

Die [Server-Teilabnahme](server-acceptance.md) belegt Anmeldung, Passwortwechsel,
CE-Fehlerwiederanlauf und gepaarte Persistenz bereits im Image `c526571`.
Die unten beschriebenen AOSP-Dateien sind im jeweils bezeichneten Checkout
gelesen und mit SHA-256 dokumentiert; die zugehörige Gastkonfiguration ist
separat beobachtet.
Die [vollständige Phase-1-Abnahme](phase-1-acceptance-progress.md) bleibt offen.

Ein lokaler [Plattformtest](identity-platform-test.md) bestätigt Passwortprüfung,
Passwortwechsel und CE-Sperre für einen persönlichen AOSP-Benutzer. Er prüft
den neuen AEGIS-Adapter nicht. Ein separater [Persistenztest](persistent-qemu.md#tatsächlicher-neustarttest)
bestätigt inzwischen den Erhalt passwortgeschützter Daten nach einem geordneten
vollständigen Neustart.

## Aktuelle Quellbindung und Konfigurationsbeobachtung auf b832d6c

Die vier unten beschriebenen AOSP-Dateien `SP800Derive`, `SecureRandomUtils`,
`SyntheticPasswordCrypto` und `SyntheticPasswordManager` sind nun gegen den
gespeicherten Build von `b832d6c` geprüft. Das Buildmanifest pinnt
`frameworks/base` auf `99b01a65cc4c104933788b3143285ab6bae65827`.
Die ersten beiden Dateien stimmen unmittelbar mit dieser Revision überein;
bei den beiden vorbereiteten Synthetic-Password-Dateien stimmen sowohl die
Upstream-Eingangsprüfsummen als auch die gespeicherten Build-Ausgangsprüfsummen.
Alle vier Dateien sind bytegleich zum nachfolgend dokumentierten f098f43-Stand.

`out/phase1-dod/b832d6c-base/crypto-source-continuity.json`, SHA-256
`ebdd238a56a9189ea3c02bedd5610f372c65871ee5e81024d0b8fb74f9ac8774`,
bindet diese Vergleiche an das gepinnte Manifest, das vorbereitete
Quellmanifest, den erfolgreich verifizierten Build und den AVB-Beleg des
aktuellen vorbereiteten Images. Das bestätigt die Kontinuität der beschriebenen
kryptographischen Implementierungen. Es ist ein statischer Teilnachweis ohne
Gastaktion; Passwörter oder Schlüssel wurden nicht gelesen. Historische
Gastkonfigurationen, Passwort-, Lösch- oder Neustarttests werden dadurch nicht
zu aktuellen Integrationsnachweisen. Weitere Identitätsdateien und dynamische
Prüfungen benötigen ihre eigene aktuelle Zuordnung.

Die separate Leseaufnahme vom 4. Oktober um 07:27 UTC bestätigt im laufenden
b832d6c-Profil FBE (`file`), `encrypted`, aktivierte Metadatenverschlüsselung,
authentifiziertes ADB und SELinux `Enforcing`. Der beobachtete AVB-Digest stimmt
mit dem vorbereiteten Image überein. Die installierte fstab hat weiterhin
SHA-256 `50ff9f6fa265b68b4e392e892e98f4eafe3574e656f92ecd8856b74c83a71677`;
die eingefrorenen Bootmeldungen nennen AES-256-HCTR2 und AES-256-XTS.
Beleg: `out/phase1-dod/b832d6c-base/boot3-service-observation.json`, SHA-256
`4c2b2aa33673f659ba56645ec629f07ded54ab7f7624919f08d7433b2ba0105b`.
Die Aufnahme bindet Profil, Boot-ID und ursprüngliche SystemServer-Identität.
Sie liest öffentliche Konfiguration und vorhandene Logs, keine Schlüssel.
Einzelne Inode-Policies und sämtliche Schutzpfade sind damit weiterhin nicht
vollständig auditiert; persönliche Daten- und Lebenszyklustests bleiben separat.

Für die neun unten im Abschnitt zum Passworttransport benannten CLI-, JNI-,
AIDL-, Dienst- und Testdateien ist die Quellbindung ebenfalls erneuert:
`out/phase1-dod/b832d6c-base/credential-transport-source-binding.json`, SHA-256
`ee6a2ff03fa62c02aaccff11ce3cb04b4693eee40e7afe435ccdfc77e56d0637`.
Jede Datei stimmt bytegleich mit dem aktuellen Imagecommit, dessen gespeichertem
Identitäts-Buildmanifest und dem früheren statischen Review überein. Damit sind
die beschriebenen Terminal-, Puffer- und AIDL-Eigenschaften für dieselben neun
Quellen an b832d6c gebunden. Dies ist keine neue dynamische Offenlegungsprüfung
und keine neue Ausführung von `CredentialTransportTest`. Generierte Proxys,
tatsächliche Argumente, History, Dateien, Logs und relevante Fehlerpfade bleiben
vor dem vollständigen Abschluss von T01.5 gesondert nachzuweisen.

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

### Ergänzende Quellprüfung des Passworttransports

Neun CLI-, JNI-, AIDL-, Dienst- und Testdateien sind zusätzlich bytegleich zum
Imagecommit `f098f43` und zum gespeicherten Identitäts-Buildmanifest geprüft.
Beleg: `out/phase1-dod/f098f439/credential-transport-source-binding.json`,
SHA-256 `88e84b5de6d37cd48f6b73683f77800d157c8a2ff279084daa823855e793037d`.

Die CLI erwartet bei Anmeldung/Wechsel nur die Zielidentität als Argument;
das Passwort liest sie anschließend separat aus dem interaktiven Terminal.
Der native Passwortmodus deaktiviert `ECHO` und `ECHONL`. Die Implementierung
versucht die Terminalwiederherstellung bei Abschluss sowie in ihren Signal-
und Exitpfaden. Console und CLI löschen ihre verwendeten Byte-/Zeichenpuffer;
der gelesene Kommandoeingabepfad enthält keine persistente History.

Die persönlichen und Paket-AIDL-Schnittstellen sind als `SensitiveData`
deklariert. Der zusätzliche LockSettings-Transport markiert Parcels als sensibel
und setzt `FLAG_CLEAR_BUF`; dadurch bleibt auch beim dienstinternen Aufruf
die Kopier-/Besitzsemantik der AIDL-Übergabe erhalten. Die gelesenen Dienstpfade
schließen `LockscreenCredential` und löschen empfangene Passwortarrays in
Aufräumblöcken. Die betrachteten CLI-Fehlerausgaben verwenden feste Meldungen
und geben keine Passwortwerte oder unerwarteten Argumente aus.

Das ist ein an den Build gebundener **statischer Teilnachweis**. Tatsächliche
Argument-, History-, Datei-/Logbeobachtungen, generierte AIDL-Proxys und relevante
Laufzeitfehlerpfade bleiben gesondert zu prüfen. Für den mitgelesenen
`CredentialTransportTest` wird keine neue Ausführung behauptet. Explizite
Löschaufrufe beweisen außerdem nicht, dass zu keinem Zeitpunkt weitere Kopien
im verwalteten Speicher existierten. T01.5 bleibt offen.

## Beobachtete Konfiguration auf f098f43

Der zweite Boot desselben f098f43-Profils bestätigt erneut FBE, den Zustand
`encrypted` und aktivierte Metadatenverschlüsselung. Die installierte
`/vendor/etc/fstab.cf.f2fs.hctr2` hat weiterhin SHA-256
`50ff9f6fa265b68b4e392e892e98f4eafe3574e656f92ecd8856b74c83a71677`;
die Kernelbeobachtung enthält AES-256-HCTR2 und AES-256-XTS.
Beleg: `out/phase1-dod/f098f439/crypto-current-observation.json`, SHA-256
`e154e92235a2d07baf6c6f60d9fbe87eb5156dce5e31c93ae9f457f8da296c6c`.

Die vier unten beschriebenen Quelldateien sind seit der früheren Aufnahme
bytegleich. Ihre genaue Herkunft ist jetzt zusätzlich gegen das gespeicherte
Buildmanifest geprüft: `SP800Derive` und `SecureRandomUtils` entsprechen direkt
der gepinnten AOSP-Revision. `SyntheticPasswordCrypto` und
`SyntheticPasswordManager` enthalten bereits vorhandene AEGIS-Erweiterungen
für bestätigte Protector-/Benutzerlöschung. Bei diesen beiden stimmen sowohl
die Upstream-Eingangsprüfsummen als auch die vorbereiteten Ausgangsprüfsummen
mit `runtime-storage-source.json` des tatsächlichen Builds überein.

Ein erster Vergleich, der für alle vier Dateien vollständige Upstream-Gleichheit
erwartete, scheiterte an diesen beiden Ergänzungen. Er bleibt in
`crypto-source-pin-comparison.json` erhalten und ist im aktuellen Beleg verlinkt.
Die beschriebenen kryptographischen Verfahren werden dadurch nicht als eigene
AEGIS-KDF oder zweite Passwortverwaltung ausgegeben. Konfigurationsbeobachtung
und Quellbindung ersetzen keine vollständige Einzelprüfung jeder CE-Inode-Policy
oder aller Metadatenschutzpfade. Es wurden keine Schlüssel gelesen.

Der tatsächliche gepaarte Neustart mit zunächst gesperrtem persönlichem CE,
anschließend frischen Anmeldungen und bytegleichen Originaldaten ist separat in
`paired-reboot-readback-proof.json` belegt, SHA-256
`aad3626470a13b33ceb4bcac84725d30c2cdc288ae769d921fa9d50b95773683`.

## Frühere beobachtete Konfiguration auf 209278d

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
