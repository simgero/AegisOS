# Phase 1 – Ergebnisindex des aktuellen Images

Stand: 3. Oktober 2026. **Unvollständig; keine Gesamtfreigabe.**
Verbindlich bleiben sämtliche Anforderungen der [DoD](architecture/phase-1-dod.md)
und des [Entwicklerauftrags](architecture/phase-1-developer-brief.md).
Dieser Index ordnet einzelne Varianten zu. Ein bestandener Teilfall schließt
weder eine ganze T-Zeile noch ein D-Kriterium. Historische Ergebnisse stehen im
[Arbeitsprotokoll](phase-1-acceptance-progress.md); sie gelten hier nicht
automatisch für das aktuelle Image.

## Bindung und Statusregeln

- Produkt-/Image-Commit: `209278def7d5bc5612eeb397bdd8ee20ccb16d86`.
- Profil: `2366ca04-d587-4170-8c56-a63c8a8e1774`.
- Erster Boot: `44dbff5f-3a76-4e97-9334-beb437fcd461`.
- Zweiter Boot: `2816650c-96bf-4db0-84e6-f169f9dfada9`.
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
| T04.1 Gesperrtes CE | Kein Runtime-Start und kein Ersatzbetrieb | Offen: expliziter Startversuch fehlt | — |
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
| T13.2 `install user` | Gleiche vollständige Autorisierungsmatrix | Teilbelegt: gültige Alpha-Freigabe und tatsächliche Installation; drei Ablehnungsarten offen | E04 |
| T13.3 `update all` | Erlaubte Aktion und alle drei Ablehnungsarten | Teilbelegt: leere Adminauswahl beendet Plan ohne Veröffentlichung; beide aktiven Kontexte und Paketbestände unverändert. Falsche/Nicht-Adminfreigabe und erlaubte Ausführung noch offen | E28 |
| T13.4 `update user` | Erlaubte Aktion und alle drei Ablehnungsarten | Offen | — |
| T13.5 `remove all` | Erlaubte Aktion und alle drei Ablehnungsarten | Offen | — |
| T13.6 `remove user` | Erlaubte Aktion und alle drei Ablehnungsarten | Offen | — |
| T13.7 Bereich/Eigentümer | Fehlender Bereich und manipulierte Eigentümer ändern nichts | Offen | — |
| T13.8 Antragsteller ≠ Admin | Privater Bestand gehört Antragsteller; Freigabe gewährt Admin keinen persönlichen Lesezugriff | Offen: Beta-Antrag mit frischer Alpha-Freigabe erforderlich | — |
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
| T16.1 Gemeinsames Update bei privatem Bestand | Aktive Kontexte konsistent erhalten, Aktivierung ausstehend anzeigen | Offen: Installationsaktivierung ersetzt diesen Updatefall nicht | — |
| T16.2 Aktivierung nach Update | Gemeinsame Updates plus private Auswahl konsistent aktivieren oder Konflikt erklären | Offen: aktueller Systemnachweis für korrigierten früheren Fehler erforderlich | — |
| T16.3 Private Version bleibt | Keine stille Überschreibung privater Festlegungen beim Abgleich | Offen | — |
| T17.1 Gleichzeitig gemeinsam/privat | Serialisierung oder sichtbare Ablehnung, kein Teilbestand als Erfolg | Offen | — |
| T17.2 Aktionen verschiedener Benutzer | Eigentum/Autorisierung und konsistenter Bestand bleiben erhalten | Offen | — |
| T17.3 Abbruch | Letzter konsistenter Bestand bleibt erhalten/wird wiederhergestellt | Teilbelegt: Abbruch des gemeinsamen Updateplans vor Freigabe erhält beide Kontexte und Paketbestände; Ausführungsphasen bleiben offen | E28 |
| T17.4 Installationsfehler | Fehler sichtbar, keine teilweise aktivierte Umgebung als Erfolg | Offen | — |
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

## Abschlusskriterien und Referenzablauf

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
