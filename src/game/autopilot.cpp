#include "game/autopilot.hpp"

#include <algorithm>
#include <vector>

namespace ldu {
namespace {

inline constexpr double kSwitchMarginS = 0.07;  // Abstand zum Chamäleon-Wechsel, der noch sicher ist

AutopilotProfile with_error(double error_rate) {
    AutopilotProfile p;
    p.error_rate = error_rate;
    return p;
}

}  // namespace

Autopilot::Autopilot(std::uint32_t seed, AutopilotProfile profile) : rng_(seed), profile_(profile) {}

Autopilot::Autopilot(std::uint32_t seed, double error_rate) : Autopilot(seed, with_error(error_rate)) {}

std::optional<Autopilot::Move> Autopilot::decide(const Game& game, double dt_s) {
    wait_s_ -= dt_s;
    if (wait_s_ > 0.0 || !game.ready() || game.blobs().empty()) {
        return std::nullopt;
    }
    const auto& blobs = game.blobs();
    const auto nearest = std::min_element(blobs.begin(), blobs.end(),
                                          [](const Blob& a, const Blob& b) { return a.head < b.head; });
    Blob target = *nearest;

    // Schüsse in der Luft vorwegnehmen, der weiteste zuerst – wie Game sie auflöst.
    std::vector<Shot> flying = game.shots();
    std::sort(flying.begin(), flying.end(), [](const Shot& a, const Shot& b) { return a.pos > b.pos; });
    for (const Shot& s : flying) {
        if (target.kind == BlobKind::gold || target.colors.empty()) {
            // Gold ist schon getroffen bzw. der Klumpen schon weg: abwarten, bis es sich
            // aufgelöst hat, statt auf einen Klumpen dahinter zu raten.
            return std::nullopt;
        }
        const double impact_s = std::max(0.0, target.head - 1.0 - s.pos) / (game.shot_speed() + game.blob_speed(target));
        const bool front_shifting = static_cast<int>(target.colors.size()) <= target.shifting;
        const Color front = front_shifting ? game.chameleon_color_in(impact_s) : target.colors.front();
        if (s.joker || s.color == front) {
            target.colors.erase(target.colors.begin());
            target.shifting = std::min(target.shifting, static_cast<int>(target.colors.size()));
        } else {
            target.colors.insert(target.colors.begin(), s.color);
        }
    }
    if (target.colors.empty()) {
        return std::nullopt;
    }

    Move move;
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    if (target.kind == BlobKind::gold) {
        move.color = Color::red;
    } else {
        const double impact_s = std::max(0.0, target.head - 1.0) / (game.shot_speed() + game.blob_speed(target));
        const bool front_shifting = static_cast<int>(target.colors.size()) <= target.shifting;
        Color want = front_shifting ? game.chameleon_color_in(impact_s) : target.colors.front();
        const bool danger = target.head < profile_.danger_cells;
        if (danger && game.jokers() > 0) {
            move.joker = true;
        } else if (front_shifting && (game.chameleon_color_in(impact_s - kSwitchMarginS) != want ||
                                      game.chameleon_color_in(impact_s + kSwitchMarginS) != want)) {
            // Wechsel zu knapp am Einschlag: kurz warten, der nächste Takt ist sicherer.
            wait_s_ = 0.03;
            return std::nullopt;
        } else if (profile_.error_rate > 0.0 && unit(rng_) < profile_.error_rate && game.colors_in_play() > 1) {
            const int offset = 1 + static_cast<int>(rng_() % static_cast<std::uint32_t>(game.colors_in_play() - 1));
            want = static_cast<Color>((static_cast<int>(want) + offset) % game.colors_in_play());
        }
        move.color = want;
    }
    wait_s_ = profile_.min_think_s + profile_.extra_think_s * unit(rng_);
    return move;
}

}  // namespace ldu
