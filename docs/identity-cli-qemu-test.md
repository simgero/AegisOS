# Zwei persönliche Benutzer über die AEGIS-CLI

Am 28. September 2026 wurde die **installierte AEGIS-CLI mit dem echten
AOSP-Identitätsdienst** in lokalem Mac-QEMU geprüft. Benutzeranlage, Anmeldung,
Benutzerwechsel, Passwortwechsel, bestätigte Abmeldung und ein vollständiger
geordneter Neustart funktionieren für zwei persönliche Benutzer. Die
GNU/Linux-Runtime bleibt ausdrücklich nicht installiert (`runtime=not-installed`).

## Geprüfter Stand

- Produktcommit `25fde9955adfada99530382491a2a2d272b15eed`,
  [verifizierter Vollbuild](https://github.com/simgero/AegisOS/releases/tag/aosp-20260928T193617Z-25fde995-aecfe99a).
- Helper `secure-env-20260928T134804Z-024354c1`, Persistenzprotokoll
  `persistent-state-v1`.
- Separates Profil `out/qemu-profiles/identity-25fde995`, UUID
  `5469b354-c9cf-4ac1-95bd-ad7941059678`; Läufe
  `out/identity-cli-25fde995/boot-1` und `boot-2`, ADB-Port 15855.
- Beide Läufe vollständig gebootet, SELinux `Enforcing`, `ro.adb.secure=1`,
  AEGIS-Binder-Dienst registriert. Das sichtbare Profil auf Port 15755 wurde
  für diese Tests nicht verändert.
- Android-Overlay und Helper-Disk wurden gemeinsam erhalten. Der zweite Lauf
  verwendete dasselbe Profil ohne `--create-profile` und ohne erneute
  Schlüsselprovisionierung. Beide Läufe endeten mit Android `Power down` und
  `AEGIS_HELPER_SHUTDOWN_CLEAN`.

## Reale CLI-Prüfung

Der Host-Testprozess bediente `adb shell -tt su 0 /system_ext/bin/aegis` über
ein PTY. Zufällige Testpasswörter blieben in seinen Speicherpuffern. Sie wurden
erst nach der verdeckten Java-Console-Abfrage eingegeben, nicht als
Befehlsargumente, Umgebungsvariablen oder Credential-Dateien. Die Aufzeichnung
enthält nur Befehle, Antworten, IDs und Prüfsummen. Die vollständigen
Testpasswörter wurden in keinem der sechs Android-, Helper- und
Logcat-Protokolle gefunden. Nach Testende wurden die eigenen Credential-Puffer
überschrieben; dies ist kein Nachweis aller möglichen Speicher- oder Logkopien.

| Vorgang | Beobachtung |
| --- | --- |
| `setup "Aegis Test Alpha"` | Persönlicher Administrator 10, Seriennummer 10; aktiv, gestoppt, CE gesperrt. Anschließend getrennte Anmeldung erforderlich. |
| Falsches Alpha-Passwort | AOSP lehnt ab; Benutzer `RUNNING_LOCKED`. |
| Richtiges Alpha-Passwort | Benutzer 10 im Vordergrund, laufend, CE entsperrt. |
| `user add "Aegis Test Beta"` | Frische Alpha-Adminpasswortabfrage; persönlicher Benutzer 11, Seriennummer 11, ohne Adminrecht; gestoppt und CE gesperrt. |
| Falsches Beta-Passwort | Abgewiesen; Benutzer 11 bleibt `RUNNING_LOCKED`, die bestehende Terminalbindung bleibt Alpha. |
| `switch "Aegis Test Beta"` | Ziel frisch authentifiziert, Benutzer 11 im Vordergrund und entsperrt. |
| `passwd` | Altes Passwort geprüft, neues über AOSP gesetzt; vorhandene Testdatei unverändert. |
| `logout` | Benutzer beendet und CE gesperrt; ursprünglicher Klartext-Dateipfad nicht lesbar. |
| Altes Beta-Passwort | Vor und nach dem Neustart abgewiesen. |
| Neues Beta-Passwort | Vor und nach dem Neustart akzeptiert; unveränderte Dateibytes. |
| Neustart | IDs, Seriennummern und Rollen beider Benutzer identisch. Vor Anmeldung CE-Liste nur `[0]`; persönliche Dateipfade nicht lesbar. |
| Erneute Alpha-Anmeldung | Falsches Passwort abgewiesen; richtiges akzeptiert und unveränderte Dateibytes gelesen. |
| Abschluss | Beide persönlichen Benutzer abgemeldet, CE-Liste `[0]`, beide Dateipfade unzugänglich; Test-VMs sauber beendet. |

Für jeden Benutzer wurden 4096 zufällige Bytes in
`/data/misc_ce/ID/aegis-cli-smoke/probe.bin` geschrieben und vor/nach dem
Neustart bytegenau verglichen:

- Alpha: `5d8bfeb72efa5b442b136d3bc7dd7a97404e5549def7762e990cf53091e3e2c5`.
- Beta: `97d5bf9838bf0037359503da3a01f1e48b58bf0ed96f298289956127a8470705`.

Die Leseprüfungen verwendeten autorisierten **Entwicklungs-root**, keine
Linux-Runtime-Benutzer. Gesperrte Klartextpfade lieferten Exitcode 1, null
Ausgabebytes und `ENOENT`, während das CE-Elternverzeichnis vorhanden blieb.
Das wurde mit AOSPs CE-Liste und dem anschließenden erfolgreichen Lesen
derselben Bytes abgeglichen. Es wurde kein unabhängiger Kernel-ioctl für den
fscrypt-Schlüsselstatus durchgeführt. Aus diesen Prüfungen folgt insbesondere
kein Schutz gegen einen kompromittierten privilegierten AOSP-Dienst oder den
Mac-Eigentümer.

## Korrekturen am Testablauf

Ein erster Versuch mit 22 Zeichen wurde noch vor Reservierung/Benutzeranlage
abgewiesen. Der gepinnte AOSP-Quellstand definiert
`DevicePolicyManager.MAX_PASSWORD_LENGTH = 16`; der erfolgreiche Test verwendete
16 Zeichen. Das interne Transportlimit von 128 Zeichen hebt AOSPs Vorgabe für
neue Passwörter nicht auf. Die generische CLI-Fehlermeldung erklärt diese
Ursache bislang nicht.

Der erste Sperrtest erwartete fälschlich `ENOKEY`. Tatsächlich liefert die
Klartextnamenssuche auf diesem gesperrten Dateisystem `ENOENT`, wie schon beim
früheren Plattformtest. Dieser ursprüngliche Assert schlug fehl. Die oben
dokumentierte Kombination aus AOSP-Speicherzustand, verweigertem Lesezugriff
und anschließend identischen Bytes ist der tatsächlich erbrachte Nachweis.

Nach dem Neustart trat einmal ein Bestätigungs-Timeout der seriellen Konsole
auf. Danach scheiterte die erste ADB-Authentifizierung, die nächste frische
Verbindung funktionierte mit dem erhaltenen Mac-Schlüssel. Deshalb erlaubt
`connect-local-adb.py` nun auch beim Neustart einen zweiten begrenzten
Authentifizierungsversuch. Ein fehlender Schlüssel wird dadurch nicht
hinzugefügt, und Erfolg verlangt weiterhin `get-state=device` und
`ro.adb.secure=1`. Der einzelne Konsolen-Timeout bleibt separat offen.

## Nachweise und Grenzen

Lokale Nachweise: `out/identity-cli-25fde995/result.json`, `events.json`,
`before-reboot-locked.json`, `after-reboot-locked.json`,
`after-reboot-wrong-alpha.json`, `after-reboot-old-beta.json`, `final-locked.json`
und die sechs Laufprotokolle. `result.json` hält die SHA-256-Werte dieser
Evidenzdateien fest. Testkonten und Testprofil bleiben als getrenntes Artefakt
erhalten; die zufälligen Passwörter wurden nicht gespeichert.

Noch nicht geprüft: echte unterbrochene Ersteinrichtung, CLI-Benutzerlöschung,
ID-Wiederverwendung, vollständige Negativtests der Adminberechtigungen,
Stromausfall, Image-Migration, Linux-Ausführung/Isolation und Pakettransaktionen.
Der in den längeren Logs beobachtete Telefoniedienst wartet wiederholt auf
`IRadioModem/slot1`, erreicht einen Start-ANR und wird neu gestartet. Die
frühere leere Crash-Puffer-Momentaufnahme belegt deshalb keine dauerhafte
Gerätestabilität. Dieser Produktfehler bleibt zu korrigieren.
