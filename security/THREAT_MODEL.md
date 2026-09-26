# Bedrohungsmodell — Foundation

**Status:** Entwurf, 2026-09-27. Sicherheitsziele sind Anforderungen, keine bereits erreichten Eigenschaften.

## Schutzgüter

Benutzerdokumente, Konfigurationen, temporäre private Daten, Anmeldeinformationen, Schlüssel und Integrität von System und Updates. Verfügbarkeit, Metadatenminimierung und reproduzierbare Wiederherstellung werden gesondert betrachtet.

## Vertrauensgrenzen

```text
nicht vertrauenswürdige App / CLI-Aufruf
       | Aufruferidentität und Autorisierung
Aegis-Systemdienst / AOSP-Plattformadapter
       | Plattform- und Kernel-Schnittstellen
AOSP-Systemdienste / Kernel / Hardware-Sicherheitskomponenten
       | im Entwicklungsbetrieb zusätzlich
Hypervisor / Hostadministrator / Build-Infrastruktur
```

Ein CLI-Parameter behauptet eine Identität, beweist sie aber nicht. App-Zugriff benötigt eine zusätzliche Autorisierungsprüfung auch dann, wenn ein Benutzer bereits entsperrt ist.

Der Entwicklungs-VM-Host ist vertrauenswürdig vorausgesetzt: Wer ihn kontrolliert, kann grundsätzlich die Testumgebung manipulieren. Eine VM ist kein Beleg für Schutz durch ein echtes Secure Element. Keine Produktivschlüssel und keine echten privaten Dokumente in Entwicklungsimages oder CI verwenden.

## Szenarien und Abnahmebedingungen

| Szenario | Geforderter Schutz / ausdrücklich gesetzte Grenze |
|---|---|
| Benutzer A greift auf B zu | Getrennte Storage-Bereiche und effektive Prozessberechtigungen; testen, während beide Benutzer aktiv sind und nachdem B ausgeloggt wurde. Kein Vertrauen auf Verzeichnisnamen. |
| Falsches Passwort | Keine Sitzung und keine Datenfreigabe; Plattform-Versuchslimits dürfen nicht umgangen werden. |
| Neustart | Private Testdaten vor erster erfolgreicher Anmeldung nicht lesbar. Ein Entschlüsseln für A darf B nicht implizit entsperren. |
| Logout oder Absturz während Logout | Alle abhängigen Ressourcen berücksichtigen. Bei unvollständiger Sperre `LOCK_FAILED`, keine Erfolgsmeldung. |
| Passwortänderung | Alter Zugang verliert seine Berechtigung im aktuellen System; Daten bleiben zugänglich. Unterbrochene Änderung testen. Ältere Snapshots sind ein gesondertes Rollback-Problem. |
| Benutzerlöschung | Sitzungen beenden, Schlüsselzugriff widerrufen, ID-Wiederverwendung verhindern. Backups und Hypervisor-Snapshots werden dadurch nicht rückwirkend gelöscht. |
| Gestohlener Datenträger | Schutz gesperrter Nutzdaten durch die Plattformverschlüsselung; Hardwarebindung und Schutz gegen Offline-Raten erst auf einer konkreten Plattform bewerten. |
| Manipuliertes Boot-/Systemimage | Für Releases verifizierte Bootkette, kontrollierte Signierung und Rollback-Regeln. Ein entsperrtes Entwicklungsimage erfüllt dies nicht. |
| Bösartige Linux-App | Darf den Benutzerkontext nicht verlassen. Apps innerhalb desselben Linux-Kontexts sind ohne zusätzliche App-Sandbox nicht voneinander isoliert. |
| Kompromittierter Kernel oder Hostadministrator | Kein genereller Schutz entschlüsselter Laufzeitdaten zugesagt. Schutz gesperrter Schlüssel ist hardware- und angriffsabhängig, keine pauschale Root-Resistenz. |
| Manipulierter Download / Build-Abhängigkeit | Gepinnte Versionen, überprüfte Bezugsquelle, Prüfsummen und Signaturen soweit verfügbar; keine Release-Schlüssel im gewöhnlichen Buildjob. |

Grundlagen: [AOSP-Authentifizierung](https://source.android.com/docs/security/features/authentication), [Verified Boot](https://source.android.com/docs/security/features/verifiedboot), [fscrypt-Bedrohungsmodell](https://www.kernel.org/doc/html/latest/filesystems/fscrypt.html#threat-model).

## Verschlüsselung ist nicht gleich Zugriffskontrolle oder Integrität

FBE/fscrypt schützt gespeicherte Inhalte; entsperrte Dateien brauchen weiterhin wirksame Zugriffsregeln. Dateisystemverschlüsselung bietet nicht automatisch Authentizität sämtlicher veränderlicher Dateien und Metadaten. Ein späteres Backup-/Synchronisationsformat benötigt eine eigene, authentifizierte Konstruktion. [Linux: fscrypt](https://www.kernel.org/doc/html/latest/filesystems/fscrypt.html)

Geheimnisse dürfen nicht in Logs, Argumentlisten, Crashdumps oder ungeschütztem Swap landen. Flüchtige Verzeichnisse in RAM allein lösen das Swap-Problem nicht. Für die Zielplattform wird eine dokumentierte Swap-/Dump-Policy verlangt; VM-Snapshots mit RAM sind im Testbetrieb als sensibel zu behandeln.

## Nicht akzeptierte Abkürzungen

- Keine global permissive SELinux-Konfiguration als Lösung.
- Keine zweite Aegis-Passwortdatenbank als Ersatz für AOSP.
- Kein `chmod 700`, `chroot`, Namespace oder `HOME`-Wechsel als behauptete Verschlüsselung.
- Kein Erfolg durch bloßes Umschalten eines internen `unlocked=true`.
- Kein Produktionsbetrieb mit Testschlüsseln, `adb root` oder unkontrollierter Release-Signierung.

## Nachweis statt Behauptung

Zu jedem Sicherheitsziel gehören positive und negative Tests, ein konkreter Image-/Kernelstand sowie klar benannte Testprivilegien. Tests gegen Mocks belegen ausschließlich Ablauflogik. Die vorhandene Repository-CI ist **kein Sicherheitsnachweis**.

Vorerst offen: Hardwaremodell, echte KeyMint-/Weaver-Implementierung, Verhalten der einzelnen Android-Benutzertypen beim Stoppen, sichere CE-Schlüsselentfernung, späteres UID-Mapping und unabhängige Sicherheitsprüfung. Bis zur Klärung keine Freigabe für schützenswerte Echtdaten.
