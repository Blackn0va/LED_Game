// Konsole ohne Fenster: Bildaufbau (render_screen, render_calibration) und Tastenbelegung.

#include "console/differ.hpp"
#include "console/keys.hpp"
#include "console/view.hpp"

#include <windows.h>

#include <doctest.h>

#include <string>
#include <string_view>
#include <vector>

namespace ldu::console {
namespace {

int count(const std::string& text, std::string_view needle) {
    int n = 0;
    for (std::size_t pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + needle.size())) {
        ++n;
    }
    return n;
}

bool contains(const std::string& text, std::string_view needle) {
    return text.find(needle) != std::string::npos;
}

std::string fg(Rgb c) {
    return "\x1b[38;2;" + std::to_string(static_cast<int>(c.r)) + ";" + std::to_string(static_cast<int>(c.g)) + ";" +
           std::to_string(static_cast<int>(c.b)) + "m";
}

// Zeilen des Bilds ohne Steuerfolgen am Zeilenende.
std::vector<std::string> lines_of(const std::string& screen) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < screen.size()) {
        std::size_t end = screen.find('\n', start);
        if (end == std::string::npos) {
            end = screen.size();
        }
        lines.push_back(screen.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

HudInfo sample_hud() {
    HudInfo hud;
    hud.score = 1240;
    hud.level = 2;
    hud.lives = 2;
    hud.base_lives = 3;
    hud.colors_in_play = 3;
    hud.unlock_level = {1, 1, 1, 4, 7};
    hud.jokers = 1;
    hud.max_jokers = 3;
    return hud;
}

const std::vector<Rgb> kEmpty(10, kBlack);
const Rgb kHeart{230, 40, 60};
const Rgb kJoker{255, 215, 80};

}  // namespace

TEST_CASE("Bildschirm: nur Zeichen, die das klassische Konsolenfenster kann (kein ♡ ☆ ▕ ▏)") {
    HudInfo hud = sample_hud();
    hud.lives = 0;
    hud.boss = BossBar{"Farbwurm", "Lang.", 3, 10, true, false};
    hud.zone = std::vector<Rgb>(8, kWhite);
    const std::string s = render_screen(kEmpty, hud, 120);
    for (const std::string_view bad : {"♡", "☆", "★", "▕", "▏"}) {
        CHECK_FALSE(contains(s, bad));
    }
}

TEST_CASE("Bildschirm: Rahmen steht gerade – jede Zeile gleich breit, bei 120 und bei 80 Spalten") {
    for (const int width : {120, 80}) {
        HudInfo hud = sample_hud();
        hud.combo = 12;
        hud.multiplier = 3;
        hud.message = "Treffer! +40";
        hud.zone = std::vector<Rgb>(8, kWhite);
        hud.stats_line = "Schüsse 26 · Treffer 20";
        hud.link_status = "Govee 192.0.2.21 · 30 fps";
        const auto lines = lines_of(render_screen(std::vector<Rgb>(21, kBlack), hud, width));
        REQUIRE(lines.size() > 10);
        const int expected = visible_width(lines[0]);
        CHECK(expected == std::min(width, 108));
        for (std::size_t i = 0; i + 1 < lines.size(); ++i) {
            INFO("Breite " << width << ", Zeile " << i);
            CHECK(visible_width(lines[i]) == expected);
        }
    }
}

TEST_CASE("Bildschirm: Kopf zeigt Stufe, Punkte mit Tausenderpunkt, Leben und Joker als ♥ und ♦") {
    const std::string s = render_screen(kEmpty, sample_hud(), 120);
    CHECK(contains(s, "Stufe 2"));
    CHECK(contains(s, "Punkte 1.240"));
    CHECK(count(s, "♥") == 3);
    CHECK(count(s, fg(kHeart) + "♥") == 2);
    CHECK(count(s, "♦") == 3);
    CHECK(count(s, fg(kJoker) + "♦") == 1);
    CHECK_FALSE(contains(s, "Kombo"));
}

TEST_CASE("Bildschirm: Extraleben über die Startzahl werden als volle Herzen gezeigt") {
    HudInfo hud = sample_hud();
    hud.lives = 5;
    const std::string s = render_screen(kEmpty, hud, 120);
    CHECK(count(s, fg(kHeart) + "♥") == 5);
    CHECK(count(s, "♥") == 5);
}

TEST_CASE("Bildschirm: Kombo mit Multiplikator erscheint ab zwei Treffern") {
    HudInfo hud = sample_hud();
    hud.combo = 7;
    hud.multiplier = 2;
    const std::string s = render_screen(kEmpty, hud, 120);
    CHECK(contains(s, "Kombo 7"));
    CHECK(contains(s, "×2"));
}

TEST_CASE("Bildschirm: Streifen zweizeilig, breite Zellen wenn Platz ist, unbeleuchtet als dunkler Block") {
    std::vector<Rgb> frame(30, kBlack);
    frame[3] = to_rgb(Color::blue);
    frame[9] = to_rgb(Color::red);
    const std::string wide = render_screen(frame, sample_hud(), 120);
    CHECK(count(wide, fg(to_rgb(Color::blue)) + "███") == 2);  // 104 Spalten / 30 Zellen = 3, zwei Zeilen
    CHECK(count(wide, fg(Rgb{38, 38, 46}) + "███") == 2 * 28);
    const std::string narrow = render_screen(frame, sample_hud(), 44);
    CHECK(count(narrow, fg(to_rgb(Color::blue)) + "█") == 2);
    CHECK_FALSE(contains(narrow, fg(to_rgb(Color::blue)) + "██"));
}

TEST_CASE("Bildschirm: Effektbereich in derselben Zeile hinter dem Computerende, beschriftet") {
    HudInfo hud = sample_hud();
    hud.zone = std::vector<Rgb>(8, Rgb{0, 200, 255});
    const std::string s = render_screen(std::vector<Rgb>(21, kBlack), hud, 120);
    CHECK(count(s, fg(Rgb{0, 200, 255}) + "██") == 2 * 8);
    CHECK(contains(s, "Computer ▲"));
    CHECK(contains(s, "Effekte"));
    const auto lines = lines_of(s);
    bool same_row = false;
    for (const std::string& l : lines) {
        same_row = same_row || (contains(l, fg(Rgb{38, 38, 46})) && contains(l, fg(Rgb{0, 200, 255})));
    }
    CHECK(same_row);
}

TEST_CASE("Bildschirm: gesperrte Farben nennen die Stufe, ab der sie kommen") {
    const std::string s = render_screen(kEmpty, sample_hud(), 120);
    CHECK(contains(s, "Gelb\x1b[0m"));
    CHECK(contains(s, " (ab 4)"));
    CHECK(contains(s, " (ab 7)"));
    CHECK(count(s, "(ab ") == 2);
    CHECK(contains(s, "[Leertaste] Joker"));
}

TEST_CASE("Bildschirm: Endgegner zeigt Name, Lebensleiste, Zähler und Wut; Warnung ohne Leiste; sonst der nächste") {
    HudInfo hud = sample_hud();
    CHECK(contains(render_screen(kEmpty, hud, 120), "der nächste wartet auf Stufe 3"));
    hud.boss = BossBar{"Farbwurm", "Lang und zäh.", 4, 10, true, false};
    const std::string s = render_screen(kEmpty, hud, 120);
    CHECK(contains(s, "ENDGEGNER Farbwurm"));
    CHECK(contains(s, "4/10"));
    CHECK(contains(s, "WÜTEND!"));
    CHECK(contains(s, "Lang und zäh."));
    CHECK(count(s, "░") == 6);
    hud.boss->warning = true;
    const std::string w = render_screen(kEmpty, hud, 120);
    CHECK(contains(w, "ACHTUNG – Endgegner naht: Farbwurm"));
    CHECK(count(w, "░") == 0);
}

TEST_CASE("Bildschirm: Meldung in ihrer Farbe; Spielende zeigt Ergebnis und Bestenliste; Pause verdrängt") {
    HudInfo hud = sample_hud();
    hud.message = "Treffer! +20";
    hud.message_color = Rgb{120, 230, 120};
    CHECK(contains(render_screen(kEmpty, hud, 120), fg(Rgb{120, 230, 120}) + "\x1b[1m► Treffer! +20"));
    hud.over = true;
    hud.highscore_lines = {"Bestenliste:", "  1.  1.240 Punkte, Stufe 2, 2026-10-18   ← neu"};
    const std::string over = render_screen(kEmpty, hud, 120);
    CHECK(contains(over, "Spiel vorbei – 1.240 Punkte, Stufe 2."));
    CHECK(contains(over, "← neu"));
    CHECK_FALSE(contains(over, "Treffer!"));
    hud.over = false;
    hud.paused = true;
    CHECK(contains(render_screen(kEmpty, hud, 120), "PAUSE"));
}

TEST_CASE("Testbild-Ansicht: Titel, Streifen zweizeilig und alle Hinweiszeilen im Rahmen") {
    const std::vector<std::string> lines{"Zeile eins", "Zeile zwei"};
    const std::vector<Rgb> frame(12, kWhite);
    const std::string s = render_calibration(frame, lines, 120);
    CHECK(contains(s, "TESTBILD"));
    CHECK(contains(s, "Zeile eins"));
    CHECK(contains(s, "Zeile zwei"));
    CHECK(count(s, fg(kWhite) + "████") == 24);
    const auto rows = lines_of(s);
    for (std::size_t i = 0; i + 1 < rows.size(); ++i) {
        CHECK(visible_width(rows[i]) == visible_width(rows[0]));
    }
}

TEST_CASE("Zeilenvergleich: erst alles, dann nur Geändertes, unverändert gar nichts") {
    ScreenDiffer differ;
    const std::string a = "\x1b[Hzeile eins\x1b[K\nzeile zwei\x1b[K\nzeile drei\x1b[K\n\x1b[0m\x1b[J";
    const std::string first = differ.update(a);
    CHECK(contains(first, "\x1b[1;1Hzeile eins"));
    CHECK(contains(first, "\x1b[3;1Hzeile drei"));
    CHECK(differ.update(a).empty());
    const std::string b = "\x1b[Hzeile eins\x1b[K\nzeile ZWEI\x1b[K\nzeile drei\x1b[K\n\x1b[0m\x1b[J";
    const std::string change = differ.update(b);
    CHECK(change == "\x1b[2;1Hzeile ZWEI\x1b[K");
    const std::string shorter = differ.update("\x1b[Hzeile eins\x1b[K\n\x1b[0m\x1b[J");
    CHECK(contains(shorter, "\x1b[2;1H\x1b[0m\x1b[J"));  // Rest darunter gelöscht
    differ.reset();
    CHECK(contains(differ.update(a), "\x1b[2;1Hzeile zwei"));  // nach reset wieder alles
}

TEST_CASE("Zeilenvergleich: ein echtes Spielbild mit nur neuer Punktzahl ändert genau eine Zeile") {
    ScreenDiffer differ;
    HudInfo hud = sample_hud();
    differ.update(render_screen(kEmpty, hud, 120));
    hud.score = 1250;
    const std::string change = differ.update(render_screen(kEmpty, hud, 120));
    CHECK(count(change, ";1H") == 1);
    CHECK(contains(change, "Punkte 1.250"));
}

TEST_CASE("Automodus-Anzeige: Titel mit Tempo, Tastenhilfe fürs Zuschauen, Zeile mit Partie und bestem Lauf") {
    HudInfo hud = sample_hud();
    hud.mode_label = "AUTO ×4";
    hud.auto_mode = true;
    hud.info_line = "Partie 3 · bester Lauf: Stufe 11, 24.310 Punkte";
    const std::string s = render_screen(kEmpty, hud, 120);
    CHECK(contains(s, "LICHTDUELL · AUTO ×4"));
    CHECK(contains(s, "[A] selbst spielen"));
    CHECK(contains(s, "[← / →] Tempo"));
    CHECK_FALSE(contains(s, "[A] Computer spielt"));
    CHECK(contains(render_screen(kEmpty, sample_hud(), 120), "[A] Computer spielt"));
    CHECK(contains(s, "[Enter] neue Partie"));
    CHECK_FALSE(contains(s, "[Leertaste] Joker"));
    CHECK(contains(s, "Partie 3 · bester Lauf: Stufe 11, 24.310 Punkte"));
    const auto lines = lines_of(s);
    for (std::size_t i = 0; i + 1 < lines.size(); ++i) {
        CHECK(visible_width(lines[i]) == visible_width(lines[0]));
    }
}

TEST_CASE("Sichtbare Breite: Steuerfolgen zählen nicht, Umlaute und Sonderzeichen je eine Spalte") {
    CHECK(visible_width("abc") == 3);
    CHECK(visible_width("\x1b[38;2;1;2;3mGrün\x1b[0m") == 4);
    CHECK(visible_width("╔═♥♦▲►█░·–") == 10);
    CHECK(visible_width("\x1b]0;Titel\x07x") == 1);
}

TEST_CASE("Tasten: Ziffern, Ziffernblock und Buchstaben wählen dieselbe Farbe") {
    CHECK(color_for_key('1') == Color::red);
    CHECK(color_for_key('R') == Color::red);
    CHECK(color_for_key(VK_NUMPAD2) == Color::green);
    CHECK(color_for_key('B') == Color::blue);
    CHECK(color_for_key('4') == Color::yellow);
    CHECK(color_for_key('Y') == Color::yellow);
    CHECK(color_for_key(VK_NUMPAD5) == Color::purple);
    CHECK_FALSE(color_for_key('Q').has_value());
}

TEST_CASE("Tasten: Esc beendet, P pausiert, Enter startet neu, Leertaste Joker, Pfeile und S fürs Testbild") {
    CHECK(action_for_key(VK_ESCAPE) == Action::quit);
    CHECK(action_for_key('P') == Action::pause);
    CHECK(action_for_key(VK_RETURN) == Action::restart);
    CHECK(action_for_key(VK_SPACE) == Action::joker);
    CHECK(action_for_key(VK_LEFT) == Action::left);
    CHECK(action_for_key(VK_RIGHT) == Action::right);
    CHECK(action_for_key(VK_UP) == Action::up);
    CHECK(action_for_key(VK_DOWN) == Action::down);
    CHECK(action_for_key('S') == Action::save);
    CHECK(action_for_key('A') == Action::toggle_auto);
    CHECK(action_for_key('1') == Action::none);
}

}  // namespace ldu::console
