// Kommandozeile und Einstellungsdatei von LichtDuell.
//
// Deutsche Optionsnamen wie in den benutzernahen Skripten des Hauses. Jede Zahl wird
// vollständig und mit Grenzen geprüft – „--leds 30x“ ist ein Fehler, nicht 30.
//
// Reihenfolge: Vorgaben → einstellungen.txt (siehe settings.hpp) → Kommandozeile. Die Datei
// hält, was zum Streifen gehört (Geräteart, IP, Segmente, Versatz …), damit nach dem
// Einrichten ein nacktes „LichtDuell“ reicht. Widersprüche (--nur-vorschau gegen --ip)
// zählen nur, wenn beides auf der Kommandozeile steht; eine IP aus der Datei weicht
// --nur-vorschau still.
//
// Grenzen: --segmente richtet sich nach der Geräteart (lights/device.hpp) und wird erst am
// Ende geprüft, weil --gerät auch hinter --segmente stehen darf; ebenso, ob die Universen ab
// --universum für alle Abschnitte reichen. Spielzellen bleiben bei höchstens kMaxLeds, auch
// auf einem 2040-LED-Streifen – mehr Zellen machen die Strecke nicht länger (die Laufzeit
// hängt an Sekunden, game/level.cpp), nur die Konsole zu breit.
//
// Threads: reine Funktionen.
// Fehler: parse_options wirft Error(Kind::usage) mit einem Satz, der die Option nennt.

#pragma once

#include "lights/device.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ldu::cli {

inline constexpr int kDefaultLeds = 30;
inline constexpr int kMinLeds = 8;
inline constexpr int kMaxLeds = 84;       // Spielzellen (und Govee-Grenze, govee/protocol.hpp)
inline constexpr int kMaxSegments = lights::kMaxLongStripUnits;  // größte Grenze aller Gerätearten
inline constexpr int kDefaultFps = 30;
inline constexpr int kMinFps = 5;
inline constexpr int kMaxFps = 60;
inline constexpr int kMaxLevelOption = 30;  // gleich ldu::kMaxStartLevel (game/level.hpp)
inline constexpr int kMaxUniverseOption = 63999;  // größte Grenze (sACN); je Art genauer am Ende

// --auto: Der Computer spielt allein, Tasten übernehmen nicht (anders als --demo).
enum class AutoMode { off, slow, normal, fast };

struct Options final {
    AutoMode auto_mode = AutoMode::off;
    bool help = false;
    bool scan = false;
    bool setup = false;          // --einrichten: suchen, wählen, speichern, Testbild
    bool preview_only = false;
    bool reversed = false;
    bool demo = false;
    bool mute = false;
    bool calibration = false;
    bool save = false;           // --speichern: Streifen-Werte in einstellungen.txt schreiben
    bool no_settings = false;    // --ohne-einstellungen: Datei nicht lesen
    std::string settings_file;   // --einstellungen <datei>: statt der Vorgabe-Datei
    lights::DeviceType device = lights::DeviceType::govee;
    std::string ip;
    std::optional<int> universe;  // Art-Net/sACN; leer = Vorgabe der Geräteart
    int leds = kDefaultLeds;
    bool leds_given = false;     // --leds ausdrücklich gesetzt (Datei oder Kommandozeile)
    int fps = kDefaultFps;
    int brightness = 100;
    int start_level = 1;
    int segments = 0;  // Farben je Bild an das Gerät; 0 = wie --leds
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

    // Spielzellen: mit --segmente ohne --leds die Feld-Einheiten (1:1, jede Zelle ein
    // Abschnitt), höchstens kMaxLeds; sonst --leds, verteilt auf die Feld-Einheiten.
    int game_cells() const noexcept {
        return segments > 0 && !leds_given ? std::min(field_units(), kMaxLeds) : leds;
    }
};

// Liest die Kommandozeile auf `base` (Vorgaben plus Einstellungsdatei).
Options parse_options(std::span<const std::string> args, const Options& base = {});
std::string usage_text();

// Prüft eine Zahl gegen ihre Grenzen; wirft Error(usage) mit `what` im Satz.
int parse_bounded(std::string_view what, std::string_view text, int min, int max);

// Geräteart aus einem Schlüssel; wirft Error(usage) mit `what` im Satz.
lights::DeviceType parse_device(std::string_view what, std::string_view text);

// Prüft, was erst mit der Geräteart feststeht (Segmente, Universum). Wirft Error(usage).
void check_device_limits(const Options& o);

// Pfad hinter --einstellungen, bevor parse_options läuft (main liest die Datei vorher).
// nullopt ohne die Option; wirft Error(usage), wenn der Pfad fehlt.
std::optional<std::string> settings_file_argument(std::span<const std::string> args);

}  // namespace ldu::cli
