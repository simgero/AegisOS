# Komponenten vom Builder zum lokalen QEMU

Der Komponentenbau und sein Export sind getrennte Schritte. Ein erfolgreicher
Komponentenbau erzeugt **kein bootfähiges Image** und führt keine Android-Tests
aus. Java- und native ARM64-Tests werden erst in Android-QEMU auf dem Mac
ausgeführt. Insbesondere dürfen die enthaltenen Framework-Dateien nicht als
einzelne Ersatzdateien in eine laufende VM kopiert werden.

## Auswahl und Paketierung

`scripts/aosp/components.py package` akzeptiert ausschließlich einen explizit
gewählten Lauf `identity-UTC-COMMIT8-ZUFALL6` mit:

- Status `IDENTITY_COMPILED_NOT_INSTALLED` und passendem vollständigem
  `project-commit.txt`;
- dem vollständigen Satz von 16 Komponenten des aktuellen System-Ext-Profils,
  passenden Installationsrechten und den beim Build erzeugten SHA-256-Werten;
- Quellinventar von Identitätsmodulen und Produkt sowie dem Nachweis der
  vorbereiteten AOSP-Speicher-Hooks.

Abgebrochene, unvollständige oder nachträglich veränderte Komponentensätze
werden abgewiesen. Die Paketierung kopiert nur diese benannten Dateien und
Metadaten, keine Logs, Zugangsdaten oder Signierschlüssel. Sie bewahrt das
Originalverzeichnis und dessen Buildstatus. Alte Läufe mit den früheren
`system/framework/aegis*.jar`-Pfaden gehören nicht zu diesem Transportprofil.

Ein neues Ausgabeverzeichnis erhält drei Dateien:

| Datei | Inhalt |
| --- | --- |
| `components.tar.gz` | 16 Komponenten unter `modules/`, sechs Nachweise unter `metadata/` |
| `components.json` | Build-Commit, Export-Werkzeug-Commit, Lauf, Dateipfade, Größen, Modi, SHA-256-Werte und Prüfungsumfang |
| `SHA256SUMS` | Prüfsummen der beiden anderen Release-Dateien |

Das Archiv verwendet feste Metadaten und Reihenfolge. Der Paketierer liest es
vor Abschluss wieder ein. Die Prüfung verlangt genau den vorgesehenen
Dateisatz, reguläre Dateien und begrenzte Größen; Links, zusätzliche,
doppelte, fehlende oder veränderte Einträge werden abgewiesen.
Der Umfang bleibt ausdrücklich `COMPILED_NOT_INSTALLED_OR_TESTED`.
Die Nachweise beschreiben den ausgewählten Builder-Lauf; sie sind keine
unabhängige Signatur oder Bestätigung eines erfolgreichen Gasttests.

## Veröffentlichung über GitHub

Auf `aegis-build` übernimmt `scripts/export-components.sh` den Export. Es wird
mit dem vollständigen Commit des Exportwerkzeugs, dem vollständigen
**Build-Commit** und der exakten Laufkennung aufgerufen:

```text
sudo bash export-components.sh --token-stdin EXPORT_COMMIT BUILD_COMMIT RUN_ID
```

Der GitHub-Token kommt nur über Standardeingabe aus `gh auth token`; er gehört
nicht in Argumente, Dateien oder Chatnachrichten. Vor der Pipeline muss die
interaktive sudo-Anmeldung im Benutzerterminal abgeschlossen sein. Das
Startkommando mit konkreten, über GitHub bezogenen Skriptversionen wird erst
für einen tatsächlich erfolgreichen Lauf vorbereitet.

Das Exportskript benötigt die vorhandenen Bootstrap-/Buildsperren und läuft
nicht gleichzeitig mit einem Build. Es bezieht das Paketierwerkzeug vom
gepinnten GitHub-Commit und führt es ohne GitHub-Zugangsdaten unter dem
vorhandenen Konto `aegis-build` aus. Es kompiliert nichts und ändert keine
Serverkonfiguration.

Der Release wird zunächst als Entwurf auf dem **Build-Commit** angelegt.
Alle drei Assets werden hochgeladen, wieder heruntergeladen und byteweise
verglichen. Auch das zurückgelesene Archiv wird vollständig geprüft. Erst
danach wird veröffentlicht; der Veröffentlichungsstatus, Tag und Ziel-Commit
werden nochmals von GitHub gelesen. Nur dieser vollständige Pfad setzt
`COMPONENTS_UPLOAD_VERIFIED` im neuen Exportlauf. Fehler behalten die vorhandenen
Ergebnisse zur Diagnose und behaupten keinen erfolgreichen Abschluss.

## Download und Entpacken auf dem Mac

Mit dem exakten veröffentlichten Tag und **Build-Commit**:

```sh
python3 scripts/fetch-release.py TAG downloads/TAG \
    --kind components --build-commit FULL_BUILD_COMMIT
```

Der Download lehnt Entwürfe ab und prüft sowohl die Release-Assets als auch
jedes Archivmitglied samt Bindung an den gewünschten Build-Commit. Er entpackt
und startet nichts. Für ein neues, noch nicht vorhandenes Zielverzeichnis:

```sh
python3 scripts/aosp/components.py unpack downloads/TAG \
    --build-commit FULL_BUILD_COMMIT --output out/COMPONENT_RUN
```

Beim Entpacken werden nur die festgelegten, erneut geprüften regulären Dateien
in einem temporären Verzeichnis angelegt. Erst ein vollständiger Erfolg macht
das Ziel verfügbar; vorhandene Ziele bleiben erhalten. Ausführungsrechte
werden für die vorgesehenen ARM64-Testprogramme wiederhergestellt, ohne sie
auf dem Mac auszuführen.

Die automatisierten Transporttests verwenden ausschließlich kleine, inerte
Dateien. Sie prüfen Paketierung, Entpacken, Fehlerzustände und den Exportablauf
mit einer lokalen GitHub-Fixture. Sie ersetzen weder einen echten erfolgreichen
Komponentenbuild noch den tatsächlichen GitHub-Upload oder Android-Gasttests.
