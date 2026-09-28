# Speichergrenze und bestätigter Prozessstopp

Stand: Quelltext und neun native Gerätetests vorbereitet. **Noch nicht auf dem
Builder kompiliert oder in QEMU ausgeführt.** Zehn Hosttests zur Registrierung
und neun zum Komponententransport bestehen; sieben nur unter Linux ausführbare
Transporttests sind auf dem Mac ausgelassen. Kein aktiver Runtime-Broker.

`memory_group.c` verwaltet eine interne, ausschließlich dem Host-Broker
gehörende Cgroup v2. AOSP-ID und Seriennummer sind unveränderlich gebunden;
die Zuordnung ist keine Benutzerautorisierung. Der Broker muss den leeren
Elternknoten mit `root:root 0700` und aktiviertem Speichercontroller bereitstellen.
Die Bibliothek verändert keine Controller an Androids gemeinsamer Cgroup-Wurzel.
Der bisherige QEMU-Gast stellt dort bereits den Speichercontroller bereit.

Jeder Kontext wird exklusiv als `u<ID>-s<SERIAL>` angelegt. Vorhandene Gruppen
werden nicht übernommen. Die anfänglichen Grenzen sind 1 GiB Arbeitsspeicher,
768 MiB Rückgewinnungsschwelle, kein Swap, gemeinsame OOM-Behandlung und keine
Untergruppen. Sämtliche Werte werden vor Freigabe zurückgelesen. Diese erste
Grenze ist fest im Broker-Baustein; Clients können sie nicht verändern.

`aegis_namespace_create_limited` verlangt die passende Gruppe und verwendet
`CLONE_INTO_CGROUP`. Das Kind gehört schon bei der Erzeugung zur begrenzten
Gruppe, auch während es auf seine UID/GID-Maps wartet. Der Clone-Deskriptor
wird nur einmal freigegeben; auch ein fehlgeschlagener Clone benötigt danach
Aufräumen und eine neue Gruppe. Das unbeschränkte Namespace-Primitiv bleibt
ausschließlich für inerte Gerätetests vorgesehen.

Vor Freigabe des Kindes setzt der Launcher dessen `oom_score_adj` auf null
und liest den Wert zurück. Damit übernimmt die Runtime nicht die mögliche
OOM-Ausnahme eines Android-Verwaltungsdienstes. Die besondere Behandlung von
`-1000` ist in der [Kernel-Dokumentation](https://docs.kernel.org/admin-guide/cgroup-v2.html#memory)
beschrieben.

Beim Abbau wird die Gruppe dauerhaft für neue Starts gesperrt. Der Broker
veranlasst `cgroup.kill` und wartet bis zu zehn Sekunden auf `populated=0`.
Das ist nur ein Teil des Logout-Nachweises: zusätzlich muss er den Exit seines
Kindes über das Pidfd beobachten und verbrauchen, alle CE-/Mount-/Socketreferenzen
schließen und AOSPs bestätigten Schlüsselentzug abwarten. Ein Timeout oder
ersetzter/verlorener Cgroup-Knoten gilt niemals als erfolgreiche Bereinigung.
Eine leere Gruppe kann anschließend entfernt werden. Bei Fehlern bleiben die
Besitzreferenzen erhalten; eine nach fehlgeschlagenem Öffnen nicht entfernbare
Gruppe erfordert explizite Wiederherstellung durch den Broker.

Die Tests prüfen tatsächliche Cgroup-Dateien, fehlende Controller, unveränderte
vorhandene Gruppen, Grenzwertänderung, falsche Identitäten, einmalige Freigabe,
Prozessbesitz über einen direkten `clone3` hinweg und Stopp eines Kindes bei weiterlaufendem Kind
einer anderen Gruppe. Ein kombinierter Namespace-Test prüft Mitgliedschaft
vor Exec und das Entfernen geerbter OOM-Privilegierung. Sie verwenden ausschließlich
eigene leere Testgruppen und inerte Kinder im lokalen Android-QEMU.
Die Besitzprüfung liest die Kernel-PID direkt, damit Bionics nach einem direkten
Klon geerbter PID-Cache keine Berechtigung für den elterlichen Handle erhält.

Offen bleiben insbesondere Kompilierung und Ausführung dieser Tests, ein
Speicherdrucktest der tatsächlichen Grenze, CPU-Budget, Broker-Neustart/Recovery,
SELinux-Anbindung und vollständiger Zwei-Benutzer-Logout. `CONFIG_CGROUP_PIDS`
ist im bisherigen Kernel nicht aktiv; hier wird keine PID-Cgroup-Grenze behauptet.
Der bestehende Aufseher setzt zusätzlich `RLIMIT_NPROC` für seine Nutzprozesse.
