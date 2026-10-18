#include "game/level.hpp"

#include "core/rgb.hpp"
#include "game/boss.hpp"

#include <algorithm>

namespace ldu {

LevelParams level_params(int level) {
    const int l = std::max(level, 1);
    const double step = static_cast<double>(l - 1);
    LevelParams p;
    // Stufe 1: 14 s für die Strecke, je Stufe 1,2 s weniger, nie unter 4 s. Startwerte,
    // am Streifen nicht ausbalanciert (Gedaechtnis.md → Offen); dort kommt die
    // Übertragungslatenz noch dazu.
    p.enemy_crossing_s = std::max(4.0, 14.0 - 1.2 * step);
    p.spawn_interval_s = std::max(1.0, 3.5 - 0.25 * step);
    p.colors = l <= 3 ? 3 : (l <= 6 ? 4 : kColorCount);
    p.max_blob_size = l <= 2 ? 1 : (l <= 5 ? 2 : 3);
    p.blobs_to_next = 6 + l;
    p.boss = is_boss_level(l);
    return p;
}

int first_level_with_colors(int colors) {
    for (int l = 1; l < 100; ++l) {
        if (level_params(l).colors >= colors) {
            return l;
        }
    }
    return 100;
}

}  // namespace ldu
