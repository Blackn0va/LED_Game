#include "sound/sfx.hpp"

namespace ldu::sound {

std::vector<Tone> sfx_tones(Sfx sfx) {
    constexpr Wave sq = Wave::square;
    constexpr Wave si = Wave::sine;
    switch (sfx) {
    case Sfx::shot:
        return {{880, 0.03, 0.20, sq}};
    case Sfx::joker_shot:
        return {{1320, 0.03, 0.25, si}, {1760, 0.03, 0.25, si}, {2350, 0.04, 0.25, si}};
    case Sfx::hit:
        return {{660, 0.04, 0.30, sq}, {990, 0.05, 0.30, sq}};
    case Sfx::clear:
        return {{660, 0.05, 0.35, sq}, {880, 0.05, 0.35, sq}, {1320, 0.09, 0.35, sq}};
    case Sfx::stuck:
        return {{150, 0.10, 0.40, sq}, {110, 0.12, 0.40, sq}};
    case Sfx::life_lost:
        return {{440, 0.10, 0.45, sq}, {330, 0.10, 0.45, sq}, {220, 0.22, 0.45, sq}};
    case Sfx::level_up:
        return {{523, 0.07, 0.35, sq}, {659, 0.07, 0.35, sq}, {784, 0.07, 0.35, sq}, {1047, 0.16, 0.35, sq}};
    case Sfx::boss_warning:
        return {{220, 0.16, 0.45, sq}, {0, 0.06, 0, sq}, {220, 0.16, 0.45, sq}, {0, 0.06, 0, sq},
                {165, 0.35, 0.45, sq}};
    case Sfx::boss_shot:
        return {{300, 0.05, 0.25, sq}};
    case Sfx::boss_bite:
        return {{110, 0.08, 0.50, sq}, {82, 0.25, 0.50, sq}};
    case Sfx::boss_defeated:
        return {{523, 0.09, 0.40, sq}, {659, 0.09, 0.40, sq}, {784, 0.09, 0.40, sq},
                {1047, 0.12, 0.40, sq}, {784, 0.08, 0.40, sq}, {1047, 0.32, 0.40, sq}};
    case Sfx::joker:
        return {{1568, 0.05, 0.30, si}, {2093, 0.10, 0.30, si}};
    case Sfx::gold:
        return {{1319, 0.06, 0.35, si}, {1760, 0.12, 0.35, si}};
    case Sfx::game_over:
        return {{392, 0.20, 0.45, sq}, {330, 0.20, 0.45, sq}, {262, 0.20, 0.45, sq}, {196, 0.50, 0.45, sq}};
    }
    return {};
}

int sfx_priority(Sfx sfx) {
    switch (sfx) {
    case Sfx::shot:
    case Sfx::joker_shot:
    case Sfx::boss_shot:
        return 0;
    case Sfx::hit:
        return 1;
    case Sfx::clear:
    case Sfx::stuck:
        return 2;
    case Sfx::joker:
    case Sfx::gold:
        return 3;
    case Sfx::level_up:
    case Sfx::life_lost:
    case Sfx::boss_bite:
        return 4;
    case Sfx::boss_warning:
    case Sfx::boss_defeated:
    case Sfx::game_over:
        return 5;
    }
    return 0;
}

bool SoundScheduler::admit(Sfx sfx, double now_s, double duration_s) {
    const int p = sfx_priority(sfx);
    if (now_s < busy_until_s_ && p < priority_) {
        return false;
    }
    busy_until_s_ = now_s + duration_s;
    priority_ = p;
    return true;
}

}  // namespace ldu::sound
