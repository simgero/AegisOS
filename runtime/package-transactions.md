# Vollständige Paketgenerationen

## Auswahl einer gespeicherten Generation für die Runtime

Stand `c60d6ea2` besteht **86/86 gezielte Gerätetests** im lokalen Hintergrund-QEMU.
Ein asynchroner, beaufsichtigter Helfer wählt und prüft die persönliche Generation,
sonst den gemeinsamen Stand, sonst die unveränderliche Systembasis. Vorhandene
beschädigte Stores, fremde Identitäten, fehlende ausgewählte Abbilder und gesperrte
Stores ergeben einen Fehler. Persönliche Ableitungen müssen zur gemeinsamen
Basis passen; Konflikte werden nicht durch stilles Zurückfallen verdeckt.

Der Helfer liefert einen vollständigen readonly/nosuid/nodev/noexec-Mount nach
bestätigtem Prozess-/Cgroup-Abbau. Langsame Prüfsummen bleiben außerhalb der kurzen
Anmeldezulassung. Die neuen Tests führen auch private Installation, Update und
Entfernung aus. Nach einem gemeinsamen Update zeigt ein weiterhin geöffneter Mount
die alte Version, ein neu ausgewählter die neue. [Belege und Grenzen](../docs/component-tests.md).

**Nächste Verbindung:** `Broker START` und STOP/HELLO/Logout müssen diese Auswahl
selbst besitzen: Registrierung vor CE-Zugriff, sichere read-only-Auflösung eines
optional vorhandenen privaten Stores, asynchrone Statusabfrage, Übernahme genau
des geprüften Mounts und vollständiger Abbau bei Stopp. Bestehende Kontexte bleiben
bis zum kontrollierten Neustart auf ihrem Stand. Der Auswahlhelfer allein aktiviert
noch keine Produkt-Runtime. Anschließend bleiben öffentlicher Paketkanal,
vertrauenswürdige Repository-Planung, frische AOSP-Adminfreigabe und produktive
SELinux-/CE-/Zwei-Benutzer-Abnahme zu verbinden. Benutzer und sichtbarer Launcher
wurden nicht verändert; das QEMU-Fenster bleibt geschlossen.

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
