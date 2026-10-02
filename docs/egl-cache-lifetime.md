# EGL-Cache: verzögerter Schreibthread beim Prozessende

Der gepinnte AOSP-Stand von `frameworks/native`
`2827a4a16b0340ecd07c2d5a6c89991799b362bb` enthält einen reproduzierten
Lebenszyklusfehler in `opengl/libs/EGL/egl_cache.cpp`.

`setBlob()` startet im monolithischen Modus einen abgekoppelten Thread. Dieser
wartet vier Sekunden und sperrt anschließend `mMutex`. Der Cache ist jedoch ein
statisches C++-Objekt; beim normalen Prozessende wird sein Mutex zerstört,
ohne auf diesen Thread zu warten. `eglTerminate()` beendet nicht den wartenden
Thread. Der Header beschreibt den Singleton dagegen bereits als niemals zerstört.

Der separate Gast-Regressionsprozess verwendet die unveränderte ausgelieferte
Bibliothek, legt ausschließlich einen synthetischen Cache-Eintrag an und ruft
`exit(0)` auf. Ein zuvor registrierter Exit-Beobachter hält den eigenen Prozess
sechs Sekunden am Leben. Der späte Thread trifft tatsächlich auf den zerstörten
Mutex bei `cache + 0x34`. **Der Prozess kann dabei trotzdem Exitcode 0 liefern.**
Die Prüfung verlangt deshalb die tatsächlichen Marker und prüft FORTIFY-Ausgaben.
Sie deaktiviert keine Mutexprüfung und ändert keinen Android-Dienst.

Beleg auf Image `d149766`, Boot `c85c6448-6bd3-4a70-b2fc-bdc53b6910dd`:
`out/phase1-dod/egl-cache-regression/baseline/result.json` und `process.log`,
Log-SHA-256 `c85afe3c03b0d1042d6d40a6af1a9a76a76dcd6401a63be8c427716b0aa855e4`.
Installiertes `libEGL.so`: SHA-256
`7a8118c82f2aea4910246a1434f32ab728fdd9093450870c6bc09f7469662464`.
Der ursprüngliche SystemServer bleibt PID 1146.

Die beiden ersten Hilfsprogrammversuche zählen nicht als erfolgreiche Prüfungen:
Ein Versuch scheiterte beim Linken des nicht exportierten `atexit`; ein zweiter
lief mit Toybox-`true`, ohne die Exit-Callbacks auszuführen. Die endgültige Probe
registriert über die Bionic-C++-ABI und ruft das normale libc-`exit` ausdrücklich
auf. Diese Ausgaben bleiben lokal erhalten.

## Korrektur und verbleibende Abnahme

Der Cache wird einmalig angelegt und für die gesamte Prozesslaufzeit gehalten.
Es gibt damit keinen konkurrierenden statischen Destruktor mehr; der Kernel gibt
den einen Cache beim Prozessende frei. `terminate()` schreibt den Cache weiterhin
und gibt seine Inhalte frei. Weil die neue Speicheranlage keine statische
Nullinitialisierung erhält, wird `mSavePending` ausdrücklich auf `false` gesetzt.
Der verzögerte Schreibthread und die reguläre Prüfung bleiben unverändert.

`register-egl-cache.py` bindet beide Quelldateien an Commit und SHA-256,
verweigert fremde Änderungen und wird vor/nach dem AOSP-Build ausgeführt.
Vier Hosttests prüfen diese Integration; neun Build-Snapshot-Tests bestehen.
**Die korrigierte Bibliothek ist noch im Gast zu prüfen und anschließend in
einem vollständigen Image mit Boot-/Bedienungsregression abzunehmen.**

Zeitabstand, geerbter Threadname und Mutex-Offset passen zum ursprünglichen
Bootanimation-FORTIFY. Für dessen ursprünglichen Prozess fehlt jedoch weiterhin
ein Backtrace beziehungsweise eine gespeicherte Bibliothekszuordnung. Die
gezielte EGL-Reproduktion ist belastbar; sie ersetzt nicht die noch nötige
Prüfung des korrigierten vollständigen Bootablaufs. D1 bleibt offen.

## Wiederholung auf dem Buildserver

```sh
sudo -n /srv/aegis/work/aosp/prebuilts/clang/host/linux-x86/clang-r547379/bin/clang \
  --target=aarch64-linux-android35 -fPIC -shared -nostdlib -fuse-ld=lld \
  -Wall -Wextra -Werror tests/fixtures/egl_cache_exit_probe.c \
  -o out/libegl_cache_exit_probe.so
python3 scripts/qemu-egl-cache-test.py \
  --probe out/libegl_cache_exit_probe.so \
  --output out/egl-cache-original --expect destroyed-mutex
```

Die Probe verwendet die internen exportierten Symbole des gepinnten AOSP-Stands.
Für den Komponentenvergleich kann `--library PATH_TO_FIXED_LIBEGL_SO` zusammen
mit einem neuen Ausgabeverzeichnis und `--expect clean` verwendet werden.
Nur der eigene Prüfprozess lädt diese Bibliothek; das installierte Systemimage
wird dabei nicht ersetzt. Binärdateien und Gastbelege bleiben lokal.
