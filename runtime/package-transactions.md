# Vollständige Paketgenerationen

## Implementierungsschritt: Auswahl und unveränderliche Abbilder

`package_store.{h,cpp}` implementiert einen internen Speicherbaustein. Er ist
nur in die nativen Gerätetests eingebunden, noch nicht in den produktiven
Broker. **Es gibt damit noch keine funktionierende Paketinstallation.**
Die vorhandene CLI und ihre Berechtigungen werden durch diesen Baustein
nicht verändert. AOSP-Kompilierung und die zehn neuen Tests stehen noch aus.

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

Die zehn vorbereiteten Gerätetests verwenden kleine inerte Textdateien unter
`/data/local/tmp`, keine echten Paketimages oder persönlichen CE-Stores. Sie
prüfen Auswahl, Reopen, alte offene Referenzen, private Basisbindung, falsche
Eigentümer, Abbruch, beschädigte Quellen/Metadaten, konkurrierende Schreiber,
Symlinks/Hardlinks, verwaiste nicht ausgewählte Dateien und Prozessbindung.
Ein Test beendet einen bestätigten Publisher mit `_exit`, ohne Destruktoren;
er behauptet keinen Fehler während des Kopierens und keinen Stromausfalltest.
