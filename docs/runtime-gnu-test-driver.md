# Wiederholbarer lokaler GNU-/Identitätstest

`scripts/qemu-runtime-test.py` steuert ausschließlich einen bereits laufenden
lokalen QEMU-Gast über dessen authentifiziertes ADB und eine echte interaktive
AEGIS-CLI. AOSP- und native Builds laufen als `aegis-build`; auf dem Buildserver
werden die lokal verifizierten Images direkt verwendet. Dafür sind weder
GitHub-Releases noch Artefaktuploads vorgesehen; siehe
[Serverentwicklung](server-development.md). Der Treiber baut, bootet, ersetzt oder löscht
kein Profil. Er ist ein Entwicklungswerkzeug, keine neue persönliche Anmeldung.

## Voraussetzungen und Start

Benötigt werden ein **neues ausschließlich für diesen Test angelegtes Profil**,
AVB-geprüfte Images und ein vollständig gebooteter Gast mit `managed-v1`,
SELinux Enforcing und authentifiziertem ADB. Der Treiber verweigert vorhandene
persönliche Benutzer, entsperrte persönliche Speicher, belegte Runtime-Kontexte
sowie ein bereits vorhandenes Ergebnisverzeichnis. Eine Wiederholung braucht
ein neues Profil; ein altes Profil darf dafür nicht zurückgesetzt werden.

Aufrufstruktur:

```sh
python3 scripts/qemu-runtime-test.py \
  --run PFAD_ZUM_LAUFENDEN_QEMU_RUN \
  --prepared PFAD_ZU_DEN_GEPRUEFTEN_IMAGES \
  --commit VOLLSTAENDIGER_IMAGE_COMMIT \
  --output NEUES_ERGEBNISVERZEICHNIS
```

Die Pfade und der volle Commit sind ausdrücklich zu ersetzen. `--run` enthält
`profile-path.txt` und `adb-address.txt`; `--prepared` enthält
`avb-checked.json`, `android.raw` und die gebundene Bootkonfiguration. Der
Profilbezug und der tatsächliche AVB-Digest müssen übereinstimmen. Python `-O`
ist nicht zulässig, weil es die Abnahme-Assertions entfernen würde.

Nach `READY` nimmt der Treiber jeweils **einen Steuerbefehl pro Zeile** an.
Das ist seine Teststeuerung, nicht der AEGIS-Prompt. Nach jeder Aktion deren
Ergebnis prüfen. `FAILED` ist kein bestandener Schritt; spätere Wiederholung
heilt einen fehlgeschlagenen ersten Login nicht. Keine vollständige Befehlsliste
blind in den Eingabestrom kopieren.

Der Treiber erzeugt zufällige Passwörter im Speicher und übergibt sie ohne Echo
an die echte Passwortabfrage. Sie werden weder als Prozessargumente noch in
Ereignisdateien gespeichert. Nach Ende des Treibers sind sie für erneute
Anmeldung nicht mehr verfügbar. Das synthetische Testprofil und seine
verschlüsselten Daten bleiben trotzdem erhalten. `quit` schließt den Client
und verwirft die Speicherpuffer; es ersetzt ausdrücklich keinen Logout.

## Erster Zugang ohne Aufwärmversuch

1. `open`, danach `setup`: Alpha wird über AOSP als erster persönlicher
   Administrator angelegt. `expect-ce base` bestätigt CE `[0]`.
2. `direct-first-a`: keine vorherige richtige oder falsche Anmeldung im
   Treiber zulässig. Vor der Passwortübermittlung werden Vordergrundziel,
   CE und GNU-Kontext separat gelesen. Die Zielauswahl darf CE nicht
   entsperren oder einen Kontext starten. Nach korrektem Passwort müssen
   sechs Sekunden später die Sitzung und danach eine tatsächlich ausgeführte
   GNU-Prüfung bestehen. Bei Erfolg befindet sich der Treiber in Bash.
3. `home-initial` prüft genau zehn private Verzeichnisse vor eigenen
   Home-Schreibzugriffen. `basic-runtime` prüft GNU-Werkzeuge, Eigentümer,
   Namespace-/Mountsicht, schreibgeschützte Basis und Prozessbeschränkungen.
4. `gnu-write-a` erzeugt die synthetische Datei. `gnu-bg-start-a` startet
   einen auf 30 Minuten begrenzten Hintergrundprozess. `bg-observe-a`
   bindet dessen tatsächliche Host-PID, Startzeit, Kontext und Fortschritt.
5. `shell-exit` beendet nur Bash. `add-b` verlangt frische AOSP-Adminprüfung
   zur Anlage von Beta. `direct-first-b` prüft dessen ersten Zugang auf
   dieselbe Weise. Anschließend `home-initial`, `basic-runtime`,
   `gnu-write-b`, `gnu-bg-start-b` und `bg-observe-b` jeweils prüfen.

Die Hintergrundproben sind nur bis 25 Minuten nach Start als Nachweis
zugelassen; ihr natürliches Ende darf nicht als Logout-Erfolg ausgegeben werden.

## Weitere gezielte Prüfungen

| Steuerung | Zweck und notwendige Folgeprüfung |
| --- | --- |
| `isolation-from-a`, `isolation-from-b` | Aus der passenden GNU-Shell: eigener Dateireadback, verweigerte Zugriffe und SIGSTOP an den fremden Prozess. Derselbe unabhängige Peer muss vorher/nachher fortschreiten. Beide Jobs müssen bereits existieren. |
| `second-switch-a`, `second-switch-b` | Aus der jeweils anderen aktiven GNU-Shell: Anmeldung über eine zweite CLI, Widerruf der ersten PTY und Fortbestand ihres Hintergrundprozesses. |
| `screen-off`, `status`, `bg-observe-a`, `bg-observe-b`, `expect-ce both` | Widerruf des offenen Terminals bei zulässigem Hintergrundbetrieb. Power/Keyguard zusätzlich unabhängig auslesen. Eine Bildschirmsperre beweist keinen CE-Schlüsselentzug. |
| `screen-on`, `login-a` / `switch-b`, `stable-a` / `stable-b` | Frische Passwortprüfung; verzögerte Sitzungsprüfung. Kein Ersatz für `direct-first-*`. |
| `logout`, `bg-gone-a` / `bg-gone-b`, `locked-gnu-a` / `locked-gnu-b` | Ohne vorgeschalteten Runtime-Stopp: ursprünglicher Prozess beendet, Kontext entfernt, CE gesperrt und genau die zuvor von GNU geschriebene Datei unlesbar. |
| `wrong-a`, `wrong-b` | Falsches Passwort; Ziel-CE vorher/nachher mit `expect-ce` kontrollieren. Anschließende Linux-Aktion muss abgelehnt werden. |
| `hold-login-a` / `hold-login-b`, `screen-off`, `screen-on`, `submit-held` | Passwortabfrage offenhalten, tatsächliche Sperre unabhängig bestätigen, dann korrektes Passwort an den alten Versuch senden. Die widerrufene Vorbereitung muss scheitern und zuvor gesperrtes CE gesperrt bleiben. |
| `passwd-b`, `old-b`, `login-b-new` | AOSP-Passwortwechsel. Zwischen Versuchen ausdrücklich abmelden und tatsächliche CE-Sperre bestätigen. Altes Passwort muss scheitern, neues eigenen Dateizugriff erlauben. |
| `home-change-a`, `home-retained-a` | Alpha ändert Ordner und Konfigurationsbytes; Readback nach Kontextneustart, Wechsel oder Reboot darf sie nicht zurücksetzen. |
| `resize`, `ctrl-c` | Tatsächliche PTY-Größe mit explizitem Host-SIGWINCH sowie überlebende Shell nach Unterbrechung. Ctrl-C allein beweist nicht sämtliche Prozessgruppen-Semantik. |
| `shell-exit`, `exit-receipt` | GNU-Exitcode 7 erreicht die CLI. Kein Benutzer-Logout. |
| `scan` | Suche der kompletten generierten Passwörter in den lokalen Bootlogs. Dies ist kein umfassender Informationsflussbeweis. |

`gnu-checked BEFEHL` führt eine zusätzliche Prüfung im angemeldeten GNU-Kontext
aus und verlangt einen eindeutigen tatsächlich ausgegebenen Exitcode 0.
`gnu BEFEHL` ist nur eine Diagnose ohne diese Erfolgsbehauptung. Eingaben und
synthetische Dateiproben werden protokolliert; keine fremden Secrets verwenden.

## Paketaktionen mit frischer Adminprüfung

Der Treiber kann einen echten Paketplan bis zur Adminabfrage öffnen und dort
anhalten: `package-install-user hello=2.10-5`, `package-install-all ed`,
`package-remove-user hello`, `package-remove-all ed` sowie
`package-update-user` und `package-update-all`. Bereich und Version werden
unverändert an die Produkt-CLI weitergereicht; der Treiber führt kein APT aus.

Den angezeigten Plan prüfen, danach genau eine Fortsetzung senden:
`package-approve` verwendet Alphas aktuelles AOSP-Adminpasswort;
`package-wrong` das falsche Testpasswort; `package-nonadmin` Betas korrektes
aktuelles Passwort. Nach `passwd-b` ist das dessen neues Passwort.
`package-cancel-plan` sendet eine leere Adminauswahl. Während der Abfrage sind
andere Aktionssteuerungen gesperrt. `close` beendet den Testkanal; ob eine
angefangene Transaktion dabei tatsächlich abgebrochen wurde, muss gesondert
nachgewiesen werden. `package-status` und `package-cancel` rufen die
entsprechenden CLI-Befehle auf. `package-unauthenticated` erwartet die
Ablehnung einer persönlichen Installation aus einem unangemeldeten Kanal.
Der Host wartet auf Paketplanung und Abschluss jeweils höchstens zehn Minuten,
weil ARM unter TCG mehrere Minuten benötigen kann. Diese Wartezeit verändert
weder die Gast-Autorisierung noch deren Fristen.

`cli BEFEHL` ist für gezielte CLI-Prüfungen ohne weitere interaktive Rückfrage
vorgesehen, beispielsweise die Ablehnung einer nicht verfügbaren Version.
Die aufgezeichnete Antwort allein ist kein Erfolgssignal. Paketgenerationen,
Antragsteller/Seriennummer, unveränderte fremde Speicher und ausgeführte
Programme müssen jeweils unabhängig geprüft werden. Eine Adminfreigabe darf
die persönliche Installation nicht auf das Administratorkonto umleiten.

Die Paketsteuerung ist aus dem realen Zwei-Benutzer-Durchlauf mit Image
`020ae750` übernommen. Ihre wiederverwendbare Integration wurde am 1. Oktober
im Vollimage `f2d1d0e7` für gemeinsame und persönliche Installation, falsche
und Nicht-Adminfreigabe sowie Aktivierungsstatus ausgeführt. Update, Entfernung
und die vollständige Konkurrenz-/Abbruchmatrix sind damit nicht abgenommen.

## Neustart und Belege

### Verwaltete CLI-Löschung

Diese Steuerungen benötigen ein Image mit freigegebener verwalteter
CLI-Löschung. Alle vier Ablehnungen und die erlaubte Löschung bei zwei
laufenden GNU-Jobs sind im Vollimage `f2d1d0e7` über die echte AEGIS-CLI
bestanden; siehe [Produktbelege](component-tests.md). Sie verwenden ausschließlich
Alpha/Beta aus demselben frischen Testlauf. Eine neue Identität mit anderer
Seriennummer wird von diesem Treiber ausdrücklich nicht stillschweigend übernommen.

* `remove-denied-unauthenticated`: neuer, noch nicht angemeldeter CLI-Kanal;
  selbst das korrekte Adminpasswort darf keine Sitzung ersetzen.
* `remove-denied-nonadmin`: als Beta darf dessen korrektes Passwort keine
  Löschung von Alpha autorisieren.
* `remove-denied-self`: Alpha darf sich aus seiner eigenen Sitzung nicht löschen.
* `remove-denied-wrong-password`: Alpha mit falscher frischer Bestätigung darf
  Beta nicht löschen.

Jede Ablehnung verlangt die passende tatsächliche Fehlermeldung und dieselben
AOSP-Identitäten, gestarteten Benutzer, CE-Zustände, Schlüsselverzeichnisnamen
und Runtime-Kontexte davor/danach. Schlüsselinhalt wird nicht gelesen.
Die ursprünglichen Hintergrundprozesse separat mit `bg-observe-a/b` prüfen.

Nach einem bereits belegten Passwortwechsel verwendet der Nicht-Admin-Test
das neue Passwort. Für Löschprüfungen nach dem Persistenz-Neustart erzeugt
`gnu-bg-renew-a/b` neue, begrenzte Hintergrundproben: Der alte Prozess muss
nachweislich weg sein, bei gewechselter Boot-ID muss der ursprüngliche
Neustart-Checkpoint vorliegen, und die GNU-Datei wird zuvor bytegleich gelesen.
Die alten Prozessbelege bleiben erhalten.

Nach diesen vier Prüfungen entfernt `remove-beta` den noch laufenden Beta über
die echte CLI mit frischer Adminbestätigung. Beide ursprünglichen Jobs müssen
vorher existieren. Danach müssen Beta, dessen Schlüsselverzeichnisse, sämtliche
geprüften Daten-/XML-Pfade sowie Einträge in allen vorhandenen Benutzerlisten
fehlen; AOSPs Abschlussmeldung muss zur ursprünglichen ID/Seriennummer passen.
Betas Originalprozess muss beendet sein und Alphas weiterlaufen. Anschließend
Alphas GNU-Datei bytegleich lesen und Alpha regulär abmelden. Der Durchlauf ist
kein Ersatz für einen separaten Zwei-Benutzer-Persistenztest, da Beta gelöscht
wurde.

Vor einem Neustart beide Benutzer mit der CLI vollständig abmelden, tatsächliche
CE-Sperre und Ressourcenabbau bestätigen und `close` ausführen. Erst dann kann
`reboot-checkpoint` den Zustand festhalten. Er verlangt zwei tatsächlich
GNU-geschriebene Dateien und beide zuvor beobachteten, jetzt beendeten
Hintergrundprozesse. Der Treiber selbst löst keinen Neustart aus.

Das **gleiche** Android-/KeyMint-Paar wird über den normalen QEMU-Ablauf
geordnet gestoppt und wieder gestartet. Den Treiber dabei weiterlaufen lassen,
damit die Passwörter im Speicher bleiben. Neue Boot-ID, unveränderte Profil-ID,
AVB, SELinux und zunächst gesperrtes persönliches CE unabhängig prüfen. Danach
über `open` und eigene Anmeldung beide `gnu-read-*` sowie gegebenenfalls
`home-retained-a` aus tatsächlichen GNU-Shells wiederholen. Ein Root-Readback
ersetzt diese Schritte nicht. Abschließend erneut abmelden und den Abbau prüfen.

`inputs.json` bindet Lauf, Profil, Image-Commit und Treiber-SHA. `events.json`
enthält tatsächliche Aktionen und Ausgaben; `reboot-checkpoint.json` dokumentiert
nur seinen ausdrücklich begrenzten Zustand. Es gibt kein automatisches
pauschales `PASS`: Fehler, ausgelassene Schritte und Grenzen müssen im
Prüfbericht erhalten bleiben. Frühere Abnahmen und Prüfsummen stehen im
[GNU-Testbericht](runtime-gnu-qemu-test.md).

Der wiederverwendbare Treiber ist aus den tatsächlich ausgeführten lokalen
Treibern abgeleitet. Seine Parametrisierung, beide `direct-first-*`,
Benutzerwechsel, Bildschirmsperre, widerrufene Passwortabfrage, Passwortwechsel,
Logout und Dateierhalt nach Reboot wurden am 29. September im vollständigen
`2f29f0ac` ausgeführt. Auch die Verweigerung eines Starts auf dem inzwischen
mit persönlichen Benutzern belegten Profil ist geprüft. Zeiten, Grenzen und
Treiber-SHA stehen im [GNU-Testbericht](runtime-gnu-qemu-test.md).
Pakettransaktionen, sichere Benutzerlöschung und umfassende IPC-/Systemaufruf-
Angriffe werden dadurch nicht implementiert oder als bestanden erklärt.
# Linux-Server und ausstehender CE-Schlüsselentzug (1. Oktober 2026)

Der Treiber unterstützt jetzt auch lokale Linux-QEMU-Gäste. Images können aus
einem lokalen Buildrun stammen; AVB-Receipt, voller Image-Commit und die
Prüfsumme der GPT-Basis müssen zum gepaarten Profil passen.

Nach `gnu-write-a` und `shell-exit` startet `held-ce-start-a` einen begrenzten
Entwickler-root-Prozess, der genau diese CE-Datei offen hält. `logout` darf bei
überschrittener Sperrfrist keinen Erfolg melden. `pending-login-a` prüft vor
Freigabe die Ablehnung ohne Passwortabfrage, ohne Runtime und mit demselben
Systemserver-Prozess. `held-ce-release-a` schließt den FD bestätigt; anschließend
sind falsches Passwort, frische korrekte Anmeldung, `gnu-read-a` und reguläre
Abmeldung getrennt zu prüfen. Entsprechende `-b`-Befehle existieren für Beta.
Der Halteprozess endet auch bei verschwundenem Treiber spätestens nach seiner
begrenzten Schleife. Dies ist kontrollierte Fehlerauslösung mit Entwickler-root,
kein Nachweis unprivilegierter Isolation.
