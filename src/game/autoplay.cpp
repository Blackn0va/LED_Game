#include "game/autoplay.hpp"

#include <algorithm>
#include <cmath>

namespace ldu {

AutoPreset auto_preset(AutoSpeed speed) {
    AutoPreset p;
    switch (speed) {
    case AutoSpeed::slow:
        p.tempo = 0.5;
        p.profile = AutopilotProfile{0.03, 0.18, 0.20, 4.0};
        p.restart_s = 8.0;
        p.name = "langsam";
        break;
    case AutoSpeed::normal:
        p.tempo = 1.0;
        p.profile = AutopilotProfile{0.05, 0.08, 0.12, 4.0};
        p.restart_s = 6.0;
        p.name = "normal";
        break;
    case AutoSpeed::fast:
        p.tempo = 4.0;
        p.profile = AutopilotProfile{0.01, 0.02, 0.03, 5.0};
        p.restart_s = 3.0;
        p.name = "schnell";
        break;
    }
    return p;
}

std::size_t nearest_tempo_step(double tempo) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < kTempoSteps.size(); ++i) {
        if (std::abs(kTempoSteps[i] - tempo) < std::abs(kTempoSteps[best] - tempo)) {
            best = i;
        }
    }
    return best;
}

std::string tempo_label(double tempo) {
    const long hundredths = std::lround(tempo * 100.0);
    std::string s = "×" + std::to_string(hundredths / 100);
    long frac = hundredths % 100;
    if (frac != 0) {
        s += ",";
        if (frac < 10) {
            s += "0";
        }
        if (frac % 10 == 0) {
            frac /= 10;
        }
        s += std::to_string(frac);
    }
    return s;
}

void GameClock::slow_motion(double now_s, double duration_s, double factor) {
    slow_until_s_ = now_s + duration_s;
    slow_factor_ = std::clamp(factor, 0.05, 1.0);
}

int GameClock::advance(double now_s, double real_dt_s, int max_steps) {
    if (!(real_dt_s > 0.0)) {
        return 0;
    }
    const double factor = slow_motion_active(now_s) ? slow_factor_ : 1.0;
    accumulator_s_ += std::min(real_dt_s, 0.25) * tempo_ * factor;
    int steps = 0;
    while (accumulator_s_ >= kGameStepS && steps < max_steps) {
        accumulator_s_ -= kGameStepS;
        ++steps;
    }
    if (steps == max_steps) {
        accumulator_s_ = std::min(accumulator_s_, kGameStepS);
    }
    return steps;
}

}  // namespace ldu
