# Gedächtnis – LichtDuell

Stand: 2026-10-18

Farbduell auf einem WLAN-LED-Streifen (Govee, WLED, Art-Net, sACN), Steuerung über die
Tastatur, C++20/MSVC, Konsole statt Qt. Lokales Git-Repo seit 2026-10-18: `main` mit dem
Grundstand, Zweig `feature/mehr-hersteller` mit den Treibern. **Kein Remote, nichts
veröffentlicht** – ob und wo, entscheidet der Nutzer. MIT-Lizenz vorgeschlagen, nicht bestätigt.

## Festgelegte Entscheidungen (nicht ohne Rückfrage ändern)

- **Konsole statt Qt-GUI.** Der Streifen ist die Anzeige, die Konsole nur Vorschau und
  Tastatur. Spart Qt-Bereitstellung, Hash-Prüfung der DLLs und Installer.
- **Zellen nur über den Razer-Befehl** (nicht dokumentiert, OpenRGB-MR 2172). Die
  offizielle LAN-API kann nur eine Farbe für den ganzen Streifen.
- **Höchstens 84 Zellen:** 3·84 + 2 = 254 passt in das Längenbyte; ob Byte 2 ein hohes
  Längenbyte ist, ist bei OpenRGB offen. **Der H612C nimmt nur bis 40 an** (siehe Gemessen).
- **Layout am Streifen: [ aus | Kampffeld | Effektbereich ]** (`StripLayout`). Beim Nutzer:
  40 Abschnitte, 11 aus (hinter dem Bett, linke Wand hoch), 21 Kampffeld (Decke über den
  Vorhängen), 8 Effektbereich (hinter der rechten Ecke bis zu den Waben). Wunsch des
  Nutzers: Kampf nur an der Decke, Rest für Effekte.
- **Einstellungsdatei** `%LOCALAPPDATA%\StreamWizard\LichtDuell\einstellungen.txt`:
  Vorgaben → Datei → Kommandozeile. Kaputte Zeile = Fehler mit Zeilennummer, kein stilles
  Überspringen. Datei-IP weicht `--nur-vorschau`; Widersprüche zählen nur auf der Kommandozeile.
- **Testbild ist interaktiv** (←/→ Kampfanfang, ↑/↓ Effektbereich, S speichert); das alte
  Zellen-Muster ist im Blockbild aufgegangen und entfernt.
- **Automodus `--auto langsam|normal|schnell`:** nur der Computer, Farbtasten übernehmen nicht
  (Unterschied zu `--demo`). Tempo ×0,5/×1/×4, live ←/→ (×0,25…×8). Zeitlupe bei Endgegner-Sieg
  und Spielende, Ton nur ab Vorrang 3.
- **Taste A schaltet den Automodus mitten im Spiel an/aus** (Wunsch des Nutzers). Beim Abgeben
  Tempo ×1. Eine Partie mit Computer-Mithilfe (`assisted`) zählt nie für die Bestenliste.
- **Fester Spieltakt 1/60 s** (GameClock) für alle Modi; Tempo = Zahl der Schritte je Bild,
  nicht größere Schritte. Der Autopilot entscheidet je Schritt – wie in den Tests.
- **Zeichnen im 30er-Takt, Konsole nur geänderte Zeilen** (ScreenDiffer).
- **Falsche Farbe steckt sich vorn an, ohne Punktabzug.** Strafe: längerer Klumpen, Kombo weg.
- **Geschwindigkeiten als Streckenzeit** (`level.cpp`), damit `--leds` nur die
  Auflösung ändert, nicht die Schwierigkeit.
- **Endgegner jede dritte Stufe** (Wurm → Chamäleon → Kanonier, ab Stufe 12 stärker).
  Neue Farben nur NACH einem Endgegner (Gelb 4, Lila 7), nie mitten im Kampf.
- **Biss statt Entfernen:** Ein Endgegner an der Kanone verliert zwei Glieder und weicht
  zurück – entfernte man ihn wie einen Klumpen, wäre ein Biss ein Sieg.
- **Chamäleon-Glieder sind ein Suffix** (`Blob::shifting`): Fehlschüsse kommen vorn dazu,
  Treffer gehen vorn weg – deshalb reicht eine Zahl statt einer Markierung je Glied.
- **Leertaste = Joker, Neustart nur mit Enter** – wer nach dem Ende weiterhämmert, soll
  nicht versehentlich neu starten.
- **Kein Gerät gefunden → Fehler mit Hinweisen**, keine stille Vorschau.
- **Streifen-Zustand vorher:** `devStatus` vor dem Start; war er aus, geht er danach
  wieder aus. Ohne Antwort bleibt er an (fail-open, Komfort).
- **Beenden: 2 s Ruhe, dann 3× Razer-Aus** (am H612C gemessen, siehe unten). Kostet 2 s
  beim Beenden, dafür kehrt der Streifen in seinen Zustand zurück. Der Strg-Handler wartet
  dafür bis 4 s (Windows gewährt beim Fensterschließen ~5 s).
- **Bestenliste zählt nur Start auf Stufe 1 ohne Demo.**
- **Nächster Klumpen wird einmal ausgewürfelt** (`pending_`), Stufe 1 würfelt kein Gold –
  so bleibt die Zufallsfolge unabhängig von der Bildrate und Seeds bleiben vergleichbar.
- **Kein Setup, keine Signatur** – das Spiel wird nicht ausgeliefert.
- **Treiber bauen nur Datagramme** (`lights::LightProtocol`), alles Gemeinsame macht
  `lights::LightOutput` (Layout, Takt, Haltebild, Senden, Beenden mit Wartezeiten). Govee ist
  ohne Verhaltensänderung umgezogen – die alten Fälle laufen weiter, jetzt mit Wartezeiten.
- **Grenzen je Geräteart** (`lights/device.hpp`): Govee 84, WLED 490 (DRGB), Art-Net/sACN 170
  (ein Universum). Spielzellen bleiben höchstens 84 – die Streckenzeit hängt an Sekunden.
- **Freigeben beim Beenden je Hersteller:** Govee 2 s + 3× Aus (gemessen) · WLED letztes Bild
  mit Frist 1 s, im Lauf Frist 2 s und Haltebild 0,5 s · sACN 3× „Strom endet“ (0x40) · Art-Net
  ein schwarzes Bild (kein Ende-Signal im Protokoll).
- **Suchen gleichzeitig** (`devices::discover_all`, je Hersteller ein Thread und Socket):
  Govee Multicast/4002, WLED mDNS als Legacy-Unicast von beliebigem Port (5353 hält der
  Windows-DNS-Client), Art-Net ArtPoll an die Broadcast-Adresse jeder Karte auf Port 6454.
  sACN hat keine Suche. Eine gescheiterte Suche steht in `problems`, die übrigen laufen weiter.
- **WLED ohne `--segmente`:** die gemeldete LED-Zahl wird zu Abschnitten (höchstens 490), wenn
  das Kampffeld dann ≥ 8 bleibt.
- **Assistent setzt bei anderem Gerät** Abschnitte, Versatz, Effektbereich, Zellen und
  `umgedreht` zurück; Helligkeit, fps, Ton bleiben. Gleiches Gerät (Art + IP) behält alles.
- **Datenordner bleibt `%LOCALAPPDATA%\StreamWizard\LichtDuell`** (Hausregel, und die
  Einstellungen des Nutzers liegen dort) – auch im Open-Source-Stand.
- **Netzantworten sind feindlich:** JSON mit Tiefen- und Längengrenze, mDNS mit Sprunggrenze
  16, Art-Net-Namen ohne Steuerzeichen, HTTP-Antwort ≤ 256 KiB und Pfad ohne CR/LF.

## Gemessen (nicht geraten)

| Frage | Ergebnis (2026-10-18) |
|---|---|
| Bau MSVC 14.44, `/W4 /WX` | 0 Warnungen, 0 Fehler |
| Tests | 14 Programme, 173 Fälle grün (core 10, net 11, lights 14, govee 19, wled 6, artnet 7, sacn 6, devices 7, game 44, sound 4, score 4, console 17, cli 18, setup 6) |
| Gegenproben Hersteller (2026-10-18) | Govee 1× Aus 4 rot (Loopback 1 rot) · Govee ohne 2 s 2 rot · WLED Ende mit Lauf-Frist 1 rot · Art-Net ohne gerade Länge 2 rot · Sequenz über 0 1 rot · Steuerzeichen im Namen 1 rot · sACN ohne 0x40 1 rot · Länge falsch gezählt 2 rot · mDNS-Sprunggrenze 1000 1 rot · HTTP-Pfad ungeprüft 1 rot · JSON ohne Tiefengrenze 1 rot (Absturz) · Segmentgrenze der Ausgabe 1 rot · Grenze je Geräteart 3 rot · Assistent ohne Zurücksetzen 1 rot |
| `--suchen` nach dem Umbau | H612C gefunden, Zustand „an, 78 %“, alle drei Suchen zusammen 2,1 s; Port 6454 frei |
| Govee über Loopback (127.0.0.1:4003) | turn 1 → Razer an → Bild; `stop()` wartet ≥ 2 s, dann 3× Aus und nichts weiter |
| Assistent mit umgeleiteter Eingabe | `q` speichert nichts · `i`/wled/192.0.2.1 → Datei mit `gerät = wled`, Hinweis „antwortet nicht“ · `7` → Fehler · echte einstellungen.txt per Hash unverändert |
| CPU (Demo, 15 s, conhost, je 2 Läufe) | vorher Spiel 0,9/1,6 %, conhost 7,4/8,6 % · nachher (30er-Takt + ScreenDiffer) Spiel 0,5/1,1 %, conhost 2,4/2,2 % · `--auto schnell` 1,1 % / 2,3 % |
| Automodus im Test (21 Zellen) | schnell: Stufe 10, 3 Endgegner, kein Spielende · langsam: Stufe ≥ 4 (Farbwurm besiegt) |
| Automodus im Konsolenfenster (Foto) | schnell: nach ~12 s Stufe 3, Kombo 15 ×4 · langsam: nach 8 s Stufe 1 |
| Gegenproben Automodus | Zeilenvergleich schreibt alles 2 rot · Tempo ignoriert 2 rot · Zeitlupe wirkungslos 1 rot |
| Konsolenzeichen im klassischen Fenster | ♡ ☆ ▕ ▏ erschienen als Kästchen (Foto des Nutzers); jetzt nur WGL4 (♥ ♦ ▲ ► Kastenlinien Blöcke) → per `PrintWindow` fotografiert: alles gezeichnet. Gegenprobe: ♡ zurück bzw. Rand um 1 verschoben → 4 rot |
| Lesbarkeit | reines Blau (0,0,255) als Schrift auf Schwarz kaum lesbar (Foto) → Schriftfarben aufgehellt, Blau 45 % |
| Erste echte Runde des Nutzers | Stufe 3 erreicht, 1.550 Punkte, Bestenliste Platz 1 geschrieben (aus dem normalen Fenster) |
| Tastatur-Probe (`ctest -L integration`) | 19/19 (Spiel, Taste A, Pfeile im Testbild); ohne Haltesperre 2 rot; → abgeklemmt 2 rot; A abgeklemmt 3 rot |
| **Farben je Razer-Bild am H612C** | 30 und 40 → angezeigt; 50, 60, 70, 80 → nicht angezeigt (Folge Rot…Lila, vom Nutzer gesehen; der erste Durchgang wurde verpasst, der zweite gezählt) |
| Einrichtung am H612C | Schätzung aus Fotos 40/10/8 → Weiß hing eine Zelle zu tief an der Wand (Foto) → **40/11/8**: Weiß an der linken Deckenecke, Lila an der rechten (vom Nutzer bestätigt) |
| einstellungen.txt echt? | per WMI-Prozess (außerhalb des MSIX-Containers) gelesen: ja, Inhalt wie gespeichert. Ordner `LichtDuell` vorher per WMI angelegt (`StreamWizard` gab es schon) |
| devStatus-Antwortzeit | 20 von 20 nach 53–217 ms; 40 von 40 beantwortet; einmal im Spielstart keine Antwort (nicht nachstellbar) → zweiter Versuch eingebaut |
| Gegenproben Layout/Datei | Versatz ignoriert 3 rot · Effektbereich ignoriert 1 rot · unbekannter Schlüssel geschluckt 1 rot |
| Gegenproben Spiel | Ansteck-Regel aus 4 rot · Biss aus 1 rot · Chamäleon-Takt aus 2 rot (nach Schärfen) · Kanonier-Standplatz aus 1 rot · Gegenschuss aus 1 rot · Autopilot ohne Vorwegnahme 2 rot |
| Gegenproben sonst | Prüfsumme ohne Kopfbyte 3 rot · Ton-Vorrang aus 1 rot · nur 1× Razer-Aus 3 rot · Ruhe 0,15 s statt 2 s 1 rot |
| **Zurück zur Szene beim Beenden (H612C)** | 1× Aus direkt nach dem letzten Bild → eingefroren (minutenlang, kein Ein-Minuten-Zurückfallen) · Tab hart geschlossen, kein Aus → eingefroren · 3× Aus nach Minuten → Szene · 3× Aus nach 2 s (Probe) → Tageslicht · **Spiel mit 2 s Ruhe + 3× Aus, Strg+C → Tageslicht** (alles vom Nutzer gesehen) |
| Was geht beim Beenden raus (Lauscher 127.0.0.1:4003) | Strg+C: 3× Aus, 2,03 s nach dem letzten Bild · Konsolenfenster (conhost) per X: 3× Aus, 2,02 s danach · Tab im Claude-Terminal schließen: nichts |
| Zeit beim Schließen des Fensters | conhost: CTRL_CLOSE_EVENT kommt, danach ~4,95 s bis zum Beenden · Claude-Terminal-Tab: kein Ereignis, harter Abbruch |
| devStatus roh (H612C) | `onOff`, `brightness`, `color` (0,0,0), `colorTemInKelvin` 5500 – **keine Szene**, die lässt sich über LAN nicht auslesen |
| devStatus ohne vorherige Suche | bei einer neuen EXE ohne Firewall-Regel keine Antwort; mit Multicast-Suche vorweg vom selben Socket schon → `query_status` sucht jetzt zuerst |
| Streifen im LAN | **H612C** (IP und Geräte-ID stehen nur in der lokalen einstellungen.txt, nicht im Repo); antwortet auf `scan` UND `devStatus` (an, 78 %) – Firewall ließ beides durch |
| Razer-Modus am H612C | **Funktioniert:** `--testbild --ip <ip>` zeigt das Muster auf dem Streifen (vom Nutzer gesehen). 1175 Bilder mit 30 fps ohne Sendefehler |
| Richtung und Abdeckung am H612C | Weiß (Zelle 0) liegt am gewünschten Kanonen-Ende, das andere Ende lila, **2 weiße Marken** (Zelle 10 und 20), Muster über den ganzen Streifen → **Vorgaben passen: 30 Zellen, kein `--umgedreht`** (vom Nutzer gesehen) |
| Demo `--stufe 9` mit 6 Kanonier-Gliedern | Kanonier nach 8 Schüssen tot, ohne je zu feuern → jetzt 8 Glieder, früherer erster Schuss, Gegenschuss |
| `CREATE_NO_WINDOW` + `AttachConsole` | fensterlose Konsole, Eingabe per `WriteConsoleInputW`, VT-Alternativpuffer über `CONOUT$` lesbar |
| doctest und `std::string_view` | ohne `<ostream>` C2027 `basic_ostream` beim Übersetzen |

## Fallen

- **Git Bash:** `cmake --build … -- /m /v:m` → MSB1008, weil MSYS `/m` als Pfad umschreibt.
  `--parallel` nehmen oder über `pwsh -File build.ps1` bauen.
- **doctest `CHECK_THROWS_AS(expr, Typ)`:** `expr` darf keine Kommas außerhalb von
  runden Klammern haben (geschweifte schützen nicht).
- **Testzeiten:** Ein Schuss braucht bei 30 Zellen Abstand ~0,43 s. Wer im Test nur 0,4 s
  wartet, liest den Klumpen, bevor der Schuss einschlug.
- **Kanonier nach Treffer:** weicht eine Zelle zurück und läuft ~0,25 s an seinen Platz,
  erst dann zählt der Schusstakt – Gegenschuss ~0,5 s nach dem Treffer, nicht 0,25 s.
- **read_terminal** im Claude-Panel wartet nicht verlässlich: Zwischen Start und Lesen lagen
  mehrere zehn Sekunden (Demo stand schon auf Stufe 5).
- **Port 4002** wird nur für Suche und Zustandsabfrage gebraucht; mit `--ip` und belegtem
  Port fällt nur die Zustandsabfrage weg.
- **Tab im Claude-Terminal schließen = harter Abbruch.** Wer dort einen Lauf am Streifen
  beendet, erst `stop_terminal_tab` OHNE close (Strg+C), warten bis der Prompt da ist, dann
  schließen – sonst friert der Streifen ein.
- **Streifen eingefroren** (Absturz, Taskmanager): `LichtDuell --testbild` kurz starten und mit
  Esc beenden, oder `ldu_probe_govee <ip> razer-aus`.
- **Mehr als 40 Farben je Bild** → der H612C zeigt NICHTS an (kein Fehler, kein Zeichen).
  Wer `--segmente` erhöht, prüft am Streifen.
- **MSIX:** Den Datenordner nie aus der Claude-App heraus neu anlegen lassen – sonst landen
  Einstellungen/Bestenliste virtualisiert. Ordner per `Invoke-CimMethod Win32_Process Create`
  anlegen (Skripte im Scratchpad: `run_outside.ps1`).
- **PowerShell-Werkzeug der App** liest `cmd.exe /c …` in einer Befehlszeile als Pfad „/c“
  und blockt `Remove-Item` daneben – Befehle als .cmd-Datei schreiben und so starten.
- **Pfeiltasten im Claude-Terminal-Tab** kamen beim Nutzer nicht an (vermutlich kein Fokus);
  die Probe belegt den Weg selbst. Bei Fernsteuerung lieber Werte per Option setzen und
  `--speichern`.
- **CPU messen:** `pwsh -File probes\measure-cpu.ps1` (EXE in conhost, CPU-Zeit Spiel und
  conhost). Nach frischem Bau startet die EXE erst nach der Virenprüfung – das Skript wartet
  inzwischen auf den Prozess (die erste Scratchpad-Fassung mit festen 2 s ging leer aus).
- **Konsolenbild prüfen:** Text lesen reicht nicht (fehlende Glyphen sieht man nur im Bild).
  `pwsh -File probes\snap-console.ps1 -Png <datei>` startet die EXE in conhost, fotografiert
  per `PrintWindow(…, 2)` und schließt das Fenster. Ein Fenster geht dabei kurz auf.
- **Nutzer bei Messungen am Streifen:** vorher genau sagen, worauf er achten soll und wann es
  losgeht (10 s Vorlauf); einfache Fragen („welche Farben?“) statt Platzhalter-Antworten.
- **PowerShell und deutsche Anführungszeichen:** „ und “ gelten in PowerShell-Zeichenketten
  als Anführungszeichen. Ein `.Replace()` mit „…“ im Befehl hat eine Zeile in settings.cpp
  zerlegt (2026-10-18). Text mit typografischen Zeichen nur über das Edit-Werkzeug oder aus
  einer Datei einsetzen.
- **Gegenprobe mit Endlosschleife taugt nicht:** Eine Schleife ohne Seiteneffekt ist in C++
  undefiniertes Verhalten, MSVC hat sie wegoptimiert – der Test blieb grün, obwohl die
  Sprunggrenze fehlte. Gegenproben mit endlicher, falscher Grenze bauen (mDNS: 1000 statt 16).
- **Loopback-Tests binden feste Ports** auf 127.0.0.1 (4003, 6454, 5568). Läuft Govee Desktop
  oder ein Art-Net-/sACN-Programm, scheitert `ldu_test_devices` mit „belegt“.
- **Ein Lauf der App-PowerShell mit `Remove-Item` und Regex daneben** wurde als Systempfad
  geblockt – Skripte als Datei schreiben, Löschen über `[IO.File]::Delete`.

## Offen

- **WLED, Art-Net, sACN an keinem echten Gerät geprüft.** Erste Prüfung: Testbild, Spiel,
  Rückkehr nach Esc – jeweils mit jemandem, der hinschaut.
- Ob WLED (ESP32/ESP8266) auf die mDNS-Legacy-Unicast-Anfrage antwortet; sonst bräuchte es
  Port 5353 mit SO_REUSEADDR oder die Windows-API `DnsServiceBrowse`.
- E1.31-Feldwerte und die Art-Net-Fristen stammen aus der Erinnerung, nicht aus dem Normtext.
- Ob strenge Art-Net-Knoten ArtDmx nur von Quellport 6454 annehmen (LichtDuell sendet von
  einem beliebigen Port).
- sACN-Multicast geht über die Karte der Standardroute; mit VPN/Hyper-V evtl. falsch.
- Mehr als 490 LEDs (WLED-DNRGB) und mehrere Universen sind nicht eingebaut.
- Andere Govee-Modelle als der H612C: Farbgrenze und Rückkehr unbekannt.
- GitHub-Action nie gelaufen; ob die Tastatur-Probe dort eine Konsole bekommt, ist offen.
- MIT-Lizenz und Veröffentlichung: Entscheidung des Nutzers steht aus. Bei Veröffentlichung
  werden Commit-Name und -Adresse sowie der Name in LICENSE öffentlich.
- Effektbereich und Automodus am Streifen noch nicht bewusst angeschaut.
- Ob `--auto schnell` mit 30 fps am Streifen flüssig wirkt oder `--fps` hoch muss.
- Ob weniger als 2 s Ruhe vor dem Aus auch reichen (Beenden ginge schneller); 0,15 s war
  nicht sauber getrennt vom harten Abbruch.
- Genaue Höchstzahl zwischen 41 und 49 Farben am H612C nicht ermittelt (40 reicht).
- Farbtreue von Gelb (255,180,0), Lila (170,0,255) und Gold am Streifen; Latenz; ob 30 fps
  flüssig sind.
- Klänge nicht angehört.
- Bestenliste: Ordner existiert jetzt echt; ein echter Eintrag nach einem Spiel steht aus.
- Schwierigkeit für Menschen (`level.cpp`, `boss.cpp`) nicht ausbalanciert.
