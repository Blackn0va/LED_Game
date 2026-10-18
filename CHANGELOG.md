# Änderungsprotokoll

Alle nennenswerten Änderungen an LichtDuell. Format nach
[Keep a Changelog](https://keepachangelog.com/de/1.1.0/), Versionen nach SemVer.

## [Unveröffentlicht]

### Neu

- Farbduell auf dem Govee-Streifen: Computer schickt Klumpen, gleiche Farbe löst die
  vorderste auf, falsche steckt sich an; Leben, Stufen mit neuen Farben.
- Endgegner auf jeder dritten Stufe: Farbwurm (wird ab der Hälfte wütend), Chamäleon
  (wechselt im Takt die Farbe), Kanonier (schießt im Takt und als Gegenschuss); Biss an
  der Kanone, Feuerwerk beim Sieg, ab Stufe 12 stärker.
- Kombo-Multiplikator bis ×5, Joker-Regenbogenschuss (Leertaste), Goldpunkte, Extraleben.
- Demo mit Computerspieler (`--demo`), jederzeit per Taste übernehmbar.
- Startstufe (`--stufe`), Testbild zum Einrichten (`--testbild`).
- Klänge aus dem Speicher mit Vorrang (`--ohne-ton` schaltet ab).
- Bestenliste (Top 5) unter `%LOCALAPPDATA%\StreamWizard\LichtDuell`.
- Ausgabe über den Razer-Modus der Govee-LAN-API mit höchstens 30 fps, Haltebild alle
  2 s; beim Beenden Modus aus, und war der Streifen vorher aus, wieder aus.
- Gerätesuche per Multicast über jede aktive IPv4-Schnittstelle, mit Zustand
  (`--suchen`).
- Konsolenvorschau mit demselben Bild wie der Streifen (`--nur-vorschau`).
- Optionen `--ip`, `--leds`, `--fps`, `--helligkeit`, `--umgedreht`, `--seed`.
- Tastatur-Probe in echter Konsole (`ctest -L integration`), auch für die Pfeile im Testbild.
- Layout am Streifen: dunkler Anfang (`--versatz`), Kampffeld, Effektbereich
  (`--effektbereich`) mit Leben, Jokern, Kombo-Leiste, Endgegner-Lebensleiste und
  Ereignis-Effekten; Abschnitte frei wählbar (`--segmente`).
- Interaktives Testbild: Pfeile verschieben Kampfanfang und Effektbereich, S speichert.
- Einstellungsdatei `einstellungen.txt` (`--speichern`, `--ohne-einstellungen`).

- Automodus `--auto langsam|normal|schnell`: nur der Computer spielt, endlos, Tempo live
  mit ←/→, Zeitlupe bei Endgegner-Sieg und Spielende, Partienzähler mit bestem Lauf.
- Computerspieler mit Persönlichkeiten (Reaktionszeit, Fehlerquote, Joker-Mut).
- Taste A schaltet den Automodus mitten im Spiel an und aus.
- Weitere Hersteller: **WLED** (UDP-Echtzeit DRGB, bis 490 LEDs, Suche per mDNS, LED-Zahl
  aus `/json/info`), **Art-Net** (ArtDmx, Suche per ArtPoll) und **sACN/E1.31** (Multicast
  oder Unicast, dreimal „Strom endet“ beim Beenden) – `--gerät`, `--universum`. Nach der
  jeweiligen Beschreibung gebaut und ohne Gerät getestet, an echter Hardware noch nicht.
- Einrichtungs-Assistent `--einrichten`: sucht alle Hersteller gleichzeitig, nummerierte
  Auswahl oder Eingabe von Hand, speichert und startet das Testbild.
- `--einstellungen <datei>` für mehrere Streifen mit je eigener Einstellungsdatei.
- Open-Source-Grundlagen: MIT-Lizenz, CONTRIBUTING (neuen Gerätetreiber bauen), SECURITY,
  EditorConfig, GitHub-Action (windows-2022, Actions per Commit gepinnt), Issue-Vorlagen.

### Geändert

- Treiber-Schnittstelle `lights::LightProtocol`: Layout, Takt, Haltebild, Senden und
  Beenden macht `lights::LightOutput` für alle Hersteller; Govee zieht dorthin um, ohne sein
  Verhalten zu ändern (dieselben Pakete in derselben Reihenfolge, Wartezeiten jetzt im Test).
- `--suchen` listet Govee, WLED und Art-Net, gleichzeitig in 2 s statt nacheinander.
- `--segmente` richtet sich nach der Geräteart (Govee 84, WLED 490, Art-Net/sACN 170);
  Spielzellen bleiben bei höchstens 84.
- `einstellungen.txt` kennt `gerät` und `universum`; ältere Dateien ohne `gerät` gelten
  als Govee.

- Weniger Last: fester Spieltakt, Bild und Konsole im 30er-Takt, Konsole schreibt nur
  geänderte Zeilen – conhost von 7,4–8,6 % auf 2,2–2,4 % eines Kerns (gemessen).
- Neue Konsolenanzeige: Rahmen, großer zweizeiliger Streifen mit Effektbereich in derselben
  Zeile, farbige Meldungen, Hinweis auf den nächsten Endgegner, Fenstertitel „LichtDuell“.

### Behoben

- Kästchen statt Zeichen im klassischen Konsolenfenster (♡ ☆ ▕ ▏): nur noch WGL4-Zeichen.
- Blaue Schrift auf Schwarz kaum lesbar: Schriftfarben aufgehellt.
- Streifen blieb nach dem Beenden auf dem letzten Bild stehen: jetzt 2 s Ruhe und
  dreimal Razer-Aus (am H612C gemessen); der Strg-Handler gibt dafür bis zu 4 s.
- Zustandsabfrage ohne Firewall-Regel unbeantwortet: jetzt erst Multicast-Suche, dazu ein
  zweiter Versuch.
