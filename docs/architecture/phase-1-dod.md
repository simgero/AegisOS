# Phase 1 – Zieldefinition und Definition of Done

Stand: 1. Oktober 2026. **Phase 1 ist noch nicht vollständig abgenommen.**

Dieses Dokument macht den [Phase-1-Entwicklerauftrag](phase-1-developer-brief.md)
prüfbar. Es bündelt insbesondere dessen Abschnitte 17, 19 und 20; es reduziert
weder die dortigen Sicherheitsanforderungen noch ersetzt es Testergebnisse.
Die [Abnahme der fünf Server-Meilensteine](../server-acceptance.md) ist eine
Teilabnahme. Die folgenden Kästchen sind die noch offene Gesamtfreigabe, keine
Behauptung, dass sämtliche Einzelbestandteile fehlen.

## Ziel

**Phase 1 liefert einen reproduzierbar startbaren AEGIS-Entwicklungsprototyp,
in dem mehrere persönliche AOSP-Benutzer über das Terminal sicher arbeiten,
gemeinsame GNU/Linux-Software und abweichende private Paketversionen verwenden
und ihre verschlüsselten Daten über Abmeldung und Neustart hinweg behalten.**

AOSP bleibt allein zuständig für Identität, Passwortprüfung, Adminberechtigungen
und persönliche Speicherschlüssel. Die Linux-Runtime verwendet denselben Kernel
und führt keine zweite persönliche Anmeldung oder Passwortdatenbank ein.

Die Abnahme erfolgt direkt auf dem Buildserver mit ARM64-QEMU. Mac/HVF wird
später separat geprüft und blockiert diese Serverabnahme nicht. Code und
Dokumentation werden in Git versioniert und nach GitHub übertragen. Images,
Profile und umfangreiche Prüfbelege bleiben lokal; Build-Uploads erfolgen erst
für einen ausdrücklich vorgesehenen späteren Mac-Test.

## Abschlussregel

Phase 1 ist erst **Done**, wenn alle sieben Abschlusskriterien erfüllt sind,
sämtliche Pflichtszenarien der Testmatrix bestanden haben und der vollständige
Referenzablauf auf dem abgenommenen Entwicklungsimage nachgewiesen ist.

Ein implementierter Befehl, ein erfolgreicher Build, eine Anzahl grüner
Komponententests oder ein einzelner erfolgreicher Demonstrationslauf genügt
jeweils allein nicht. Ein fehlender, übersprungener oder fehlgeschlagener
Pflichttest bleibt offen. Ein sicherheitsrelevanter Fehler in einem Pflichtfall
blockiert den Abschluss. Ein erwarteter Fehlerfall zählt als bestanden, wenn
die Operation sicher abgewiesen wird und ihre zugesagten Zustände erhalten
bleiben.

## Sieben Abschlusskriterien

- [ ] **D1 – Reproduzierbares und stabiles Basissystem.** Aus einem festgehaltenen
  Git-Commit mit dokumentiertem AOSP-, Kernel- und Runtime-Stand lässt sich das
  Image bauen, prüfen und nach Anleitung in QEMU starten. Bildschirm, Eingabe
  und authentifiziertes ADB funktionieren. Android-Datenpartition und
  KeyMint-Helferzustand bilden ein zusammengehöriges persistentes Profil.
  Der Abnahmelauf enthält keine ungeklärten relevanten Dienstabstürze;
  Framework-Neustarts dürfen Fehler nicht verdecken. SELinux und die
  vorgesehenen Speicher-/Integritätsschutzmechanismen sind aktiv.
- [ ] **D2 – Vollständige AOSP-Benutzer- und Sitzungsverwaltung.** Die `aegis`-CLI
  unterstützt Anlegen, Auflisten, Entfernen, Anmeldung, Wechsel, Passwortwechsel,
  Abmeldung und Status. Namen bestimmen keine Speicheridentität; Zuordnungen
  bleiben an AOSP-Benutzer und Lebenszyklus gebunden. Falsche Passwörter,
  unberechtigte Aufrufer und manipulierte Zielidentitäten werden abgewiesen.
  Passwörter erscheinen nicht in Argumenten, History, Dateien oder Logs.
- [ ] **D3 – Geschützte und persistente persönliche Daten.** HOME, Einstellungen,
  private Pakete und deren Metadaten liegen im jeweiligen CE-Speicher.
  Korrekte Anmeldung entsperrt nur den berechtigten Zielbenutzer. Logout gilt
  erst nach Prozess-/Ressourcenabbau und bestätigter AOSP-CE-Sperrung als
  erfolgreich. Fehler bleiben sichtbar und erlauben keinen unsicheren Neustart
  der Runtime. Passwortwechsel und gepaarter VM-Neustart erhalten Daten;
  Benutzerlöschung entfernt alten Schlüsselzugriff und private Zuordnungen.
- [ ] **D4 – Nutzbare GNU/Linux-Runtime mit getrenntem Lebenszyklus.** Eine
  gemeinsame Debian-/Ubuntu-Basis mit glibc, bash, apt und GNU-Werkzeugen ist
  über `linux start`, `shell`, `stop` und `status` nutzbar. Jeder persönliche
  Kontext besitzt eigenes HOME, eigene Konfiguration und private flüchtige
  Verzeichnisse. Shell-Ende, Runtime-Stopp, Wechsel, Bildschirmsperre,
  AOSP-Ressourcenstopp und Logout haben ihre jeweils dokumentierte Wirkung.
- [ ] **D5 – Nachgewiesene Benutzerisolation.** Beide Kontexte sehen intern
  UID/GID 1000, besitzen aber getrennte Host-Zuordnungen und die erforderlichen
  Namespaces. Sie können weder fremde private Daten lesen/verändern noch fremde
  Prozesse oder IPC beeinflussen. Das gilt auch bei gleichzeitig entsperrten
  Benutzern. Fehlende Isolation verhindert den Start. Runtime-Rechte ergeben
  keine Host- oder AOSP-Adminrechte; gemeinsame Software ist für normale
  Runtime-Prozesse schreibgeschützt.
- [ ] **D6 – Vollständige Paketverwaltung.** Installation, Aktualisierung und
  Entfernung funktionieren für die expliziten Bereiche `all` und `user`,
  jeweils mit AOSP-Adminautorisierung. Private Pakete gehören dem authentifizierten
  Antragsteller, auch wenn eine andere Person die Adminfreigabe erteilt.
  Unterschiedliche Versionen desselben Pakets samt Abhängigkeiten funktionieren
  getrennt. Gemeinsame Änderungen, Aktivierung, private Entfernung, Konflikte,
  Parallelität und Abbruch erhalten einen konsistenten Paketbestand.
- [ ] **D7 – Vollständige, nachvollziehbare Abnahme und Übergabe.** Alle folgenden
  Pflichtszenarien und der Referenzablauf sind automatisiert beziehungsweise
  reproduzierbar ausgeführt und mit dem getesteten Stand verknüpft. Source,
  Build-/Startanleitung, CLI-Dokumentation, Architektur, Threat Model,
  Kryptographieentscheidungen und ein vollständiger Abnahmebericht liegen vor.
  Ausstehende Pflichtfälle werden nicht als bloße Prototypgrenzen ausgeklammert.

## Verbindliche Testmatrix

Die Zeilen T01–T17 entsprechen den 17 Bereichen aus Abschnitt 17 des
Entwicklerauftrags. Jede Zeile umfasst alle genannten Varianten; ein einzelner
bestandener Teilfall schließt die Zeile nicht ab.

| ID | Pflichtszenario | Bestehenskriterium |
| --- | --- | --- |
| T01 | Benutzer und Authentifizierung | Zwei persönliche AOSP-Benutzer anlegen/auflisten; falsches Passwort ohne Entsperrung/Runtime-Start ablehnen; korrektes Passwort entsperrt nur den Zielbenutzer. Keine parallelen persönlichen Linux-Konten oder Passwortspeicher. Passworttransport und relevante Dateien, Argumente, History und Logs auf Offenlegung prüfen. |
| T02 | Passwortwechsel | Neues Passwort funktioniert nach erneuter Sperrung und Neustart; altes Passwort scheitert. Vorher angelegte Dateien bleiben unverändert verfügbar, ohne vollständige Neuverschlüsselung. |
| T03 | Identität und Namespaces | Intern jeweils UID/GID 1000, getrennte Host-UID/GID-Bereiche und User-, Mount-, PID- und IPC-Namespaces tatsächlich beobachten. Keine unübersetzte Host-UID 1000. Zuordnung bleibt bei Benutzerwechsel erhalten. |
| T04 | Startbedingungen | Gesperrtes CE, fehlende Namespaces, ungültiges Mapping und fehlende Sicherheitsvoraussetzungen jeweils gezielt prüfen: kein Runtime-Start und kein schwächerer Ersatzbetrieb. |
| T05 | Shell-Zugang | Echte GNU-Befehle im eigenen Kontext ausführen; HOME und Identität stimmen. Fremden/gesperrten Kontext ablehnen. `exit` beendet nur die Shell; Runtime und AOSP-Sitzung bleiben bestehen. |
| T06 | Gegenseitiger Zugriff | Aus beiden gewöhnlichen GNU-Kontexten Zugriffe auf fremde CE-Dateien, HOME, Konfiguration, Pakete, temporäre Dateien, Prozesse, IPC und Test-Secrets versuchen. Kein fremder Inhalt, keine Veränderung und keine Prozessbeeinflussung, auch wenn beide Benutzer entsperrt sind. |
| T07 | Persistenz | Dateien, Konfiguration und private Pakete nach Runtime-Neustart und vollständigem VM-Neustart unverändert wiederfinden; Einstellungen beider Benutzer bleiben getrennt. Alte Inhalte von `/tmp` und `/run` sind nach Kontextneustart verschwunden. |
| T08 | Wechsel, Bildschirmsperre, Ressourcenstopp | Hintergrundprozesse behalten Identität und dürfen nach AOSP-Regeln weiterlaufen. Bildschirmsperre und Wechsel behaupten keinen CE-Schlüsselentzug. Stoppt AOSP einen Hintergrundbenutzer, werden auch dessen Runtime-Ressourcen abgebaut. |
| T09 | Runtime-Stopp | `linux stop` beendet nur den eigenen Kontext samt flüchtigen Ressourcen. AOSP-Sitzung und tatsächlicher CE-Zustand bleiben korrekt angezeigt; kein behaupteter Logout, keine Beeinträchtigung anderer Kontexte. |
| T10 | Erfolgreicher Logout | Eigene Prozesse, offene Zugriffe, Mounts und IPC verschwinden; AOSP bestätigt CE-Sperrung. Bekannte persönliche Dateien liefern ohne erneute Anmeldung keine Bytes. Systembenutzer und andere Sitzungen bleiben erhalten. Vordergrundwechsel und konkurrierender Start umgehen die Sperrung nicht. |
| T11 | Logout-Fehler | Ausstehende/fehlgeschlagene CE-Sperrung provozieren: kein falscher Erfolg, keine neue Freigabe, kein verdeckter Framework-Neustart. Nach Beheben der Ursache sichere Wiederherstellung und frische Authentifizierung. Laufende Paketaktionen hinterlassen keinen unbemerkten persönlichen Zugriff. |
| T12 | Reboot und Benutzerlöschung | Profilpaar vollständig stoppen/starten; persönliche Daten bleiben vor Anmeldung gesperrt und danach erhalten. Über die CLI einen Benutzer löschen: Schlüsselzugriff, Runtime und privater Zustand entfernt. Neuer Benutzer und kontrollierte Wiederverwendung einer ID erhalten keinen Zugriff auf alte Daten/Zuordnungen. |
| T13 | Paketautorisierung | Für `install`, `update`, `remove` jeweils `all` und `user` prüfen: gültige Adminfreigabe erlaubt; fehlende, falsche oder Nicht-Adminfreigabe verweigert. Fehlender Bereich und manipulierte Eigentümerangaben ändern nichts. Die Freigabe erteilt dem Admin kein Leserecht auf persönliche CE-Daten des Antragstellers und entsperrt keinen unbeteiligten Benutzer. |
| T14 | Paketbereiche | Gemeinsame Software funktioniert bei bestehenden und nachträglich angelegten Benutzern. Persönliche Installationen/Änderungen bleiben privat; persönliche Einstellungen bleiben auch bei gemeinsamem Programm getrennt. |
| T15 | Private Versionen | Dasselbe Paket in unterschiedlichen expliziten Versionen mit passenden Abhängigkeiten tatsächlich ausführen; private Version hat Vorrang. Nicht verfügbare/unauflösbare Version ohne stille Ersetzung ablehnen. Private Entfernung einschließlich ausgewiesener Rückkehr zur gemeinsamen Variante konsistent prüfen. |
| T16 | Paketaktivierung | Gemeinsames Paket aktualisieren, während private Pakete/Versionen bestehen. Laufende Kontexte bleiben konsistent; Status zeigt ausstehende Aktivierung. Neuer Start aktiviert einen konsistenten Stand oder meldet einen erklärten Konflikt; private Versionen werden nicht still überschrieben. |
| T17 | Paketfehler und Parallelität | Gleichzeitige gemeinsame/private Aktionen und Aktionen verschiedener Benutzer, Abbruch, Installationsfehler sowie Logout während einer Transaktion prüfen. Keine teilweise aktivierte Umgebung als Erfolg; letzter konsistenter Bestand bleibt erhalten oder wird wiederhergestellt. Fehler und Reparaturbedarf sind sichtbar. |

Bei T13 sind mindestens sechs Kombinationen aus Aktion und Bereich abzudecken,
jeweils mit erlaubter und verweigerter Autorisierung. Ein erfolgreicher
`install --scope user` belegt kein `update` oder `remove`.

Bei T15 genügt die Kombination „gemeinsames ed, privates hello“ ausdrücklich
nicht: Für die Versionsanforderung muss es **dasselbe Paket** sein. Paketquellen,
verwendete Versionen und Abhängigkeiten müssen vor dem Test festgehalten werden.

## Zusammenhängender Referenzablauf

Der Ablauf beginnt mit dokumentiertem Build-/Startverfahren und einem frischen,
nicht vorab mit persönlichen Benutzern oder Paketen präparierten Profilpaar.
Synthetische Benutzernamen und zufällige Testpasswörter sind zulässig. Mindestens
A ist AOSP-Admin; B ist ein normaler persönlicher Benutzer.

1. Android und KeyMint-Helfer starten; Boot, aktive Schutzmechanismen,
   Bildschirm, Eingabe und authentifiziertes ADB bestätigen.
2. A und B über AEGIS/AOSP anlegen, verschiedene Passwörter setzen und Identitäten
   festhalten. Ein falsches Passwort darf den Zielbenutzer nicht entsperren.
3. A korrekt anmelden, Runtime starten, GNU-Shell öffnen, Datei und persönliche
   Konfiguration erzeugen. Die Shell verlassen und fortbestehende Sitzung sowie
   Runtime anhand derselben Prozessidentität prüfen.
4. Ein Paket in Version V1 mit Adminfreigabe gemeinsam installieren, dasselbe
   Paket in Version V2 nur für A installieren. Aktivierungsstatus und gegebenenfalls
   notwendigen Kontextneustart prüfen; A führt V2 mit konsistenten Abhängigkeiten aus.
5. Zu B wechseln und B authentifizieren. A wird dadurch nicht abgemeldet und
   behält seine Prozessidentität im zulässigen Hintergrundbetrieb. B startet
   seinen eigenen Kontext und führt die gemeinsame Version V1 aus.
6. Getrennte Host-Zuordnungen trotz gleicher interner UID/GID sowie gegenseitige
   Datei-, Prozess- und IPC-Isolation aus echten GNU-Prozessen prüfen. B erzeugt
   eigene Datei und Konfiguration; A sieht weiterhin ausschließlich seine Daten.
7. Bildschirmsperre und zulässigen Hintergrundbetrieb getrennt vom Logout
   prüfen. Danach B ausdrücklich abmelden: Prozesse/Ressourcen weg, CE-Sperrung
   bestätigt, bekannte private Daten unlesbar; A bleibt davon unberührt.
8. Zu A zurückkehren, Datei und Konfiguration unverändert lesen. A abmelden und
   dessen bestätigte CE-Sperrung sowie Ressourcenabbau prüfen.
9. Android und KeyMint-Helfer vollständig stoppen und mit demselben Profilpaar
   neu starten. Neue Boot-ID, unveränderte Profilbindung; vor Anmeldung bleiben
   beide persönlichen Speicher gesperrt.
10. A und B jeweils korrekt anmelden. Dateien/Konfiguration unverändert sowie
    private Version V2 bei A und gemeinsame V1 bei B tatsächlich nachweisen.
    Der erste korrekte Login darf keinen vorherigen Fehlversuch benötigen.
11. Einen weiteren Benutzer C anlegen: Gemeinsame Software ist verfügbar,
    private Daten und private Paketversionen von A/B sind nicht zugänglich.
12. Die ergänzenden Pflichtfälle aus T01–T17 durchführen, insbesondere
    Passwortwechsel, Löschung/ID-Wiederverwendung, Updates, Entfernung,
    Konflikte, verweigerte Freigaben und konkurrierende Vorgänge. Destruktive
    Testfälle dürfen dafür gesonderte frische Profile verwenden.

Der Referenzablauf ersetzt die Testmatrix nicht. Ein bestandener Hauptablauf
bei offenen Negativ-, Versions- oder Parallelitätstests bedeutet weiterhin
„Teilabnahme“.

## Erforderliche Nachweise und Übergabe

Für die endgültige Abnahme werden abgelegt:

- Source-Commit, AOSP-Manifest, Kernel-/Runtime-Versionen, Buildkonfiguration,
  Image-Prüfsummen und reproduzierbare Build-/Startbefehle.
- Testskripte und ein Ergebnisindex mit T01–T17, den einzelnen Varianten,
  erwarteter/tatsächlicher Wirkung, Ergebnis, Belegpfad und Prüfsumme.
- Zuordnung zu Image, Profil-ID, Benutzer-ID/Seriennummer und Boot-ID;
  relevante Prozessidentitäten enthalten Startzeit beziehungsweise Bootbezug,
  damit PID-Wiederverwendung keinen Überlebensnachweis vortäuscht.
- Positive GNU-Ausführungen und negative Zugriffstests aus gewöhnlichen
  Runtime-Prozessen. Entwicklungs-root darf beobachten und gezielt Fehler
  injizieren, ersetzt aber keine erfolgreiche Benutzeranmeldung oder
  Isolation aus Sicht eines normalen Prozesses.
- AOSP-CE-Zustand und tatsächliche Unlesbarkeit bekannter Testdaten vor
  Anmeldung/nach Logout; nach Anmeldung bytegleicher Inhalt. Prozessende oder
  Unmount allein gelten nicht als Nachweis des Schlüsselentzugs.
- Paketversionen, ausgeführte Programme, Paketmetadaten und Aktivierungszustand
  vor/nach Aktionen; ein CLI-Erfolgstext allein belegt keine konsistente Installation.
- CLI-Anleitung einschließlich Admin-Ersteinrichtung, Benutzer-/Paketaktionen,
  Fehlerzuständen und sauberem gepaarten Stop/Start sowie Architektur,
  UID/GID-Mapping, Kryptographieentscheidungen und Threat Model mit den zwölf
  Szenarien aus Abschnitt 16 des Entwicklerauftrags. Die Kryptographiedokumentation
  nennt verwendete Verfahren, Modi, Schlüssellängen, AOSP-Ableitungs-/Schutzpfade,
  Zufallsquellen und Hardwareabhängigkeiten.

Pflicht-Integrationstests müssen zum final abgenommenen Image passen. Ältere
Komponententests dürfen mit nachgewiesen identischem Quellstand ergänzend
referenziert werden; sie ersetzen keine fehlenden Systemabläufe. Änderungen
mit Einfluss auf ein bestandenes Kriterium erfordern dessen erneute Prüfung.

Logs und Berichte enthalten keine Passwörter oder Schlüssel. Lokale Belege
müssen auffindbar bleiben; GitHub enthält die Dokumentation und Testwerkzeuge,
keine privaten Profile oder ungeprüften Geheimnisse. Die Vertrauensgrenzen
von Software-TPM, Entwicklungs-root und kontrollierendem Host sind ausdrücklich
benannt. Schutz gegen einen vollständig kompromittierten Host und eine
tatsächlich nicht vorhandene Hardware-Vertrauenswurzel werden nicht behauptet.

## Abgrenzung und aktueller Ausgangsstand

Phase 1 umfasst keinen eigenen Desktop, Launcher, grafischen Login, App Store,
Cloud-/Backup-Dienst, biometrischen Login, Smartphone-Telefonie oder eigene
Hardware. Die Vorbereitung für spätere Grafikarbeit ändert diese DoD nicht.
Eine Produktions-Sicherheitszertifizierung, Mac/HVF-Abnahme sowie eine zusätzliche
Stromausfall-/Imagemigrationskampagne sind nicht Voraussetzung dieser
Serverabnahme. Die oben verlangten Paketabbrüche und Fehlerfälle bleiben Pflicht.

Bereits belegt sind wesentliche Kernabläufe: QEMU-Bedienung und ADB, zwei Benutzer,
AOSP-Anmeldung/Passwortwechsel, GNU-Ausführung, gemeinsame und private
Installation, gegenseitige Isolation, CE-Fehlerwiederherstellung und gepaarter
Neustart mit Dateierhalt. Siehe [Serverabnahme](../server-acceptance.md).

**Noch offen ist die vollständige Zuordnung und Abnahme aller Pflichtfälle.**
Insbesondere fehlen durchgängige Nachweise für abweichende Versionen desselben
Pakets, gemeinsame Updates bei privaten Versionen, die vollständige
Installations-/Update-/Entfernungsmatrix, konkurrierende Transaktionen,
Benutzerlöschung/ID-Wiederverwendung sowie die ergänzenden Lebenszyklusfälle
und den nachträglich angelegten Benutzer. Vorhandene Komponententests sind dabei
zu berücksichtigen; fehlende Systemnachweise bedeuten nicht automatisch,
dass die jeweilige Funktion noch nicht implementiert ist.

Die abschließende Freigabe lautet erst nach Erfüllung aller Kriterien:
**„Phase 1 auf dem Buildserver vollständig abgenommen“**, mit Source-/Image-Stand,
Datum und verlinktem Ergebnisindex. Bis dahin lautet der Status
**„Phase 1 in Umsetzung – Kernprototyp teilabgenommen“**.
