# AOSP-Speicheroperationen und Runtime-Abbau

Stand 28. September 2026: **Quelltext vorbereitet; nicht auf dem Builder
kompiliert und nicht in QEMU ausgeführt.** Die Schnittstelle besitzt noch keinen
Runtime-Controller. `ro.aegis.runtime.mode=absent` bleibt gesetzt; diese Änderung
aktiviert weder Linux-Kontexte noch Paketoperationen.

## Warum eine vorgeschaltete Sperre erforderlich ist

Im festgelegten AOSP-Stand `android-16.0.0_r1` kann die Benachrichtigung über
gesperrten CE-Speicher erst nach dem Schlüsselentzug erfolgen.
`onUserStopping` allein bestätigt ebenfalls keinen vollständigen Ressourcenabbau.
Persönliche Linux-Prozesse, offene Dateien und Mountreferenzen müssen daher
bereits vor der eigentlichen Speicheroperation beendet beziehungsweise
geschlossen sein. AOSP bleibt allein für Passwörter und Schlüssel zuständig.

Der vorbereitete Integrator ergänzt `StorageManagerService` an fünf Stellen:

| Operation | Vertrag des noch fehlenden Controllers |
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

Acht zusätzliche Android-Java-Tests sind vorbereitet: Reihenfolge,
fehlgeschlagene Akquisition, Modusprüfung, Threadbindung, einmalige Freigabe
und Fehlerweitergabe. Zusammen mit der [Zugangsserialisierung](admission.md)
sind 44 Java- und 50 native Gerätetests vorbereitet; **die neuen Gerätetests sind weiterhin unkompiliert und unausgeführt**.

Es fehlen weiterhin der tatsächliche Controller/Broker, die gemeinsame
Serialisierung von Start/Stop und Pakettransaktionen, Ressourcenabbau auch bei
externem AOSP-Benutzerstopp, SELinux-Regeln und echte Parallelitätstests.
Direkte privilegierte vold-Aufrufe durchlaufen diese Java-Schnittstelle nicht;
insbesondere die direkte Löschbereinigung des vorhandenen AOSP-Adapters muss
vor Aktivierung des verwalteten Modus ebenfalls in die Koordination einbezogen
werden. Die bestehende Beschränkung `requireRuntimeAbsent()` bleibt bestehen.
