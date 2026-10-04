# Phase 1 – Übergabe und Nachweisgrenzen

Stand: 4. Oktober 2026. **Keine Gesamtfreigabe; D1–D7 bleiben offen.**
Diese Übersicht ordnet die 14 Liefergegenstände aus Abschnitt 19 des
[Entwicklerauftrags](architecture/phase-1-developer-brief.md) den vorhandenen
Quellen und Nachweisen zu. Die vollständige [DoD](architecture/phase-1-dod.md)
einschließlich T01–T17 und Referenzablauf bleibt verbindlich.

## Welcher Stand ist gemeint?

| Stand | Rolle und Nachweisumfang |
| --- | --- |
| `b832d6c077baeee4324e00d00dc3618372f3e9d9` | Letzter umfangreicher persönlicher Gastlauf. T03, T07, T08 und T14 sind diesem Image zugeordnet; weitere Bereiche besitzen Teilbelege. Bekannter Shell-Startfehler und weitere Pflichtfälle offen. |
| `dedd1dabc45efe080a499b3657b7ee09d2fda5a7` | Shell-Korrektur gebaut; erster Boot ausschließlich mit Systembenutzer 0, ADB und Bildschirmaufnahme belegt. Vorhandene Bootlogs sind offline geprüft. Keine persönliche Abnahme dieses Images. |
| `ec01e5fa5f2822da5763ab54c644bc5c5c5ab413` | Neuester Android-Vollbuild, zusätzlich mit misctrl-Korrektur. 20 Images, 18 Buildbelege sowie tatsächlich ausgelieferte misctrl-Dateien geprüft. Noch kein Boot dieses Images. |
| `d9a0d30a48c1f745ffcc02dbcb6b4c15054e89e6` | Neueres Vorbereitungsrezept für unveränderte ec01e5fa-Images. Übergibt den bestehenden Verity-Modus ausdrücklich und lehnt deaktivierende AVB-Flags ab. Keine neue Android-Kompilierung und kein Gastnachweis dadurch. |

Die neueste Factory-Vorbereitung liegt unter
`/srv/aegis/runs/phase1-ec01e5fa-verity0`, gehört `aegis-build` und besitzt
noch kein persistentes Profil. Build-/Rezeptbefehle, Kernel-/AOSP-/Debian-Pins
und Prüfsummen stehen im [Buildinventar](phase-1-build-inventory.md).
Bestehende Profile bleiben an ihre ursprünglichen Images, Bootkonfiguration
und Helper-Dateien gebunden. Diese Übersicht autorisiert keine Imagemigration.

## Liefergegenstände aus Abschnitt 19

„Vorhanden“ bedeutet eine vorhandene Implementierung oder Dokumentation.
Die rechte Spalte nennt den verbleibenden Nachweis; sie erteilt keine
Abnahme aufgrund vorhandener Dateien oder erfolgreicher Kompilierung.

| Nr. | Liefergegenstand und maßgebliche Ablage | Verbleibender Abschlussnachweis |
| --- | --- | --- |
| 1 | Projektquellen in Git; AOSP-/Kernel-/Runtime-Pins im [Buildinventar](phase-1-build-inventory.md) | Endgültigen Abnahmecommit samt Build-/Rezeptstand festlegen; bisherige Belege behalten ihre eigenen Commitbindungen. |
| 2 | [Buildinventar](phase-1-build-inventory.md), [Terminalanleitung](terminal-quickstart.md), [Profilpaar](persistent-qemu.md) | Vollständigen Start-/Referenzablauf auf dem abschließenden Stand ausführen. Das konkrete interaktive Startbeispiel verwendet derzeit den älteren belegten b832d6c-Stand. |
| 3 | Lokales ec01e5fa-Development-Image und separate geprüfte Vorbereitung | Tatsächlichen Boot, Eingabe, ADB, Dienststabilität und Integritätsschutz dieser Kombination gemäß D1 prüfen. |
| 4 | [CLI-Quellen](../packages/aegis/identity/cli/org/aegisos/identity/Aegis.java), [Bedienung](terminal-quickstart.md) | Sitzungs-, Passwort-, Shell- und Paketvarianten aus T01–T17 auf dem endgültigen Image. Insbesondere die neue Shell-Korrektur ist noch nicht im Gast nachgewiesen. |
| 5 | [AOSP-Backend](../packages/aegis/identity/src/org/aegisos/identity/AospIdentityBackend.java), [Speicherlebenszyklus](../runtime/aosp-storage-lifecycle.md) | Vollständige Anlage-/Lösch-/Seriennummern- und Wiederverwendungsfälle T01/T12; historische Löschbelege reichen nicht für ec01e5fa. |
| 6 | [Anmelde-/Transportgrundlage](identity-crypto-baseline.md), AOSP-Backend und Terminalbindung | Alle Ziel-/Aufrufer-/Fehlerfälle und dynamischen Offenlegungsprüfungen T01; Passwortwechsel über Sperre und Neustart T02. |
| 7 | [Persönlicher CE-Speicher](../runtime/personal-storage.md), [Kryptographie](identity-crypto-baseline.md) | Konkrete Speicherzustände, tatsächliche Datenzugriffe und bestätigter Schlüsselentzug im finalen Ablauf T07/T10–T12. |
| 8 | [Debian-Basis](../runtime/README.md), [Generationen](../runtime/generations.md) | Tatsächliche GNU-Ausführung der gemeinsamen Basis und Paketvarianten im neuen Gast; Image-/Basisprüfsummen allein reichen nicht. |
| 9 | [UID/GID-Zuordnung](../runtime/uid-mapping.md), [Namespaces](../runtime/namespace-setup.md), [Admission](../runtime/admission.md), [Terminalkanal](../runtime/terminal-handoff.md) | Vollständige Start-, Shell-, Isolations- und Lebenszyklusmatrix T03–T11 mit realen Kontexten. |
| 10 | [Testtreiber](runtime-gnu-test-driver.md), [Ergebnisindex](phase-1-result-index.md), [Abnahmeübersicht](phase-1-current-status.md) | Alle Pflichtvarianten ausführen und dem endgültigen Image zuordnen; Hosttests und historische Läufe ersetzen fehlende Systemtests nicht. |
| 11 | [Architekturgrundlage](architecture/foundation-20260927.md), [UID-Mapping](../runtime/uid-mapping.md), [Pakettransaktionen](../runtime/package-transactions.md), [Broker](../runtime/broker-control.md) | Quellen und konkrete Laufzeitbelege am endgültigen Stand abgleichen. Historische Entwicklungsabschnitte sind keine aktuellen Freigaben. |
| 12 | [Threat Model](../security/THREAT_MODEL.md) mit allen zwölf geforderten Szenarien, Schutzwirkung, Annahmen und Grenzen | Technische Nachweise für jedes Szenario mit T01–T17 abschließen; keine allgemeine Host-/Kernel- oder Hardware-Sicherheitszusage. |
| 13 | [Kryptographiegrundlage](identity-crypto-baseline.md): Modi, Schlüssellängen, AOSP-Ableitung, Zufall und Helper-Grenzen | Neue Quellbindung ist vorhanden; aktuelle Gastkonfiguration, betroffene CE-Policies und vollständige Schlüssel-Lebenszyklen bleiben gesondert zu prüfen. |
| 14 | [Paketverwaltung](../runtime/package-transactions.md), [CLI-Bereiche](terminal-quickstart.md#gemeinsame-und-persönliche-pakete) | Ganze Aktions-/Autorisierungs-, Versions-, Konflikt-, Aktivierungs- und Parallelitätsmatrix T13–T17; vorhandene erfolgreiche b832d6c-Abläufe werden nicht übertragen. |

## Zuständigkeiten und Bedienungsgrenzen

AOSP ist die persönliche Identitäts-, Passwort- und CE-Schlüsselautorität.
Die gemeinsame Debian-Basis enthält ein technisches Konto mit internem
UID/GID 1000 und HOME `/home/user`; persönliche Kontexte besitzen getrennte
Host-Zuordnungen. Namen wählen eine AOSP-Identität aus, ersetzen aber nicht
deren ID, Seriennummer und Lebenszyklusbindung.

Die Terminalanmeldung gehört zum jeweiligen CLI-Prozess. Der korrigierte
Shell-Zugang verlangt einen vorhandenen bereiten Kontext. Shell-Exit, Ende
des CLI-Clients, Runtime-Stopp, Benutzerwechsel, Bildschirmsperre und
bestätigter Logout sind verschiedene Vorgänge. Besonders Wechsel und
Bildschirmsperre behaupten keinen CE-Schlüsselentzug. Die 15-Minuten-Grenze
des Startabgleichs ist eine Wartegrenze, kein Beleg erfolgreichen Starts
oder abgeschlossenen Abbaus.

Für gemeinsame und persönliche Paketaktionen ist die frische AOSP-Adminfreigabe
erforderlich. Persönlicher Eigentümer bleibt der authentifizierte Antragsteller.
Eine Zustimmung gibt dem Admin kein direktes Leserecht auf fremde CE-Dateien.
Gemeinsam installierte ausführbare Software bleibt eine Verwaltungs-
Vertrauensentscheidung; normale GNU-Prozesse erhalten keinen schreibbaren
Zugriff auf die gemeinsame Basis. Unterschiedliche private Versionen und
gemeinsame Updates erfordern die vollständigen Konsistenznachweise T15–T17.

## Neu geprüfte Quellkontinuität

Der lokale Bericht
`out/phase1-dod/ec01e5fa-handoff/crypto-and-transport-source-binding.json`,
SHA-256 `e52ffe64c237715cb570fc4f68cf568187265459780a917cee0166b3eb3aa685`,
bindet vier AOSP-Kryptographiedateien und neun CLI-/JNI-/AIDL-/Dienst-/
Transporttestdateien an das tatsächliche ec01e5fa-Buildmanifest. Die vier
Kryptographiedateien und acht Transportdateien sind bytegleich zum früheren
Review. Der Service unterscheidet sich ausschließlich innerhalb `linuxShell`.
Dieser statische Vergleich liest keine Passwörter oder Schlüssel und erzeugt
keinen neuen Nachweis für Laufzeitkopien, Argumente, History oder Logs.

## Abschluss und Ablage

Vor der Freigabe müssen alle sieben DoD-Kriterien, sämtliche T01–T17-Varianten
und der zusammenhängende Referenzablauf auf dem endgültigen Stand belegt sein.
Die [Abnahmeübersicht](phase-1-current-status.md) enthält die vollständige
Restliste des letzten persönlichen Laufs. Zusätzlich benötigen die neueren
Shell-/misctrl-/Startkorrekturen ihre jeweils betroffenen Gastnachweise.
Diese Übersicht schließt D7 nicht vorzeitig ab.

GitHub enthält Quellen, Testwerkzeuge und Dokumentation. Images, mutable
Profilpaare und umfangreiche Rohbelege bleiben auf dem Buildserver. Android-
Datenpartition und KeyMint-Helferzustand werden zusammen erhalten; die
Software-TPM-/userdebug-Umgebung bietet keine vom Host unabhängige
Hardware-Vertrauenswurzel. Private Echtdateien und Produktionsschlüssel
gehören nicht in diese Entwicklungsprofile.
