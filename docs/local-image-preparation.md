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

Bei knappem Speicher auf APFS können unveränderte Blöcke eines vorhandenen
unveränderlichen Imagesatzes genutzt werden:

```sh
python3 scripts/prepare-local-images.py downloads/RELEASE_TAG out/NEW_IMAGE_SET \
  --build-commit FULL_BUILD_COMMIT --reuse-prepared out/OLD_IMAGE_SET
# Nach AVB-Prüfung und Erzeugung der neuen Metadata-/Misc-/FRP-Dateien:
python3 scripts/make-qemu-disk.py out/NEW_IMAGE_SET \
  --reuse-disk out/OLD_IMAGE_SET/android.raw
```

Die Ziele sind eigenständige Dateien mit anderen Inodes, keine Hardlinks.
`fclonefileat` öffnet die Quelle ausschließlich lesend; nachfolgende Änderungen
betreffen nur den neuen Klon. Symlinks, Spezialdateien und bestehende Ziele
werden abgelehnt. Ein ausdrücklich gewünschter Klon wird bei fehlender
Dateisystemunterstützung abgebrochen, ohne stillschweigend eine große Vollkopie
zu erzeugen. Aktive beschreibbare Profilplatten sind keine zulässigen Seeds;
verwendet werden die unveränderlichen Basisdateien außerhalb des Profilpaars.

Die Wiederverwendung vertraut keinen alten Imagebytes: Das neue Release wird
weiter vollständig geprüft und entpackt. Jeder Dateibereich wird mit dem neuen
Stream verglichen, Abweichungen werden geschrieben und die gesamte fertige
Datei erneut gehasht. Für die GPT-Platte gilt das auch für Sparse-Lücken,
Null-Füllungen, Partitionspadding und die GPT-Zwischenräume. Alte Restdaten
werden ausdrücklich genullt, alte Dateiende-Bytes abgeschnitten. Die GPT-GUIDs
werden neu erzeugt. `android.raw.json` protokolliert Größe, Gesamtprüfsumme und
geschriebene beziehungsweise identische Bytes; es ist kein Bootnachweis.

Danach folgen weiterhin getrennt:

1. AVB-Signaturen, Hashes und Verkettungen der unveränderten Images prüfen;
   die Bootparameter aus genau diesem Imagesatz berechnen.
2. Eine neue GPT-Datei mit Metadata, Misc und FRP erstellen.
3. Ein neues gepaartes Profil über `qemu-with-secure-env.py --create-profile`
   anlegen und beide lokalen QEMU-Gäste starten.
4. Bootabschluss, authentifiziertes ADB, SELinux, FBE, Module und die tatsächlichen
   AEGIS-Funktionen im Gast prüfen. Ein entpacktes Archiv bestätigt keinen Boot.

Die elf Hosttests in `test_prepare_local_images.py` verwenden inerte Archive
und Metadaten. Sie prüfen unter anderem falsche Commits, beschädigte Assets,
Pfadausbrüche, fremde Kernel-/Partitionsnachweise und geänderte Manifestdateien.
Sie bauen und starten keine nativen Programme.
Weitere neun Tests in `test_local_image_io.py` prüfen den tatsächlichen
macOS-Klon, getrennte Inodes, unveränderte Quellen, abgeschnittene Restdaten,
fehlerhafte Streams, GPT-Prüfsummen und Sparse-Expansion. Ein absichtlich
vollständig mit falschen Bytes gefüllter Seed muss dieselbe vollständige
Platte ergeben wie ein frischer Aufbau mit identischen Test-GUIDs. Die vier
Tests mit tatsächlichen APFS-Klonen laufen nur auf macOS; hier bestehen alle
20 Prüfungen ohne Auslassung.

Am 29. September wurde außerdem der echte, bereits geprüfte Imagesatz
`ebf3610` unabhängig geklont und vollständig neu zur GPT-Platte aufgebaut.
Alle 16.273.899.520 logischen Bytes wurden zurückgelesen. Sämtliche
Partitionsbereiche und Paddingbytes stimmen mit der bisherigen Basis überein;
nur die neuen GPT-GUIDs unterscheiden sich. Dafür wurden 33.792 Bytes neu
geschrieben. Die Quellen und bestehenden Profilpaare wurden nicht verändert.
Beleg: `out/local-image-cow-ebf3610/result.json`. Auch dieser Speicher-/Dateitest
ist kein zusätzlicher Gast-Bootnachweis.

Am 28. September 2026 wurde zusätzlich der bereits veröffentlichte Release
`aosp-20260927T181835Z-38f4c95f-e6f263b2` tatsächlich verarbeitet:
74 Dateien mit insgesamt 3.359.874.486 Bytes. Kernel, `boot.img` und `super.img`
stimmen mit den zuvor verwendeten Originalen überein. Der Nachweis liegt lokal
unter `out/release-preparation-38f4c95f-2/prepared.json`. Dieser Versuch bestätigt
die Verarbeitung des bisherigen Releases; er ist kein Test des noch laufenden
neuen Vollbuilds und hat keine VM gestartet.
