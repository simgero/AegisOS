# Phase 1 – Ergebnisindex der Referenzläufe

Stand: 3. Oktober 2026. **Unvollständig; keine Gesamtfreigabe.**
Verbindlich bleiben sämtliche Anforderungen der [DoD](architecture/phase-1-dod.md)
und des [Entwicklerauftrags](architecture/phase-1-developer-brief.md).
Dieser Index ordnet einzelne Varianten zu. Ein bestandener Teilfall schließt
weder eine ganze T-Zeile noch ein D-Kriterium. Historische Ergebnisse stehen im
[Arbeitsprotokoll](phase-1-acceptance-progress.md); sie gelten hier nicht
automatisch für das aktuelle Image.

## Bisheriger Referenzlauf und Statusregeln

- Produkt-/Image-Commit: `209278def7d5bc5612eeb397bdd8ee20ccb16d86`.
- Profil: `2366ca04-d587-4170-8c56-a63c8a8e1774`.
- Erster Boot: `44dbff5f-3a76-4e97-9334-beb437fcd461`.
- Zweiter Boot: `2816650c-96bf-4db0-84e6-f169f9dfada9`.
- Dritter Boot: `e720d2bf-faef-4af8-87b9-709112eb3c41`.
- Alpha: AOSP-Benutzer/Seriennummer `10/10`, Administrator.
- Beta: AOSP-Benutzer/Seriennummer `11/11`, normaler Benutzer.
- Alle unten bezeichneten Belege liegen lokal unter
  `out/phase1-dod/209278de/`. Die vollständigen Logs liegen zusätzlich unter
  `/srv/aegis/runs/phase1-209278de/`; Belege binden deren Pfade und Prüfsummen.

**Bestanden** bezieht sich nur auf die ausdrücklich beschriebene Variante.
**Teilbelegt** benennt eine konkrete verbleibende Lücke. **Offen** bedeutet,
dass der nötige aktuelle Nachweis noch nicht vollständig erhoben/zugeordnet ist.
Komponententests ersetzen keine geforderten Benutzerabläufe. D1–D7 und alle
T-Gesamtzeilen bleiben bis zum vollständigen Audit offen.
Die folgende T01–T17-Tabelle bezieht sich auf den bisherigen Referenzlauf
`209278de`. Die zusätzlichen Ergebnisse des Korrekturimages `f098f43` stehen
im gesonderten Abschnitt unten; dort sind auch die inzwischen ausgeführten
persönlichen CLI-, Isolations- und Neustartvarianten aufgeführt.

## Pflichtvarianten T01–T17

| Variante | Erwartete Wirkung | Tatsächliches Ergebnis / Status | Beleg |
| --- | --- | --- | --- |
| T01.1 Anlage und Auflistung | Zwei unterschiedliche persönliche AOSP-Identitäten, A Admin, B normal | Bestanden: tatsächliche Anlage und CLI-Auflistung mit Alpha 10/10 als Admin und Beta 11/11 als normalem Benutzer | E01, E02, E19 |
| T01.2 Erster korrekter Zugang A/B | Jeweils erster richtiger Login ohne Aufwärmversuch; nur Ziel-CE neu entsperrt | Bestanden vor Reboot; Vorbereitung und echte GNU-Ausführung erfasst | E01, E02 |
| T01.3 Falsches Passwort | Keine Entsperrung, keine Sitzung, kein Runtime-Start | Bestanden für Betas nach dem Passwortwechsel ungültiges bisheriges Passwort: AOSP verweigert, CE bleibt gesperrt, Kontext fehlt, Runtime-Start abgelehnt | E23 |
| T01.4 Identitätsquelle | Keine parallelen persönlichen Linux-Konten/Passwortspeicher | Offen: vollständige Quell-/Gastzuordnung erforderlich | — |
| T01.5 Passworttransport | Keine Offenlegung in Transport, Argumenten, History, Dateien und Logs | Offen: begrenzte Quellkontinuität allein genügt nicht | — |
| T02.1 Passwortwechsel und erneute Sperre | Neues Passwort erlaubt Zugriff, altes scheitert; Originaldaten erhalten | Bestanden Beta: AOSP-Passwortwechsel, regulärer Logout, altes Passwort verweigert, neues bestätigt; ursprüngliche Datei/Konfiguration und Inode-Metadaten unverändert | E23 |
| T02.2 Passwortwechsel über Reboot | Neues Passwort nach Reboot gültig, altes ungültig; keine vollständige Neuverschlüsselung | Bestanden Beta: gleiches Profilpaar geordnet gestoppt/gestartet, erster neuer Passwortzugang gültig, anschließend altes Passwort verweigert; Originalbytes und Inode-Metadaten erhalten. Quellprüfung bestätigt Weiterverwendung des Synthetic Password | E23, E24 |
| T03.1 Interne/äußere Identität | Intern UID/GID 1000, getrennte Hostbereiche, keine unübersetzte Host-UID 1000 | Bestanden: Host-UIDs 1007500/1107500 | E01, E02 |
| T03.2 Namespaces | Unterschiedliche User-, Mount-, PID- und IPC-Namespaces | Bestanden, zusätzlich UTS/Net getrennt beobachtet | E02 |
| T03.3 Zuordnung bei Wechsel | Ursprüngliche Prozess-/Benutzerzuordnung bleibt erhalten | Bestanden für beide externen Wechselrichtungen | E08 |
| T04.1 Gesperrtes CE | Kein Runtime-Start und kein Ersatzbetrieb | Teilbelegt: nach verweigertem Beta-Login wurde `linux start` ausdrücklich abgewiesen; CE blieb gesperrt und Kontext fehlte. Dieser Fall hat zusätzlich keine gültige CLI-Sitzung und isoliert daher die CE-Startvoraussetzung noch nicht | E23, E24 |
| T04.2 Fehlende Namespaces | Start wird verweigert | Offen: gezielter aktueller Nachweis | — |
| T04.3 Ungültiges Mapping | Start wird verweigert | Offen: gezielter aktueller Nachweis | — |
| T04.4 Fehlende Sicherheitsvoraussetzungen | Jede erforderliche Voraussetzung erzwingen, kein schwächerer Ersatz | Offen: Voraussetzungen und Varianten vollständig zuordnen | — |
| T05.1 Eigene GNU-Shell | GNU-Programme mit eigener Identität und HOME ausführen | Bestanden für A/B | E01, E02 |
| T05.2 Fremder/gesperrter Kontext | Shell-Zugang verweigern | Offen: beide Varianten explizit zuordnen | — |
| T05.3 Shell-Ende | Nur Shell endet; Sitzung und derselbe Hintergrundprozess bleiben | Bestanden für Alpha; ergänzende Exitcode-/PTY-Prüfungen getrennt dokumentieren | E01 |
| T06.1 CE/HOME-Dateizugriff A→B und B→A | Keine fremden Dateibytes | Bestanden für die konkret geprüften Pfade | E05 |
| T06.2 Konfiguration/Test-Secrets A→B und B→A | Kein Lesen/Verändern; ursprüngliche Peer-Bytes erhalten | Bestanden | E06 |
| T06.3 Private Pakete B→A | Vorhandenen privaten Store weder lesen noch verändern | Bestanden für Alphas vorhandenen Storeeintrag | E06 |
| T06.4 Private Pakete A→B | Vorhandenen privaten Store weder lesen noch verändern | Offen: Beta hatte keinen privaten Store; Abwesenheit zählt nicht | — |
| T06.5 Flüchtige Dateien A→B und B→A | Kein fremder Inhalt/Schreibzugriff bei gleichzeitig entsperrten Benutzern | Bestanden; Alphas flüchtige Probe wurde separat nach Kontextneustart erneuert | E06 |
| T06.6 Prozesse A→B und B→A | Kein Zugriff/Stoppsignal; ursprünglicher Peer macht weiter Fortschritt | Bestanden, mit unabhängiger Beobachtung derselben PID/Startzeit | E05 |
| T06.7 IPC A→B und B→A | Gleicher Queue-Name bleibt privat, fremde Queue unzugänglich | Bestanden für POSIX-Mqueues; keine Aussage über beliebige weitere IPC-Schnittstellen | E07 |
| T07.1 Daten nach Runtime-Neustart | Originaldatei, Konfiguration und private Software erhalten | Bestanden für beide Benutzer: Originalbytes, Konfiguration, tatsächliche jq-Versionen und unveränderte Paketgenerationen | E03, E04, E17 |
| T07.2 Flüchtige Daten nach Kontextneustart | Alte `/tmp`- und `/run`-Proben fehlen, persistente Bytes bleiben | Bestanden für beide Benutzer mit unmittelbar vorher neu angelegten flüchtigen Proben | E17 |
| T07.3 Daten/Pakete nach VM-Neustart | Beide Originaldateien/Konfigurationen und A-private/B-gemeinsame Version erhalten | Bestanden: Originalbytes, getrennte Konfiguration, jq/libjq1 u4/u3, unveränderte Paketgenerationen/-datensätze; alte flüchtige Proben fehlen. Originaldaten und unterschiedliche Versionen auch nach zweitem gepaarten Neustart mit Beta-Passwortwechsel bestätigt | E13, E15, E16, E24, E27 |
| T08.1 Wechsel beider Richtungen | Alten Terminalkanal widerrufen, zulässigen Hintergrundprozess erhalten | Bestanden, frische Anmeldung über zweite CLI | E08 |
| T08.2 Bildschirmsperre | Terminal widerrufen, zulässige Arbeit weiterführen; kein behaupteter CE-Entzug | Bestanden: Keyguard/Asleep unabhängig bestätigt, beide Originalprozesse laufen, CE bleibt entsperrt | E09 |
| T08.3 AOSP-Ressourcenstopp | Gestoppten Hintergrundbenutzer samt Runtime-Ressourcen abbauen | Bestanden für den beobachteten AOSP-Limitfall: Beta-Prozess/Kontext entfernt, CE gesperrt, anschließend frische Anmeldung und bytegleiche Originaldaten; Alpha-Prozess bleibt erhalten. Vorzeitige fehlgeschlagene Endzustandsprüfung bleibt dokumentiert | E20, E22 |
| T09.1 Eigener Runtime-Stopp A/B | Nur eigener Kontext/flüchtige Ressourcen enden, Sitzung/CE korrekt | Bestanden in beiden Richtungen: Originalprozess/Kontext weg, Sitzung und CE erhalten; anschließende Daten-/Tmp-/Run-/Mqueue-Prüfungen bestanden | E17 |
| T09.2 Runtime-Stopp mit aktivem Peer | Peer bleibt unverändert arbeitsfähig | Bestanden in beiden Richtungen mit gleicher Peer-PID/Startzeit und fortschreitendem Zähler | E17 |
| T10.1 Regulärer Logout Beta mit Alpha aktiv | Beta-Prozess/Kontext weg, CE gesperrt, bekannte Datei unlesbar; Alpha unverändert aktiv | Bestanden | E10 |
| T10.2 Regulärer Logout Alpha | Originalprozess/Kontext weg, CE gesperrt, bekannte Datei unlesbar, Systembenutzer erhalten | Bestanden | E11, E12 |
| T10.3 Offene Zugriffe, Mounts, IPC | Vollständigen Ressourcenabbau und fehlenden Wiederzugriff zeigen | Teilbelegt: Kontext/Cgroup leer; eigenständiger Mqueue-/Mount-/Zugriffsabbau noch vollständig zuordnen | E12 |
| T10.4 Konkurrierender Start/Vordergrundwechsel | Keine erneute Freigabe während Abmeldung | Offen | — |
| T11.1 Ausstehende CE-Sperrung | Kein falscher Erfolg/Neustart der Runtime, Fehler sichtbar | Offen auf diesem Image | — |
| T11.2 Fehlgeschlagene CE-Sperrung | Kein falscher Erfolg, kein verdeckter Framework-Neustart | Offen auf diesem Image | — |
| T11.3 Wiederherstellung | Nach Beheben sichere Sperrung und frische Authentifizierung | Offen auf diesem Image | — |
| T11.4 Logout bei Paketaktion | Kein unbemerkter persönlicher Zugriff zurückgelassen | Offen | — |
| T12.1 Gepaarter Stopp | Android und KeyMint-Helfer vollständig und sauber beenden | Bestanden, Launcher Exit 0 und Helferzustand ausgehängt | E13 |
| T12.2 Gepaarter Start | Gleiches Profil/Datenträger, neue Boot-ID; beide CE vor Login gesperrt | Bestanden: identisches Manifest und Disk-Inodes, neue Boot-ID, CE `[0]`, Originaldateien und privater Store unlesbar | E15 |
| T12.3 Erster Login nach Reboot | Beide Originaldaten nach jeweils erstem korrektem Login erhalten | Bestanden für A/B ohne vorherigen Fehlversuch; verzögerte Sitzungskontrolle und tatsächlicher GNU-Zugriff | E16 |
| T12.4 CLI-Benutzerlöschung | Schlüsselzugriff, Runtime und privater Zustand entfernt | Offen | — |
| T12.5 Neue Identität und ID-Wiederverwendung | Kein Zugriff auf alte Daten/Zuordnungen, Seriennummer/Lebenszyklus korrekt | Offen; kontrolliertes zusätzliches Profil zulässig | — |
| T13.1 `install all` | Gültige Freigabe erlaubt; fehlende/falsche/Nicht-Adminfreigabe verweigert | Teilbelegt: gültige Alpha-Freigabe und tatsächliche Installation; drei Ablehnungsarten offen | E03 |
| T13.2 `install user` | Gleiche vollständige Autorisierungsmatrix | Teilbelegt: frühere gültige Alpha-Installation und alle drei Ablehnungsarten belegt. Neuer Beta-Auftrag erhält gültige Alpha-Freigabe, scheitert jedoch anschließend beim expliziten Versionsrückgang; keine Veröffentlichung, aktive Bestände unverändert. Fehler offen | E04, E33–E36 |
| T13.3 `update all` | Erlaubte Aktion und alle drei Ablehnungsarten | Bestanden: leere Adminauswahl, falsches Adminpasswort und Nicht-Adminfreigabe verhindern Veröffentlichung; frische Alpha-Freigabe erlaubt genau den geplanten gemeinsamen Updatebestand, anschließend tatsächlich aktiviert und ausgeführt | E28–E32 |
| T13.4 `update user` | Erlaubte Aktion und alle drei Ablehnungsarten | Offen | — |
| T13.5 `remove all` | Erlaubte Aktion und alle drei Ablehnungsarten | Offen | — |
| T13.6 `remove user` | Erlaubte Aktion und alle drei Ablehnungsarten | Offen | — |
| T13.7 Bereich/Eigentümer | Fehlender Bereich und manipulierte Eigentümer ändern nichts | Offen | — |
| T13.8 Antragsteller ≠ Admin | Privater Bestand gehört Antragsteller; Freigabe gewährt Admin keinen persönlichen Lesezugriff | Offen: Beta-Antrag mit frischer Alpha-Freigabe ausgeführt, danach Paketfehler vor Veröffentlichung. Private Zuordnung und anschließende Zugriffsprüfung noch nicht belegt | E36 |
| T13.9 Unbeteiligtes CE | Freigabe entsperrt keinen unbeteiligten Benutzer | Offen: für aktuelle vollständige Aktionsmatrix zuordnen | — |
| T14.1 Gemeinsame Software bestehender Benutzer | Gemeinsames Programm tatsächlich ausführen | Bestanden Alpha vor privater Aktivierung und Beta | E03, E02 |
| T14.2 Nachträglicher Benutzer | C erhält gemeinsame Software ohne A/B-private Daten/Versionen | Teilbelegt: Gamma erhält frisches HOME und führt gemeinsame jq/libjq1 u3 aus, mit passendem Bibliotheks-Hash und gemeinsamer Paketgeneration; eigene Konfigurationsordner leer. Explizite Zugriffsprüfung auf A/B-private Daten aus Gamma noch offen | E21 |
| T14.3 Persönliche Einstellungen | Unterschiedliche private Konfiguration trotz gemeinsamer Basis | Bestanden für erfasste A/B-Proben, einschließlich bytegleicher Wiederherstellung nach Reboot | E02, E06, E16 |
| T15.1 Dasselbe Paket in V1/V2 | Beide Versionen samt passenden Abhängigkeiten tatsächlich ausführen | Bestanden: jq/libjq1 u4 bei A, u3 bei B, gleicher Programmauftrag | E02, E04 |
| T15.2 Vorrang privater Version | Private V2 bestimmt tatsächlichen Programmlauf | Bestanden Alpha | E04 |
| T15.3 Nicht verfügbare Version | Ohne stille Ersetzung ablehnen, Bestand erhalten | Offen auf diesem Image | — |
| T15.4 Unauflösbare Abhängigkeiten | Ohne stille Ersetzung ablehnen, Bestand erhalten | Offen auf diesem Image | — |
| T15.5 Private Entfernung abweichender Version | Rückkehr zur gemeinsamen Variante anzeigen und konsistent ausführen | Offen | — |
| T15.6 Private Entfernung gleicher Version | Private Auswahl tatsächlich aufheben, auch ohne Versionsänderung | Offen | — |
| T16.1 Gemeinsames Update bei privatem Bestand | Aktive Kontexte konsistent erhalten, Aktivierung ausstehend anzeigen | Bestanden: fünf gemeinsame Updates veröffentlicht; beide bisherigen Prozessidentitäten, Mounts und vollständigen Paketbestände erhalten. Beide zeigen ausstehende Aktivierung und führen ihre bisherigen jq-/Bibliotheksversionen tatsächlich aus | E31 |
| T16.2 Aktivierung nach Update | Gemeinsame Updates plus private Auswahl konsistent aktivieren oder Konflikt erklären | Bestanden: beide eigenen Kontextneustarts aktivieren exakt die geplanten Versionen einschließlich PCRE2/OpenSSL; beide vollständigen 81-Paket-Bestände geprüft, jq tatsächlich ausgeführt, Status aktuell. Der jeweils andere Kontext bleibt unverändert | E32 |
| T16.3 Private Version bleibt | Keine stille Überschreibung privater Festlegungen beim Abgleich | Bestanden: Alphas explizite private jq-u4-Auswahl bleibt bytegleich und die neue private Generation bindet an die neue gemeinsame Basis. Das gemeinsame jq wird bei diesem Update ebenfalls u4; der vorherige V1/V2-Unterschied ist gesondert belegt | E04, E31, E32 |
| T17.1 Gleichzeitig gemeinsam/privat | Serialisierung oder sichtbare Ablehnung, kein Teilbestand als Erfolg | Offen | — |
| T17.2 Aktionen verschiedener Benutzer | Eigentum/Autorisierung und konsistenter Bestand bleiben erhalten | Offen | — |
| T17.3 Abbruch | Letzter konsistenter Bestand bleibt erhalten/wird wiederhergestellt | Teilbelegt: Abbruch des gemeinsamen Updateplans und des persönlichen Installationsplans vor Freigabe erhält beide Kontexte und Paketbestände; Ausführungsphasen bleiben offen | E28, E33 |
| T17.4 Installationsfehler | Fehler sichtbar, keine teilweise aktivierte Umgebung als Erfolg | Teilbelegt: tatsächlicher Fehler beim gültigen privaten Versionsrückgang wird als Fehler gemeldet; beide Kontexte und Auswahlen unverändert, kein privater Teilbestand ausgewählt. Weitere Fehlerphasen bleiben offen; gültige Installation selbst muss korrigiert werden | E36 |
| T17.5 Logout während Transaktion | Kein persönlicher Restzugriff, sichtbarer Abschluss/Abbruch/Reparaturbedarf | Offen | — |

## Belegkatalog

Die SHA-256-Werte beziehen sich auf die vollständigen lokalen JSON-Dateien.
Diese enthalten Ereignisse beziehungsweise verknüpfte Rohbelege und grenzen
ihre Aussage ein. Keine Profile, Passwörter oder Buildartefakte werden hochgeladen.

| ID | Lokaler Pfad relativ zum Belegverzeichnis | SHA-256 |
| --- | --- | --- |
| E01 | `alpha-first-flow/result.json` | `a08f725588f7a841f875b54c3eba4d255d7767b55a3c332e5fe6b3c6b1fce663` |
| E02 | `two-user-version-baseline/result.json` | `7c3ec5ef166622fc6139757b6b90229f1633e479d30d6477ef0ae5187d1b0ba5` |
| E03 | `shared-u3-activation/result.json` | `2c1029a6d2a63a64aeb3a5440524802e1d0ef7911f53d67da61dc10e129601f9` |
| E04 | `alpha-u4-activation/result.json` | `3e942b3d1bb018ad1b8a9dad03e9f7a57c3f57c1616825d9d9071736e73080b2` |
| E05 | `reciprocal-file-process-proof.json` | `6b288f84c2af9d8bb3d9c83de80b9f0c29fbdc536bb6b5709f5e0eb121942bc7` |
| E06 | `reciprocal-private-state-proof.json` | `b09a95151346b9c1dc27c94a4516e48be02f8a77d6cc553b57751bb7e7c8c256` |
| E07 | `reciprocal-mqueue-proof.json` | `c6761a190c94640c5452d79db494ebdb3a6263e4a47b367ba9f9a2fe859470c3` |
| E08 | `reciprocal-external-switch-proof.json` | `2c6bd123b9b3eff2185562d756644513c022c9701b6f6ccba7a7b9a871982f66` |
| E09 | `screen-lock-background-proof.json` | `9b100ec38351394a93667898655c346f82eef3b313af1f7004a3cdc02af0f283` |
| E10 | `beta-logout-proof.json` | `deba29150aa6e9f3f9e244d4e9ef0cbb7f5beabf1ac76a059d618fe3a5cbaa88` |
| E11 | `alpha-logout-proof.json` | `1947ad2d2c8f912e579d747ab4d39acd1b87f19a8e669fc6ad940c425a00578e` |
| E12 | `identity-test/reboot-checkpoint.json` | `40df7073677cda4cf824cec36ecbab9fe5aaa193e254664a140517b5eb2e684a` |
| E13 | `paired-shutdown-1.json` | `a1ddc71361415466e697cd6e363b2a36ab42ae84e58f5b6b4435e3d3c7657ed4` |
| E14 | `first-reference-boot-health.json` | `b41f371a1c0b92b80eace46ba06021dff01005cbb671f2e4f8ded5bbaebd9553` |
| E15 | `postboot-locked-baseline.json` | `38868bfb264e865835c1a375c92c6377ef17744a9a1a782e21c31f7b09fc0553` |
| E16 | `paired-reboot-readback-v2/result.json` | `5f527250f03c94b3677224987d110dd9c8a888f4bb90c79d943db6909db19ab1` |
| E17 | `reciprocal-runtime-stop-proof.json` | `e6e3763b9ffc5b4b72fd0ccfc8431dd02e956d81d53908d580d8cf1495ba8c85` |
| E18 | `private-state-hash-verifier-check.json` | `ed64a678635800422ce3d7e00cb512a7acfa21bb2ca95858fa3f55680e1f0239` |
| E19 | `before-third-user.json` | `645ee4ccaccbcd3e04ffa97b5699e1b8c2f940a766f58768a51ed80adea41e6c` |
| E20 | `beta-aosp-resource-stop-proof.json` | `1c65561ea6a2461714261f7fe0b709174c20e5c2acc4162c8ccb9a0e25da6ae4` |
| E21 | `gamma-shared-program-proof.json` | `76b162a64cd6ea9962e9bda60d5f9412f6da79e927eda4c8ed08e9bffbf0f9a9` |
| E22 | `beta-aosp-resource-recovery-proof.json` | `4283d6658f6ca6e6b94cd0422caeed23b8b9931d45ec117e1e364728ec0f1d82` |
| E23 | `beta-password-change-proof.json` | `551d7d9f268a4a3f3c05aed474e5f949f50fe4b25320334b2f4edb61ba54eac1` |
| E24 | `beta-password-reboot-proof.json` | `31db67c68b1fcbbc1bd64480edb2e53abea325290c8aa5fef0dccd2d3b167964` |
| E25 | `boot2-pre-shutdown-health.json` | `580f1d6e32b6507fb56803f7dd38b4e43d9746773bb96ab8f51be839058035a1` |
| E26 | `boot-migration-cleanup-classification.json` | `c798664aeeebb93514bc66de8927ea754d9dede938598f6792618b758dcce877` |
| E27 | `alpha-second-reboot-readback-proof.json` | `b3b4b958c2ff1f32d057290627e3ebd76dd9f420df00b3fb775510cb0873b212` |
| E28 | `update-all-cancel-proof.json` | `45770e57849e8820b745bc47b06f7a9ad3abaefad14acce93921013e1c91fcc8` |
| E29 | `update-all-wrong-password-proof.json` | `9f9643bfb5eb89b1ad733d3c1579b77cf15c8401c0c69ce08b5d688483a32401` |
| E30 | `update-all-nonadmin-proof.json` | `8083b52d469e496c317f9b1bd1259755c89ae2dcd2cdbe03963b3009055e25d6` |
| E31 | `shared-update-publication-proof.json` | `4cda0dc2b398d6c8809a441b63f4c1b924ae25f250d2d683fd06dbd60a0f31ba` |
| E32 | `shared-update-activation-proof.json` | `80ec5a01d0a952c7d1455360b9155d2760c17a08665b5097b36acc48d119b961` |
| E33 | `beta-private-install-cancel-proof.json` | `37174daf62c17c7ccba4843f20d08ed2c1f06419c3d27cc7f3cfdf043fb82093` |
| E34 | `beta-private-install-wrong-proof.json` | `178488713f28a6f2cb220be5c452336b8b5d492ce7a451c91cc7d90288a7b605` |
| E35 | `beta-private-install-nonadmin-proof.json` | `1f07beb76514f6894a69ad79702d3b93a203d25c4d0218128d17bd924e3a16d2` |
| E36 | `beta-private-install-valid-failure-proof.json` | `4625a96c725a71959554368546ac43cb5a62dc3cf5f437dd3724a34d89e223b2` |
| E37 | `reviewed-downgrade-before-fix/result.json` | `9280d539c42b8683bde43c233c71dcaf99a1fd81bc6383dd11a609a91ac016c6` |

E37 ist eine **fehlgeschlagene Regression zur Fehlerreproduktion**, kein
bestandener Abnahmefall. Sie läuft im separaten frischen Profil
`af23f1ac-854c-4883-a499-665ca9494aef` auf Image `209278de`; Testcommit
`065c6e8` ändert ausschließlich den Executor-Test, alle zehn Hilfsprogramme
sind bytegleich. Der normale geprüfte Archivplan für Version 2 → 1 endet
in der APT-Simulation mit Status 100: Die nichtinteraktive Ausführung benötigt
`--allow-downgrades`. Boot-/SystemServer-Identität, Enforcing und CE `[0]`
bleiben unverändert. Die vorbereitete Korrektur erlaubt den Versionsrückgang
bei vorhandenem geprüftem Plan; die exakte Prüfung der simulierten Änderungen
bleibt erhalten. Der Korrekturstand `f098f43` ist inzwischen vollständig lokal
gebaut; 20 Images, Kernel-/Runtime-Eingaben, AVB und die neue Basisdisk sind
geprüft. Boot und alle sechs gezielten Executor-Tests sind inzwischen bestanden,
einschließlich derselben zuvor roten Regression. Auch der echte private
CLI-Versionsrückgang ist inzwischen auf dem Korrekturimage bestanden; sein
gesonderter Nachweis steht in der folgenden Tabelle. Die übrige Abnahme bleibt offen.

## Zusätzlicher Korrekturstand f098f43

Image und native Tests stammen aus
`f098f439f051c34e92fb1be0b4d908cc542358ea`. Das neue Profil ist
`535c2e93-df64-445b-b9e2-b71e6b403db7`. Der erste Boot hat ID
`33e0e0e8-3896-4c42-a19d-270d4665153c`, SystemServer `1404/40097`.
Die späteren, jeweils im Beleg gebundenen Prüfungen nach gepaartem Neustart
laufen im zweiten Boot `17a75d6e-75f2-4f18-bf02-ec3d093e57b8`, SystemServer `1124/21865`.
Die folgenden Belege liegen lokal unter `out/phase1-dod/f098f439/`.

| Nachweis | Ergebnis und Grenze | Beleg / SHA-256 |
| --- | --- | --- |
| Bootkonfiguration | Boot abgeschlossen, authentifiziertes ADB, Enforcing, FBE/Metadatenverschlüsselung, passender AVB-Digest, CE nur `[0]`; begrenzte Beobachtung | `boot-observation.json`, `6178275cc63b98a851415842911019042b72d62e4394e649fbbd676318cf9255` |
| Paketkorrektur | 6/6 Executor-Tests bestanden: gewöhnliche Installation/Aktualisierung/Entfernung; geprüfter Ablauf mit Konfiguration/Abhängigkeitsmarken; gewöhnlicher privater Versionsrückgang; Abgleich mit älterer Basis; private Entfernung mit Rückfall; abweichende Simulation vor Paketskripten verweigert | `native-downgrade-controls/result.json`, `e004cf92be2c70defd8e90998ec3bb1e0c2abb83a28a08c290b7d000edfd55e8` |
| Bildschirm und Eingabe | Einstellungen sichtbar; QMP-TAB/Enter öffnet Netzwerkseite, ein Maus-Klick in unabhängig beobachteten Zurück-Koordinaten führt zur Startseite; Systemidentität unverändert | `qmp-ui-proof.json`, `d6997661ac4a1a639c00af0ea084894f422191f809c7f7bb1fabfaf89425ec9e` |
| Binäres ADB | 262144 synthetische Bytes identisch übertragen/zurückgelesen; eigene temporäre Gastdatei entfernt | `adb-binary-proof.json`, `6c29af2f7ef997b27db191ed58ebffcdad7ba448418b9a2cae8d75eeff78b7b9` |
| Erster persönlicher CLI-Ablauf | Zwei Benutzer angelegt; Alpha erstmals angemeldet, GNU-Befehle und Home-Struktur geprüft, 1024-Byte-Datei geschrieben; ursprünglicher Hintergrundprozess überlebt Shell-Ende. Beta bleibt gesperrt. Noch kein Zwei-Benutzer-/Persistenznachweis | `initial-personal-flow.json`, `53ca89bcd135f6b9856448d89692eadae41d46edb370805d9029f38be4c5cdee` |
| Gemeinsamen Installationsplan abbrechen | Leere Adminauswahl beendet den jq-Plan; Laufzeit, Paketdatenbank, Auswahl und CE unverändert. Generische Fehlermeldung der CLI bleibt ungenau; keine Installation bewiesen | `shared-install-cancel-proof.json`, `e440ef7c69b981fd7b2a72c9e9608a485259554541820a96a78291f42a7959e1` |
| Gemeinsame Installation, falsche Freigabe | AOSP verweigert falsches Adminpasswort; Paketdatenbank, Auswahl, Laufzeit und CE unverändert, Anmeldung weiterhin gültig | `shared-install-wrong-proof.json`, `698f71f2a92aea4decaf6f196c084b5c19f604273be9cec5a4fefe90b5ce3534` |
| Gemeinsame Installation, Nicht-Adminfreigabe | Beta darf nicht freigeben; Planung/Ablehnung erhalten Paketbestand, Kontext und CE, Alpha bleibt angemeldet. Generischer erneuter Anmeldehinweis bleibt ein Bedienungsfehler | `shared-install-nonadmin-proof.json`, `c24d2162f05fb0f1ca92944c57f9b9b89a0624e252b51a0f37f1aec33909802f` |
| Paketbereich und Eigentümerargument | Je drei CLI-Ablehnungen für fehlenden Bereich und unzulässiges Eigentümerargument bei install/update/remove; Paket-/Laufzeit-/CE-Zustand exakt unverändert | `package-argument-denials-proof.json`, `2551b8fb3f5ef23a35e2ecac4b934bafa06c48df785378baa26947c3401e6939` |
| Gemeinsame Installation und Aktivierung | Gültige Freigabe veröffentlicht u3; ursprüngliche Runtime bleibt bis bewusstem Stopp/Start unverändert. Neuer Kontext führt jq mit passender Bibliothek aus; genau drei zusätzliche Pakete, Originaldaten erhalten, alte flüchtige Dateien weg. Falscher Logout-Prüfaufruf separat erhalten | `shared-u3-activation-proof.json`, `3bee77ca545d0a6b67e612bed633b34239f6cce7c31860ba421a1cc15a2d8a8b` |
| Private Installation und Aktivierung | Private jq/libjq1-u4-Generation gehört Alpha 10/10; gemeinsame u3-Auswahl und laufender Kontext bleiben zunächst unverändert. Nach Stopp/Start u4 tatsächlich ausgeführt, Originaldatei/Einstellungen erhalten, alte flüchtige Dateien weg | `private-u4-activation-proof.json`, `b14ea49a66246e89b0418b44856509f5bbf5108fe32e4deeda7bc2fa3dd74474` |
| Zwei Benutzer und abweichende Versionen | Erster Beta-Login stabil und GNU ausgeführt; Alpha privat u4, Beta gemeinsam u3. Getrennte tatsächliche Host-UID/GID-Bereiche und sechs Namespaces, Alphas ursprünglicher aktueller Hintergrundprozess überlebt Wechsel. Gegenseitige Zugriffstests bleiben separat | `two-user-version-baseline-proof.json`, `e9069aa25986d82adf291cf05b5eb6f95877c9319e82a1fba8c205999b8e727a` |
| Gegenseitige GNU-Isolation | Beide Richtungen: bekannte Testdateien unlesbar, Konfiguration und flüchtige Dateien weder lesbar noch veränderbar, fremder Testprozess nicht adressierbar und vor/nachher unverändert fortschreitend. POSIX-Nachrichtenwarteschlangen getrennt. Betas Zugriff auf Alphas private Paketauswahl verweigert; Beta besitzt noch keinen privaten Store | `two-user-isolation-proof.json`, `e4ce118967b669b41907a1420cbd4f90218d4e7786a8fb3d617f9c370bdb8283` |
| Bildschirmsperre | Offene GNU-Shell widerrufen, CLI unangemeldet; Android Asleep/Keyguard bestätigt. Beide ursprünglichen Hintergrundprozesse schreiten weiter, CE `[0,10,11]` bleibt entsperrt, ursprünglicher SystemServer erhalten. Kein Logout-Nachweis | `screen-lock-background-proof.json`, `cdb4ffd32e9b3b4decc6d64d5852d11989266263fb95b90d65b3a7d4139fb380` |
| Regulärer Logout beider Benutzer | Beta zuerst: ursprünglicher Prozess/Kontext weg, CE gesperrt, bekannte Datei ohne Bytes; Alpha schreitet fort und liest Originaldaten unverändert. Danach Alpha ebenso abgemeldet. Nur System-CE und keine persönlichen Kontexte/Paketworker; ursprünglicher SystemServer erhalten | `both-logout-before-reboot-proof.json`, `1c21ea6ccfa78082147f4882fcab0eada5446650f6e0caa6b01acb5261e6db8d` |
| Sauberer Paar-Stopp | Ursprüngliche Android-/KeyMint-Prozesse samt Starter beendet; Android Power down und Helper clean bestätigt, Profilmanifest unverändert. Nachfolgender Boot und Datenreadback separat belegt | `paired-shutdown-proof.json`, `4dd9969d3a48f88a47a54be02b157086df5d455af655fcc1d1ba429dbab77fce` |
| Zweiter Boot vor persönlicher Anmeldung | Gleiches Profil und Schutz-/Imageeigenschaften, neue Boot-ID `17a75d6e-75f2-4f18-bf02-ec3d093e57b8`, SystemServer 1124/21865. Nur System-CE entsperrt, keine persönlichen Kontexte; beide ursprünglichen Testdateien unlesbar. Readback nach Anmeldung separat belegt | `postboot-locked-baseline.json`, `d6e739a9cd01a3e4dd1a287677876a495332c62037b65cf5b68c959670f602c0` |
| Daten und Versionen nach Paar-Neustart | Beide ersten korrekten Anmeldungen stabil; Originaldateien/Konfiguration bytegleich, alte flüchtige Dateien und IPC weg. Alpha führt privat u4, Beta gemeinsam u3 aus. Je 81 Pakete, vollständige Paketdatenbank-Prüfsummen, Auswahlen und private Festlegung exakt unverändert; neue Host-UID/GID-Zuordnungen getrennt | `paired-reboot-readback-proof.json`, `aad3626470a13b33ceb4bcac84725d30c2cdc288ae769d921fa9d50b95773683` |
| Nachträglicher Benutzer, T14.2 / Referenzschritt 11 | Gamma 12/12 über CLI mit Alpha-Adminbestätigung angelegt; vor Passwort gesperrt, erster Login stabil. Frisches HOME, getrennte UID/GID/Namespaces, gemeinsame u3-Software samt Abhängigkeiten und eigenen Einstellungen ausgeführt. Bekannte A/B-Testdateien und Alphas vorhandene private Paketauswahl liefern aus Gamma keine Bytes. Alpha bleibt aktiv; Beta wird regulär durch AOSP gestoppt und gesperrt. Beta besitzt noch keinen privaten Paketstore; dieser fehlende Gegenfall wird nicht als geprüft gezählt | `third-user-common-and-isolation-proof.json`, `1af75bc265b6905f516e48c08955bf272984e6e31941aa207d42c7e796784847` |
| AOSP-Ressourcenstopp und Wiederanlauf, T08 | Nach AOSP-Stopp und bestätigter Beta-CE-Sperre: frische Anmeldung ohne vorherige Entsperrung durch Zielauswahl, Originaldatei/Konfiguration bytegleich, gemeinsame u3-Software tatsächlich ausgeführt. Vollständiger Paketbestand und Auswahlen unverändert; neuer Beta-Kontext und getrennt erfasste neue Hintergrundprobe. Alphas ursprünglicher Kontext und Prozess bleiben erhalten, Gamma ist abgemeldet | `beta-resource-stop-recovery-proof.json`, `bc0b3107b85f0cd631b7e21a27b3fda36fea7c6eb86195b3c9c39da527d8c2e3` |
| Gemeinsames Update ohne Adminauswahl, T13.3-Teilfall | Beta beantragt fünf Updates; leere Adminauswahl beendet den Plan ohne Veröffentlichung. Vollständige Paketdatenbanken, Auswahlen, beide Kontexte und CE exakt unverändert, Betas Sitzung weiterhin gültig. Weitere Freigabevarianten und Aktivierung bleiben separat | `update-all-cancel-proof.json`, `5d6531948a7feb12d258029d7dbcfa8ad39974e860149530c5bbe534abbe4bc3` |
| Gemeinsames Update, falsches Adminpasswort, T13.3-Teilfall | AOSP verweigert das falsche Alpha-Passwort für Betas Updateantrag. Keine Veröffentlichung; vollständige Paket-, Auswahl-, Kontext- und CE-Felder identisch, Beta bleibt angemeldet | `update-all-wrong-proof.json`, `ee220317ebdc6c832065165f024b30f500d86d38b5b53bc5abe6e4ef78166f6d` |
| Gemeinsames Update, Nicht-Adminfreigabe, T13.3-Teilfall | Betas korrektes Passwort erteilt keine Adminfreigabe. Keine Veröffentlichung, sämtliche verglichenen Zustandsfelder unverändert und Beta weiterhin angemeldet. Generischer erneuter Anmeldehinweis bleibt als CLI-Fehler sichtbar | `update-all-nonadmin-proof.json`, `e87848ce3d11c6957398962eb7a7bc3bdde7dda812ae3a1eaae62e45270e899b` |
| Gemeinsames Update mit frischer Alpha-Freigabe, T13.3 / T16.1 | Beta beantragt, Alpha bestätigt; fünf geplante Updates werden gemeinsam veröffentlicht. Beide bisherigen Kontexte samt vollständiger Paketdatenbanken und Alphas privater Festlegung bleiben identisch; ursprüngliche Hintergrundprozesse schreiten fort. Beide melden ausstehende Aktivierung und führen ihre bisherigen Versionen tatsächlich aus. Neue Versionen nach bewusstem Neustart noch separat zu prüfen | `shared-update-publication-proof.json`, `b746d1189753e8df3d83fec3be6ce0b4b95bc743904550a8c15ec8d87a7c5bcc` |
| Gemeinsame Updateaktivierung und beide eigenen Runtime-Stopps, T16.2 / T09 | Beide bewussten Stopps entfernen nur den eigenen Kontext, behalten AOSP-Sitzung/CE und lassen den Peer fortschreiten. Beide neuen Kontexte führen die erwarteten Versionen aus; vollständige 81-Paket-Bestände, private Alpha-Auswahl und Originaldateien geprüft. Frisch vorbereitete temporäre Dateien verschwinden; beide melden aktuellen Paketstand. Privater Beta-Versionsrückgang bleibt separat | `shared-update-activation-proof.json`, `f812ec421300f35c27a2683431fb3e3536d932053f7182b093972e5a0158df13` |
| Tatsächlich ausstehende CE-Sperrung und Wiederherstellung, T11-Teilfall | Alpha-Logout nicht bestätigt: drei belegte Inodes, Sitzung widerrufen, eigener Kontext entfernt, Beta unverändert. Reguläre Anmeldevorbereitung bestätigt Sperrung vor Passwort; frische Authentifizierung stellt ursprüngliche Daten und exakten Paketbestand wieder bereit. Ursprünglicher Logout bleibt fehlgeschlagen, Ursache der belegten Inodes und weitere T11-Varianten offen | `alpha-pending-logout-recovery-proof.json`, `c7e75df4adbcb90ad9e4b97b2a325dca50b8e07514e43d2bc7523ce2cbfb0f38` |
| Private Beta-Installation ohne Adminauswahl, T13.2-Teilfall | jq/libjq1-u3-Plan bei gemeinsamem u4 ohne Freigabe abgebrochen. Betas komplette Zustandsfelder unverändert; Alpha und Gamma CE-gesperrt, deren private Stores nicht gelesen | `private-beta-cancel-proof.json`, `ed2f532b01bd8cf175b1322be03efbbac08264721a30d90829d7b8df51adf6fb` |
| Private Beta-Installation mit falschem Adminpasswort, T13.2-Teilfall | Identischer Plan, ausdrückliche AOSP-Passwortablehnung; vollständiger Vergleich unverändert und Beta weiterhin gültig angemeldet. Keine Veröffentlichung oder private Versionsausführung behauptet | `private-beta-wrong-proof.json`, `51cf29a728e834f1c5d78ac6815bc75ddd9147de9e26ec4fe4b5ad2560a6b81b` |
| Private Beta-Installation mit Nicht-Adminfreigabe, T13.2-Teilfall | Betas korrektes Passwort darf den identischen Plan nicht freigeben; alle Zustandsfelder bleiben exakt gleich, Beta bleibt angemeldet. Alpha/Gamma weiterhin gesperrt. Generischer Anmeldehinweis bleibt als CLI-Fehler dokumentiert | `private-beta-nonadmin-proof.json`, `c106fa34d1e4bb382be42910073606ad1d100a9ceada0cee3836088126fd2c07` |
| Private Beta-Installation mit Alpha-Freigabe und echter Versionsrückgang, T13.2 / T15-Teilfall | Frische Alpha-Freigabe veröffentlicht ausschließlich Betas private Auswahl; vorheriger Kontext zunächst unverändert. Nach Neustart privates jq/libjq1 u3 gegen gemeinsame u4-Basis tatsächlich ausgeführt, alle 81 Pakete und ursprüngliche Daten geprüft, Status aktuell. Alpha/Gamma in den Zustandsaufnahmen gesperrt. Zwei fehlgeschlagene Live-Worker-Aufnahmen explizit erhalten; keine lückenlose CE-Zeitreihe behauptet | `private-beta-downgrade-activation-proof.json`, `e04b127f05db4e84025d693f6c829d684d4a180bf3b79bab8e932a2859bd8811` |
| Private Entfernung ohne Adminauswahl, T13-Teilfall | Plan zeigt Rückkehr von privatem jq/libjq1 u3 zur gemeinsamen u4-Version; leere Auswahl beendet ihn ohne Veröffentlichung. Alle Paket-, Kontext-, Auswahl- und CE-Felder exakt unverändert; Beta weiterhin angemeldet | `private-remove-beta-cancel-proof.json`, `142ff9a48c2a06b08bdaa86cd7188c20fd1e4cffea5d98ee69f0b49c15073a8c` |
| Private Entfernung mit falschem Adminpasswort, T13-Teilfall | AOSP verweigert das falsche Alpha-Passwort für denselben Rückkehrplan. Keine Veröffentlichung; vollständige Zustandsfelder unverändert, Betas Sitzung gültig. Noch keine ausgeführte private Entfernung | `private-remove-beta-wrong-proof.json`, `66a5a9ea9636c640a95f9d25ac3600eccca46e88bb197d047cad0677626aa23f` |
| Private Entfernung mit Nicht-Adminfreigabe, T13-Teilfall | Betas korrektes neues Passwort erlaubt keine Adminaktion. Keine Veröffentlichung, vollständige Zustandsfelder identisch und Beta weiterhin angemeldet. Tatsächliches Ereignis nach Passwortwechsel vom korrigierten Offline-Prüfer geprüft; gültige Ausführung separat | `private-remove-beta-nonadmin-proof.json`, `32855c9cb2a4e9ed4320b72330b61e7af68564b7550dbeadfc41e26bd0018323` |
| Private Entfernung mit Alpha-Freigabe und Rückkehr zu anderer gemeinsamer Version, T13 / T15 / T16-Teilfälle | Beta hebt private u3-Wahl auf, Alpha bestätigt frisch. Veröffentlichung erhält beide alten Kontexte; Beta führt weiter u3 aus. Eigener Stopp entfernt alten Init/Kontext und erhält Sitzung/CE/Alpha. Neuer Kontext führt jq/libjq1 u4 aus, private Festlegung leer, alle 81 Versionen korrekt, Originaldaten erhalten und neue temporäre Dateien entfernt. Status aktuell; gleiche-Version-Fall separat offen | `private-remove-beta-activation-proof.json`, `0fb703e608d8924a87607351f4fb4f159f4be2430474b0b104935541ed9d7bed` |
| Private Entfernung bei gleicher Version, T15 / T16-Teilfälle | Alpha hebt private u4-Wahl bei gemeinsamer u4-Version mit frischer Freigabe auf. Plan zeigt keine Versionsänderung; Veröffentlichung erhält beide Kontexte. Eigener Stopp/Start aktiviert leere Festlegung bei identischen 81 Versionen und bytegleicher Paketdatenbank. Originaldaten erhalten, neue temporäre Dateien entfernt, Beta unverändert, Status aktuell | `private-remove-alpha-proof.json`, `790219c2c476e262fee76e7b5feb544a9b338066a3191295cab62d77a14f9bbd` |
| Beide vorhandenen privaten Paketauswahlen, T06-Teilfall | Beide GNU-Kontexte verweigern Lese-/Schreibzugriffe auf sieben vorhandene Peer-Pfade einschließlich privater Paketauswahl; Bytes unverändert. Bekannte Originaldateien und SIGSTOP in beiden Richtungen abgewiesen, Peer-Prozesse danach mit gleicher Identität fortschreitend. Vollständige Paket-/Kontextaufnahmen exakt unverändert. Keine neue IPC- oder vollständige Paketdateiabdeckung behauptet | `two-private-contexts-isolation-proof.json`, `fd37f26f8b072c53880b4babf9e145d593608e5a329a895fd243d996e0ba58b4` |
| Passwortwechsel und erneute Anmeldung nach Sperrung, T02-Teilfall | AOSP bestätigt Betas Änderung; vollständige Pakete/Kontexte zunächst unverändert. Bestätigter Logout, alte Probe entfernt, bekanntes Original liefert keine Bytes. Altes Passwort einmal abgewiesen, neues akzeptiert; Originaldatei/Konfiguration/private u3 und alle 81 Pakete im neuen Kontext erhalten, alte temporäre Dateien weg. Alpha unverändert. Gepaarter VM-Neustart mit neuem Passwort bleibt offen | `beta-password-change-readback-proof.json`, `276ff9864fe81196268aae2ff673156bea885d4a8a781e1b3edab3c276262180` |
| Neues Passwort und beide privaten Bestände nach gepaartem Neustart, T02 / T07 / T12-Teilfälle | Beide Benutzer regulär abgemeldet und CE gesperrt; Android/KeyMint sauber beendet, dasselbe Profil als boot-3 gestartet. Vor Anmeldung Originaldateien unlesbar. Erste korrekte Logins ohne vorherige Fehlversuche, Beta mit neuem Passwort; Originaldaten/Konfigurationen, beide privaten u4/u3-Ausführungen und vollständige 81-Paket-Bestände erhalten. Alte-Passwort-Ablehnung bleibt separat im vorherigen Boot belegt | `password-paired-reboot-readback-proof.json`, `28244bb5466e7ab96ad56a4ac337d541e12f75d74c2f44af6518b4e33d05f0c8` |
| Altes Passwort nach drittem Boot, T02-Teilfall | Nach erstem korrektem neuen Login und gesondertem bestätigtem Logout: einmalige alte Anmeldung abgewiesen, CE gesperrt, Originaldatei ohne Bytes, Beta-Kontext entfernt. Alphas vollständiger Kontext und Paketbestand unverändert; kein Framework-Neustart | `boot3-old-password-denial-proof.json`, `59f512b71e2e512ce3a0414d8993af4be0d4e7cca01243ccc65e334a2575c496` |
| Neue Anmeldung nach alter Passwortablehnung, T02 / T07-Teilfälle | Neues Passwort angenommen; ursprüngliche Datei und Einstellungen bytegleich, private u3-Ausführung und alle 81 Paketversionen erhalten. Neuer Beta-Kontext mit getrennten Namespaces, Alpha vollständig unverändert | `boot3-beta-recovery-proof.json`, `33a866cf464e6d07eb06b3870cf4da7b44e8a2513585f6ce9fe02492bf00d064` |
| Offline-Prüfer bei gewechseltem Testpasswort | Sieben Host-Tests und erneute Auswertung dreier bestehender Gastablehnungen bestanden. Neuer Ereignisname wird erkannt, Mehrdeutigkeit und Zustandsänderungen bleiben Fehler. Kein neuer Gastfreigabeversuch durch diesen Beleg | `denial-verifier-rotated-credential/result.json`, `af80100159e8aef660b6bb01a6f199a38e60deb98f07b3380e8fc2a3486f0234`; `historical-replay.json` im selben Unterverzeichnis, `2e4c34dabb2e60d3ce98bda876e1629607ebb027a5c39250b5e274be997ce0a9` |
| Dritter Boot, begrenzte Dienstbeobachtung | Ein SystemServer-Eintritt und keine geprüften Kernel-Panik-/Fatal-/ANR-/FORTIFY-/Watchdog-Muster bis nach den ersten Readbacks. Keine vollständige Klassifizierung aller Dienst-Rückgaben behauptet | `boot3-bounded-health.json`, `aebc15ccd52519bfba81dc13f6ae42bc9d1b74b0636cebd40781dac12b05cead` |
| Zweiter Boot, Dienstbeobachtung | Ein SystemServer-Start und keine geprüften Fatal-/ANR-/FORTIFY-/Watchdog-Meldungen bis zu den Datenreadbacks. Zusätzliche einmalige Altbestand-Aufräumrückgabe quellen-/zustandsgebunden eingeordnet, nicht als behoben behauptet | `boot2-health-classification.json`, `46ac763c50fe95c3bd5447692022c7dd2efdf1e7f4b64ac639833b9b33394002` |
| Passworttransport-Quellbindung | Neun Dateien bytegleich zu Imagecommit und gespeichertem Buildmanifest; separate interaktive Eingabe, Echo-Abschaltung, Pufferbereinigung und sensible Binderdeklarationen statisch geprüft. Keine umfassende Laufzeit-Offenlegungsprüfung behauptet | `credential-transport-source-binding.json`, `88e84b5de6d37cd48f6b73683f77800d157c8a2ff279084daa823855e793037d` |
| Aktuelle Verschlüsselungskonfiguration | FBE/Metadatenverschlüsselung und XTS/HCTR2 beobachtet; vier Quellen gegen Upstream beziehungsweise gespeicherte AEGIS-Buildvorbereitung gebunden. Anfänglicher zu pauschaler Upstream-Vergleich erhalten; keine Schlüssel gelesen | `crypto-current-observation.json`, `e154e92235a2d07baf6c6f60d9fbe87eb5156dce5e31c93ae9f457f8da296c6c` |
| Technisches Linux-Konto | A auf Werks-/Privatbasis und B auf gemeinsamer Basis liefern denselben technischen POSIX-Eintrag und bytegleiche öffentliche Kontometadaten; Zuordnung zu AOSP-Identitätsquellen geprüft. Keine Shadow-/Schlüsseldateien gelesen | `two-user-technical-identity-proof.json`, `5843f9d6fe3c4e5e937d010b8d65b727be2e56ca1309d1920b4b00ce71ff71b8` |
| Begrenzte Dienstbeobachtung | Ein SystemServer-Start, keine beobachteten Fatal-Signale/Java-Fatal-/ANR-/FORTIFY-/Watchdog-Abbrüche; frühe Rückgaben und reguläre Signal-Stopps erneut quellengebunden eingeordnet | `init-exit-classification.json`, `791bf54516d21d8e161d5b92596933792e1d76835ce73ae3db470eccae8b5314` |

Vor/nach den sechs Tests bleiben Boot-ID, SystemServer-Startzeit, Enforcing
und CE `[0]` exakt gleich. Die UI-Prüfung erhält drei fehlgeschlagene anfängliche
Automationsabfragen mit leerem Root-Knoten während Seitenübergängen; nur die
Beobachtung wurde wiederholt, die Eingaben wurden jeweils einmal gesendet.
Die erste Bildschirmaufnahme zeigte noch den normalen System-Sperrbildschirm.
Die später bestätigten Einstellungsseiten sind separat erfasst.

Diese Ergebnisse schließen den Executor-Regressionsfall und die bezeichneten
Bedienungsvarianten. Der vollständige persönliche CLI-Referenzablauf,
Paketautorisierung, Persistenz, Isolation und die übrigen D1–D7-/T01–T17-Fälle
des Korrekturstands bleiben erforderlich.

## Abschlusskriterien und Referenzablauf

Für `f098f43` ist zusätzlich der Abbruch eines echten privaten Updateplans
jq/libjq1 `u3` → `u4` bei leerer Adminauswahl geprüft. Die vollständigen
Zustandsaufnahmen bleiben gleich und die Antragstellersitzung gültig.
`out/phase1-dod/f098f439/update-user-cancel-proof.json`, SHA-256
`b50b868a4e14d9f94e1d83fcfe6686c1e8804f187934a5c966175eb1ffeb135c`,
bindet Ereignisse 697 bis Präfix 700. Zusätzlich sind falsches Adminpasswort
und Nicht-Adminfreigabe mit unverändertem vollständigem Zustand belegt:
`update-user-wrong-proof.json`, SHA-256
`5b306c5147db7b697ef6d62b082329ade431e845cd28736b627cc6e363117ba3`,
und `update-user-nonadmin-proof.json`, SHA-256
`9e9fc7e573fef34902216c38998ab9fa810570b4949b5d98ae673af04e832322`.
Der vorausgehende reguläre private Installationslauf stellt nur den
Update-Ausgangsbestand her. Die gültige Alpha-Freigabe und erfolgreiche private
Aktivierung sind anschließend separat belegt:
`private-update-activation-proof.json`, SHA-256
`a928b7c364beeeb85ecae3a105f3c4e1257c522146f3a51787ccd1d2a7512cc0`,
Ereignisse 707 bis Präfix 733. Der laufende Beta-Kontext behält zunächst `u3`;
sein eigener Stopp/Start aktiviert `u4` samt expliziter privater Wahl. Alle 81
Versionen, Originaldaten und der unveränderte Alpha-Kontext sind geprüft.
Damit sind die vier Autorisierungsfälle für `update user` erfüllt;
`remove all` und die übrigen Pflichtvarianten bleiben offen. Einzelheiten im
[Abnahmeabgleich](phase-1-acceptance-progress.md).

Für `remove all` sind anschließend leere Adminauswahl und falsches
Adminpasswort mit gültiger Antragstellersitzung und vollständig unverändertem
Zustand belegt: `remove-all-cancel-proof.json`, SHA-256
`44aafe8667cd25b6ad7d45af5f74fdf8f5cdd41d81ae657f3a5fa2113a66c1c8`,
und `remove-all-wrong-proof.json`, SHA-256
`35485f9621a15105c515e9a30f5bd91cc3ab39166227151ad86a3c456e0b0d83`.
Nicht-Adminfreigabe ist ebenfalls unverändert abgewiesen:
`remove-all-nonadmin-proof.json`, SHA-256
`f7b6c35c7374da7ab656c52004c9ff55c2115a4ae964c8fc721fb6c1a155c62f`.
Gültige Alpha-Freigabe veröffentlicht die gemeinsame Entfernung, und Betas
private Aktivierung erhält alle 81 Pakete sowie Originaldaten. **Alphas
anschließender Start scheitert jedoch ohne bestätigte Aktivierung.** Er bleibt
bei gültiger AOSP-Sitzung gestoppt; ausgewählte Generationen und Betas Kontext
bleiben unverändert. Fehlerbeleg `shared-removal-alpha-failure-proof.json`,
SHA-256 `1d5ffac286d8766897491c8962b07afc461108a9ae3012873a27d9b18038eb3f`,
Ereignisse 742 bis Präfix 785. Die vollständige gemeinsame Entfernung und die
Gesamtfreigabe bleiben damit ausdrücklich offen; Ursache noch ungeklärt.

Der gezielte native Regressionstest aus Commit `5975256` ist lokal erfolgreich
kompiliert und in den unten bezeichneten Gastgruppen geprüft. Der Buildbeleg
`out/phase1-dod/5975256-removal/native-build-receipt.json` hat SHA-256
`d311e91a93d14fe7b9eaad1872817940aacf46112911e212c75151dac53e05b4`.
Nur die Testquelldatei weicht ab; zehn produktive Hilfsprogramme sind bytegleich
mit dem Referenzbuild. Dies ist kein bestandener T13-/T16-Fall und kein Nachweis
einer Fehlerbehebung.

Die sechs vorhandenen Auswahlfälle sind im separaten Profil
`3ce0c9c7-364f-4523-a496-7c785fc2d255` inzwischen bestanden, mit identischer
Boot-ID, SystemServer-Startzeit, Enforcing und System-CE vor/nach dem Lauf.
Beleg `out/phase1-dod/5975256-removal/selection/result.json`, SHA-256
`2c2b21d80782a4422cc27bb0631998ba623ec215976c71b003b32c72e2666723`.
Auch die drei danach ausgeführten Entfernungsfälle bestehen ohne übersprungene
Tests, einschließlich Ausführung, Veröffentlichung und erneutem Öffnen.
Beleg `out/phase1-dod/5975256-removal/reconciliation-removal/result.json`, SHA-256
`cb0f032fb2864ada0d62e1ab7c11d1b14014f3439c7251ffe67261d09f785b75`.
Der Systemzustand bleibt gleich. Alphas CLI-Fehler bleibt ungeklärt: Die
synthetische gemeinsame Generation enthält nach Entfernung keine Bibliothek
mehr, während der freigegebene CLI-Plan nur jq entfernt. Ein zusätzlicher Test
mit verbliebener automatischer gemeinsamer Bibliothek ist in Commit `c1663c3`
kompiliert und zusammen mit dem ursprünglichen vollständigen Fall bestanden
(2/2, keine übersprungenen Tests, identischer Systemzustand vor/nachher).
Beleg `out/phase1-dod/c1663c3-orphan/tests/result.json`, SHA-256
`78a55279e744c864b1d1a85309633b2bbc8d181fd7d88b4018d594cebe63b0b4`.
Auch diese Variante reproduziert den tatsächlichen Alpha-Fehler nicht.
Kein T13-/T16-Gesamtabschluss wird daraus abgeleitet.

## Diagnoseimage 3fdb058

Das Diagnoseimage `3fdb058310b75922b17819d47c985bc4eecd09c4` ergänzt feste
Startphasen und numerische Fehlercodes im Brokerlog. Vollbuild, 20 Images,
AVB und Basisdisk sind lokal geprüft; die Einzelbelege stehen im
[Arbeitsprotokoll](phase-1-acceptance-progress.md#diagnoseimage-erster-boot-und-drei-startfehler-kontrollen-bestanden).
Profil `88e1d3da-7d80-443f-be3f-8f2a8717b0c9` bootet mit Enforcing,
authentifiziertem ADB und allein System-CE. Boot-ID
`68ec879a-f27d-4bef-b8d5-a41c44c2e061`, SystemServer `1339/37416`.

Drei bestehende Startfehler-Kontrollen bestehen auf den exakt passenden
Komponenten, ohne übersprungene Tests und mit identischem Ausgangs-/Endzustand.
Beleg `out/phase1-dod/3fdb058-diagnostics/start-failure/result.json`, SHA-256
`7833c12c26c93e449d650fcaa963acbd045b672b92d3a874f2a9d8109449868d`.
Der neue CLI-Anfang mit zwei angelegten AOSP-Benutzern, Alphas erstem korrekten
Login und echter GNU-Ausführung ist belegt; Beta bleibt dabei gesperrt.
Originaldatei und Konfiguration sind im Anfangszustand erfasst. Beleg
`out/phase1-dod/3fdb058-diagnostics/initial-alpha-proof.json`,
SHA-256 `c8459054840d21f084dab72f412fd96d660c3a9ac2a7a907bb4de3826c3e0d4e`.
Anschließend ist die gemeinsame Installation von jq/libjq1 u3 und libonig5
einschließlich ausdrücklichem Runtime-Neustart, genauer Paketprüfung und
bytegleichem Erhalt der ursprünglichen Alpha-Daten bestätigt. Beleg
`out/phase1-dod/3fdb058-diagnostics/shared-u3-activation-proof.json`, SHA-256
`97ccef9f03226b7f45ceca8bff2b3bc8defc391e14c19449976a180703119644`.
Beta bleibt dabei gesperrt; dies belegt keinen Android-Neustart.
Danach aktiviert Alpha privat jq/libjq1 u4 bei unverändertem gemeinsamen
u3-Bestand und bytegleichem Erhalt seiner ursprünglichen Daten. Beleg
`out/phase1-dod/3fdb058-diagnostics/private-u4-activation-proof.json`, SHA-256
`d51975698692b5fd486f0952c9da27eac2f409dba5dcdcf0d9c828a1d974f5d6`.
Beta besteht seinen ersten korrekten Login und führt die gemeinsame u3-Version
tatsächlich aus; eigene Originaldatei und Konfiguration sind angelegt.
Beide Kontexte besitzen verschiedene Namespaces/Host-Zuordnungen, während
Alphas ursprüngliche Prozessidentität und private Auswahl erhalten bleiben.
Beleg `out/phase1-dod/3fdb058-diagnostics/beta-first-common-u3-proof.json`,
SHA-256 `e864f7691bc6caf7f7481065068ce192812e6bed19b6d257c2b2c957acc472bc`.
Diese Fälle ersetzen weder die vollständige gegenseitige Isolation noch
Logout-, Reboot-, Autorisierungs- und Fehlerfallmatrix.
Die eigentliche CLI-Fehlerreproduktion steht noch aus. Die früheren f098f439-Ergebnisse
werden dadurch nicht automatisch zu bestandenen Abläufen auf diesem Image;
der ursprüngliche Alpha-Fehler und die vollständige DoD bleiben offen.

### Historische Restliste des Referenzlaufs 209278de

Die folgende D1–D7-Liste und die anschließende Zuordnung der Referenzschritte
gehören zum früheren Image `209278de`; ihre E-Nummern verweisen auf dessen
Belegkatalog. Sie sind keine aktuelle Restliste für `f098f439`. Insbesondere
sind Betas späterer privater Paketbestand, zusätzliche Autorisierungsfälle und
der dritte Benutzer in den f098f439-Belegen oben dokumentiert. Ein vollständiger
aktueller Variantenabgleich bleibt erforderlich; Alphas fehlgeschlagene
Aktivierung nach gemeinsamer Entfernung bleibt dabei ausdrücklich offen.

| Kriterium | Noch erforderliche vollständige Abnahme |
| --- | --- |
| D1 | Build-/Versionsinventar, Bedienung/ADB und Schutzmechanismen vollständig indexieren; weitere Läufe prüfen. E14 bestätigt den ersten Referenzboot, E25 die begrenzte Boot-2-Beobachtung vor Shutdown. Zusätzlicher Migrations-Aufräumstatus ist in E26 eingeordnet, nicht als behoben behauptet. |
| D2 | Gesamte Benutzerverwaltung, Ablehnungen, Identitätsbindung und Passworttransport einschließlich T01/T02/T12 vervollständigen. |
| D3 | Logoutfehler und Löschung vollständig belegen; Passwortpersistenz ist mit E23/E24 und regulärer Reboot mit E13/E15/E16 nachgewiesen. |
| D4 | Abschließende Zuordnung aller Runtime-Lebenszyklusanforderungen prüfen. AOSP-Ressourcenstopp samt Wiederanmeldung/Datenvergleich ist mit E20/E22 belegt; Runtime-Stopp mit aktivem Peer in beiden Richtungen mit E17. |
| D5 | Fehlende Startvoraussetzungen und Betas privaten Paketbestand prüfen; Rechte-/Adminbegrenzung vollständig zuordnen. |
| D6 | Sechs vollständige Autorisierungskombinationen, Entfernung, Updates, Konflikte und Parallelität nachweisen. |
| D7 | Sämtliche offenen Varianten schließen; Source-/Build-/CLI-/Architektur-/Threat-Model-/Kryptographieübergabe gegen den Entwicklerauftrag prüfen. |

Referenzschritte 1–8 besitzen die oben genannten Teilbelege. Der in Schritt 2
verlangte falsche Passwortversuch ist auf diesem Image noch offen. Schritte 9
und 10 sind mit E13/E15/E16 belegt; die Schritte 11 und 12
bleiben offen. Deshalb wird auch der zusammenhängende Referenzablauf nicht als
vollständig bestanden ausgegeben.

Ein zusätzlicher Prüfkanalfehler bleibt ausdrücklich erhalten: Am 3. Oktober
um 00:11:31 UTC versuchte ein kombinierter GNU-Prüfbefehl nach erfolgreicher
Versionsausführung zusätzlich interne Paketmetadaten zu lesen. Deren Modus
0600 verweigerte diesen Zugriff. Die gesonderte gewöhnliche Programmausführung
bestand anschließend; Berechtigungen wurden nicht erweitert. E04 enthält diese
Einordnung und ersetzt den fehlgeschlagenen Beobachtungsschritt nicht still.

E16 erhält außerdem den ersten fehlgeschlagenen Paketdatenbankvergleich nach
Reboot. Der frühere lokale Beleg endet mit einer einzelnen Newline; der neue
Rohlesevorgang enthält eine zusätzliche abschließende Leerzeile. Der geprüfte
Vergleich erlaubt ausschließlich diese eine zusätzliche Newline, verlangt
ansonsten identische Datensatzbytes und speichert die vollständigen Rohdaten.
Die Produktdateien wurden nicht verändert. Beide Paketdatenbanken umfassen
weiterhin dieselben jeweils 81 installierten Pakete.
