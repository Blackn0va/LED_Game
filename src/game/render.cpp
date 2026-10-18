// Game::render – das Bild für den Streifen, getrennt von den Regeln in game.cpp.
//
// Reihenfolge der Ebenen: Klumpen → Schüsse → Kanone → Warnblinken → Effekte → Vorhang.
// Spätere Ebenen übermalen frühere; Effekte mischen nur, damit darunter liegende Punkte
// erkennbar bleiben.

#include "game/game.hpp"

#include <algorithm>
#include <cmath>

namespace ldu {
namespace {

inline constexpr Rgb kWarnRed{255, 0, 0};
inline constexpr int kWarnCells = 5;
inline constexpr double kCurtainS = 1.0;
inline constexpr double kChameleonFlickerS = 0.25;  // so lange vor dem Wechsel flackert er
inline constexpr double kGunnerChargeS = 0.3;        // so lange vor dem Schuss blitzt sein Kopf
inline constexpr Rgb kZoneLife{230, 0, 30};
inline constexpr Rgb kZoneCombo{0, 200, 255};
inline constexpr Rgb kZoneBoss{255, 90, 0};

int cell_of(double pos) {
    return static_cast<int>(std::floor(pos));
}

// Rechteckiges Blinken mit `hz` Wechseln je Sekunde; true in der ersten Hälfte.
bool blink(double t, double hz) {
    return std::fmod(t * hz, 1.0) < 0.5;
}

}  // namespace

std::vector<Rgb> Game::render() const {
    std::vector<Rgb> frame(static_cast<std::size_t>(cells_), kBlack);
    auto inside = [this](int c) { return c >= 0 && c < cells_; };
    auto at = [&frame](int c) -> Rgb& { return frame[static_cast<std::size_t>(c)]; };

    for (const Blob& b : blobs_) {
        const int front = cell_of(b.head);
        const std::size_t fixed = b.colors.size() - static_cast<std::size_t>(b.shifting);
        for (std::size_t k = 0; k < b.colors.size(); ++k) {
            const int c = front + static_cast<int>(k);
            if (!inside(c)) {
                continue;
            }
            switch (b.kind) {
            case BlobKind::normal:
                at(c) = to_rgb(b.colors[k]);
                break;
            case BlobKind::gold:
                // Funkeln: zwei Helligkeiten im schnellen Wechsel, damit Gold sich auch dann
                // von der Spielfarbe Gelb abhebt, wenn der Streifen beide ähnlich zeigt.
                at(c) = scale(kGold, blink(elapsed_, 12.0) ? 1.0 : 0.45);
                break;
            case BlobKind::boss: {
                // Endgegner atmen: Helligkeit pulsiert, wütend doppelt so schnell.
                const double speed = boss_enraged_ ? 16.0 : 6.0;
                const double pulse = 0.65 + 0.35 * (0.5 + 0.5 * std::sin(elapsed_ * speed + 0.6 * static_cast<double>(k)));
                Rgb px = scale(to_rgb(effective_color(b, k)), pulse);
                if (boss_plan_.type == BossType::chameleon && k >= fixed && boss_timer_ < kChameleonFlickerS &&
                    blink(elapsed_, 24.0)) {
                    px = blend(px, kWhite, 0.6);
                }
                if (boss_plan_.type == BossType::gunner && k == 0 && b.head <= gunner_hold_ + 1e-9 &&
                    boss_timer_ < kGunnerChargeS && blink(elapsed_, 20.0)) {
                    px = blend(px, kWhite, 0.7);
                }
                at(c) = px;
                break;
            }
            }
        }
    }

    for (const Shot& s : shots_) {
        const int c = cell_of(s.pos);
        if (c < 1 || c >= cells_) {
            continue;
        }
        const Rgb color = s.joker ? hue(elapsed_ * 3.0 + s.pos * 0.05) : to_rgb(s.color);
        // Schweif eine Zelle dahinter, damit man die Flugrichtung sieht.
        if (c - 1 >= 1 && at(c - 1) == kBlack) {
            at(c - 1) = scale(color, 0.25);
        }
        at(c) = color;
    }

    if (cooldown_ > 0.0) {
        frame[0] = last_shot_joker_ ? kWhite : to_rgb(last_shot_color_);
    } else if (jokers_ > 0) {
        // Joker bereit: Die Kanone glimmt im langsamen Regenbogen.
        frame[0] = scale(hue(elapsed_ * 0.3), 0.35);
    } else {
        frame[0] = kCannonIdle;
    }

    if (boss_phase_ == BossPhase::warning && blink(elapsed_, 3.5)) {
        for (int c = std::max(1, cells_ - kWarnCells); c < cells_; ++c) {
            at(c) = kWarnRed;
        }
    }

    const double cells = static_cast<double>(cells_);
    for (const Effect& e : effects_) {
        if (e.age < 0.0) {
            continue;
        }
        const double progress = e.age / e.duration;
        const double fade = 1.0 - progress;
        switch (e.type) {
        case EffectType::flash:
            if (inside(e.cell)) {
                at(e.cell) = blend(at(e.cell), e.color, fade);
            }
            break;
        case EffectType::burst: {
            const double radius = 1.0 + 3.0 * progress;
            const int reach = static_cast<int>(std::ceil(radius));
            for (int d = -reach; d <= reach; ++d) {
                const int c = e.cell + d;
                const double dist = std::abs(static_cast<double>(d));
                if (c < 1 || c >= cells_ || dist > radius) {
                    continue;
                }
                at(c) = blend(at(c), e.color, fade * (1.0 - dist / (radius + 1.0)));
            }
            break;
        }
        case EffectType::tint:
            for (Rgb& px : frame) {
                px = blend(px, e.color, 0.6 * fade);
            }
            break;
        case EffectType::sweep: {
            // Grünes Band läuft von der Kanone zum Computer: „Stufe geschafft“.
            const double center = progress * cells;
            for (int c = 1; c < cells_; ++c) {
                const double d = std::abs(static_cast<double>(c) - center);
                if (d < 3.0) {
                    at(c) = blend(at(c), e.color, 0.8 * (1.0 - d / 3.0));
                }
            }
            break;
        }
        case EffectType::rainbow: {
            // Start: Regenbogen füllt den Streifen von der Kanone aus und verblasst.
            const double reach = std::min(1.0, progress * 2.0) * cells;
            for (int c = 1; c < cells_ && static_cast<double>(c) <= reach; ++c) {
                at(c) = blend(at(c), hue(static_cast<double>(c) / cells + elapsed_ * 0.5), 0.8 * fade);
            }
            break;
        }
        }
    }

    if (over_) {
        // Spielende: roter Vorhang fällt vom Computer zur Kanone und pulsiert dann.
        const double reach = std::min(1.0, over_time_ / kCurtainS) * cells;
        const Rgb glow = scale(kWarnRed, 0.15 + 0.1 * std::sin(elapsed_ * 4.0));
        for (int c = 0; c < cells_; ++c) {
            if (static_cast<double>(c) >= cells - reach) {
                at(c) = glow;
            }
        }
    }
    return frame;
}

std::vector<Rgb> Game::render_zone(int length) const {
    std::vector<Rgb> zone(static_cast<std::size_t>(std::max(length, 0)), kBlack);
    if (zone.empty()) {
        return zone;
    }
    const int n = static_cast<int>(zone.size());
    auto at = [&zone](int i) -> Rgb& { return zone[static_cast<std::size_t>(i)]; };

    const auto boss_now = boss();
    if (over_) {
        std::fill(zone.begin(), zone.end(), scale(kWarnRed, 0.2 + 0.15 * std::sin(elapsed_ * 3.0)));
    } else if (boss_now && boss_now->warning) {
        if (blink(elapsed_, 3.5)) {
            std::fill(zone.begin(), zone.end(), kWarnRed);
        }
    } else if (boss_now) {
        // Lebensleiste des Endgegners; pulsiert, wütend schneller.
        const int max = std::max(1, boss_now->max_segments);
        const int filled = std::clamp((std::min(boss_now->segments, max) * n + max - 1) / max, 0, n);
        const double pulse = 0.6 + 0.4 * (0.5 + 0.5 * std::sin(elapsed_ * (boss_now->enraged ? 16.0 : 6.0)));
        for (int i = 0; i < n; ++i) {
            at(i) = i < filled ? scale(kZoneBoss, pulse) : scale(kZoneBoss, 0.08);
        }
    } else {
        // Status von der Ecke aus: Leben, Lücke, Joker, Lücke, Kombo-Leiste zum nächsten Joker.
        int i = 0;
        for (int l = 0; l < lives_ && i < n; ++l) {
            at(i++) = kZoneLife;
        }
        ++i;
        for (int j = 0; j < jokers_ && i < n; ++j) {
            at(i++) = kGold;
        }
        ++i;
        if (i < n) {
            const int bar = n - i;
            const int lit = jokers_ >= kMaxJokers ? 0 : (combo_ % kComboPerJoker) * bar / kComboPerJoker;
            for (int k = 0; k < bar; ++k) {
                at(i + k) = k < lit ? kZoneCombo : scale(kZoneCombo, 0.06);
            }
        }
    }

    for (const ZoneEffect& z : zone_effects_) {
        const double progress = z.age / z.duration;
        const double fade = 1.0 - progress;
        switch (z.type) {
        case ZoneEffectType::flash:
            for (Rgb& px : zone) {
                px = blend(px, z.color, 0.8 * fade);
            }
            break;
        case ZoneEffectType::sweep: {
            const double center = progress * static_cast<double>(n);
            for (int i = 0; i < n; ++i) {
                const double d = std::abs(static_cast<double>(i) - center);
                if (d < 2.0) {
                    at(i) = blend(at(i), z.color, 0.9 * (1.0 - d / 2.0));
                }
            }
            break;
        }
        case ZoneEffectType::rainbow:
            // Sieg: laufender Regenbogen, der erst im letzten Drittel verblasst.
            for (int i = 0; i < n; ++i) {
                at(i) = blend(at(i), hue(static_cast<double>(i) / static_cast<double>(n) + elapsed_ * 1.5),
                              std::min(1.0, 3.0 * fade));
            }
            break;
        case ZoneEffectType::sparkle:
            for (int i = 0; i < n; ++i) {
                if ((i + static_cast<int>(elapsed_ * 20.0)) % 3 == 0) {
                    at(i) = blend(at(i), z.color, fade);
                }
            }
            break;
        }
    }
    return zone;
}

}  // namespace ldu
