# Erste AEGIS-Terminalintegration

Stand 29. September 2026: Im Vollbuild `2f29f0ac` sind zwei tatsächliche GNU-
Kontexte, AOSP-Anmeldung, Wechsel, Logout mit CE-Sperre und Dateierhalt nach
Neustart geprüft. Beide ersten Anmeldungen und beide ersten Zugänge nach
Reboot funktionieren ohne vorherigen Fehlversuch. Eine während der
Passwortabfrage widerrufene Vorbereitung bleibt auch mit korrektem Passwort
abgewiesen. Der frühere nachträgliche Terminalwiderruf wurde in diesem
Durchlauf nicht beobachtet. Details und Grenzen:
[GNU-Test](runtime-gnu-qemu-test.md); der frühere reine AOSP-Test bleibt im
[CLI-Gasttest](identity-cli-qemu-test.md) dokumentiert. Paketverwaltung und
verwaltete Benutzerlöschung fehlen weiterhin.

## Geprüfte Anmeldereihenfolge

Der neue Quelltext trennt die Auswahl des Android-Anmeldeziels von der
Passwortprüfung. Die bereits autorisierte Entwicklungskonsole wählt zuerst
den persönlichen Vordergrundbenutzer, entsprechend einer Auswahl auf dem
Android-Anmeldebildschirm. Der Dienst wartet begrenzt, bis AOSPs
`getCurrentAndTargetUserIds()` das Ziel als aktuellen Benutzer und keinen
laufenden Wechsel mehr meldet. Die frühe Änderung von `getCurrentUserId()`
allein genügt nicht. Erst danach fordert die CLI das Passwort an.

Diese Auswahl gewährt keine persönliche Sitzung, keine Adminrolle, keinen
GNU-Start und keinen zusätzlichen CE-Zugriff. Ein falsches Passwort kann
daher das bereits sichtbare Android-Anmeldeziel gewechselt haben, muss aber
dessen gesperrte Daten und Runtime unzugänglich lassen. Ein zuvor bereits
entsperrter anderer Benutzer darf weiter im Hintergrund laufen. Normale Apps
und Runtime-Prozesse erhalten keinen Zugriff auf diese Entwicklungsschnittstelle.

Die Vorbereitung ist an die ursprüngliche Terminalsitzung, Benutzer-ID,
Seriennummer und aktuelle Widerrufszähler gebunden und nur einmal nutzbar.
Sperre, Benutzerstopp oder Sitzungsende verwerfen sie. Nach der Passworteingabe
folgt eine frische AOSP-Prüfung; die endgültige Bindung verlangt weiterhin
unveränderte Widerrufszähler, bestätigtes CE und den abgeschlossenen richtigen
Vordergrundwechsel. Kein alter Passwortnachweis wird nach einer Sperre
wiederverwendet. Die bisherigen Widerrufe offener GNU-Terminals bleiben bestehen.

Sechs neue Komponententests betreffen Einmaligkeit, fremde/wiederverwendete
Identitäten, Sperre, Benutzerstopp, Abbruch und konkurrierende Verwendung.
Commit `2f29f0ac` ist auf `aegis-build` kompiliert und über GitHub übertragen;
**68/68 Java-Komponententests bestehen** im lokalen QEMU. Zusätzlich wurde der
vollständige Build mit installiertem Dienst geprüft: vor Passwortübermittlung
unverändertes Ziel-CE und kein Kontext, verzögert stabile erste Anmeldungen vor
und nach Reboot, falsches/altes Passwort bei gesperrtem CE und Verweigerung
einer durch reale Bildschirmsperre widerrufenen Passwortabfrage. Offene GNU-
Terminals werden bei Wechsel und Sperre weiterhin entzogen. Ein kompletter
Zwei-Benutzer-Durchlauf ersetzt keine erschöpfende Konkurrenzprüfung.
[Komponentenbelege](component-tests.md), [Dienstablauf](runtime-gnu-qemu-test.md).

## Erster Komponentenstand (25fde995)

- `aegis-identity-core`: Adapter zu AOSP für persönliche Benutzer, Passwörter,
  Entsperrung und bestätigten Android-Benutzerstopp.
- `aegis-identity-protocol`: interne Binder-Schnittstellen.
- `aegis-identity-service`: vorgesehener Systemserver-Dienst. Veröffentlicht
  `aegis_identity` erst bei ausdrücklich passender Entwicklungskonfiguration.
- `aegis`: Terminalprogramm mit verdeckter Passworteingabe und interaktiver Sitzung.
- `AegisIdentityTests`: Android-Tests für die Eigentümerschaft von
  Credential-Kopien bei lokalen Binder-Aufrufen sowie die tatsächlich wirksame
  Framework-Dienstliste, Mehrbenutzervorgaben und die fehlenden QEMU-Funkgeräte.
  Hinzu kommen die Runtime-UID/GID-Zuordnung und installierte Ressourcenkennungen;
  Zusätzlich zwölf Tests zur ausdrücklichen Fortsetzung einer unterbrochenen
  Ersteinrichtung mit einer simulierten Plattform und acht Tests der
  [vorgeschalteten Speicher-Schnittstelle](../runtime/aosp-storage-lifecycle.md)
  sowie zwölf Tests der [Zugangsserialisierung](../runtime/admission.md);
  einschließlich der Brokerprotokoll-Tests insgesamt 52 kompilierte Android-Tests. Die Zuordnung startet keinen Linux-Kontext.
  Alle 52 Java-Gerätetests einschließlich der vier Produktkonfigurationstests
  bestehen im vollständig gestarteten Image `25fde995`.

Die erste CLI unterstützte im Quelltext `setup`, `user list`, `user add`,
`user remove`, `login`, `switch`, `passwd`, `status` und den bestätigten
Android-Logout im ausdrücklich runtimefreien Build. Benutzeranlage und Löschung
fordern für jede Aktion erneut das Passwort des angemeldeten AOSP-Administrators.
In diesem historischen Stand fehlten die `linux`-Operationen. Inzwischen
sind `linux start|status|shell|stop` integriert und tatsächlich geprüft.
Paketoperationen fehlen weiterhin; die Entfernung verwalteter Benutzer ist
ausdrücklich gesperrt, bis AOSPs vollständige Löschung vor ID-Freigabe
integriert ist. Unterbrochene Ersteinrichtung und vollständige Admin-Negativtests
sind noch nicht als reale CLI-Abläufe nachgewiesen.

## Sitzung gehört zum aufrufenden Terminal

Der Aufruf `aegis` öffnet einen interaktiven Client. `aegis login NAME` führt
die Anmeldung aus und bleibt danach ebenfalls in dieser interaktiven Sitzung.
Innerhalb desselben Clients stehen die folgenden Befehle zur Verfügung:

```text
aegis> user list
aegis> login "Anzeigename"
Passwort: [verdeckte Eingabe]
aegis> status
aegis> user add "Zweiter Benutzer"
aegis> user remove "Zweiter Benutzer"
aegis> passwd
aegis> switch "Anderer Anzeigename"
aegis> logout
aegis> exit
```

Ein optionales `aegis` vor einem interaktiven Befehl wird akzeptiert. Namen
können in einfache oder doppelte Anführungszeichen gesetzt werden. Es gibt
keine Shell-Erweiterung oder Ausführung frei eingegebener Programme.

Der Dienst bindet jede Sitzung an Binder-Aufrufer-UID, PID, die Startzeit des
Prozesses aus `/proc/PID/stat` sowie dessen Binder-Lebenszeichen. Ein übertragener
Binder-Verweis allein oder eine wiederverwendete PID erteilt keine Berechtigung.
Wenn die Prozessprüfung nicht möglich ist, wird abgelehnt. Die dafür benötigten
SELinux-Zugriffe funktionieren für den geprüften Entwicklungs-root-Client bei
Enforcing; daraus folgt kein vollständiger Nachweis aller Aufrufer-/Missbrauchsfälle.

Erst eine erfolgreiche AOSP-Passwortprüfung bindet den Client an eine persönliche
`userId` samt Seriennummer. Ein globaler Vordergrundwechsel ändert keine andere
Clientbindung. Vor persönlichen Operationen werden Benutzer und tatsächliche
CE-Entsperrung erneut abgefragt. AOSP-Benutzerstopp widerruft die zugehörigen
Clientbindungen, ohne den Systemserver-Hauptthread auf eine Operation warten
zu lassen. Gleichzeitige Anmeldung und Widerruf werden durch eine Generation
pro Benutzer erkannt; dieser Mechanismus ersetzt keine AOSP-Identität.

`exit`, EOF oder Clientabsturz schließen die Clientberechtigung. Sie sind kein
Logout des AOSP-Benutzers: zulässiger Hintergrundbetrieb bleibt erhalten.
Ein neu gestartetes `aegis` erbt keine Anmeldung aus einem anderen Terminal.
Die erste Schnittstelle ist ausschließlich für den bereits autorisierten
Entwicklungszugang mit Android-UID root/shell in einem debuggable Build vorgesehen.
Diese Verbindung allein begründet weder persönliche Anmeldung noch Adminrechte.
Normale Apps und spätere Runtime-UIDs werden damit noch nicht angebunden.

## Ersteinrichtung und Benutzerverwaltung

`setup NAME` ist nur aus der autorisierten Entwicklungs-Rootkonsole, nach
abgeschlossenem Android-Start und ohne persönliche Bindung in diesem Client
zulässig. Es richtet den ersten persönlichen AOSP-Administrator mit Passwort
ein; danach ist eine getrennte Anmeldung erforderlich. Bereits vorhandene
persönliche Vollbenutzer blockieren die Einrichtung, auch wenn sie deaktiviert,
unvollständig oder zur Entfernung vorgemerkt sind.

Der Reservierungszustand liegt als technische Metainformation in AOSPs
`Settings.Global` unter `aegis_initial_setup_state`. Er gewährt keine Identität
oder Adminrolle. Der direkte Provider-Aufruf unterscheidet einen fehlenden
Eintrag von einer fehlgeschlagenen Abfrage; Schreibvorgänge werden zurückgelesen.
`reserved`, `created:ID:SERIAL` und `complete:ID:SERIAL` erlauben keine automatische
Wiederholung einer begonnenen Ersteinrichtung. Der neue Quelltextpfad
`setup --resume NAME` setzt sie ausdrücklich fort; auch er ist kompiliert, aber noch nicht als
echter Kontoablauf in QEMU getestet. Ein Stromausfalltest der AOSP-Persistenz steht aus.

Bei `reserved` ist eine Kontoanlage nur zulässig, wenn weiterhin überhaupt kein
persönlicher Vollbenutzer existiert. Bei `created:ID:SERIAL` muss der angegebene
Name zum exakt protokollierten AOSP-Benutzer samt Seriennummer gehören; andere
persönliche Vollbenutzer blockieren die Fortsetzung. Gelöschte oder ersetzte
Kennungen, Teilkonten, fremde Credential-Typen und beschädigte Protokolle werden
abgewiesen. `complete` wird nicht wieder geöffnet. Weder Benutzer noch Daten
werden zur Wiederherstellung gelöscht.

Hat das deaktivierte Konto noch kein Passwort und noch keine Adminrolle, erfolgt
die initiale Passwortsetzung über AOSP. Ein bereits vorhandenes Passwort muss
dagegen frisch über AOSP verifiziert werden; es wird nicht ersetzt oder
zurückgesetzt. Das gilt auch, wenn die Anlage schon bis zur Aktivierung gelangt
ist, aber noch keinen Abschluss protokolliert hat. Ein bereits aktivierter
Nicht-Admin darf über diesen Weg keine Adminrolle erhalten.

Der gepinnte [LockSettingsService](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/locksettings/LockSettingsService.java)
entsperrt bei erfolgreicher Passwortprüfung auch CE-Speicher; die Prüfung ist
keine nebenwirkungsfreie Passwortabfrage. Die Fortsetzung bestätigt deshalb
anschließend den tatsächlichen Benutzerstopp und die CE-Sperre, bevor sie fehlende
Admin-/Aktivierungsflags setzt und den Abschluss protokolliert. Danach muss sich
der Benutzer getrennt anmelden. Caller-Bindung, AOSP-Beschränkungen, Kontozuordnung
und Reservierung werden zwischen den Schritten erneut geprüft. Abbruch oder
Timeout melden keinen Erfolg; ein weiterhin deaktiviertes Konto wird nach
Möglichkeit über AOSP erneut gestoppt und gesperrt, ohne Daten oder Passwort zu ändern.

Die Lücke zwischen AOSP-Kontoanlage und dem Schreiben von `created:ID:SERIAL`
bleibt ausdrücklich sichtbar: Existiert bereits ein Konto, aber nur `reserved`,
wird es nicht anhand des Namens übernommen. Dieser unklare Zustand sowie
Teilkonten verlangen eine gesonderte AOSP-Diagnose. Der Befehl behauptet keine
vollständige Wiederherstellung beliebiger beschädigter Plattformzustände.

Die zwölf neuen Gerätetests verwenden eine simulierte Plattform und prüfen
unter anderem Abbrüche nach jedem Mutationsschritt, wiederverwendete IDs,
falsche Passwörter, widerrufene Clientbindung, geänderte Reservierung und eine
nicht bestätigte CE-Sperre. Sie führen selbst keine realen Kontooperationen aus.
Zusätzlich sind nach dem Server-Build echte QEMU-Tests mit unterbrochener
Ersteinrichtung und Neustarts erforderlich.

Nach Anmeldung in demselben interaktiven Client legt `user add NAME [--admin]`
einen weiteren persönlichen AOSP-Benutzer an. Die aktuelle Adminrolle, die
AOSP-Benutzerbeschränkungen und das frisch eingegebene Adminpasswort werden
geprüft. Die ursprüngliche Clientbindung wird an mehreren Mutationspunkten
erneut geprüft. Es gibt kein wiederverwendbares Admin-Ticket.

Neue Konten beginnen deaktiviert und ohne Adminflag. Erst nach AOSP-Passwortsetzung,
bestätigter CE-Sperre und erneuter Prüfung werden sie gegebenenfalls zum Admin
und schließlich aktiviert. AOSP erstellt neue CE-Schlüssel zunächst entsperrt;
der Adapter fordert deshalb ausdrücklich die Sperrung an und liest sie zurück.
`FLAG_DISABLED` allein verhindert nicht jeden privilegierten Hintergrundstart:
Der Adapter prüft zusätzlich, dass der neue Benutzer weiterhin nicht läuft.
Fehlgeschlagene Konten werden weder automatisch aktiviert noch zurückgesetzt.
Die Liste zeigt auch deaktivierte oder unvollständige Konten zur Diagnose.

`user remove NAME` verlangt einen anderen angemeldeten AOSP-Administrator und
dessen frische Passwortprüfung. Systembenutzer 0 und Selbstlöschung sind
ausgeschlossen. Der Zielbenutzer wird gestoppt und sein CE-Speicher gesperrt.
Die Annahme des Löschauftrags durch AOSP reicht nicht: Der Adapter wartet auf
die Abwesenheit des ursprünglichen Benutzers und prüft weiterhin CE-Sperre und
beendeten Benutzerzustand. Ein Timeout meldet keinen Erfolg; AOSP kann die
Operation später noch abschließen.

Der gepinnte StorageManager protokolliert bestimmte Löschfehler, ohne sie an
den Aufrufer weiterzugeben. Nach abgeschlossener UserManager-Bereinigung
fordert der Entwurf daher eine zusätzliche Bestätigung der idempotenten
Schlüssel-/Datenbereinigung direkt von AOSPs `vold`. Zwischen diesen Aufrufen
wird erneut geprüft, dass die Benutzerkennung nicht wieder belegt ist.
Dies ist bislang auf internen Speicher begrenzt: Erkannte oder gespeicherte
private Zusatzvolumes, auch abgezogene, führen vor der Löschung zur Ablehnung.
Dieser Pfad ist noch ungetestet; er behauptet weder forensisch sicheres Löschen
noch Schutz vor einem kompromittierten privilegierten AOSP-Systemdienst.

## Passwörter und Fehler

Passwörter werden ausschließlich mit AOSPs `Console.readPassword` eingelesen.
Ohne interaktives Terminal schlägt die Abfrage fehl; es gibt keinen Pipe-,
Argument-, Umgebungsvariablen- oder Datei-Ersatz. Eigene Passwortpuffer werden
im Erfolgs- und Fehlerpfad überschrieben, nicht in Strings umgewandelt.

Das interne Protokoll begrenzt Credentials auf 4 bis 128 druckbare ASCII-Zeichen.
Die AOSP-Regeln für neue Passwörter gelten zusätzlich; die bisher getestete
Android-Einstellung begrenzt neue Passwörter auf 16 Zeichen. Unicode-Passwörter
werden in dieser ersten CLI ausdrücklich abgelehnt, nicht abgeschnitten oder
still anders codiert.

Die Credential-Schnittstelle trägt `@SensitiveData`. Der gepinnte AIDL-Generator
markiert damit Java-Anfrage-Parcels als sensibel und setzt `FLAG_CLEAR_BUF` für
die Binder-Transaktion. Eigene decodierte Bytepuffer und `LockscreenCredential`
werden zusätzlich gelöscht. Ein lokaler Aufruf innerhalb von `system_server`
würde sonst dasselbe Java-Objekt an LockSettings weitergeben. Da AOSP Teile eines
Passwortwechsels verzögert verarbeitet, erzwingt `CredentialTransport` auch dort
die AIDL-Parcel-Kopie: AOSP verwaltet die empfangene Kopie, während der Adapter
seine eigene löscht. Drei vorbereitete Android-Tests prüfen getrennte
Pufferlebensdauer in beide Richtungen und die Weitergabe von Transportfehlern.
Sie verwenden öffentliche Testdaten, kontaktieren keine echten LockSettings
und bestehen im Komponentenlauf. Das behauptet keine vollständige Eliminierung
aller temporären Kopien in einer verwalteten Laufzeit.

AOSPs falsches Passwort und dessen Wiederholungsverzögerung werden getrennt
behandelt. Der Client führt keine automatische Wiederholung aus. Andere
Backend-Fehler werden ohne Stacktrace, originale Fehlertexte oder Credentials
ausgegeben. Der Dienst bestätigt Logout erst nach tatsächlichem Benutzerende
und CE-Sperre. Fehlgeschlagene Aktionen liefern einen Fehlerstatus.

## Grenze zur noch fehlenden Runtime

Der Dienstentwurf startet nur bei `ro.aegis.runtime.mode=absent`. Fehlende oder
andere Werte führen zur Ablehnung. Dies ist die explizite Produktkonfiguration
für den Zwischenstand ohne Runtime, keine Erkennung oder Bereinigung beliebiger
Prozesse. Sobald eine Runtime eingebaut wird, muss ein echter Koordinator diesen
Pfad ersetzen: Starts blockieren, Paketoperationen koordinieren, Prozesse reapen,
Mounts/IPC abbauen und dann den AOSP-Benutzer samt CE-Speicher stoppen.

Die aktuelle Logout-Antwort nennt ausdrücklich, dass keine Runtime installiert
ist. Sie erfüllt noch nicht den vollständigen Phase-1-Logout mit Linux-Kontext.
Auch für die privaten Paketoperationen bleibt frische AOSP-Adminautorisierung
verpflichtend; ein Terminal- oder Runtime-UID wird nicht zur Adminberechtigung.

## Gezielte Kompilierprüfung auf dem Builder

`scripts/aosp/check-identity.sh FULL_PROJECT_COMMIT` läuft ausschließlich auf
Linux x86-64 als vorhandener Benutzer `aegis-build`. Der Projektcheckout muss
sauber sein und exakt dem zuvor über GitHub bezogenen Commit entsprechen.
Der Check verwendet die gemeinsame Buildsperre, prüft den Manifest-Pin,
registriert die verwalteten Produkt- und Paketquellen und baut `aegis`,
`aegis-identity-service`, `AegisIdentityTests`, `framework-res` und
`selinux_policy`, die generierten Vendor-/System-Ext-Kennungsregister sowie
`aegis-runtime-init` und `AegisRuntimeNativeTests` mit maximal zwölf Jobs.
Der native Aufseher bleibt ein gesondertes Buildziel ohne Produktaktivierung;
siehe [Prozessaufseher](../runtime/process-supervisor.md).
Zuvor prüft AOSPs hashgeprüfter Originalparser die vollständige
Produktliste der Kennungsdefinitionen auf Konflikte; siehe
[`runtime/uid-mapping.md`](../runtime/uid-mapping.md). Damit werden auch Ressourcen und
SELinux-Regeln des neuen Produktstands kompiliert; es wird weiterhin kein
fertiges Boot-Image erzeugt.

Die Registrierung überschreibt keine fremden oder lokal veränderten Quellen.
Vorherige verwaltete Fassungen bleiben außerhalb der AOSP-Quellsuche unter
`out/aegis-identity-backups` erhalten. Unvollständige Kopien liegen ebenfalls
außerhalb der Quellsuche unter `out/aegis-identity-staging`. Sieben lokale Tests
prüfen echte Dateiveränderungen, zusätzliche Dateien, symbolische Links,
Kopierabbrüche und die Aufbewahrung der vorigen Fassung. Sie prüfen weder Java
noch AOSP-Berechtigungen.

Inhaltlich unveränderte Registrierungen behalten jetzt ihre Dateien und
Zeitstempel. Bei einer Teiländerung behalten unveränderte Dateien ihren
Zeitstempel; dadurch sollen unveränderte Make-/Soong-Eingaben keine erneute
Buildplanerzeugung auslösen. Neue Inhalte erhalten weiterhin neue Zeitstempel
und die vorige verwaltete Fassung bleibt als Backup erhalten. Die Prüfung auf
fremde Änderungen erfolgt auch unmittelbar vor dem Austausch der Identitätsquellen.
25 gezielte Hosttests zu Produktregistrierung, Identitätsregistrierung und
QEMU-Befehlsbildung bestehen. Die Linux-CI besteht insgesamt mit 195 Tests ohne
Auslassungen. Im Lauf `d16e74f` erreichte der Dienst nach 28 Sekunden den
Ninja-Compilerlauf (14:24:28 bis 14:24:56 UTC); zuvor dauerte die Vorbereitung
mehrere Minuten. Das ist keine Messung der gesamten Builddauer.

Erst erfolgreicher Komponentenbau und vorhandene JAR-/Startdateien samt
Framework-Ressourcen, Test-APK und nativen ARM64-Artefakten setzen
`IDENTITY_COMPILED_NOT_INSTALLED`. Logs, Quellinventar und Modulprüfsummen liegen
im zugehörigen Verzeichnis unter `/srv/aegis/runs`. Erfolgreiche Läufe und
Gastprüfungen stehen im [Komponentenbericht](component-tests.md).
Er startet keine VM, installiert keinen Dienst und veröffentlicht
keine Artefakte. Die geprüften Ergebnisse müssen anschließend über GitHub
transportiert werden; ein Modulbau allein liefert noch kein startbares System.

Für den bereits eingerichteten Builder gibt es jetzt
`scripts/start-components.sh FULL_COMMIT` (dort als Root starten). Es erzeugt
einen neuen Checkout und startet `aegis-components.service` als vorhandenes
unprivilegiertes Buildkonto. Der Dienst bezieht exakt den vollständigen Commit
vom öffentlichen GitHub-Repository und ruft den Komponenten-Check auf. Die
vorhandene AOSP-Arbeitskopie wird unter ihrer exklusiven Buildsperre verwendet.
Es werden keine Pakete installiert, Servereinstellungen geändert oder Tests
auf dem Builder ausgeführt. Zwölf Stunden begrenzen den Dienst, Quellen und
Ergebnisse bleiben erhalten. Das Journal zeigt auch Fehler vor Anlage eines
Identitäts-Laufverzeichnisses. Die Meldung des Startskripts bestätigt nur die
Übergabe an systemd; erst Status, Buildlog und Artefakte belegen Kompiliererfolg.

Die Ausgabe des Komponentenbaus lässt sich in einem weiteren Terminal auf dem
Mac live verfolgen:

```sh
ssh aegis-build 'journalctl -fu aegis-components'
```

`Strg+C` beendet nur diese Anzeige. Der Serverdienst läuft unabhängig davon
weiter. Prozentwerte gehören zur gerade angezeigten Teilphase und messen
nicht den Gesamtfortschritt. Der vollständige Imagebau verwendet stattdessen
den Dienst `aegis-build`.

Der vollständige Build übernimmt die Identitätsquellen und Produktkonfiguration
über den [gepinnten GitHub-Quelltransport](build-inputs.md). Die Einbindung in
das Produkt und den Systemstart ist im Vollbuild `25fde995` gebaut und im
vollständig gestarteten Gast bestätigt.
Das Test-APK wird gemäß dem gepinnten Soong-Installationsschema aus
`testcases/AegisIdentityTests/arm64/AegisIdentityTests.apk` aufgenommen. Nach dem
Transport ist es ausschließlich in der lokalen QEMU-VM zu installieren und mit
`org.aegisos.identity.tests/androidx.test.runner.AndroidJUnitRunner` auszuführen.
Die nativen Tests und deren Aufseher-Binary werden zusammen aus
`data/nativetest64/AegisRuntimeNativeTests/` gesammelt. Sie dürfen ebenfalls
ausschließlich im lokalen Android-QEMU ausgeführt werden; weder Server-Build
noch Host-CI führen den neuen Runtime-Code aus.
Der [Komponentenexport](component-transport.md) über GitHub mit geprüftem
Download und Entpacken ist vorbereitet. Er verlangt einen tatsächlich
erfolgreichen Komponentenlauf und erhält dessen ursprünglichen Status.

Noch offen sind:

1. Fremde Binder-Aufrufer, zwei parallele Clients, Clienttod, externer
   Benutzerstopp und Rennen zwischen diesen Operationen; EOF-/Abbruchpfade
   samt Terminalzustand.
2. Unterbrochene Ersteinrichtung einschließlich `setup --resume`,
   Benutzerlöschung, verweigerte Adminaktionen, Löschfehler und ID-Wiederverwendung.
3. Integration des vollständigen Runtime-/Paketlebenszyklus.

Der [reale CLI-Gasttest](identity-cli-qemu-test.md) deckt bereits normale
Ersteinrichtung/Anlage, korrekte und falsche Passwörter, Wechsel, Passwortänderung,
Logout und einen geordneten Neustart mit zwei Benutzern ab.

## Vorbereitete Produktintegration

`aegis_qemu_arm64.mk` installiert CLI und Dienst-JAR unter `system_ext` und ergänzt
`PRODUCT_SYSTEM_SERVER_JARS_EXTRA` um `system_ext:aegis-identity-service`,
damit das JAR nach den gemeinsamen
Framework-Diensten einsortiert wird. Die AEGIS-Module nutzen private
Framework-Schnittstellen und deklarieren deshalb `system_ext_specific: true`;
siehe [AOSP: System-Erweiterungen](https://source.android.com/docs/core/architecture/partitions/shared-system-image#system_ext-partition).
Die geerbte `generic_system.mk` behält ihre geprüfte Artefaktgrenze, ohne
Ausnahmen für die AEGIS-Dateien. Die Änderungen an `services` und
`framework-res` verbleiben als Änderungen vorhandener Plattformmodule in
`system`.
Der Präfix `system_ext:` ist zusätzlich zur Moduleigenschaft erforderlich:
[AOSPs gepinnte Klassenpfadliste](https://android.googlesource.com/platform/build/soong/+/refs/tags/android-16.0.0_r1/android/configured_jars.go)
leitet daraus den Gerätepfad und die erwarteten Systemserver-Dex-Artefakte ab.
Ohne Präfix erwartet die Prüfung weiterhin Dateien unter `system/framework`.

Der CLI-Starter liegt unter `/system_ext/bin/aegis`, seine JAR-Datei und das
Dienst-JAR unter `/system_ext/framework/`. Ein eigener Wrapper setzt den
CLI-Klassenpfad und startet den absoluten `/system/bin/app_process`-Pfad mit
unveränderter Argumentübergabe. Das ist nötig, weil der
[gepinnten Soong-Implementierung](https://android.googlesource.com/platform/build/soong/+/refs/tags/android-16.0.0_r1/java/java.go)
zufolge der Standardwrapper `/system/framework` fest einträgt, auch wenn das
Modul auf `system_ext` installiert wird. Komponentencheck und Imagebau gleichen
den tatsächlich installierten Wrapper mit der Quelle ab; der Gastcheck nutzt
den neuen absoluten Pfad. Diese Korrektur ist noch im Serverbuild und Gast zu
bestätigen.

Ein gezieltes Build-Overlay trägt
`org.aegisos.identity.AegisIdentityService` in die Framework-Ressource
`config_deviceSpecificSystemServices` ein. Nur dieses eigene Overlay ist von
der automatischen RRO-Umwandlung ausgenommen, damit die private Bootkonfiguration
in `framework-res` liegt. Die geerbten Cuttlefish-RROs bleiben aktiv.

Der geprüfte AOSP-Standard enthält eine leere Dienstliste. Die gepinnten
Cuttlefish-Core- und Phone-RROs definieren diese Ressource nicht; das klassische
Phone-Framework-Overlay betrifft `frameworks/base/libs`. Bei einem Wechsel der
Plattformbasis ist erneut zu prüfen, ob weitere Dienste in die Liste gehören.
Die beiden neuen Android-Produkttests lesen die wirksamen installierten
Ressourcen und verlangen genau einen AEGIS-Eintrag sowie mindestens vier
Benutzerplätze und den vollständigen Systembenutzer für den Logout-Rückwechsel.
Sie laufen zusätzlich zu den drei Credential-Transporttests.

Die zusätzliche SELinux-Policy liegt unter `sepolicy/private` im eigenen
Gerätebaum und wird als `SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS` eingebunden:

- `aegis_identity` erhält einen eigenen Typ mit `system_server_service` und
  `service_manager_type`, ohne `app_api_service` oder `system_api_service`.
- AOSP erlaubt `system_server` bereits die Registrierung dieser Service-Typen
  und das Lesen der benötigten `/proc/PID/stat`-Dateien. Dafür werden keine
  zusätzlichen pauschalen Freigaben eingeführt.
- `ro.aegis.runtime.mode` erhält einen eigenen internen Property-Typ und den
  erlaubten Wert `absent`. Systemserver und Entwicklungs-Shell können ihn lesen;
  eine Schreibfreigabe für die Shell wird nicht hinzugefügt.

Die vorhandenen privilegierten AOSP-Debugdomänen bleiben Teil der
Entwicklungs-Vertrauensgrenze. Der neue Service-Typ allein ersetzt nicht die
UID-/PID-/Sitzungsprüfung im Binder-Dienst. SELinux-Kompilierung, Klassenladen,
Dienstregistrierung und tatsächliche Zugriffskontrollen sind noch nachzuweisen.

Nach dem vollständigen Server-Build, Transport über GitHub und Start des neuen
Images im lokalen QEMU kann der ausschließlich lesende Basistest laufen:

```sh
python3 scripts/check-local-identity.py 127.0.0.1:15555
```

Er verlangt den QEMU-Produkttyp, Bootabschluss, authentifiziertes ADB,
SELinux `Enforcing`, den expliziten Runtime-Modus und eine registrierte
Binder-Schnittstelle. Danach muss ein neuer `aegis status`-Prozess einen
unangemeldeten Zustand liefern, und der Dienst muss weiterhin vorhanden sein.
`IDENTITY_SERVICE_RESPONDS` bestätigt nur diese Einbindung, keine Passwort-,
Benutzerlebenszyklus-, Runtime- oder Sicherheitstests.

Am 28. September wurde dieser Check gegen den noch laufenden alten QEMU-Stand
`mouse-1` ausgeführt. Die ersten Prüfungen bestanden; der fehlende
`ro.aegis.runtime.mode` führte erwartungsgemäß zum Abbruch mit Exitcode 1.
Es wurde keine Gastkonfiguration geändert. Ein positiver Lauf auf dem neuen
Image steht aus.

## Abgeglichene Quellen

Alle Quellen gehören zu `android-16.0.0_r1`:

- [Console.readPassword](https://android.googlesource.com/platform/libcore/+/refs/tags/android-16.0.0_r1/ojluni/src/main/java/java/io/Console.java)
- [AIDL-Java-Generator](https://android.googlesource.com/platform/system/tools/aidl/+/refs/tags/android-16.0.0_r1/generate_java_binder.cpp)
- [SystemService und Benutzer-Lifecycle](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/SystemService.java)
- [SystemServer: gerätespezifische Dienste](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/java/com/android/server/SystemServer.java)
- [Soong: java_binary-Wrapper](https://android.googlesource.com/platform/build/soong/+/refs/tags/android-16.0.0_r1/java/java.go)
- [Soong: android_test-Installation](https://android.googlesource.com/platform/build/soong/+/refs/tags/android-16.0.0_r1/java/app.go)
- [UserManagerService: Benutzeranlage und Bereinigung](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/pm/UserManagerService.java)
- [StorageManagerService: Schlüssel- und Datenoperationen](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/StorageManagerService.java)
- [vold: FBE-Bereinigung](https://android.googlesource.com/platform/system/vold/+/refs/tags/android-16.0.0_r1/FsCrypt.cpp)
