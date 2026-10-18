// Kommandozeile und Einstellungsdatei von LichtDuell.
//
// Deutsche Optionsnamen wie in den benutzernahen Skripten des Hauses. Jede Zahl wird
// vollständig und mit Grenzen geprüft – „--leds 30x“ ist ein Fehler, nicht 30.
//
// Reihenfolge: Vorgaben → einstellungen.txt (siehe settings.hpp) → Kommandozeile. Die Datei
// hält, was zum Streifen gehört (IP, Segmente, Versatz …), damit nach dem Einrichten ein
// nacktes „LichtDuell“ reicht. Widersprüche (--nur-vorschau gegen --ip) zählen nur, wenn
// beides auf der Kommandozeile steht; eine IP aus der Datei weicht --nur-vorschau still.
//
// Threads: reine Funktionen.
// Fehler: parse_options wirft Error(Kind::usage) mit einem Satz, der die Option nennt.

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace ldu::cli {

inline constexpr int kDefaultLeds = 30;
inline constexpr int kMinLeds = 8;
inline constexpr int kMaxLeds = 84;  // Grenze des Razer-Pakets, siehe govee/protocol.hpp
inline constexpr int kDefaultFps = 30;
inline constexpr int kMinFps = 5;
inline constexpr int kMaxFps = 60;
inline constexpr int kMaxLevelOption = 30;  // gleich ldu::kMaxStartLevel (game/level.hpp)

// --auto: Der Computer spielt allein, Tasten übernehmen nicht (anders als --demo).
enum class AutoMode { off, slow, normal, fast };

struct Options final {
    AutoMode auto_mode = AutoMode::off;
    bool help = false;
    bool scan = false;
    bool preview_only = false;
    bool reversed = false;
    bool demo = false;
    bool mute = false;
    bool calibration = false;
    bool save = false;           // --speichern: Streifen-Werte in einstellungen.txt schreiben
    bool no_settings = false;    // --ohne-einstellungen: Datei nicht lesen
    std::string ip;
    int leds = kDefaultLeds;
    bool leds_given = false;     // --leds ausdrücklich gesetzt (Datei oder Kommandozeile)
    int fps = kDefaultFps;
    int brightness = 100;
    int start_level = 1;
    int segments = 0;  // Farben im Razer-Paket; 0 = wie --leds
    int skip = 0;      // dunkle Einheiten am Kanonen-Ende (z. B. hinter dem Bett)
    int zone = 0;      // Einheiten am Computer-Ende für den Effektbereich
    std::optional<std::uint32_t> seed;

    // So viele Einheiten gehen tatsächlich an den Streifen.
    int units() const noexcept {
        return segments > 0 ? segments : leds;
    }

    // Einheiten des Kampffelds zwischen Versatz und Effektbereich.
    int field_units() const noexcept {
        return units() - skip - zone;
    }

    // Spielzellen: mit --segmente ohne --leds genau die Feld-Einheiten (1:1, jede Zelle ein
    // Abschnitt); sonst --leds, verteilt auf die Feld-Einheiten.
    int game_cells() const noexcept {
        return segments > 0 && !leds_given ? field_units() : leds;
    }
};

// Liest die Kommandozeile auf `base` (Vorgaben plus Einstellungsdatei).
Options parse_options(std::span<const std::string> args, const Options& base = {});
std::string usage_text();

// Prüft eine Zahl gegen ihre Grenzen; wirft Error(usage) mit `what` im Satz.
int parse_bounded(std::string_view what, std::string_view text, int min, int max);

}  // namespace ldu::cli
