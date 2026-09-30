# Kontrollierter Paketabruf

Der unveränderliche Paketplaner kann einen vom Broker zugelassenen Internetmodus
verwenden. Persönliche Shells und die späteren Installationsprogramme behalten
jeweils ihren eigenen Netzwerkraum ohne diese Verbindung. Die öffentliche
CLI/Binder-Anbindung, frische AOSP-Adminfreigabe und Produkt-SELinux-Einbindung
sind noch nicht aktiviert.

## Datenweg und Besitz

`PackagePlannerStart` erstellt zunächst den normalen, angehaltenen Planer mit
privatem Benutzer-, Prozess-, Mount- und Netzwerkraum. Sein eigener, über den
privaten Startkanal übertragener Netzwerk-Deskriptor wird mit dem zugehörigen
Benutzerraum verglichen. Es wird kein fremder numerischer Prozesspfad geöffnet.

Ein gesondert gepinnter Helfer erzeugt in diesem Netzwerkraum einen
Listener auf `127.0.0.1:1080`, kehrt in Androids Netzwerkraum zurück und gibt alle
Capabilities ab. Seine UID/GID ist die reservierte Systemdienst-Kennung `7502`.
Sie liegt außerhalb aller persönlichen UID/GID-Abbildungen. Als zusätzliche
Gruppe bleibt nur Androids `inet` (3003). Signal-Systemaufrufe, ptrace und fremde
Prozessspeicherzugriffe sind gesperrt. Die gemeinsame Dienstkennung berechtigt
den festen Helfer damit nicht zum Signalisieren anderer Aufträge über diese
Systemaufrufe.
Der Helfer nutzt dynamisches Bionic für Androids Netd-DNS-Schnittstelle; dessen
statische Standardimplementierung liefert keinen DNS-Proxy. Der Helfer
übernimmt keine CE-, Quellen-, Archiv- oder Runtime-Deskriptoren; nach der
Vorbereitung hält er nur den Listener. Neue ausgehende Sockets entstehen im
Android-Netzwerkraum. Globale Routen, Weiterleitung und Firewall bleiben unberührt.

Die feste, schreibgeschützte APT-Konfiguration enthält den lokalen SOCKS5h-Proxy.
Der Online-Modus erhält zusätzlich ein root-eigenes, begrenztes Zertifikatsbündel
als festen Produkteingang; es wird schreibgeschützt in die Planerrichtlinie
kopiert und in deren Digest aufgenommen. Das private Protokoll übergibt genau
sechs Eingabedeskriptoren einschließlich dieses Bündels und weist einen siebten
bereits beim Senden ab. HTTPS-Zertifikatsprüfung bleibt aktiv.

Der Broker-Startcode stellt diese Produkteingaben mit `aegis_package_policy_open`
selbst zusammen: Debian 13 `trixie`, `trixie-updates` und `trixie-security`, jeweils
`main` und ausschließlich HTTPS. Die drei festen Debian-Archivschlüssel stammen
aus der bereits geprüften schreibgeschützten Basis. Das TLS-Bündel stammt aus dem
signierten, schreibgeschützten Conscrypt-APEX; dessen Zertifikate gehören Androids
Systemkonto (1000:1000). Geprüft werden Mounttyp, Schreibschutz, Eigentümer,
SELinux-Typ, Dateityp, Größen und sichere Auflösung relativ zum gehaltenen
Verzeichnis. Persönliche Zertifikatsablagen und Paketkonfigurationen werden nicht
als Vertrauensquelle verwendet.

Quellenliste, Schlüssel und Zertifikate werden in drei schreibgeschützte memfds
mit vollständigen Schreib-/Größen-/Seal-Sperren überführt. Der Broker übernimmt
sie gemeinsam vor dem ersten Auftrag und erlaubt keine spätere Ersetzung.
Ein Fehler liefert keine teilweise eingerichtete Konfiguration. Die interne
Eingangsprüfung akzeptiert weiterhin normale geprüfte Dateien für isolierte
Fixtures; sie allein beweist weder Herkunft noch Berechtigung. Der Produktpfad
verwendet ausschließlich die festen, versiegelten Eingaben.

Diese Startanbindung ist implementiert, aber noch nicht im laufenden Produktimage
installiert. Ein Komponentenlauf prüft die Funktionen im lokalen Gast; erst ein
neues Vollimage und dessen tatsächlicher Brokerstart können die Start- und
SELinux-Anbindung zur Laufzeit belegen. Die Verbindung zum öffentlichen
Paketauftrag bleibt ebenfalls offen.
Dieser Proxy erlaubt ausschließlich CONNECT zu `deb.debian.org` und
`security.debian.org`, jeweils Port 80 oder 443. Numerische Ziele, weitere
SOCKS-Befehle, andere Namen/Ports und spezielle oder lokale IPv4-Adressen nach
DNS-Auflösung werden abgewiesen. IPv6 wird zunächst nicht verwendet. Die
SOCKS5h-Unterstützung ist in der [Debian-APT-Dokumentation](https://manpages.debian.org/trixie/apt/apt-transport-http.1.en.html)
beschrieben. APTs Signatur-, Zeit- und Hash-Prüfungen und die unabhängige Bindung
von Release, vollständigem Paketindex und Archiv bleiben erforderlich.

Pakethelfer verwenden ein eigenes cgroup-Blatt `p<user>-s<serial>` neben dem
Runtime-Blatt `u<user>-s<serial>`. Dadurch kann der Planer mit einer laufenden
Runtime derselben Identität koexistieren; ein Paketabbruch signalisiert deren
Prozesse nicht. Beide Zwecke bleiben innerhalb desselben begrenzten Aggregats.
Der Broker bindet eine Paketauswahl vor Planung an den gemeinsamen oder
persönlichen Scope; eine Runtime-START-Auswahl ist dafür nicht verwendbar.

Der Broker besitzt beide Kinder über beim Start erzeugte pidfds. Ein begrenzter
zweiter cgroup-Start ist nur nach der ersten Belegung, für denselben Benutzer und
dieselbe Seriennummer, genau einmal und vor jedem STOP erlaubt. Beide Kinder und
alle Verbindungen teilen das bestehende Speicherlimit. Höchstens acht
Verbindungsarbeiter mit begrenzten Puffern, Handshake-/Verbindungsfristen,
30 Sekunden Wartezeit, 15 Minuten Lebensdauer und 1 GiB Übertragungsbudget pro
Verbindung werden zugelassen. Nach Abbruch beziehungsweise Planerabschluss muss
der Broker beide direkten Kinder einsammeln und eine leere cgroup bestätigen,
bevor er Ergebnisdateien freigibt.

## Konfigurierter Paketauftrag

`BrokerBeginConfiguredPackage` registriert den ursprünglichen `PackageIntent`
(Aktion, Paket, optionale Version und gemeinsamer/persönlicher Bereich) vor der
Auswahl beziehungsweise dem Öffnen privater Daten. Die Identität kommt getrennt
aus der frisch geprüften AOSP-Sitzung. Die gespeicherte Kopie kann durch spätere
Änderungen am Aufruferobjekt nicht verändert werden. Ein zweiter noch offener
Auftrag derselben Identität bleibt ausgeschlossen.

Planer und Netzwerkhelfer werden gemeinsam beim Start gepinnt. Die interne
Übernahme prüft die readonly ausführbaren Dateien; der Produkt-Bootstrap muss
vorher zusätzlich feste Pfade, EROFS/Schreibschutz und genaue SELinux-Typen prüfen.
Wie bei der Richtlinie ist diese interne Setter-Funktion keine öffentliche
Protokolloperation. Ohne vollständige Konfiguration beginnt kein Auftrag.

`BrokerContinueConfiguredPackagePlanning` nimmt nur Identität, Seriennummer,
Auftrags-ID und neue Frist entgegen. Es übergibt ausschließlich die behaltene
Auswahl, den gespeicherten Wunsch, die festen Quellen/Schlüssel/CA und die
gepinnte Helferkonfiguration an den Planer. Die Auftrags-ID bleibt gleich. Der
allgemeine interne FD-basierte Übergang darf eine solche konfigurierte Auswahl
nicht übernehmen. Umgekehrt kann die konfigurierte Fortsetzung einen allgemeinen
internen Planauftrag nicht als eigenen verwenden.

Ob ein Ziel-Store neu angelegt werden muss, folgt aus der geprüften Anwesenheit
seines Verzeichnisses vor der Auswahl. Eine fehlende aktuelle Generation bedeutet
nicht, dass auch der Store fehlt: Ein korrekt initialisierter leerer Store wird
beibehalten, ein unvollständiger oder beschädigter Store abgewiesen. Diese
Unterscheidung fließt in den späteren Plan-Digest ein. Ein geprüftes, vollständig
leeres Verzeichnis ohne Store-Metadaten gilt wie ein noch nicht angelegter Store;
das erlaubt einen neuen Versuch nach abgebrochener Vorbereitung. Sobald irgendein
Eintrag vorhanden ist, muss die strenge Store-Prüfung bestehen. Teilweise
Initialisierung wird nicht repariert oder als Abwesenheit behandelt. Fehlende persönliche CE
liefert einen behaltenen fehlgeschlagenen Auftrag; auch `ENOENT` allein beweist
keine Freigabe der Ressourcen. Abbruch/STOP/HELLO bleiben bis zur bestätigten
Beendigung verantwortlich. Eine Fortsetzung eines entfernten Auftrags erzeugt
keinen Ersatzauftrag.

Diese Anbindung ist bislang intern implementiert und in nativen Fixtures
geprüft. Der tatsächliche Broker-Start aktiviert die Planer-Helferkonfiguration
noch nicht. Die feste Bereitstellung der Helfer mit Produkt-SELinux, die
öffentlichen Wire-/Binder-/CLI-Operationen und frische AOSP-Adminbestätigung
müssen folgen.

`BrokerPrepareConfiguredTransaction` übernimmt inzwischen ausschließlich den
behaltenen geprüften Auftrag (Identität, Seriennummer, Auftrags-ID und neue Frist).
Der Aufrufer liefert weder Digest noch Datei-, Store-, Arbeitsverzeichnis-,
Archiv- oder Helferdeskriptoren. Ausführung und Veröffentlichung werden beim
Broker-Start gemeinsam gepinnt; spätere Ersetzung ist ausgeschlossen. Die
bestehende vorbereitende Hilfe stammt aus der gepinnten Auswahlkonfiguration.

Der Übergang registriert dieselbe Auftrags-ID als Installation, bevor er feste
Speicherpfade öffnet oder anlegt. Gemeinsam genutzte Dateien liegen ausschließlich
unter `shared-packages` und `shared-staging` im gepinnten Broker-Verzeichnis;
Arbeitsverzeichnisse haben eine neue Zufallskennung. Besitzer, Modus, SELinux-Typ,
Dateisystem, ACLs und Verzeichnisbindung werden geprüft. Persönliche Daten gehen
weiter über die CE-/Seriennummer-Prüfung des ursprünglichen Antragstellers. Der
Broker öffnet genau das zuvor ausgewählte Ausgangsabbild; die asynchrone
Vorbereitung prüft dessen vollständigen Hash erneut. Fehler nach Registrierung
bleiben unter derselben ID abholbar. Die allgemeine interne FD-Schnittstelle kann
einen konfigurierten Auftrag nicht übernehmen.

Die Vorbereitung führt noch keine Paketprogramme aus und wählt keine neue
Generation aus. Erst eine weitere frisch zugelassene AOSP-Aktionsfreigabe darf
die vorbereitete Ausführung starten. Produkt-SELinux muss insbesondere die
festen gemeinsamen Verzeichnisse korrekt anlegen; native SU-Fixtures beweisen
diesen Übergang nicht. Noch ausstehend sind auch automatische Begrenzung und
Bereinigung alter Arbeitsverzeichnisse: Abbruch schließt Ressourcen, vorhandene
Arbeitsdateien werden bislang behalten. Dieser Pfad wird deshalb noch nicht als
öffentlicher Befehl freigeschaltet.

## Nachweisgrenze

Die Implementierung wird als Komponentensatz im lokalen QEMU geprüft.
Der echte Debian-Index benötigt Zeilen größer als 64 KiB (beispielsweise das
`Provides`-Feld von `librust-winapi-dev` mit 75.649 Bytes). Der vollständige Index
wird weiterhin gegen den authentifizierten Hash geprüft; einzelne Zeilen sind
nun auf 128 KiB begrenzt, Absätze weiterhin auf 1 MiB und 256 Felder.
Ein Grenztest bestätigt die lange Zeile und weist einen veränderten Indexhash ab.
Ein erfolgreicher Abruf belegt noch keine öffentliche Paketinstallation oder
Adminfreigabe. Die vorhandenen vier deaktivierten AOSP-CE-Integrationstests sind
kein Bestandteil dieses Nachweises. Build- und Gerätetestergebnisse stehen in [component-tests.md](component-tests.md).
