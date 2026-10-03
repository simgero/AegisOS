# Architektur

- [Phase 1: Zieldefinition und Definition of Done](phase-1-dod.md) – sieben Abschlusskriterien, vollständige Pflicht-Testmatrix und Referenzablauf; aktueller Status: Kernprototyp teilabgenommen.

- [AegisOS – Foundation / Phase 1: Entwicklerauftrag](phase-1-developer-brief.md) – verbindliche Spezifikation mit AOSP als einziger Identity Authority, isolierten GNU/Linux-Runtime-Kontexten und gemeinsamer beziehungsweise privater Paketverwaltung. Beschreibt Anforderungen und spätere Abnahmetests; kein Implementierungsnachweis.
- [Aegis Desktop und Konsolenwerkzeuge](desktop-and-cli.md) – abgestimmter Bedienumfang und gemeinsamer Architekturplan: eigener Desktop, Terminal-App und gezielte CLI für Entwicklung, Tests, Administration und Diagnose. Die technische Ausgestaltung bleibt zu prüfen; Phase 1 behält ihre CLI-Abnahme.
- [Gemeinsame Grafik für Android und Linux](shared-display-study.md) – vereinbarte Grafikrichtung: Aegis übernimmt für Linux-Anwendungen die Mutter-/KWin-Rolle über ein Wayland-Frontend; SurfaceFlinger bleibt gemeinsame Kompositionsinstanz. Quellenvergleich, Kompatibilitätsumfang und offene Nachweise.

## Implementierung und Nachweise

- [Bedrohungsmodell](../../security/THREAT_MODEL.md) – zwölf verbindliche Szenarien, Vertrauensannahmen und Grenzen des Entwicklungsprototyps.
- [AOSP-Identität und Kryptographie](../identity-crypto-baseline.md) – Zuständigkeiten, UID-Zuordnung, Passworttransport und an den Build gebundene Verschlüsselungskonfiguration; statische und tatsächliche Gastnachweise bleiben getrennt.
- [Interaktiver Testtreiber und CLI-Ablauf](../runtime-gnu-test-driver.md) – ausführbare Benutzer-, Runtime- und Paketaktionen; ältere CLI-Stände sind separat in [Terminalintegration](../identity-cli.md) dokumentiert.
- [Paketpfad und Konsistenz](../package-network.md) – Antragstellerbindung, frische Adminfreigabe, Planung, unveränderliche Generationen, Aktivierung und private Entfernung.
- [Serverentwicklung](../server-development.md) und [persistente QEMU-Profile](../persistent-qemu.md) – Build-/Startverfahren und zusammengehöriger Android-/KeyMint-Zustand.
- [Ergebnisindex](../phase-1-result-index.md) – konkrete Belege und offene Pflichtfälle. Einzelne bestandene Abläufe bedeuten keine vollständige Phase-1-Freigabe.

## Historische Grundlage

- [Foundation-Entwurf vom 27. September 2026](foundation-20260927.md)
- [Früher Phase-1-Plan](phase-1.md)

Für den heutigen Umfang und Entwicklungsablauf gilt der oben verlinkte Entwicklerauftrag; die historischen Cuttlefish-/x86-Pläne sind überholt.
