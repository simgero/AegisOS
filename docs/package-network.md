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
bereits beim Senden ab. HTTPS-Zertifikatsprüfung bleibt aktiv. Der Gerätetest
erzeugt sein Bündel aus dem verifizierten, schreibgeschützten Conscrypt-APEX;
dessen Zertifikate gehören Androids Systemkonto (1000:1000). Die tatsächliche
Produktbereitstellung des festen Bündels bleibt Teil der Brokeranbindung.
Dieser Proxy erlaubt ausschließlich CONNECT zu `deb.debian.org` und
`security.debian.org`, jeweils Port 80 oder 443. Numerische Ziele, weitere
SOCKS-Befehle, andere Namen/Ports und spezielle oder lokale IPv4-Adressen nach
DNS-Auflösung werden abgewiesen. IPv6 wird zunächst nicht verwendet. Die
SOCKS5h-Unterstützung ist in der [Debian-APT-Dokumentation](https://manpages.debian.org/trixie/apt/apt-transport-http.1.en.html)
beschrieben. APTs Signatur-, Zeit- und Hash-Prüfungen und die unabhängige Bindung
von Release, vollständigem Paketindex und Archiv bleiben erforderlich.

Der Broker besitzt beide Kinder über beim Start erzeugte pidfds. Ein begrenzter
zweiter cgroup-Start ist nur nach der ersten Belegung, für denselben Benutzer und
dieselbe Seriennummer, genau einmal und vor jedem STOP erlaubt. Beide Kinder und
alle Verbindungen teilen das bestehende Speicherlimit. Höchstens acht
Verbindungsarbeiter mit begrenzten Puffern, Handshake-/Verbindungsfristen,
30 Sekunden Wartezeit, 15 Minuten Lebensdauer und 1 GiB Übertragungsbudget pro
Verbindung werden zugelassen. Nach Abbruch beziehungsweise Planerabschluss muss
der Broker beide direkten Kinder einsammeln und eine leere cgroup bestätigen,
bevor er Ergebnisdateien freigibt.

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
