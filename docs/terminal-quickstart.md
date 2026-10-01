# AEGIS im Terminal verwenden

Diese Anleitung gilt für das lokale ARM64-Entwicklungsimage mit `managed-v1`.
Builds und Profile bleiben auf dem Server. Den jeweiligen Teststand und seine
Grenzen dokumentiert [Serverentwicklung](server-development.md).

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
adb -s 127.0.0.1:15871 shell -t su 0 aegis
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
AEGIS_PREPARED=out/server-stability/candidate-c526571
AEGIS_PROFILE=out/qemu-profiles/server-personal-c526571
AEGIS_RUN="$AEGIS_PREPARED/interactive-$(date -u +%Y%m%dT%H%M%SZ)"
python3 scripts/qemu-with-secure-env.py \
  "$AEGIS_PREPARED/images" out/server-stability/baseline/helper \
  "$AEGIS_PREPARED/android.raw" "$AEGIS_PREPARED/runtime.bootconfig" \
  "$AEGIS_RUN" --profile "$AEGIS_PROFILE" --create-profile \
  --seconds 0 --cpus 8 --memory-mib 4096 --adb-port 15871 --network user
```

Bei **jedem weiteren Start** desselben Profils `--create-profile` weglassen
und einen neuen Wert für `AEGIS_RUN` verwenden. Die Startprüfung hasht zuerst
die gebundenen Dateien; auf dem Server ohne KVM dauert auch der Android-Start
mehrere Minuten. Der Launcher bleibt im Vordergrund. In einem zweiten Terminal
den tatsächlich ausgegebenen Run-Pfad für den ADB-Aufruf verwenden; nur beim
allerersten Zugang zum neuen Profil `--authorize-this-host` ergänzen.

Das synthetische Abnahmeprofil unter `candidate-c526571/profile` bleibt als
Beleg erhalten. Seine Zufallspasswörter werden nicht ausgegeben. Das eigene
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
angemeldeten Administrators. Innerhalb der GNU-Shell stehen unter anderem Bash,
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
eigene Passwort durch AOSP. `status` zeigt die aktuelle Terminalanmeldung,
`linux status` den eigenen Kontext und Paketstand.

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
`linux package cancel` fordert das Aufräumen des aktuellen Auftrags an;
„Aufräumen noch nicht bestätigt“ bedeutet weiterhin einen offenen Abschluss.
Nur die bestätigte Veröffentlichung gilt als Installationserfolg.

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
