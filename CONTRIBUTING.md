# Mitmachen bei LichtDuell

Danke fürs Reinschauen. Am meisten hilft gerade: **LichtDuell an einem echten WLED-, Art-Net- oder sACN-Gerät ausprobieren** und berichten, was der Streifen zeigt (Vorlage „Neues Gerät / Rückmeldung“). Diese drei Treiber sind nach ihrer Beschreibung gebaut und nur ohne Gerät getestet.

## Grundsätze

- **Belegen, nicht erinnern.** Eine Aussage über ein Gerät („nimmt 490 LEDs“, „kehrt nach 1 s zurück“) kommt erst in Code oder Doku, wenn sie gemessen oder in der Herstellerbeschreibung nachgelesen ist – und der Kommentar sagt, welches von beiden.
- **Gegenprobe.** Ein Test für eine Regel muss ohne die Regel rot werden. Im Pull Request beide Läufe zeigen: Regel aus → rot, Regel an → grün.
- **Kommentare tragen das Warum** (deutsch): was vorher war, warum es falsch war, was passiert, wenn jemand es zurückdreht. Nicht den Code nacherzählen.
- **Umfang halten.** Ein Pull Request, ein Thema. Kein Umformatieren nebenbei.
- **Kein `TODO`/`FIXME` im Code** – Offenes gehört in [Gedaechtnis.md](Gedaechtnis.md) unter „Offen“ oder in ein Issue.

## Bauen und testen

```bash
pwsh -File build.ps1
```

Alle Tests mit Label `unit` laufen ohne Netz und ohne Gerät (Loopback-Tests auf 127.0.0.1 zählen dazu). Die Tastatur-Probe startet die echte EXE:

```bash
ctest --test-dir out/rel -C Release -L integration --output-on-failure
```

Tests sind doctest-Fälle mit deutschen Sätzen als Namen („WLED-Treiber: Haltebild alle 0,5 s – deutlich unter der Frist von 2 s“). Erwartete Bytes stehen als Zahlen im Test, von Hand nach der Beschreibung gerechnet – ein Test, der die Konstanten des Codes benutzt, prüft nur sich selbst.

## Code-Stil (kurz)

- C++20, MSVC, `/W4 /WX` für eigenen Code. 4 Leerzeichen, ~120 Spalten, angehängte Klammern, Klammern auch bei Einzeilern.
- Dateien `snake_case.hpp/.cpp`; jeder Header beginnt mit einem Kopfkommentar (Zweck, Warum, dann `Threads: …` und `Fehler: …`), danach `#pragma once`.
- Typen `PascalCase`, Funktionen und Variablen `snake_case`, private Member mit `_` am Ende, Konstanten `inline constexpr kName`, `enum class`.
- Fehler als `ldu::Error` mit deutschem Nutzertext und `Kind`. Sendefehler werfen nicht (ein verlorenes UDP-Bild beendet kein Spiel); wo bewusst fail-open, steht es im Kommentar.
- Daten aus dem Netz sind nicht vertrauenswürdig: Längen prüfen, Schleifen begrenzen, Text vor der Konsolenausgabe von Steuerzeichen befreien.
- UTF-8 ohne BOM und LF; Ausnahme `*.ps1` (UTF-8 **mit** BOM, CRLF). Siehe `.editorconfig`.

## Einen neuen Gerätetreiber bauen

Beispiel: ein Hersteller „Funkel“ mit UDP-Protokoll.

### 1. Geräteart anmelden – `src/lights/device.hpp/.cpp`

`DeviceType::funkel` ergänzen, in `kDeviceTypes` aufnehmen, und in `device_key` („funkel“ – Schlüssel für `--gerät` und die Einstellungsdatei), `device_name` („Funkel“), `max_units` (höchstens so viele Farben je Bild – mit Quelle im Kopfkommentar) und bei Bedarf `uses_universe` einen Fall schreiben.

### 2. Modul anlegen – `src/funkel/`

```
src/funkel/CMakeLists.txt   ldu_add_module(funkel SOURCES protocol.cpp driver.cpp [discovery.cpp]
                                           PUBLIC_DEPS ldu::core ldu::net ldu::lights)
src/funkel/protocol.hpp     Pakete bauen und Antworten lesen – reine Funktionen, kein Netz
src/funkel/driver.hpp       class FunkelProtocol final : public lights::LightProtocol
src/funkel/discovery.hpp    optional: Geräte im LAN finden (über net::DiscoverySocket)
```

Das Modul in `CMakeLists.txt` in `LDU_MODULES` eintragen (vor `devices`).

### 3. Den Treiber schreiben – `lights::LightProtocol`

Der Treiber baut **nur** Datagramme. Netz, Uhr, Layout, Helligkeit, Spiegeln, Drosselung und Fehlerzählung übernimmt `lights::LightOutput`.

| Methode | Aufgabe |
|---|---|
| `name()` | Anzeigename für die Statuszeile |
| `max_units()` | höchstens so viele Farben je Bild |
| `keepalive_s()` | ein unverändertes Bild spätestens nach so vielen Sekunden erneut senden – sicher unter der Frist, nach der das Gerät den Echtzeitmodus verlässt |
| `start(now_s)` | Datagramme beim Start (etwa „Echtzeitmodus an“), darf leer sein |
| `frame(leds, now_s)` | Datagramme für ein Bild; `leds` hat genau die Einheiten des Layouts, schon gedimmt und gespiegelt |
| `stop(last)` | Schritte zum Freigeben: `StopStep{wait_s, packet}` – erst warten, dann senden. Ziel: Das Gerät kehrt in seinen Zustand von vorher zurück |

Für die Rückkehr gibt es je nach Protokoll verschiedene Wege – Govee braucht Ruhe und dreimal Aus (gemessen), WLED eine kurze Frist im letzten Paket, sACN dreimal „Strom endet“, Art-Net ein schwarzes Bild. Was dein Gerät braucht, steht in seiner Beschreibung oder muss gemessen werden. Wer misst: Ergebnis mit Datum in den Kopfkommentar und in `probes/README.md`.

### 4. Anschließen – `src/devices/connect.cpp`

Je ein Fall in

- `search()` – Suche aufrufen und `Found` füllen (oder `break`, wenn das Protokoll keine Suche kennt),
- `resolve()` – Ziel aus `--ip` oder Suche; was das Gerät selbst meldet (LED-Zahl, Port), in `Target`,
- `open_output()` – Treiber bauen und den UDP-Zielport setzen,
- `no_device_hint()` – was tun, wenn nichts antwortet.

Soll die Suche in `--suchen` und `--einrichten` mitlaufen, die Geräteart in `discover_all()` aufnehmen. Jede Suche braucht ihren eigenen Socket und Port – sie laufen gleichzeitig.

### 5. Tests – `tests/funkel/`

```
tests/funkel/CMakeLists.txt   ldu_add_test(ldu_test_funkel SOURCES funkel_test.cpp LIBS ldu::funkel)
```

Mindestens:

- Paketaufbau Byte für Byte gegen die Beschreibung (Offsets und Werte als Zahlen im Test),
- Antworten lesen: gültig, abgeschnitten, fremd, böswillig (Längen, Steuerzeichen),
- Ablauf hinter `LightOutput` mit `support/fake_sink.hpp` (Start, Bild, Haltebild, Ende samt Wartezeiten),
- ein Loopback-Fall in `tests/devices/connect_test.cpp` mit `support/udp_listener.hpp`,
- je Regel eine Gegenprobe (im Pull Request zeigen).

### 6. Doku

README → Tabelle „Geräte“ (ehrlich: „am Gerät geprüft“ nur, wenn es so ist), Fehlersuche, Optionen; [CHANGELOG.md](CHANGELOG.md) unter `[Unveröffentlicht]`; [Gedaechtnis.md](Gedaechtnis.md) unter „Offen“, was noch niemand am Gerät gesehen hat.

## Commits

Deutsch, [Conventional Commits](https://www.conventionalcommits.org/de/) (`feat:`, `fix:`, `docs:`, `test:` …). Der Betreff ist ein Satz ohne Punkt und sagt, **warum**; im Text Ursache, Wirkung, Messwerte und unter `Gegenproben:` beide Läufe.
