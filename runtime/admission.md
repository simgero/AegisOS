# Serialisierung von Runtime-Zugang und CE-Operationen

Stand 29. September 2026: Die bisherigen Serialisierungs-/Storage-Tests sind
im lokalen Android-QEMU bestanden. Commit `2a766ab5` verbindet die Bibliothek
mit dem echten AOSP-Anmeldepfad, dem nativen Broker und dem Storage-Controller.
Kompilierung und alle neuen
Sitzungsbindungs-Gerätetests sind im Komponentenstand bestanden. Das Produkt bleibt
`absent`; `managed-v1` darf erst mit Init, SELinux und Systemtests aktiviert werden.

## Reihenfolge und Eigentümer

Jede numerische AOSP-Benutzer-ID besitzt eine eigene faire Sperre und einen
Widerrufszähler. Benutzer-ID plus AOSP-Seriennummer bestimmen den aktuellen
Eigentümer. Die Daten liegen nur im Arbeitsspeicher; sie sind keine persönliche
Benutzerdatenbank, Passwortprüfung oder Schlüsselautorität.

Der Identitätsdienst legt unmittelbar vor der echten AOSP-Authentifizierung
einen einmaligen Versuch an. Nach erfolgreicher Authentifizierung und
Prüfung des tatsächlichen Benutzers, seiner Seriennummer und des CE-Status
löst er diesen Versuch ein. Abgelehnte Versuche werden verworfen.
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

## Bindung an eine konkrete Anmeldung

Nach einer bestätigten Anmeldung erhält nur die interne Sitzungsverwaltung
eine `Binding` mit Gate, Benutzer, Seriennummer und zugelassener Epoch. Sie wird
weder serialisiert noch an die CLI weitergereicht. Bestehende Zugriffe verwenden
diese konkrete Bindung; eine Benutzer-ID allein reicht nicht. Nach Widerruf
bleibt die alte Bindung ungültig, selbst wenn derselbe Benutzer in einem anderen
Terminal frisch angemeldet wird. Mehrere gültige Anmeldungen können denselben
bestehenden Kontext nutzen, solange kein Widerruf stattfindet.

## Noch erforderliche Integration und Nachweise

Der aktuelle `Quiescer` widerruft Terminalbindungen ohne Identitätsmonitor und
verlangt vom nativen Besitzer bestätigten Stopp aller Seriennummern. Im aktuellen
Pfad gibt es keine öffentlichen PTY- oder Paketoperationen; deren spätere
Einführung muss den Ressourcenbesitz und Widerruf ergänzen. AOSP/LockSettings
wird niemals unter dieser Sperre aufgerufen. Die CLI bietet zunächst ausschließlich
Start, Status und Stopp für die eigene, im Dienst authentifizierte Auswahl.

`revoke()` ist eine nicht blockierende Entwertung für Lifecycle-Callbacks,
**kein** Prozessabbau und keine abgeschlossene Abmeldung. Bei einer destruktiven
AOSP-Speicheroperation bestätigt erst die separate Storage-Lease den nativen
Abbau vor dem Schlüsselaufruf. Broker-Neustart/Reparatur, öffentliche Terminals,
Pakettransaktionen, SELinux-/Init-Aktivierung, direkte vold-Löschpfade sowie
wirkliche externe AOSP-Stopp-/Login-Rennen bleiben zu prüfen beziehungsweise
zu integrieren.

Drei zusätzliche Tests betreffen die dauerhafte Ungültigkeit einer alten
Sitzungsbindung, mehrere gültige Anmeldungen und fremde Gate-/geschlossene
Scope-Bindungen. Die neue Suite enthält 62 Java- und 111 native Gerätetests.
Der simulierte Quiescer in diesen Tests ersetzt keinen tatsächlichen
CE-Schlüsselentzug oder Prozessabbau. Sie werden auf `aegis-build` gebaut und
nur im lokalen Android-QEMU ausgeführt.
