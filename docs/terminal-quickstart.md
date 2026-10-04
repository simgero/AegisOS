# AEGIS im Terminal verwenden

Diese Anleitung gilt für das lokale ARM64-Entwicklungsimage mit `managed-v1`.
Der konkrete Start unten verwendet Image-Commit
`ec01e5fa5f2822da5763ab54c644bc5c5c5ab413` mit Startrezept `d9a0d30`.
Builds und Profile bleiben auf dem
Server. Den aktuellen Abnahmestand und seine offenen Pflichtfälle dokumentiert
der [Ergebnisindex](phase-1-result-index.md); Phase 1 ist noch nicht vollständig
abgenommen. Der chronologische Buildverlauf steht in
[Serverentwicklung](server-development.md); konkrete Quellpins, geprüfte
Buildartefakte und lokale Neubau-Befehle stehen im
[Build- und Startinventar](phase-1-build-inventory.md).

Auf diesem Stand sind Boot, aktive Verity-Mounts, Bildschirm, QMP-Eingabe und
authentifiziertes ADB geprüft. Alphas erste AOSP-Anmeldung und GNU-Ausführung
sowie der eigene Stopp-/Shell-/Neustartfall sind ebenfalls belegt. Die
vollständige persönliche Abnahme bleibt offen. Pfade, Quellbindungen und
Nachweisgrenzen stehen im Buildinventar und Ergebnisindex.

## QEMU und Zugang

Das persistente Profil besteht gemeinsam aus `android.qcow2`, `secure-env.ext4`
und `profile.json`. Beide virtuellen Maschinen mit
`scripts/qemu-with-secure-env.py` starten; beim ersten Anlegen ausdrücklich
`--profile PROFIL --create-profile`, bei weiteren Starts nur `--profile PROFIL`
und jeweils ein neues Laufverzeichnis verwenden. Basisdisk, Kernel,
Bootkonfiguration und Helper müssen zur unveränderten Profilbindung passen.
Details: [Persistente Profile](persistent-qemu.md).

Nach dem Start verbindet auf diesem Server beispielsweise:

```sh
python3 scripts/connect-local-adb.py PFAD_ZUM_NEUEN_RUN --wait-boot 1800
adb -s 127.0.0.1:15871 shell -tt su 0 /system_ext/bin/aegis
```

Port und Run an den konkreten Launcher-Aufruf anpassen. Nur bei der ersten
bewussten Provisionierung des eigenen Entwicklungsgastes zusätzlich
`--authorize-this-host` verwenden. Beim Neustart bleibt der bereits freigegebene
öffentliche ADB-Schlüssel erhalten. ADB und Rootkonsole sind Entwicklungszugänge;
die Benutzeraktionen im AEGIS-Terminal benötigen weiterhin AOSP-Passwörter.

## Konkreter Start auf diesem Buildserver

Nach Beendigung des automatischen Testgastes können die lokal vorhandenen
Images für ein **neues eigenes Profil** verwendet werden. Im Repository
`/home/simeongerodetti/AegisOS`:

```sh
AEGIS_PREPARED=/srv/aegis/runs/phase1-ec01e5fa-verity0
AEGIS_PROFILE=out/qemu-profiles/server-personal-ec01e5fa
AEGIS_RUN="out/phase1-interactive/$(date -u +%Y%m%dT%H%M%SZ)"
AEGIS_UNIT="aegis-personal-$(date -u +%Y%m%dT%H%M%SZ)"
sudo systemd-run --unit "$AEGIS_UNIT" --uid simeongerodetti \
  --working-directory /home/simeongerodetti/AegisOS \
  --property=KillMode=mixed --property=TimeoutStopSec=120 \
  --property=RuntimeMaxSec=24h \
  /usr/bin/python3 scripts/qemu-with-secure-env.py \
  "$AEGIS_PREPARED/images" out/server-stability/baseline/helper \
  "$AEGIS_PREPARED/android.raw" "$AEGIS_PREPARED/runtime.bootconfig" \
  "$AEGIS_RUN" --profile "$AEGIS_PROFILE" --create-profile \
  --seconds 0 --display none --cpus 8 --memory-mib 4096 \
  --helper-timeout 600 --adb-port 15871 --network user
```

Bei **jedem weiteren Start** desselben Profils `--create-profile` weglassen
und einen neuen Wert für `AEGIS_RUN` verwenden. Die Startprüfung hasht zuerst
die gebundenen Dateien; auf dem Server ohne KVM dauert auch der Android-Start
mehrere Minuten. Der systemd-Dienst läuft unabhängig vom Terminal; Name und
Run-Pfad für Status und ADB aufbewahren. Auch `AEGIS_UNIT` erhält beim nächsten
Start einen neuen Namen. Der dokumentierte Testbetrieb hat eine ausdrückliche
24-Stunden-Laufzeitgrenze und ist kein automatischer Dauerstart nach Host-Reboot.
Den Startfortschritt zeigt `journalctl -u "$AEGIS_UNIT" -f`; den Dienstzustand
zeigt `systemctl status "$AEGIS_UNIT"`. Für ADB den tatsächlichen Run-Pfad
verwenden; nur beim allerersten Zugang zum neuen Profil
`--authorize-this-host` ergänzen.

Das synthetische Abnahmeprofil unter
`/srv/aegis/runs/phase1-ec01e5fa-verity0/profile` und die älteren Profile bleiben
als Belege erhalten. Ihre Zufallspasswörter werden nicht ausgegeben. Das eigene
Profil startet ohne persönliche Benutzer; dort `setup` verwenden und eigene
Passwörter interaktiv setzen. Gemeinsame und private Testpakete gehören zum
Abnahmeprofil, nicht zum unveränderlichen Factory-Image.

## Zwei Benutzer und GNU-Programme

Im **selben** `aegis>`-Terminal nacheinander arbeiten; Passwörter nur in den
verdeckten interaktiven Abfragen eingeben:

```text
setup Alpha
login Alpha
user add Beta
linux start
linux shell
```

`setup` ist einmalig für den ersten Administrator. Neue Benutzer erhalten ihr
Passwort bei der Anlage; `user add` verlangt außerdem das frische Passwort des
angemeldeten Administrators. `setup --resume NAME` setzt ausschließlich eine
protokollierte unterbrochene Ersteinrichtung fort und ist kein Passwortreset.
`user list` zeigt die persönlichen AOSP-Benutzer. Ein weiterer Administrator
wird ausdrücklich mit `user add NAME --admin` angelegt. Namen mit Leerzeichen
in Anführungszeichen setzen. Innerhalb der GNU-Shell stehen unter anderem Bash,
Coreutils, APT und Dpkg bereit:

```sh
printf 'Meine Datei\n' > "$HOME/Documents/beispiel.txt"
cat "$HOME/Documents/beispiel.txt"
exit
```

`exit` kehrt von GNU zur AEGIS-CLI zurück. Dann beispielsweise:

```text
switch Beta
linux start
linux shell
```

Jeder Benutzer hat ein eigenes `/home/user`, eigene temporäre Verzeichnisse
und getrennte Laufzeitprozesse. Ein Wechsel authentifiziert den Zielbenutzer;
der vorherige Benutzer kann im Hintergrund weiterlaufen. `passwd` ändert das
eigene Passwort durch AOSP und fragt das bisherige sowie zweimal das neue
Passwort verdeckt ab. `status` zeigt die aktuelle Terminalanmeldung,
`linux status` den eigenen Kontext und Paketstand.

Der vorgesehene Ablauf lautet immer `linux start`, danach `linux shell`.
Im korrigierten Servicecode verlangt `linux shell` einen bereits bereiten
Kontext und startet ihn nicht selbst. `exit` am `aegis>`-Prompt schließt dagegen
den AEGIS-Client; eine spätere CLI erbt dessen Anmeldung nicht. Auch dieses
Client-Ende ist kein bestätigter AOSP-Logout.

Nach einem gemeinsamen Paketupdate kann `linux start` die persönliche
Paketauswahl neu abgleichen. Auf ARM64-QEMU ohne Hardwarebeschleunigung dauert
bereits die Planung mehrere Minuten. Im aktuellen Quellstand wartet dieser
Aufruf insgesamt höchstens 15 Minuten.
Ein Zeitablauf bestätigt weder einen gestarteten Kontext noch dessen Abbau.
Dann den Zustand mit `linux status` prüfen und vor einem neuen Start den eigenen
Kontext mit `linux stop` geordnet beenden. Dies ist keine Abmeldung.

## Gemeinsame und persönliche Pakete

Zurück am `aegis>`-Prompt:

```text
linux package install ed --scope all
linux package install hello --scope user
linux package status
```

`all` verändert die gemeinsame Softwarebasis; `user` nur die persönliche
Paketauswahl des angemeldeten Benutzers. In diesem ersten Entwicklungsstand
verlangen **beide** Aktionen eine frische AOSP-Adminfreigabe. Den angezeigten
Plan mit Versionen und Änderungen prüfen und anschließend Adminname und
Passwort in die Abfragen eingeben. Der Adminname verändert das persönliche
Installationsziel nicht. Ein leerer Adminname bricht ab.

Eine genaue Version lässt sich als `hello=VERSION` anfordern. Auch `update`
und `remove` verwenden einen ausdrücklichen Bereich und geprüften Plan.
Die vollständigen Formen sind:

| Aktion | Gemeinsame Software | Persönliche Software |
| --- | --- | --- |
| Installieren | `linux package install NAME[=VERSION] --scope all` | `linux package install NAME[=VERSION] --scope user` |
| Aktualisieren | `linux package update --scope all` | `linux package update --scope user` |
| Entfernen | `linux package remove NAME --scope all` | `linux package remove NAME --scope user` |

`NAME` und `VERSION` ersetzen; eckige Klammern kennzeichnen die optionale
Versionsangabe und werden nicht eingegeben. Update nimmt keinen Paketnamen
entgegen. Alle sechs Formen verlangen Adminfreigabe. Private Entfernung hebt
die eigene ausdrückliche Auswahl auf; der Plan zeigt, ob danach eine gemeinsame
Version verwendet wird, eine Abhängigkeit bleibt oder das Paket entfernt wird.

`linux package cancel` fordert das Aufräumen des aktuellen Auftrags an;
„Aufräumen noch nicht bestätigt“ bedeutet weiterhin einen offenen Abschluss.
Nur die bestätigte Veröffentlichung gilt als Installationserfolg.
`linux package status` und `linux package approve` beziehen sich auf den Auftrag
des betreffenden Terminals. Ein separat gestartetes `aegis` erbt weder die
persönliche Anmeldung noch dessen Paketauftrag. Die genannten Befehle beschreiben
die Schnittstelle; ihre vollständige Variantenabnahme steht im Ergebnisindex.

Laufende GNU-Kontexte behalten ihre bisherige Softwaregeneration. Wenn
`linux status` eine ausstehende Aktivierung meldet, zuerst eigene Arbeiten
beenden, dann `linux stop`, `linux start`, `linux shell`. Die Basis ist innerhalb
der normalen GNU-Shell schreibgeschützt; Installationen erfolgen über die
AEGIS-Paketbefehle.

## Abmelden und Neustarten

Von GNU zuerst `exit`, dann am `aegis>`-Prompt `logout`. Erfolgreicher Logout
bestätigt das Ende des eigenen Kontextes und die AOSP-CE-Sperre. Ein bloßes
`exit`, Benutzerwechsel oder Bildschirmsperre ist keine vollständige Abmeldung.
Eine unbestätigte Sperre nicht als Erfolg behandeln; der Dienst verhindert
während des ausstehenden Schlüsselentzugs eine neue persönliche Freigabe.

Zum geordneten Beenden des gesamten Entwicklungsgastes:

```sh
adb -s 127.0.0.1:15871 shell su 0 setprop sys.powerctl shutdown
```

Der gepaarte Launcher beendet danach den KeyMint-Helfer. Android-Power-down
und `helper-shutdown.txt` mit `clean` kontrollieren. Beim nächsten Start
**dasselbe vollständige Profilpaar** verwenden. Keine einzelne Disk ersetzen
oder zurücksetzen. Die erneute AEGIS-Anmeldung entsperrt die persönlichen Daten.

Der Helper ist ein Software-TPM für Entwicklung. Der Hostadministrator gehört
zur Vertrauensbasis. Eine Migration auf andere Images ist nicht implementiert. `user remove NAME`
entfernt einen anderen persönlichen Benutzer einschließlich seiner Daten
und verlangt eine frische Adminprüfung; nur für bewusst zu löschende Konten
verwenden. Die automatischen Testprofile enthalten ausschließlich synthetische
Benutzer mit nach Testende verworfenen Zufallspasswörtern.
