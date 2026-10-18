# Messproben

Kleine Programme, die eine Frage am echten System beantworten, statt sie zu erraten.
Die Tastatur-Probe läuft **nicht** in `build.ps1 -Stage test`, sondern auf Wunsch:

```bash
ctest --test-dir out/rel -C Release -L integration --output-on-failure
```

Die übrigen Proben brauchen den Streifen oder ein Konsolenfenster und laufen nur von Hand.

| Frage | Ergebnis (Datum) | Verwendet in |
|---|---|---|
| Kommen echte Konsolen-Tastenereignisse im Spiel an – Pause, Joker, Farbtasten, A (Automodus), Esc, Pfeile im Testbild? | Ja, 19/19 (2026-10-18); A abgeklemmt: 3 rot. `CREATE_NO_WINDOW` gibt eine fensterlose Konsole, an die `AttachConsole` andocken kann; `WriteConsoleInputW` weckt das Spiel, `ReadConsoleOutputCharacterW` auf `CONOUT$` liest auch den VT-Alternativpuffer. Gegenprobe → abgeklemmt: 2 rot. | `key_probe.cpp`, `src/console/console.cpp` |
| Feuert eine gehaltene Taste (Tastenwiederholung) mehrfach? | Nein: 5× gedrückt ohne Loslassen → genau 1 Schuss. Ohne Haltesperre: 2 rot. (2026-10-18) | `Console::poll_keys` |
| Was sendet das Spiel beim Beenden noch? | Strg+C und Konsolenfenster-X: 3× Razer-Aus ~2,0 s nach dem letzten Bild. Tab im Claude-Terminal schließen: nichts (harter Abbruch). (2026-10-18) | `listen_probe.cpp`, `govee/driver.cpp` |
| Wie viel Zeit bleibt beim Schließen des Fensters? | conhost: CTRL_CLOSE_EVENT, danach ~4,95 s. Claude-Terminal-Tab: kein Ereignis. (2026-10-18) | `ctrl_probe.cpp`, `console.cpp` (`kCloseWaitMs`) |
| Wann kehrt der H612C nach dem Razer-Modus zurück? | 1× Aus direkt nach dem Bild: nie (minutenlang eingefroren). 3× Aus nach 2 s oder später: sofort. (2026-10-18, Nutzer sah hin) | `govee_probe.cpp ende-vergleich`, `govee/driver.hpp` |
| Wie viele Farben je Bild nimmt der H612C an? | 30 und 40 ja, 50–80 nein. (2026-10-18, Nutzer sah hin) | `govee_probe.cpp anzahl-test`, `--segmente` |
| Wo geht die Rechenzeit hin? | Ins Zeichnen: Spiel 0,9–1,6 %, conhost 7,4–8,6 % eines Kerns. Nach 30er-Takt und Zeilenvergleich conhost 2,2–2,4 %. (2026-10-18, Demo, 15 s) | `measure-cpu.ps1`, `console/differ.cpp`, `app/main.cpp` |
| Zeichnet das klassische Konsolenfenster alle Zeichen? | Mit ♡ ☆ ▕ ▏: nein (Kästchen, Foto des Nutzers). Nur WGL4-Zeichen: ja (Foto per `PrintWindow`). (2026-10-18) | `snap-console.ps1`, `console/view.cpp` |
| Antwortet der H612C auf devStatus, und wie schnell? | 40/40, 53–217 ms – aber nur mit Multicast-Suche vorweg, wenn die EXE keine Firewall-Regel hat. (2026-10-18) | `govee_probe.cpp status`, `query_status` |
