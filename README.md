<div align="center">

# LichtDuell

**Farbduell auf deinem WLAN-LED-Streifen – der Computer schießt, du schießt zurück, Endgegner inklusive. Nur mit der Tastatur.**

![C++20](https://img.shields.io/badge/C%2B%2B-20-8b5cf6) ![Windows](https://img.shields.io/badge/Windows-10%2F11-8b5cf6) ![Lizenz](https://img.shields.io/badge/Lizenz-MIT-8b5cf6) ![Tests](https://img.shields.io/badge/Tests-173%20gr%C3%BCn-2ea44f) ![Probe](https://img.shields.io/badge/Tastatur--Probe-19%2F19-2ea44f) ![Govee](https://img.shields.io/badge/Govee%20H612C-am%20Ger%C3%A4t%20gepr%C3%BCft-2ea44f) ![WLED](https://img.shields.io/badge/WLED%20%C2%B7%20Art--Net%20%C2%B7%20sACN-ungepr%C3%BCft-lightgrey)

[Geräte](#geräte) · [Schnellstart](#schnellstart) · [Spielen](#spielen) · [Automodus](#automodus) · [Spielregeln](#spielregeln) · [Endgegner](#endgegner) · [Effektbereich](#effektbereich) · [Einrichten](#einrichten) · [Optionen](#optionen) · [Bauen](#bauen) · [Wie es funktioniert](#wie-es-funktioniert) · [Fehlersuche](#fehlersuche) · [Mitmachen](#mitmachen) · [Stand](#stand)

</div>

![LichtDuell im Konsolenfenster – Demo mit Effektbereich](docs/bilder/demo.png)

![Stufe 3: der Farbwurm mit seiner Lebensleiste im Effektbereich](docs/bilder/endgegner.png)

*Klassisches Konsolenfenster, per `PrintWindow` fotografiert. Der breite Balken ist dasselbe Bild wie am Streifen: links die Kanone, dann das Kampffeld, rechts der Effektbereich.*

> **In English:** LichtDuell is a keyboard arcade game for addressable LED strips on your LAN. The computer fires coloured dots along the strip; you shoot the matching colour back to dissolve them – a wrong colour sticks and grows the blob. Three bosses, combos, jokers, an auto-play mode for ambient light shows. Drivers: **Govee** (LAN API, undocumented "razer" mode – tested on an H612C), **WLED** (UDP DRGB), **Art-Net** (ArtDmx) and **sACN/E1.31**. The last three are implemented from their specifications and covered by byte-level and loopback tests, but have **not been tried on real hardware yet** – reports welcome. Windows 10/11, C++20, MSVC, no runtime dependencies. UI and docs are German. `LichtDuell --einrichten` finds your devices and walks you through the setup.

## Geräte

| Gerät | Weg | Suche | Farben je Bild | Am Gerät geprüft |
|---|---|---|---|---|
| **Govee** WLAN-Streifen mit „LAN-Steuerung“ | nicht dokumentierter Razer-Modus der LAN-API, UDP 4003 | Multicast, Antwort auf 4002 | bis 84 (H612C: 40) | ✅ H612C – andere Modelle nicht |
| **WLED** (ESP8266/ESP32) | UDP-Echtzeit „DRGB“, Port 21324 | mDNS `_wled._tcp`, dann `/json/info` | bis 490 | ⏳ nein |
| **Art-Net**-Knoten (auch WLED, Pixel-Controller) | ArtDmx, ein Universum, UDP 6454 | ArtPoll-Broadcast | bis 170 | ⏳ nein |
| **sACN / E1.31**-Empfänger (auch WLED, xLights-Controller) | E1.31-Datenpakete, Multicast oder Unicast, UDP 5568 | keine (Empfänger melden sich nicht) | bis 170 | ⏳ nein |

„Ungeprüft“ heißt: Die Pakete sind nach der jeweiligen Beschreibung gebaut und Byte für Byte sowie über echte Sockets (127.0.0.1) getestet – an einem echten WLED-, Art-Net- oder sACN-Gerät lief LichtDuell noch nie. Wer eins hat: [Rückmeldung als Issue](.github/ISSUE_TEMPLATE/neues-geraet.yml) hilft sehr.

Andere Hersteller (Philips Hue, Nanoleaf, Tuya …) sind nicht eingebaut. Wie ein Treiber dazukommt, steht in [CONTRIBUTING.md](CONTRIBUTING.md).

## Schnellstart

1. Bauen (siehe [Bauen](#bauen)) – Ergebnis `out\rel\bin\Release\LichtDuell.exe`.
2. Govee: in der Govee-App beim Streifen **„LAN-Steuerung“** einschalten. WLED: nichts zu tun. Art-Net/sACN: im Empfänger das Protokoll und das Universum einstellen.
3. Einrichten – sucht alle Geräte, du wählst per Nummer, das Testbild hilft beim Ausrichten:

```bash
out\rel\bin\Release\LichtDuell.exe --einrichten
```

4. Spielen:

```bash
out\rel\bin\Release\LichtDuell.exe
```

Ohne Streifen, nur in der Konsole:

```bash
out\rel\bin\Release\LichtDuell.exe --nur-vorschau
```

## Spielen

Die Werte zum Streifen stehen nach dem Einrichten in `%LOCALAPPDATA%\StreamWizard\LichtDuell\einstellungen.txt`, ein nacktes `LichtDuell` reicht dann. Der Computer spielt vor, eine Farbtaste oder die Leertaste übernimmt:

```bash
out\rel\bin\Release\LichtDuell.exe --demo
```

Gleich zum ersten Endgegner:

```bash
out\rel\bin\Release\LichtDuell.exe --stufe 3
```

Beim Beenden mit **Esc** gibt LichtDuell das Gerät frei, und der Streifen kehrt in sein Licht von vorher zurück: Govee nach 2 s Ruhe und dreimal Razer-Aus, WLED nach 1 s, sACN sofort per „Strom endet“, Art-Net bekommt ein schwarzes Bild (Art-Net kennt kein Ende-Signal).

## Automodus

Nur der Computer spielt – endlos, Partie um Partie. **Taste `A` schaltet ihn jederzeit an und aus**, mitten in der Partie: an → der Computer übernimmt mit dem zuletzt gewählten Tempo; aus → du spielst an derselben Stelle weiter, das Tempo springt auf ×1.

```bash
out\rel\bin\Release\LichtDuell.exe --auto langsam
```

```bash
out\rel\bin\Release\LichtDuell.exe --auto schnell
```

| Modus | Tempo | Computerspieler | nach dem Spielende |
|---|---|---|---|
| `langsam` | ×0,5 | ruhig, überlegt lange, selten daneben – Lichtstimmung fürs Zimmer | 8 s Pause |
| `normal` | ×1 | wie die Demo | 6 s Pause |
| `schnell` | ×4 | Blitzreflexe, Joker ohne Zögern – Feuerwerk | 3 s Pause |

- **A** an/aus, **← / →** ändert das Tempo live in Stufen von ×0,25 bis ×8, **Enter** startet sofort eine neue Partie, **P** pausiert. Ohne `--auto` beginnt A mit „normal“ (×1).
- Eine Partie, in der der Computer mitgespielt hat, zählt nicht für die Bestenliste.
- **Zeitlupe:** Fällt ein Endgegner oder endet die Partie, läuft das Spiel 1,6 s mit Viertel-Tempo.
- Ton nur bei großen Momenten (Joker, Gold, neue Stufe, Endgegner, Spielende) – bei ×4 wäre jeder Schuss Dauerfeuer.

![Automodus schnell: Stufe 3 nach gut 10 Sekunden, der Farbwurm kündigt sich an](docs/bilder/auto-schnell.png)

## Spielregeln

- Der Computer schickt farbige Punkte los, ab Stufe 4 auch Klumpen aus zwei, ab Stufe 7 aus drei Farben.
- Du schießt mit einer Farbtaste zurück. Der Schuss trifft immer die **vorderste** Farbe des **vordersten** Klumpens:
  - **gleiche Farbe** → die Farbe verschwindet;
  - **falsche Farbe** → der Schuss **steckt sich vorn an**: der Klumpen wird länger, rückt eine Zelle näher, und deine falsche Farbe muss als Erstes wieder weg.
- Erreicht ein Klumpen die Kanone, ist ein Leben weg. Drei Leben zum Start, höchstens fünf.
- **Kombo:** Treffer in Folge ohne Fehlschuss. Ab 5 zählen Treffer doppelt, ab 10 dreifach … bis fünffach.
- **Joker** (Leertaste): Regenbogenschuss, passt auf jede Farbe. Einer zum Start, einer je 8er-Kombo, je Goldpunkt und je besiegtem Endgegner, höchstens drei.
- **Goldpunkt:** funkelt, ist schnell, jeder Schuss trifft – 100 × Stufe Punkte und ein Joker.
- Neue Farben kommen nach einem Endgegner: **Gelb ab Stufe 4**, **Lila ab Stufe 7**.

Punkte: 10 × Stufe × Kombo-Faktor je Treffer, 50 × Stufe je aufgelöstem Klumpen, 500 × Stufe je Endgegner. Die **Bestenliste** (Top 5) zählt Spiele ab Stufe 1 ohne Demo und ohne Automodus.

| Taste | Wirkung |
|---|---|
| `1` / `R` | Rot |
| `2` / `G` | Grün |
| `3` / `B` | Blau |
| `4` / `Y` | Gelb (ab Stufe 4) |
| `5` / `L` | Lila (ab Stufe 7) |
| `Leertaste` | Joker |
| `A` | Automodus an/aus |
| `← / →` | im Automodus: Tempo |
| `P` | Pause |
| `Enter` | neues Spiel nach dem Ende |
| `Esc` / `Strg+C` | beenden |

Ziffernblock 1–5 geht auch. Gehaltene Tasten feuern nur einmal.

## Endgegner

Jede dritte Stufe. Vorher blinkt das Computerende 2,5 s rot. Sieg: ein Leben, ein Joker, die nächste Stufe und ein Feuerwerk. Erreicht ein Endgegner die Kanone, **beißt** er: ein Leben weg, er verliert zwei Glieder und weicht sechs Zellen zurück.

| Stufe | Gegner | Was er tut |
|---|---|---|
| 3, 12, 21 | **Farbwurm** | Lange Kette aus Farbläufen, langsam. Ab der Hälfte wird er **wütend**: pulsiert schneller und rückt 1,7× so schnell vor. |
| 6, 15, 24 | **Chamäleon** | Alle Glieder wechseln gemeinsam im Takt die Farbe. Kurz vor dem Wechsel **flackert** er. |
| 9, 18, 27 | **Kanonier** | Bleibt bei 55 % des Felds stehen und schießt schnelle Einzelpunkte – im Takt und als **Gegenschuss auf jeden Treffer**. |

Ab Stufe 12 kommen alle drei stärker wieder.

## Effektbereich

Optional: Ein Stück am Computer-Ende gehört nicht zum Kampf, sondern zeigt, was los ist (`--effektbereich <n>`):

| Lage | Anzeige |
|---|---|
| normal | **Leben** rot · Lücke · **Joker** gold · Lücke · **Kombo-Leiste** hellblau bis zum nächsten Joker |
| Endgegner naht | ganzer Bereich blinkt rot |
| Endgegnerkampf | seine **Lebensleiste** in Orange |
| Ereignisse | neue Stufe: grüner Lauf · Leben weg: roter Blitz · Sieg: Regenbogen · Gold/Joker: Funkeln |
| Spielende | glimmt rot |

Ein Streifen muss nicht ganz mitspielen: `--versatz <n>` lässt Abschnitte am Kanonen-Ende dunkel. Beispiel aus dem ersten Aufbau: 40 Abschnitte, die ersten 11 laufen unsichtbar hinter einem Bett, 21 bilden das Kampffeld an der Decke, die letzten 8 hinter einer Zimmerecke sind der Effektbereich.

```
[ aus (versatz) ][ Kampffeld: Kanone ► … ◄ Computer ][ Effektbereich ]
```

## Einrichten

**Assistent** – sucht Govee, WLED und Art-Net gleichzeitig (2 s), listet sie nummeriert, speichert die Wahl und zeigt auf Wunsch das Testbild:

```bash
out\rel\bin\Release\LichtDuell.exe --einrichten
```

```
   1  Govee    192.168.1.40     H612C · Geräte-ID …
   2  WLED     192.168.1.61     Flur · 150 LEDs · 0.15.0
   s  sACN (E1.31) per Multicast – Empfänger melden sich nicht, Universum 1
   i  Gerät von Hand angeben (Art und IP)
   q  abbrechen
```

Bei WLED übernimmt der Assistent die gemeldete LED-Zahl als Abschnitte. Bei Art-Net nimmt er das Universum, das der Knoten für seinen ersten Ausgang meldet.

**Testbild** – zum Ausrichten von Versatz und Effektbereich, auch später jederzeit:

```bash
out\rel\bin\Release\LichtDuell.exe --testbild
```

- **Weiß (pulsiert)** = deine Kanone, **Lila** = Computer, **Grün/Blau** im Wechsel = Kampffeld (jede 10. Einheit weiß), **Orange** = Effektbereich, davor ist alles **aus**.
- **← / →** Kampf früher/später beginnen lassen · **↑ / ↓** Effektbereich größer/kleiner · **S** speichern · **Esc** beenden.
- Vorher ins Konsolenfenster klicken, sonst kommen die Tasten nicht an.

**Ohne Suche**, direkt per Kommandozeile – dann mit `--speichern` festhalten:

```bash
out\rel\bin\Release\LichtDuell.exe --gerät wled --ip 192.168.1.61 --speichern
```

```bash
out\rel\bin\Release\LichtDuell.exe --gerät artnet --ip 192.168.1.70 --universum 0 --segmente 170 --speichern
```

```bash
out\rel\bin\Release\LichtDuell.exe --gerät sacn --universum 1 --segmente 150 --speichern
```

**Mehrere Streifen:** je Streifen eine eigene Datei.

```bash
out\rel\bin\Release\LichtDuell.exe --einrichten --einstellungen C:\LichtDuell\flur.txt
```

```bash
out\rel\bin\Release\LichtDuell.exe --einstellungen C:\LichtDuell\flur.txt
```

Die Einstellungsdatei ist von Hand änderbar (`schlüssel = wert`, `#` für Kommentare); ein Tippfehler bricht mit Zeilennummer ab, statt still mit Vorgaben zu spielen. Schlüssel: `gerät`, `ip`, `universum`, `segmente`, `versatz`, `effektbereich`, `leds`, `umgedreht`, `helligkeit`, `fps`, `ton`. Fehlt `gerät`, gilt `govee`.

## Optionen

| Option | Wirkung | Vorgabe |
|---|---|---|
| `--einrichten` | Assistent: suchen, wählen, speichern, Testbild | – |
| `--gerät <art>` | `govee`, `wled`, `artnet` oder `sacn` (auch `e131`, `art-net`) | `govee` |
| `--ip <adresse>` | Gerät direkt ansprechen, keine Suche (sACN: Unicast statt Multicast) | aus der Datei, sonst Suche |
| `--universum <n>` | Art-Net 0–32767, sACN 1–63999 | Art-Net 0, sACN 1 |
| `--segmente <n>` | Farben je Bild ans Gerät: Govee bis 84 (**H612C: 40**), WLED bis 490, Art-Net/sACN bis 170 | WLED: seine LED-Zahl, sonst wie `--leds` |
| `--versatz <n>` | Abschnitte am Kanonen-Ende aus | 0 |
| `--effektbereich <n>` | Abschnitte am Computer-Ende für den Effektbereich | 0 |
| `--leds <8–84>` | Spielzellen; mit `--segmente` sonst so viele wie das Kampffeld (höchstens 84) | 30 |
| `--fps <5–60>` | Bilder je Sekunde an den Streifen | 30 |
| `--helligkeit <1–100>` | Helligkeit in Prozent (in Software gerechnet) | 100 |
| `--umgedreht` | Kanone am anderen Ende | aus |
| `--stufe <1–30>` | Startstufe (3/6/9: Endgegner) | 1 |
| `--demo` | der Computer spielt, eine Taste übernimmt | aus |
| `--auto <langsam\|normal\|schnell>` | nur der Computer spielt, endlos | aus |
| `--ohne-ton` | keine Klänge | Ton an |
| `--seed <zahl>` | feste Zufallsfolge | Uhrzeit |
| `--nur-vorschau` | ohne Streifen | aus |
| `--testbild` | Ausrichten (siehe oben) | – |
| `--suchen` | Govee, WLED und Art-Net auflisten (Govee mit Zustand) und beenden | – |
| `--speichern` | aktuelle Streifen-Werte in die Einstellungsdatei schreiben | – |
| `--einstellungen <datei>` | andere Einstellungsdatei lesen und schreiben | `%LOCALAPPDATA%\StreamWizard\LichtDuell\einstellungen.txt` |
| `--ohne-einstellungen` | Einstellungsdatei nicht lesen | – |

Reihenfolge: Vorgaben → Einstellungsdatei → Kommandozeile.

## Bauen

Voraussetzungen: Windows 10/11 x64, **Visual Studio 2022 oder Build Tools 2022** (MSVC, C++-Desktop), **CMake ≥ 3.30**, **PowerShell 7** (`pwsh`). Keine weiteren Abhängigkeiten – der Testrahmen doctest liegt SHA-256-gepinnt in `third_party/`.

```bash
pwsh -File build.ps1
```

Stufen check → configure → build → test. Ergebnis: `out\rel\bin\Release\LichtDuell.exe`. Bewusst ohne Setup und Signatur. Die Tastatur-Probe (startet die echte EXE in einer Konsole) läuft getrennt:

```bash
ctest --test-dir out/rel -C Release -L integration --output-on-failure
```

Die GitHub-Action in `.github/workflows/build.yml` macht dasselbe auf `windows-2022`.

## Wie es funktioniert

```
Tastatur ──► Konsole ──► Spiellogik ──► Kampffeld ─┐
   ▲                         │  └──► Effektbereich ─┤──► Layout [ aus | Feld | Effekte ] ──► Treiber ──► UDP ──► Gerät
Autopilot (--demo/--auto) ───┘                      │        Helligkeit, Spiegeln, ≤ 30 fps     Govee · WLED · Art-Net · sACN
                                    Konsolenbild ◄──┘──► Ton (PlaySound, im Speicher erzeugt)
```

- **Treiber** (`src/<hersteller>/driver.*`) bauen nur Datagramme – ohne Netz und ohne Uhr. Layout, Takt, Haltebild, Senden, Fehlerzählung und das Freigeben beim Beenden macht `lights::LightOutput` für alle gleich. Deshalb ist jeder Treiber ohne Gerät testbar.
- **Govee:** Die offizielle LAN-API (JSON über UDP, Suche per Multicast `239.255.255.250:4001`, Antworten an `4002`, Befehle an `4003`) kennt nur **eine** Farbe für den ganzen Streifen. Einzelne Abschnitte gehen über den nicht dokumentierten **`razer`-Befehl** (derselbe Weg wie Govee Desktop), Aufbau nach [OpenRGB-MR 2172](https://gitlab.com/CalcProgrammer1/OpenRGB/-/merge_requests/2172): `BB 00 <Länge> <Befehl> <Daten> <XOR>` als Base64 in `{"msg":{"cmd":"razer","data":{"pt":"…"}}}`. **Am H612C gemessen:** 30 und 40 Farben je Bild gehen, 50 bis 80 nicht; beim Beenden braucht er **2 s Ruhe und dreimal Razer-Aus**, sonst bleibt er auf dem letzten Bild stehen.
- **WLED:** „DRGB“ nach der WLED-Doku: `02 <Frist> R G B …`. Jedes Bild trägt eine Frist von 2 s – stirbt das Programm hart, kehrt WLED danach von selbst zu seinem Effekt zurück. Haltebild alle 0,5 s.
- **Art-Net:** ArtDmx an die IP des Knotens, Sequenz 1–255, gerade Kanalzahl; Suche per ArtPoll an die Broadcast-Adresse jeder Netzwerkkarte.
- **sACN:** E1.31-Datenpakete mit zufälliger Quellkennung je Start, Priorität 100; ohne IP an die Multicast-Gruppe `239.255.<hi>.<lo>` des Universums; beim Beenden dreimal „Strom endet“.
- **Spieltakt:** feste Schritte von 1/60 s, das Tempo bestimmt nur, wie viele je Bild – auch ×8 rechnet so genau wie ×1.
- **Sparsam zeichnen:** Bild, Streifen und Konsole im 30er-Takt; die Konsole bekommt nur geänderte Zeilen (gemessen: Konsolenlast von 7,4–8,6 % auf 2,2–2,4 % eines Kerns).

Module: `core` (Farben, Text, Dateien, JSON), `net` (UDP, Suche, HTTP, mDNS), `lights` (Gerätearten, Layout, Takt, Ausgabe), `govee`, `wled`, `artnet`, `sacn` (Treiber und Suche), `devices` (alle zusammen), `game` (Regeln, Endgegner, Autopilot, Testbild), `sound`, `score`, `console`, `cli` (Optionen, Einstellungsdatei), `setup` (Assistent), `app`.

## Fehlersuche

| Meldung / Verhalten | Ursache und Abhilfe |
|---|---|
| Govee bleibt nach einem Absturz auf dem letzten Bild stehen | Das Aus kam nicht an (Absturz, Taskmanager, Terminal hart geschlossen). `LichtDuell --testbild` starten und mit Esc beenden. |
| Govee zeigt nichts | `--segmente` zu hoch? Der H612C nimmt über 40 Farben keine Bilder an. |
| „Kein Govee-Gerät hat auf die Suche geantwortet“ | LAN-Steuerung in der App aus, anderes Netz, oder die Windows-Firewall blockt eingehend UDP 4002. `--ip` nehmen. |
| „UDP-Port 4002 ist belegt“ | Govee Desktop o. Ä. lauscht schon. Beenden; mit `--ip` fällt nur die Zustandsabfrage weg. |
| „Kein WLED-Gerät hat auf die mDNS-Suche geantwortet“ | Firewall, anderes Netz – oder die Firmware antwortet nicht auf die hier benutzte Unicast-Anfrage (ungeprüft). `--gerät wled --ip …` geht ohne Suche. |
| WLED zeigt nur einen Teil | Mehr als 490 LEDs gehen nicht über ein DRGB-Paket; `--segmente` auf die LED-Zahl setzen (der Assistent macht das). |
| WLED kehrt mitten im Spiel kurz zum Effekt zurück | WLAN verliert Pakete über mehr als 2 s. Näher an den Router oder `--fps` senken. |
| „UDP-Port 6454 ist belegt“ | Ein anderes Art-Net-Programm (Jinx, xLights, QLC+) läuft. Beenden oder `--ip` angeben. |
| sACN-Multicast kommt nicht an | Windows sendet Multicast über die Netzwerkkarte der Standardroute – mit VPN/Hyper-V kann das die falsche sein. `--ip` (Unicast) nehmen. |
| Farben vertauscht (Art-Net/sACN) | LichtDuell sendet RGB; GRB-Streifen im Empfänger umstellen. |
| „einstellungen.txt Zeile N: …“ | Tippfehler in der Datei – Zeile korrigieren oder neu einrichten. |
| Pfeiltasten im Testbild wirken nicht | Erst ins Konsolenfenster klicken. |
| Kein Ton | Ohne Audiogerät bleibt es still; `--ohne-ton` schaltet ihn bewusst ab. |

## Mitmachen

Fehler, neue Geräte, Rückmeldungen von echten WLED-/Art-Net-/sACN-Aufbauten: gern als Issue. Einen neuen Gerätetreiber bauen: [CONTRIBUTING.md](CONTRIBUTING.md). Sicherheitslücken bitte nicht öffentlich, sondern wie in [SECURITY.md](SECURITY.md) beschrieben melden.

Lizenz: [MIT](LICENSE). Fremdbestandteile: [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Stand

- ✅ Spiellogik, drei Endgegner, Kombo, Joker, Gold – jede Sonderregel mit Gegenprobe rot/grün
- ✅ Autopilot schlägt fehlerfrei alle drei Endgegner bis Stufe 10 (Test)
- ✅ Tastatur in echter Konsole inkl. Pfeiltasten im Testbild und Taste A: Probe 19/19
- ✅ **Govee H612C am Gerät:** Razer-Modus, 40 Abschnitte, Richtung, Versatz und Effektbereich gesehen; nach Esc kehrt das Licht von vorher zurück
- ✅ Treiber-Schnittstelle: Govee-Verhalten nach dem Umzug unverändert (dieselben Pakete in derselben Reihenfolge, jetzt auch die Wartezeiten 2,0/0,1/0,1 s geprüft); Suche findet den H612C weiterhin
- ✅ WLED, Art-Net, sACN: Pakete Byte für Byte nach der Beschreibung, alle vier Treiber über echte Sockets (Loopback); 16 Gegenproben rot/grün
- ✅ Assistent `--einrichten` mit umgeleiteter Eingabe durchgespielt (Abbruch, Eingabe von Hand, Unsinn)
- ⏳ **WLED, Art-Net, sACN an keinem echten Gerät geprüft**; ob WLED auf die mDNS-Unicast-Anfrage antwortet, ist offen
- ⏳ E1.31-Feldwerte aus der Erinnerung an die Norm, nicht gegen den Normtext oder ein Gerät geprüft
- ⏳ Andere Govee-Modelle als der H612C nicht geprüft (Farbgrenze kann abweichen)
- ⏳ GitHub-Action nicht gelaufen (kein Remote)
- ⏳ Klänge nicht angehört; Schwierigkeit für Menschen nicht ausbalanciert
- ⏳ Nur Windows; kein Linux/macOS
