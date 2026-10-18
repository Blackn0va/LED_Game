// Computerspieler für die Vorführung (--demo) und für Tests, die ganze Partien spielen.
//
// Er sieht nur, was ein Mensch auf dem Streifen sieht (über die öffentliche Sicht von
// Game) und nimmt Schüsse vorweg, die schon in der Luft sind – sonst schösse er dieselbe
// Farbe zweimal und steckte sich selbst an. Beim Chamäleon rechnet er den Farbwechsel bis
// zum Einschlag mit und wartet, wenn der Wechsel knapp davor oder danach liegt. Kommt ein
// Klumpen der Kanone zu nahe, nimmt er einen Joker. error_rate > 0 lässt ihn absichtlich
// danebenschießen, damit die Vorführung das Anstecken zeigt.
//
// Threads: ein Objekt, ein Thread.
// Fehler: keine.

#pragma once

#include "game/game.hpp"

#include <cstdint>
#include <optional>
#include <random>

namespace ldu {

// Persönlichkeit des Computerspielers. Zeiten in Spielzeit – bei Tempo ×4 reagiert er also
// in Echtzeit viermal so schnell, im selben Verhältnis wie das Spiel läuft.
struct AutopilotProfile final {
    double error_rate = 0.0;     // Anteil absichtlich falscher Farben
    double min_think_s = 0.08;   // Reaktionszeit: mindestens …
    double extra_think_s = 0.12; // … plus bis zu so viel Zufall
    double danger_cells = 4.0;   // so nah an der Kanone greift er zum Joker
};

class Autopilot final {
public:
    struct Move final {
        bool joker = false;
        Color color = Color::red;
    };

    Autopilot(std::uint32_t seed, AutopilotProfile profile);
    // Standardprofil mit eigener Fehlerquote (Demo, Tests).
    Autopilot(std::uint32_t seed, double error_rate);

    // Nächster Zug oder nullopt (nachdenken, warten, nichts zu tun). dt_s ist die Zeit seit
    // dem letzten Aufruf; darüber läuft die Reaktionszeit.
    std::optional<Move> decide(const Game& game, double dt_s);

private:
    std::mt19937 rng_;
    AutopilotProfile profile_;
    double wait_s_ = 0.0;
};

}  // namespace ldu
