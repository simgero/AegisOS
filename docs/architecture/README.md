# Architektur

- [AegisOS – Foundation / Phase 1: Entwicklerauftrag](phase-1-developer-brief.md) – verbindliche Spezifikation mit AOSP als einziger Identity Authority, isolierten GNU/Linux-Runtime-Kontexten und gemeinsamer beziehungsweise privater Paketverwaltung. Beschreibt Anforderungen und spätere Abnahmetests; kein Implementierungsnachweis.
- [Aegis Desktop und Konsolenwerkzeuge](desktop-and-cli.md) – abgestimmter Bedienumfang und gemeinsamer Architekturplan: eigener Desktop, Terminal-App und gezielte CLI für Entwicklung, Tests, Administration und Diagnose. Die technische Ausgestaltung bleibt zu prüfen; Phase 1 behält ihre CLI-Abnahme.
- [Gemeinsame Grafik für Android und Linux](shared-display-study.md) – vereinbarte Grafikrichtung: Aegis übernimmt für Linux-Anwendungen die Mutter-/KWin-Rolle über ein Wayland-Frontend; SurfaceFlinger bleibt gemeinsame Kompositionsinstanz. Quellenvergleich, Kompatibilitätsumfang und offene Nachweise.

## Historische Grundlage

- [Foundation-Entwurf vom 27. September 2026](foundation-20260927.md)
- [Früher Phase-1-Plan](phase-1.md)
- [Bedrohungsmodell](../../security/THREAT_MODEL.md)

Für den heutigen Umfang und Entwicklungsablauf gilt der oben verlinkte Entwicklerauftrag; die historischen Cuttlefish-/x86-Pläne sind überholt.
