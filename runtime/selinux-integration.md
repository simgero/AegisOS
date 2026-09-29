# SELinux- und Init-Integration der Runtime

Stand 29. September 2026: Die Policy und Komponenten von
`6633a0862796d70304456c363158d133e9711513` wurden auf `aegis-build` erfolgreich
kompiliert, einschließlich Neverallow-, API-Freeze-, Treble-, Kontext- und
Policy-Tests. Lauf `identity-20260928T235046Z-6633a086-BVktdL` endete mit
`IDENTITY_COMPILED_NOT_INSTALLED`. Das ist noch kein Nachweis eines gestarteten,
erzwingend getrennten Linux-Kontexts.
Das bisher als sichtbarer Stand geprüfte Profil bleibt im Modus `absent`.
Der neue Vollbuild `a8d38b97` bootet im separaten Profil tatsächlich mit
`managed-v1`, SELinux Enforcing und bestätigtem Verity. Init startet den Broker,
dieser bricht jedoch vor dem Anlegen seiner Zustandsdatei mit Exitcode 1 ab.
Ein spezifischer AVC fehlt; seine bisherigen stderr-Diagnosen werden von
Init verworfen. Damit ist weder ein produktiver Runtime-Start noch dessen
Isolation nachgewiesen. Feste Android-Logmeldungen und die Korrektur der
beobachteten Helfer-Eigentümerschaft `root:shell` sind als Quellstand `4e53dc18`
kompiliert. Seine 113 nativen root-Fixture-Tests bestehen im unveränderten
Testimage `a8d38b97`; der neue produktive Dienst und seine tatsächlichen
Diagnosemeldungen sind damit weiterhin nicht geprüft.

Der Teststand aktiviert `managed-v1` nur mit den explizit geprüften
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
# Init-Namespace-Übergabe nach dem verwalteten Bootversuch

Das vollständige Image `4e53dc18` bootet im lokalen QEMU mit Enforcing und
Verity. Der Init-gestartete Broker meldet im durchgehenden seriellen Log
`AEGIS_RUNTIME_NAMESPACE_FAILED: open init proc directory errno=2`.
Der Gast verwendet `/proc` mit `hidepid=invisible`. Die bisherigen Root-
Gerätetests im Entwicklungsbereich konnten PID 1 lesen und deckten diese
Produktionsgrenze nicht ab.

Die vorbereitete Korrektur verwendet drei feste `file /proc/1/ns/... r`-
Einträge im Init-Dienst. Der gepinnte AOSP-Init öffnet diese vor dem Fork,
publiziert sie im Kind und wechselt anschließend zum Dienstprogramm.
Der Broker akzeptiert ausschließlich die bekannten Deskriptoreinträge aus
diesem Startpfad, prüft NSFS, Zugriffsmodus, Namespace-Art, eigene Namespace-
Identität und die bisherigen Root-/Mapping-/Einzelthreadbedingungen. Er hält
CLOEXEC-Kopien; ein Fork oder der Austausch einer einmal gebundenen Namespace-
Identität kann keine neue Besitzerberechtigung erzeugen. Die private Mount-
Namespace bleibt danach an ihren ursprünglichen Prozess gebunden.

Ein NSFS-eigenes Genfs-Label ersetzt hier das zuvor beobachtete `unlabeled`
für Kernel-Namespace-Objekte. Die Laufzeit erhält keine Leserechte auf
allgemeine unbeschriftete Dateien. Init und Vold behalten ihren vorhandenen
lesenden Namespace-Zugriff; die Runtime erhält nur die notwendigen Lese-
und Typ-/Eigentümerabfragen. Ptrace auf Init, Readproc-Gruppenzugehörigkeit,
Schreibzugriff auf Namespace-Objekte oder Abschalten von SELinux sind kein
Bestandteil der Korrektur. Die bisherigen ungenutzten PID-1-Procrechte des
Brokers entfallen.

Kompilierung, vollständiger Image-Start mit der neuen Policy, persönliche
Kontexte und GNU-Ausführung stehen für diese Korrektur noch aus.
