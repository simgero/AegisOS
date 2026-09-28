# AOSP-Anbindung für AEGIS-Identität

Stand 28. September 2026: **Bibliothek, Systemdienst und erste interaktive CLI
sind auf `aegis-build` gebaut und im Image `25fde995` in lokalem QEMU installiert.**
Der [reale Zwei-Benutzer-Test](../../../docs/identity-cli-qemu-test.md) bestätigt
Anlage, Anmeldung, Passwortwechsel, Abmeldung und geordneten Neustart mit
unveränderten CE-Dateien. Linux-Runtime und Paketverwaltung sind nicht aktiviert.
52 Java- und 83 von 86 nativen Tests bestehen; die drei Basis-Mount-Fehler stehen
im [Komponentenbericht](../../../docs/component-tests.md).
Sitzungsmodell und offene Integrationsschritte stehen in
[`docs/identity-cli.md`](../../../docs/identity-cli.md).

Die folgenden Bausteinbeschreibungen dokumentieren ihren ursprünglichen
Entwurfsstand. Angaben zur damaligen fehlenden Kompilierung werden durch den
aktuellen Komponentenbericht oben ersetzt; sie sind keine aktuelle Statusliste.

Unter `runtime/` liegt zusätzlich der noch unkompilierte native
[Prozessaufseher](../../../runtime/process-supervisor.md) mit privaten
Kontrollkanälen, PTYs und Gerätetests. Er ist nur ein explizites Buildziel;
Produktaktivierung, Broker und SELinux-Übergänge fehlen noch. Er verändert
weder die AOSP-Authentifizierung noch den weiterhin runtimefreien Dienstmodus.

Die interne Bibliothek `libaegis-runtime-child` ergänzt die Beobachtung und das
gezielte Beenden eines Kindprozesses über Pidfd samt zehn weiteren Gerätetests.
Auch sie ist unkompiliert und nicht aktiviert. Bestätigtes Prozessende ersetzt
weder Ressourcenabbau noch AOSP-Speichersperrung; die konkrete Runtime-Anbindung
fehlt weiterhin.

`libaegis-runtime-namespace` ergänzt den
[zweistufigen Start persönlicher Namespaces](../../../runtime/namespace-launch.md):
zunächst pausiertes Kind mit Pidfd, danach geprüfte UID/GID-Maps und Freigabe
eines vertrauenswürdigen Setup-Helfers. Neun weitere Gerätetests sind vorbereitet.
Bibliothek und Probe-Helfer sind unkompiliert und nicht im Produkt aktiviert;
Broker und dessen AOSP-Autorisierung fehlen weiterhin. Der inzwischen ebenfalls
vorbereitete Mount-Helfer ist unten beschrieben.

Zusätzlich ist eine [persönliche Basis-Sicht](../../../runtime/base-mounts.md)
als detached ID-mapped Mount vorbereitet. Die Maps lassen sich dafür ohne
Exec-Freigabe einrichten; die schreibgeschützte Quelle wird nicht umgeschrieben.
Vier weitere native Tests sind vorbereitet. Auch diese Mount-Erweiterung ist
noch nicht kompiliert oder im Gast ausgeführt; CE-Einbindung und Rootwechsel
fehlen weiterhin.

Die [persönliche CE-Speicheranbindung](../../../runtime/personal-storage.md)
prüft jetzt im Quelltext AOSPs Seriennummer und Verschlüsselungsrichtlinie,
bereitet einen privaten HOME-Bestand vor und erzeugt eine passende detached
Mount-Sicht. Namespace-Handles tragen ID und Seriennummer unveränderlich.
Fünf zusätzliche Negativtests sind vorbereitet. Alles ist noch unkompiliert;
erfolgreiche CE-Anlage, Lebenszyklus-Koordination und Gastnachweise fehlen.

Ein [privates Geräteverzeichnis](../../../runtime/private-devices.md) ergänzt
jetzt sechs feste Zeichengeräte und Platzhalter für persönliche Terminal-/IPC-
Mounts. Es wird nur detached im Broker vorbereitet; kein Host-Gerätebaum wird
eingehängt. Sitzung und Prozessgruppe werden vor dem Setup-Exec getrennt.
Zwei weitere native Tests sind vorbereitet. Kompilierung, SELinux und der
vollständige Runtime-Start fehlen weiterhin.

Der [statische Starthelfer](../../../runtime/namespace-setup.md) übernimmt nun
im Quelltext die vorbereiteten Mounts, richtet private Prozess-/Terminal-/IPC-
und temporäre Dateisysteme ein und entfernt vor dem Aufseher-Exec die alte
Android-Wurzel. Ein festes privates Protokoll bindet alle Referenzen an ID und
Seriennummer. Acht zusätzliche Gerätetests sind vorbereitet. **Unkompiliert,
nicht aktiviert und nicht im Gast ausgeführt.** Der reale Broker, Ressourcen-
und SELinux-Anbindung sowie vollständige Runtime-/Logout-Nachweise fehlen.

`AospIdentityBackend` ist für den späteren AEGIS-Systemdienst vorgesehen.
Der Konstruktor verlangt den Android-Systemprozess-UID. Die Klasse besitzt
keinen Binder-Endpunkt und keine eigene Benutzer-, Passwort- oder Schlüsseldatenbank.

Implementiert im Quelltext:

- Persönliche AOSP-Vollbenutzer auflisten und Namen eindeutig auflösen. Der
  Systembenutzer, Profile, Gäste und vorab erzeugte Konten sind ausgeschlossen.
  Deaktivierte und unvollständige Konten sind zur Diagnose sichtbar; Anmeldung
  ist nur für aktivierte, vollständig angelegte Benutzer zulässig.
- Jede Zuordnung mit AOSP-`userId` **und** Seriennummer prüfen. Die Kennung
  selbst erteilt keine Berechtigung.
- Passwort über `ILockSettings.verifyCredential(..., flags=0)` prüfen und auf
  AOSP-Benutzerentsperrung sowie CE-Entsperrung warten. Auch ein bereits
  entsperrter Benutzer wird erneut authentifiziert. AOSPs Sperrfrist bleibt erhalten.
- Optional erst nach erfolgreicher Authentifizierung in den Vordergrund wechseln.
- Passwortwechsel über AOSP einschließlich dessen Passwortregeln und Historie.
  Eigene Credential-Puffer und der kurzlebige AOSP-Historienfaktor werden
  anschließend gelöscht. Eine Parcel-Kopie hält die Eigentümerschaft für
  verzögerte Arbeit in LockSettings getrennt. CE-Schlüssel werden nicht
  abgefragt/exportiert.
- Einmalige Ersteinrichtung aus der autorisierten Entwicklungs-Rootkonsole,
  anschließend Anlage/Löschung über einen frisch authentifizierten persönlichen
  AOSP-Administrator. Neue Konten werden erst nach Passwortsetzung und CE-Sperre
  aktiviert. Ein fehlgeschlagener Vorgang wird nicht automatisch wiederholt.
- Benutzerlöschung bestätigt AOSP-Benutzerende, CE-Sperre und Metadatenabwesenheit
  und fordert anschließend AOSPs vold-Bereinigung für Schlüssel und interne Daten
  an. Private Zusatzvolumes werden bis zur entsprechenden Integration abgelehnt.
- Den Android-Teil einer Abmeldung durchführen: zum Systembenutzer zurückkehren,
  `stopUserWithCallback` ohne verzögertes Sperren aufrufen und anschließend
  **sowohl** den beendeten Benutzer **als auch** den gesperrten CE-Speicher prüfen.

## Verpflichtender Rahmen vor der Verwendung

Die Bibliothek allein ist kein Aufrufdienst. Der neue Dienstentwurf prüft vor
jedem Aufruf UID, PID, Kernel-Prozessstartzeit und Binder-Lebenszeichen des
ursprünglichen Clients. Diese Durchsetzung ist noch im Gast zu testen.
AOSPs Vordergrundbenutzer ist keine
Autorisierung für einen Hintergrundprozess. Status- und Benutzerlisten müssen
entsprechend eingeschränkt werden. Administrative Aktionen brauchen eine frische
AOSP-Authentifizierung und eine aktuelle AOSP-Adminprüfung für genau diese Aktion.

Die CLI muss Passwörter am Terminal ohne Echo einlesen und außerhalb von
Befehlsargumenten, Umgebungsvariablen, Shell-History und Logausgaben zum Dienst
übergeben. Der Dienst darf Kennwörter nicht in Statusobjekte oder Fehlermeldungen
aufnehmen. IPC-Abbruch, Zeitüberschreitungen und alle Fehlerpfade benötigen Tests.

Vor `stopAndroidUserAndLock` muss der Koordinator die Runtime-Prozesse und
Pakettransaktionen stoppen, auf Prozessende warten und persönliche Mounts/IPC
entfernen. Neue Starts bleiben währenddessen gesperrt. Ein Fehler oder Timeout
darf nicht als vollständiger Logout gemeldet werden. Ein späterer AOSP-Callback
kann trotz eines Timeouts noch eintreffen; dann muss der tatsächliche Zustand
erneut ermittelt werden. Die Systemdienst-Lifecycle-Callbacks dürfen nicht auf
blockierende Adapteroperationen warten, sondern müssen Arbeit einreihen.

Die Produktkonfiguration bindet die Module nun in den Systemserver-Classpath,
die Framework-Startliste und eine eigene SELinux-Service-Zuordnung ein. Diese
Integration ist im aktuellen Image gebaut und im Gast geprüft. Die ausdrückliche
Fortsetzung einer protokollierten Ersteinrichtung über `setup --resume NAME`
ist jetzt im Quelltext vorbereitet: vorhandene Passwörter werden über AOSP
verifiziert, niemals zurückgesetzt; Benutzerstopp und CE-Sperre müssen vor dem
Abschluss bestätigt sein. Zwölf Gerätetests mit simulierter Plattform bestehen;
ein echter Unterbrechungstest steht aus. Unklar zugeordnete oder partielle Konten werden
nicht automatisch übernommen oder gelöscht.
Offen sind weiterhin Runtime-Koordination, Paketverwaltung und die im
CLI-Gasttest ausdrücklich aufgeführten Verwaltungs-/Fehlerfälle.

## Abgeglichene AOSP-Schnittstellen

Alle folgenden Quellen gehören zu `android-16.0.0_r1`:

- [LockSettingsService](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/locksettings/LockSettingsService.java):
  Die erfolgreiche Credential-Prüfung führt durch AOSP zum Entsperren des
  Keystores, CE-Speichers und Benutzers. Der Adapter fordert keine Passwort-Handles an.
- [ActivityManagerService](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/am/ActivityManagerService.java)
  und [UserController](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/am/UserController.java):
  Der Stop-Callback kann vor dem asynchronen CE-Schlüsselentzug erfolgen.
- [IStorageManager](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/core/java/android/os/storage/IStorageManager.aidl):
  Die direkte Binder-Abfrage liefert den tatsächlichen CE-Entsperrstatus. Ein
  fehlender Dienst oder Kommunikationsfehler führt im Adapter zu einem Fehler.
- [PasswordMetrics](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/core/java/android/app/admin/PasswordMetrics.java)
  und [LockPatternUtils](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/core/java/com/android/internal/widget/LockPatternUtils.java):
  Validierung anhand der AOSP-Vorgaben einschließlich administrativer Regeln.

Ein erfolgreicher Test mit den eingebauten Android-Dialogen bestätigt die
Plattformgrundlage. Er ersetzt weder das Kompilieren dieser Bibliothek noch
End-to-End-Tests der zukünftigen AEGIS-CLI.
