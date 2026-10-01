# Aegis Desktop und Konsolenwerkzeuge

Stand: 1. Oktober 2026. Der Bedienumfang ist mit dem Nutzer abgestimmt.
Die Grafikrichtung ist ein Aegis-Wayland-Frontend als Ersatz der Mutter-/KWin-
Rolle für Linux-Anwendungen mit SurfaceFlinger als gemeinsamer Bildausgabe.
Die folgende Planung ist kein Implementierungsnachweis; konkrete Integration,
Kompatibilität und Leistung bleiben zu prüfen.

## Beschlossener Bedienumfang

AegisOS erhält ein eigenes Desktop Environment als primäre Oberfläche.
Es soll Anmeldung, Fenster, App-Wechsel, Einstellungen und Systembedienung
zusammenführen. Ein Android-Launcher allein erfüllt dieses Ziel nicht.
Normale Alltagsaufgaben sollen ohne Terminalbefehle möglich sein.
Einzelne Desktop-Komponenten dürfen technisch Android-Systemapps sein;
entscheidend ist der übernommene Funktionsumfang.

Die Kompatibilität richtet sich auf Android- und Linux-Anwendungen sowie die
dafür benötigten Schnittstellen. GNOME-Shell-Erweiterungen und KWin-Plugins
einschließlich KWin-Skripten und Effekten sind ausdrücklich ausgeschlossen.
GTK- und Qt-Anwendungen aus dem GNOME-/KDE-Umfeld bleiben Teil des
Kompatibilitätsziels.

Ein Terminal innerhalb des Desktops ermöglicht Linux-Befehle, Entwicklung
und Skripte im persönlichen Runtime-Kontext. Die Aegis-CLI bleibt für
Entwicklung, automatisierte Tests, Administration und Diagnose erhalten.
Ein begrenzter Zugang bei ausgefallenem Desktop wird gesondert geplant.

Eine vollständige zweite Benutzeroberfläche für reine Konsolenbedienung
ist nicht vorgesehen. Weder Funktionsgleichheit sämtlicher grafischer Aktionen
mit Konsolenbefehlen noch Serverbetrieb ohne Grafik oder ein regulärer Start
in eine Konsole sind derzeit Produktanforderungen.

## Verhältnis zu Phase 1

Der [Phase-1-Auftrag](phase-1-developer-brief.md) bleibt bestehen:
Neue Fundamentfunktionen werden zunächst über die CLI implementiert und
abgenommen. Ein eigener Desktop und grafischer Login sind keine zusätzlichen
Abnahmekriterien für Phase 1. Die Desktop-Architektur kann parallel geplant
werden; ihre Implementierung ist ein nachfolgender Arbeitsumfang.

## Gemeinsame Architektur als Planungsgrundlage

Desktop und CLI sollen strukturierte Schnittstellen der zuständigen
Aegis-Systemdienste verwenden. Die grafische Oberfläche soll keine
Shellbefehle zusammensetzen oder Terminalausgaben als Dienstprotokoll auswerten.
Vor einer Erweiterung werden die vorhandenen Dienste und Schnittstellen geprüft.

Benutzerzuordnung, Sitzungsbindung, Berechtigungen und frische Adminfreigaben
werden in den Diensten durchgesetzt. AOSP bleibt gemäß Phase-1-Auftrag die
einzige Autorität für persönliche Identität, Passwörter und Speicherentsperrung.
Die genaue Anbindung grafischer Anmeldung und Freigabedialoge ist zu entwerfen.

Beide Zugänge sollen dieselben tatsächlichen Zustände erhalten: Fortschritt,
Abbruchmöglichkeit, Fehler, ausstehende Paketaktivierung und erforderliche
Freigaben. Ein geschlossener Dialog oder ein abgestürzter Desktop darf nicht
als erfolgreiche Abmeldung oder abgeschlossene Paketaktion gelten.
Benutzerwechsel, Bildschirmsperre, Runtime-Stopp und vollständiger Logout
behalten ihre getrennten Bedeutungen.

## Gemeinsamer Arbeitsplan

1. **Bedienabläufe und Zuständigkeiten festlegen.** Für Anmeldung, Sperre,
   Benutzerwechsel, Logout, App-Start, Terminal und Paketaktionen werden
   Normalablauf, Freigaben, Abbruch und Fehler beschrieben. Das Ergebnis ordnet
   jeden Ablauf der Oberfläche und dem verantwortlichen Dienst zu und benennt
   den notwendigen CLI-Umfang.

2. **Gewählte Grafikrichtung konkretisieren.** Das Aegis-Wayland-Frontend soll
   Linux-Anwendungen die benötigten Compositorfunktionen anbieten und ihre
   Fenster in Androids gemeinsamen Grafik- und Eingabepfad integrieren.
   Prüfen, welche Aufgaben von WindowManager und SystemUI weiterverwendet
   oder durch Aegis übernommen werden; SurfaceFlinger bleibt als gemeinsame
   Kompositionsinstanz vorgesehen. Ergebnis sind ein Schnittstellenentwurf,
   eine App-/Protokoll-Testmatrix und eine Liste zusätzlicher Systemfunktionen,
   etwa Berechtigungsdialoge, Benachrichtigungen und Dateiauswahl.

3. **Gemeinsame Schnittstellen konkretisieren.** Vorhandene CLI-/Dienstpfade
   abgleichen und nur fehlende Verträge ergänzen. Als Nachweis dienen gleiche
   Autorisierungsentscheidungen und verständliche Ergebnisse für beide Zugänge,
   einschließlich paralleler Aktionen, Sperre und Verbindungsabbruch.

4. **Einen begrenzten Desktop-Prototyp prüfen.** Nach dem Schnittstellenentwurf
   Anmeldung und eine erste Sitzung mit einer Android-App, einer grafischen
   Linux-App und Terminal erproben. Fokus, Größenänderung, Tastatur, Maus,
   Benutzertrennung und Desktop-Neustart müssen im gewählten Ansatz funktionieren.
   Mit zwei Benutzern prüfen, dass Fenster, Eingaben und Zwischenablageninhalte
   nicht in die fremde Sitzung gelangen.
   Dieser Prototyp ist ein erster technischer Nachweis, keine vollständige
   Desktop-Abnahme und keine Zusicherung allgemeiner App-Kompatibilität.

5. **Zugang bei Desktop-Ausfall festlegen und testen.** Zunächst Diagnose,
   kontrollierter Neustart und ausdrücklich autorisierte Reparatur beschreiben.
   Zugang und Authentifizierung müssen unabhängig von der ausgefallenen
   Desktop-Oberfläche nutzbar sein. Der Ausfall von AOSP-Diensten ist ein anderer
   Fehlerfall und benötigt gegebenenfalls einen separaten Recovery-Entwurf.
   Der vorhandene QEMU-/ADB-Entwicklungszugang und ein späterer Zugang für
   ausgelieferte Geräte werden dabei getrennt bewertet.

## Noch offene Entscheidungen

Die [Untersuchung zur gemeinsamen Grafik](shared-display-study.md) hält den
vereinbarten Ersatz der Mutter-/KWin-Rolle auf der Linux-App-Seite fest.
SurfaceFlinger soll die gemeinsame Komposition übernehmen. Die technische
Ausgestaltung bleibt von den dort beschriebenen Nachweisen abhängig.

- Implementierung des Wayland-Frontends, UI-Toolkit, Grafikpufferübergabe und
  genaue Verbindung zwischen Android- und Linux-Fenstern.
- Übernahme oder Ersatz einzelner Android-Systemoberflächen mitsamt ihren Aufgaben.
- Lokaler Diagnosezugang, dessen verfügbare Befehle und Authentifizierung;
  Fernzugriff ist damit nicht beschlossen.
- Gemeinsames Bedienmodell für Tastatur/Maus und spätere Touch-Geräte.
- Konkrete Android-App-Testmatrix und gewünschter Umfang formaler Kompatibilität.
- Konkrete Linux-App-/Protokoll-Testmatrix und erforderliche Desktop-Dienste.

Der Diagnosezugang darf AOSP-Authentifizierung, Benutzertrennung und verschlüsselten
Speicher nicht umgehen. Ein Terminalfenster im Desktop allein deckt einen
Desktop-Ausfall nicht ab. Änderungen am laufenden QEMU, an Bootdiensten oder
Android-Oberflächen sind durch dieses Planungsdokument noch nicht umgesetzt.
