# Neue Build-Images für QEMU vorbereiten

Downloads und vorhandene Profile bleiben erhalten. Ein neuer Vollbuild bekommt
ein neues Ausgabeverzeichnis und anschließend ein neues, zusammengehöriges
Android-/KeyMint-Profil. Die Profilbindung darf nicht durch das Austauschen von
Images oder das Bearbeiten von Prüfsummen umgangen werden.

```sh
python3 scripts/fetch-release.py RELEASE_TAG downloads/RELEASE_TAG
python3 scripts/prepare-local-images.py downloads/RELEASE_TAG out/NEW_IMAGE_SET \
  --build-commit FULL_BUILD_COMMIT
```

Die Vorbereitung verlangt den vollständigen, ausdrücklich gewählten Buildcommit
und prüft alle Release-Assets vor und nach dem Entpacken. Die gesplitteten
XZ-Archivteile werden direkt gelesen. Zulässig sind begrenzte, reguläre Dateien
auf einer Ebene; Links, Geräte, Pfadausbrüche, doppelte Namen und fehlende
Boot-Images führen zum Fehler. Auch versteckte AOSP-Metadatendateien bleiben
gewöhnliche, nicht ausführbare Hostdateien. Archiv-Eigentümer, Set-ID-Rechte und
erweiterte Attribute werden nicht übernommen.

Wenn die Release-Nachweise einen eigenen Kernel enthalten, werden dessen
Eingabenbindung, die tatsächlichen Kernelbytes in `kernel` und `boot.img`,
Releasekennung und eingebettete Konfiguration geprüft. Bei einer enthaltenen
Debian-Basis wird der auf dem Builder erzeugte Partitionsnachweis an genau die
heruntergeladene `super.img` und den ausgewählten Basisnachweis gebunden.
Die lokalen Metadatenprüfungen ersetzen keine erneute Gast-Einbindung oder
Runtime-Ausführung. Ältere Releases ohne diese optionalen Eingaben erhalten
keine Aussage über einen neuen Kernel oder eine gemeinsame Basis.

Erfolg erzeugt `prepared.json` mit dem Status
`EXTRACTED_CHECKED_NOT_AVB_VERIFIED_NOT_BOOTED`, Dateigrößen, SHA-256 und
gewähltem Commit. Bei einem Fehler bleiben Diagnose-Dateien erhalten, aber
es gibt keinen Erfolgsnachweis; ein erneuter Versuch verlangt ein neues Ziel.

Danach folgen weiterhin getrennt:

1. AVB-Signaturen, Hashes und Verkettungen der unveränderten Images prüfen;
   die Bootparameter aus genau diesem Imagesatz berechnen.
2. Eine neue GPT-Datei mit Metadata, Misc und FRP erstellen.
3. Ein neues gepaartes Profil über `qemu-with-secure-env.py --create-profile`
   anlegen und beide lokalen QEMU-Gäste starten.
4. Bootabschluss, authentifiziertes ADB, SELinux, FBE, Module und die tatsächlichen
   AEGIS-Funktionen im Gast prüfen. Ein entpacktes Archiv bestätigt keinen Boot.

Die zehn Hosttests in `test_prepare_local_images.py` verwenden inerte Archive
und Metadaten. Sie prüfen unter anderem falsche Commits, beschädigte Assets,
Pfadausbrüche, fremde Kernel-/Partitionsnachweise und geänderte Manifestdateien.
Sie bauen und starten keine nativen Programme.

Am 28. September 2026 wurde zusätzlich der bereits veröffentlichte Release
`aosp-20260927T181835Z-38f4c95f-e6f263b2` tatsächlich verarbeitet:
74 Dateien mit insgesamt 3.359.874.486 Bytes. Kernel, `boot.img` und `super.img`
stimmen mit den zuvor verwendeten Originalen überein. Der Nachweis liegt lokal
unter `out/release-preparation-38f4c95f-2/prepared.json`. Dieser Versuch bestätigt
die Verarbeitung des bisherigen Releases; er ist kein Test des noch laufenden
neuen Vollbuilds und hat keine VM gestartet.
