# AegisOS

Ein geplantes, datenschutzorientiertes Betriebssystem auf AOSP-Basis für Smartphone, Tablet, Notebook und Desktop.

**Status: Architekturentwurf und Entwicklungsinfrastruktur. Noch kein einsatzfähiges Betriebssystem und keine implementierte Sicherheitsgarantie.**

## Grundentscheidungen

- AOSP ist die Plattform; gerätespezifische Unterstützung bleibt vom Aegis-Code getrennt.
- AOSP bleibt die maßgebliche lokale Benutzer- und Authentifizierungsinstanz. Keine parallele Passwortdatenbank.
- Eine spätere Debian-/Ubuntu-Runtime erhält isolierte Benutzerkontexte, keine zweite Anmeldung.
- Zunächst nur Terminal: keine eigene grafische Oberfläche und keine Cloud.
- Bestehende Plattformmechanismen und überprüfte Kryptographie verwenden; keine eigenen Verschlüsselungsverfahren.

## Dokumentation

- [Architektur und Schlüsselverwaltung](docs/architecture/README.md)
- [Bedrohungsmodell und Sicherheitsgrenzen](security/THREAT_MODEL.md)
- [Phase 1: Meilensteine und Abnahmetests](docs/architecture/phase-1.md)

## Jetzt ausführbar

Auf dem Linux-Entwicklungsrechner, im Repository:

```bash
bash scripts/check-host.sh
```

Die Vorprüfung liest Architektur, Ressourcen und Hinweise auf KVM-Verfügbarkeit. Sie installiert nichts, lädt nichts herunter und ändert keine Systemkonfiguration. Ein positives Ergebnis ersetzt keinen erfolgreichen Start eines Android-Testsystems.

Eine `aegis`-CLI, ein Login-Dienst und eine GNU/Linux-Runtime sind noch nicht implementiert. Die vorhandene CI prüft bisher nur die Repository-Struktur, nicht die Sicherheit eines Betriebssystems.
