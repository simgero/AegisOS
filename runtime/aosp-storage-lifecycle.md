# AOSP-Speicheroperationen und Runtime-Abbau

Stand 29. September 2026: Die fünf AOSP-Hooks sind im gebooteten Image vorhanden;
ihre Gerätetests bestehen im Komponentenstand `ac87df1f`. Der aktuelle Quelltext
verbindet im Modus `managed-v1` den echten nativen Besitzer, `RuntimeAdmission`
und den Identitätsdienst. Diese neue Verbindung ist noch nicht kompiliert oder
praktisch geprüft. Das Produkt bleibt `ro.aegis.runtime.mode=absent`, bis auch
SELinux, Init und die erforderlichen Systemtests integriert sind. Es sind keine
öffentlichen PTY- oder Paketoperationen freigegeben.

## Warum eine vorgeschaltete Sperre erforderlich ist

Im festgelegten AOSP-Stand `android-16.0.0_r1` kann die Benachrichtigung über
gesperrten CE-Speicher erst nach dem Schlüsselentzug erfolgen.
`onUserStopping` allein bestätigt ebenfalls keinen vollständigen Ressourcenabbau.
Persönliche Linux-Prozesse, offene Dateien und Mountreferenzen müssen daher
bereits vor der eigentlichen Speicheroperation beendet beziehungsweise
geschlossen sein. AOSP bleibt allein für Passwörter und Schlüssel zuständig.

Der vorbereitete Integrator ergänzt `StorageManagerService` an fünf Stellen:

| Operation | Vertrag des Controllers |
| --- | --- |
| Schlüssel erzeugen, löschen oder CE sperren | Neue Runtime-/Paketaktionen für alle Seriennummern der numerischen Benutzer-ID sperren; laufende Transaktionen abschließen oder abbrechen; Prozesse beenden und einsammeln; sämtliche CE-, Mount-, Terminal- und Socketreferenzen schließen. Erst danach darf die AOSP-Operation beginnen. |
| CE entsperren oder Passwortschutz aktualisieren | Gegen konkurrierenden Schlüsselentzug serialisieren, ohne einen gültigen laufenden Kontext allein deshalb zu beenden. |

Die erworbene Sperre umfasst den ursprünglichen vold-Aufruf und AOSPs
Statusaktualisierung. Auch ein bereits als gesperrt gemeldeter CE-Speicher
darf im verwalteten Modus den vorgeschalteten Abbau nicht überspringen.
Fehlender Controller, unbekannter Modus oder fehlende Abbaubestätigung führen
zu einem Fehler. Der dauerhaft erforderliche Systembenutzer erhält keine
persönliche Runtime und bleibt von diesem Controller ausgenommen.

Das Freigeben der Sperre erlaubt **keinen** neuen Runtime-Start. Nach einem
Abbau muss eine spätere frisch autorisierte AOSP-Prüfung Benutzer-ID,
Seriennummer und tatsächlichen CE-Status erneut abgleichen. Ein Fehler lässt
die Zulassung gesperrt. Alle Wartezeiten müssen begrenzt sein. Der Controller
darf weder den Operationsmonitor des Identitätsdienstes erwerben noch in
Storage/LockSettings zurückrufen: Diese Sperren können beim Aufruf bereits
gehalten werden.

## Fehler bleiben sichtbar

Die Originalmethoden für Erzeugen, Löschen und Sperren protokollieren bestimmte
vold-Fehler, ohne sie ihrem Aufrufer weiterzugeben. Der Patch ergänzt die
Fehlerweitergabe. `UserController` behandelt fehlgeschlagene Sperrung ohne
Absturz seines Hintergrundthreads und ruft in diesem Fall keine erfolgreiche
`keyEvicted`-Bestätigung auf. Nach normaler Rückkehr prüft er zusätzlich den
von AOSP gemeldeten CE-Status. Das ist kein eigenständiger kryptographischer
Schlüsseltest und ersetzt den späteren Gastnachweis nicht.

## Gepinnte und nachvollziehbare Integration

[`aosp-storage-hooks.json`](aosp-storage-hooks.json) enthält die vollständigen
SHA-256-Werte der beiden Originaldateien aus dem festgelegten AOSP-Tag:
[StorageManagerService](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/StorageManagerService.java)
und [UserController](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-16.0.0_r1/services/core/java/com/android/server/am/UserController.java).

`scripts/aosp/register-runtime-storage.py` prüft die Git-Originale und alle
Zieldateien vor dem ersten Schreiben. Fremde Änderungen und symbolische
Verknüpfungen werden abgewiesen. Ersetzte verwaltete Dateien bleiben unter
`out/aegis-runtime-storage/backups/` erhalten, außerhalb der Java-Quellsuche.
Eine unterbrochene Installation akzeptiert nur exakt bekannte Zwischenstände.

Der Bericht `runtime-storage-source.json` hält Eingaben und erzeugte Quellen
fest. Komponenten- und Vollbuild prüfen nach dem Kompilieren deren Hashes.
Der Komponentenlauf baut dafür auch `services` und sammelt `services.jar`.
Ein Vollbuild liefert den Bericht über GitHub mit; fehlender Bericht oder
veränderte heruntergeladene Bytes verhindern `UPLOAD_VERIFIED`.
Der Bericht bestätigt Quelltextzuordnung, keinen ausgeführten Abmeldevorgang.

## Tests und verbleibende Arbeit

Neun Hosttests prüfen echte Originalmethoden als reine Textfixtures,
Eigentümerschaft, Backups, Wiederholung, Unterbrechungen, Symlinks und
Berichtsprüfung. Zwei weitere Worker-Tests prüfen fehlende beziehungsweise
veränderte Release-Berichte. Sie kompilieren oder starten keinen Android-Code.
Die Quellintegration wurde außerdem mit den vollständigen gepinnten Dateien
in einem temporären Verzeichnis geprüft.

Die acht Android-Java-Tests für Reihenfolge, fehlgeschlagene Akquisition,
Modusprüfung, Threadbindung, einmalige Freigabe und Fehlerweitergabe bestehen
im 59er-Komponentenstand. Sie verwenden kontrollierte Provider; dies beweist
keinen tatsächlichen nativen Ressourcenabbau vor AOSP-Schlüsselentzug.

Die neu angebundene Verwaltung hält keinen Runtime-Gate während AOSP-Aufrufen.
Vor Logout sperrt sie die Zulassung und verlangt nativen Stopp; die anschließende
AOSP-Keyoperation erwirbt selbst nochmals ihre Storage-Lease. Neue Anmeldungen
müssen ihre eigene interne Bindung erhalten. Lifecycle-Callbacks widerrufen
Sitzungen ohne den Identitätsmonitor zu erwerben; beim gleichzeitigen Wechsel
verhindert ein atomarer Vergleich das Löschen der neuen Benutzerbindung.

Weiterhin offen sind produktive SELinux-/Init-Aktivierung, öffentlicher
Terminalbesitz, Pakettransaktionen sowie reale Start/Stop/CE-Rennentests.
Direkte privilegierte vold-Aufrufe durchlaufen die Java-Schnittstelle nicht.
Die Löschbereinigung nach Freigabe einer numerischen AOSP-ID muss in den noch
reservierten AOSP-Lebenszyklus verlegt werden. Für Benutzerlöschung bleibt
`requireRuntimeAbsent()` deshalb bestehen; die übrigen neuen Lifecycle-Pfade
berechtigen noch nicht zur Aktivierung des Produkts.
