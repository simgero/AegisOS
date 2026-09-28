# Gemeinsame Debian-Basis

Stand 28. September 2026: **Originalarchiv importiert und geprüft, noch keine
ausführbare AEGIS-Runtime.** Kein Debian-Programm wurde dabei ausgeführt und
kein Dateisystem auf dem Mac oder im Gast installiert.

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

Der erste [persönliche Prozessaufseher](process-supervisor.md) liegt inzwischen
als nativer Quelltext vor: eigene PTYs, UID/GID-Wechsel, Einsammeln beendeter
Prozesse und Kontextende bei Verlust des Kontrollkanals. Er ist noch nicht
kompiliert oder aktiviert. Der [statische Mount-Helfer](namespace-setup.md)
ist ebenfalls als ungeprüfter nativer Quelltext vorbereitet. AOSP-Broker,
praktischer Mount-/Rootwechsel-Nachweis und SELinux-Anbindung fehlen weiterhin;
daraus folgt noch keine ausführbare Runtime.

Die [vorgeschaltete AOSP-Speicherkoordination](aosp-storage-lifecycle.md)
ist ebenfalls als Quelltext vorbereitet. Sie definiert die bestätigte
Ressourcenfreigabe vor Schlüsseloperationen; der dafür erforderliche
Runtime-Controller fehlt noch. Der verwaltete Modus bleibt deaktiviert.

Die [Zugangsserialisierung](admission.md) ergänzt im Quelltext einen
Widerruf vor und nach destruktiven Speicheroperationen sowie eine
Seriennummernbindung. Der Adapter bleibt unregistriert, bis tatsächlicher
Ressourcenabbau und die übrigen Lebenszykluspfade integriert sind.

## Vor dem ersten Runtime-Start noch erforderlich

Das [Buildrezept für eine gemeinsame Softwaregeneration](generations.md)
ist jetzt vorbereitet. Eine reine Planung mit der echten Basis erfasst 78
Pakete und 3.271 Einträge einschließlich des technischen NSS-Kontos. Ein
ext4-Image wurde daraus bisher nicht erzeugt, veröffentlicht oder eingebunden.

1. Die [vorbereitete UID/GID-Zuordnung](uid-mapping.md) auf dem Builder gegen
   sämtliche Produktkennungen prüfen und im Gast einrichten. Alle 38 Kennungen
   der echten Basis sind abgedeckt; es gibt noch keinen Namespace oder Broker.
   Die 18 technischen Debian-Konten sind keine AEGIS-Benutzer. Ihre Quell-IDs und
   Archiv-Eigentümer dürfen nicht unverändert als Hostberechtigungen verwendet
   werden. Der normale Prozess soll intern UID/GID 1000 sehen, mit einer eigenen
   Hostkennung je AOSP-Benutzer. Ein generischer NSS-Eintrag für diese UID ist in
   der Originalbasis noch nicht vorhanden.
2. Auf dem Builder eine verwaltete, für normale Runtime-Prozesse schreibgeschützte
   Generation mit dem vorbereiteten Rezept erzeugen. Die Originalbasis enthält
   normale Debian-Set-ID-Dateien; das Rezept entfernt diese Rechte. Die Dateien
   erteilen keine AOSP-Adminberechtigung. `nosuid`, `no_new_privs`, Capability- und
   SELinux-Grenzen müssen vor dem Ausführen von Programmen durchgesetzt und
   getestet werden. Der Import allein setzt diese Grenzen nicht.
3. Den Kernel mit User-/Mount-/PID-/IPC-Unterstützung integrieren, pro Kontext
   private temporäre Dateisysteme und IPC-/Netzwerkgrenzen schaffen sowie den
   persönlichen CE-Speicher erst nach AOSP-Authentifizierung einbinden.
4. Runtime-Starts, Shell-Zugriff, Prozessende und Mountabbau an die bestehende
   AOSP-Identität mit `userId` **und** Seriennummer sowie deren Abmeldung binden.
5. Gemeinsame/private Paketgenerationen und deren Transaktionen mit jeweils
   frischer AOSP-Adminprüfung implementieren. Die Original-APT-Quellen zeigen
   auf laufende Debian-Repositories; ein gepinntes Ausgangsarchiv allein pinnt
   keine zukünftigen Installationen oder Updates. Jede Transaktion muss die
   tatsächlich ausgewählten Paketversionen/Hashes erfassen und konsistent aktivieren.
6. Erzeugte Artefakte mit Prüfsummen über AegisOS-GitHub-Releases transportieren
   und den vollständigen Ablauf mit zwei Benutzern in QEMU nachweisen.

Der AOSP-Quellsnapshot transportiert Pin, Importer, Generationsrezept und die
explizite Auswahl eines abgeschlossenen Basislaufs. Der Vollbuild kann die
geprüften Basisdateien dadurch in das Produkt aufnehmen und deren Nachweise
über GitHub transportieren. Dieser Ablauf ist noch nicht auf dem Server
ausgeführt und aktiviert keine Runtime. `ro.aegis.runtime.mode=absent` bleibt
bis zur echten Broker-/Mount-/Lebenszyklus-Integration zutreffend.
