// Bildschirm des Spiels als ein VT-Text – ohne Konsole, ohne Spiellogik.
//
// Der Streifen in der Konsole ist dasselbe Bild, das an den Govee-Streifen geht (vor
// Spiegelung und Helligkeit): So lässt sich ohne Gerät spielen, und mit Gerät sieht man
// sofort, ob Streifen und Spiel auseinanderlaufen. Die Funktionen sind rein, damit Tests
// den Aufbau prüfen können; alles Spielwissen kommt fertig über HudInfo herein.
//
// Nur Zeichen aus WGL4 (Kastenlinien, Blöcke, ♥ ♦ ▲ ►): Im klassischen Konsolenfenster
// erschienen ♡, ☆ und ▕ ▏ als Kästchen (Bildschirmfoto des Nutzers, 2026-10-18). Wer andere
// Sonderzeichen nimmt, prüft sie im Fenster, das ein Doppelklick auf die EXE öffnet.
//
// Threads: reine Funktionen.
// Fehler: keine.

#pragma once

#include "core/rgb.hpp"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ldu::console {

struct BossBar final {
    std::string name;
    std::string hint;
    int segments = 0;
    int max_segments = 0;
    bool enraged = false;
    bool warning = false;  // noch nicht erschienen
};

struct HudInfo final {
    int score = 0;
    int level = 1;
    int lives = 0;
    int base_lives = 0;  // so viele Herzen werden mindestens gezeigt (leere inklusive)
    int colors_in_play = 0;
    std::array<int, kColorCount> unlock_level{};  // ab welcher Stufe jede Farbe kommt
    int jokers = 0;
    int max_jokers = 0;
    int combo = 0;
    int multiplier = 1;
    std::optional<BossBar> boss;
    bool paused = false;
    bool over = false;
    std::string mode_label;  // „DEMO“, „AUTO SCHNELL ×4“ … – leer im normalen Spiel
    bool auto_mode = false;  // Tastenhilfe für den Automodus statt fürs Spielen
    std::string message;                       // letzte Meldung („Treffer!“ …)
    Rgb message_color = kWhite;                // Farbe der Meldung (Treffer grün, Fehlschuss rot …)
    std::vector<std::string> highscore_lines;  // nur beim Spielende gezeigt
    std::string info_line;                     // zweite Meldungszeile, z. B. Partie und bester Lauf
    std::string stats_line;
    std::string link_status;  // Zeile zum Streifen
    std::vector<Rgb> zone;    // Effektbereich (leer = keiner eingerichtet)
};

std::string render_screen(std::span<const Rgb> frame, const HudInfo& hud, int console_width);

// Sichtbare Breite eines VT-Texts: Steuerfolgen zählen nicht, jedes Zeichen eine Spalte
// (gilt für alle hier verwendeten Zeichen, keine breiten CJK-/Emoji-Zeichen).
int visible_width(std::string_view text);

// Testbild-Ansicht: Streifen plus erklärende Zeilen.
std::string render_calibration(std::span<const Rgb> frame, std::span<const std::string> lines, int console_width);

}  // namespace ldu::console
