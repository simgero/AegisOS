# Phase 1: Implementierungsstand

**Gesamtstatus: Phase 1 in Umsetzung – Kernprototyp teilabgenommen.**
Die [Zieldefinition und DoD](architecture/phase-1-dod.md) legt die vollständigen
Abschlusskriterien fest. Die fünf nachstehenden Server-Meilensteine erfüllen
noch nicht die gesamte Pflicht-Testmatrix des Phase-1-Auftrags.

## 2026-10-01: Serverabnahme der fünf Terminal-Meilensteine

Die [aktuelle Abnahme](server-acceptance.md) bestätigt im Image `c526571`
den Zwei-Benutzer-Ablauf mit gemeinsamer/privater Software, CE-Fehlerwiederanlauf,
Isolation und vollständigem Android-/KeyMint-Neustart. Der unten beschriebene
CE-Absturz ist darin korrigiert und gezielt erneut geprüft. Entwicklung und
Tests erfolgen nach neuer Nutzeranweisung direkt auf dem Buildserver, mit
lokalen Commits und ohne GitHub-Build-Upload. Die folgenden Abschnitte bleiben
als historischer Diagnoseverlauf erhalten.

## 2026-10-01: Main-Zusammenführung und offener Fehler nach CE-Timeout

Die Zusammenführung nach `main` ist ein Entwicklungsstand, keine vollständige
Abnahme von Phase 1. Das Vollimage `1b1a0a9a` wurde über GitHub verifiziert und
im lokalen Mac-QEMU mit Enforcing, FBE und dm-verity gestartet. Reguläre
Abmeldung, unlesbare private GNU-Datei im gesperrten Zustand, abgewiesenes
falsches Passwort und bytegleicher Inhalt nach korrekter Anmeldung sind geprüft.

Eine diagnostisch außerhalb der persönlichen Runtime geöffnet gehaltene Datei
verzögert die Abmeldung korrekt: Während des offenen Dateideskriptors blieb die
Erfolgsmeldung aus; nach dessen explizitem Schließen bestätigte vold den
Schlüsselentzug. Der Test mit 45 Sekunden Haltezeit überschritt hingegen die
zehnsekündige Sperrfrist. AEGIS meldete keinen Erfolg und die persönliche Runtime
war abgebaut. Der zwischengespeicherte CE-Zustand blieb jedoch entsperrt.
Beim anschließenden Anmeldeversuch brach der CLI-Dienstkontakt ab; danach wurde
ein anderer `system_server`-Prozess bei unveränderter Kernel-Boot-ID beobachtet.
Die Ursache dieses Neustarts und eine sichere Wiederherstellung ohne Neustart
sind noch zu beheben und erneut im Gast zu prüfen.

Lokale Nachweise im primären Workspace:
`out/full-build-1b1a0a9a/identity-test/normal-ce-cycle/`, `held-short-1/` und
`held-long-1/` unter demselben `identity-test/`-Verzeichnis. Diese Fehlerprüfung
ist kein bestandener Timeout-/Wiederanlaufnachweis. Die älteren Abschnitte
beschreiben den jeweiligen damaligen Entwicklungsstand.

## 2026-09-30: Konfigurierte Paketvorbereitung im neuen Vollimage geprüft

`178cbb6e` besteht **366/366 aktivierte native Tests** im passenden neuen
Mac-QEMU-Image. Die interne Vorbereitung verwendet ausschließlich den gehaltenen
Auftrag und beim Start gepinnte Helfer. Installation, Entfernen, Abbruch mit
erneutem Versuch und verschwundene Eingaben sind geprüft. Ein eigener
SELinux-Typ trennt beschreibbare Vorbereitungsdateien von veröffentlichten
schreibgeschützten Generationen. [Nachweise und Grenzen](component-tests.md).

Die Basis wurde nach der Änderung des Kennungsrezepts auf dem Buildserver
neu erzeugt und geprüft; der alte Basiseingang wurde korrekt abgewiesen.
Das neue Image bootet mit FBE, dm-verity, authentifiziertem ADB und Enforcing;
der reale Broker läuft in seiner erwarteten Domäne. Das neue Testprofil besitzt
nur Android-Benutzer 0; dessen Identitäten, CE- und Schlüsselzuordnungen bleiben
unverändert. Der bisherige c740-Gast und die anderen Profilpaare bleiben erhalten.

Als Nächstes fehlen begrenzte Bereinigung der Vorbereitungsdateien, produktive
Pakethelfer/SELinux-Aktivierung und öffentliche Paketbefehle mit frischer
AOSP-Adminbestätigung. Danach ist der vollständige Zwei-Benutzer-Ablauf nachzuweisen.
Vier reale CE-Tests bleiben deaktiviert; die unveränderten 130 Java-Tests wurden
hier nicht erneut ausgeführt.

Auf Nutzerwunsch wurden weitere ersetzte Test-Binärdateien und die nach
Prüfung/Entpacken doppelte Image-Downloadkopie entfernt (zusammen rund 1,94 GiB).
Quellcode, aktuelle Komponenten, vorbereitete Images und Testnachweise bleiben
vorhanden. Belege: `out/cleanup-failed-components-20260930.json` und
`out/cleanup-verified-image-archive-20260930.json` im primären Workspace.

## 2026-09-30: Paketvorbereitung kollidiert nicht mehr mit der Runtime

`5427b789` besteht **354/354 aktivierte native Tests**. Paketarbeiter erhalten
getrennte cgroups unter demselben begrenzten Gesamtbudget. Runtime-Auswahl,
gemeinsamer und persönlicher Paketauftrag sind zweckgebunden; ein Paketabbruch
übernimmt oder beendet keine Runtime-Auswahl. Sechs neue Tests umfassen die
Koexistenz mit einem echten isolierten Prozess derselben Identität sowie die
fest verankerte gemeinsame/persönliche Quellenauswahl. [Nachweise](component-tests.md).

Die Komponententests verändern keine realen Benutzer, Schlüssel oder das
laufende Produktimage. Die öffentliche CLI/Binder-Paketstrecke mit frischer
AOSP-Adminbestätigung, feste Quellen/Schlüssel/TLS-Eingaben und SELinux-Einbindung
müssen noch folgen. Anschließend sind Vollimage und vollständiger
Zwei-Benutzer-Ablauf einschließlich angemeldeter offener Shell nachzuweisen.
Vier deaktivierte reale CE-Tests bleiben offen.

## 2026-09-30: Echter Debian-Abruf mit Zertifikats-, Signatur- und Hashprüfung

`83744d58` besteht **54/54 gezielte und 348/348 aktivierte native Tests** im
lokalen QEMU. Der intern gehaltene Planer lädt `hello` aus Debian über HTTPS,
prüft Quellenfreigabe, TLS, Signatur, Ablaufdatum, vollständigen Paketindex und
Archivbytes. Ungültige Zertifikate, fremde Ziele und Abbrüche sind geprüft.
Der Netzwerkhelfer gehört demselben begrenzten Auftrag; gewöhnliche Runtime-
und Installationsumgebungen erhalten keinen Zugang zu seinem privaten Listener.
[Nachweise und Grenzen](component-tests.md), [Datenweg](package-network.md).

Als Nächstes folgen die Produktbereitstellung von Quellen/Schlüsseln/TLS-Bündel,
SELinux-Einbindung und der öffentliche CLI/Binder-Paketweg mit frischer
AOSP-Adminbestätigung; danach der gesamte Zwei-Benutzer-Ablauf. Dies ist noch
keine freigeschaltete Paketverwaltung. Produktgast, Profilpaar, Benutzer und
Schlüssel bleiben unverändert. Die vier deaktivierten AOSP-CE-Fälle bleiben offen.

## 2026-09-30: Planung und Paketinstallation im selben Auftrag verbunden

`b827ba82` besteht **46/46 gezielte und 340/340 aktivierte native Tests**.
Ein Plan aus signierter Quelle durchläuft nun intern Auswahl, Review, Vorbereitung,
Installation, unabhängige Prüfung und Veröffentlichung unter derselben Kennung.
Der Test öffnet den vollständigen neuen Bestand erneut und prüft Versionen sowie
Abhängigkeitsmarkierungen. Veränderte Freigabe-Digests werden abgewiesen;
Abbruch entfernt auch bereits geprüfte Pläne. [Nachweise](component-tests.md).

Offen bleiben kontrollierter Internetabruf, die öffentliche CLI/Binder-Verbindung
mit echter frischer AOSP-Adminbestätigung und Produkt-SELinux-Einbindung, anschließend
der vollständige Zwei-Benutzer-Ablauf. Der interne Test verwendet eine signierte
Offline-Quelle und ersetzt keinen dieser Produktnachweise. Laufender QEMU-Gast,
Benutzer, Schlüssel und sichtbarer Launcher bleiben unverändert.

## 2026-09-30: Paketplanungsauftrag besitzt Arbeiter und Ergebnis bis zum Abbruch

`33482ef2` besteht **43/43 gezielte und 337/337 aktivierte native Tests**.
Die verifizierte Auswahl wird unter derselben Auftragskennung an den isolierten
Planer übergeben. Auch das fertige Ergebnis bleibt bis zur Weiterverarbeitung
oder zum Abbruch im Broker registriert. Abmeldung/STOP, Fristen, Teilfehler und
Folgeaufträge sind geprüft. [Belege und Grenzen](component-tests.md).
Benutzer, Schlüssel, Produktgast und sichtbare Version bleiben unverändert.

Offen bleiben kontrollierter produktiver Netzwerkabruf, Bindung des gehaltenen
Ergebnisses an den Plan samt erneuter Gültigkeitsprüfung, frische AOSP-Adminfreigabe
und öffentliche CLI/Binder-Befehle. Der Zwei-Benutzer-Produktablauf ist noch offen.

Auf Nutzerwunsch wurden anschließend weitere 70 überholte lokale Komponenten-
kopien (rund 0,39 GiB belegte Blöcke) entfernt. Aktueller Komponentenstand und
vorheriger geprüfter Vergleichsstand, Quellcode, Berichte, Launcher sowie alle
fünf benötigten Profile bleiben erhalten. Beleg:
`out/cleanup-planner-copies-20260930.json`.

## 2026-09-30: Paketplan mit Release-/Index-/Archivbelegen verbunden

`9e705281` besteht **34/34 gezielte und 328/328 aktivierte native Tests**.
Der unveränderliche APT-Planer prüft die bereits authentifizierten Release-
Metadaten, vollständigen Paketlisten und heruntergeladenen Archive zusammen
und übergibt diese Belege an die vorhandene Bindung des Freigabeplans.
[Nachweise und Grenzen](component-tests.md). Benutzer, Schlüsselzuordnungen,
aktueller QEMU-Gast und sichtbare Version bleiben unverändert.

Als Nächstes fehlen die produktive Netzwerk-/Besitzverwaltung des Planungsauftrags,
CE-/Abbruchintegration und erneute Fristprüfung vor der frischen AOSP-Adminfreigabe,
danach die öffentliche CLI/Binder-Verbindung. Der gesamte Zwei-Benutzer-Ablauf
mit gemeinsamer und persönlicher Paketverwaltung ist weiterhin nicht abgeschlossen.

## 2026-09-30: Netzwerk im bestehenden QEMU-Profil geprüft

Der optionale Netzwerkmodus des Launchers verbindet den Android-Testgast nach
geordnetem Neustart automatisch mit DHCP, DNS und der Debian-Quelle (HTTP 200).
Das bestehende Profil, Benutzer und Schlüsselzuordnungen bleiben erhalten;
der KeyMint-Helfer bleibt ohne Netzwerk. Eine veraltete ADB-Prozessnummer nach
Neustart wird jetzt verworfen, ohne den fremden Android-Prozess zu beenden.
**34/34 passende native Regressionen bestehen** unter dem Netzwerkmodus.
[Konfiguration, Nachweise und Grenzen](persistent-qemu.md#optionales-android-netzwerk-und-adb-neustart-30-september-2026).

Aktueller Hintergrundgast: `out/qemu-network-20260930/boot-2`, Boot-ID
`6c6dc3df-cf70-4c1b-8f16-f131b5a981a5`, unverändertes Produktimage `c7401f60`.
Die sichtbare Version bleibt unverändert. Nächster Schritt ist weiterhin die
produktive, kontrollierte Paketbeschaffung samt Auftrags-/CE-Lebenszyklus und
unabhängig geprüften Repository-Belegen; anschließend CLI/Binder und frische
AOSP-Adminfreigabe. QEMU-Konnektivität allein aktiviert keine Paketverwaltung.

## 2026-09-30: Paketplanung aus unveränderlicher Werksbasis geprüft

`8e500d7e` besteht **9/9 gezielte und 322/322 aktivierte native Tests** im
lokalen QEMU. Der neue Planer verwendet ausschließlich Werksprogramme und feste
schreibgeschützte Richtlinien, übernimmt aber den ausgewählten Paketstatus.
Manipulierte Programme der ausgewählten Generation beeinflussen ihn nicht;
ungültige Quellen und veränderte Paketdaten werden abgewiesen.
[Belege und Grenzen](component-tests.md).

Als Nächstes fehlen die produktive Planungs-/Netzbeschaffungshülle mit
Auftrags-/CE-Lebenszyklus und unabhängig gebundenen Repository-Belegen, danach
öffentliche CLI/Binder-Verbindung samt frischer AOSP-Adminfreigabe. Vollständiger
Zwei-Benutzer-Produktablauf bleibt offen; keine Produktaktivierung in diesem Schritt.

## 2026-09-30: Paketarbeiter geprüft, Produktverbindung als nächster Schritt

Komponentenstand `cacb1718` besteht **23/23 gezielte Prüfungen** und danach
**313/313 aktivierte native Tests** im lokalen QEMU. Installation, Update und
Entfernung übernehmen jetzt die freigegebenen automatischen Paketmarkierungen;
die unabhängige Prüfung vergleicht Simulation und endgültigen Bestand mit dem
versiegelten Plan. Die drei vorherigen Fehler sind behoben. Vier deaktivierte
AOSP-Integrationstests bleiben gesondert; Java wurde nicht erneut ausgeführt.
[Belege, Grenzen und nächster Schritt](component-tests.md).

Offen bleiben der produktive unveränderliche Planer mit Quellen-/Archivabruf,
CLI/Binder und frische AOSP-Adminfreigabe sowie der vollständige Paketablauf mit
zwei Benutzern im Produktimage. Der laufende Gast `c7401f60` samt Benutzern,
Schlüsseln und sichtbarem Launcher ist unverändert. Das Gesamtziel bleibt offen.

Nach ausdrücklicher Freigabe wurden zusätzlich 28 überholte QEMU-Profile und
54 ausschließlich dazugehörige Eingabedateien entfernt: 25,8 GiB tatsächlich
freigegeben. Fünf benötigte Profile bleiben erhalten (sichtbarer Stand, aktueller
Testgast, drei Vergleichsstände). Beleg: `out/obsolete-profiles-cleanup-20260930.json`.
Die unten dokumentierten 33 Profile beschreiben den früheren Bereinigungsstand.


## 2026-09-30: Paket-Ausführungsprüfung in Arbeit; lokaler Buildcache bereinigt

Komponentenstand `207d7aa7` kompiliert, besteht aber erst **15/18** gezielte
QEMU-Prüfungen. Die native Bestandsprüfung besteht; positive Ausführung und
gebundene Publikation brauchen noch Korrekturen. Keine Produktaktivierung.
[Fehler, Belege und nächste konkrete Schritte](component-tests.md).

Auf ausdrücklichen Nutzerwunsch wurden 1.595 entbehrliche lokale Build-/
Testkopien entfernt. Tatsächlich frei wurden rund 33,7 GiB; anschließend waren
rund 39,8 GiB verfügbar. Quellcode, Testberichte, alle 33 Profilverzeichnisse,
Schlüssel und vorhandene Profilabhängigkeiten bleiben erhalten. Aktueller
Testgast und sichtbarer Launcher sind vollständig vorhanden; frühere bereits
fehlende Abhängigkeiten alter Profile wurden nicht zur Löschung ausgewählt.
Lokaler Einzeldateinachweis: `out/cleanup-unused-20260930.json`.


## 2026-09-30: automatische Abhängigkeiten an den Freigabeplan gebunden

Komponentenstand `6caa5bf2` besteht **76/76 gezielte Gerätetests**. Tatsächlich
verifizierte Archive, automatische/manuale Markierungen und ursprünglicher APT-
Zustand gehören jetzt zu demselben internen Plan-Digest. Ein veränderter
Abhängigkeitsgrund wird beim Start eines bereits vorbereiteten Auftrags abgewiesen.
[Belege und Grenzen](component-tests.md).

Noch fehlen die tatsächliche Übernahme dieser Markierungen im Paketarbeiter,
der unabhängige Vergleich seiner Effekte mit dem Plan und der produktive
Planungs-/Beschaffungsweg samt frischer AOSP-Adminfreigabe. Die öffentliche
Paketverwaltung bleibt inaktiv; Produktgast, Benutzer und Profilpaare bleiben erhalten.

## 2026-09-30: signierte Paketquelle bis zu den Archivbytes geprüft

Komponentenstand `d4147584` besteht **67/67 gezielte Prüfungen** im lokalen
Hintergrund-QEMU. APT weist veränderte Archive gleicher Größe zurück und übernimmt
die unveränderten Originale. AEGIS ordnet die exakt aufgelösten Versionen dem
signaturbestätigten Index zu und prüft die heruntergeladenen Dateien nochmals
unabhängig. Automatische Abhängigkeiten bleiben in diesem Adapter erhalten.
[Nachweis einschließlich der behobenen Testfehler und Grenzen](component-tests.md).

Die öffentliche Paketverwaltung bleibt inaktiv. Produktiver Netzabruf, der
unveränderliche Planer mit Auftragslebenszyklus, automatische Markierungen im
Freigabe-/Ausführungspfad und frische AOSP-Adminfreigabe sind noch zu verbinden.
Der Produktgast bleibt `c7401f60`; Benutzer, Schlüssel und Profilpaare sind erhalten.
Das QEMU-Fenster bleibt geschlossen.

## 2026-09-30: echte APT-Auflösung gegen signierte Testquellen

Komponentenstand `322ac8a2` besteht **53/53 gezielte Prüfungen** im lokalen
Hintergrund-QEMU. Echte APT-Auflösung ermittelt exakte Abhängigkeiten und abhängige
Entfernungen. Unsignierte, veränderte und abgelaufene Testquellen werden abgewiesen;
Simulationen verändern weder Paketstatus noch führen sie Installationsskripte aus.
[Nachweis und Grenzen](component-tests.md).

Die öffentliche Paketverwaltung ist noch nicht aktiviert. Produktive
Quellen-/Archivprüfung, unveränderlicher Planer, Lebenszyklus, automatische
Paketmarkierungen und die Verbindung mit frischer AOSP-Adminfreigabe fehlen noch.
Der Produktgast bleibt `c7401f60`; vorhandene Benutzer und Profilpaare sind erhalten.


Stand: 29. September 2026. **Das vollständige Phase-1-Ziel ist nicht erreicht.**
Entwicklung erfolgt lokal, Kompilierung auf `aegis-build`, Systemtests in lokalem
Mac-QEMU und Quell-/Artefakttransport über GitHub.

Für die Paketverwaltung sind ein interner Speicherbaustein für vollständige
Generationen und ein eigener Veröffentlichungsprozess implementiert.
Komponentenstand `d4fdb778` besteht **146/146 native Gasttests**. Die acht neuen
Fälle prüfen echte Kindprozesse und Cgroups, darunter einen Abbruch während
einer beobachteten unvollständigen Kopie mit anschließend bestätigter bisheriger
Auswahl. Der gesamte bisherige native Umfang wurde ebenfalls geprüft. Echte
APT-Ausführung, produktive AOSP-Adminfreigabe und Broker-/CE-Anbindung fehlen
weiterhin. [Nachweis und offene Integration](../runtime/package-transactions.md).

Für die Passwortbestätigung sind der interne AOSP-Adapter und die einmalige
Bindung an Paketaktion, Antragsteller und privates Ziel implementiert und auf
dem Server kompiliert. Komponentenstand `09b10fd7` besteht **119/119 Java-Tests**,
darunter 14 isolierte Passwort-Policy- und 14 neue Koordinator-Fixtures. Auch
bei Bestätigung durch einen anderen Admin bleibt der Antragsteller Eigentümer.
Der Adapter vermeidet im Quellpfad die anschließende Benutzer-/CE-Entsperrung;
das ist noch nicht mit echten Adminpasswörtern im neuen Vollsystem geprüft.
Die produktive Dienstanbindung, der native Planer und Paketarbeiter sowie die
CLI fehlen weiterhin. [Belege und Grenzen](component-tests.md).

Neuester lokal gestarteter Vollbuild: **`927cf51d`**, mit Enforcing, FBE,
authentifiziertem ADB und tatsächlichem dm-verity. Im separaten Gast bestehen
nun **119/119 Java-Tests** aus Komponentenstand `09b10fd7`, einschließlich der drei
Prüfungen des echten AOSP-Allocators in isolierten App-Fixtures. Gelöschte
Kennungen bleiben darin während desselben Systemserver-Laufs reserviert,
auch wenn der Nummernraum erschöpft ist. Die echten Benutzer- und
Schlüsselverzeichnisse bleiben unverändert. [Belege und Grenzen](component-tests.md).
Dieser Lauf ist keine erneute Prüfung des vollständigen persönlichen Ablaufs.

Der jüngste tatsächliche Plattform-Löschtest verwendet **`73ddcb61`**. Die gezielte
SELinux-Korrektur ermöglicht jetzt den vollständigen AOSP-Abbau eines privaten
GNU-Benutzers. Ein absichtlich vor dem Schlüsselabbau gestörter Löschvorgang
behält seine Kennung reserviert und wird nach Neustart desselben Profilpaars
automatisch abgeschlossen. Eine anschließend regulär wiederverwendete Nummer
erhält eine neue Seriennummer; das alte Passwort und die alten Probedateien
gehen nicht auf den neuen Benutzer über. [Ablauf und Grenzen](../runtime/aosp-storage-lifecycle.md).
Die normale AEGIS-CLI-Löschung bleibt bis zur Berechtigungsabnahme gesperrt;
Pakettransaktionen sind weiterhin nicht implementiert.

Der umfassende Zwei-Benutzer-Nachweis stammt aus **`2f29f0ac`**. Zwei persönliche
AOSP-Benutzer führen jetzt tatsächlich GNU/Linux-Programme in getrennten
Kontexten aus. Geprüfte gegenseitige Datei- und Prozesszugriffe werden
verhindert. Benutzerwechsel und Bildschirmsperre widerrufen offene
Terminalkanäle, lassen die beobachteten Hintergrundprozesse aber weiterlaufen.
Vollständige Abmeldung beendet die zugehörigen Prozesse und sperrt CE.
Die von GNU geschriebenen Dateien überstehen den Neustart desselben
Android-/KeyMint-Paars bytegenau. Falsche und nach Passwortwechsel alte
Passwörter entsperren die Daten nicht. Enforcing, FBE, authentifiziertes ADB
und tatsächliches dm-verity bleiben bestätigt. Die zehn privaten Standardordner
werden jetzt in echtem CE angelegt. Eigene Ordneränderungen und Konfiguration
bleiben nach erneuter Anmeldung und vollständigem Reboot erhalten, ohne den
anderen Benutzer zu verändern. Passwortwechsel, altes Passwort vor und nach
Reboot sowie echter GNU-Dateizugriff sind in diesem Image erneut geprüft.

**Die neue Anmeldereihenfolge ist nun im installierten Dienst geprüft.**
Beide ersten Zugänge sowie beide ersten Anmeldungen nach Reboot funktionieren
direkt, ohne vorgeschalteten Fehlversuch, mit verzögerter Sitzungsprüfung und
echter GNU-Ausführung. Eine während der Passwortabfrage erfolgte Bildschirmsperre
verwirft den Versuch: Auch das danach korrekte Passwort entsperrt CE nicht.
Erst eine neue Anmeldung funktioniert. Der frühere nachträgliche Terminalwiderruf
wurde in diesem Durchlauf nicht beobachtet; weitere Konkurrenz- und
Dauerlastprüfungen bleiben offen. Beide tatsächlichen GNU-Größenrückmeldungen
des korrigierten Resize-Treibers bestehen.
Vollständiger Ablauf, genaue Grenzen und Prüfsummen:
[GNU-Test mit zwei Benutzern](runtime-gnu-qemu-test.md).

**128/128 native Tests** von `d44ccb33` bestehen im vorherigen lokalen
`6a807692`-Image. Die vier neuen Tests betreffen private Standardordner;
deren reale CE-Provisionierung ist nun zusätzlich im Vollbuild bestätigt.
**68/68 Java-Tests** des nachfolgenden `2f29f0ac` bestehen nach vollständiger
Benutzerabmeldung im lokalen `d44ccb33`. Der vollständige `2f29f0ac`-Build ist
über GitHub verifiziert und dessen installierter Dienst im oben beschriebenen
lokalen Ablauf geprüft. Diese Dienstprüfung ist keine erneute Ausführung der
Komponentensuiten. [Komponentenbelege](component-tests.md).

Der sichtbare Launcher bleibt beim geprüften Stand `2a766ab5`. Weitere
Kandidaten werden auf Nutzerwunsch ohne sichtbares QEMU-Fenster geprüft.
Bisherige gekoppelte Android-/KeyMint-Profile und alle gebundenen Basisdateien
bleiben erhalten. Vollständige Pakettransaktionen sowie Freischaltung und
Berechtigungsabnahme der verwalteten CLI-Benutzerlöschung stehen noch aus.

## Bestätigte Nachweise

Das vollständige Image `25fde995` bootet mit SELinux Enforcing,
authentifiziertem ADB, FBE und tatsächlichen dm-verity-Tabellen für `system`
und `system_ext`. Im getrennten persistenten Profil wurden zwei persönliche
Benutzer über die installierte AEGIS-CLI angelegt und angemeldet. Falsche
Passwörter werden abgewiesen; Benutzerwechsel, Passwortwechsel, bestätigte
CE-Sperre nach Logout und unveränderte Testdateien nach geordnetem Neustart
wurden beobachtet. AOSP bleibt die Identitäts- und Passwortautorität.
Die Dateiproben stammen von Entwicklungs-root, nicht von isolierten
Linux-Benutzern. Siehe [CLI-Test und Grenzen](identity-cli-qemu-test.md).

Der korrigierte Komponentenstand `bfe90925` besteht auf diesem Image
**88 von 88 nativen Tests** ohne Abwahl. Die drei früheren Mountfehler sind
behoben: Der Broker bindet die unveränderte Basis zuerst in seinen eigenen
Mount-Namespace ein und erzeugt daraus die persönlichen Ansichten. Auf dem
älteren Komponentenstand bestehen außerdem **52 von 52 Java-Tests** im
vollständig gebooteten Produkt. Diese Ergebnisse betreffen unterschiedliche
Quellstände, keine gemeinsam abgenommene Runtime. Nachweise und Rohlogs:
[Komponententests](component-tests.md).

Die gemeinsame Debian-13.7-ARM64-Basis enthält 78 Pakete in einem 256-MiB-
ext4-Image. Zwei Builds waren bytegleich; Metadaten und Dateibytes sowie die
Kopie innerhalb von `super.img` wurden geprüft. Die tatsächliche Nutzung dieser Basis ist inzwischen im
[GNU-Test mit zwei Benutzern](runtime-gnu-qemu-test.md) nachgewiesen. Frühere
Komponentenläufe allein erbrachten diesen Ausführungsnachweis nicht.

## Aktuelle Builds und nächste Prüfung

Vollbuild `aosp-20260928T210223Z-bfe90925-32d1cb4b` ist kompiliert, paketiert
und mit `UPLOAD_VERIFIED` über
[GitHub veröffentlicht](https://github.com/simgero/AegisOS/releases/tag/aosp-20260928T210223Z-bfe90925-32d1cb4b).
Er enthält die Korrektur geerbter Telefoniefunktionen und den bis dahin
committeten AEGIS-Auftritt. Das neue lokale Profil bootet vollständig, mit
authentifiziertem ADB, SELinux Enforcing, dm-verity und antwortender AEGIS-CLI.
**88/88 native und 52/53 Java-Tests bestehen.** Die geerbte Vendor-RRO
überschreibt `config_sms_capable` weiterhin auf true. Eine gezielte Product-RRO
ist vorbereitet; der Test bleibt unverändert. Im ersten Beobachtungsintervall
trat kein Telefonie-ANR mehr auf. Nach geordnetem Android-/Helper-Stopp wurde
dasselbe Profil sichtbar neu gestartet. Die bisherigen Profilpaare bleiben
erhalten; keine Image-Migration. Siehe [QEMU-Hardware](qemu-hardware.md).

Commit `ac87df1fc333893d08db0ceb9c67b13c59eed758` erweitert den internen
Brokerkanal um begrenzte Linux-Befehle, persönliche PTY-Übergabe und getrennte
laufende/beendete Ergebnisse. Alle acht ARM64-Syntaxprüfungen bestehen im
Builderlauf `runtime-syntax-20260928T215856Z-ac87df1f-rOLeXi`.
Der Komponentenlauf `identity-20260928T222328Z-ac87df1f-Iu66cE` ist kompiliert
und über [GitHub](https://github.com/simgero/AegisOS/releases/tag/components-20260928T223051Z-ac87df1f-ac87df1f-IVJkEu)
verifiziert übertragen. Im separaten lokalen Profil `runtime-bfe90925`
bestehen **106/110 native und 58/59 Java-Tests**. Vier positive Terminaltests
scheitern, weil deren Fixture `posix_openpt()` und damit Androids Tmpfs-
`/dev/ptmx` verwendet. Der produktive Aufseher öffnet bereits `/dev/pts/ptmx`.
Die Fixture ist in `2a766ab5` daran angeglichen; ein zusätzlicher Test
verlangt weiterhin die Ablehnung des Legacy-Geräts. Die produktive Prüfung
wurde nicht gelockert. Der Java-Fehler bleibt die bekannte SMS-RRO.
Rohbelege: `out/components-ac87df1f/component-tests/`.

Commit `2a766ab5d6a6ef5ed4c01fd87fa4e97bb2e2961d` verbindet die AOSP-Storage-Barrieren mit
dem nativen Besitzer und ergänzt `linux start|status|stop` im tatsächlichen
AEGIS-Binder-/CLI-Pfad. Eine interne, pro Anmeldung gebundene Epoch-Zuordnung
verhindert, dass eine spätere Anmeldung einen widerrufenen Terminalkanal
reaktiviert. AOSP-Aufrufe erfolgen außerhalb der Runtime-Sperre; Logout verlangt
zunächst bestätigten nativen Abbau und danach bestätigten AOSP-Stopp/CE-Sperre.
Der Komponentenlauf `identity-20260928T223826Z-2a766ab5-sutWPl` ist kompiliert
und über [GitHub verifiziert](https://github.com/simgero/AegisOS/releases/tag/components-20260928T224549Z-2a766ab5-2a766ab5-u7YQ7B).
Im separaten lokalen QEMU bestehen **111/111 native und 61/62 Java-Tests**;
alle neuen Sitzungsbindungs- und korrigierten Terminaltests bestehen. Einziger
Fehler ist weiterhin die SMS-RRO des gebooteten alten Images. Ein vollständiger
Build dieses Commits wird deshalb für den tatsächlichen Overlay-Bootnachweis
benötigt. Belege: `out/components-2a766ab5/component-tests/`. Die Komponenten-
prüfungen führen den verwalteten AOSP-Dienst nicht aus. Das Produkt bleibt `absent`;
SELinux/Init-Aktivierung, öffentliche PTY-/Shell-Verbindung, Paketaktionen und
Löschungsintegration vor AOSP-ID-Freigabe fehlen weiterhin. `managed-v1` ist
kein zur manuellen Aktivierung freigegebener Schalter.
Vollbuild `aosp-20260928T224751Z-2a766ab5-ed1329db` ist inzwischen mit
`UPLOAD_VERIFIED` abgeschlossen. Nach GitHub-Download, Prüfsummen- und
AVB-Kettenprüfung bootet das neue lokale Profil `foundation-2a766ab5`
vollständig. **111/111 native und 62/62 Java-Tests bestehen** auf demselben
Image- und Komponentencommit. Die SMS-Product-RRO wirkt nun tatsächlich;
alle drei Telefonie-Booleans sind false. Die installierte Dienstfassung
antwortet und weist Linux-Lebenszyklusaktionen im Modus `absent` zurück.
Der erste Testboot endete mit Android `Power down` und sauberem Helper-Abschluss.
Frühere Profilpaare bleiben erhalten; es erfolgt keine Benutzerdatenmigration.
Details: [Komponententests](component-tests.md) und
[Terminalübergabe](../runtime/terminal-handoff.md).

Die neue SELinux-/Init-Integration ist inzwischen unter `6633a086` auf dem
Build-Server kompiliert. Androids Neverallow-, API-Freeze-, Treble-, Typ- und
Kontextprüfungen bestehen. Private Domänen trennen Broker, Mount-Einrichtung,
Aufseher und GNU-Programme; AOSP behält die Schlüsselverwaltung. Neun inerte
Tests prüfen die gepinnte und gegen fremde Änderungen abgesicherte
Policy-Quellintegration. Details: [Runtime-Policy](../runtime/selinux-integration.md).

Vollbuild `aosp-20260928T235701Z-e835d7ea-d28a494a` scheiterte an der
Artifact-Path-Prüfung für die beiden neuen `/system/bin`-Helfer. `a8d38b97`
ergänzt ausschließlich diese zwei Pfade in der vorgesehenen Freigabeliste;
die Prüfung bleibt aktiv. Der Dienst-Socket übernimmt zudem korrekt den
Broker-Peer-Kontext; sein Dateisystemname behält das separate Socket-Label.
Dienste und Modus werden nur bei ausgewählten, geprüften Basis- und
Kernel-Eingaben aktiviert. Das bisher geprüfte Profil bleibt `foundation-2a766ab5`.
Die Bestandsaufnahme dort zeigt den Android-Cgroup-Elternbereich als
`system:system` mit Modus `0775`. Die bisherige native Root-Annahme ist in
`ae1e5d5` gezielt korrigiert, mit zwei zusätzlichen negativen/positiven
Gerätetests und weiterhin zwingender Root-Eigentümerschaft privater Gruppen.
Im Komponentenstand `a8d38b97` ist diese Korrektur nun kompiliert und lokal
geprüft: **113/113 native Tests bestehen** auf dem unveränderten Image
`2a766ab5`. Der korrigierte Vollbuild
`aosp-20260929T001619Z-a8d38b97-55707a05` ist mit `UPLOAD_VERIFIED`
abgeschlossen und über GitHub geprüft empfangen. Nach AVB-Kettenprüfung
bootet das frische Profil `runtime-a8d38b97` vollständig mit authentifiziertem
ADB, FBE, tatsächlichen Verity-Tabellen und SELinux Enforcing. Der ausgewählte
Modus ist tatsächlich `managed-v1`. Init startet den Broker, dieser beendet
sich jedoch nach fünf Millisekunden mit Exitcode 1. Sein Zustandsverzeichnis
bleibt leer; ein spezifischer AVC wurde nicht aufgezeichnet. Die bisherigen
Fehlermeldungen gehen in Inits `/dev/null`-Standardfehlerkanal verloren.
Dieser Kandidat ist deshalb **nicht als sichtbarer funktionierender Stand
übernommen**. Echte GNU-Ausführung bleibt unbewiesen.

Quellstand `4e53dc18` schreibt feste Phasen-/Errno-Diagnosen über das
bereits freigegebene Android-Logging; sie erweitert keine SELinux-Rechte.
Außerdem verlangt die Helferprüfung nun die im Image beobachtete AOSP-
Eigentümerschaft `root:shell` statt `root:root`, weiterhin mit exaktem Modus
0755, Label und schreibgeschütztem EROFS. Dies ist ein separat belegter
späterer Prüfkonflikt, **keine bestätigte Ursache des frühen Dienstabbruchs**.
Diese Komponenten sind inzwischen auf dem Builder kompiliert und über GitHub
verifiziert empfangen. Im unveränderten verwalteten Testimage `a8d38b97`
bestehen erneut **113/113 native Tests** ohne Abwahl. Der Broker dieses Images
bleibt gestoppt; die neuen produktiven Diagnosepfade und die Helferprüfung
sind damit noch nicht ausgeführt. Der vollständige Image-Build
`aosp-20260929T010358Z-4e53dc18-1c116294` ist gestartet; sein Bootnachweis
steht aus.
Bootbelege: `out/full-build-a8d38b97/boot-1/boot-health.json`.

Vollbuild `aosp-20260929T010358Z-4e53dc18-1c116294` ist inzwischen mit
`UPLOAD_VERIFIED` abgeschlossen und über GitHub verifiziert empfangen.
Das separate Profil `runtime-4e53dc18` bootet mit FBE, authentifiziertem ADB,
SELinux Enforcing und tatsächlichen Verity-Tabellen. Die neue Bootdiagnose
belegt den frühen Fehler: `open init proc directory errno=2`. Androids
`/proc` ist mit `hidepid=invisible` eingehängt; der Broker erhält bewusst
weder die Readproc-Gruppe noch Ptrace-Rechte. Der Dienst bleibt gestoppt.
Beleg: `out/full-build-4e53dc18/boot-1/boot-health.json` sowie das durchgehende
serielle Log. Das vorherige A8D-Testprofil wurde sauber und gepaart gestoppt.

Die folgende Korrektur lässt Init seine drei festen User-/PID-/Mount-
Namespace-Deskriptoren übergeben. Der Broker übernimmt geprüfte eigene
Kopien, vergleicht Namespace-Art und Identität und bindet sie an seinen
ursprünglichen Prozess. Die Prüfung öffnet `/proc/1` nicht mehr. NSFS bekommt
ein eigenes SELinux-Label mit den benötigten Lese-/Metadatenrechten; allgemeiner
Zugriff auf unbeschriftete Dateien wird nicht freigegeben. Init und Vold
behalten das Lesen dieser Namespace-Handles. Ein neuer nativer Test prüft
falsche Handles, unveränderliche Bindung und Deskriptorlecks; der vorhandene
Fork-Test prüft nun auch die Abweisung einer erneuten Host-Bindung.
Diese Korrektur ist unter `3b350e74` einschließlich der Policy kompiliert.
Im unveränderten lokalen Image `4e53dc18` bestehen **114/114 native Tests**;
der neue produktive Dienststart ist dadurch noch nicht geprüft. Die folgende
Policy-Ergänzung `afaf6462` benennt zusätzlich den privaten Cgroup-Typ bereits
bei der Verzeichniserstellung. Nur Policy und Dokumentation unterscheiden sich
von den getesteten Komponenten; die nativen Quellen bleiben identisch.
Ein vollständiger Build dieses Standes ist für den tatsächlichen Startnachweis
erforderlich. Eine laufende GNU-Sitzung ist weiterhin nicht nachgewiesen.

Der Vollbuild `aosp-20260929T015209Z-afaf6462-d5919499` ist inzwischen mit
`UPLOAD_VERIFIED` abgeschlossen und lokal mit einem neuen Profilpaar gebootet.
Enforcing, FBE, authentifiziertes ADB und tatsächliche Verity-Tabellen sind
bestätigt. Die feste Init-Namespace-Übergabe, das neue NSFS-Label und die
private Cgroup-Vorbereitung funktionieren im tatsächlichen Dienststart.
Die nächste konkrete Blockade ist ein Kernel-AVC für `rootfs:dir mounton`
beim privaten Propagationswechsel des Brokers. Die folgende Policy-Korrektur
ergänzt nur dieses Recht für den vertrauenswürdigen Besitzer; ein neuer
Image-Test bleibt erforderlich. Der Kandidat ist weiterhin nicht als
funktionierende Runtime oder sichtbarer Standard übernommen.
Belege: `out/full-build-afaf6462/boot-1/`.

Das bisherige Profil wurde nach rund 73 Minuten geordnet heruntergefahren;
Android meldet `Power down`, der KeyMint-Helfer bestätigt seinen sauberen
Abschluss. Das durchgehend aufgezeichnete Log enthält in diesem begrenzten
Intervall keinen Treffer für App-Absturz, ANR oder das frühere Modem-Warten.
Beleg: `out/full-build-2a766ab5/interactive-20260929T011639-68287/extended-stability.json`.
Der Nutzer erlaubt weitere Aktualisierungen der sichtbaren lokalen Version.
Neue Images werden zunächst mit einem separaten Profilpaar geprüft;
bisherige Android-/KeyMint-Paare bleiben erhalten.

## Erfüllung der fünf Ziele

| Ziel | Tatsächlicher Stand und fehlender Nachweis |
| --- | --- |
| Stabiles, dauerhaftes QEMU | Gepaarte Android-/KeyMint-Persistenz und geordnete Neustarts nachgewiesen. Neues Image `2a766ab5` besteht alle 111 nativen und 62 Java-Tests einschließlich SMS-Konfiguration. Kein Telefonie-ANR im begrenzten ersten Beobachtungsintervall. Lesbares Bild und früherer QMP-Mausklick bestätigt; physische Mac-Eingabe, längere Stabilität, Stromausfall und Image-Migration offen. |
| AEGIS-Benutzer und Anmeldung | Zwei-Benutzer-CLI-Test mit AOSP-Passwörtern, Wechsel, Passwortwechsel und Logout bestanden. Unterbrochene Ersteinrichtung, vollständige Admin-Negativtests, CLI-Löschung und ID-Wiederverwendung offen. Die bisherige Nachbereinigung nach Freigabe einer gelöschten AOSP-ID muss vor Löschungsfreigabe in den reservierten Plattform-Lebenszyklus verlegt werden. |
| Gemeinsame GNU/Linux-Runtime | Debian-Basis gebaut und im Image geprüft. Vollbuild `afaf6462` bootet mit Enforcing und Verity; Init-Namespace-Übergabe, NSFS-Label und private Cgroup-Vorbereitung sind bestätigt. Der Broker stoppt danach an einem belegten `rootfs:dir mounton`-Verbot; die enge Policy-Korrektur benötigt einen weiteren vollständigen Image-Test. Die unveränderten nativen Quellen bestehen 114/114 Komponententests. Aktiver Dienst, tatsächliche GNU-Domänenübergänge, CLI-PTY-Sitzung und Linux-Ausführung bleiben unbewiesen. Das sichtbare Standardprofil bleibt `absent`. |
| Pakete und Isolation | Pakettransaktionen und konsistente Aktivierung fehlen. Sowohl gemeinsame als auch private Pakete erfordern frische AOSP-Adminautorisierung. Persönliche Linux-Datei-/Prozessisolation muss mit zwei angemeldeten Benutzern geprüft werden. |
| Gesamtablauf | Identitäts-/CE-Teil mit zwei Passwortbenutzern und Neustart nachgewiesen. Linux-Programme, Paketaktionen, Abbau aller Runtime-Ressourcen vor CE-Sperre und der vollständige integrierte Ablauf bleiben offen. |

## Frühere Vorbereitungsschritte

Die folgenden Absätze dokumentieren den damaligen Stand vor dem ersten
erfolgreichen Komponentenbau. Aussagen „unkompiliert“ oder „ungetestet“ darin
werden durch den aktuellen Nachweis oben präzisiert; die noch fehlende
Integration und vollständigen Systemtests bleiben offen.

Die ausdrückliche Fortsetzung einer protokollierten Ersteinrichtung ist mit
`setup --resume NAME` im Quelltext angebunden. Vorhandene Passwörter bleiben
erhalten und müssen über AOSP bestätigt werden; der Abschluss verlangt einen
gestoppten Benutzer und gesperrten CE-Speicher. Zwölf zusätzliche Android-Tests
sind vorbereitet. **Kompilierung, Ausführung der neuen Gerätetests sowie echte
Abbruch-/Neustartversuche in QEMU stehen aus.** Nicht eindeutig zugeordnete oder
partielle AOSP-Konten werden dabei nicht automatisch übernommen.

Als nächster Runtime-Baustein liegt jetzt der
[persönliche Prozessaufseher](../runtime/process-supervisor.md) mit privaten
Kontrollkanälen, Shell-PTYs, UID/GID-Wechsel und Prozessende bei Brokerverlust
im Quelltext vor. Die native Kennungsdatei wird mit Java und AOSP-Register
gemeinsam erzeugt. Der Komponenten-Build umfasst zusätzlich den Aufseher und
vorbereitete ARM64-Gerätetests. **Noch nicht kompiliert, nicht aktiviert und
nicht im Gast getestet.** Broker, Mounts, SELinux-Anbindung, Paketverwaltung
und vollständige AOSP-Logout-Koordination bleiben offen.

Die Verwaltungsseite besitzt zusätzlich eine interne Bibliothek zur Bestätigung
des tatsächlichen Kindprozessendes über eine stabile Kernel-Prozessreferenz.
Timeouts behalten die Referenz; anderweitig verbrauchte Exitdaten bestätigen
keinen Erfolg. Zehn zusätzliche Gerätetests sind vorbereitet. **Auch dieser
Code ist unkompiliert und nicht im Gast geprüft.** Der Abgleich mit AOSP zeigt,
dass `onUserStopping` allein keine bestätigte Barriere für den Runtime-Abbau
bietet; die vollständige Stopp-/CE-Koordination bleibt zu implementieren.

Der [Namespace-Start](../runtime/namespace-launch.md) ist jetzt ebenfalls im
Quelltext vorbereitet: gemeinsam erzeugte Namespaces/Pidfd, begrenztes Warten
auf beide geprüften UID/GID-Maps, keine geerbten Zusatzgruppen, feste Umgebung
und FD-Übergabe sowie beobachtbares Prozessende auch nach Startfehlern. Neun
zusätzliche native Gerätetests sind vorbereitet; damit sind es insgesamt
zunächst 31 native Tests. **Noch nicht kompiliert, aktiviert oder in QEMU ausgeführt.**
Der neue Code ersetzt weder den fehlenden Broker noch den Mount-Helfer,
SELinux-Integration, AOSP-Sitzungsprüfung oder den Zwei-Benutzer-Nachweis.

Die Maps können inzwischen ohne Exec-Freigabe vorbereitet werden. Darauf baut
eine [persönliche, schreibgeschützte Basis-Sicht](../runtime/base-mounts.md) auf:
derselbe Dateibestand erhält unterschiedliche Mount-Eigentümer, ohne die Quelle
umzuschreiben. Vier weitere native Tests sind vorbereitet, insgesamt jetzt 35.
**Auch diese Erweiterung ist unkompiliert und nicht im Gast geprüft.** Die Tests
verwenden ein inertes Tmpfs; echter ext4-Basis-Mount, CE-Einbindung, Rootwechsel
und ausführbarer Runtime-Kontext bleiben gesondert offen.

Die [persönliche Speicheranbindung](../runtime/personal-storage.md) ist jetzt
ebenfalls im Quelltext vorbereitet: unveränderliche ID-/Seriennummernbindung,
AOSP-Seriennummer und gleiche CE-Richtlinie, kontrollierte HOME-Erstanlage,
strikte Wiederöffnung und detached Mount ohne doppelte ID-Zuordnung.
Fünf weitere native Negativtests sind vorbereitet, insgesamt jetzt 40.
**Unkompiliert und nicht im Gast geprüft.** AOSP-Lebenszyklus-Sperre, echter
Mount-Helfer, SELinux, private Paketbestände und Zwei-Benutzer-Nachweis fehlen.

Das [private Geräteverzeichnis](../runtime/private-devices.md) ist ebenfalls
im Quelltext vorbereitet: begrenztes frisches Tmpfs, sechs festgelegte
Zeichengeräte, leere Terminal-/IPC-Mountpunkte, feste Links und geprüfte
schreibgeschützte Metadaten mit persönlicher ID-Zuordnung. Der Namespace-Start
trennt zusätzlich Terminalsitzung und Prozessgruppe vom Broker. Zwei weitere
native Tests sind vorbereitet, insgesamt 42. **Unkompiliert und nicht im Gast
ausgeführt.** Die echte Mountübergabe, private Prozess-/Terminal-Dateisysteme,
Rootwechsel und durchgesetzte SELinux-Regeln bleiben offen.

Der [statische Mount- und Starthelfer](../runtime/namespace-setup.md) ist jetzt
ebenfalls im Quelltext vorhanden: feste, an ID und Seriennummer gebundene
FD-Übergabe, eigene Procfs-/Devpts-/IPC-/Tmpfs-Sichten, Rootwechsel mit Abtrennen
der Android-Wurzel und anschließendes Rücklesen der Mounts. Acht weitere native
Tests sind vorbereitet, insgesamt 50. **Noch nicht kompiliert oder im Gast
ausgeführt.** Broker, Lebenszyklus-Sperre, Ressourcen-Cgroups, SELinux-Typen und
Übergänge sowie der praktische Rootwechsel-/Isolationsnachweis fehlen weiterhin.

Die [vorgeschaltete Speicherkoordination](../runtime/aosp-storage-lifecycle.md)
ist jetzt an fünf AOSP-Schlüsseloperationen im Quelltext angebunden. Der
fehlende Controller muss den Ressourcenabbau bestätigen, bevor Schlüssel
entzogen werden; Fehler dürfen keine erfolgreiche Sperrbestätigung erzeugen.
Acht zusätzliche Java-Tests sind vorbereitet, insgesamt 32. Der Komponentenlauf
baut nun auch die betroffenen Framework-Dienste. **Noch nicht auf dem Server
kompiliert oder im Gast getestet; Runtime-Modus weiterhin `absent`.**

Die [Zugangsserialisierung](../runtime/admission.md) liegt ebenfalls als
Quelltext vor: einmalige Anmeldeversuche, getrennte Benutzersperren,
Seriennummernbindung und Widerruf vor/nach destruktiven Speicheroperationen.
Zwölf weitere Android-Tests sind vorbereitet, insgesamt 44. **Unkompiliert,
ungetestet und noch nicht aktiviert.** Native Abbaubestätigung, vollständige
AOSP-Lebenszyklus-Anbindung und Pakettransaktionen fehlen weiterhin.

Auch die [gemeinsame Softwaregeneration](../runtime/generations.md) besitzt nun
ein Buildrezept: technischer NSS-Benutzer, explizite Datei-Eigentümer, Entfernen
der Set-ID-Bits, wiederholte ext4-Erzeugung und Lesen des tatsächlichen
Image-Inhalts. Die echte Debian-Basis ergibt im Plan 78 Pakete und 3.271
Einträge. Vierzehn Hosttests mit inerten Archiv-/Dateisystemfixtures prüfen
Metadaten und Fehlerpfade. **Das echte Image wurde noch nicht auf dem Server
gebaut, hochgeladen oder in QEMU eingebunden.**

Die explizite Übernahme eines abgeschlossenen Basislaufs in den vollständigen
Systembuild ist ebenfalls im Quelltext angebunden: Rekonstruktion des Plans
aus dem Originalimport, erneute Inhaltsprüfung, Erhalt einer ausgewählten
Kernelkonfiguration, Produktdateien und Release-Nachweise. Elf Metadatentests,
vier Worker-Transporttests und eine Pfadprüfung wurden ergänzt. **Server-Build,
Prüfung der tatsächlichen Partitionsimages, Gast-Mounts und Runtime-Start stehen
weiterhin aus.** Die Ablage von Basisdateien aktiviert keinen Runtime-Modus.

Der Vollbuild enthält jetzt zusätzlich einen vorbereiteten Prüfschritt für die
Basisdateien innerhalb der tatsächlich ausgelieferten `super.img`: Slot-A-
Partition lesen, EROFS-Inhalte prüfen und Größen/Hashes abgleichen. Der zugehörige
Bericht ist für jeden Release erforderlich und wird nach dem Upload zurückgelesen.
Fehlerfalltests verwenden inerte Dateien; Linux-CI prüft zusätzlich echte kleine
EROFS-Textfixtures. **Mit dem neuen echten AOSP-Image und dessen Werkzeugen ist
dieser Schritt noch nicht ausgeführt; er bestätigt keinen Boot oder Runtime-Start.**

## Bestätigte Kernel-Lücke

Der laufende Kernel 6.12.18 meldet in `/proc/config.gz`:

```text
# CONFIG_SYSVIPC is not set
# CONFIG_USER_NS is not set
# CONFIG_PID_NS is not set
# CONFIG_VIRTIO_NET is not set
CONFIG_UTS_NS=y
CONFIG_NET_NS=y
CONFIG_EXT4_FS=y
CONFIG_OVERLAY_FS=y
```

Die Zeile zu `CONFIG_VIRTIO_NET` beschreibt nur die GKI-Konfiguration: Im
laufenden Gast ist `virtio_net` als separates, passendes Virtual-Device-Modul
bereits geladen (`/proc/modules`, Lauf `mouse-1`). Daraus folgt kein fehlender
Netzwerktreiber. Der Launcher richtet bislang keine Gast-Netzwerkkarte ein.

Wegen der fehlenden User-/PID-Namespaces und System-V-IPC ist für die
beauftragte gemeinsame GNU/Linux-Runtime ein gezielter
Kernel-Build einschließlich passender Module erforderlich. User-, PID-, Mount-
und IPC-Isolation müssen anschließend tatsächlich funktionieren. Ein chroot
allein oder eine zusätzliche Linux-VM erfüllt den Auftrag nicht. Der genaue
Kernel-Quellstand, Konfiguration, Module und Android-Integration sind vor dem
Build abzugleichen; das bloße Hinzufügen von Konfigurationszeilen reicht nicht.

Die passenden 40 Kernel-Quellprojekte sind inzwischen aus dem offiziellen
Manifest des laufenden Builds `13257114` festgelegt. Ein Buildrezept mit
gemeinsamem Namespace-Fragment für Kernel und Module liegt unter
[`kernel/`](../kernel/README.md). Die Übernahme eines ausdrücklich gewählten
Laufs in den vollständigen AOSP-Build ist inzwischen im Quelltext vorbereitet:
eingebettete Kernelkonfiguration, Modulversionen, Boot-Treiber, gemeinsame
Pfadauswahl und der resultierende Kernel werden geprüft, der Eingabenachweis
über GitHub mitgeliefert. Der neue Kernel wurde auf dem Builder gebaut und einschließlich der tatsächlichen
Module geprüft. Der laufende Vollbuild hat diese Eingaben ausgewählt; Prüfung
der resultierenden Android-Images und Gasttests stehen weiterhin aus.

## Reihenfolge

1. Erreichten geordneten Neustartnachweis um Absturz- und Migrationsfälle
   ergänzen; gekoppelte Android-/Helper-Profile beibehalten.
2. QEMU-Hardwarekonfiguration und Bedienung bereinigen; Kernel-Build vorbereiten.
3. AOSP-vermittelte AEGIS-Anmeldung und Benutzerlebenszyklus integrieren.
4. Runtime und Paketoperationen mit AOSP-Adminautorisierung integrieren.
5. Alle Anmelde-, Daten-, Prozess- und Logout-Anforderungen mit zwei Benutzern
   einschließlich Fehlerfällen prüfen.

Der Komponentencheck für `5447a968d00f63aa7cb700d868508c437d89980c` wurde am
28. September auf `aegis-build` gestartet. Der erste Lauf endete um 12:41 UTC
durch OOM beim Soong-Buildplan: nur 14,7 GiB Gast-RAM statt der ursprünglichen
rund 94 GiB, 8 GiB Swap nahezu ausgeschöpft. AOSPs Parser akzeptierte zuvor
die 1.002 vorbereiteten Runtime-Kennungen; das belegt keine laufenden UID-Maps.

Nach der vom Nutzer vorgenommenen Hyper-V-Anpassung meldet Ubuntu rund 65 GiB
RAM und 62 GiB verfügbar. Der Ersatzlauf startete um 12:48 UTC mit derselben
Revision und InvocationID `5b747d59dd5947a4b8b0ca30ddf703c3`. Soong schloss
seine Analyse ab; der Speicherhöchststand lag bei rund 37,8 GiB. Um 12:54:52 UTC
brach Kati nach 5:42 Minuten an der Artefaktgrenze von `generic_system.mk` ab:
CLI, Dienst-JAR und zugehörige Dex-Artefakte wurden vom Geräteprodukt
fälschlich in `system` installiert. Das Journal belegt den Fehler, auch wenn
der inzwischen eingesammelte Dienst `inactive` und `ExecMainStatus=0` meldet.
Der Lauf `identity-20260928T124851Z-5447a968-1zAsj2` enthält die
32 Java-Tests dieser Revision und die 50 nativen Tests; die später vorbereitete
Zugangsserialisierung ist darin noch nicht enthalten. Kein Gasttest läuft auf
dem Server. Die neue RAM-Vorprüfung und bereinigte Skript-Fehlerbehandlung sind
für einen folgenden Quellstand vorbereitet. Die Partitionskorrektur setzt CLI
und Dienst auf `system_ext` und ergänzt den nötigen expliziten CLI-Klassenpfad;
Details in [Identitätsintegration](identity-cli.md#vorbereitete-produktintegration).
**Weiterhin kein bestätigter Komponentenabschluss oder Gasttest.** Der
fehlgeschlagene Checkout bleibt erhalten. Ein um 13:18 UTC gestarteter weiterer
Lauf verwendete nochmals die alte Revision `5447a96` und endete um 13:24:45 UTC
mit demselben Partitionsfehler; er prüfte die Korrektur noch nicht.

Der [Komponenten-Transport](component-transport.md) ist jetzt ebenfalls
vorbereitet: erfolgreicher Buildstatus und vollständige Modulprüfsummen als
Voraussetzung, getrennte Build-/Export-Commits, GitHub-Entwurf, Rückdownload
aller Assets und geprüfte lokale Extraktion. Die Transportfixtures führen
keinen neuen Android-/Runtime-Code aus. **Noch kein echter Komponentenexport:**
Ein erfolgreicher Lauf mit dem korrigierten System-Ext-Profil ist weiterhin
Voraussetzung. Die späteren Exportwerkzeuge verändern die mit `283452a`
zu kompilierenden Komponenten nicht.

Nach der Mitteilung des Nutzers wurde der passwortlose `sudo -n`-Zugriff
erfolgreich geprüft. Am 28. September um 13:29:07 UTC wurde der **korrigierte**
Stand `283452ae7ce82344bd94fc865e35abe58b1241b6` direkt gestartet:
InvocationID `7b0d8b055ab94cafb892e6c424f8e1f1`, Lauf
`identity-20260928T132908Z-283452ae-N06WMX`. Quellcode und Startskript kamen von
GitHub. Dieser Lauf passierte die Artefaktgrenze; die erzeugten Modulregeln
verweisen korrekt auf `system_ext`. Um 13:35:20 UTC endete er nach 5:57 Minuten
an der anschließenden Systemserver-Dex-Prüfung, die noch `system/framework`
erwartete. Die Produktliste erhält deshalb zusätzlich den in AOSPs
`ConfiguredJarList` vorgesehenen Präfix `system_ext:aegis-identity-service`.
Kompilierabschluss und Gasttests bleiben offen. Die Linux-CI des separaten
Exportwerkzeugs `f234349` besteht mit 190 Tests ohne Auslassungen; davon prüfen
16 neue Tests ausschließlich Transport und Veröffentlichungsablauf mit
inerten Dateien und einer GitHub-Fixture.

Der Lauf `identity-20260928T133821Z-024354c1-4sBywQ` passierte am
28. September beide Partitionsprüfungen und begann die native Kompilierung.
Er endete um 13:46:09 UTC nach 7:31 Minuten mit Status `FAILED`: Bionics
ARM64-`statfs.f_type` ist vorzeichenlos, der erwartete Dateisystemtyp in
`sandbox.c` war dagegen `long`. Die Erwartungen in Sandbox und Mount-Helfer
werden deshalb einheitlich als `uint64_t` geführt; die Prüfwerte und
Compilerwarnungen bleiben unverändert. Mehrere native Quellen und Tests
kompilierten bereits, aber kein vollständiger Modulabschluss ist bestätigt.
Die Linux-CI von `024354c` besteht mit 190 Tests ohne Auslassungen.

Der persistente KeyMint-Helfer wurde anschließend aus `024354c` auf dem
Server kompiliert und als `secure-env-20260928T134804Z-024354c1` über GitHub
veröffentlicht. Das Exportskript hat das Archiv zurückgeladen und bytegenau
verglichen. Lokale Prüfung und tatsächliche Wiederentschlüsselung nach
einem Neustart wurden anschließend bestätigt; siehe `persistent-qemu.md`.

Der nächste Komponentenlauf `identity-20260928T135507Z-d8cfe22b-ZoTz0n`
kompilierte die korrigierte Dateisystemprüfung. Er endete um 14:03:17 UTC nach
7:53 Minuten: `MQUEUE_MAGIC` fehlt in Bionics UAPI-Headern. Der Mount-Helfer
erhält den Wert `0x19800202` aus `ipc/mqueue.c` des exakten Kernel-Pins
`50eb8d5d443b43f38d6e72f005f1b8601ac88a05` unter einem eigenen Konstantennamen.
Die Prüfung des Message-Queue-Dateisystems bleibt erhalten. Der weitere
`sizeof`-Fehler war eine Folge des ungültigen Tabelleninitialisierers.

Der Lauf `identity-20260928T140941Z-5e045c4e-i5Skvo` kompilierte und linkte
die nativen Init-/Setup-/Probe-Binaries. Auch CLI, Identitätskern und
Speichergate erreichten die Java-Kompilierung. Der Gesamtlauf endete nach
9:32 Minuten mit `FAILED`: `ProductConfigurationTest` verwendete
`Os.getpwnam`, `getpwuid` und `StructPasswd`, die in der stabilen libcore-API
des Testmoduls nicht enthalten sind. Die Vorwärtsprüfung verwendet nun
`Process.getUidForName`/`getGidForName`; ein zusätzlicher nativer Test prüft
kanonische Vorwärts-/Rückwärtsnamen über Bionic. Damit sind 44 Java- und
51 native Gerätetests vorbereitet. Ihr vollständiger Build und ihre Ausführung
bleiben offen; der Dienst-/Framework-Build ist noch nicht abgeschlossen.

Unveränderte Produkt-/Identitätsdateien behalten nun ihre Zeitstempel, damit
kleine Änderungen nicht allein wegen erneuter Quellkopien den gesamten
Make-/Soong-Buildplan invalidieren. Die Linux-CI von `13485f1` besteht mit
195 Tests ohne Auslassungen. Der Lauf `identity-20260928T142429Z-d16e74f1-oH6LDF`
erreichte Ninja nach 28 Sekunden statt mehrminütiger Vorbereitung. Die
korrigierten Java-Gerätetests kompilierten; beim Dienst scheiterte nun
`CallerProcess` an `Files.readString`, das in diesem Android-API-Stand fehlt.
Der gleiche ASCII-Lesevorgang verwendet deshalb `Files.readAllBytes` und den
expliziten String-Konstruktor. Die Bindung an PID, UID und Kernel-Startzeit
bleibt erhalten. Ein erfolgreicher Gesamtabschluss ist noch nicht bestätigt.


## 2026-10-01: Bestätigter Schlüsselentzug und ergänzter IPC-Nachweis

Komponentenstand `1b1a0a9a3fa4d17b868087065d23df0ebe39c555` wurde auf
`aegis-build` einschließlich vold erfolgreich kompiliert. Der verifizierte
[Komponentenrelease](https://github.com/simgero/AegisOS/releases/tag/components-20261001T133453Z-1b1a0a9a-1b1a0a9a-zSMhW1)
wurde über GitHub auf den Mac übertragen. Neun neue native
`FscryptEviction`-Prüfungen und sechs `AegisCeLockTest`-Prüfungen bestehen im
lokalen f2d1d0e7-Gast. Die nativen Prüfungen verwenden denselben Algorithmus
wie der neue vold-Pfad; die Java-Prüfungen verwenden kontrollierte Rückgaben.
Die tatsächlich installierten Plattformdienste des Testgasts bleiben f2d1d0e7.
Benutzer, CE-Zustand, Schlüsselverzeichnisse und Runtime-Kontexte waren vor
und nach diesen Prüfungen identisch. Der erste Transferversuch scheiterte
vor der Ausführung an den Rechten des Testverzeichnisses; sein Fehlernachweis
bleibt getrennt vom erfolgreichen zweiten Versuch erhalten.

Nachweise: `out/components-1b1a0a9a/completion-tests-2/result.json`,
Native-Protokoll SHA-256
`169dd160489dd70e86a3dfd991f9164b1d27058882c123784758e6d3f8c4cbed`,
Java-Protokoll SHA-256
`900227999e81ee61a9eb236daa9e9b52d9cafd798a5078e339da15d8c8a5a8e4`.
Der passende Vollbuild `aosp-20261001T134005Z-1b1a0a9a-98335bc1` ist
kompiliert. Sein tatsächlicher Boot und die Tests mit einer absichtlich
geöffnet gehaltenen verschlüsselten Datei stehen noch aus. Insbesondere
müssen ein Ablauf über der zehnsekündigen Frist und die anschließende
Wiederholung beziehungsweise Wiederanmeldung geprüft werden. Die bisherigen
Ergebnisse schließen die reale Logout-Lücke noch nicht.

Im getrennten lokalen 020ae750-Gast bestanden zusätzliche echte
Unix-Socket-Kontrollen mit Simeon (10/10) und Isabelle (11/11): Jeder
GNU-Kontext konnte den eigenen abstrakten Stream-Socket erreichen und die
erwarteten Daten lesen. Die Verbindung zum fremden Socket wurde in beiden
Richtungen mit `ECONNREFUSED` abgewiesen. Für jeden fremden Zugriffsversuch
liegt ein erfolgreicher eigener Zugriff auf denselben Ziel-Listener vorher
und nachher vor. Beide Listener wurden danach mit geprüfter Prozessidentität
beendet, beide Benutzer regulär abgemeldet; die persönlichen Kontexte sind
abgebaut. Ein erster, zu kurz begrenzter Listenerlauf zählt nicht als dieser
vollständige Nachweis.

Nachweis: `out/full-build-020ae750/identity-test/ipc-socket-proof.json`,
Ereignisprotokoll SHA-256
`e89e850ab4c0609b7626988c289e8a553e9740077bfba9dfbfc05f00cb3029f4`.
Dies deckt abstrakte Unix-Stream-Sockets ab, keine pauschale IPC-Isolation
oder D-Bus-Funktion.

Offen ist ein tatsächlicher POSIX-Mqueue-Fehler: Der normale Runtime-Prozess
kann nicht einmal eine eigene Nachrichtenwarteschlange erstellen. Der Kernel
weist `mq_open` mit `EACCES` ab; SELinux protokolliert fehlendes `search` von
`aegis_runtime_program` auf `mqueue:dir`. Die Richtlinie wurde im Gast nicht
abgeschwächt. Eine gezielte Produktkorrektur braucht anschließend positive
Eigenzugriffe, gegenseitige Isolation und Abbaukontrollen. System-V-IPC
liefert dagegen wie konfiguriert `ENOSYS`: `CONFIG_SYSVIPC` ist bewusst aus,
während `CONFIG_IPC_NS` und `CONFIG_POSIX_MQUEUE` aktiv sind. Dies wird nicht
als bestandener System-V-IPC-Test gewertet.
