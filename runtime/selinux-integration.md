# SELinux- und Init-Integration der Runtime

Stand 29. September 2026: Implementierung zur Kompilierung vorbereitet;
noch kein Nachweis eines gestarteten, erzwingend getrennten Linux-Kontexts.
Das ausgelieferte Produkt bleibt `ro.aegis.runtime.mode=absent`.

Vier eigene Domänen trennen den vertrauenswürdigen Speicher-/Prozessbesitzer,
die kurzlebige Namespace-Einrichtung, den persönlichen Aufseher und normale
GNU-Programme. Nur SystemServer darf den init-eigenen Broker-Socket verbinden;
Dateirechte und native Peer-/Protokollprüfung gelten zusätzlich. Es wird kein
öffentlicher Binder- oder Shell-Zugang zum nativen Owner eingeführt.

Die gepinnte Plattformpolicy verbietet neuen Domänen Mounts, Mknod und DAC-
Ausnahmen. `register-runtime-policy.py` ergänzt deshalb ausdrücklich benannte
Attribute für genau diese neuen vertrauenswürdigen Komponenten. Es entfernt
keine Neverallow-Regel und nimmt keine bestehende Android-Domäne neu aus.
Zusätzliche Neverallows schließen die Attributmitgliedschaft auf die genannten
AEGIS-Typen. Normale GNU-Programme erhalten keine Capability, Mountberechtigung
oder Schreibberechtigung auf die gemeinsame Softwarebasis. Die normale
Android-Shell-Ausführungsfreigabe wird für sämtliche Runtime-Domänen entfernt.
SD-Card-/FUSE-Attribute und permissive Domänen werden nicht verwendet.

Die AOSP-Revision und beide ursprünglichen Policydateien sind gehasht gepinnt.
Der Integrator prüft sämtliche Zieldateien vor Änderungen, erhält Sicherungen,
verweigert fremde Bearbeitungen/Symlinks und prüft die tatsächlichen Bytes nach
der Integration sowie nach dem Build erneut. Sieben inerte Hosttests prüfen
diese Quellübernahme. Sie kompilieren keine Policy und belegen keine Isolation.

Die Produktpolicy kennzeichnet unveränderliche Images, private CE-Verzeichnisse,
Geräteansichten, temporäre Dateien und den eigenen Cgroup-Unterbaum getrennt.
FScrypt-Ioctl-Freigaben betreffen ausschließlich Richtlinien-/Schlüsselstatus,
nicht Erzeugung oder Entzug von Schlüsseln. AOSP bleibt dafür zuständig.
Die einzige `no_new_privs`-/`nosuid`-Transition führt vom geprüften Aufseher
zu einem gewöhnlichen Programm aus der unveränderlichen Softwarebasis.

Der Init-Eintrag startet nur im expliziten verwalteten Modus nach `post-fs-data`.
Ein fehlgeschlagener Broker wird nicht automatisch neu gestartet. Der native
Handshake darf erst nach bestätigter Altprozessbereinigung erfolgreich sein.
Der Modus wird erst nach Policy-Kompilierung und eigenen neuen QEMU-Prüfungen
aktiviert. Noch erforderlich sind die tatsächlichen Domänenübergänge,
Mount-/CE-/PTY-Zugriffe und Fehlerfälle einschließlich Benutzerisolation und
vollständigem Abbau vor CE-Sperre. Ein erfolgreicher Compiler allein genügt nicht.
