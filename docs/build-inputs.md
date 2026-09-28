# Gepinnte Build-Eingaben über GitHub

Stand 28. September 2026: Download und Registrierung sind implementiert und
mit Datei-/Fehlerfalltests geprüft. Ein neuer Server-Build mit diesem Ablauf
steht noch aus.

Der Bootstrap `build.sh` löst den angeforderten Projektstand einmal auf einen
vollständigen Git-Commit auf. Der neue Downloader übernimmt daraus alle Dateien
unter diesen Verzeichnissen:

- `scripts/aosp/`
- `device/aegis/qemu_arm64/`
- `packages/aegis/identity/`
- `scripts/runtime/`
- `runtime/`

Damit reisen neben den bisherigen Produktdateien auch Java-/AIDL-Quellen,
Tests, verschachtelte Overlays und SELinux-Regeln sowie der Debian-Basis-Pin
und sein Importer über GitHub. Der Rootfs-Download selbst wird nicht in Git
abgelegt und noch nicht automatisch durch den AOSP-Worker ausgeführt.
Der bisherige Download nur dreier Make-Dateien konnte diese Erweiterungen
nicht transportieren. Nicht ausgewählte Repository-Verzeichnisse werden
nicht auf dem Server abgelegt.

`fetch-build-inputs.py` verlangt einen vollständigen Commit, einen vollständigen
GitHub-Dateibaum und reguläre Dateien. Jede heruntergeladene Datei wird anhand
von Länge und Git-Blob-ID gegen den Baum geprüft. Symbolische Links, Submodule,
Pfadtraversal und unvollständige Eingaben werden abgelehnt. Die Prüfsumme dient
der Zuordnung zum Git-Objekt; die Herkunft des Baums wird über die authentifizierte
TLS-Verbindung zu GitHub festgestellt. Pro Datei gelten derzeit maximal 8 MiB,
für die ausgewählte Quelle insgesamt 64 MiB.

Erst die vollständige, geprüfte Quelle erscheint unter
`/opt/aegis-builder/COMMIT`. Ein Manifest hält Commit und Dateiinventar fest.
Ein vorhandener Snapshot wird nur bei identischem Inhalt, passenden Zugriffsrechten
und Eigentümer wiederverwendet. Abweichungen werden erhalten und führen zum
Abbruch. Der Bootstrap hält die exklusive Einrichtungssperre während des Downloads.

Das Layout entspricht dem Projektcheckout; der Worker liegt jetzt unter
`scripts/aosp/worker.sh`. Der Compiler läuft weiterhin als `aegis-build`.
GitHub-Credentials werden für den Download verwendet und vor dem Compilerstart
aus der Umgebung entfernt. Das Transportverfahren verlagert keine Kompilierung
oder OS-Tests auf den Mac oder zu GitHub Actions.

Die Produktregistrierung kopiert jetzt den ganzen verwalteten Gerätebaum,
einschließlich Unterverzeichnissen. Sie migriert den bisherigen Drei-Dateien-Stand,
überschreibt aber keine fremden oder veränderten Quellen. Vorige Fassungen und
unvollständige Kopien liegen unter `out/` außerhalb der AOSP-Produktsuche. Ein
unterbrochenes Update der Eigentumsdatei führt zur Ablehnung bis zur Prüfung,
nicht zum stillen Überschreiben.

Der vollständige Build registriert außerdem die Identitätsquellen. Beide
Quellinventare werden im Laufverzeichnis abgelegt. **Eine registrierte Quelle
allein aktiviert den Systemdienst noch nicht.** Produktpakete, Systemserver-
Classpath, Dienststart und SELinux-Regeln sind inzwischen im Quelltext
eingebunden. Diese [Produktintegration](identity-cli.md#vorbereitete-produktintegration)
muss noch auf dem Builder kompiliert und in QEMU geprüft werden.
