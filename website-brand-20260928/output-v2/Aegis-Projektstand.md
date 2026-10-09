# Aegis OS · Projektstand

Stand der ausgewerteten lokalen Projektprotokolle: 28. September 2026.

## Die Produktidee

Android-Anwendungen und GNU/Linux-Werkzeuge in einem gemeinsamen Betriebssystem auf AOSP-Basis. Die GNU/Linux-Runtime soll denselben Kernel verwenden und die AOSP-Systemidentität übernehmen. Eine zweite persönliche Benutzer- oder Passwortverwaltung ist nicht vorgesehen.

Die Runtime-Basis stammt aus Debian. glibc, Bash, apt und grundlegende GNU-Werkzeuge bilden den vorgesehenen Einstieg. Klassische grafische Linux-Anwendungen und ein eigenes Geräteerlebnis sind spätere Schritte.

## Was nachgewiesen ist

- ARM64-Android startet lokal in QEMU, mit sichtbarer Android-Oberfläche.
- ADB, Dateiübertragung und Bildschirmaufnahme wurden geprüft.
- AOSP wies ein falsches Passwort zurück, sperrte den Speicher eines gestoppten Testbenutzers und stellte mit dem richtigen Passwort denselben Dateiinhalt wieder bereit.
- Ein Passwortwechsel und die anschließende Plattformlöschung des Testbenutzers wurden geprüft.

Diese Tests betreffen die bestehende AOSP-Plattform, nicht die vollständige neue Aegis-Integration.

## Was noch in Arbeit ist

- Aegis-Identitätsdienst und CLI sind im Quelltext vorbereitet, aber nicht vollständig im Gast integriert und nachgewiesen.
- Die Debian-Basis wurde importiert und geprüft. Es gibt noch keine ausführbare Aegis-GNU/Linux-Runtime.
- Runtime-Start, Isolation, persönliche Speicheranbindung, Paketverwaltung und vollständiger Abmeldeablauf müssen integriert werden.
- Neustarts, konkurrierende Vorgänge und Fehlerfälle müssen am gesamten System getestet werden.

## Paketarchitektur

Ein gemeinsamer Softwarebestand soll allen Kontexten zur Verfügung stehen. Persönliche Pakete und ausdrücklich ausgewählte private Versionen sollen auf den eigenen Kontext begrenzt bleiben. Laufende Kontexte sollen bei gemeinsamen Updates einen konsistenten Stand behalten, bis eine kontrollierte Aktivierung erfolgt. Beide Paketbereiche erfordern die vorgesehene Aegis-Adminautorisierung.

Diese Beschreibung ist ein Entwicklungsziel, kein vorhandener Paketmanager.

## Perspektive

Langfristig: Smartphone, Tablet, ARM-Notebook und Desktop. Aktuell: ARM64-Entwicklung in QEMU. Keine zugesagte Hardwareliste, kein öffentlicher Endnutzer-Release, keine generelle Android- oder Linux-App-Kompatibilitätszusage.

## Quellenbasis

Ausgewertet im lokalen AegisOS-Projekt:

- docs/architecture/phase-1-developer-brief.md, insbesondere Projektidee, Systemarchitektur, GNU/Linux-Integration und Paketverwaltung.
- docs/phase-1-progress.md, Stand 28. September 2026.
- docs/qemu-first-boot.md, Abschnitt „Verifizierter Android-Start am 28. September 2026“.
- docs/identity-platform-test.md, Plattformversuche und Aussagegrenzen.
- runtime/README.md, importierte Debian-Basis und verbleibende Integrationsschritte.

Das öffentliche Repository ist https://github.com/simgero/AegisOS. Der lokale Entwicklungsstand kann neuer sein als der veröffentlichte Stand. Die ältere Haupt-README ist beim Bootstatus nicht maßgeblich; die datierten Testprotokolle enthalten den neueren Nachweis.

Die Website-Oberfläche illustriert ein mögliches Zusammenspiel und ist keine laufende Aegis-Oberfläche. Die Protokolle sind Projektquellen, keine unabhängige Sicherheitsprüfung.
