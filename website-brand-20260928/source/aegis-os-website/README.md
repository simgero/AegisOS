# Aegis OS — Marketing-Website

Eine deutschsprachige, responsive Produkt- und Projektwebsite mit dem Leitgedanken:

**Dein digitales Leben. Wieder deins.**

Die Seite vermittelt die langfristige Produktvision und den frühen Entwicklungsstand, ohne bereits implementierte Produktfunktionen, einen Veröffentlichungstermin oder bestätigte Gerätemodelle zu erfinden.

## Öffnen

Für eine schnelle Ansicht ist die separat bereitgestellte Datei **Aegis-OS.html** vollständig eigenständig: im Browser öffnen. Sie enthält die Illustration eingebettet und benötigt keine Installation, keinen Build und keine externen Ressourcen.

Im Webprojekt kann **index.html** direkt geöffnet werden. Der Ordner `assets` muss daneben liegen. Für eine lokale HTTP-Vorschau mit Python:

```sh
python3 serve.py
```

Die lokale Adresse wird beim Start ausgegeben. Standardmäßig wird ausschließlich auf `127.0.0.1:8000` gelauscht. Der Vorschau-Server ist nicht für den öffentlichen Produktivbetrieb gedacht.

## Enthalten

- Große Produktbühne mit originalen HTML-/CSS-Geräteansichten und eigenständiger SVG-Hintergrundillustration.
- Projektidee, Privatsphäre und verständliche Produktziele.
- Interaktive Beispielprofile mit sichtbar getrennten Inhalten.
- Vier Entwicklungsphasen mit Maus- und Tastaturbedienung.
- Eigene Bereiche für Entwickler, Gestalter, Sponsoren und Partner.
- Lokale Downloads von Entwickler-, Sponsoren- und Projektbriefings als Markdown.
- FAQ, native Dialoge, mobile Navigation und sichtbare Tastaturfokusse.
- Rücksicht auf reduzierte Bewegung; Hauptinhalte sind ohne JavaScript lesbar.
- Keine externen Schriften, Bild-CDNs, Analysewerkzeuge, Cookies, Formulare oder Backend-Abhängigkeiten.

## Dateien

```text
index.html                 Vollständige Seite mit eingebettetem CSS und JavaScript
assets/wallpaper.svg        Eigenständige Illustration der Designvision
serve.py                   Optionaler lokaler Vorschau-Server
README.md                  Diese Dokumentation
docs/CONTENT_NOTES.md      Inhaltliche Grundlagen und offene Veröffentlichungsangaben
tests/check_site.py         Automatisierte Chromium-Funktionsprüfungen
tests/test-report.json      Ergebnis der durchgeführten Tests
```

## Inhalte anpassen

Die Seitentexte stehen im HTML. Die interaktiven Entwicklungsphasen sind im Array `phases`, Dialoginhalte im Objekt `dialogs` und herunterladbare Briefings im Objekt `briefings` definiert. Designvariablen stehen am Anfang des CSS unter `:root`.

Es wurde bewusst kein Formular eingebaut, das eine angebliche Anmeldung oder Übermittlung bestätigt. Es existieren noch keine verifizierten öffentlichen Kontakt- und Zahlungswege. Die Briefing-Downloads funktionieren tatsächlich und erzeugen die Dateien ausschließlich im Browser.

## Vor der Veröffentlichung

1. Verantwortliche Anbieterangaben und einen verifizierten Kontaktweg hinterlegen; der aktuelle Impressumsdialog ist ausdrücklich nur ein Vorschauhinweis.
2. Datenschutzhinweise an den tatsächlichen Hostingbetrieb und eventuell ergänzte Kontaktfunktionen anpassen.
3. Projektstatus, Textfreigabe und längerfristige Ziele durch die Projektverantwortlichen bestätigen.
4. Falls öffentliche Codezugänge oder Fördermöglichkeiten verfügbar werden, echte Ziele ergänzen. Das bestehende private Repository wird nicht als öffentlich zugänglicher Beitragskanal beworben.
5. `noindex, nofollow` im HTML erst nach Freigabe für Suchmaschinen entfernen. Domainabhängige Metadaten wie Canonical-URL anschließend ergänzen.

Die Dateien wurden **nicht** veröffentlicht und das AegisOS-Repository wurde **nicht** verändert.

## Tests

Getestet wurde in Chromium bei 1440, 768, 390 und 320 Pixeln Viewportbreite: kein horizontaler Überlauf, keine JavaScript-Fehler, keine externen Netzwerkaufrufe; Navigation, Profile, Tastatursteuerung der Phasen, Dialoge, FAQ und Briefing-Download funktionieren. Inhalte wurden außerdem ohne JavaScript und mit reduzierter Bewegung geprüft.

Die Tests verwenden Playwright für Python und einen Chromium-Browser. Optional kann `AEGIS_CHROMIUM` den ausführbaren Browserpfad festlegen. Mit vorbereiteter Testumgebung:

```sh
python3 tests/check_site.py
```

Die automatisierte Prüfung speist das lokale HTML inklusive eingebetteter Illustration in den Browser ein. Sie ist kein Nachweis einer vollständigen Barrierefreiheitskonformität oder ein Test aller Browser, Hostingumgebungen und Geräte.
