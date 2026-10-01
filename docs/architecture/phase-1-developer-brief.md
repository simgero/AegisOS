# AegisOS – Foundation / Phase 1

**Status: verbindlicher Entwicklerauftrag; noch kein Implementierungsnachweis.**

Dieser Auftrag beschreibt den zu entwickelnden Phase-1-Prototyp. Die Erstellung dieser Spezifikation implementiert weder AOSP-Integration noch Runtime, Verschlüsselung oder Paketverwaltung. Die Funktions- und Security-Tests in Abschnitt 17 sind Anforderungen an die spätere Implementierung; eine Prüfung dieses Dokuments ersetzt ihre Ausführung nicht.

Die zentrale Architekturentscheidung lautet: **AegisOS/AOSP ist die einzige Identity Authority.** AegisOS vermittelt zu AOSPs Benutzerverwaltung, Authentifizierung, Berechtigungen und Schlüsselverwaltung. Die Shared GNU/Linux Runtime erhält daraus isolierte Runtime-Kontexte und führt keine zusätzliche persönliche Benutzerverwaltung.

## 1. Projektidee

AegisOS soll langfristig ein einheitliches, datenschutz- und sicherheitsorientiertes Betriebssystem für folgende Geräteklassen werden:

- Smartphone
- Tablet
- ARM-Notebook
- Desktop

Das System baut auf AOSP (Android Open Source Project) auf und behält Android-Kompatibilität als festen Bestandteil. Zusätzlich integriert es eine Shared GNU/Linux Runtime, zunächst mit Software aus Debian oder Ubuntu. Damit sollen perspektivisch Android-Anwendungen und klassische Linux-Anwendungen auf demselben System ausgeführt werden können.

Debian/Ubuntu bezeichnet die Herkunft des GNU/Linux-Userspace, keine zusätzliche persönliche Benutzerverwaltung und kein zweites Hostbetriebssystem.

Die drei grundlegenden Prinzipien lauten:

**Security first. Privacy first. One OS across devices.**

Für Phase 1 wird keine eigene grafische Oberfläche entwickelt. Sämtliche neuen Funktionen werden zunächst über Terminal/CLI umgesetzt.

## 2. Ziel von Phase 1

Es soll ein reproduzierbarer AegisOS-Prototyp entstehen, der:

1. auf AOSP basiert,
2. in einer Entwicklungs-/VM-Umgebung bootet,
3. mehrere persönliche AOSP-Benutzer unterstützt,
4. sie über AOSP per Passwort authentifiziert,
5. benutzerspezifische verschlüsselte Datenbereiche besitzt,
6. eine Shared GNU/Linux Runtime bereitstellt,
7. pro Benutzer einen isolierten Runtime-Kontext ausführt,
8. persistente Daten, Einstellungen und private Pakete zwischen Benutzern trennt,
9. nach einem Neustart reproduzierbar funktioniert,
10. Benutzerwechsel mit zulässigem Hintergrundbetrieb von einer vollständigen Abmeldung unterscheidet,
11. Pakete mit AegisOS-Adminautorisierung wahlweise für alle Nutzer oder nur für den eigenen Kontext verwaltet.

Desktop, Launcher, grafischer Login und eigene grafische Apps sind nicht Bestandteil dieser Phase.

## 3. Entwicklungsumgebung

**Aktualisierung durch Nutzeranweisung, 1. Oktober 2026:** Entwicklung, Builds
und QEMU-Systemtests dürfen direkt auf dem Buildserver stattfinden; ARM64
ist weiterhin das verwendete Ziel. Code regelmäßig lokal committen. Keine
Build-Artefakte nach GitHub laden, außer für einen später ausdrücklich
vorgesehenen Mac-Test. Der [aktuelle Serverablauf](../server-development.md)
ersetzt für diese Arbeit die folgenden historischen Orts-/Transportvorgaben.
Die Produkt- und Sicherheitsanforderungen bleiben bestehen.

Für die Entwicklung gelten verbindlich folgende Regeln:

1. Entwicklung lokal in diesem Projekt auf dem Mac.
2. Boot- und Systemtests lokal in **QEMU auf dem Mac**.
3. AOSP-Builds auf dem per SSH-Alias **`aegis-build`** erreichbaren Server.
4. Synchronisation und Transport über **GitHub**: Quellcode über Git, Build-Artefakte und zugehörige Prüfsummen über GitHub Releases.

SSH dient zur Steuerung und Diagnose des Builders. Der Server bezieht den zu bauenden Projektstand von GitHub; der Mac bezieht die Build-Ergebnisse ebenfalls von GitHub. Der vollständige Build und Start müssen dokumentiert und reproduzierbar sein. Verwendeter Projekt-Commit, AOSP-Stand, Kernelkonfiguration und Runtime-Basis müssen eindeutig festgehalten werden.

Verbindliches Buildziel ist **`aegis_qemu_arm64-userdebug`**: ein eigenes ARM64-AOSP-Produkt für QEMU `virt` auf dem Mac, mit HVF als zu prüfender Beschleunigung und zunächst ADB sowie Entwicklungs-Shell als Zugang. Ausgangsbasis bleibt `android-16.0.0_r1` mit dem bestehenden Manifest-Pin. Produktintegration und passende QEMU-Startkonfiguration sind noch zu implementieren. Das vorhandene Buildskript verwendet bislang ein Android-Emulator-Produkt und setzt diese Entscheidung noch nicht um. Die konkreten Abnahmekriterien stehen im [Entwicklungsablauf](../development-workflow.md#verbindliches-qemu-buildziel).

Das langfristige Hardwareziel ist ARM64. Die Architektur darf keine unnötige Abhängigkeit von x86-64 erzeugen.

Die Zielumgebung muss die erforderlichen Namespaces, AOSP-File-Based-Encryption und eine durchsetzbare Sicherheitsrichtlinie unterstützen. Fehlende Isolations- oder Entsperrvoraussetzungen führen zu einem erklärten Startfehler. Ein schwächer isolierter Ersatzbetrieb erfüllt diesen Auftrag nicht.

Die VM des Entwicklungsaufbaus ist von der GNU/Linux-Integration zu unterscheiden: Innerhalb des gestarteten AOSP nutzt die Runtime dessen Kernel. Einschränkungen der virtuellen Hardware, insbesondere gegenüber TEE, Secure Element und Hardware Root of Trust, müssen dokumentiert werden.

## 4. Systemarchitektur

```text
AegisOS / AOSP
│
├── AOSP als einzige Identity Authority
│   ├── Benutzer und Adminberechtigungen
│   ├── Passwortauthentifizierung
│   ├── Schlüsselverwaltung und Storage-Entsperrung
│   └── Benutzer- und Sitzungslebenszyklus
│
├── Aegis Identity Layer / aegis CLI
│   └── Vermittlung zu AOSP und Steuerung der Runtime-Kontexte
│
├── Shared GNU/Linux Runtime
│   ├── gemeinsame Softwarebasis aus Debian/Ubuntu
│   ├── glibc, GNU-Werkzeuge, bash, apt
│   └── für normale Runtime-Prozesse schreibgeschützt
│
├── AOSP-Benutzer 10: Simeon (Beispiel)
│   ├── eigener Credential-Encrypted-Speicher
│   └── eigener Runtime-Kontext mit privaten Daten und Paketen
│
└── AOSP-Benutzer 11: Isabelle (Beispiel)
    ├── eigener Credential-Encrypted-Speicher
    └── eigener Runtime-Kontext mit privaten Daten und Paketen
```

AOSP bleibt Host- und Basisbetriebssystem einschließlich Android Framework, Android Runtime, Multi-User und Android-Sicherheitsmodell. Die Shared GNU/Linux Runtime verwendet denselben Linux-Kernel und geeignete Kernel-Isolation.

Der Aegis Identity Layer besitzt keine zweite Benutzer-, Passwort- oder Schlüsselautorität. Er übersetzt CLI-Aufträge in autorisierte AOSP-Aktionen und ordnet Runtime-Ressourcen dem jeweiligen AOSP-Benutzer zu. Technische Zuordnungen sind Verwaltungsmetadaten, keine zusätzlichen persönlichen Konten.

## 5. Benutzerverwaltung

Ein persönlicher AegisOS-Benutzer ist ein persönlicher AOSP-Benutzer. Anlegen, Auflisten, Authentifizieren, Berechtigungsprüfung und Entfernen erfolgen über AOSP.

```sh
aegis user add simeon
aegis user add isabelle
aegis user list
aegis login simeon
aegis switch isabelle
aegis logout
```

Die Namen sind für Menschen bestimmte Bezeichnungen. Die interne Zuordnung erfolgt über den AOSP-Benutzer und dessen Lebenszyklus, nicht über einen Dateipfad aus dem Anzeigenamen. Mehrdeutige Namensauflösung muss zurückgewiesen werden. Umbenennung darf weder einen neuen Runtime-Eigentümer erzeugen noch Daten eines anderen Benutzers zuordnen.

```text
AOSP-Benutzer
    └── AOSP userId
        └── Runtime-Kontext dieses Benutzers
            ├── Namespaces und Host-UID/GID-Zuordnung
            ├── persönlicher CE-Speicher
            └── Runtime- und Paketstatus
```

Die AOSP `userId` ist nicht die Linux-Prozess-UID. Benutzer 10 und 11 sind Beispielwerte, keine reservierten IDs der Implementierung.

Es werden keine persönlichen Konten für Simeon oder Isabelle innerhalb Debians/Ubuntus angelegt. Die Runtime erhält weder eigene persönliche Passwörter noch eine zweite Anmeldung über Linux-Loginmechanismen.

Ein generischer POSIX-Eintrag wie `runtime` mit UID/GID `1000` und Home `/home/user` darf für NSS-, `getpwuid()`- und Programmkompatibilität vorhanden sein. Technische Dienstkennungen sind ebenfalls zulässig. Solche Einträge berechtigen weder zu einer persönlichen Anmeldung noch zur Verwaltung von AegisOS-Benutzern. Auch Paketinstallationsskripte dürfen keine parallele persönliche Identity Authority einführen.

Persönliche Benutzer sind vom dauerhaft erforderlichen AOSP-Systembenutzer zu unterscheiden. Dieser ist kein persönliches Runtime-Konto und kein reguläres Logout-Ziel. Das Entfernen eines persönlichen Benutzers beendet seine Kontexte und entfernt dessen Schlüsselzugriff und privaten Runtime-Zustand. Eine später wiederverwendete `userId` darf keinen Zugriff auf alte Daten oder Zuordnungen erhalten.

## 6. Authentifizierung

Von Anfang an muss echte Passwortauthentifizierung über AOSP erfolgen. Der Aegis Identity Layer nutzt die zum gewählten AOSP-Stand passenden Authentifizierungs- und Entsperrpfade; die Runtime verifiziert keine persönlichen Passwörter.

Passwörter dürfen nicht im Klartext gespeichert oder in CLI-Argumenten, Shell-History und Logs übergeben werden. Abfragen erfolgen interaktiv ohne Echo. Es darf weder eine unabhängige Aegis-Passwortdatenbank noch eine persönliche Runtime-Passwortdatenbank entstehen.

Eine erfolgreiche AOSP-Authentifizierung ist mit der AOSP-Schlüsselverwaltung und der Entsperrung des persönlichen CE-Speichers verbunden. Ein falsches Passwort darf weder den Zielbenutzer entsperren noch dessen Runtime starten. Ein Benutzerwechsel ersetzt nicht die gegebenenfalls erforderliche Authentifizierung des Zielbenutzers.

Adminrechte werden von AOSP festgestellt und durch die AegisOS-Verwaltungsdienste für die angeforderte Aktion geprüft. Eine UID, eine behauptete Benutzerkennung oder `root` innerhalb eines Runtime-Kontextes gilt nicht als AegisOS-Adminautorisierung.

Die Architektur soll spätere zusätzliche Faktoren ermöglichen:

- biometrische Authentifizierung,
- Hardware Security Module / Secure Element,
- TPM/TEE,
- Recovery Keys,
- externe Security Keys.

Diese Erweiterungen werden an die zentrale Identitäts- und Schlüsselarchitektur angeschlossen; sie sind keine eigenständigen Login-Systeme der GNU/Linux Runtime.

## 7. Kryptographie

**Keine selbst entwickelte Kryptographie.** Es werden etablierte, öffentlich dokumentierte und breit analysierte Standards und Implementierungen verwendet.

Für die Passwortauthentifizierung und den verschlüsselten Benutzerspeicher ist AOSPs bestehende Sicherheits- und File-Based-Encryption-Infrastruktur maßgeblich. Der Auftrag schreibt keine konkurrierende Passwort-KDF oder eigene Master-Key-Hierarchie neben AOSP vor.

Die tatsächlich verwendeten Verfahren müssen vor Implementierung dokumentiert werden: Verschlüsselungsmodi und Schlüssellängen, Schutz der Anmeldeinformationen, Schlüsselableitung, Zufallsquellen, Hardwareabhängigkeiten und Grenzen des VM-Prototyps. Die von AOSP unterstützten FBE-Verfahren, beispielsweise AES-256-basierte Modi, werden anhand des Zielsystems ausgewählt.

Argon2id bleibt ein möglicher Kandidat für einen später separat begründeten Anwendungsfall, ersetzt aber nicht eigenmächtig AOSPs Authentifizierung. Kryptographische Domain Separation und sichere Zufallsquellen bleiben Anforderungen an zusätzliche Schlüsselbereiche. Solche Erweiterungen müssen etablierte Bibliotheken und unterstützte Schnittstellen verwenden.

Die Integration soll AOSP-Updates und einen begründeten Austausch unterstützter Verfahren ermöglichen. Ein AegisOS-eigenes Kryptographieframework ist nicht erforderlich.

## 8. Schlüsselarchitektur

Benutzerdaten werden nicht unmittelbar mit dem Passwort verschlüsselt. Erzeugung, Schutz und Lebenszyklus des zufälligen benutzerspezifischen Schlüsselmaterials bleiben bei AOSP.

Das folgende Diagramm beschreibt Zuständigkeiten, keine neue kryptographische Konstruktion:

```text
Persönliches AOSP-Passwort
    │
    ▼
AOSP-Authentifizierung / LockSettings
    │
    ▼
AOSP-geschütztes benutzerspezifisches Schlüsselmaterial
    │
    ▼
AOSP FBE / vold: Entsperrung des Benutzer-CE-Speichers
    │
    ├── persönliche Dateien und Anwendungsdaten
    ├── Runtime-HOME und Konfiguration
    └── privater Paketbestand und private Paketmetadaten
```

Die Runtime erhält nach autorisierter Entsperrung Zugriff auf die ihr zugeordneten Dateien, keine Kopie des persönlichen Passworts oder des CE-Master-Schlüsselmaterials. CE-Schlüssel werden nicht von AegisOS in eine zusätzliche Schlüsseldatei exportiert.

Ein Passwortwechsel erfolgt über AOSP und darf keine vollständige Neuverschlüsselung aller Benutzerdaten erfordern. Alte Anmeldeinformationen dürfen nach erfolgreichem Wechsel keine erneute Entsperrung ermöglichen.

Zukünftige Secrets-, Backup- oder E2E-Cloud-Schlüssel benötigen getrennte, dokumentierte Zwecke und kryptographische Domain Separation. Ihre spätere Einführung darf weder CE-Schlüssel als allgemeine Anwendungsschlüssel verwenden noch eine neue persönliche Identität erzeugen. Backup und E2E-Cloud werden in Phase 1 nicht implementiert.

Grundlage für FBE und Schlüsselverwaltung ist die [AOSP-Dokumentation zu File-Based Encryption](https://source.android.com/docs/security/features/encryption/file-based). Die konkrete Integration muss zum festgelegten AOSP-Stand passen.

## 9. Storage / Multi-User Encryption

Persönliche Dateien, HOME, Konfiguration, private Paketdateien und private Paketmetadaten liegen im Credential-Encrypted-Speicher (CE) des jeweiligen AOSP-Benutzers. Device-Encrypted-Speicher (DE) darf nur die notwendigen vor der persönlichen Entsperrung verfügbaren Systemdaten enthalten; er ist kein Ausweichort für persönliche Runtime-Daten oder Secrets.

```text
Gerät
├── Systemdaten und gemeinsame Runtime-Software
├── CE-Speicher von AOSP-Benutzer 10
│   └── persönliche Dateien, Runtime-Zustand und private Pakete
└── CE-Speicher von AOSP-Benutzer 11
    └── persönliche Dateien, Runtime-Zustand und private Pakete
```

Die Anmeldung eines Benutzers entsperrt nicht automatisch einen anderen Benutzer. Auch wenn mehrere Benutzer bereits entsperrt sind, müssen Dateiberechtigungen, Namespaces und AOSP-Sicherheitsrichtlinien den gegenseitigen Zugriff verhindern.

| Ereignis | Sitzung und Runtime | Persönlicher CE-Speicher |
| --- | --- | --- |
| Anmeldung / erforderliche Entsperrung | AOSP authentifiziert den Zielbenutzer; ein Runtime-Start ist anschließend zulässig. | Nur der autorisierte Zielbenutzer wird bei Bedarf entsperrt. |
| Benutzerwechsel | Der bisherige Kontext darf nach AOSP-Regeln im Hintergrund weiterlaufen; er behält seinen Eigentümer. | Kein garantierter Schlüsselentzug für den bisherigen Benutzer. |
| Bildschirmsperre | Hintergrundbetrieb bleibt nach AOSP-Regeln zulässig; die interaktive Nutzung wird gesperrt. | Keine Zusage, dass CE-Schlüssel allein dadurch entzogen werden. |
| `aegis linux stop` | Beendet nur den zugehörigen Runtime-Kontext. | Kein Logout und kein zugesicherter CE-Schlüsselentzug. |
| Ausdrückliche Abmeldung | Beendet die persönliche Sitzung einschließlich aller zugehörigen Runtime-Prozesse. | Unmittelbare Sperrung über AOSP; Abschluss muss bestätigt sein. |
| AOSP stoppt einen Hintergrundbenutzer | Dessen Runtime-Ressourcen werden ebenfalls abgebaut. | Tatsächlichen AOSP-Speicherstatus anzeigen; nicht aus dem Prozessstatus ableiten. |

AOSP kann Hintergrundbenutzer zur Ressourcenverwaltung stoppen. Weiterlaufende Hintergrundkontexte sind daher erlaubt, aber nicht unbegrenzt garantiert. Siehe [AOSP Multi-User](https://source.android.com/docs/devices/admin/multi-user).

Eine ausdrückliche Abmeldung muss folgende Bedingungen erfüllen:

1. Neue Starts und neue auf den Benutzer bezogene Verwaltungsoperationen werden gesperrt; laufende Paketoperationen werden geordnet abgeschlossen oder sicher abgebrochen.
2. Für einen aktuellen Vordergrundbenutzer erfolgt zuerst der erforderliche Wechsel in einen zulässigen System-/Anmeldekontext ohne Entsperrung einer anderen Person. Der nicht stoppbare Systembenutzer bleibt erhalten. Die dafür nötige CLI-/Systemsteuerung erfordert keine neue grafische Login-Oberfläche.
3. Die persönliche AOSP-Sitzung und alle zugeordneten Runtime-Prozesse werden beendet. Offene Dateizugriffe, IPC-Ressourcen, persönliche Mounts sowie flüchtige Verzeichnisse werden abgebaut.
4. Der unterstützte AOSP-Pfad zum Stoppen und unmittelbaren Sperren des Benutzer-CE-Speichers wird genutzt. Ein verzögerter Schlüsselentzug genügt nicht als erfolgreicher Logout. Ein konkurrierender Runtime-Neustart darf den Abschluss nicht umgehen.
5. Erst nach bestätigter Beendigung und Speicher-Sperrung darf die CLI „abgemeldet und Speicher gesperrt“ melden. Bei Fehler oder ausstehender Sperrung bleibt der Zustand sichtbar; eine erfolgreiche Abmeldung wird nicht behauptet. Nicht mehr benötigtes temporäres Geheimnismaterial wird freigegeben beziehungsweise über die zuständigen AOSP-Mechanismen entzogen.

Prozessende, Aushängen eines Verzeichnisses und eine Bildschirmsperre sind jeweils allein kein Nachweis des CE-Schlüsselentzugs. Die konkreten AOSP-APIs und Abschlussmeldungen sind gegen den verwendeten Branch zu prüfen; der [AOSP UserController](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/services/core/java/com/android/server/am/UserController.java) zeigt unter anderem die Trennung von Benutzerstopp und möglicher verzögerter Datensperrung.

## 10. Benutzer-Dateisystem

AOSP-interne Speicherpfade bleiben erhalten. AegisOS definiert darauf einen eindeutig zugeordneten persönlichen Speicher, der später über kontrollierte Schnittstellen sowohl von Android- als auch Linux-Anwendungen genutzt werden kann.

Die Runtime-Sicht ist für alle persönlichen Kontexte einheitlich:

```text
/home/user                    HOME des jeweiligen Runtime-Kontextes
├── Desktop
├── Documents
├── Downloads
├── Pictures
├── Videos
├── Music
├── Books
├── .config
├── .local
└── .cache
```

`HOME=/home/user` wird aus dem CE-Speicher des zugehörigen AOSP-Benutzers eingebunden. Derselbe sichtbare Pfad verweist in unterschiedlichen Kontexten auf unterschiedliche Daten. Persönliche persistente Zustände außerhalb von HOME müssen ebenfalls im zugeordneten CE-Speicher liegen.

Eine spätere menschenlesbare Darstellung wie `/Users/<Anzeigename>/` ist keine Speicheridentität und kein verbindlicher Hostpfad für Phase 1. Innerhalb der Runtime bleibt HOME unabhängig vom Anzeigenamen `/home/user`.

`/tmp` und `/run` sind private flüchtige Dateisysteme pro Kontext. Sie werden bei dessen Beendigung verworfen und beim nächsten Start neu erstellt. Session-Bus, Runtime-Sockets und vergleichbare Ressourcen dürfen nicht über gemeinsame beschreibbare Verzeichnisse andere Benutzer erreichen.

## 11. GNU/Linux-Integration

Die Shared GNU/Linux Runtime enthält mindestens:

- glibc,
- bash,
- apt einschließlich der benötigten Paketwerkzeuge,
- grundlegende GNU/Linux-Werkzeuge.

Beispiel für die geplante CLI; die Paketaktion wird durch AegisOS autorisiert:

```sh
aegis login simeon
aegis linux start
aegis linux status
aegis linux shell
id
uname -a
exit
aegis linux package install curl --scope user
aegis linux stop
aegis logout
```

Bereits Phase 1 bietet mit `aegis linux shell` einen interaktiven Shell-Zugang in den bestehenden autorisierten Kontext. Dort gelten `HOME=/home/user` und die generische interne Runtime-UID/GID. `exit` beendet nur diese Shell; der Runtime-Kontext und die AOSP-Sitzung bleiben bestehen. Fehlt ein gestarteter, entsperrter Kontext, wird der Shell-Zugang abgelehnt. Ein Anzeigename im Prompt begründet kein Debian-Benutzerkonto.

Die Runtime nutzt den Kernel des AOSP-Hosts. Eine zusätzliche vollständige VM ist für diese Integration nicht vorgesehen, sofern die Sicherheitsanforderungen mit Kernel-Isolation erfüllt werden können. Scheitern diese Voraussetzungen, muss dies als unerfüllte Anforderung behandelt werden; ein stiller Verzicht auf Isolation ist ausgeschlossen.

`apt` ist das Paketwerkzeug innerhalb der verwalteten Umgebung. Ein normaler Runtime-Prozess erhält daraus keine Berechtigung, die gemeinsame Basis oder den verwalteten privaten Paketbestand zu verändern. Schreibende Paketaktionen erfolgen über den autorisierten Verwaltungsweg aus Abschnitt 13 und 14.

## 12. Isolation der Linux-Umgebung

Jeder angemeldete persönliche AOSP-Benutzer kann einen eigenen Runtime-Kontext erhalten. Ein Kontext wird erst gestartet, wenn sein CE-Speicher entsperrt ist. Ein Wechsel bindet einen bestehenden Kontext niemals an die neue Person um.

Mindestens folgende Bereiche müssen getrennt sein:

- HOME, `.config`, `.local`, `.cache` und sonstiger persönlicher persistenter Zustand,
- private Paketdateien, Paketdatenbanken und Verwaltungsmetadaten,
- `/tmp`, `/run` und weitere Runtime-Verzeichnisse,
- Prozesse und die sichtbare Prozessliste,
- IPC, Session-D-Bus und private Sockets,
- Credentials und Secrets.

Jeder Kontext erhält eigene User-, Mount-, PID- und IPC-Namespaces. Innerhalb der persönlichen Kontexte verwendet der normale Runtime-Prozess jeweils UID/GID `1000`:

```text
AOSP-Benutzer 10                     AOSP-Benutzer 11
└── Runtime-Kontext A               └── Runtime-Kontext B
    ├── intern UID/GID 1000              ├── intern UID/GID 1000
    ├── eigener Host-UID/GID-Bereich     ├── anderer Host-UID/GID-Bereich
    └── /home/user → CE von User 10      └── /home/user → CE von User 11
```

Die von AegisOS verwalteten `uid_map`-/`gid_map`-Zuordnungen sind pro persönlichem Kontext nicht überlappend und müssen mit AOSPs UID-Zuteilung, Dateiberechtigungen und Sicherheitsrichtlinien vereinbar sein. Das gilt auch für zusätzliche technische Kennungen. Host-UID `1000` darf nicht als unübersetzte Runtime-UID verwendet werden: Sie ist in Android für `AID_SYSTEM` reserviert.

Eigene Mount-, PID- und IPC-Namespaces allein bilden keine unterschiedlichen Host-UIDs ab. User-Namespaces und Mapping sind zusätzliche Voraussetzungen. Fehlt eine Voraussetzung, schlägt der Start fehl. Grundlagen: [Linux-User-Namespaces](https://man7.org/linux/man-pages/man7/user_namespaces.7.html) und [AOSP Discretionary Access Control](https://source.android.com/docs/core/permissions/filesystem).

Die Isolation muss außerdem SELinux-Regeln, benötigte Capabilities, Gerätezugriffe, Netzwerkschnittstellen und erlaubte Host-Dienste berücksichtigen. Insbesondere dürfen Host-IPC, D-Bus, abstrakte Sockets oder gemeinsam sichtbare Ressourcen die Benutzertrennung nicht umgehen. Die genannten Namespaces allein begründen keine pauschale Zusage vollständiger Isolation.

Eine Änderung der persönlichen Einstellungen oder Pakete durch Simeon darf Isabelles persönlichen Zustand nicht verändern. Benutzer dürfen weder private temporäre Dateien noch private Prozesse oder IPC-Ressourcen anderer Benutzer einsehen oder beeinflussen.

## 13. Gemeinsame Linux-Software

Die Runtime trennt gemeinsame Software von persönlichem Zustand. Gemeinsame Basisbestandteile sollen nicht für jeden Benutzer vollständig dupliziert werden müssen. Für Phase 1 sind einfachere Speicherverfahren zulässig, solange beide Installationsbereiche, private Versionen und deren Sicherheits- und Konsistenzanforderungen erfüllt werden.

```text
Gemeinsame Runtime-Basis
für normale Runtime-Prozesse schreibgeschützt
    │
    ├── Kontext A: eigener konsistenter Paketbestand + private CE-Daten
    └── Kontext B: eigener konsistenter Paketbestand + private CE-Daten
```

### Installationsbereiche und Autorisierung

| Bereich | Wirkung | Berechtigung |
| --- | --- | --- |
| `--scope all` / „für alle Nutzer“ | Gemeinsame Pakete stehen bestehenden und künftig angelegten persönlichen Benutzern zur Verfügung. | AegisOS-Adminautorisierung über AOSP. |
| `--scope user` / „nur für mich“ | Private Pakete gelten ausschließlich im Kontext des anfordernden authentifizierten AOSP-Benutzers. | Ebenfalls AegisOS-Adminautorisierung über AOSP. |

Die Zielauswahl ist bei jeder Installation, Aktualisierung und Entfernung verpflichtend; es gibt keinen stillen Standardbereich. Fehlende Autorisierung oder ein fehlender Bereich führt zur Ablehnung vor einer Änderung.

Bei `user` bestimmt der vertrauenswürdige Verwaltungsdienst den Eigentümer aus dem authentifizierten Aufruferkontext. Eine Adminfreigabe ändert diesen Zielbenutzer nicht. Ein frei übergebener Dateipfad, eine Runtime-UID oder eine fremde Benutzerkennung darf die Zuordnung nicht ersetzen.

Die Runtime führt keine persönlichen Administrator- oder `sudo`-Konten ein. Erforderliche privilegierte Paketoperationen werden durch AegisOS kontrolliert und auf die jeweilige Aktion und deren Ziel begrenzt. Paketinstallationsskripte dürfen nicht unkontrolliert Hostrechte oder Zugriff auf andere CE-Bereiche erhalten.

### Private Versionen und Konsistenz

Private Installationen dürfen eine andere Version eines gemeinsam installierten Pakets einschließlich der dazu passenden Abhängigkeiten verwenden. Im persönlichen Kontext ist dann die private Version wirksam. Andere Kontexte verwenden weiterhin ihren jeweiligen Paketbestand.

Dateien, Paketdatenbank, Abhängigkeiten, Konfiguration, technische Dienstkennungen und Installationsskripte müssen für den tatsächlich ausgeführten Bestand zusammenpassen. Ein beschreibbares Overlay über einer veränderlichen gemeinsamen Basis ist allein kein Nachweis dieser Konsistenz. Die konkrete Speicher- und Transaktionsimplementierung muss diese Anforderungen nachweisen.

Persönliche Installation, Aktualisierung oder Entfernung darf weder gemeinsame Pakete noch andere persönliche Kontexte verändern. Das Entfernen einer privaten Variante kann die gemeinsame Variante wieder sichtbar machen; dieser Effekt muss vor der Aktion ausgewiesen und auf Konsistenz geprüft werden. Gemeinsame Paketänderungen dürfen private Daten und bewusst abweichende private Versionen nicht stillschweigend entfernen oder überschreiben.

Persönliche Einstellungen und Secrets bleiben auch bei gemeinsam installierter Software im jeweiligen CE-Speicher. Weder gemeinsame Paketmetadaten noch Installationsprotokolle dürfen private Geheimnisse aufnehmen.

### Aktivierung und Fehlerbehandlung

Gemeinsame Paketänderungen werden kontrolliert aktiviert, spätestens beim nächsten erfolgreichen Runtime-Start. Bereits laufende Kontexte behalten bis dahin einen konsistenten Paketbestand; Dateien oder Bibliotheken werden nicht währenddessen unkontrolliert ausgetauscht. Statusausgaben zeigen ausstehende Aktivierung an.

Vor dem Start eines Kontextes müssen gemeinsame Änderungen mit dessen privaten Paketen zu einem konsistenten Bestand zusammengeführt beziehungsweise aufgelöst sein. Eine private Version bleibt für ihren Kontext maßgeblich. Kann kein konsistenter Bestand hergestellt werden, wird der Start mit erklärtem Konflikt abgelehnt; eine vollständige Aktivierung wird nicht behauptet.

Pakettransaktionen müssen konkurrierende Operationen koordinieren. Fehlgeschlagene oder unterbrochene Aktionen dürfen keine teilweise aktivierte Umgebung als erfolgreich melden. Der letzte konsistente Bestand bleibt erhalten oder wird wiederhergestellt; Reparaturbedarf und Aktivierungsstatus bleiben sichtbar. Das gilt auch für Abmeldung während einer Paketoperation.

## 14. CLI

Das erste Kommandozeilenwerkzeug heißt `aegis`. Es umfasst mindestens:

```text
aegis user add <name>
aegis user list
aegis user remove <name>
aegis login <name>
aegis switch <name>
aegis passwd
aegis logout
aegis status
aegis linux start
aegis linux shell
aegis linux stop
aegis linux status
aegis linux package install <paket>[=<version>] --scope user|all
aegis linux package update --scope user|all
aegis linux package remove <paket> --scope user|all
```

`user|all` bedeutet in dieser Übersicht die Wahl genau eines Bereichs, keinen wörtlich zu übergebenden Wert. Die eckigen Klammern kennzeichnen eine optionale exakte Paketversion; ohne Versionsangabe erfolgt die Auswahl nach den eingerichteten Paketquellen und Abhängigkeitsregeln. Eine explizit angeforderte, nicht verfügbare oder nicht auflösbare Version führt zur Ablehnung statt zu einer stillen Ersetzung. `package update` aktualisiert Paketinformationen und installierte Pakete innerhalb des gewählten Bereichs über die kontrollierte Paketverwaltung.

Beispiel mit Platzhaltern für ein Testpaket und zwei in den festgehaltenen Paketquellen verfügbaren Versionen:

```text
aegis linux package install <paket>=<gemeinsame-version> --scope all
aegis linux package install <paket>=<private-version> --scope user
```

| Kommando | Verbindliche Bedeutung |
| --- | --- |
| `user add`, `user remove` | Autorisierte AOSP-Benutzerverwaltung einschließlich Schlüssel- und Runtime-Lebenszyklus; keine Debian-Kontoverwaltung. |
| `login` | Authentifiziert und aktiviert den Zielbenutzer über AOSP und entsperrt bei Bedarf seinen Speicher. Eine vorherige Sitzung wird dadurch nicht automatisch abgemeldet. |
| `switch` | Wechselt zum Zielbenutzer ohne automatische Abmeldung des bisherigen Benutzers; erforderliche Zielauthentifizierung erfolgt über AOSP. |
| `passwd` | Ändert das Passwort des authentifizierten Benutzers über AOSP nach dessen Authentifizierungsregeln. |
| `logout` | Führt den vollständigen und bestätigten Abmeldeablauf aus Abschnitt 9 aus. |
| `linux start` | Startet den Kontext des autorisierten Benutzers erst nach Prüfung von CE-Entsperrung und Isolationsvoraussetzungen. |
| `linux shell` | Öffnet eine Shell im bestehenden autorisierten Kontext; `exit` beendet ausschließlich diese Shell. |
| `linux stop` | Beendet diesen Runtime-Kontext einschließlich seiner Prozesse und flüchtigen Ressourcen; meldet den Benutzer nicht ab. |
| `linux package …` | Führt eine auf Bereich und AOSP-Berechtigung geprüfte Paketaktion aus. |

Der Aufruferkontext wird serverseitig geprüft. Bei einem Vordergrundwechsel dürfen bereits laufende Prozesse nicht über eine globale Einstellung „aktueller Benutzer“ die Identität der neuen Person erben. Die Kontrolle einer fremden Sitzung setzt die entsprechende AOSP-Berechtigung voraus.

`aegis status` unterscheidet Vordergrundbenutzer, zulässige Hintergrundsitzungen, Runtime-Zustände und tatsächlichen CE-Entsperrstatus. `aegis linux status` zeigt den zugeordneten Kontext, dessen Zustand und ausstehende Paketaktivierung. Sichtbarkeit fremder Statusinformationen richtet sich nach AOSP-Berechtigungen und darf keine privaten Prozesse, Pfade oder Secrets offenlegen.

Passwortabfragen sind interaktiv ohne Echo und dürfen nicht in Argumenten, History oder Logs erscheinen. Eine abgelehnte oder fehlgeschlagene Operation meldet keinen Erfolg und liefert einen Fehlerstatus. Insbesondere sind „Runtime gestoppt“, „Benutzer gestoppt“ und „Speicher gesperrt“ unterscheidbare Ergebnisse.

## 15. Security-Anforderungen

Security ist Bestandteil der Basisarchitektur. Es gelten insbesondere:

- keine Klartext-Passwörter und keine zweite persönliche Authentifizierungsquelle,
- keine eigenen Kryptographiealgorithmen oder fest eingebauten Secrets,
- keine unnötigen Root-Prozesse; Least Privilege auch für Verwaltungsdienste und Paketoperationen,
- Trennung von Systemdaten, gemeinsamer Software und persönlichen Daten,
- getrennte kryptographische Benutzerkontexte über AOSP,
- sichere Zufallsquellen, Dateiberechtigungen und durchsetzbare AOSP-/SELinux-Richtlinien,
- keine sensiblen Daten in Logs,
- möglichst kleine Trusted Computing Base,
- dokumentiertes Threat Model,
- Ablehnung von Runtime-Starts bei fehlender Isolation oder gesperrtem CE-Speicher,
- keine Ableitung von Host-Adminrechten aus Rechten innerhalb eines Runtime-Kontextes.

Sicherheitskritische Eigenimplementierungen werden vermieden, wenn AOSP/Linux etablierte Mechanismen bereitstellt. Namespace-Isolation ergänzt AOSPs Sicherheitsmodell; sie ersetzt weder dessen Berechtigungsprüfung noch die Storage-Verschlüsselung.

Adminfreigabe für gemeinsame Software ist eine Vertrauensentscheidung: Gemeinsame ausführbare Programme können das Verhalten anderer Kontexte beeinflussen. Das Threat Model muss diese Verwaltungsbefugnis von einem gewöhnlichen Benutzer und von einem kompromittierten Host unterscheiden. Daraus folgt kein pauschales Leserecht auf fremde CE-Daten.

## 16. Threat Model für Phase 1

Mindestens folgende Szenarien sind zu dokumentieren:

1. Ein Angreifer besitzt ein ausgeschaltetes Gerät oder dessen Storage-Image.
2. Ein Angreifer kennt das Passwort eines Benutzers und versucht, weitere Benutzer zu entsperren.
3. Benutzer A versucht auf Dateien, Konfiguration, private Pakete, Prozesse oder IPC von Benutzer B zuzugreifen; beide können bereits entsperrt sein.
4. Ein Linux-Prozess versucht, aus seinem Kontext auszubrechen oder Namespace-Rechte als Hostrechte zu verwenden.
5. Ein Benutzer ist vollständig abgemeldet, während ein anderer angemeldet ist.
6. Das Gerät wird neu gestartet; persönliche Daten dürfen nicht allein durch den Boot automatisch freigegeben werden.
7. Ein Benutzerpasswort wird geändert; das alte Passwort darf keine erneute Entsperrung ermöglichen.
8. Ein Benutzer wird gelöscht; alte Schlüsselzugriffe, Kontexte und gegebenenfalls später wiederverwendete IDs dürfen keine Daten freigeben.
9. Ein Benutzer wird in den Hintergrund gewechselt oder der Bildschirm gesperrt: CE-Daten können weiterhin für dessen eigene Hintergrundprozesse verfügbar sein, bleiben aber für andere Benutzer gesperrt.
10. Ein Logout, eine Speicher-Sperrung oder eine Paketoperation scheitert beziehungsweise konkurriert mit einem Start oder Benutzerwechsel.
11. Eine Paketoperation versucht, den gewählten Bereich, die AOSP-Adminprüfung oder die Eigentümerzuordnung zu umgehen.
12. Gemeinsame Updates treffen auf abweichende private Paketversionen oder konkurrierende Transaktionen.

Für jedes Szenario sind erwartete Schutzwirkung, Vertrauensannahmen, technische Nachweise und verbleibende Grenzen des VM-Prototyps anzugeben. Die Dokumentation darf aus einer Bildschirmsperre keine nachgewiesene Schlüsselentfernung und aus Namespaces keinen Schutz vor einem vollständig kompromittierten Host ableiten.

## 17. Tests

### Prüfung der Spezifikation

Die Dokumentationsprüfung kontrolliert die Vollständigkeit der 20 Abschnitte, konsistente Begriffe, auflösbare interne Links sowie die Übereinstimmung von Architektur, CLI-Beispielen, Threat Model und Definition of Done. Sie prüft insbesondere die alleinige AOSP-Identitätsautorität, einheitliches Runtime-HOME und die Trennung von Wechsel, Bildschirmsperre, Runtime-Stopp und Logout.

Diese Prüfung ist kein Funktions-, Verschlüsselungs- oder Isolationstest. Die nachstehenden Tests müssen später auf dem tatsächlichen AOSP-Prototyp implementiert und ausgeführt werden. Hier aufgeführte Szenarien gelten nicht durch ihre Aufnahme in das Dokument als bestanden.

### Automatisierte Funktions- und Security-Tests des Prototyps

| Bereich | Erforderliche Szenarien und Nachweise |
| --- | --- |
| Benutzer und Authentifizierung | Benutzer über AOSP erstellen und auflisten; falsches Passwort ablehnen; korrektes Passwort entsperrt nur den Zielbenutzer; keine persönlichen Debian-Konten oder parallelen Passwortspeicher; Passwörter erscheinen nicht in Dateien, Argumenten, History oder Logs. |
| Passwortwechsel | Änderung über AOSP; neues Passwort funktioniert nach erneuter Sperrung, altes Passwort nicht; persönliche Dateien bleiben ohne vollständige Neuverschlüsselung verfügbar. |
| Identität und Namespaces | Zwei persönliche Kontexte sehen intern UID/GID `1000`, besitzen aber getrennte Host-Zuordnungen sowie User-, Mount-, PID- und IPC-Namespaces; keine unübersetzte Verwendung von Host-UID `1000`. |
| Startbedingungen | Gesperrter Benutzer, fehlende Namespace-Unterstützung, unzulässiges Mapping oder fehlende Sicherheitsvoraussetzungen verhindern den Start ohne schwächeren Ersatzbetrieb. |
| Shell-Zugang | GNU/Linux-Befehle im eigenen gestarteten Kontext ausführen; HOME und UID/GID prüfen; fremden oder gesperrten Kontext ablehnen; `exit` beendet weder Runtime noch AOSP-Sitzung. |
| Gegenseitiger Zugriff | Benutzer A kann weder CE-Dateien, HOME, Konfiguration, private Pakete, temporäre Dateien noch private Prozesse, IPC und Secrets von Benutzer B lesen oder beeinflussen; auch bei zwei entsperrten Benutzern. |
| Persistenz | HOME, Einstellungen und private Pakete bleiben nach Runtime-Stopp und Neustart erhalten; Konfigurationen bleiben getrennt; `/tmp` und `/run` werden beim Kontextneustart neu erstellt. |
| Wechsel und Bildschirmsperre | Hintergrundbetrieb bleibt im Rahmen der AOSP-Regeln möglich; kein Identitätswechsel bestehender Prozesse; keine falsche Anzeige eines CE-Schlüsselentzugs; Ressourcestopp durch AOSP baut auch Runtime-Ressourcen ab. |
| Runtime-Stopp | `aegis linux stop` beendet ausschließlich den zugeordneten Kontext und meldet keinen vollständigen Benutzer-Logout. |
| Logout | Alle zugehörigen Runtime-Prozesse, offenen Zugriffe und persönlichen Mounts verschwinden; AOSP bestätigt die CE-Sperrung; Systembenutzer und andere Sitzungen bleiben erhalten. Vordergrundwechsel sowie erneute Starts während des Vorgangs testen. |
| Logout-Fehler | Fehlgeschlagene oder ausstehende Sperrung wird sichtbar; kein Erfolg allein aufgrund von Prozessende oder Unmount; Paketoperationen verhindern keine unbemerkte Restzugänglichkeit. |
| Reboot und Löschung | Benutzer und Daten überstehen einen Reboot; gesperrte Daten bleiben bis zur eigenen Entsperrung unzugänglich. Benutzerlöschung entfernt Schlüsselzugriff und privaten Zustand; eine neue oder wiederverwendete ID erhält keinen Zugriff auf frühere Daten. |
| Paketautorisierung | Installation, Aktualisierung und Entfernung jeweils für `user` und `all` mit erlaubter und verweigerter Adminautorisierung; fehlenden Bereich und manipulierte Eigentümerangaben ablehnen. |
| Paketbereiche | Gemeinsame Pakete sind für bestehende und neu angelegte Benutzer verfügbar; private Pakete und Änderungen bleiben auf den eigenen Kontext begrenzt; persönliche Konfiguration bleibt auch bei gemeinsamen Programmen getrennt. |
| Private Versionen | Zwei Benutzer können explizit ausgewählte unterschiedliche Versionen mit passenden Abhängigkeiten verwenden; nicht verfügbare oder nicht auflösbare Version ablehnen; private Variante gegenüber gemeinsamer Variante wirksam; private Entfernung einschließlich möglicher Rückkehr zur gemeinsamen Variante konsistent. |
| Paketaktivierung | Gemeinsames Update bei vorhandenem privatem Paketbestand; laufende Kontexte bleiben konsistent, neue Starts aktivieren den neuen gemeinsamen Stand oder melden einen Konflikt; keine stille Überschreibung privater Versionen. |
| Paketfehler und Parallelität | Gleichzeitige gemeinsame und private Aktionen sowie Aktionen verschiedener Benutzer; Abbruch, Installationsfehler und Logout während einer Transaktion; keine teilweise aktivierte Umgebung als Erfolg. |

Security-relevante Tests sind Bestandteil der Definition of Done. Ergebnisse müssen den getesteten AOSP-/Kernel-/Runtime-Stand und etwaige Einschränkungen erkennen lassen.

## 18. Noch NICHT implementieren

Phase 1 beinhaltet ausdrücklich nicht:

- grafischen Desktop,
- Launcher,
- grafischen Login,
- eigene Design-Sprache,
- App Store,
- Cloud-Synchronisation,
- E-Mail,
- Photos,
- Books als Anwendung,
- Password Manager,
- Smartphone-Telefonie,
- biometrischen Login,
- eigene Hardware,
- Android-App-Redesign.

Diese Bereiche folgen später. Der vorbereitete Ordner `Books` aus Abschnitt 10 ist keine Implementierung einer Books-Anwendung. Die CLI-Paketverwaltung aus Abschnitt 13 und 14 ist Teil von Phase 1.

## 19. Deliverables

Am Ende der implementierten Phase 1 werden erwartet:

1. vollständiger Source Code in Git,
2. reproduzierbare Build- und Startanleitung mit festgehaltenen Versionsständen,
3. AOSP-basiertes bootfähiges Development Image,
4. `aegis` CLI einschließlich Sitzungs-, Passwort- und Paketaktionen,
5. funktionierende zentrale Benutzerverwaltung über AOSP,
6. echte AOSP-Passwortauthentifizierung,
7. verschlüsselter benutzerspezifischer CE-Speicher,
8. Shared GNU/Linux Runtime,
9. isolierter Runtime-Kontext pro persönlichem Benutzer mit dokumentiertem Lebenszyklus,
10. automatisierte und ausgeführte Funktions- und Security-Tests samt Ergebnissen,
11. Architektur-Dokumentation einschließlich UID/GID-Mapping, Adminautorisierung und Paketkonsistenz,
12. Threat Model mit dokumentierten VM-Grenzen,
13. Dokumentation aller kryptographischen Entscheidungen und der verwendeten AOSP-Mechanismen,
14. Paketverwaltung für gemeinsame und private Installationen einschließlich abweichender privater Versionen.

Die hier vorliegende Spezifikation beschreibt diese Deliverables; sie belegt deren Implementierung oder erfolgreiche Abnahme noch nicht.

## 20. Definition of Done

Phase 1 gilt als erfolgreich, wenn auf einer frischen Entwicklungsumgebung der folgende Ablauf reproduzierbar demonstriert werden kann:

```text
AegisOS booten
→ persönlichen AOSP-Benutzer „simeon“ erstellen und Passwort setzen
→ persönlichen AOSP-Benutzer „isabelle“ erstellen und anderes Passwort setzen
→ als Simeon über AOSP anmelden und dessen CE-Speicher entsperren
→ Simeons Runtime-Kontext starten
→ über aegis linux shell eine Datei und persönliche Konfiguration unter /home/user erzeugen
→ Shell mit exit verlassen; Runtime und AOSP-Sitzung bleiben bestehen
→ mit AegisOS-Adminautorisierung ein Paket für alle Nutzer installieren
→ mit AegisOS-Adminautorisierung eine abweichende Version nur für Simeon installieren
→ kontrollierte Paketaktivierung einschließlich nötigem Runtime-Neustart prüfen
→ zu Isabelle wechseln, ohne Simeon automatisch abzumelden
→ erforderliche AOSP-Authentifizierung für Isabelle durchführen
→ Isabelles eigenen Runtime-Kontext starten
→ getrennte Host-Zuordnungen trotz gleicher interner UID/GID 1000 nachweisen
→ kein Zugriff auf Simeons private Daten, Prozesse oder private Pakete
→ gemeinsame Paketversion bei Isabelle und private Version bei Simeon nachweisen
→ Isabelles eigene Datei und Konfiguration erzeugen
→ Bildschirmsperre und zulässigen Hintergrundbetrieb getrennt vom Logout prüfen
→ Isabelle ausdrücklich abmelden
→ Prozessende, Ressourcenabbau und tatsächliche CE-Sperrung bestätigen
→ zu Simeon zurückkehren und eigene Daten wiederfinden
→ Simeon ausdrücklich abmelden und dessen CE-Sperrung bestätigen
→ System neu starten
→ persönliche Daten bleiben bis zur eigenen Entsperrung unzugänglich
→ Simeon erneut anmelden
→ eigene Dateien, Konfiguration und private Pakete wiederfinden
```

Zusätzlich müssen sämtliche verpflichtenden Tests aus Abschnitt 17 erfolgreich ausgeführt sein, insbesondere Passwortwechsel, Benutzerlöschung, verweigerte Adminautorisierung, gemeinsame Updates bei privaten Paketen und Fehlerfälle bei Logout und Pakettransaktionen. Ein weiterer neu angelegter Benutzer muss die gemeinsame Software erhalten, ohne auf bestehende persönliche Daten zuzugreifen.

Benutzertrennung, Speicher-Sperrung, Paketkonsistenz und Verschlüsselung müssen technisch nachvollziehbar und durch Ergebnisse auf dem tatsächlichen Prototyp belegt sein. Eine Dokumentationsprüfung allein erfüllt diese Definition of Done nicht.

## Langfristige Vision

Phase 1 soll keine Wegwerf-Demo sein. Sie bildet das Fundament für ein Betriebssystem, das langfristig denselben Kern auf Smartphone, Tablet, Notebook und Desktop verwenden kann.

AegisOS soll Android-App-Kompatibilität, klassische Linux-Anwendungen und ein eigenes E2E-verschlüsseltes Ökosystem miteinander verbinden. Die Architektur muss deshalb von Anfang an sicherheitsorientiert, modular, hardwareunabhängig und ARM64-fähig sein. Die zentrale AOSP-Identität bleibt auch bei späteren Oberflächen und Diensten die verbindliche Grundlage.
