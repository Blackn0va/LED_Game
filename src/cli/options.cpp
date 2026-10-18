#include "cli/options.hpp"

#include "core/error.hpp"

#include <charconv>
#include <limits>
#include <string_view>

namespace ldu::cli {
namespace {

template <typename T>
T parse_number(std::string_view option, std::string_view text, T min, T max) {
    T value{};
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size() || value < min || value > max) {
        throw Error(Error::Kind::usage, std::string(option) + " erwartet eine ganze Zahl von " + std::to_string(min) +
                                            " bis " + std::to_string(max) + ", nicht „" + std::string(text) + "“.");
    }
    return value;
}

}  // namespace

int parse_bounded(std::string_view what, std::string_view text, int min, int max) {
    return parse_number<int>(what, text, min, max);
}

Options parse_options(std::span<const std::string> args, const Options& base) {
    Options o = base;
    bool level_given = false;
    bool ip_given = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        auto value = [&]() -> const std::string& {
            if (i + 1 >= args.size()) {
                throw Error(Error::Kind::usage, a + " braucht einen Wert.");
            }
            return args[++i];
        };
        if (a == "--hilfe" || a == "-h" || a == "--help" || a == "/?") {
            o.help = true;
        } else if (a == "--suchen") {
            o.scan = true;
        } else if (a == "--nur-vorschau") {
            o.preview_only = true;
        } else if (a == "--umgedreht") {
            o.reversed = true;
        } else if (a == "--demo") {
            o.demo = true;
        } else if (a == "--auto") {
            const std::string& v = value();
            if (v == "langsam") {
                o.auto_mode = AutoMode::slow;
            } else if (v == "normal") {
                o.auto_mode = AutoMode::normal;
            } else if (v == "schnell") {
                o.auto_mode = AutoMode::fast;
            } else {
                throw Error(Error::Kind::usage, "--auto erwartet langsam, normal oder schnell, nicht „" + v + "“.");
            }
        } else if (a == "--ohne-ton") {
            o.mute = true;
        } else if (a == "--testbild") {
            o.calibration = true;
        } else if (a == "--speichern") {
            o.save = true;
        } else if (a == "--ohne-einstellungen") {
            o.no_settings = true;
        } else if (a == "--ip") {
            o.ip = value();
            ip_given = true;
            if (o.ip.empty()) {
                throw Error(Error::Kind::usage, "--ip braucht eine Adresse.");
            }
        } else if (a == "--leds") {
            o.leds = parse_number<int>(a, value(), kMinLeds, kMaxLeds);
            o.leds_given = true;
        } else if (a == "--fps") {
            o.fps = parse_number<int>(a, value(), kMinFps, kMaxFps);
        } else if (a == "--helligkeit") {
            o.brightness = parse_number<int>(a, value(), 1, 100);
        } else if (a == "--segmente") {
            o.segments = parse_number<int>(a, value(), kMinLeds, kMaxLeds);
        } else if (a == "--versatz") {
            o.skip = parse_number<int>(a, value(), 0, kMaxLeds - kMinLeds);
        } else if (a == "--effektbereich") {
            o.zone = parse_number<int>(a, value(), 0, kMaxLeds - kMinLeds);
        } else if (a == "--stufe") {
            o.start_level = parse_number<int>(a, value(), 1, kMaxLevelOption);
            level_given = true;
        } else if (a == "--seed") {
            o.seed = parse_number<std::uint32_t>(a, value(), 0, std::numeric_limits<std::uint32_t>::max());
        } else {
            throw Error(Error::Kind::usage, "Unbekannte Option „" + a + "“ – LichtDuell --hilfe zeigt alle.");
        }
    }
    if (o.preview_only && ip_given) {
        throw Error(Error::Kind::usage, "--nur-vorschau und --ip schließen sich aus.");
    }
    if (o.preview_only) {
        o.ip.clear();  // IP aus der Einstellungsdatei weicht der Vorschau
    }
    const bool automatic = o.auto_mode != AutoMode::off;
    if (automatic && o.demo) {
        throw Error(Error::Kind::usage, "--auto und --demo schließen sich aus (bei --demo darfst du übernehmen).");
    }
    if (o.scan && (o.preview_only || ip_given || o.demo || o.calibration || level_given || automatic)) {
        throw Error(Error::Kind::usage, "--suchen steht allein (höchstens mit --hilfe).");
    }
    if (o.field_units() < kMinLeds) {
        throw Error(Error::Kind::usage, "--versatz " + std::to_string(o.skip) + " und --effektbereich " +
                                            std::to_string(o.zone) + " lassen von " + std::to_string(o.units()) +
                                            " Einheiten weniger als " + std::to_string(kMinLeds) +
                                            " für das Kampffeld.");
    }
    if (o.calibration && (o.demo || level_given || o.seed || automatic)) {
        throw Error(Error::Kind::usage, "--testbild zeigt nur das Testbild – ohne --demo, --auto, --stufe und --seed.");
    }
    return o;
}

std::string usage_text() {
    return "LichtDuell – Farbduell auf einem Govee-LED-Streifen\n"
           "\n"
           "Der Computer schickt farbige Punkte vom fernen Ende los. Schieß mit derselben\n"
           "Farbe zurück, um den vordersten Punkt aufzulösen. Eine falsche Farbe steckt sich\n"
           "vorn an und muss selbst erst wieder weg. Erreicht ein Punkt die Kanone, ist ein\n"
           "Leben weg. Jede dritte Stufe wartet ein Endgegner: Farbwurm, Chamäleon, Kanonier.\n"
           "\n"
           "Aufruf:\n"
           "  LichtDuell                     spielen (Streifen aus einstellungen.txt oder Suche)\n"
           "  LichtDuell --ip 192.168.1.50   Streifen direkt ansprechen (ohne Suche)\n"
           "  LichtDuell --nur-vorschau      ohne Streifen, nur in der Konsole\n"
           "  LichtDuell --demo              der Computer spielt vor (Farbtaste: selbst übernehmen)\n"
           "  LichtDuell --auto langsam      nur der Computer, gemächlich – Lichtstimmung fürs Zimmer\n"
           "  LichtDuell --auto schnell      nur der Computer, sehr schnell – Feuerwerk an der Decke\n"
           "                                 (A schaltet ihn auch mitten im Spiel an und aus,\n"
           "                                  ←/→ Tempo, Enter neue Partie)\n"
           "  LichtDuell --testbild          Streifen einrichten (Pfeile verschieben, S speichert)\n"
           "  LichtDuell --suchen            Govee-Geräte im Netz auflisten\n"
           "\n"
           "Optionen:\n"
           "  --leds <8–84>          Spielzellen (Vorgabe 30, mit --segmente: die sichtbaren)\n"
           "  --segmente <8–84>      Abschnitte, in die der Streifen geteilt wird (H612C: höchstens 40)\n"
           "  --versatz <n>          so viele Abschnitte am Kanonen-Ende auslassen (z. B. hinter dem Bett)\n"
           "  --effektbereich <n>    so viele Abschnitte am Computer-Ende für Leben, Joker und Effekte\n"
           "  --fps <5–60>           Bilder je Sekunde an den Streifen (Vorgabe 30)\n"
           "  --helligkeit <1–100>   Helligkeit in Prozent (Vorgabe 100)\n"
           "  --umgedreht            Kanone am anderen Ende des Streifens\n"
           "  --stufe <1–30>         auf einer höheren Stufe beginnen (3: erster Endgegner)\n"
           "  --ohne-ton             keine Klänge\n"
           "  --seed <zahl>          feste Zufallsfolge (gleiches Seed, gleiches Spiel)\n"
           "  --speichern            Streifen-Werte in einstellungen.txt schreiben und beenden\n"
           "  --ohne-einstellungen   einstellungen.txt nicht lesen\n"
           "  --hilfe                diese Hilfe\n"
           "\n"
           "Tasten:\n"
           "  1/R Rot · 2/G Grün · 3/B Blau · 4/Y Gelb (ab Stufe 4) · 5/L Lila (ab Stufe 7)\n"
           "  Leertaste Joker (passt auf jede Farbe) · A Automodus an/aus · P Pause · Enter neues Spiel\n"
           "  Esc Ende\n"
           "\n"
           "Einstellungen: %LOCALAPPDATA%\\StreamWizard\\LichtDuell\\einstellungen.txt\n"
           "Voraussetzung: In der Govee-App beim Streifen „LAN-Steuerung“ einschalten.\n";
}

}  // namespace ldu::cli
