# Erste AEGIS-Terminalintegration

Stand 28. September 2026: **Quelltext vorbereitet, nicht kompiliert und nicht
im Gast installiert.** Die bestehenden Android-Dialogtests aus
[`identity-platform-test.md`](identity-platform-test.md) testen diesen Code nicht.

## Vorbereitete Komponenten

- `aegis-identity-core`: Adapter zu AOSP für persönliche Benutzer, Passwörter,
  Entsperrung und bestätigten Android-Benutzerstopp.
- `aegis-identity-protocol`: interne Binder-Schnittstellen.
- `aegis-identity-service`: vorgesehener Systemserver-Dienst. Veröffentlicht
  `aegis_identity` erst bei ausdrücklich passender Entwicklungskonfiguration.
- `aegis`: Terminalprogramm mit verdeckter Passworteingabe und interaktiver Sitzung.

Die erste CLI unterstützt im Quelltext `user list`, `login`, `switch`, `passwd`,
`status` und den bestätigten Android-Logout im ausdrücklich runtimefreien Build.
`user add`, `user remove`, Adminfreigabe und alle `linux`-Operationen sind noch
nicht implementiert. Sie werden abgelehnt und nicht als erfolgreich ausgegeben.

## Sitzung gehört zum aufrufenden Terminal

Der Aufruf `aegis` öffnet einen interaktiven Client. `aegis login NAME` führt
die Anmeldung aus und bleibt danach ebenfalls in dieser interaktiven Sitzung.
Innerhalb desselben Clients stehen die folgenden Befehle zur Verfügung:

```text
aegis> user list
aegis> login "Anzeigename"
Passwort: [verdeckte Eingabe]
aegis> status
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
SELinux-Zugriffe müssen noch im tatsächlichen Gast überprüft werden.

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
die Binder-Transaktion. Decodierte Bytepuffer und `LockscreenCredential` werden
zusätzlich gelöscht. Das behauptet keine vollständige Eliminierung aller
temporären Kopien in einer verwalteten Laufzeit.

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
registriert die verwalteten Produkt- und Paketquellen und baut nur `aegis` sowie
`aegis-identity-service` mit maximal zwölf Jobs.

Die Registrierung überschreibt keine fremden oder lokal veränderten Quellen.
Vorherige verwaltete Fassungen bleiben außerhalb der AOSP-Quellsuche unter
`out/aegis-identity-backups` erhalten. Unvollständige Kopien liegen ebenfalls
außerhalb der Quellsuche unter `out/aegis-identity-staging`. Sieben lokale Tests
prüfen echte Dateiveränderungen, zusätzliche Dateien, symbolische Links,
Kopierabbrüche und die Aufbewahrung der vorigen Fassung. Sie prüfen weder Java
noch AOSP-Berechtigungen.

Erst erfolgreicher Modulbau und vorhandene JAR-/Startdateien setzen
`IDENTITY_COMPILED_NOT_INSTALLED`. Logs, Quellinventar und Modulprüfsummen liegen
im zugehörigen Verzeichnis unter `/srv/aegis/runs`. Dieser Check ist noch nicht
ausgeführt. Er startet keine VM, installiert keinen Dienst und veröffentlicht
keine Artefakte. Die geprüften Ergebnisse müssen anschließend über GitHub
transportiert werden; ein Modulbau allein liefert noch kein startbares System.

Vor der tatsächlichen Nutzung fehlen:

1. Erfolgreiche Server-Kompilierung samt Prüfung der erzeugten Schnittstellen.
2. Produktintegration in den Systemserver-Classpath, Dienststart über
   `config_deviceSpecificSystemServices`, ausdrücklicher Runtime-Modus sowie
   eng begrenzte SELinux-Service-Labels und Zugriffsregeln.
3. Neues Image auf dem Server bauen, über GitHub beziehen und in QEMU starten.
4. CLI-Gasttests für korrekte/falsche Passwörter, Wechsel, Änderung, Logout,
   fremde Binder-Aufrufer, zwei parallele Clients, Clienttod, Benutzerstopp und
   Rennen zwischen diesen Operationen. Verdeckt-Eingabe, EOF und Abbruch samt
   Terminalzustand sind ebenfalls praktisch zu prüfen.
5. Benutzeranlage/-löschung, abgesicherte Ersteinrichtung, Adminaktionen und
   Integration des vollständigen Runtime-/Paketlebenszyklus.

## Abgeglichene Quellen

Alle Quellen gehören zu `android-16.0.0_r1`:

- [Console.readPassword](https://android.googlesource.com/platform/libcore/+/refs/tags/android-16.0.0_r1/ojluni/src/main/java/java/io/Console.java)
- [AIDL-Java-Generator](https://android.googlesource.com/platform/system/tools/aidl/+/refs/tags/android-16.0.0_r1/generate_java_binder.cpp)
- [SystemService und Benutzer-Lifecycle](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/SystemService.java)
- [SystemServer: gerätespezifische Dienste](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/java/com/android/server/SystemServer.java)
- [Soong: java_binary-Wrapper](https://android.googlesource.com/platform/build/soong/+/refs/tags/android-16.0.0_r1/java/java.go)
