# Architekturentwurf 0001 — AegisOS Foundation

**Stand:** 2026-09-27 · **Status:** Entwurf zur Prüfung, keine Implementierungsbestätigung.

Die folgenden Festlegungen sind Projektentscheidungen. Verlinkte Quellen belegen Eigenschaften der verwendeten Plattformen, nicht die Sicherheit einer noch zu entwickelnden AegisOS-Implementierung.

## 1. Ziel und Umfang

Ein gemeinsamer Aegis-Systemkern soll verschiedene Geräteklassen unterstützen. ARM64 bleibt ein Ziel; erste Integrationstests dürfen auf x86_64 stattfinden. Gerätemodule, Firmware, Bootkonfiguration und Hardware-Abstraktionsschichten sind getrennte, versionsgebundene Bestandteile.

Phase 1 entwickelt ausschließlich Terminal-Funktionen: Plattformstatus, Benutzerverwaltung, Passwortanmeldung, Sitzungsende und nachweisbare Storage-Isolation. GNU/Linux-Runtime, Cloud, Desktop und eigene Apps folgen später.

## 2. Eine Identität, keine zweite Passwortverwaltung

AOSP ist auf dem Zielgerät die einzige maßgebliche lokale Instanz für Benutzer, Credentials und deren Lebenszyklus. Aegis stellt Bedienung und zusätzliche Regeln darüber bereit. Ein zukünftiger Cloud-Account ist keine Voraussetzung für eine lokale Anmeldung.

Android trennt Gerätebenutzer und App-Identitäten. Deshalb ist eine Android-`userId` weder eine gewöhnliche POSIX-UID noch ein frei verwendbarer Container-UID-Bereich. [AOSP: Multi-User](https://source.android.com/docs/devices/admin/multi-user)

Ein Aegis-Datensatz darf eine stabile Projekt-ID, die zugehörige Android-Identität und nichtgeheime Verwaltungsinformationen enthalten. Wiederverwendung einer Android-User-ID nach Löschung darf keine alten Daten oder Berechtigungen übernehmen. **Kein zweiter Passwort-Hash, kein eigenständiges Debian-Login und kein Export von Plattform-Masterschlüsseln.**

Die spätere GNU/Linux-Runtime bekommt einen technischen Ausführungskontext. Ein synthetischer Eintrag wie `user:x:1000:1000:...` für POSIX/NSS-Kompatibilität ist zulässig, aber kein separat verwaltetes Personenkonto und besitzt kein Login-Passwort.

## 3. Komponenten und Privilegien

```text
Terminal / aegis CLI
       |
       | authentifizierter, autorisierter lokaler IPC
       v
Aegis-Dienst: Sitzungen, Regeln, Zustandsautomat
       |
       v
versionsgebundener AOSP-Adapter
       |
       +-- Benutzer- und Sitzungsverwaltung
       +-- LockSettings / Plattformauthentifizierung
       +-- Storage-Lifecycle / vold
       +-- Keystore / KeyMint

später: separater Runtime-Manager für GNU/Linux-Kontexte
```

Der Client darf nicht allein durch Angabe einer Benutzer-ID fremde Daten entsperren. Jeder privilegierte Aufruf prüft die tatsächliche Aufruferidentität, die erlaubte Operation und den Zielbenutzer. Unter Android werden Binder-Identität, Berechtigungen und SELinux-Domänen berücksichtigt. Ein Linux-Testadapter muss seinen Teststatus sichtbar machen.

Der Dienst bietet **keine beliebigen Shell-, Mount-, Pfad- oder Namespace-Operationen** an. Pfade entstehen aus intern verwalteten Identitäten; Eingaben werden nicht in Shell-Kommandos interpoliert. Enrollment und Löschung benötigen administrative Autorisierung. Die Erstinitialisierung erhält einen gesondert zu prüfenden, lokalen Provisionierungsablauf.

SELinux bleibt im Integrations- und Abnahmetest enforcing. Neue Regeln werden minimal und begründet in das Image integriert. Android beschränkt auch privilegierte Prozesse durch SELinux; `root` allein ist daher kein Integrationskonzept. [AOSP: SELinux](https://source.android.com/docs/security/features/selinux)

**Implementierungsvorschlag, noch zu bestätigen:** speichersichere CLI und Zustandslogik in Rust; Android-Anbindung über die für das gewählte Release geeigneten Plattformsprachen und Binder-Schnittstellen. Keine selbst geschriebene Kryptobibliothek.

## 4. Passwort- und Schlüsselarchitektur

Die Plattformmechanismen werden verwendet, nicht durch ein eigenes `Argon2 → Master Key → FBE`-Schema ersetzt. AOSPs `LockSettingsService` verwaltet die Authentifizierung und den Synthetic-Password-Lebenszyklus. Je nach Gerät wirken Weaver beziehungsweise Gatekeeper und KeyMint mit. [Weaver](https://source.android.com/docs/security/features/authentication/weaver), [Gatekeeper](https://source.android.com/docs/security/features/authentication/gatekeeper)

Konzeptionell, **keine neu zu implementierende Kryptokette**:

```text
Benutzerpasswort
    -> AOSP-Authentifizierung und Schutz des Synthetic Password
    -> plattformverwaltete Freigabe des Benutzerkontexts
         +-- Credential-Encrypted Storage
         +-- gemäß ihrer Policy nutzbare Keystore-Schlüssel
```

Aegis erhält Erfolg, Fehler und begrenzte Fähigkeiten, keine Rohschlüssel aus der Plattformhierarchie. Ein einzelner selbst verwalteter Root-Key für alle Daten, Geräteidentitäten und Backups ist nicht vorgesehen.

### Kryptographische Leitplanken

| Bereich | Entscheidung für die Foundation |
|---|---|
| Private lokale Dateien | Bestehendes AOSP-FBE/fscrypt; AES-256-XTS als Inhaltsverschlüsselung für geeignete Zielhardware. Dateinamen- und Metadatenverschlüsselung gesondert prüfen. |
| Passwortprüfung | Plattformauthentifizierung einschließlich vorhandener hardwaregestützter Versuchslimits, keine parallele Passwortprüfung im Aegis-Daemon. |
| Zusätzliche Schlüssel | Plattform-Keystore; getrennte Zwecke und Berechtigungen. Hardwaregarantien nur nach tatsächlichem Nachweis. |
| Backup und E2E | Nicht implementieren. Später eigenes geprüftes Protokoll und voneinander getrennte Schlüssel, keine Wiederverwendung von Storage-Keys. |

FBE unterscheidet benutzerspezifischen Credential-Encrypted-Storage (CE) von Device-Encrypted-Storage (DE), der bereits vor der Benutzerentsperrung verfügbar ist. Private Dokumente, Konfigurationen, Caches und persistente Runtime-Schreibdaten gehören für Aegis in CE. [AOSP: FBE](https://source.android.com/docs/security/features/encryption/file-based)

AES-256 ist standardisiert; eine größere Zahl im Namen ist kein Auswahlkriterium. [NIST FIPS 197](https://csrc.nist.gov/pubs/fips/197/final) Argon2id kann für ein späteres, eigenständiges passwortgeschütztes Exportformat geprüft werden; RFC 9106 ist eine öffentlich dokumentierte CFRG-Empfehlung mit Status **Informational**, kein IETF-Standards-Track-Dokument. Das rechtfertigt keinen Umbau der AOSP-Anmeldung. [RFC 9106](https://www.rfc-editor.org/rfc/rfc9106.html)

Passwörter werden ausschließlich verdeckt über einen vertrauenswürdigen Eingabekanal erfasst; niemals als Prozessargument, Umgebungsvariable, Log oder Telemetrie. Auch im ersten nutzbaren Prototyp gibt es keinen passwortlosen Ersatzpfad. Fehlende Plattformfunktionen führen zu einem Fehler, nicht zu einer simulierten Entsperrung.

## 5. Sitzungsende ist nicht bloß Bildschirmsperre

Bei normaler Android-Bildschirmsperre kann zuvor entsperrter CE-Storage weiterhin verfügbar sein. Deshalb unterscheiden wir Anzeige-/Sitzungssperre und kryptographische Datensperre ausdrücklich. [Android: Direct Boot](https://developer.android.com/privacy-and-security/direct-boot)

Vorgeschlagener Zustandsautomat:

```text
LOCKED -> AUTHENTICATING -> UNLOCKED -> LOCKING -> LOCKED
              |                          |
              +-> LOCKED                 +-> LOCK_FAILED
```

`LOCKED` darf nur bei bestätigter Storage-Sperre gemeldet werden. Für einen vollständigen Logout: neue Zugriffe blockieren, betroffene Sitzungen und Prozesse beenden, Ressourcen schließen, Runtime-Mounts entfernen und den AOSP-Benutzer-/Storage-Lifecycle ausführen. Anschließend Zustand und Zugriffsverweigerung prüfen.

Offene Dateien können eine vollständige fscrypt-Schlüsselentfernung verhindern. Erfolgreicher Aufruf ist nicht automatisch vollständige Sperre. Aegis darf `vold` nicht durch unkoordinierte eigene fscrypt-Aufrufe umgehen. [Linux: Schlüsselentfernung](https://www.kernel.org/doc/html/latest/filesystems/fscrypt.html#removing-keys)

Welche Benutzerrollen beendet werden können, welche Schlüssel zurückgehalten werden und welche API dafür nötig ist, wird am gewählten Android-Image geprüft. Nicht unterstützte Fälle blockieren die Abnahme; ein stiller Rückfall auf reine Bildschirmsperre ist unzulässig. Es gibt kein Versprechen, sämtliche RAM-Kopien oder Hypervisor-Snapshots löschen zu können.

## 6. Spätere GNU/Linux-Runtime

Vorgesehen sind eine gemeinsame schreibgeschützte Basis und pro Benutzer getrennte Schreibbereiche für HOME, Konfiguration, Cache, `/tmp`, `/run` und Paketmanagerzustand. Systemweite Paketänderungen sind eine administrative, versionierte Updateoperation; normale Benutzer ändern die gemeinsame Basis nicht.

Gleiche interne UID 1000 in mehreren Kontexten ist nur mit getrennten User-Namespaces und disjunkten Host-UID/GID-Zuordnungen zulässig. Mount-, PID- und IPC-Namespaces allein bewirken diese UID-Abbildung nicht. Fehlt sichere Unterstützung im Zielkernel, werden alternativ unterschiedliche Host-Identitäten geprüft; andernfalls bleibt die Runtime deaktiviert. [Linux: User-Namespaces](https://man7.org/linux/man-pages/man7/user_namespaces.7.html)

Zusätzlich erforderlich: minimale Capabilities, seccomp, SELinux, Ressourcengrenzen und ein explizites Netzwerk-/Dateifreigabemodell. Kein Host-Root, kein Docker-Socket und kein pauschaler Zugriff auf `/data`. Ein `chroot` oder `HOME=...` gilt nicht als Sicherheitsgrenze.

Private POSIX-Home-Daten und ausgewählte, mit Android geteilte Dokumente werden getrennt geplant. Ein Vollzugriff auf `/data/media` darf Androids App-Berechtigungen nicht umgehen. Der genaue CE-Pfad wird im Plattformadapter festgelegt, nicht im Core hardcodiert.

Ein Kontext pro Person trennt noch nicht jede Linux-App dieser Person voneinander. Eine zusätzliche App-Sandbox ist eine eigene spätere Anforderung. Die gemeinsame Kernel-Vertrauensgrenze bleibt bestehen.

## 7. Minimaler Upstream-Eingriff

Beginn mit einem unveränderten, fest gepinnten **vollständigen Cuttlefish-Testimage plus passendem Host-Paket**. Ein GSI ist ein generischer Systembestandteil, kein allein bootfähiges Universal-VM-Paket. [Cuttlefish](https://source.android.com/docs/devices/cuttlefish/get-started), [GSI](https://source.android.com/docs/core/tests/vts/gsi)

AOSP-Source und Systemimage werden erst gebaut, wenn ein dokumentierter Integrationsbedarf dies verlangt: privilegierter Dienst, init-Konfiguration, Berechtigungen, SELinux, Framework-Anpassung oder Release-Signierung. Das ist ein Produktbuild, nicht zwangsläufig ein umfangreicher Fork.

Für Releases sind überprüfbare Boot-/Update-Ketten und zum Gerät passende Vertrauensanker erforderlich. Änderungen an einem signierten Image bewahren dessen bisherige Vertrauenskette nicht automatisch. [AOSP: Verified Boot](https://source.android.com/docs/security/features/verifiedboot)

Offene Entscheidungen und die nächsten ausführbaren Schritte stehen im [Phase-1-Plan](phase-1.md); die Grenzen im [Bedrohungsmodell](../../security/THREAT_MODEL.md).
