// Gemeinsame Helfer der Spieltests.
//
// manual(): kein Nachschub, kein Joker, kein Start-Regenbogen – jeder Test setzt genau
// das aufs Feld, was er prüft, und das Bild enthält nur das.
//
// Threads: Test-Thread.
// Fehler: keine.

#pragma once

#include "game/game.hpp"
#include "game/level.hpp"

#include <cmath>
#include <vector>

namespace ldu::test {

inline GameConfig manual(int cells = 30, int start_level = 1) {
    GameConfig c;
    c.cells = cells;
    c.auto_spawn = false;
    c.jokers = 0;
    c.intro = false;
    c.start_level = start_level;
    return c;
}

// Schreitet in 60-Hz-Bildern fort wie die echte Schleife; die Zahl der Schritte ist
// ganzzahlig, damit sich keine Gleitkommareste zu einem Extraschritt aufsummieren.
inline void run(Game& game, double seconds) {
    const int steps = static_cast<int>(std::lround(seconds * 60.0));
    for (int i = 0; i < steps; ++i) {
        game.update(1.0 / 60.0);
    }
}

inline double enemy_speed(const Game& game) {
    return static_cast<double>(game.cells()) / level_params(game.level()).enemy_crossing_s;
}

inline bool has_event(const std::vector<GameEvent>& events, EventKind kind) {
    for (const GameEvent& e : events) {
        if (e.kind == kind) {
            return true;
        }
    }
    return false;
}

inline const Blob* find_kind(const Game& game, BlobKind kind) {
    for (const Blob& b : game.blobs()) {
        if (b.kind == kind) {
            return &b;
        }
    }
    return nullptr;
}

}  // namespace ldu::test
