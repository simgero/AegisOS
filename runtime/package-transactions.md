## Aktueller Stand: signierte Planung bis zur privaten Veröffentlichung verbunden

`58be0749` besteht **362 ausgewählte native Prüfungen**; das byte-identische APK
und die Originalbelege bestätigen die vorherigen **54 Java-Tests**.
[Belege und genaue Grenzen](../docs/component-tests.md).

Ein Brokerauftrag verbindet jetzt private, bisherige gemeinsame und aktuelle
gemeinsame Generation mit signierter APT-Planung, separater Bindung, vollständiger
privater Vorbereitung, tatsächlicher Ausführung und gesperrter Veröffentlichung.
Das erneute Öffnen bestätigt Konfiguration, technische Eigentümer, Paketdatenbank
und unveränderte private Versionsvorgaben. Eine reine Basisfortschreibung ohne
Paketänderung ist möglich. Beschädigte Programme und eine inzwischen geänderte
gemeinsame Auswahl verhindern die Veröffentlichung; STOP schließt den gesamten
Auftrag. Normale Paketpläne können diesen internen Weg nicht übernehmen.

Die Tests verwenden ausschließlich eigene synthetische Abbilder. Die internen
konfigurierten Einstiege öffnen CE nach Registrierung und leiten die Ausführung
nur aus dem vorhandenen Reconciliation-Plan ab; dieser positive CE-Weg muss noch
im passenden Produktimage nachgewiesen werden. Noch fehlen automatische
Weiterführung, erneute Auswahl und Aktivierung beim Runtime-Start sowie die
gemeinsame Sperre für gewöhnliche private Paketaktionen. Danach sind Abgleich,
Konflikte und Neustart mit zwei echten AOSP-Benutzern zu prüfen.

`7e49bceb` ändert ausschließlich die QEMU-Zeitressource und ihre Dokumentation;
die übrigen exportierten Module sind byte-identisch. Der korrigierte periodische
Zeitabgleich ist gebaut, aber noch nicht im Vollimage installiert.
Das aktuelle Vollimage `d0b866e1` und seine vorhandenen Benutzer bleiben erhalten.

## Vorheriger Stand: privater Abgleich bis zur Veröffentlichung geprüft

`c844b9b4` besteht **347 native Prüfungen**; **54 Java-Tests** sind über das
byte-identische APK und die Originalbelege bestätigt.
[Belege und Grenzen](../docs/component-tests.md).

Der separate Reconciliation-Binder verbindet den vollständigen Plan jetzt mit
einer Veröffentlichung, die den geprüften aktuellen gemeinsamen Store sperrt.
Der Publisher verifiziert die gemeinsame Generation vor dem privaten Storezugriff
und hält ihre Sperre bis zur dauerhaften privaten Auswahl. Der Ressourcenbesitzer
behält und schließt sämtliche gemeinsamen/privaten Referenzen auch bei STOP.
Eine konkurrierende Änderung führt zur Ablehnung; sie wird nicht als erfolgreicher
Abgleich ausgegeben. Ein bestätigtes privates Abbild bleibt beim nächsten Start
erneut gegen den dann aktuellen gemeinsamen Stand zu prüfen.

Verbundene Tests prüfen echte APT-Ausführung, Konfiguration, technische Kennungen,
unveränderte private Vorgaben, Veröffentlichung, erneutes Öffnen und reine
Basisfortschreibung ohne Paketänderungen. Ein eigener Publisher-Test bestätigt
identische Image-Bytes mit geändertem Basisdatensatz ohne Inode-Austausch.
Diese Prüfungen verwenden ausschließlich eigene synthetische Abbilder.

Die Produktintegration bleibt offen: Dreifachauswahl und Planer-Übergabe unter
CE-Admission, abgeleitete Autorisierung, Auswahl nach Veröffentlichung und
Aktivierung beim Runtime-Start müssen verbunden und im installierten Produkt
mit echten AOSP-Zugangsdaten geprüft werden. Gewöhnliche private Paketaktionen
verwenden die neue gemeinsame Publikationssperre noch nicht. Das Vollimage
`d0b866e1` und seine bestehenden Benutzer wurden nicht ersetzt.

## Vorheriger Stand: gebundene Ausführung geprüft; Produktintegration offen

`a63c1214` besteht **338 native Prüfungen**, einschließlich tatsächlicher
APT-Auflösung und separater Ausführung mit Konfigurationserhalt, passenden
Abhängigkeiten, Downgrade und Basisfortschreibung ohne Paketänderungen.
Der byte-identische Nachweis von **54 Java-Tests** bleibt gültig.
[Belege und Grenzen](../docs/component-tests.md).

Die geprüfte interne Ausführungsstufe bindet private Generation, bisherige und aktuelle
gemeinsame Basis, vollständigen erwarteten Paketbestand einschließlich Holds
sowie alle Manual-/Automatic-Markierungen. Die private Auswahl bleibt unverändert.
Ein eigener interner Binder erzeugt einen Ausführungsplan auch für eine reine
Basisfortschreibung ohne Paketänderungen. Der unabhängige native Wächter prüft
die gesamte resultierende Registry und schreibt vollständige Markierungen erst
nach beendeten Paketkindern atomar in den unveröffentlichten Kandidaten.
Die normale Paketbindung nimmt keine Zusammenführungspläne entgegen.

Die Komponentenprüfungen erfolgen auf synthetischen, ausschließlich eigenen
Abbildern; sie ersetzen keine Produktabnahme mit echten AOSP-Zugangsdaten.
Die Ausführung verwendet den vollständigen ursprünglichen privaten Bestand;
Konfiguration und technische Kennungen dürfen nicht durch ein gemeinsames Overlay
ersetzt werden. Integration in Broker-Admission, Autorisierung, Prüfung des noch
aktuellen gemeinsamen Stands bei Veröffentlichung sowie Aktivierung beim nächsten
Runtime-Start und der Nachweis im installierten Produkt bleiben erforderlich.

## Vorheriger Stand: gemischte Transaktionen; gemeinsame/private Zusammenführung offen

Der Komponentenstand `2c7bf728` besteht **291 native Prüfungen**; die
byte-identische Java-Test-App behält ihren **54-Test-Nachweis**. Eine bereits
installierte Version lässt sich nun ausdrücklich privat auswählen: Der normale
Freigabeplan bindet die geprüfte installierte Version und die dauerhafte private
Auswahl. Die Ausführung ändert nur Auswahl und manuelle Markierung, führt keine
erneute Installation oder Paket-Skripte aus und veröffentlicht erst nach
unabhängiger Prüfung des vollständigen Bestands. Die echte APT-No-op-Auflösung
ist leer; behauptete gleichversionige Paketänderungen werden abgewiesen.
[Nachweis und Grenzen](../docs/component-tests.md).

Dieser Stand ist noch nicht im Vollimage installiert. Die Zusammenführung
bleibt offen: bisherige gemeinsame Generation, neue gemeinsame Generation und
private Auswahl müssen gemeinsam aufgelöst werden. Ausgangspunkt für die
Ausführung muss der vollständige bisherige private Bestand sein, damit
Konfiguration, technische Kennungen und Paketdatenbank erhalten bleiben.
Ein neuer gemeinsamer Hash allein darf die bisherige private Bindung nicht
umschreiben oder einen unveränderten alten Bestand als aktiviert ausweisen.

Die tatsächliche CLI im Vollimage `d0b866e1` hat frische AOSP-Adminfreigabe,
private Installation sowie private Dateien/Pakete über Logout, gemeinsamen
Android-/KeyMint-Neustart und Passwortwechsel nachgewiesen. Eine anschließende
gemeinsame Installation zeigt jedoch die offene Zusammenführung: Der Benutzer
mit alter privater Basis kann seine Runtime nicht mehr starten oder aktualisieren.
Die bisherigen Images bleiben dabei unverändert erhalten. Details stehen in
[den aktuellen Komponentennachweisen](../docs/component-tests.md).

Als Voraussetzung erweitert `7fc2f44a` den vollständig freigegebenen Plan um
kombinierte Installations- und Entfernungseffekte. Vorbereitung und Broker
übertragen nur tatsächliche Archive, in Effektreihenfolge mit ausgelassenen
Entfernungen. Das versionierte interne Protokoll verlangt für gemischte Vorgänge
ein vollständiges Review. Der feste Offline-Aufruf ergänzt ausschließlich bei
geprüften Entfernungen das APT-Suffix `-`; Simulation und abschließender Paketstatus
müssen weiterhin sämtliche gebundenen Effekte treffen. Benutzer können daraus
keine freien Optionen oder Befehle einschleusen. Der Resolver beschafft auch
bei einer Entfernung Archive, wenn die aufgelösten Abhängigkeiten dies erfordern.

**130 gezielte native QEMU-Prüfungen bestanden.** Der tatsächliche APT-Test
entfernt eine App und aktualisiert ihre Bibliothek gemeinsam; die veröffentlichte
Generation bewahrt Konfiguration und technische Dateieigentümer. Ein falsches
Archiv lässt die bisherige Auswahl unverändert. Dieser Komponentenstand ist noch
kein neues installiertes Produktimage. Private Absichten dauerhaft speichern,
gegen gemeinsame Änderungen auflösen sowie ausstehende Aktivierung und echte
Konflikte anzeigen bleiben die nächsten integrierten Anforderungen.

`73b2c062` ergänzt die Java-Freigabegrenze: Gemischte Effekte werden vollständig
angenommen und angezeigt, während Zielrichtung, exakte angeforderte Version und
Repository-Gültigkeit erhalten bleiben. **51 Android-Instrumentierungstests**
für Protokoll, Transaktion, Freigabekoordinator und CLI bestanden im lokalen
QEMU. Der Komponentenstand ist weiterhin nicht im Produktimage installiert;
echte Passwortfreigabe und vollständige gemeinsame/private Zusammenführung
werden durch diese Fixtures nicht erneut nachgewiesen.

Die folgenden Abschnitte dokumentieren frühere Stände und deren damalige Grenzen.

## Früherer Stand: Plan aus signierter Quelle bis zur veröffentlichten Generation verbunden

`b827ba82` besteht **46 gezielte und 340 aktivierte native Prüfungen**.
Der Auftrag bindet die tatsächlich authentifizierten Repository-/Archivbelege
an seine überprüfte Quellenauswahl, hält das Review selbst und übergibt ausschließlich
eigene Archive an Vorbereitung und Ausführung. Kennung, Antragsteller, Bereich,
Ausgangsgeneration und Review-Digest bleiben durchgängig zusammen. Die Gültigkeit
wird auch vor dem Ausführungsstart geprüft. Abbruch behält die Ressourcen bis
zur bestätigten Beendigung und entwertet auch ein fertiges Review.

Der vollständige interne Test installiert App und Abhängigkeit aus der signierten
Offline-Quelle, veröffentlicht den unabhängig geprüften Bestand und verifiziert
anschließend beide Versionen und automatischen Markierungen durch echtes Reopen.
Die neue Übergabe nutzt für private Ziele die vorhandene CE-Verankerung; der hier
vollständig ausgeführte Test verwendet einen separaten gemeinsamen Teststore.
[Belege und genaue Grenzen](../docs/component-tests.md).

Noch offen: produktiver Netzwerkabruf, öffentliche CLI/Binder-Befehle mit frischer
AOSP-Adminbestätigung, produktiver SELinux-Kontext und vollständiger gemeinsamer/
persönlicher Paketablauf mit zwei Benutzern. Der native Freigabeaufruf ist kein
Nachweis einer AOSP-Passwortprüfung. Nachfolgende Abschnitte sind historische Stände.

## Vorheriger Stand: isolierter Planer im registrierten Auftrag

`33482ef2` besteht **43 gezielte und alle 337 aktivierten nativen Prüfungen**.
Der Broker übernimmt die geprüfte Generationsauswahl direkt in denselben
Planungsauftrag. Er behält Arbeiter und Ergebnis bis zu bestätigter Beendigung
bzw. Abbruch; Poll gibt keine privaten Dateideskriptoren heraus. STOP_USER,
Verbindungswechsel, Zeitablauf und Teilfehler gehören zu derselben Verwaltung.
Für einen neuen Auftrag ist eine frisch geprüfte Generationsansicht nötig.
[Belege und Grenzen](../docs/component-tests.md).

Der Arbeiter verwendet unveränderliche Werksprogramme, feste Quellen und
Schlüssel sowie separat kopierte Paketmetadaten. Die Integration ist mit einer
signierten Offline-Quelle geprüft. Produktiver Netzwerkabruf, öffentliche
CLI/Binder-Verbindung, Produkt-SELinux-Kontext sowie Übergang des gehaltenen
Ergebnisses zu erneut geprüftem Plan, frischer AOSP-Adminfreigabe und Ausführung
bleiben offen. Vier deaktivierte echte AOSP-CE-Prüfungen sind nicht Bestandteil
dieses Nachweises; Benutzer und laufendes Produktimage bleiben unverändert.
Die nachfolgenden Abschnitte dokumentieren frühere Implementierungsstände.

## Vorheriger Stand: authentifizierte Paketbelege durchgängig gebunden

`9e705281` besteht 34 gezielte und alle 328 aktivierten nativen Prüfungen.
Der unveränderliche Resolver sammelt Release-/Index-/Archivbelege nach beendetem
APT-Abruf, kontrolliert Gültigkeit und vollständige Dateihashes und bindet die
Ergebnisse an den Freigabeplan. Signaturvertrauen stammt weiterhin ausschließlich
aus der zuvor erfolgreichen APT-Prüfung unter festen Quellen und Schlüsseln.
Die öffentlichen Paketbefehle, registrierte produktive Netzbeschaffung und
frische AOSP-Adminbestätigung bleiben offen; die Quelle dieser Integrationstests
ist eine signierte Offline-Fixture. [Details und Belege](../docs/component-tests.md).

# Vollständige Paketgenerationen

## Unveränderlicher Planer-Kern geprüft; produktive Hülle folgt

`8e500d7e` besteht **9/9 gezielte und 322/322 aktivierte native Prüfungen**.
`PackageResolverRun` verwendet eine geprüfte schreibgeschützte Werksbasis,
feste schreibgeschützte APT-Richtlinien und separat kopierte Paketmetadaten der
ausgewählten Generation. Echte APT-Auflösung, signierter Archivabruf und
Manipulationsablehnungen funktionieren im isolierten lokalen QEMU-Gerätetest.
[Belege und Grenzen](../docs/component-tests.md).

Der Kern wird derzeit mit denselben Quellen in Geräteprobe und Tests übersetzt;
der jeweilige Verbraucher deklariert seine zulässige Crypto-Abhängigkeit.
Die Produktionshülle ist noch nicht verdrahtet. Sie muss ausschließlich intern
geprüfte Werk-/Quellen-/Schlüssel-Mounts liefern, Netzwerkzugriff und einen
abbrechbaren Auftrag an den CE-Lebenszyklus binden, vollständige authentifizierte
Repository-/Archivbelege ableiten und sie vor einer frischen AOSP-Freigabe an
`PackageBindAptArchives` übergeben. `Collected` ist ausdrücklich keine Freigabe.
Der Prüfstand verwendet `copy:` mit einem ausschließlich für Tests signierten
Repository. Öffentliche CLI, Produktnetz und vollständiger Zwei-Benutzer-Ablauf
bleiben offen. Nachfolgende Abschnitte beschreiben ihre jeweiligen älteren Stände.

## Unabhängige Ausführungsprüfung bestanden; Produktintegration offen

`cacb1718` besteht **23/23 gezielte Prüfungen und 313/313 aktivierte native Tests**
im lokalen Mac-QEMU. Der Arbeiter vergleicht anfänglichen Status/automatische
Markierungen, vollständige strukturierte APT-Simulation und endgültigen
installierten Bestand mit dem versiegelten Review. Er übernimmt freigegebene
Manual-/Automatic-Markierungen; Hook und feste Konfiguration liegen schreibgeschützt.
Lokale Archive haben bei APT 3.0.3 eine leere Hook-Suchliste, Entfernung die exakten
Paketnamen. In beiden Fällen ist der vollständige Effektvergleich verbindlich.

Echte Install-/Update-/Remove-Fixtures bewahren lokale Konfigurationsänderungen
und gewünschte Abhängigkeitsmarkierungen. Die gebundene Transaktion veröffentlicht
nur unter dem ursprünglichen Digest. Vier deaktivierte AOSP-Integrationstests
sind nicht Teil der 313 Prüfungen; kein öffentlicher Paketendpunkt wurde aktiviert.
[Belege und Grenzen](../docs/component-tests.md).

Nächster Schritt ist die Verbindung des unveränderlichen produktiven Planers,
Quellen-/Archivbeschaffung und Auftragslebenszyklus mit dieser Ausführung, danach
sessiongebundene CLI/Binder-Operationen mit frischer AOSP-Adminbestätigung und
vollständige gemeinsame/private Paketprüfungen im Zwei-Benutzer-Produktablauf.
Die nachfolgenden Abschnitte sind historische Nachweise ihres jeweiligen Stands.

## Archivbefunde und vollständiger Freigabeplan: Verbindung geprüft

Die Version-2-Bindung verlangt nun eine ausdrückliche Manual-/Automatic-Markierung
für jede Änderung und den anfänglichen APT-`extended_states`-Beleg. Unbekannter
Zustand, bestätigte Abwesenheit und vorhandene leere Datei sind verschiedene Fälle;
unbekannt wird abgewiesen. Vorhandene Dateien benötigen Größe und SHA-256, bis
16 MiB. Der vollständige validierte Plan bleibt als Review erhalten.

`PackageBindAptArchives` übernimmt sämtliche aufgelösten Archivbefunde, prüft die
geordneten gepinnten Dateien unabhängig und erzeugt daraus denselben Digest für
Vorbereitung und Publikation. Eine konkurrierende Änderungsliste im Kontext,
fehlende/vertauschte/veränderte Archive, ungeklärte Markierungen und abgelaufene
Quellen verhindern die Bindung. Entfernung darf keine Archiv-FDs einschleusen.

**Komponentenstand 6caa5bf2 besteht am 2026-09-30T06:53:22Z im lokalen QEMU
76/76 gezielte Gerätetests.** Tatsächlich signiert bezogene Archivbytes werden
mit dem Review verbunden. Ein geänderter Abhängigkeitsgrund verweigert im
registrierten Transaktionstest den Start; der ursprüngliche Plan bleibt nutzbar.
[Nachweis und Grenzen](../docs/component-tests.md).
Die öffentliche CLI/AOSP-Freigabe und der produktive Planer bleiben unverbunden. Der mechanische Paketarbeiter übernimmt diese
Markierungen noch nicht und vergleicht seine tatsächlichen Änderungen noch nicht
unabhängig mit dem Review. Die neue Bindung allein autorisiert keine Ausführung.


## Verbindung mit tatsächlichen Archiven: Komponentennachweis

`PackageMatchAptArchives` verknüpft die von APT ausgewählten Änderungen mit den
exakten Versionen/Architekturen in vollständigen authentifizierten Paketindizes.
Es prüft die Indexbytes gegen den bereits signaturbestätigten Beleg und bewahrt
automatische Markierungen. Relative Archivpfade, Größe und SHA-256 werden daraus
abgeleitet; widersprüchliche Inhalte werden abgewiesen. Ein separater Prüfschritt
verifiziert die gepinnten Archivdateien ohne Pfadsuche oder Ausführung.

Das Testrepository enthält jetzt echte auf `aegis-build` erzeugte Debianarchive.
APT muss veränderte Archivbytes ablehnen und die unveränderten Dateien per
lokaler `copy:`-Quelle beziehen. Die bisherige echte Skript-/Konfigurationsprüfung
bleibt getrennt bestehen. **Komponentenstand d4147584 besteht am
2026-09-30T06:37:35Z im lokalen Hintergrund-QEMU 67/67 gezielte Prüfungen.**
Die Ablehnung einer gleich großen verfälschten Datei, der anschließende Abruf
des Originals und die unabhängige native Byteprüfung sind tatsächlich bestätigt.
[Nachweis und Grenzen](../docs/component-tests.md).
Produktive Netzbeschaffung, unveränderlicher Planer,
Lebenszyklus, automatische Markierungen im Freigabe-/Ausführungsplan und frische
AOSP-Adminfreigabe sind weiterhin zu verbinden.

## APT-Auflösung und signierte Metadaten: Komponentenschritt

Der neue Adapter liest die vollständigen Änderungen aus APTs strukturiertem
JSON-Protokoll 0.2. Er bewahrt gewählte Versionen, automatische Abhängigkeiten und
abhängige Entfernungen. Kandidatenversion und tatsächlich gewählte Version sind
getrennt; unvollständige, widersprüchliche oder übergroße Ergebnisse scheitern.

Ein getrenntes Testrepository trägt echte Signaturen eines ausschließlich dafür
erzeugten Schlüssels. Die Gastprüfung verlangt eine gültige Signatur und
Index-Prüfsumme, lehnt fehlende/veränderte Signaturen sowie abgelaufene Releases
ab und lässt APT Installation, Update und abhängige Entfernung planen. Während
der Simulation sind dpkg und Zustandsänderungen gesperrt. **Dieser Schritt besteht am 2026-09-30T05:52:41Z im lokalen QEMU
53/53 gezielte Gerätetests**, einschließlich der tatsächlichen Signatur-/Frist-/
Indexablehnungen und der drei APT-Pläne. [Belege](../docs/component-tests.md).

Produktive Quellenbeschaffung, Archivbelege, unveränderlicher Planer samt
Auftragslebenszyklus, Übernahme automatischer Installationsmarkierungen und
Vergleich vor der tatsächlichen Ausführung fehlen weiterhin. Der öffentliche
Paketendpunkt bleibt inaktiv. [Testdaten und Protokoll](../packages/aegis/identity/runtime/package-apt-fixture.md).

## Bindung des aufgelösten Paketplans

Der neue interne `PackageBindResolvedPlan` erzeugt aus dem vertrauenswürdig
aufgelösten Plan sowohl Vorbereitung als auch Veröffentlichungsziel. Derselbe
versionierte SHA-256 umfasst Antragsteller/Seriennummer, Bereich, ursprünglichen
Befehl und Versionswunsch, Quellabbild und gemeinsame Basis, erwarteten bisherigen
Stand, Planerabbild, Quellen-/Schlüsselrichtlinie, anfänglichen Paketstatus sowie
Repository-Belege und sämtliche geplanten Versions-/Archivänderungen.

Die Codierung verwendet längenbegrenzte Felder und eine eindeutige Ordnung.
APT-Cache-Namen werden abgeleitet; insbesondere wird der Doppelpunkt einer
Versionsepoche nach [APT 3.0.3](https://github.com/Debian/apt/blob/3.0.3/apt-pkg/acquire-item.cc)
als `%3a` codiert ([QuoteString](https://github.com/Debian/apt/blob/3.0.3/apt-pkg/contrib/strutl.cc)).
Archive und ihre FDs müssen in derselben Reihenfolge übergeben werden.
Persönlicher Eigentümer bleibt der Antragsteller. Veränderte gemeinsame Basis,
abgelaufene Metadaten, fehlende exakte Wunschversion, doppelte/unsortierte Einträge
und nicht abbildbare gemischte Installations-/Entfernungseffekte werden abgewiesen.
Entfernung bewahrt Konfigurationsdateien; sie bedeutet kein Purge.

**Grenze:** Dies ist die Bindung eines bereits aufgelösten Plans, keine Prüfung
von Repository-Signaturen und keine Abhängigkeitsauflösung. Der künftige Adapter
muss die Release-/Packages-/Archivkette mit der unveränderlichen Vertrauensbasis
prüfen, Status und Quellen verifizieren und die Frist vor der frischen
AOSP-Bestätigung erneut kontrollieren. Ein Digest allein schafft kein Vertrauen.
Zusätzlich fehlt der unabhängige Vergleich der erwarteten mit den tatsächlich
geplanten APT-Effekten. Öffentliche CLI, produktive Ausführung und Adminfreigabe
bleiben deshalb unverändert inaktiv. **19/19 gezielte Gerätetests bestehen**
am 2026-09-30T05:19:21Z: 18 Bindungsprüfungen und ein echter registrierter
Vorbereitung-/APT-/Publikationsdurchlauf. Ein falscher Digest wird vor dem Start
abgewiesen, der richtige Auftrag bleibt ausführbar. [Belege und Grenzen](../docs/component-tests.md).
Der laufende Produktgast, Benutzer und Profilpaare bleiben unverändert.

## Produktintegration der Startauswahl

**Vollimage c7401f60 ist für die integrierte Startauswahl geprüft.** Die gezielte
SELinux-Korrektur behebt den mounton-Fehler von 6473cf51. Zwei echte AOSP-Benutzer
starten GNU-Kontexte aus ihren privaten vollständigen CE-Generationen. Gegenseitige
Datei-/Prozesszugriffe werden abgewehrt; Wechsel erlaubt geprüften Hintergrundbetrieb,
Logout beendet den jeweiligen Kontext, gibt das private Abbild frei und sperrt CE.
Derselbe Android-/KeyMint-Profilstand übersteht einen Neustart mit bytegleich
lesbaren Dateien und denselben ausgewählten Generationen. Der AOSP-Passwortwechsel
bewahrt Betas Daten und verweigert danach das alte Passwort.
[Konkrete Belege und Grenzen](../docs/component-tests.md).

14 zusätzliche CE-Probeaufrufe bestehen; die Veröffentlichung ihrer Testabbilder
läuft als Entwicklungsroot. Das ist keine Adminfreigabe oder Paketinstallation
über die öffentliche CLI. Die unveränderten Komponenten behalten die gesonderten
239 nativen und 130 Java-Testbelege. Enforcing, FBE und dm-verity sind in beiden
Boots bestätigt. Frühere Profilpaare bleiben erhalten; der sichtbare Launcher
bleibt unverändert.

**Nächste Verbindung:** öffentliche Paketbefehle samt vertrauenswürdiger Repository-
und Abhängigkeitsplanung, frischer AOSP-Adminbestätigung für beide Bereiche und
produktiver Paket-Ausführung. Danach sind gemeinsame/private Installationen,
Updates, Entfernung, Konflikte und tatsächlicher Logout während APT unter der
produktiven Sicherheitsrichtlinie nachzuweisen. Die fünfteilige Aufgabe bleibt offen.

Die folgenden Abschnitte dokumentieren die vorangegangenen Stände.

## Auftragssichere Fortsetzung im AEGIS-Service

Stand `95f2b925` besteht **234/234 native und 130/130 Java-Gerätetests** im lokalen
Hintergrund-QEMU. Der interne Brokerkanal v3 gibt beim Warten die bestehende
Auswahlkennung zurück. CONTINUE_START kann nur diesen Auftrag fortsetzen;
STOP, HELLO oder Ersetzung machen ihn ungültig. Es gibt keinen impliziten
Neustart nach einem Abbruch. Die Kennung ersetzt keine AOSP-Berechtigung.

Der Service kontrolliert bei jedem Versuch die ursprüngliche Anmeldung und
wartet außerhalb der kurzen CE-Zulassung. Vor der tatsächlichen Shellausführung
werden Kontext und Terminalkapazität erneut geprüft. Timeout und Unterbrechung
bestätigen keine Bereinigung. [Belege und Grenzen](../docs/component-tests.md).

**Nächster Schritt:** Der Produkt-Bootstrap muss Systemabbild und Receipt,
Auswahlhelfer und den optionalen festen gemeinsamen Store verankern und die
persönliche CE-Auswahl beim ersten START registrieren. Erst nach Helfer-/SELinux-
Integration und einem neuen vollständigen Build kann der Ablauf mit tatsächlicher
Anmeldung, gewählter Generation, Linux-Sitzung, Logout und Neustart abgenommen
werden. Öffentliche Paketbefehle, vertrauenswürdige Repository-Auflösung und
frische Adminfreigabe für beide Paketbereiche bleiben zusätzlich erforderlich.
Das laufende Image und der sichtbare Launcher wurden noch nicht ersetzt.

Die folgenden Abschnitte halten die vorherigen Komponentenstände fest.

## Auswahl einer gespeicherten Generation für die Runtime

Stand `93270f09` besteht **229/229 native Gerätetests und zwei zusätzliche Prüfungen
mit real gesperrtem AOSP-CE** im lokalen Hintergrund-QEMU. Der Brokerbesitzer hält
die gesamte Auswahl vom registrierten Auftrag vor CE-Zugriff über den fertigen
Mount bis zur nativen START-Übernahme oder dem bestätigten STOP/HELLO-Abbau.
Eine laufende Auswahl liefert EAGAIN; fehlgeschlagene Auswahl startet niemals
stillschweigend die Systembasis. Teilstarts behalten sämtliche Ressourcen.

Der asynchrone Helfer prüft persönlich -> gemeinsam -> unveränderliche Systembasis.
Beschädigte Stores, fremde Identitäten, fehlende ausgewählte Abbilder und
Sperrkonflikte ergeben Fehler. Persönliche Ableitungen müssen zur gemeinsamen
Basis passen. Ein weiterhin geöffneter alter Mount behält nach einem Update
seinen Inhalt. Der nur vorübergehend eingehängte private Anker wird nach dem
Klonen wieder entfernt und anhand echter Kernel-Mount-IDs kontrolliert.
Die optionale CE-Auflösung prüft Schlüssel und Identität vor Abwesenheit; sie
legt nichts an und repariert nichts. [Konkrete Belege und Grenzen](../docs/component-tests.md).

**Nächste Verbindung:** Produkt-Bootstrap sowie asynchroner Java-/CLI-Start mit
frischer Zulassung bei jeder Anfrage und Warten außerhalb des CE-Gates.
Der native Teilstart belegt Mount-Übernahme, noch keinen erfolgreichen vollständigen
GNU-Kontext mit ausgewählter Generation. Positiver entsperrter CE-Nachweis,
öffentlicher Paketkanal, vertrauenswürdige Repository-Planung, frische
AOSP-Adminfreigabe und produktive SELinux-/Zwei-Benutzer-Abnahme bleiben erforderlich.
Benutzer und sichtbarer Launcher wurden nicht verändert; das QEMU-Fenster bleibt
auf Wunsch geschlossen.

Die folgenden Abschnitte dokumentieren die früheren Komponentenstände.

## Durchgehende registrierte Transaktion

Stand `89adbb55` verbindet Vorbereitung, echtes Offline-APT mit Konsistenzprüfung,
Hashbildung und Auswahl des vollständigen Abbilds unter **einer Auftragskennung**.
**76/76 gezielte Gerätetests** bestehen im lokalen Hintergrund-QEMU. Der Auftrag
bindet Bereich, Antragsteller, Seriennummer, Plan, Store, Helfer, Größe und
erwarteten bisherigen Stand vor der späteren frischen AOSP-Startfreigabe.
Private Ziele bleiben beim Antragsteller; eine bestätigende Adminperson erhält
keine eigene Zielzuordnung. Die private Variante öffnet ausschließlich den
festen AOSP-CE-Bereich des Antragstellers nach Seriennummer-/Schlüsselprüfung.

Nach bestätigtem APT-Abbau nutzt der Publisher die gehaltene Arbeitsablage,
berechnet selbst den SHA-256 des stillgelegten Abbilds und prüft ihn beim Kopieren
erneut. Lange Hash-/Kopierarbeiten laufen in seinem registrierten Kindprozess.
Bestätigte Auswahl plus vollständige Ressourcenfreigabe liefern `Published`
mit Generationsmetadaten. Statusabfragen exportieren keine privaten FDs.
Abbruch, Benutzerstopp und Verbindungsabbau behalten auch während `Publishing`
die Zuständigkeit. Unbestätigte Prozessenden behaupten keinen Rollback.
Alte Generationen bleiben erhalten; zwischenzeitliche Auswahlkonflikte scheitern.
[Konkrete Installations-/Update-/Entfernungs- und Abbruchbelege](../docs/component-tests.md).

**Nächste Integration:** diese gespeicherten Generationen beim Runtime-Start
verwenden und den öffentlichen Paketbefehl mit vertrauenswürdiger Repository-
Planung sowie frischer AOSP-Adminbestätigung verbinden. Die produktive
SELinux-/CE-Abnahme, private Updates/Entfernung und echte Benutzerwechsel/
Abmeldung/Reboot unter Paketarbeit bleiben offen. Die neue Transaktion ist
bisher direkt als Entwickler-root geprüft, nicht über eine öffentliche CLI.
Der laufende Produkt-Broker und der sichtbare Launcher wurden nicht ersetzt.

Die folgenden Abschnitte halten frühere Bausteine mit ihren damaligen Grenzen
fest. Die ältere `BrokerPrepareCandidate`-Schnittstelle ohne gebundenes Ziel
bleibt weiterhin in `AwaitingValidation`; nur die neue vollständige Transaktion
führt die Veröffentlichung unter demselben Auftrag fort.

## Paketkonsistenz und Zuständigkeit nach APT

Stand `ef8242a1` besteht **44/44 gezielte Tests** im lokalen Hintergrund-QEMU.
Die feste Offline-Ausführung beendet zunächst ihre Nachkommen, prüft dann
AEGIS-Konten- und Dateivorgaben sowie mit Debian selbst Abhängigkeiten,
Paketmetadaten und Paketdateien. Veränderte Konfigurationen bleiben nach
`--force-confold` erhalten; neue fehlende Dateien oder veränderte Programme
werden abgewiesen. Absichtliche Auslassungen der gepinnten Slim-Basis werden
vor APT eng begrenzt und unveränderlich im vertrauenswürdigen PID1 erfasst.
Ein später überschriebenes Log erweitert diese Ausnahmen nicht.

Ein Paket-Hintergrundprozess unter technischer UID 42 wird vor der Prüfung
beendet und tatsächlich abgeholt. Ausschließlich der vertrauenswürdige PID1
behält dafür `CAP_KILL`; ausgeführter Paketcode behält die bisherigen sechs
Rechte. Native Konten-/Dateibaumprüfung vor den Debian-Prüfungen verhindert,
dass eine durch FIFO ersetzte Programmdatei die lesende Prüfung aufhält.
[Buildbeleg, konkrete Regressionen und Grenzen](../docs/component-tests.md).

Nach erfolgreichem APT und diesen Prüfungen hält derselbe registrierte Auftrag
die exakte Staging-Referenz (`AwaitingValidation`). Statusabfragen geben keine
private Dateireferenz weiter und verbrauchen den Auftrag nicht. Abbruch,
Benutzerstopp und Verbindungsabbau schließen die Referenz vor bestätigter
Abwesenheit. Ein erneutes Öffnen anhand eines CLI-Pfads ist nicht vorgesehen.
Der Paketdateivergleich ist keine Repository-Authentifizierung.

**Nächste Verbindung:** endgültige Hashbildung des vollständig stillgelegten,
eigenen Abbilds und Veröffentlichung/Aktivierung unter derselben Auftragsbindung.
Danach bleiben vertrauenswürdige Repository-Planung, öffentlicher Paketkanal,
frische AOSP-Adminbestätigung und produktive SELinux-/CE-Abnahme zu verbinden.
Die Tests nutzen die Besitzerbibliothek direkt als Entwicklungs-root; laufender
Produkt-Broker, sichtbarer Launcher sowie Daten- und KeyMint-Paar bleiben erhalten.

## Private CE-Ablage im vollständigen Produkt geprüft

Vollimage `ab38cf24` legt den privaten Paketbereich beim tatsächlich zugelassenen
Runtime-Start an. Der Bereich liegt neben HOME, bleibt root:root 0700 und erhält
den eigenen SELinux-Typ. Die Tests im lokalen Mac-QEMU bestätigen tatsächliche
AOSP-Anmeldung, vollständige private Basisabbilder, Kandidatenvorbereitung,
Abmeldung, Schlüsselsperre und Wiederöffnung nach Neustart desselben Daten- und
KeyMint-Paars. **16/16 opt-in Probeaufrufe** bestehen mit der reinen
ABX-Testkorrektur `76983c08`. [Genaue Belege und Grenzen](../docs/component-tests.md).

Die nativen Tests rufen Store und Besitzer direkt als Entwickler-Root auf.
Die Vorbereitung wird vor der echten AOSP-Abmeldung gestoppt. Das ist noch
kein Paketbefehl mit frischer Adminfreigabe und kein tatsächliches Logout
während produktiver APT-Ausführung. Die Java-/CLI-/Broker-Verbindung,
produktive Arbeiterdomäne, Repository-Planung, semantische Validierung und
Aktivierung kompletter Generationen bleiben offen. Erst danach folgen reale
Installations-/Update-/Entfernungstests für gemeinsame und private Software.

Der vorangegangene Komponentenstand `a08e3d7d` registriert private Aufträge vor
dem ersten Zugriff und löst ausschließlich den festen AOSP-CE-Pfad auf.
Store, temporäre Aufträge, Seriennummer und fscrypt-Policy gehören zum
Antragsteller; unterbrochene Aufträge werden nicht übernommen. Seine 187er
Standardsuite wurde für ab38cf24 erfolgreich wiederholt. Die folgenden
Abschnitte dokumentieren frühere Bausteine mit ihren damaligen Grenzen.

## Registrierte Vorbereitung kompletter Kandidaten

Stand `adce0475` besteht **183/183 native Tests** im lokalen Hintergrund-QEMU.
[Belege und Grenzen](../docs/component-tests.md). `BrokerPrepareCandidate`
verbindet die bisher fehlende langsame Vorbereitung mit demselben registrierten
Auftrag wie die APT-Ausführung: neue vollständige Basiskopie, Größen-/Hashprüfung,
getrennter beschreibbarer ext4-Mount und einzeln verifizierte APT-Archive.
Der feste hostseitige Helfer führt keine Paketskripte aus. Bestehende Archive
werden nur nach erneuter Inhaltsprüfung wiederverwendet; vorhandene Ablagen
werden nie übernommen. Die reale Broker-umask 0077 bleibt im Elternprozess
erhalten, während der separate Helfer korrekte Kandidatenrechte erzeugt.

Der Slot durchläuft `Preparing` und `Prepared`, bevor die bestehende frische
AOSP-Freigabe die Ausführung starten darf. Job, Antragsteller, Seriennummer und
Planhash bleiben unverändert. Sämtliche Teilstarts, Antwortwarteschlangen und
vorbereiteten Mounts gehören zum selben `STOP_USER`-/Wiederverbindungs-/Abschaltpfad.
Auch nach beendetem Kopierprozess bleibt ein vorbereiteter Mount eine gehaltene
private Ressource. Der Native-Nachweis bestätigt Abbruch während einer realen
Teilkopie, Abbau der Cgroup, Freigabe wartender/übernommener Mounts und Verschwinden
des ausschließlich zugehörigen Loopgeräts. Eigene Aufruferreferenzen müssen
weiter gesondert geschlossen werden. Reale AOSP-CE-Abmeldung ist damit noch
nicht bewiesen.

**Nächste Verbindung:** private CE-Ablage und produktive Cgroup-/SELinux-Einrichtung,
vertrauenswürdiger Repository-/Abhängigkeitsplaner mit kanonischen APT-Archivnamen,
frische AOSP-Adminfreigabe samt Java-/CLI-Anbindung, semantische Validierung und
Auswahl vollständiger Generationen. Gemeinsame/private Updates, Konflikte und
Reboot müssen anschließend im integrierten Vollimage geprüft werden. Der
bisherige Gast bietet noch keinen produktiven Paketendpunkt. Die älteren
Bausteinnachweise folgen mit ihren jeweils damaligen Grenzen.

## Brokerverwaltete Paket-Ausführung

Stand `90b9732d` besteht am 2026-09-29T21:31:27Z **167/167 native Tests**
im lokalen Mac-QEMU. Der neue `PackageExecutor` führt echtes Debian-APT in
vollständigen, ausschließlich für den Auftrag erzeugten ext4-Kopien aus.
Installation, Update, Konfigurationserhalt, Entfernung, technische Eigentümer
und laufende Paketskripte sind nachgewiesen. [Belege und Grenzen](../docs/component-tests.md).

Vorbereiten, Starten, Beobachten und Abbrechen gehören jetzt zum selben nativen
Brokerbesitzer wie Veröffentlichungen. `STOP_USER`, Wiederverbindung und
Abschaltung besuchen beide Arten; Teilstarts bleiben bis zum tatsächlichen
Aufräumen registriert. Beide teilen 16 Plätze, monotone IDs und höchstens einen
nicht abgeholten Auftrag je Antragsteller. Start/Status/Abbruch sind an ID,
Seriennummer, Auftrag und Plan gebunden. Die API erhält keine separate
Admin-Zielkennung und erteilt selbst keine Freigabe.

Die Startübergabe benutzt ein gemeinsames begrenztes Zeitbudget und übernimmt
alle vorbereiteten FDs. Der künftige AOSP-Aufrufer muss sie innerhalb seiner
bestehenden Zulassung registrieren und erst danach das Gate freigeben. Kopieren und Hashen laufen inzwischen in der oben beschriebenen registrierten
Vorbereitung. Die vertrauenswürdige Repository-Prüfung und Planung bleiben offen. Der Aufrufer muss seine Originalreferenzen gesondert
schließen. Ein eigener cgroup-begrenzter Namespace-PID1 erhält nur den
Kandidaten, eine private Geräteansicht und versiegelte Auftragsdaten. Nach
Abhängen Androids, vollständiger Mountprüfung und Rechtebegrenzung startet er
festes APT mit lokal bereitgestellten Archiven; keine frei gewählten Befehle.

Bestätigtes Ende setzt PID1-Reaping, leere/entfernte Cgroup und geschlossene
übernommene Referenzen voraus. Timeouts behalten den Besitzer. Ein getöteter
Arbeiter ist `Unconfirmed`, nicht vermeintlich zurückgerollt. Selbst erfolgreiches
APT liefert lediglich `NeedsValidation`. Abbruch nach natürlichem Ende und vor
Abholung widerruft auch diese Weitergabe. `remove` erhält Konfiguration;
Purge war nur Teil der älteren separaten Probe und wird nicht implizit angeboten.

Normale Runtime-Sitzungen behalten ihre Gruppensperre. Ausschließlich die
Paketvorbereitung erlaubt Gruppenwechsel innerhalb derselben festen Abbildung
und einen beschreibbaren Kandidaten; sie erhält keinen persönlichen HOME-Mount.
Der Produktionseinstieg verlangt eine eigene genaue SELinux-Domäne, die bislang
nicht aktiviert wurde. Die Testausführung verwendet einen getrennten Test-Einstieg
mit gemeinsamem Kern; ein produktiver SELinux-/CE-Nachweis wird nicht behauptet.

**Weiterhin offen:** Vertrauenswürdiger Planer,
produktive Cgroup-/SELinux-Einrichtung, private CE-Ablage, frische AOSP-Adminfreigabe
samt Java-/CLI-Anbindung, semantische Validierung und Auswahl vollständiger
Generationen. Gemeinsame/private Versionen, Rebase-Konflikte, tatsächliches
AOSP-Logout während APT und Reboot benötigen ein neues integriertes Vollimage
und reale Benutzertests. Im bislang laufenden System gibt es weiterhin keinen
freigeschalteten Paketendpunkt. Die folgenden älteren Bausteinnachweise bleiben
mit ihren jeweiligen Grenzen erhalten.

## Begrenzte Rechte für den isolierten Paketarbeiter

Die interne Funktion `aegis_limit_package_worker` erhält im eigenen, exakt
zugeordneten Benutzer-/PID-Namespace nur sechs für Dateieigentümer und
technische Konten nötige Capabilities. Sie sperrt zusätzliche Rechte und
Mount-/Namespace-Manipulationen vor Ausführung von Paketcode. Ein normaler
UID-Wechsel entfernt die wirksamen Rechte. Der Aufrufer muss vorher Androids
Wurzel abgehängt, einen exklusiven Kandidaten bereitgestellt und die genaue
SELinux-Domäne geprüft haben; diese Funktion allein erteilt keine AOSP-Freigabe.
Fehler nach Beginn des Rechteabbaus sind terminal, ohne Rückfall oder Retry.

Stand `ac01f261` besteht lokal **157/157 native Tests**, einschließlich eines
echten Pivot-/Exec-/UID-Wechsels in einer eigenen leeren tmpfs.
[Vollständiger Nachweis](../docs/component-tests.md). APT, produktiver
Arbeiteraufbau, vollständige Generationen und CE-Lebenszyklus bleiben offen.
Die Funktion wird noch von keinem produktiven Paketpfad aufgerufen.

## Gemeinsamer Ressourcenbesitzer und Abmeldung

Der native Brokerbesitzer verwaltet jetzt auch Vorbereitungen und laufende
Veröffentlichungen. Seine tatsächlichen `STOP_USER`-/`HELLO`-/Abschaltpfade
besuchen Paketarbeiten mit derselben Benutzerkennung und schließen ihre
FDs vor bestätigtem `ABSENT`. Ein Fehler beim Aufräumen eines Auftrags lässt
andere Aufträge nicht aus. Teilstarts bleiben gesperrt und registriert.
Der produktive Broker-Loop erhält außerdem nicht blockierendes Reaping;
ein verschwundener CLI-Client muss seine Paketarbeit nicht selbst aufräumen.

Vorbereitungen kopieren den vollständigen Auftrag und vertrauenswürdige FDs
unter der vorhandenen AOSP-Zulassung. Der Aufrufer muss seine eigenen FDs vor
Freigabe des Gates schließen. IDs werden vom Besitzer vergeben und innerhalb
seiner Laufzeit nicht wiederverwendet. Start und Abbruch eines falschen
Benutzers, einer anderen Seriennummer oder eines anderen Plans greifen nicht
auf den Auftrag zu. Eine gültige Startübergabe verbraucht die Vorbereitung.
Fertige Antworten enthalten keine offenen privaten Deskriptoren; ein passendes
Poll konsumiert sie. Die feste Kapazität beträgt 16 Plätze und höchstens einen
nicht abgeholten Auftrag je Antragsteller, einschließlich fertiger Antworten.
Damit kann ein Benutzer nicht alle globalen Plätze durch liegen gelassene
Ergebnisse belegen. Gemeinsame Store-Schreiber werden zusätzlich
vom bereits implementierten Store serialisiert.

Diese Änderungen sind direkt in die native Besitzlogik eingebunden, aber noch
nicht im bisher laufenden Vollimage installiert. Stand `74bb0db9` besteht am
29. September 2026 um 19:29:19 UTC alle **155/155 nativen Tests**, einschließlich
der acht direkten Besitzer-/Lifecycle-Tests und einer zusätzlichen Prüfung der
AOSP-Helfergruppe. [Vollständiger Nachweis](../docs/component-tests.md).
Der tatsächliche AOSP-Logout mit privatem CE und laufendem
APT bleibt offen. Ebenso fehlen native Planung, der öffentliche Paketkanal,
Java-Anbindung mit Verbindungswiderruf und die produktive Cgroup-/SELinux-
Einrichtung für den Publisher. Es wird keine funktionierende Paket-CLI behauptet.
[Native Steuerung](broker-control.md).

## Eigener Prozess für die Veröffentlichung

`PackagePublisher` startet jetzt einen separat verwalteten Prozess für Hashen,
Kopieren und die Auswahl eines bereits vollständig geprüften Paketabbilds.
Der kurze Start erhält ausschließlich vertrauenswürdige FDs und einen an den
Antragsteller gebundenen Auftrag. Er führt keine Paketauflösung oder Dateikopie
innerhalb des AOSP-Zulassungsgates aus. Der aufrufende Broker muss auch einen
fehlgeschlagenen Teilstart vor Freigabe des Gates beim Lebenszyklus registrieren.
Die native Registrierung ist nun im Brokerbesitzer implementiert und direkt
getestet; die Verbindung zum produktiven Planer, AOSP-Dienst und privaten
CE-Store bleibt offen.

Der Prozess wird mit stabilem pidfd unmittelbar in einer eigenen begrenzten
Cgroup angelegt; nach dem Raw-Clone erfolgt ausschließlich ein fester Exec
mit versiegelter Konfiguration und den benötigten Deskriptoren. Andere geerbte
FDs, Signaleinstellungen und Umgebungsvariablen werden nicht weitergereicht.
Der Helfer erhält keine Passwörter oder CLI-Pfade und führt keinen Paketcode
oder APT aus. Seine Herkunft und die semantische Konsistenz des Kandidaten
muss der vertrauenswürdige Aufrufer vorher prüfen. Er ersetzt keinen späteren
isolierten APT-Arbeiter und besitzt noch keine produktive SELinux-Anbindung.

Ein erfolgreiches Ende verlangt tatsächliches Reaping des Kindes, eine
bestätigt leere und entfernte Cgroup und das Schließen aller eigenen
Quell-/Store-FDs. Ein Timeout behält die Ressourcen zur weiteren Bereinigung.
Eine Abbruchanforderung allein bestätigt nichts. Bei erzwungenem Prozessende
oder fehlender gültiger Antwort bleibt die Veröffentlichung unbestätigt;
insbesondere wird kein Rollback behauptet. Alte Generationen und unausgewählte
Reste werden nicht automatisch gelöscht. Caller-eigene FDs bleiben ausdrücklich
Verantwortung des Callers; nur dieser kann vollständige CE-Freigabe bestätigen.

Acht neue lokale Gerätetests sind im Stand `d4fdb778` ausgeführt: reale Veröffentlichung durch
den Kindprozess, eigene FD-Kopien, privater Eigentümer/Seriennummer, paralleler
Startkonflikt, falscher Hash, unzulässiger Auftrag/Quell-FD, prozessgebundener
Besitz und Abbruch während einer beobachteten unvollständigen Kopie. Die
Fixtures verwenden ausschließlich eigene Cgroups und inerte Dateien unter
`/data/local/tmp`. Es gibt damit noch keinen echten privaten CE-/Logout- oder
APT-Nachweis. Auf dem Server kompilierter Stand `d4fdb778` besteht am
29. September 2026 um 18:49:41 UTC **146/146 native Tests** im lokalen
`927cf51d`-Gast. Der Abbruchfall bestätigt eine noch unvollständige Kopie,
Timeout mit erhaltener Verantwortung, tatsächlichen Prozessabbau und danach
die bisherige Auswahl. Benutzer-/CE-Bestand bleibt identisch, Enforcing und
Broker bleiben aktiv. [Vollständiger Nachweis](../docs/component-tests.md).
Das Komponententransportprofil v2 ergänzt den exakt inventarisierten Helfer;
ältere Release-Belege bleiben mit ihren jeweiligen gepinnten Werkzeugen gültig.

## Aktionsbindung und begrenzte Übergabe

`PackageApproval` bindet einen vorbereiteten Auftrag unveränderlich an den
authentifizierten Antragsteller mit ID/Seriennummer, expliziten Bereich,
Aktion und die vom vertrauenswürdigen Planer bestimmte Auftragskennung samt
Plan-Digest. Bei `user` bleibt der private Eigentümer der Antragsteller,
auch wenn eine andere Person ihr Adminpasswort bestätigt. Bei `all` wird
kein privater Admin-Zielbereich erzeugt. Kennung und Digest allein gewähren
keine Rechte und dürfen später nicht ungeprüft aus CLI-Eingaben stammen.

`AospPackageAuthority` ist der reale Adapter: Er prüft AOSP-Zustand und
Paketbeschränkungen des Antragstellers vor und nach Bestätigung. Die frische
Adminprüfung läuft ausschließlich über den zuvor implementierten lokalen
LockSettings-Adapter; bei dessen Fehlen existiert kein Rückfall auf eine
vorhandene Sitzung oder den normalen Login. Das Credential wird verbraucht
und überschrieben, auch bei früher Ablehnung oder einem doppelten Aufruf.

Eine Vorbereitung kann nur einmal bestätigt werden. Ein paralleler Verlierer
kann weder eine zweite Prüfung starten noch den Auftrag des Gewinners
abbrechen. Ablehnung oder Widerruf nach erfolgreicher Übernahme der Vorbereitung
widerruft genau diesen Auftrag. Beginnt die native Übergabe und scheitert
deren Bestätigung oder der anschließende Sitzungscheck, lautet das Ergebnis
ausdrücklich unbestätigt; ein unveränderter Paketbestand wird nicht behauptet.
Abbruchanforderung ist kein Beweis für Arbeiterende oder CE-Freigabe.

Die `Handoff`-Schnittstelle verlangt eine kurze Registrierung unter der
vorhandenen Runtime-Zulassung mit erneutem Sitzungscheck. Die native Instanz
muss alle Ressourcen auch bei einem fehlgeschlagenen Start übernehmen und
bis zur bestätigten Beendigung halten. Kopieren, Hashen und APT gehören nicht
in diese kurze Übergabe. Der Aufrufer bekommt keine wiederverwendbare
Adminfreigabe zurück; Rückkehr bedeutet lediglich bestätigte Übergabe.

**Noch nicht verbunden:** Es gibt keinen produktiven Planer oder nativen
Paketarbeiter, keine Paket-CLI und keine Implementierung dieser `Handoff`-
Schnittstelle im Broker. Auch die Abbruch-/CE-Verantwortung der tatsächlichen
Arbeit ist deshalb noch nicht erfüllt. Der unveränderliche Plan muss später
vollständige Versions-/Abhängigkeitsauflösung, Ausgangsgenerationen und etwaige
Rückkehr auf gemeinsame Versionen enthalten. Die neuen Koordinatortests
verwenden kontrollierte Authority-/Handoff-Fixtures; sie ersetzen keine echten
Adminpasswörter, Paketinstallationen oder Abmeldungen während APT.

Stand `09b10fd7` ist auf dem Server kompiliert und im unveränderten lokalen
`927cf51d`-Gast geprüft: **119/119 Java-Tests** bestehen am 29. September 2026
um 18:27:09 UTC, einschließlich der 14 neuen Koordinator-Fixtures. Benutzer-,
CE- und Kontextbestand bleiben identisch; Enforcing und Broker laufen weiter.
Die oben beschriebenen Grenzen zur realen Dienst-/Arbeiteranbindung gelten
weiterhin. [Vollständiger Nachweis](../docs/component-tests.md).

## Separater Schritt: AOSP-Passwortbestätigung ohne Anmeldung

Der neue interne `AegisPackageCredentials`-Adapter wird von LockSettings in
`LocalServices` registriert. Er hat keinen Binder-Endpunkt. Die Paket-CLI und
der Broker rufen ihn noch nicht auf: Aktions-/Planbindung, anfordernde Sitzung,
privater Eigentümer, CE-Lebenszyklus und der Paketarbeiter fehlen weiterhin.
Ein erfolgreiches Ergebnis dieses Adapters allein darf keine Paketaktion starten.

Die bestehende AOSP-Methode `verifyCredential` führt bei Erfolg auch
`onCredentialVerified` aus und entsperrt damit Keystore, CE und den Benutzer.
Der zusätzliche interne Pfad prüft stattdessen denselben vorhandenen
LSKF-Protektor über `SyntheticPasswordManager.unlockLskfBasedProtector` und
übergibt nur bereinigten Status beziehungsweise AOSPs Wiederholungsfrist.
Er ruft keine Benutzer-/CE-/Keystore-Entsperrung auf und fordert keinen
Gatekeeper-Passwort-Handle an. Erfolgsbenachrichtigungen einer Anmeldung,
Escrow-Aktivierung und biometrische Entsperr-Nacharbeit entfallen. Der normale
AOSP-Anmeldepfad bleibt unverändert.

Dies ist **keine nebenwirkungsfreie Kryptoprüfung**: Die bestehende AOSP-Routine
aktualisiert Gatekeeper-Hardware-Auth-Tokens und kann ihre eigenen Protektor-
Metadaten nachführen oder neu einschreiben. AOSPs Hardware-Sperrzeiten gelten
weiter; bei Sperrzeit wird weiterhin StrongAuth angefordert. Es gibt keine
zweite Passwortdatenbank, eigenen Passwortvergleich oder neue Kryptographie.
Synthetic Passwords, HATs und Handles werden nicht an den Adapter-Aufrufer
ausgegeben oder als Paketberechtigung gespeichert.

Der Adapter prüft vor und nach der Passwortprüfung Adminstatus, vollständigen
persönlichen AOSP-Benutzertyp, Aktivierung, ID und Seriennummer sowie die
passende Installations-/Entfernungsbeschränkung und `DISALLOW_APPS_CONTROL`.
Er verbraucht das übergebene Credential auch bei Ablehnung am Prozesseingang.
Nur system_server darf diesen Eingang nutzen; blockierende Passwortprüfungen
auf dem Hauptthread sind ausgeschlossen. Ein späterer Koordinator muss
zusätzlich Beschränkungen und Lebenszyklus **des Antragstellers** prüfen und
das private Ziel aus dessen Sitzung ableiten, unabhängig vom bestätigenden Admin.

Die Quellintegration verwendet Belegschema 6 und akzeptiert frühere bekannte
Schemas unverändert zur kontrollierten Aktualisierung. Unbekannte Änderungen
werden nicht übernommen. Die 14 neuen Gerätetests prüfen isolierte
Benutzer-/Prüfantwort-Fixtures; sie authentifizieren keinen echten Benutzer und
beweisen nicht die CE-Nebenwirkungen des neuen LockSettings-Pfads. Ein gebautes
und gestartetes neues Systemimage samt echter AOSP-Bestätigung bleibt dafür
erforderlich. Bis dahin wird der sichtbare, bekannte Startstand nicht ersetzt.

Stand `28811521` ist auf dem Server kompiliert und als Test-APK im unveränderten
lokalen `927cf51d`-Gast geprüft: **105/105 Java-Tests** bestehen am
29. September 2026 um 17:59:39 UTC, einschließlich aller 14 neuen Adapter-Fixtures.
Benutzer-/CE-Bestand ist vorher und nachher identisch; Enforcing und Broker
bleiben aktiv. Die zuvor beschriebene Grenze zur echten Systemserver-Prüfung
gilt weiterhin. Der erste Durchlauf enthielt ein falsch typisiertes Gast-Fixture;
dessen Korrektur änderte nur Testcode. [Vollständiger Nachweis](../docs/component-tests.md).

## Implementierungsschritt: Auswahl und unveränderliche Abbilder

`package_store.{h,cpp}` implementiert einen internen Speicherbaustein. Er ist
nur in die nativen Gerätetests eingebunden, noch nicht in den produktiven
Broker. **Es gibt damit noch keine funktionierende Paketinstallation.**
Die vorhandene CLI und ihre Berechtigungen werden durch diesen Baustein
nicht verändert. Stand `8e1c2228` ist auf `aegis-build` kompiliert und über den
[verifizierten Release](https://github.com/simgero/AegisOS/releases/tag/components-20260929T162856Z-8e1c2228-8e1c2228-OrbqxJ)
in lokalem Mac-QEMU geprüft: **138/138 native Tests** aus 20 Suiten bestehen,
einschließlich aller zehn neuen Paket-Store-Tests. Das Ergebnis belegt diesen
Baustein, keine vollständige Paketverwaltung.

Die vorgesehene Transaktion erzeugt ein vollständiges, konsistentes
Dateisystemabbild mit Programmen, Abhängigkeiten, Paketdatenbank, Konfiguration
und technischen Kennungen. Eine private Generation ist zusätzlich an den
gemeinsamen Ausgangsstand gebunden. Sie wird nicht als frei beschreibbare
Dateischicht über einer später veränderten gemeinsamen Basis ausgeführt.
Die tatsächliche APT-Auflösung, Ausführung von Installationsskripten und
semantische Validierung dieses vollständigen Bestands müssen noch folgen.

Der neue Speicherbaustein erhält ausschließlich ein vom vertrauenswürdigen
Aufrufer geprüftes Verzeichnis-FD. Dieser muss zuvor AOSP-Autorisierung sowie
bei privatem Scope CE-Zustand und Seriennummer prüfen. Der persistente Marker
enthält Bereich und technische AOSP-Zuordnung; er ist keine zweite persönliche
Identitäts- oder Passwortverwaltung. Der Marker gewährt selbst keine Rechte.
Ein geteilter Store verwendet kein persönliches Konto. Sein Verzeichnis ist
Root-eigen mit Modus 0700, private Stores liegen später innerhalb des zugehörigen
CE-Bereichs. Die Anlage und dauerhafte Verankerung des Elternverzeichnisses
gehören zum noch zu integrierenden Besitzer, nicht zu dieser FD-Schnittstelle.

Der Baustein:

* öffnet Pfade relativ zum verankerten FD, ohne Symlink- oder Mountübergänge;
* prüft Root-Eigentümer, exakte Modi, Linkanzahlen und fehlende POSIX-ACLs;
* bindet eine private Auswahl an Benutzer-ID **und Seriennummer**;
* serialisiert unabhängige Schreiber mit einer Prozesssperre und Threads
  desselben Objekts mit einem Mutex;
* verlangt als Ausgangsstand den vollständigen zuvor gelesenen Datensatz,
  einschließlich gemeinsamer Basis und Größe, nicht nur denselben Imagehash;
* kopiert einen vollständig validierten Kandidaten in eine eigene Datei,
  prüft dabei Größe, SHA-256 und unveränderte Quellmetadaten und synchronisiert
  die Kopie, bevor sie auswählbar wird;
* schließt seine schreibenden Datei-FDs vor der Veröffentlichung und gibt
  ausschließlich unabhängig geöffnete Readonly-FDs aus;
* wählt ein vollständiges Abbild durch atomare Umbenennung eines vorher
  synchronisierten Datensatzes und anschließendes Verzeichnis-fsync;
* erhält alte Abbilder und bereits geöffnete Lesereferenzen. Es gibt keine
  automatische Bereinigung alter Generationen oder unvollständiger Reste.

Normale Runtime-Prozesse dürfen diese Store-Verzeichnisse und Verwaltungs-FDs
nicht erhalten. Laufende Kontexte behalten ihre bereits gebundene Generation.
Die nötige SELinux-/Broker-/Mountanbindung ist noch nicht implementiert.
Ein Hash ist weder Adminfreigabe noch Nachweis einer konsistenten APT-Auflösung.

## Abbruch und ausstehende Integration

Abbruch wird beim Kopieren und vor dem Auswahlpunkt geprüft. Vorherige Fehler
geben keine neue Auswahl frei. Scheitert die dauerhafte Bestätigung **nach**
der Umbenennung, lautet das Ergebnis ausdrücklich `Unconfirmed`: Der Aufrufer
darf weder Erfolg noch eine unveränderte Auswahl behaupten, sondern muss den
tatsächlichen Zustand erneut prüfen. Das ist keine Simulation eines physischen
Stromausfalls. Unausgewählte Dateien werden beim Öffnen nicht still aktiviert.

Hashprüfungen und Kopieren gehören in einen vom Ressourcenbesitzer kontrollierten,
abbrechbaren Arbeiter. Sie dürfen nicht innerhalb des zehnsekündigen
Runtime-/AOSP-Storage-Gates oder des synchronen Systemserver-Binderpfads laufen.
Vor CE-Sperrung müssen dieser Arbeiter und sämtliche privaten Datei-/Mount-
Referenzen beendet sein. Diese Kopplung ist weiterhin offen.

Weitere notwendige Schritte für die vollständige Paketverwaltung:

1. Aktionsgebundene frische AOSP-Adminfreigabe für beide Bereiche; das private
   Ziel bleibt auch bei Freigabe durch einen anderen Admin der Antragsteller.
2. Vertrauenswürdig bezogene Paketquellen, exakte Versionsauflösung und
   beschränkter APT-/dpkg-Arbeiter ohne Hostrechte oder fremde CE-Mounts.
3. Konsistenzprüfung von Dateien, Datenbank, Abhängigkeiten, Konfiguration und
   technischen Konten; private Vorgaben bei gemeinsamen Updates erhalten.
4. Broker-/CE-/SELinux-Anbindung, Status ausstehender Aktivierung, Konfliktprüfung
   beim nächsten Runtime-Start, Abbruch/Logout und Wiederanlauf.
5. Tatsächliche Installations-, Update- und Entfernungstests in beiden Bereichen
   mit zwei Benutzern, verweigerter Freigabe, unterschiedlichen privaten
   Versionen, konkurrierenden Transaktionen und vollständigem Reboot.

Die zehn ausgeführten Gerätetests verwenden kleine inerte Textdateien unter
`/data/local/tmp`, keine echten Paketimages oder persönlichen CE-Stores. Sie
prüfen Auswahl, Reopen, alte offene Referenzen, private Basisbindung, falsche
Eigentümer, Abbruch, beschädigte Quellen/Metadaten, konkurrierende Schreiber,
Symlinks/Hardlinks, verwaiste nicht ausgewählte Dateien und Prozessbindung.
Ein Test beendet einen bestätigten Publisher mit `_exit`, ohne Destruktoren;
er behauptet keinen Fehler während des Kopierens und keinen Stromausfalltest.
Der Abbruchtest verwendet ein bereits vor Beginn gesetztes Abbruchsignal;
Abbruch während des Kopierens und ein fehlschlagendes fsync nach Umbenennung
sind nicht durch diesen Durchlauf nachgewiesen.

## Tatsächlicher Gastlauf

Am 29. September 2026 um 16:31:01 UTC besteht die komplette native Suite in
15.076 ms, die zehn neuen Tests benötigen 119 ms. Image `927cf51d`, Profil
`d68845b3-62a9-4181-a7cd-c0f0a8e7d316`, Boot-ID
`984f23bd-607e-4a6d-8c08-7ae91bccd4f5`. Vorher und nachher bestehen nur
Benutzer/CE-Schlüsselverzeichnisse 0; es gibt keine persönlichen Kontexte.
Enforcing und der unverändert laufende produktive Broker sind bestätigt.
Der Broker enthält diesen neuen Baustein noch nicht. Zum Zeitpunkt dieses
nativen Laufs bestand der separate 91er-Java-Nachweis von `be9d0d54`;
der neuere 119er-Java-Lauf ist oben getrennt belegt.

Der erste Versuch scheiterte bereits beim Übertragen der Testprogramme: Das
als Root angelegte Ziel war für den authentifizierten ADB-Shellbenutzer nicht
beschreibbar. Es wurde kein Test gestartet. Die korrigierte Vorbereitung
verwendet ein neues Shell-eigenes Ziel, überträgt die vier geprüften Programme,
übergibt danach Dateien und Verzeichnis an Root und prüft erst dann alle
Dateihashes vor Ausführung. Der fehlgeschlagene Versuch bleibt erhalten.

Nachweise: `out/components-8e1c2228/component-tests-attempt2/`. `native.log`
hat SHA-256 `71fb61d07b4ad333690ad61be630877150fc78ad345a369078cf7f484490067d`.
Die identischen Vorher-/Nachherdateien haben SHA-256
`0fbf6d9f89f00d69d9d3df295f40a17cb6f514a52250a721c905b1ba7998c4b3`.
