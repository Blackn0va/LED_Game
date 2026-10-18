// Automodus: Voreinstellungen, Tempostufen, Spieltakt mit Zeitlupe, Persönlichkeiten im Spiel.

#include "game/autopilot.hpp"
#include "game/autoplay.hpp"
#include "game/game.hpp"

#include <doctest.h>

// doctest gibt string_view bei Fehlschlag über operator<< aus (Falle, Gedaechtnis.md).
#include <ostream>

namespace ldu {
namespace {

// Spielt mit `profile` bis `until_level` oder Spielende, höchstens `seconds` Spielzeit.
Game play(AutopilotProfile profile, std::uint32_t seed, int until_level, double seconds) {
    GameConfig c;
    c.cells = 21;  // wie am Streifen des Nutzers
    c.seed = seed;
    Game game(c);
    Autopilot pilot(seed + 1, profile);
    const int steps = static_cast<int>(seconds / kGameStepS);
    for (int i = 0; i < steps && game.level() < until_level && !game.over(); ++i) {
        if (const auto move = pilot.decide(game, kGameStepS)) {
            if (move->joker) {
                game.fire_joker();
            } else {
                game.fire(move->color);
            }
        }
        game.update(kGameStepS);
    }
    return game;
}

}  // namespace

TEST_CASE("Automodus: langsam ist langsamer und gemächlicher als schnell") {
    const AutoPreset slow = auto_preset(AutoSpeed::slow);
    const AutoPreset normal = auto_preset(AutoSpeed::normal);
    const AutoPreset fast = auto_preset(AutoSpeed::fast);
    CHECK(slow.tempo < normal.tempo);
    CHECK(normal.tempo < fast.tempo);
    CHECK(fast.tempo >= 4.0);
    CHECK(slow.profile.min_think_s > fast.profile.min_think_s);
    CHECK(slow.restart_s > fast.restart_s);
    CHECK(slow.name == "langsam");
    CHECK(fast.name == "schnell");
}

TEST_CASE("Tempo: Stufen und Beschriftung mit deutschem Komma") {
    CHECK(tempo_label(0.25) == "×0,25");
    CHECK(tempo_label(0.5) == "×0,5");
    CHECK(tempo_label(1.0) == "×1");
    CHECK(tempo_label(1.5) == "×1,5");
    CHECK(tempo_label(8.0) == "×8");
    CHECK(kTempoSteps[nearest_tempo_step(0.5)] == 0.5);
    CHECK(kTempoSteps[nearest_tempo_step(4.0)] == 4.0);
    CHECK(kTempoSteps[nearest_tempo_step(5.0)] == 4.0);
    CHECK(nearest_tempo_step(100.0) == kTempoSteps.size() - 1);
}

TEST_CASE("Spieltakt: Tempo vervielfacht die Schritte, Reste wandern ins nächste Bild") {
    GameClock normal(1.0);
    GameClock fast(4.0);
    int n = 0;
    int f = 0;
    for (int i = 0; i < 60; ++i) {  // eine Sekunde Echtzeit in 60 Bildern
        n += normal.advance(0.0, 1.0 / 60.0);
        f += fast.advance(0.0, 1.0 / 60.0);
    }
    CHECK(n >= 59);
    CHECK(n <= 60);
    CHECK(f >= 239);
    CHECK(f <= 240);
    CHECK(normal.advance(0.0, 0.0) == 0);
}

TEST_CASE("Spieltakt: Zeitlupe dämpft für ihre Dauer, danach wieder volles Tempo") {
    GameClock clock(1.0);
    clock.slow_motion(0.0, 1.0, 0.25);
    CHECK(clock.slow_motion_active(0.5));
    const int slow = clock.advance(0.5, 0.2);  // 0,2 s × 0,25 = 0,05 s Spielzeit
    CHECK(slow >= 2);
    CHECK(slow <= 3);
    CHECK_FALSE(clock.slow_motion_active(1.5));
    const int full = clock.advance(1.5, 0.2);  // 0,2 s Spielzeit
    CHECK(full >= 12);
    CHECK(full <= 13);
}

TEST_CASE("Spieltakt: nach einem Hänger wird nicht nachgeholt") {
    GameClock clock(8.0);
    CHECK(clock.advance(0.0, 5.0, 40) == 40);  // gekappt
    CHECK(clock.advance(0.0, 1.0 / 60.0, 40) <= 9);  // kein Rückstau von Hunderten Schritten
}

TEST_CASE("Automodus schnell: der Blitzspieler schlägt alle drei Endgegner (Stufe 10)") {
    const Game game = play(auto_preset(AutoSpeed::fast).profile, 31, 10, 900.0);
    CHECK(game.level() >= 10);
    CHECK(game.stats().bosses >= 3);
    CHECK_FALSE(game.over());
}

TEST_CASE("Automodus langsam: der ruhige Spieler schafft wenigstens den Farbwurm (Stufe 4)") {
    const Game game = play(auto_preset(AutoSpeed::slow).profile, 31, 4, 900.0);
    CHECK(game.level() >= 4);
    CHECK(game.stats().bosses >= 1);
}

}  // namespace ldu
