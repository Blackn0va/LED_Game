// Einstieg von LichtDuell.exe: Optionen lesen, Streifen finden, Spiel- oder Testbildschleife.
//
// Die Schleife wartet bis zu 4 ms auf Tastatur (wacht bei einem Tastendruck sofort auf),
// rechnet das Spiel in festen 1/60-s-Schritten fort (GameClock: Tempo, Zeitlupe) und
// berechnet Bild, Streifen und Konsole nur im Zeichentakt (30/s, mit --fps höher). Die
// Konsole bekommt dabei nur geänderte Zeilen (ScreenDiffer). timeBeginPeriod(1), weil
// Windows sonst in 15,6-ms-Schritten weckt; bei 30 fps an den Streifen (33,3 ms) würden
// daraus Abstände von 46,8 ms, also gut 21 Bilder je Sekunde (gerechnet, nicht gemessen).

#include "cli/options.hpp"
#include "cli/settings.hpp"
#include "console/console.hpp"
#include "console/keys.hpp"
#include "console/differ.hpp"
#include "console/view.hpp"
#include "core/error.hpp"
#include "core/text.hpp"
#include "game/autopilot.hpp"
#include "game/autoplay.hpp"
#include "game/calibration.hpp"
#include "game/game.hpp"
#include "game/level.hpp"
#include "govee/strip.hpp"
#include "govee/udp.hpp"
#include "score/highscore.hpp"
#include "sound/player.hpp"

#include <windows.h>
#include <timeapi.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using ldu::Error;

inline constexpr auto kScanWait = 2000ms;
inline constexpr auto kStatusWait = 700ms;
inline constexpr double kMessageS = 2.2;
inline constexpr int kWaitMs = 4;
inline constexpr double kDemoErrorRate = 0.07;
inline constexpr double kDemoRestartS = 6.0;
// Konsole mit 30 statt 60 Bildern je Sekunde: Der Streifen bekommt ohnehin 30 (Vorgabe
// --fps); ein höheres --fps hebt beide an.
inline constexpr int kConsoleFps = 30;
inline constexpr int kAutoMinSoundPriority = 3;  // Joker, Gold, Stufe, Endgegner, Spielende
inline constexpr double kSlowMotionS = 1.6;
inline constexpr double kSlowMotionFactor = 0.25;
// stop() wartet 2 s, bevor es den Razer-Modus abschaltet (govee/strip.hpp) – ohne Hinweis
// sähe das wie ein Hänger aus.
inline constexpr const char* kResetNotice = "\x1b[0m\n\n  Streifen kehrt in seinen alten Zustand zurück (2 s) …\x1b[K";

class TimerResolution final {
public:
    TimerResolution() : active_(timeBeginPeriod(1) == TIMERR_NOERROR) {}
    ~TimerResolution() {
        if (active_) {
            timeEndPeriod(1);
        }
    }
    TimerResolution(const TimerResolution&) = delete;
    TimerResolution& operator=(const TimerResolution&) = delete;

private:
    bool active_;
};

std::string no_device_hint() {
    return "Kein Govee-Gerät hat auf die Suche geantwortet. Prüfen:\n"
           "  1. Govee-App → Streifen → Einstellungen → „LAN-Steuerung“ einschalten\n"
           "     (der Schalter erscheint manchmal erst nach einigen Minuten).\n"
           "  2. PC und Streifen im selben Netz (kein Gast-WLAN, keine Client-Isolation).\n"
           "  3. Windows-Firewall: eingehend UDP 4002 für LichtDuell.exe erlauben.\n"
           "  4. Oder die IP direkt angeben (steht im Router): LichtDuell --ip 192.168.x.y\n"
           "Ohne Streifen spielen: LichtDuell --nur-vorschau";
}

std::string device_line(const ldu::govee::DeviceInfo& d) {
    return "  " + d.ip + "  " + (d.sku.empty() ? "?" : d.sku) + "  " + d.device + "\n";
}

std::string status_text(const std::optional<ldu::govee::DeviceStatus>& s, const std::string& why) {
    if (!s) {
        return "Zustand unbekannt (" + why + ")";
    }
    return std::string(s->on ? "an" : "aus") + ", Helligkeit " + std::to_string(s->brightness) + " %";
}

int run_scan() {
    const ldu::govee::NetworkSession net;
    ldu::console::print("Suche Govee-Geräte im Netz (2 s) …\n");
    const auto devices = ldu::govee::scan_devices(kScanWait);
    if (devices.empty()) {
        ldu::console::print(no_device_hint() + "\n");
        return 1;
    }
    ldu::console::print("  IP-Adresse       Modell  Geräte-ID\n");
    for (const auto& d : devices) {
        std::string why;
        const auto status = ldu::govee::query_status(d.ip, kStatusWait, why);
        ldu::console::print(device_line(d) + "      " + status_text(status, why) + "\n");
    }
    return 0;
}

struct Target final {
    std::string ip;
    std::string label;
};

// Streifen aus --ip, sonst aus der Suche. Kein Gerät oder mehrere sind ein Fehler mit
// Hinweis – still in die Vorschau zu fallen, sähe aus wie ein Streifen, der nicht reagiert.
Target resolve_target(const ldu::cli::Options& o) {
    if (!o.ip.empty()) {
        return Target{o.ip, ""};
    }
    ldu::console::print("Suche Govee-Streifen im Netz (2 s) …\n");
    const auto devices = ldu::govee::scan_devices(kScanWait);
    if (devices.empty()) {
        throw Error(Error::Kind::network, no_device_hint());
    }
    if (devices.size() > 1) {
        std::string text = "Mehrere Govee-Geräte gefunden – bitte eins mit --ip wählen:\n";
        for (const auto& d : devices) {
            text += device_line(d);
        }
        throw Error(Error::Kind::usage, text);
    }
    ldu::console::print("Gefunden:\n" + device_line(devices.front()));
    return Target{devices.front().ip, devices.front().sku};
}

struct StripLink final {
    std::unique_ptr<ldu::govee::StripOutput> output;
    std::string ip;    // tatsächlich verwendete IP (auch aus der Suche) – fürs Speichern
    std::string note;  // Zusatz für die Statuszeile (Zustand vor dem Spiel)
};

StripLink open_strip(const ldu::cli::Options& o) {
    StripLink link;
    if (o.preview_only) {
        return link;
    }
    Target target = resolve_target(o);
    // Zustand vor dem Spiel: War der Streifen aus, geht er beim Beenden wieder aus.
    // Ohne Antwort bleibt er an (fail-open, Komfort – siehe query_status).
    std::string why;
    const auto before = ldu::govee::query_status(target.ip, kStatusWait, why);
    ldu::govee::StripSettings settings;
    link.ip = target.ip;
    settings.ip = std::move(target.ip);
    settings.label = std::move(target.label);
    settings.max_fps = static_cast<double>(o.fps);
    settings.reversed = o.reversed;
    settings.brightness = static_cast<double>(o.brightness) / 100.0;
    settings.turn_off_at_end = before && !before->on;
    // Das Block-Testbild liefert schon eine Farbe je Abschnitt; dort keine zweite Abbildung.
    if (!o.calibration) {
        settings.layout = ldu::govee::StripLayout{o.segments, o.skip, o.zone};
    }
    if (!before) {
        link.note = " · " + status_text(before, why) + ", bleibt danach an";
    } else if (!before->on) {
        link.note = " · war aus, geht danach wieder aus";
    }
    link.output = std::make_unique<ldu::govee::StripOutput>(std::move(settings));
    return link;
}

std::string link_status(const StripLink& link) {
    return link.output ? link.output->status() + link.note : "Nur Vorschau – kein Streifen angeschlossen.";
}

int run_calibration(const ldu::cli::Options& o, StripLink& link) {
    const TimerResolution timer;
    const auto start = std::chrono::steady_clock::now();
    auto now_s = [start] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); };
    // Ab hier wird in Abschnitten gedacht: ohne --segmente ist jede Spielzelle einer.
    ldu::cli::Options cal = o;
    if (cal.segments == 0) {
        cal.segments = cal.units();
    }
    std::string note = "Pfeile verschieben die Grenzen, S speichert.";
    ldu::console::ScreenDiffer differ;
    auto console = ldu::console::Console::open();
    if (link.output) {
        link.output->start(now_s());
    }
    double next_draw = 0.0;
    bool quit = false;
    while (!quit && !console->quit_requested()) {
        for (const auto& key : console->poll_keys()) {
            using ldu::console::Action;
            switch (ldu::console::action_for_key(key.vk)) {
            case Action::quit:
            case Action::restart:
                quit = true;
                break;
            case Action::right:
                if (cal.field_units() > ldu::cli::kMinLeds) {
                    ++cal.skip;
                }
                break;
            case Action::left:
                if (cal.skip > 0) {
                    --cal.skip;
                }
                break;
            case Action::up:
                if (cal.field_units() > ldu::cli::kMinLeds) {
                    ++cal.zone;
                }
                break;
            case Action::down:
                if (cal.zone > 0) {
                    --cal.zone;
                }
                break;
            case Action::save: {
                if (!link.ip.empty()) {
                    cal.ip = link.ip;
                }
                // Speichern ist Komfort: Ein Fehler steht sichtbar in der Hinweiszeile, das
                // Testbild läuft weiter (bewusst fail-open).
                try {
                    const auto path = ldu::cli::settings_path();
                    ldu::cli::save_settings(path, cal);
                    note = "Gespeichert: " + ldu::narrow(path.native()) + " – ab jetzt reicht „LichtDuell“.";
                } catch (const Error& e) {
                    note = std::string("Nicht gespeichert: ") + e.what();
                }
                break;
            }
            default:
                break;
            }
        }
        const double t = now_s();
        if (t >= next_draw) {
            // Gespiegelt wird in StripOutput (Layout 1:1, siehe open_strip) – hier nicht noch einmal.
            const std::vector<ldu::Rgb> frame = ldu::segment_calibration_frame(cal.segments, cal.skip, cal.zone, t);
            if (link.output) {
                link.output->push(frame, t);
            }
            if (console->take_resized()) {
                console->write("\x1b[2J");
                differ.reset();
            }
            const std::vector<std::string> lines{
                std::to_string(cal.segments) + " Abschnitte  ·  aus " + std::to_string(cal.skip) + "  ·  Kampffeld " +
                    std::to_string(cal.field_units()) + "  ·  Effektbereich " + std::to_string(cal.zone),
                "",
                "Weiß (pulsiert) = deine Kanone · Lila = Computer · Orange = Effektbereich · davor aus.",
                "",
                "  ← / →   Kampf früher / später beginnen   (Ziel: Weiß an der linken Deckenecke)",
                "  ↑ / ↓   Effektbereich größer / kleiner   (Ziel: Lila an der rechten Ecke)",
                "  S       speichern",
                "  Esc     beenden",
                "",
                note,
                "",
                link_status(link)};
            const std::string changes = differ.update(ldu::console::render_calibration(frame, lines, console->width()));
            if (!changes.empty()) {
                console->write(changes);
            }
            next_draw = t + 1.0 / static_cast<double>(kConsoleFps);
        }
        console->wait_input(kWaitMs);
    }
    if (link.output) {
        console->write(kResetNotice);
        link.output->stop();
    }
    return 0;
}

std::optional<ldu::sound::Sfx> sfx_for(const ldu::GameEvent& e) {
    using ldu::EventKind;
    using ldu::sound::Sfx;
    switch (e.kind) {
    case EventKind::fired:
        return e.value == 1 ? Sfx::joker_shot : Sfx::shot;
    case EventKind::hit:
        return Sfx::hit;
    case EventKind::cleared:
        return Sfx::clear;
    case EventKind::stuck:
        return Sfx::stuck;
    case EventKind::life_lost:
        return Sfx::life_lost;
    case EventKind::level_up:
        return Sfx::level_up;
    case EventKind::gold:
        return Sfx::gold;
    case EventKind::joker_gained:
        return Sfx::joker;
    case EventKind::boss_warning:
        return Sfx::boss_warning;
    case EventKind::boss_shot:
        return Sfx::boss_shot;
    case EventKind::boss_bite:
        return Sfx::boss_bite;
    case EventKind::boss_defeated:
        return Sfx::boss_defeated;
    case EventKind::game_over:
        return Sfx::game_over;
    case EventKind::boss_spawned:
    case EventKind::boss_enraged:
        return std::nullopt;
    }
    return std::nullopt;
}

class Messages final {
public:
    std::string text_for(const ldu::GameEvent& e) {
        using ldu::EventKind;
        switch (e.kind) {
        case EventKind::hit:
            return "Treffer! +" + ldu::format_number(e.value);
        case EventKind::cleared:
            return "Aufgelöst! +" + ldu::format_number(e.value);
        case EventKind::stuck:
            return "Falsche Farbe – " + std::string(ldu::color_name(e.color)) + " steckt jetzt vorn fest.";
        case EventKind::life_lost:
            return "Leben verloren! Noch " + std::to_string(e.value) + ".";
        case EventKind::level_up: {
            std::string text = "Stufe " + std::to_string(e.value) + " – schneller!";
            const int now = ldu::level_params(e.value).colors;
            if (now > ldu::level_params(e.value - 1).colors) {
                text += " Neue Farbe: " + std::string(ldu::color_name(static_cast<ldu::Color>(now - 1))) +
                        " (Taste " + std::to_string(now) + ").";
            }
            return text;
        }
        case EventKind::gold:
            return "Goldpunkt! +" + ldu::format_number(e.value) + " und ein Joker.";
        case EventKind::joker_gained:
            return "Kombo! Neuer Joker (" + std::to_string(e.value) + "/" + std::to_string(ldu::kMaxJokers) + ").";
        case EventKind::boss_warning:
            boss_ = static_cast<ldu::BossType>(e.value);
            return "ACHTUNG – Endgegner: " + name() + "!";
        case EventKind::boss_spawned:
            boss_ = static_cast<ldu::BossType>(e.value);
            return article(true) + name() + " ist da. " + std::string(ldu::boss_hint(boss_));
        case EventKind::boss_enraged:
            return article(true) + name() + " wird wütend – jetzt schneller!";
        case EventKind::boss_bite:
            return "Autsch – " + article(false) + name() + " hat zugebissen! Noch " + std::to_string(e.value) +
                   " Leben.";
        case EventKind::boss_defeated:
            return name() + " besiegt! +" + ldu::format_number(e.value) + ", ein Leben und ein Joker.";
        case EventKind::fired:
        case EventKind::boss_shot:
        case EventKind::game_over:
            return {};
        }
        return {};
    }

private:
    std::string name() const {
        return std::string(ldu::boss_name(boss_));
    }

    // „das Chamäleon“, aber „der Farbwurm“ und „der Kanonier“.
    std::string article(bool sentence_start) const {
        const bool neuter = boss_ == ldu::BossType::chameleon;
        if (sentence_start) {
            return neuter ? "Das " : "Der ";
        }
        return neuter ? "das " : "der ";
    }

    ldu::BossType boss_ = ldu::BossType::worm;
};

inline constexpr ldu::Rgb kInfoColor{200, 190, 255};
inline constexpr ldu::Rgb kHintColor{200, 160, 90};

// Meldungsfarbe je Ereignis: Gutes grün/gold, Schlechtes rot, Endgegner orange – so liest
// man im Augenwinkel, was passiert ist, ohne den Text zu lesen.
ldu::Rgb message_color_for(ldu::EventKind kind) {
    using ldu::EventKind;
    switch (kind) {
    case EventKind::hit:
        return {120, 230, 120};
    case EventKind::cleared:
    case EventKind::level_up:
        return {90, 255, 150};
    case EventKind::stuck:
    case EventKind::life_lost:
    case EventKind::boss_bite:
    case EventKind::game_over:
        return {255, 80, 80};
    case EventKind::gold:
    case EventKind::joker_gained:
    case EventKind::boss_defeated:
        return {255, 215, 80};
    case EventKind::boss_warning:
    case EventKind::boss_spawned:
    case EventKind::boss_enraged:
    case EventKind::boss_shot:
        return {255, 140, 40};
    case EventKind::fired:
        return kInfoColor;
    }
    return kInfoColor;
}

std::string stats_line(const ldu::GameStats& s) {
    return "Schüsse " + std::to_string(s.shots) + " · Treffer " + std::to_string(s.hits) + " · Fehlschüsse " +
           std::to_string(s.misses) + " · Beste Kombo " + std::to_string(s.best_combo) + " · Endgegner " +
           std::to_string(s.bosses) + " · Gold " + std::to_string(s.golds);
}

struct Highscores final {
    std::optional<std::filesystem::path> path;
    std::vector<ldu::score::ScoreEntry> table;
    std::string problem;
};

Highscores load_highscores() {
    Highscores h;
    // Die Bestenliste ist Komfort: Lässt sie sich nicht lesen, läuft das Spiel trotzdem,
    // und der Grund steht beim Spielende auf dem Bildschirm (bewusst fail-open).
    try {
        h.path = ldu::score::default_highscore_path();
        h.table = ldu::score::load_highscores(*h.path);
    } catch (const Error& e) {
        h.problem = e.what();
    }
    return h;
}

std::vector<std::string> record_and_list(Highscores& h, const ldu::Game& game, bool counts) {
    std::vector<std::string> lines;
    std::optional<std::size_t> rank;
    if (counts && h.path) {
        rank = ldu::score::insert_score(h.table, {game.score(), game.level(), ldu::score::today()});
        if (rank) {
            // Speichern ist ebenso Komfort wie Laden; ein Fehler landet sichtbar in der Liste.
            try {
                ldu::score::save_highscores(*h.path, h.table);
            } catch (const Error& e) {
                h.problem = e.what();
            }
        }
    }
    lines.push_back(rank ? "NEUER EINTRAG auf Platz " + std::to_string(*rank + 1) + "!  Bestenliste:" : "Bestenliste:");
    if (h.table.empty()) {
        lines.push_back("  (noch leer)");
    }
    for (std::size_t i = 0; i < h.table.size(); ++i) {
        const auto& e = h.table[i];
        std::string line = "  " + std::to_string(i + 1) + ".  " + ldu::format_number(e.points) + " Punkte, Stufe " +
                           std::to_string(e.level) + ", " + e.date;
        if (rank && *rank == i) {
            line += "   ← neu";
        }
        lines.push_back(std::move(line));
    }
    if (!counts) {
        lines.push_back("  (zählt nicht: Demo oder Start über Stufe 1)");
    }
    if (!h.problem.empty()) {
        lines.push_back("  " + h.problem);
    }
    return lines;
}

ldu::Game make_game(const ldu::cli::Options& o, std::uint32_t seed) {
    ldu::GameConfig config;
    config.cells = o.game_cells();
    config.seed = seed;
    config.start_level = o.start_level;
    return ldu::Game(config);
}

ldu::AutoSpeed to_speed(ldu::cli::AutoMode mode) {
    switch (mode) {
    case ldu::cli::AutoMode::slow:
        return ldu::AutoSpeed::slow;
    case ldu::cli::AutoMode::fast:
        return ldu::AutoSpeed::fast;
    case ldu::cli::AutoMode::normal:
    case ldu::cli::AutoMode::off:
        return ldu::AutoSpeed::normal;
    }
    return ldu::AutoSpeed::normal;
}

int run_game(const ldu::cli::Options& o, StripLink& link) {
    std::unique_ptr<ldu::sound::SoundPlayer> sound;
    if (!o.mute) {
        sound = std::make_unique<ldu::sound::SoundPlayer>();
    }
    Highscores highscores = load_highscores();

    const TimerResolution timer;
    const auto start = std::chrono::steady_clock::now();
    auto now_s = [start] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); };
    std::uint32_t seed =
        o.seed.value_or(static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    ldu::Game game = make_game(o, seed);

    // Automodus: mit --auto von Anfang an, sonst jederzeit per Taste A. Ohne --auto gilt beim
    // Einschalten die Voreinstellung „normal“ (×1); ←/→ ändert das Tempo nur im Automodus.
    const ldu::AutoPreset preset = ldu::auto_preset(to_speed(o.auto_mode));
    bool auto_on = o.auto_mode != ldu::cli::AutoMode::off;
    std::uint32_t pilot_seed = seed ^ 0x9E3779B9u;
    std::optional<ldu::Autopilot> pilot;
    if (auto_on) {
        pilot.emplace(pilot_seed, preset.profile);
    } else if (o.demo) {
        pilot.emplace(pilot_seed, kDemoErrorRate);
    }
    std::size_t auto_tempo_step = ldu::nearest_tempo_step(preset.tempo);
    ldu::GameClock clock(auto_on ? ldu::kTempoSteps[auto_tempo_step] : 1.0);
    // Hat der Computer in dieser Partie mitgespielt? Dann zählt sie nicht für die Bestenliste –
    // auch wenn du am Ende selbst gespielt hast.
    bool assisted = pilot.has_value();
    int games_played = 1;
    int best_level = 0;
    int best_score = 0;

    auto console = ldu::console::Console::open();
    if (link.output) {
        link.output->start(now_s());
    }

    ldu::console::HudInfo hud;
    for (int i = 0; i < ldu::kColorCount; ++i) {
        hud.unlock_level[static_cast<std::size_t>(i)] = ldu::first_level_with_colors(i + 1);
    }
    hud.max_jokers = ldu::kMaxJokers;
    ldu::console::ScreenDiffer differ;
    Messages messages;
    std::string message =
        auto_on ? "Automodus " + std::string(preset.name) + " – der Computer spielt. A: selbst spielen, ←/→: Tempo."
        : pilot ? "Demo: Der Computer spielt. Farbtaste oder Leertaste – und du übernimmst."
                : "Bereit – die ersten Punkte kommen gleich. A: der Computer spielt.";
    ldu::Rgb message_color = kInfoColor;
    double message_until = now_s() + 4.0;
    auto say = [&](std::string text, ldu::Rgb color, double now) {
        message = std::move(text);
        message_color = color;
        message_until = now + kMessageS;
    };
    std::vector<std::string> highscore_lines;
    bool paused = false;
    bool quit = false;
    double last = now_s();
    double next_draw = 0.0;
    double over_since = -1.0;
    // Bild für Streifen und Konsole im selben Takt: Vorher wurde es in jeder Schleifenrunde
    // (~250/s) berechnet, gebraucht werden höchstens die Bilder des Streifens.
    const double draw_interval = 1.0 / static_cast<double>(std::max(kConsoleFps, o.fps));

    auto restart = [&] {
        game = make_game(o, ++seed);
        assisted = pilot.has_value();
        ++games_played;
        paused = false;
        highscore_lines.clear();
        over_since = -1.0;
        say(auto_on ? "Partie " + std::to_string(games_played) + " – los geht's!" : "Neues Spiel!", kInfoColor,
            now_s());
    };
    auto take_over = [&] {
        if (pilot && !auto_on) {
            pilot.reset();
            say("Du übernimmst – viel Glück!", kInfoColor, now_s());
        }
    };
    // Taste A: Der Computer übernimmt mitten in der Partie bzw. gibt sie zurück. Beim Abgeben
    // springt das Tempo auf ×1 – bei ×4 könnte kein Mensch weiterspielen.
    auto set_auto = [&](bool on) {
        auto_on = on;
        if (on) {
            pilot.emplace(++pilot_seed, preset.profile);
            assisted = true;
            clock.set_tempo(ldu::kTempoSteps[auto_tempo_step]);
            say("Automodus an – der Computer spielt (" + ldu::tempo_label(clock.tempo()) +
                    "). A: selbst spielen, ←/→: Tempo.",
                kInfoColor, now_s());
        } else {
            pilot.reset();
            clock.set_tempo(1.0);
            say("Automodus aus – du spielst. A: der Computer übernimmt wieder.", kInfoColor, now_s());
        }
    };

    while (!quit && !console->quit_requested()) {
        for (const auto& key : console->poll_keys()) {
            const auto action = ldu::console::action_for_key(key.vk);
            if (action == ldu::console::Action::quit) {
                quit = true;
                break;
            }
            if (action == ldu::console::Action::pause && !game.over()) {
                paused = !paused;
                continue;
            }
            if (action == ldu::console::Action::toggle_auto) {
                set_auto(!auto_on);
                continue;
            }
            if (action == ldu::console::Action::restart) {
                if (game.over() || auto_on) {
                    restart();
                }
                continue;
            }
            if (auto_on && (action == ldu::console::Action::left || action == ldu::console::Action::right)) {
                if (action == ldu::console::Action::left && auto_tempo_step > 0) {
                    --auto_tempo_step;
                } else if (action == ldu::console::Action::right && auto_tempo_step + 1 < ldu::kTempoSteps.size()) {
                    ++auto_tempo_step;
                }
                clock.set_tempo(ldu::kTempoSteps[auto_tempo_step]);
                say("Tempo " + ldu::tempo_label(clock.tempo()), kInfoColor, now_s());
                continue;
            }
            if (auto_on) {
                continue;  // Im Automodus spielt nur der Computer; A gibt dir die Partie zurück.
            }
            if (action == ldu::console::Action::joker) {
                take_over();
                if (!paused && !game.over() && game.jokers() == 0) {
                    say("Kein Joker – den gibt es für 8 Treffer in Folge, Gold und Endgegner.", kHintColor, now_s());
                } else if (!paused) {
                    game.fire_joker();
                }
                continue;
            }
            const auto color = ldu::console::color_for_key(key.vk);
            if (!color) {
                continue;
            }
            take_over();
            if (paused) {
                continue;
            }
            if (!game.fire(*color) && !game.over() && static_cast<int>(*color) >= game.colors_in_play()) {
                say(std::string(ldu::color_name(*color)) + " kommt erst ab Stufe " +
                        std::to_string(ldu::first_level_with_colors(static_cast<int>(*color) + 1)) + ".",
                    kHintColor, now_s());
            }
        }

        const double t = now_s();
        const double real_dt = t - last;
        last = t;
        if (!paused) {
            // Feste Spielschritte, Tempo und Zeitlupe über GameClock (siehe game/autoplay.hpp).
            const int steps = clock.advance(t, real_dt);
            for (int i = 0; i < steps; ++i) {
                if (pilot) {
                    if (const auto move = pilot->decide(game, ldu::kGameStepS)) {
                        if (move->joker) {
                            game.fire_joker();
                        } else {
                            game.fire(move->color);
                        }
                    }
                }
                game.update(ldu::kGameStepS);
            }
        }

        std::optional<ldu::sound::Sfx> loudest;
        for (const auto& e : game.take_events()) {
            std::string text = messages.text_for(e);
            if (!text.empty()) {
                say(std::move(text), message_color_for(e.kind), t);
            }
            const auto sfx = sfx_for(e);
            // Im Automodus nur die großen Momente – bei Tempo ×4 wäre jeder Schuss ein Dauerfeuer.
            const bool audible = sfx && (!auto_on || ldu::sound::sfx_priority(*sfx) >= kAutoMinSoundPriority);
            if (audible && (!loudest || ldu::sound::sfx_priority(*sfx) >= ldu::sound::sfx_priority(*loudest))) {
                loudest = sfx;
            }
            if (pilot && (e.kind == ldu::EventKind::boss_defeated || e.kind == ldu::EventKind::game_over)) {
                // Zeitlupe: Sieg und Niederlage wirken länger nach, gerade bei hohem Tempo.
                clock.slow_motion(t, kSlowMotionS, kSlowMotionFactor);
            }
            if (e.kind == ldu::EventKind::game_over) {
                highscore_lines = record_and_list(highscores, game, !assisted && o.start_level == 1);
                over_since = t;
                best_level = std::max(best_level, game.level());
                best_score = std::max(best_score, game.score());
            }
        }
        if (sound && loudest) {
            sound->play(*loudest, t);
        }
        if (t >= message_until) {
            message.clear();
        }
        if (pilot && game.over() && over_since >= 0.0 &&
            t - over_since > (auto_on ? preset.restart_s : kDemoRestartS)) {
            restart();
        }

        if (t >= next_draw) {
            const std::vector<ldu::Rgb> frame = game.render();
            const std::vector<ldu::Rgb> zone = o.zone > 0 ? game.render_zone(o.zone) : std::vector<ldu::Rgb>{};
            if (link.output) {
                link.output->push(frame, zone, t);
            }
            if (console->take_resized()) {
                console->write("\x1b[2J");
                differ.reset();
            }
            hud.auto_mode = auto_on;
            hud.mode_label = auto_on ? "AUTO " + ldu::tempo_label(clock.tempo()) +
                                           (clock.slow_motion_active(t) ? " · ZEITLUPE" : "")
                             : pilot ? "DEMO"
                                     : "";
            hud.info_line = auto_on ? "Partie " + std::to_string(games_played) +
                                          (best_level > 0 ? " · bester Lauf: Stufe " + std::to_string(best_level) +
                                                                ", " + ldu::format_number(best_score) + " Punkte"
                                                          : "")
                                    : "";
            hud.zone = zone;
            hud.score = game.score();
            hud.level = game.level();
            hud.lives = game.lives();
            hud.base_lives = game.base_lives();
            hud.colors_in_play = game.colors_in_play();
            hud.jokers = game.jokers();
            hud.combo = game.combo();
            hud.multiplier = game.multiplier();
            hud.boss.reset();
            if (const auto b = game.boss()) {
                hud.boss = ldu::console::BossBar{std::string(ldu::boss_name(b->type)), std::string(ldu::boss_hint(b->type)),
                                                 b->segments, b->max_segments, b->enraged, b->warning};
            }
            hud.paused = paused;
            hud.over = game.over();
            hud.message = message;
            hud.message_color = message_color;
            hud.highscore_lines = highscore_lines;
            hud.stats_line = stats_line(game.stats());
            hud.link_status = link_status(link);
            // Nur geänderte Zeilen hinaus (console/differ.hpp) – unverändert: gar nichts.
            const std::string changes = differ.update(ldu::console::render_screen(frame, hud, console->width()));
            if (!changes.empty()) {
                console->write(changes);
            }
            next_draw = t + draw_interval;
        }
        console->wait_input(kWaitMs);
    }

    if (link.output) {
        console->write(kResetNotice);
        link.output->stop();
    }
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    // Nachgeladene DLLs nur aus System32 (CWE-427, Muster aller C++-Werkzeuge des Hauses).
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.push_back(ldu::narrow(argv[i]));
        }
        // Einstellungsdatei vor der Kommandozeile lesen – außer mit --ohne-einstellungen.
        ldu::cli::Options base;
        if (std::find(args.begin(), args.end(), "--ohne-einstellungen") == args.end()) {
            ldu::cli::load_settings(ldu::cli::settings_path(), base);
        }
        const ldu::cli::Options options = ldu::cli::parse_options(args, base);
        if (options.help) {
            ldu::console::print(ldu::cli::usage_text());
            return 0;
        }
        if (options.save) {
            const auto path = ldu::cli::settings_path();
            ldu::cli::save_settings(path, options);
            ldu::console::print("Gespeichert: " + ldu::narrow(path.native()) + "\n" + ldu::cli::format_settings(options));
            return 0;
        }
        if (options.scan) {
            return run_scan();
        }
        std::optional<ldu::govee::NetworkSession> net;
        if (!options.preview_only) {
            net.emplace();
        }
        StripLink link = open_strip(options);
        return options.calibration ? run_calibration(options, link) : run_game(options, link);
    } catch (const Error& e) {
        ldu::console::print(std::string("FEHLER: ") + e.what() + "\n");
        if (e.kind() == Error::Kind::usage) {
            ldu::console::print("Hilfe: LichtDuell --hilfe\n");
        }
        return 1;
    } catch (const std::exception& e) {
        // Prozessgrenze: Unerwartetes als ein lesbarer Satz statt eines Absturzdialogs.
        ldu::console::print(std::string("FEHLER (unerwartet): ") + e.what() + "\n");
        return 1;
    }
}
