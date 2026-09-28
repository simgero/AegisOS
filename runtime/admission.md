# Serialisierung von Runtime-Zugang und CE-Operationen

Stand: **Quelltext vorbereitet, nicht kompiliert oder in QEMU ausgeführt.**
`RuntimeAdmission` ist eine interne Bibliothek des Identitätsdienstes. Der
vorbereitete `RuntimeStorageController` verbindet sie mit der
[AOSP-Speicherschnittstelle](aosp-storage-lifecycle.md). Der aktuelle Dienst
registriert ihn ausdrücklich noch nicht und verlangt weiterhin den Modus
`absent`. Es existiert noch kein ausführbarer Linux-Verwaltungsweg.

## Reihenfolge und Eigentümer

Jede numerische AOSP-Benutzer-ID besitzt eine eigene faire Sperre und einen
Widerrufszähler. Benutzer-ID plus AOSP-Seriennummer bestimmen den aktuellen
Eigentümer. Die Daten liegen nur im Arbeitsspeicher; sie sind keine persönliche
Benutzerdatenbank, Passwortprüfung oder Schlüsselautorität.

Ein künftiger Aufrufer muss unmittelbar vor der echten AOSP-Authentifizierung
einen einmaligen Versuch anlegen. Nach erfolgreicher Authentifizierung und
Prüfung des tatsächlichen Benutzers, seiner Seriennummer und des CE-Status
darf er diesen Versuch einlösen. Abgelehnte Versuche werden verworfen.
Ein Versuch einer anderen Gate-Instanz, ein verbrauchter Versuch oder ein
inzwischen widerrufener Stand darf keinen Zugang erteilen.

Die erste Freigabe, ein zuvor gesperrter Zugang und ein Wechsel der Seriennummer
verlangen zunächst einen bestätigten Ressourcenabbau für **alle** alten
Kontexte derselben numerischen ID. Gleichzeitig werden noch offene alte
Anmeldeversuche entwertet. Andere Benutzer behalten ihre getrennten Sperren.

Eine erworbene Zugangssperre umfasst nur eine begrenzte Vorbereitung beziehungsweise
Übergabe an den Ressourcenbesitzer. Vor Veröffentlichung eines gestarteten
Kontexts muss ihr Stand nochmals geprüft werden. Sie ersetzt keine AOSP-Sitzungs-
oder Adminprüfung. Längere Paketoperationen dürfen die Sperre nicht behalten;
sie müssen beim Ressourcenbesitzer registriert und dort abbrechbar sein.

## Schlüsselentzug und konkurrierende Anmeldung

Bei einer destruktiven Speicheroperation geschieht der erste Widerruf bereits
**vor** dem Warten auf die Benutzersperre. Damit kann ein noch offener Start
seinen alten Stand nicht weiter als gültig ausgeben. Erst unter exklusiver
Sperre darf der Ressourcenbesitzer laufende Paketarbeit beenden, Prozessbäume
einsammeln und sämtliche CE-/Mount-/Terminal-/Socketreferenzen schließen.
Erst nach dessen bestätigter Rückkehr darf AOSP seine Speicheroperation ausführen.

Beim Verlassen der Speicheroperation folgt ein zweiter Widerruf, auch bei
einem AOSP-Fehler. Dieser zweite Schritt ist notwendig: Eine Anmeldung könnte
sonst erst während des Wartens auf die Sperre begonnen haben und ihren alten
CE-Nachweis nach dem späteren Schlüsselentzug wiederverwenden. Sie muss nun
mit einer neuen AOSP-Prüfung beginnen. Das Freigeben der Sperre öffnet keinen
Runtime-Zugang.

Entsperren und Aktualisieren des Passwortschutzes verwenden dieselbe
Serialisierung, ohne allein deswegen einen gültigen Kontext zu beenden.
Ein Timeout, unterbrochenes Warten oder ein Fehler beim Ressourcenabbau
bestätigt keine abgeschlossene destruktive Operation und öffnet keinen Zugang.
Ein unterbrochener Thread behält seinen Unterbrechungsstatus. Sperr-Handles
dürfen nur einmal und vom erwerbenden Thread geschlossen werden; verschachtelter
Zugriff auf weitere Benutzersperren derselben Gate-Instanz wird abgewiesen,
um umgekehrte Sperrreihenfolgen zu verhindern.

Die Warte- und Abbaufrist beträgt höchstens zehn Sekunden und verwendet eine
monotone Uhr. Der reale Ressourcenbesitzer muss diese Frist auch innerhalb
seiner nativen Aufrufe einhalten. Die Bibliothek kann einen fehlerhaft unbegrenzt
blockierenden fremden Aufruf nicht selbst sicher abbrechen; sie prüft die Frist
vor und nach dessen Rückkehr. Ein überlaufender Widerrufszähler bleibt
dauerhaft gesperrt und kehrt nicht zu einem alten gültigen Stand zurück.

## Noch erforderliche Integration und Nachweise

Der native `Quiescer` fehlt. Er muss tatsächliches Prozessende und das Schließen
aller Referenzen nachweisen, bei Fehlern verbleibende Ressourcen zur
Wiederherstellung behalten und darf weder AOSP/LockSettings zurückrufen noch
auf Arbeit warten, die dieselbe Sperre benötigt. Auch der Aufrufer darf unter
einer Zugangssperre keine AOSP-Authentifizierung durchführen.

`revoke()` ist eine nicht blockierende Entwertung für Lebenszyklus-Callbacks,
**kein** Prozessabbau und keine abgeschlossene Benutzerabmeldung. Der künftige
Controller muss Ressourcenabbau bei externem AOSP-Stopp auslösen und Starts
für einen noch stoppenden Benutzer unabhängig von dieser Bibliothek verhindern.
Sitzungsbindung, Adminprüfung, native Besitzerregistrierung, systemweiter
Broker-Neustart und die direkten vold-Bereinigungspfade bleiben zu integrieren.

Zwölf vorbereitete Android-Tests verwenden einen simulierten Ressourcenbesitzer:
einmalige Versuche, Widerruf vor/während Speicheroperationen, fehlgeschlagene
Operationen, konkurrierende Zugriffe, Timeout, Threadunterbrechung, Serienwechsel,
Threadbindung, doppelte Freigabe und verbotene Rekursion. Insgesamt sind nun
44 Java- und 50 native Gerätetests vorbereitet. **Diese neuen Tests wurden
nicht kompiliert oder ausgeführt.** Sie werden auf `aegis-build` gebaut und
ausschließlich im lokalen Android-QEMU ausgeführt; eine Simulation bestätigt
keinen tatsächlichen CE-Schlüsselentzug oder Prozessabbau.
