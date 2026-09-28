# AOSP-Anbindung für AEGIS-Identität

Stand 28. September 2026: **Interne Bibliothek in Entwicklung. Noch nicht auf
`aegis-build` kompiliert, nicht installiert und keine fertige `aegis`-CLI.**
Die laufende QEMU-VM enthält diesen Code noch nicht.

`AospIdentityBackend` ist für den späteren AEGIS-Systemdienst vorgesehen.
Der Konstruktor verlangt den Android-Systemprozess-UID. Die Klasse besitzt
keinen Binder-Endpunkt und keine eigene Benutzer-, Passwort- oder Schlüsseldatenbank.

Implementiert im Quelltext:

- Persönliche AOSP-Vollbenutzer auflisten und Namen eindeutig auflösen. Der
  Systembenutzer, Profile, Gäste und unvollständige Benutzer sind ausgeschlossen.
- Jede Zuordnung mit AOSP-`userId` **und** Seriennummer prüfen. Die Kennung
  selbst erteilt keine Berechtigung.
- Passwort über `ILockSettings.verifyCredential(..., flags=0)` prüfen und auf
  AOSP-Benutzerentsperrung sowie CE-Entsperrung warten. Auch ein bereits
  entsperrter Benutzer wird erneut authentifiziert. AOSPs Sperrfrist bleibt erhalten.
- Optional erst nach erfolgreicher Authentifizierung in den Vordergrund wechseln.
- Passwortwechsel über AOSP einschließlich dessen Passwortregeln und Historie.
  Alle übergebenen Credential-Puffer und der kurzlebige AOSP-Historienfaktor
  werden anschließend gelöscht. CE-Schlüssel werden nicht abgefragt/exportiert.
- Den Android-Teil einer Abmeldung durchführen: zum Systembenutzer zurückkehren,
  `stopUserWithCallback` ohne verzögertes Sperren aufrufen und anschließend
  **sowohl** den beendeten Benutzer **als auch** den gesperrten CE-Speicher prüfen.

## Verpflichtender Rahmen vor der Verwendung

Die Bibliothek ist noch kein sicherer Aufrufdienst. Der zu implementierende
Dienst muss vor jedem Aufruf den tatsächlichen Client prüfen und dessen Sitzung
an den ursprünglichen Aufrufer binden. AOSPs Vordergrundbenutzer ist keine
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

Noch offen sind insbesondere Benutzereinrichtung/-löschung mit Adminfreigabe,
einmalige abgesicherte Ersteinrichtung, Client-/Sitzungsbindung, CLI, Binder-/
SELinux-Integration, Runtime-Koordination sowie Build und Gasttests dieser Klasse.
Das Modul wird absichtlich noch nicht in ein Produkt aufgenommen.

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
