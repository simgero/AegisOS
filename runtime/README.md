# Gemeinsame Debian-Basis

Stand 29. September 2026: **Gemeinsame Basis erzeugt, im Android-Image geprüft
und in zwei persönlichen GNU-Kontexten im lokalen Mac-QEMU ausgeführt.**
Der unten beschriebene Import selbst bleibt eine reine Datei-/Quellprüfung.
Die anschließende Integration und ihre Grenzen stehen im
[GNU-Test](../docs/runtime-gnu-qemu-test.md). Pakettransaktionen und verwaltete
Benutzerlöschung sind noch nicht implementiert.

## Festgelegte Herkunft

`debian-arm64.json` legt Debian 13.7 / Trixie slim für ARM64/v8 aus dem offiziellen
Debuerreotype-Artefaktrepository fest. Der Tag `trixie-20260918-slim` ist nur eine
Herkunftsangabe; der Import verwendet ausschließlich den unveränderlichen Commit
`ca011a8b1c3b259e4cbbf83bf6841f1fd5f497c1` und einen gepinnten SHA-256-Wert.

- [Offizielle Imagezuordnung zum ARM64-Commit](https://github.com/docker-library/official-images/blob/f20899753df292334e40a1b35d76dc0b976841b3/library/debian)
- [Ursprüngliches OCI-Manifest](https://github.com/debuerreotype/docker-debian-artifacts/blob/ca011a8b1c3b259e4cbbf83bf6841f1fd5f497c1/trixie/slim/oci/blobs/image-manifest.json)
- [Debuerreotype-Paketliste](https://github.com/debuerreotype/docker-debian-artifacts/blob/ca011a8b1c3b259e4cbbf83bf6841f1fd5f497c1/trixie/slim/rootfs.manifest)

Das Manifest bestimmt Prüfsummen und Größen von OCI-Konfiguration und Rootfs.
Die Konfiguration bestimmt zusätzlich den Hash des unkomprimierten TAR-Streams.
Die Integritätskette beginnt beim überprüften Pin und der TLS-Verbindung zu
GitHub; sie ist keine unabhängige Signaturprüfung aller Debian-Pakete.

| Teil | Verifizierter Wert |
| --- | --- |
| Komprimiertes Rootfs | 30.189.691 Bytes |
| Unkomprimierter TAR-Stream | 103.137.280 Bytes |
| Rootfs-SHA-256 | `bd36565c0fdebaf0f3af5c3b4ce610ca085ced32e9e9da850d95912f5f18f47b` |
| Unkomprimierter SHA-256 | `8227a1264c7ff2f8bd125b585e62c1cec77dd2099c985c33e08d34292d06144f` |
| Archiv | 3.263 Einträge; 78 vollständig installierte ARM64-/architekturunabhängige Pakete |
| GNU-Komponenten | glibc `2.41-12+deb13u4`, Bash `5.2.37-2+b10`, apt `3.0.3`, dpkg `1.22.22`, coreutils `9.7-3` |

## Import und Wiederholungsprüfung

Im Projekt beziehungsweise dem gleich aufgebauten GitHub-Quellsnapshot:

```sh
python3 scripts/runtime/base.py fetch downloads/runtime-base/debian-13.7-arm64
python3 scripts/runtime/base.py verify downloads/runtime-base/debian-13.7-arm64
```

Der Import benötigt Python 3.9+ und curl mit `--max-filesize`-Unterstützung.
Er lädt nur von festgelegten HTTPS-Pfaden auf GitHub. Es werden weder Docker
noch ein Containerdaemon benötigt. OCI-Startbefehle und Umgebungsvariablen
werden nicht angewendet. Auf dem Mac ist dies eine Datei-/Quellprüfung;
Image-Erstellung und Kompilierung bleiben auf `aegis-build`, Systemtests in
lokalem QEMU.

Ein neues Ziel wird erst nach vollständiger Prüfung sichtbar. Vorhandene,
veränderte Imports bleiben erhalten und werden abgewiesen. Abgebrochene
Downloads entfernen nur das eigens angelegte Staging-Verzeichnis. Es gibt
keine automatische Wiederholung eines fehlgeschlagenen Netzwerkaufrufs.

Prüfungen umfassen:

- Manifest-/Konfigurations-/Rootfs-Hashes, Längen und begrenzte Dekompression;
- genau eine vollständige Basis-Layer, passende Linux-ARM64/v8-Metadaten und
  die ARM64-ELF-Header der benötigten Programme und glibc-Komponenten;
- vollständigen dpkg-Status, erforderliche Pakete, Versions- und Architekturangaben;
- Versionskennung, Debian-Archivschlüsseldatei und deren `Signed-By`-Verweis;
- keine persönlichen Ausgangskonten, gesperrte technische Passwort-/Gruppeneinträge;
- keine doppelten/traversierenden Archivnamen, OCI-Whiteouts, Spezialgeräte,
  Einträge unter Symlinks oder Hardlinks außerhalb regulärer Archivdateien.

Die TAR-Datei wird **nicht entpackt**. Der Importer ist kein allgemeiner sicherer
Extraktor und kein Paketinstaller. Das Ergebnis enthält die drei unveränderten
Originaldateien und `import-report.json` mit Paket-/Eigentümerinventar. Eine
Wiederholungsprüfung kontrolliert erneut die Bytes und den Bericht.
`VERIFIED_UPSTREAM_BASE_NOT_INSTALLED` meldet ausschließlich diesen Zustand.

Die echte Basisprüfung fand lokal statt. 13 zusätzliche Hosttests verwenden
kleine öffentliche Archivfixtures und prüfen unter anderem beschädigte Bytes,
falsche Architektur, aktive Linux-Credentials, fehlerhaften dpkg-Zustand,
unsichere Archivnamen, Größenlimits, Abbruch und Erhalt vorhandener Dateien.
Diese Tests starten kein Betriebssystem und prüfen keine Namespace-Isolation.

## Integrierte Runtime und nächste Schritte

Die gemeinsame [Softwaregeneration](generations.md) enthält den technischen
NSS-Eintrag `runtime` mit internem UID/GID 1000. Persönliche Identitäten und
Passwörter verwaltet ausschließlich AOSP. Die Basis ist für normale
Runtime-Prozesse schreibgeschützt; ein vorhandenes `apt` erteilt keine
Paketverwaltungsberechtigung.

[Prozessaufseher](process-supervisor.md), [Mount-Helfer](namespace-setup.md),
Broker und [SELinux-Anbindung](selinux-integration.md) sind im geprüften
`d44ccb33` aktiviert. Je Benutzer bestehen eigene User-, Mount-, PID-, IPC-,
UTS- und Netzwerk-Namespaces, private flüchtige Dateisysteme und persönliches
AOSP-CE-Home. Tatsächliche GNU-Prozesse laufen mit getrennten Hostkennungen,
null Capabilities, `NoNewPrivs=1` und Seccomp.

Die [AOSP-Speicherkoordination](aosp-storage-lifecycle.md) und
[Zugangsserialisierung](admission.md) binden Starts und Abbau an Benutzer-ID
und Seriennummer. Die vorhandenen AOSP-Speicherhooks warten vor ihren
Schlüsseloperationen auf den Ressourcenabbau. Normale Benutzerwechsel dürfen
Hintergrundkontexte erhalten; Logout verlangt bestätigten Stopp und CE-Sperre.
Der begrenzte Zwei-Benutzer-Nachweis umfasst tatsächlich geschriebene Dateien,
gegenseitige verweigerte Zugriffe, Bildschirmsperre und Neustart desselben
Android-/KeyMint-Paars. Vollständige Fehler- und Konkurrenzprüfungen bleiben offen.

Als nächste Arbeit bleiben der zuverlässige erste Terminal-Login (Korrektur
`2f29f0ac` besteht Komponenten-, noch keine vollständigen Diensttests),
verwaltete Benutzerlöschung vor Freigabe der AOSP-ID und gemeinsame/private
Paketgenerationen mit frischer AOSP-Adminprüfung für beide Bereiche. Bei einer
privaten Aktion bleibt der Zielbenutzer der authentifizierte Antragsteller,
auch wenn ein anderer Administrator zustimmt.

Die Original-APT-Quellen zeigen auf laufende Debian-Repositories. Das gepinnte
Ausgangsarchiv pinnt keine späteren Installationen. Jede zukünftige Transaktion
muss ausgewählte Versionen/Hashes, Abhängigkeiten, Datenbank, Konfiguration und
technische Konten konsistent verwalten. Bereits laufende Kontexte müssen ihren
bisherigen vollständigen Bestand behalten. Private Versionen dürfen durch ein
gemeinsames Update nicht still überschrieben werden. Diese Anforderungen sind
noch keine implementierte Paketverwaltung.
