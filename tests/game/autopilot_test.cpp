// Autopilot und Testbild: ganze Partien ohne Tastatur, Bild zum Einrichten.

#include "game/autopilot.hpp"
#include "game/calibration.hpp"
#include "game/game.hpp"

#include <doctest.h>

namespace ldu {
namespace {

// Spielt bis `until_level` oder Spielende, höchstens `seconds`; 60 Hz wie die Schleife.
void autoplay(Game& game, Autopilot& pilot, int until_level, double seconds) {
    const int steps = static_cast<int>(seconds * 60.0);
    for (int i = 0; i < steps && game.level() < until_level && !game.over(); ++i) {
        if (const auto move = pilot.decide(game, 1.0 / 60.0)) {
            if (move->joker) {
                game.fire_joker();
            } else {
                game.fire(move->color);
            }
        }
        game.update(1.0 / 60.0);
    }
}

}  // namespace

TEST_CASE("Autopilot: fehlerfrei schlägt er den Farbwurm und erreicht Stufe 4 ohne einen Fehlschuss") {
    GameConfig c;
    c.cells = 30;
    c.seed = 11;
    Game game(c);
    Autopilot pilot(5, 0.0);
    autoplay(game, pilot, 4, 150.0);
    CHECK(game.level() >= 4);
    CHECK(game.stats().bosses >= 1);
    CHECK_FALSE(game.over());
    CHECK(game.stats().misses == 0);
}

TEST_CASE("Autopilot: fehlerfrei schlägt er auch Chamäleon und Kanonier (Stufe 10)") {
    GameConfig c;
    c.cells = 30;
    c.seed = 23;
    Game game(c);
    Autopilot pilot(7, 0.0);
    autoplay(game, pilot, 10, 600.0);
    CHECK(game.level() >= 10);
    CHECK(game.stats().bosses >= 3);
    CHECK_FALSE(game.over());
}

TEST_CASE("Autopilot: mit Fehlerquote schießt er absichtlich daneben – die Demo zeigt das Anstecken") {
    GameConfig c;
    c.cells = 30;
    c.seed = 3;
    Game game(c);
    Autopilot pilot(9, 0.25);
    autoplay(game, pilot, 99, 60.0);
    CHECK(game.stats().misses > 0);
    CHECK(game.stats().hits > game.stats().misses);
}

TEST_CASE("Testbild: davor aus, Kanone weiß, Feld Grün/Blau mit Lila am Ende, Effektbereich orange") {
    const auto frame = segment_calibration_frame(40, 9, 7, 0.0);
    REQUIRE(frame.size() == 40);
    for (std::size_t u = 0; u < 9; ++u) {
        CHECK(frame[u] == kBlack);
    }
    CHECK(frame[9].r == frame[9].g);  // pulsierendes Weiß
    CHECK(frame[9].r > 0);
    CHECK(frame[10] == scale(to_rgb(Color::green), kCalibrationDim));
    CHECK(frame[11] == scale(to_rgb(Color::blue), kCalibrationDim));
    CHECK(frame[19] == kWhite);                 // 10 Abschnitte hinter der Kanone
    CHECK(frame[32] == to_rgb(Color::purple));  // letzter Feld-Abschnitt = Computer
    CHECK(frame[33] == scale(kZoneMarker, kCalibrationDim));
    CHECK(frame[34] == scale(kZoneMarker, kHiddenDim));
    CHECK(frame[39] == scale(kZoneMarker, kCalibrationDim));
    CHECK(segment_calibration_frame(10, 99, 99, 0.0).size() == 10);
    // Ohne Versatz und Effektbereich: Kanone vorn, Lila ganz hinten.
    const auto plain = segment_calibration_frame(30, 0, 0, 0.0);
    CHECK(plain[0].r > 0);
    CHECK(plain[29] == to_rgb(Color::purple));
}

}  // namespace ldu
