# Kontrollierter Paketabruf

Der unveränderliche Paketplaner kann einen vom Broker zugelassenen Internetmodus
verwenden. Persönliche Shells und die späteren Installationsprogramme behalten
jeweils ihren eigenen Netzwerkraum ohne diese Verbindung. Die öffentliche
CLI/Binder-Anbindung und die frische AOSP-Adminprüfung sind im Quellstand
`7902400f` verbunden; 400 native und 152 Java-Komponententests bestehen im
lokalen QEMU. Das Vollimage `73c0f3eb` bootet mit Enforcing, dm-verity und
authentifiziertem ADB. Zwei neue Testkonten wurden über die installierte CLI
angelegt; der Paketaufruf ohne Anmeldung wird abgewiesen. Der erste angemeldete
Aufruf `package install --user hello` erreicht den echten Broker, scheitert
aber vor der Freigabe an dessen fehlendem `NS_GET_USERNS`-Recht (NSFS-ioctl
`0xb701`). Der Broker benötigt es zum Vergleich des Planer-Netzwerkraums mit
dessen eigenem Benutzerraum (`aegis_namespace_planner_network`). Die Richtlinie
ergänzt genau dieses ioctl für die vorhandene Broker-Domäne und den eigenen
NSFS-Typ; keine neue Domäne, Capability oder Namespace-Schreibberechtigung.
Das Vollimage `c9cd225d` wurde auf dem SSH-Builder gebaut, über GitHub verifiziert
und lokal mit Enforcing gestartet. Der erneute tatsächliche CLI-Aufruf erreicht
nun den Netzwerkhelfer; die NSFS-Verweigerung tritt nicht mehr auf. Dort scheitert
noch die Übergabe von `/memfd:aegis-package-network`: Versiegelung verhindert
Schreibzugriffe auf den Inhalt, ändert aber nicht den geöffneten Modus `O_RDWR`.
SELinux prüft diesen Modus beim Domänenwechsel und verweigert zu Recht `write`.

Die aktuelle Korrektur öffnet die versiegelte, größenbegrenzte Anfrage unabhängig
als `O_RDONLY` neu, prüft dieselbe Inode und schließt die schreibbar geöffnete
Beschreibung vor der Übergabe. Derselbe Fehler wird im SCM_RIGHTS-Konfigurations-
kanal zum Ausführungshelfer behoben. Beide Empfänger verlangen den Lesemodus;
keine Schreibberechtigung wird zur Richtlinie hinzugefügt. Vier native Tests
prüfen Siegel, Zugriffsmodus, Identität, ungültige Eingänge und die tatsächliche
SCM_RIGHTS-Übertragung. Die Komponenten `b6e4b93e` kompilieren; alle 67 gezielt
betroffenen nativen Prüfungen bestehen am 30. September um 21:54:43 UTC im lokalen
c9-QEMU. Benutzer-, Schlüssel- und Kontextaufnahmen sind davor/danach identisch.
Vier deaktivierte reale CE-Tests bleiben aus. [Prüfbelege](component-tests.md).

Das passende Vollimage `b6e4b93e` wurde auf dem SSH-Builder gebaut, über GitHub
verifiziert und lokal gestartet. Der erneute CLI-Aufruf am 30. September um
22:25 UTC erreicht jetzt APT und dessen Plan-Hook; die vorherige Verweigerung
der schreibbar geöffneten Anfrage tritt nicht mehr auf. Die Veröffentlichung
von `/run/aegis-apt-plan.tmp` scheitert an `link`: Die Paketprogramm-Domäne darf
in ihrem privaten Arbeitsdateisystem Dateien anlegen und umbenennen, aber keine
Hardlinks erstellen. Eine Adminfreigabe oder Paketveröffentlichung wurde nicht
erreicht. Beide Testkonten wurden regulär abgemeldet, ihre CE-Daten gesperrt und
alle Runtime-Kontexte abgebaut.

Der Hook veröffentlicht die vollständig geschriebene und synchronisierte Datei
nun mit `renameat2(RENAME_NOREPLACE)`. Ein vorhandenes Ergebnis wird weiterhin
niemals ersetzt; ein unvollständiger Datensatz erhält keinen endgültigen Namen.
Dieser Aufruf nutzt die bestehenden Rechte im privaten Arbeitsdateisystem und
benötigt keine zusätzliche SELinux-Freigabe. Commit `b7ee7fc8` besteht am
30. September um 22:50:39 UTC alle 62 betroffenen nativen Planungs-/Ausführungstests
im lokalen b6-QEMU; Benutzer-, Schlüssel- und Kontextaufnahmen bleiben identisch.
Das passende Vollimage wurde auf `aegis-build` gebaut, über GitHub verifiziert
und lokal mit Enforcing gestartet. Am 30. September um 23:20 UTC erreicht der
tatsächliche CLI-Aufruf den Download von `hello_2.10-5_arm64.deb`; die vorherige
Hook-Verweigerung tritt nicht mehr auf. Anschließend scheitert der Broker vor
der Adminabfrage an `setattr` auf dieser privaten Arbeitsdatei.

`prepare_planned_transaction` übernimmt Archive erst nach Ende aller
Planerprozesse und Prüfung von Typ, Eigentümer, Linkzahl, Größe und tmpfs-Herkunft.
Die Auflösung erfolgt relativ zum gehaltenen Verzeichnis ohne Symlinks oder
Mountwechsel. Der Broker setzt den gepinnten Deskriptor mit `fchown`/`fchmod`
auf `root:root`, Modus `0444`, bevor er ihn an die Vorbereitung übergibt.
Die Richtlinie ergänzt dafür ausschließlich `file:setattr` für den bestehenden
Arbeitsdateityp; keine Inhalts-, Verzeichnis- oder Ausführungsrechte.
Beide Testkonten wurden danach regulär abgemeldet, CE gesperrt und alle
Runtime-Kontexte abgebaut. Ein passendes Vollimage muss die Richtlinie kompilieren
und den öffentlichen Ablauf erneut ausführen. Die unveränderten nativen
SU-Komponententests würden diese fehlende Produktberechtigung nicht prüfen.
Die echte Paketinstallation und der vollständige Zwei-Benutzer-Ablauf bleiben
unbewiesen; erfolgreiche Komponententests allein bestätigen sie nicht.

## Tatsächliche Adminprüfung im Vollimage 9b8e5065

Das passende Vollimage bootet am 30. September um 23:54:44 UTC im lokalen
Mac-QEMU mit Enforcing, FBE, dm-verity und authentifiziertem ADB. Die vier
Pakethelfer sind bytegleich zu den zuletzt geprüften b7-Komponenten. Der
Paketplan erreicht nun die öffentliche Adminabfrage; der frühere Broker-
`setattr`-Fehler ist behoben.

Der nicht angemeldete Aufruf wird ohne Zustandsänderung abgewiesen. Beta ist
anschließend als normaler Benutzer angemeldet, Alpha als Administrator gestoppt
und CE-gesperrt. AOSP weist ein falsches Alpha-Passwort zurück; die Freigabe
mit Betas korrektem Passwort scheitert an dessen fehlender Adminrolle. Nach
beiden Versuchen bleiben die Generationsauswahlen unverändert, Alpha gesperrt
und Betas Runtime erhalten. Ein neuer Plan ist ohne erneute Anmeldung möglich;
der versiegelte alte Paket-Binder verweigert weitere Statusaufrufe.

Mit dem korrekten Alpha-Passwort erreicht der Auftrag am 1. Oktober um
00:00:23 UTC den tatsächlichen Ausführungshelfer. Dessen `readback()` scheitert
vor APT/dpkg an `getattr` auf `/dev/pts/ptmx`: Der Helfer erzeugt einen eigenen
`devpts`-Mount mit `newinstance`, dessen Multiplexer bisher den allgemeinen
`devpts`-Typ erhielt. Die Richtlinie ergänzt einen eigenen
`aegis_package_worker_devpts`-Typ, den Übergang beim Anlegen durch den Helfer
und ausschließlich `getattr` für die anschließende Prüfung. Es werden keine
Terminal-Ein-/Ausgabe- oder allgemeinen Android-PTY-Rechte ergänzt.

Es wurde noch kein Paket veröffentlicht. Abbruch und normale Abmeldung sperren
beide Testkonten und entfernen alle persönlichen Runtime-Kontexte. Belege:
`out/full-build-9b8e5065/identity-test/`. Die neue Policy benötigt erneut ein
passendes Vollimage und den echten Produktablauf; Komponententests ersetzen
keinen dieser noch fehlenden Nachweise.

## Stand von Broker, Richtlinie und Paketauftrag

Das Vollimage `40179351` bootet bereits mit Enforcing, der korrigierten
Paketdomänen-Richtlinie und dem echten init-gestarteten Broker samt vier
gepinnten Helfern. Seine eigenen 393 nativen Tests und die nachfolgenden
Komponentensuiten bis 400 native/152 Java sind dokumentiert in den
[Komponentenbelegen](component-tests.md). Die früheren Policy-Fehler von
`2308ea46` und `99ead3fd` haben keine verifizierten Images erzeugt.

`BrokerPollConfiguredPackage` und `BrokerCancelConfiguredPackage` geben
Auskunft über dieselben registrierten Auswahl-, Planungs- und Ausführungsphasen.
Der ursprüngliche Auftrag bleibt beim Übergang in die Ausführung erhalten.
Fremde Identitäten und interne Fixture-Aufträge werden abgewiesen; ein
unbekannter Job wird niemals neu gestartet. Die Java-Transaktion übernimmt
auch einen partiell registrierten BEGIN und behält die Aufräumverantwortung
bei verlorener Antwort bis zum bestätigten Ganzbenutzer-STOP.

Der öffentliche Client hält einen unveränderlichen, prozessgebundenen Binder
für genau einen Auftrag. Frische AOSP-Prüfungen und Plananzeige gehen dem
begrenzten Start unter RuntimeAdmission voraus. Abbruch nach Widerruf benötigt
keine neue Anmeldung, darf aber ausschließlich den bereits registrierten Job
sperren. Nur nach bestätigtem CANCEL oder bereits erfolgter Veröffentlichung
wird dessen abschließender Status zum Aufräumen ohne neue Zulassung abgeholt.
[CLI-Befehle und Grenzen](identity-cli.md).

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

Diese Startanbindung ist im Vollimage `2f7b18e2` installiert und am 30.09.2026
im lokalen Mac-QEMU mit dem tatsächlichen init-gestarteten Broker geprüft.
Der Start erreicht nach der festen Richtlinienübernahme auch
`AEGIS_PACKAGE_HELPERS_PINNED` und `AEGIS_RUNTIME_BROKER_LISTENING` in der
vorgesehenen Broker-Domäne bei aktivem SELinux Enforcing. Der öffentliche
Paketauftrag und die Ausführungsdomänen bleiben offen.
[Genauer Bootnachweis und Grenzen](component-tests.md).

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

Der Broker-Start öffnet die vier festen Produkthelfer für Planung, Netzwerk,
Ausführung und Veröffentlichung vor dem Lauschen auf dem Kontrollkanal.
Jeder Eingang muss eine einzelne root:shell-eigene Datei mit Modus 0755 auf
schreibgeschütztem EROFS und seinem genauen eigenen SELinux-Ausführungstyp sein.
Erst nach vollständiger Prüfung werden beide internen Helferpaare eingerichtet;
ein Fehler beendet den Start ohne nutzbaren Kontrollkanal. Die zusätzlichen
lokalen Deskriptoren werden nach Übernahme geschlossen. Ein fester Logmarker
`AEGIS_PACKAGE_HELPERS_PINNED` belegt diesen Startschritt, keine Paketausführung.

Die Ausführungsdomänen, öffentliche Wire-/Binder-/CLI-Operationen und frische
AOSP-Adminbestätigung bleiben ausstehend. Die readonly Startberechtigungen
aktivieren keinen allgemeinen Exec-, Netzwerk- oder Dateischreibzugriff.
Kompilierung, GitHub-Übertragung und der tatsächliche Vollimage-Start sind für
`2f7b18e2` nachgewiesen. Alle vier festen Dateitypen, root:shell/0755, einfache
Linkzahl und Hashes sind im Gast dokumentiert. Der normale Android-Shell-Zugriff
auf die Metadaten des Planers wurde verweigert. Die wiederholte Zusatzprüfung
verwendet den vorhandenen Diagnosezugang und erweitert keine Produktberechtigungen.

`BrokerPrepareConfiguredTransaction` übernimmt inzwischen ausschließlich den
behaltenen geprüften Auftrag (Identität, Seriennummer, Auftrags-ID und neue Frist).
Der Aufrufer liefert weder Digest noch Datei-, Store-, Arbeitsverzeichnis-,
Archiv- oder Helferdeskriptoren. Ausführung und Veröffentlichung werden beim
Broker-Start gemeinsam gepinnt; spätere Ersetzung ist ausgeschlossen. Die
bestehende vorbereitende Hilfe stammt aus der gepinnten Auswahlkonfiguration.

Der Übergang registriert dieselbe Auftrags-ID als Installation, bevor er feste
Speicherpfade öffnet oder anlegt. Gemeinsam genutzte Dateien liegen ausschließlich
unter `shared-packages` und `shared-staging` im gepinnten Broker-Verzeichnis;
Arbeitsverzeichnisse haben eine neue Zufallskennung und den eigenen SELinux-Typ
`aegis_package_staging_file`. Nur diese Arbeitsabbilder erhalten Loop-Schreibzugriff;
fertige gemeinsame Generationen bleiben für den Kernel schreibgeschützt.
Besitzer, Modus, SELinux-Typ,
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
diesen Übergang nicht. Der registrierte Auftrag hält jetzt seine selbst neu
angelegten Arbeitsverzeichnisse bis zur Bereinigung. Erst nach Ende aller Helfer,
Mounts und Loop-Verbindungen werden die festen Dateien `request` und
`candidate.ext4` sowie genau dieses Verzeichnis entfernt. Größe, Modus, Besitzer,
Dateityp, Linkzahl und ursprüngliche Verzeichnisbindung werden geprüft; fremde
Einträge und ausgetauschte Verzeichnisse verhindern das Löschen. Ein Fehler
behält Auftrag und CE-Referenzen für einen erneuten Versuch und blockiert die
Abschluss-/STOP-Bestätigung. Das gilt auch für Hintergrundabschluss nach einer
bereits veröffentlichten Generation. Extern übergebene Arbeitsverzeichnisse
werden nicht gelöscht.

Diese Bereinigung betrifft den Lebenszyklus des aktuell gehaltenen Auftrags.
Die sichere Behandlung verwaister Verzeichnisse nach Prozessabsturz oder
Neustart, Kapazitätsbegrenzung und Produktintegration bleiben ausstehend.
Dieser Pfad wird deshalb noch nicht als öffentlicher Befehl freigeschaltet.

## Trennung des beschreibbaren Kandidaten

`package_prepare_worker.cpp::Mount` verwendet für beschreibbare Kopien jetzt
`defcontext=u:object_r:aegis_package_candidate_file:s0`. Dieser eigene Inode-Typ
ist ein `file_type`, kein `fs_type` und kein `contextmount_type`. Das Dateisystem
behält `labeledfs`. Der vertrauenswürdige Vorbereiter darf diese Kandidaten lesen
und beschreiben, aber nicht ausführen. Bestehende Runtime-Domänen erhalten weder
Zugriff auf die Kandidatenverzeichnisse noch Ausführungsrechte. Veröffentlichte
Auswahlen behalten ihren bisherigen schreibgeschützten `context`-Mount mit
`aegis_runtime_base_file`; AOSPs Contextmount-Schreibsperren bleiben unverändert.

Der tatsächlich gepinnte Kernelstand
`50eb8d5d443b43f38d6e72f005f1b8601ac88a05` prüft `defcontext` in
`security/selinux/hooks.c::selinux_set_mnt_opts` über
`may_context_mount_inode_relabel`. Es ändert den Standard für Inodes ohne
SELinux-Xattr. Ein vorhandenes fremdes Label wird nicht überschrieben;
insbesondere wird kein `rootcontext` verwendet, das ein fremdes Root-Label
verdecken könnte.

`aegis_package_candidate_labels` prüft deshalb den vollständigen stillstehenden
Kandidaten vor der Archivübernahme, erneut vor der Mount-Übergabe, vor dem ersten
Paketprogramm und in beiden abschließenden Validierungen. Dateien, Verzeichnisse
und die Inodes symbolischer Links müssen exakt den Kandidatentyp tragen. Die
Linkziele werden nicht verfolgt. Auflösung erfolgt relativ zu gehaltenen
Verzeichnisdeskriptoren, mit Inode-Abgleich, begrenzter Tiefe und Eintragszahl;
fremde Mounts und besondere Dateien werden abgewiesen. Alle Deskriptoren werden
auch bei Fehlern geschlossen. Es werden keine Labels repariert oder verändert.

Nur im bereits aufgebauten Ausführungs-Namespace dürfen die separat geprüften
Wurzeln `/dev`, `/proc`, `/tmp` und `/run` andere Dateisysteme sein. Ein bloßes
Verzeichnis mit einem dieser Namen wird weiter vollständig geprüft; geschachtelte
oder anders benannte Fremd-Mounts sind nicht erlaubt. Der Aufrufer muss weiterhin
die feste Mount-Inventur prüfen und alle Paketkinder vor Abschluss einsammeln.

Sieben neue Labelprüfungen und zwei Tests mit tatsächlich kopierten ext4-Abbildern
sind ergänzt. Die ext4-Tests setzen absichtlich fremde Root-/Symlink-Labels in
neuen isolierten Testkopien, berechnen deren neuen Eingabehash und verlangen die
Ablehnung vor dem Mount-Handoff. Commit `c6f43096` besteht im passenden Vollimage
alle 384 aktivierten nativen Tests; Nachweise stehen in
[component-tests.md](component-tests.md). Native SU-Fixtures allein beweisen weiterhin keine erfolgreiche
Installation in einer Produktdomäne. Öffentliche Befehle und AOSP-Freigabe
bleiben ausstehend; die folgende Domänentrennung benötigt einen neuen Build.

## Fester Einstieg in die Paketprogramm-Domäne

Die Produkt-Policy definiert getrennte Domänen für den vertrauenswürdigen
Namespace-Supervisor (`aegis_package_worker`), die GNU-Paketprogramme
(`aegis_package_program`) und den begrenzten Netzwerkvermittler
(`aegis_package_network`). Nur der Broker darf die festen Helfer starten.
Kandidaten, gewöhnliche Arbeitsdateien und kopierte Hooks sind keine
Domänen-Einstiegspunkte. Die bestehenden AOSP-Regeln für `entrypoint` bleiben
unverändert.

Nach Pivot, Schließen aller Hostdeskriptoren, Capability-Begrenzung,
`no_new_privs` und Seccomp startet der Kindprozess über `/proc/self/exe` denselben
unveränderlichen statischen Helfer erneut. Erst dessen fester Programmeinstieg
wechselt in die Paketprogramm-Domäne. Er prüft PID-/Sitzungskontext, alle
Benutzer-/Gruppen-IDs, das Fehlen ergänzender Gruppen, die gesperrten Securebits,
Seccomp, `no_new_privs` und genau sechs Capabilities einschließlich Bounding- und
Ambient-Set. Nur danach werden APT, dpkg oder apt-mark mit den festen Argumenten
des Supervisors gestartet. Steuerkanäle oder Hostdateideskriptoren überleben
nicht. Es gibt keine Umgebungsvariable zum Abschalten dieser Produktprüfung.

Die spezielle Ausnahme von AOSPs allgemeinem Inode-Ausführungsverbot ist auf
den neuen Paketprogramm-Typ begrenzt. Geschlossene Attributmitgliedschaft und
zusätzliche Verbote beschränken seine ausführbaren Dateitypen auf den Kandidaten,
den schreibgeschützt eingebundenen Hook und die beiden unveränderlichen
Helfer-Einstiegspunkte. Die bisherige schreibgeschützte Runtime-Basis bleibt ein
separater Dateisystemtyp. Gewöhnliche Arbeitsdateien sind nicht ausführbar;
Paketprogramme dürfen weder Mounts verändern noch den Supervisor beeinflussen.
Der Netzwerkhelfer verwendet gezielte TCP- und Android-DNS-Rechte, kein breites
`netdomain`-Attribut. Seine Capabilities werden vor SOCKS-Anfragen abgelegt.

Fünf neue Gerätetests prüfen direkte Produktionshelfer-Aufrufe außerhalb des
vorbereiteten Kontexts sowie die Ablehnung fremder Pfade und übergroßer
Argumentlisten. Vorhandene APT-Fixtures durchlaufen den erneuten Helferstart in
separaten, nicht installierten Probe-Binärdateien. Build, Policy-Kompilierung und
Laufzeitnachweis dieser Änderung stehen noch aus. Positive SU-Fixtures ersetzen
weiterhin keinen Nachweis der tatsächlichen SELinux-Domänenwechsel im Produkt.

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
