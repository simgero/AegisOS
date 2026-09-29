# Phase 1: Implementierungsstand

Stand: 29. September 2026. **Das vollständige Phase-1-Ziel ist nicht erreicht.**
Entwicklung erfolgt lokal, Kompilierung auf `aegis-build`, Systemtests in lokalem
Mac-QEMU und Quell-/Artefakttransport über GitHub.

Aktueller Nachweisstand: Das vollständige Image `a187a309` bootet im getrennten
lokalen Testprofil mit Enforcing, FBE, authentifiziertem ADB und tatsächlichem
dm-verity. Die ext4-Kontextprüfung besteht nach der gezielten Policy-Korrektur.
Der nächste Startschritt scheitert beim privaten Mount-Anker mit `EINVAL`,
ohne Runtime-AVC: Der Basisöffner gibt eine neu geöffnete Wurzel zurück und
schließt dadurch den ursprünglichen `fsmount`-Besitzer zu früh. Die lokale
Korrektur erhält diesen Deskriptor; zwei neue Gerätetests prüfen den Fehler
und den echten Basisöffner bis zum persönlichen Mount-Klon. Build und Test
der Korrektur stehen noch aus. **114 native
und 62 Java-Tests bestehen** für die unveränderten Komponenten von `026665fb`
im älteren Image `030dd177`. Öffentliche CLI-Negativtests und die verdeckte
Passworteingabe samt Strg+C-Wiederherstellung bestehen im vollständigen Image
`026665fb`; echte GNU-Sitzungs- und Rohmodusprüfungen fehlen weiterhin.
Details stehen in
[Komponententests](component-tests.md) und
[Runtime-Policy](../runtime/selinux-integration.md). Der sichtbare geprüfte
Launcher bleibt bis zur Abnahme beim Stand `2a766ab5`; bisherige gekoppelte
Android-/KeyMint-Profile und deren Basisdateien bleiben erhalten.

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
Kopie innerhalb von `super.img` wurden geprüft. **Eine echte Debian-Sitzung
wurde noch nicht gestartet.** Kernel, Namespace-/UID-, Mount- und
Speichergruppenbausteine sind Voraussetzungen, kein Ersatz dafür.

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
