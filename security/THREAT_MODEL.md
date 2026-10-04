# Bedrohungsmodell — Foundation

**Status:** Phase-1-Bedrohungsmodell, aktualisiert 2026-10-04. Sicherheitsziele
sind Anforderungen; die [Server-Teilabnahme](../docs/server-acceptance.md)
belegt einzelne Abläufe. Die [vollständige DoD](../docs/architecture/phase-1-dod.md)
und der [laufende Abnahmeabgleich](../docs/phase-1-acceptance-progress.md)
bleiben maßgeblich. Keine vollständige Phase-1- oder Produktionsfreigabe.

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

## Zwölf verbindliche Phase-1-Szenarien

Die Nummern entsprechen Abschnitt 16 des Entwicklerauftrags. Jede Zeile nennt
die erwartete Wirkung und den notwendigen Nachweis; die Verweise auf T01–T17
sind Prüfpflichten, keine pauschalen Erfolgsbehauptungen. Für alle Szenarien
werden der vertrauenswürdige Host, der nicht kompromittierte AOSP-Kernel und
die tatsächlich aktive Sicherheitsrichtlinie vorausgesetzt.

| Nr. | Angriff / Ereignis | Erwarteter Schutz und Nachweis | Vertrauensgrenze / aktueller Belegumfang |
| --- | --- | --- | --- |
| 1 | Ausgeschaltetes Gerät oder Storage-Image gestohlen | Persönliche Daten bleiben durch AOSP-FBE geschützt; CE vor erster Anmeldung unlesbar, nach korrekter Anmeldung identisch (T07/T12). | Der Entwicklungs-Software-TPM bietet keine vom Host unabhängige Hardwarebindung. Ein gestohlenes vollständiges VM-/RAM-Abbild oder Offline-Passwortraten ist damit nicht pauschal abgewehrt. |
| 2 | Passwort von A wird zum Entsperren von B verwendet | AOSP prüft die Zielidentität; keine implizite CE-Freigabe oder Runtime für B (T01/T04). | Kein zweiter Passwortprüfer in GNU. Der aktuelle Lauf belegt zielbezogene Anmeldung; sämtliche Negativvarianten sind im Ergebnisindex zuzuordnen. |
| 3 | A greift auf Bs Dateien, Pakete, Prozesse, IPC oder Secrets zu | Nicht überlappende Host-UID/GID-Zuordnung, Namespaces, DAC und SELinux verweigern Lesen, Schreiben und Signale, auch bei zwei entsperrten Benutzern (T03/T06). | Bestehende Belege für Dateien, Signale und POSIX-Mqueues; die vollständige Ressourcenmatrix bleibt Pflicht. Apps derselben Person sind dadurch nicht untereinander isoliert. |
| 4 | GNU-Prozess versucht Kontextausbruch oder Hostprivilegien | Keine wirksamen Capabilities, NoNewPrivs/Seccomp, kontrollierte Geräte und Mounts, keine ungeprüften Hostdienste; fehlende Voraussetzungen verhindern Start (T04/T06). | Kernel/privilegierter Broker bleiben Teil der vertrauenswürdigen Basis. Keine Behauptung, sämtliche möglichen Kernelangriffe geprüft zu haben. |
| 5 | B ist abgemeldet, A bleibt aktiv | B-Prozesse und Ressourcen entfernt, CE-Schlüsselentzug von AOSP bestätigt; bekannte B-Dateien unlesbar, A arbeitet weiter (T10/T11). | Bereits in der Teilabnahme geprüft; Paketkonkurrenz ergänzt den vollständigen Nachweis. Runtime-Stopp allein beweist keinen Schlüsselentzug. |
| 6 | Vollständiger Neustart | Android und KeyMint-Zustand werden gemeinsam erhalten; Boot entsperrt keine persönliche Identität automatisch (T07/T12). | Gepaarter geordneter Neustart belegt. Stromausfall, Imagemigration und Snapshot-Rollback sind gesonderte Grenzen. |
| 7 | Passwort wird geändert | Neues Passwort entsperrt nach erneuter Sperre, altes nicht; Dateien bleiben erhalten (T02). | AOSP verwaltet den Protector. Frühere Hypervisor-Snapshots werden durch einen Passwortwechsel nicht rückwirkend ungültig. |
| 8 | Benutzer gelöscht, Nummer später wiederverwendet | Vollständige CLI-Löschung entzieht Schlüsselzugriff und privaten Zustand. Neue Seriennummer darf keine alten Zuordnungen öffnen (T12). | Nummern werden nur während der Systemserver-Lebensdauer stillgelegt; Wiederverwendung nach Reboot muss ausdrücklich geprüft werden. Vorhandene externe Backups bleiben außerhalb der Löschgarantie. |
| 9 | Wechsel oder Bildschirmsperre bei laufender Runtime | Persönliche Hintergrundprozesse dürfen gemäß AOSP weiterlaufen, bleiben dem bisherigen Benutzer zugeordnet. Terminalzugriff wird angemessen widerrufen; kein behaupteter CE-Entzug (T08/T09). | Wechsel und Logout sind verschieden. AOSP darf Hintergrundbenutzer aus Ressourcengründen stoppen; dann muss auch die Runtime abgebaut werden. |
| 10 | Logout, Speicher-Sperrung oder Paketoperation scheitert/konkurriert | Kein falscher Erfolg, keine neue unberechtigte Freigabe, geordneter Ressourcenabbau und erklärter Wiederanlauf (T10/T11/T17). | Echter CE-EBUSY-Wiederanlauf bereits belegt; vollständige Paket-/Lebenszykluskonkurrenz weiterhin offen. |
| 11 | Paketauftrag manipuliert Bereich, Adminfreigabe oder Eigentümer | Bereich explizit, frische AOSP-Adminprüfung und serverseitige Bindung an Antragsteller/Seriennummer/Plan; keine fremde CE-Freigabe (T13/T14). | Adminfreigabe für gemeinsame ausführbare Software ist eine Vertrauensentscheidung. Ein Admin kann künftiges Programmverhalten beeinflussen; daraus folgt kein direktes Leserecht auf fremdes CE. |
| 12 | Gemeinsames Update bei privaten Versionen oder parallelen Transaktionen | Exakte private Auswahl und passende Abhängigkeiten bleiben konsistent; kontrollierte Aktivierung oder sichtbarer Konflikt, kein Teilbestand als Erfolg (T15/T16/T17). | Gemeinsame Updateaktivierung mit erhaltener privater Auswahl ist in den früheren Läufen 209278de/f098f439 belegt. Das aktuelle Image b832d6c besitzt getrennte Nachweise für private Installation und Entfernung; seine Updateaktivierung und vollständige Konflikt-/Parallelitätsmatrix bleiben offen. Siehe die ausdrücklich nach Image getrennten Abschnitte im [Ergebnisindex](../docs/phase-1-result-index.md). |

## Ergänzende Szenarien und Grenzen

| Szenario | Geforderter Schutz / ausdrücklich gesetzte Grenze |
|---|---|
| Benutzer A greift auf B zu | Getrennte Storage-Bereiche und effektive Prozessberechtigungen; testen, während beide Benutzer aktiv sind und nachdem B ausgeloggt wurde. Kein Vertrauen auf Verzeichnisnamen. |
| Falsches Passwort | Keine Sitzung und keine Datenfreigabe; Plattform-Versuchslimits dürfen nicht umgangen werden. |
| Neustart | Private Testdaten vor erster erfolgreicher Anmeldung nicht lesbar. Ein Entschlüsseln für A darf B nicht implizit entsperren. |
| Logout oder Absturz während Logout | Alle abhängigen Ressourcen berücksichtigen. Bei unvollständiger Sperre `LOCK_FAILED`, keine Erfolgsmeldung. |
| Passwortänderung | Alter Zugang verliert seine Berechtigung im aktuellen System; Daten bleiben zugänglich. Unterbrochene Änderung testen. Ältere Snapshots sind ein gesondertes Rollback-Problem. |
| Benutzerlöschung | Sitzungen beenden, Schlüsselzugriff widerrufen und Zugriff durch eine spätere Wiederverwendung der Nummer ausschließen. Backups und Hypervisor-Snapshots werden dadurch nicht rückwirkend gelöscht. |
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

Hardwaremodell, Hardware-KeyMint/Weaver, unabhängige Sicherheitsprüfung und die vollständige Phase-1-Testmatrix bleiben offen. Der aktuelle Prototyp besitzt AOSP-KeyMint im Software-Helfer, UID-Mapping und geprüfte CE-Abmeldeabläufe; deren Belege und Grenzen stehen in der Server-Teilabnahme. Dieser Stand ist weiterhin keine Freigabe für schützenswerte Echtdaten.
