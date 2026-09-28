# SELinux- und Init-Integration der Runtime

Stand 29. September 2026: Die Policy und Komponenten von
`6633a0862796d70304456c363158d133e9711513` wurden auf `aegis-build` erfolgreich
kompiliert, einschließlich Neverallow-, API-Freeze-, Treble-, Kontext- und
Policy-Tests. Lauf `identity-20260928T235046Z-6633a086-BVktdL` endete mit
`IDENTITY_COMPILED_NOT_INSTALLED`. Das ist noch kein Nachweis eines gestarteten,
erzwingend getrennten Linux-Kontexts.
Das zuletzt ausgelieferte Produkt bleibt `ro.aegis.runtime.mode=absent`.
Der nächste Teststand aktiviert `managed-v1` nur mit den explizit geprüften
Basis- und Kernel-Eingaben des Build-Workers. Eine normale Quellregistrierung
entfernt diese Auswahl; ohne Basis bleiben Modus und Dienste inaktiv, mit Basis
aber ohne ausgewählten Kernel bricht bereits die Produktkonfiguration ab.

Vier eigene Domänen trennen den vertrauenswürdigen Speicher-/Prozessbesitzer,
die kurzlebige Namespace-Einrichtung, den persönlichen Aufseher und normale
GNU-Programme. Nur SystemServer darf den init-eigenen Broker-Socket verbinden;
Dateirechte und native Peer-/Protokollprüfung gelten zusätzlich. Es wird kein
öffentlicher Binder- oder Shell-Zugang zum nativen Owner eingeführt.

Die gepinnte Plattformpolicy verbietet neuen Domänen Mounts, Mknod und DAC-
Ausnahmen. `register-runtime-policy.py` ergänzt deshalb ausdrücklich benannte
private Attribute für genau diese neuen vertrauenswürdigen Komponenten. Die
eingefrorene öffentliche Android-Policy bleibt unverändert. Eine weitere,
geschlossene Zuordnung erlaubt nur dem Aufseher und normalen GNU-Programmen
die Ausführung aus dem unveränderlichen Basis-Dateisystem. Dieses trägt nach
AOSP-Vorgabe `fs_type` und niemals zugleich `file_type`; die bestehenden
Schreibverbote für `contextmount_type` gelten unverändert. Es entfernt
keine Neverallow-Regel und nimmt keine bestehende Android-Domäne neu aus.
Zusätzliche Neverallows schließen die Attributmitgliedschaft auf die genannten
AEGIS-Typen. Normale GNU-Programme erhalten keine Capability, Mountberechtigung
oder Schreibberechtigung auf die gemeinsame Softwarebasis. Die normale
Android-Shell-Ausführungsfreigabe wird für sämtliche Runtime-Domänen entfernt.
Ebenso entfällt deren allgemeiner Android-Schreibzugriff auf Host-Cgroups;
der Broker erhält ausdrücklich nur seinen eigenen Unterbaum. Ein zusätzliches
Neverallow verbietet Runtime-Schreibzugriffe auf die übrige Cgroup-Hierarchie.
SD-Card-/FUSE-Attribute und permissive Domänen werden nicht verwendet.

Die AOSP-Revision und vier ursprüngliche Policydateien sind gehasht gepinnt.
Der Integrator prüft sämtliche Zieldateien vor Änderungen, erhält Sicherungen,
verweigert fremde Bearbeitungen/Symlinks und prüft die tatsächlichen Bytes nach
der Integration sowie nach dem Build erneut. Sieben inerte Hosttests prüfen
diese Quellübernahme. Zwei weitere prüfen die Übernahme einer eigenen älteren
Quellregistrierung und den Schutz fremder Änderungen beim Ergänzen verwalteter
Dateien. Sie kompilieren keine Policy und belegen keine Isolation.

Die Produktpolicy kennzeichnet unveränderliche Images, private CE-Verzeichnisse,
Geräteansichten, temporäre Dateien und den eigenen Cgroup-Unterbaum getrennt.
FScrypt-Ioctl-Freigaben betreffen ausschließlich Richtlinien-/Schlüsselstatus,
nicht Erzeugung oder Entzug von Schlüsseln. Die ursprüngliche Vold-Neverallow
für Schlüsseländerung und Status wird dafür getrennt: Nur beim lesenden
Statusaufruf erhält der neue Broker eine Ausnahme, zusätzlich auf seine
CE-Dateitypen begrenzt. Die Verbote für Schlüsseländerung und das Setzen einer
Verschlüsselungsrichtlinie bleiben bestehen. AOSP bleibt dafür zuständig.
Die einzige `no_new_privs`-/`nosuid`-Transition führt vom geprüften Aufseher
zu einem gewöhnlichen Programm aus der unveränderlichen Softwarebasis.

Der Init-Eintrag startet nur im expliziten verwalteten Modus nach `post-fs-data`.
Ein fehlgeschlagener Broker wird nicht automatisch neu gestartet. Der native
Handshake darf erst nach bestätigter Altprozessbereinigung erfolgreich sein.
Der aktivierte Teststand wird erst nach Policy-Kompilierung in einem eigenen
neuen QEMU-Profil geprüft. Noch erforderlich sind die tatsächlichen Domänenübergänge,
Mount-/CE-/PTY-Zugriffe und Fehlerfälle einschließlich Benutzerisolation und
vollständigem Abbau vor CE-Sperre. Ein erfolgreicher Compiler allein genügt nicht.
