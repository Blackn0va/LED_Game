#include "game/game.hpp"

#include "core/error.hpp"
#include "game/level.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace ldu {
namespace {

inline constexpr double kMaxStepS = 1.0 / 240.0;
inline constexpr double kMaxUpdateS = 0.25;
inline constexpr double kFlashS = 0.12;
inline constexpr double kBurstS = 0.35;
inline constexpr double kTintS = 0.6;
inline constexpr double kSweepS = 0.7;
inline constexpr double kIntroS = 1.4;
inline constexpr Rgb kLifeLostTint{255, 0, 0};
inline constexpr Rgb kLevelUpColor{0, 255, 80};
inline constexpr Rgb kEnrageTint{255, 90, 0};
inline constexpr int kPointsPerHit = 10;
inline constexpr int kPointsPerClear = 50;
inline constexpr int kPointsPerGold = 100;
inline constexpr int kPointsPerBoss = 500;
inline constexpr std::uint32_t kGoldChance = 12;  // im Mittel jeder 12. Nachschub, ab Stufe 2
inline constexpr double kGoldSpeed = 1.4;
inline constexpr double kProjectileSpeed = 1.5;
inline constexpr double kEnrageFactor = 1.7;
inline constexpr double kGunnerHoldShare = 0.55;
inline constexpr double kGunnerFirstShotShare = 0.35;  // erster Schuss nach 35 % des Takts am Standplatz
inline constexpr double kGunnerRetortS = 0.25;         // Gegenschuss so kurz nach einem Treffer
inline constexpr int kBiteSegments = 2;
inline constexpr int kFireworkBursts = 8;

int cell_of(double pos) {
    return static_cast<int>(std::floor(pos));
}

void pop_front(Blob& blob) {
    blob.colors.erase(blob.colors.begin());
    blob.shifting = std::min(blob.shifting, static_cast<int>(blob.colors.size()));
}

}  // namespace

Game::Game(const GameConfig& config)
    : cells_(config.cells),
      auto_spawn_(config.auto_spawn),
      rng_(config.seed),
      lives_(config.lives),
      base_lives_(config.lives),
      jokers_(config.jokers) {
    if (config.cells < kMinCells || config.cells > kMaxCells) {
        throw Error(Error::Kind::usage, "Streifenlänge " + std::to_string(config.cells) + " liegt außerhalb von " +
                                            std::to_string(kMinCells) + " bis " + std::to_string(kMaxCells) +
                                            " Zellen.");
    }
    if (config.lives < 1 || config.lives > kMaxLives) {
        throw Error(Error::Kind::usage, "Leben müssen zwischen 1 und " + std::to_string(kMaxLives) + " liegen.");
    }
    if (config.jokers < 0 || config.jokers > kMaxJokers) {
        throw Error(Error::Kind::usage, "Joker müssen zwischen 0 und " + std::to_string(kMaxJokers) + " liegen.");
    }
    if (config.start_level < 1 || config.start_level > kMaxStartLevel) {
        throw Error(Error::Kind::usage, "Startstufe muss zwischen 1 und " + std::to_string(kMaxStartLevel) + " liegen.");
    }
    level_ = config.start_level;
    if (config.intro) {
        add_effect(EffectType::rainbow, kWhite, 0, kIntroS);
    }
    if (is_boss_level(level_)) {
        enter_boss_warning();
    }
}

int Game::multiplier() const noexcept {
    return 1 + std::min(combo_, 20) / 5;
}

int Game::colors_in_play() const {
    return level_params(level_).colors;
}

bool Game::fire(Color color) {
    if (!ready() || static_cast<int>(color) >= colors_in_play()) {
        return false;
    }
    shots_.push_back(Shot{color, 0.0, false});
    after_fire(color, false);
    return true;
}

bool Game::fire_joker() {
    if (!ready() || jokers_ <= 0) {
        return false;
    }
    --jokers_;
    shots_.push_back(Shot{Color::red, 0.0, true});
    after_fire(Color::red, true);
    return true;
}

void Game::after_fire(Color color, bool joker) {
    cooldown_ = kShotCooldownS;
    last_shot_color_ = color;
    last_shot_joker_ = joker;
    ++stats_.shots;
    events_.push_back({EventKind::fired, color, 0, joker ? 1 : 0});
}

void Game::update(double dt_s) {
    // !(dt > 0) fängt auch NaN ab.
    if (!(dt_s > 0.0)) {
        return;
    }
    double remaining = std::min(dt_s, kMaxUpdateS);
    while (remaining > 0.0) {
        const double dt = std::min(remaining, kMaxStepS);
        step(dt);
        remaining -= dt;
    }
}

std::vector<GameEvent> Game::take_events() {
    return std::exchange(events_, {});
}

void Game::spawn_blob(std::vector<Color> colors, double head) {
    Blob b;
    b.colors = std::move(colors);
    b.head = head;
    spawn_blob(std::move(b));
}

void Game::spawn_blob(Blob blob) {
    if (blob.colors.empty()) {
        return;
    }
    blob.shifting = std::clamp(blob.shifting, 0, static_cast<int>(blob.colors.size()));
    blobs_.push_back(std::move(blob));
}

bool Game::spawn_boss(double head) {
    if (!is_boss_level(level_) || boss_phase_ == BossPhase::fight || over_) {
        return false;
    }
    boss_plan_ = boss_plan(level_, cells_);
    const int n = boss_plan_.segments;
    const auto colors = static_cast<std::uint32_t>(colors_in_play());
    Blob b;
    b.kind = BlobKind::boss;
    b.speed = boss_plan_.speed;
    b.head = head;
    switch (boss_plan_.type) {
    case BossType::worm:
        // Farbläufe von 1–3 gleichen Gliedern: gibt dem Wurm einen Rhythmus, den man
        // „mitschießen“ kann, statt reinem Zufallsrauschen.
        while (static_cast<int>(b.colors.size()) < n) {
            const auto c = static_cast<Color>(rng_() % colors);
            const int run = 1 + static_cast<int>(rng_() % 3u);
            for (int k = 0; k < run && static_cast<int>(b.colors.size()) < n; ++k) {
                b.colors.push_back(c);
            }
        }
        break;
    case BossType::chameleon:
        b.colors.assign(static_cast<std::size_t>(n), Color::red);
        b.shifting = n;
        break;
    case BossType::gunner:
        for (int k = 0; k < n; ++k) {
            b.colors.push_back(static_cast<Color>(rng_() % colors));
        }
        break;
    }
    blobs_.push_back(std::move(b));
    boss_phase_ = BossPhase::fight;
    boss_enraged_ = false;
    chameleon_step_ = 0;
    switch (boss_plan_.type) {
    case BossType::worm:
        boss_timer_ = 0.0;
        break;
    case BossType::chameleon:
        boss_timer_ = boss_plan_.interval_s;
        break;
    case BossType::gunner:
        boss_timer_ = boss_plan_.interval_s * kGunnerFirstShotShare;
        break;
    }
    gunner_hold_ = std::min(std::floor(static_cast<double>(cells_) * kGunnerHoldShare), static_cast<double>(cells_ - n));
    events_.push_back({EventKind::boss_spawned, Color::red, cell_of(head), static_cast<int>(boss_plan_.type)});
    return true;
}

std::optional<BossStatus> Game::boss() const {
    if (boss_phase_ == BossPhase::warning) {
        return BossStatus{boss_plan_.type, boss_plan_.segments, boss_plan_.segments, false, true};
    }
    if (boss_phase_ != BossPhase::fight) {
        return std::nullopt;
    }
    const std::size_t i = boss_index();
    if (i == blobs_.size()) {
        return std::nullopt;
    }
    const Blob& b = blobs_[i];
    const int segments = boss_plan_.type == BossType::chameleon ? b.shifting : static_cast<int>(b.colors.size());
    return BossStatus{boss_plan_.type, segments, boss_plan_.segments, boss_enraged_, false};
}

Color Game::effective_color(const Blob& blob, std::size_t index) const {
    if (index >= blob.colors.size()) {
        return Color::red;
    }
    const std::size_t fixed = blob.colors.size() - static_cast<std::size_t>(blob.shifting);
    return index >= fixed ? chameleon_color_in(0.0) : blob.colors[index];
}

Color Game::chameleon_color_in(double ahead_s) const {
    int step = chameleon_step_;
    if (boss_phase_ == BossPhase::fight && boss_plan_.type == BossType::chameleon && ahead_s >= boss_timer_ &&
        boss_plan_.interval_s > 0.0) {
        step += 1 + static_cast<int>(std::floor((ahead_s - boss_timer_) / boss_plan_.interval_s));
    }
    return static_cast<Color>(step % colors_in_play());
}

double Game::enemy_speed() const {
    return static_cast<double>(cells_) / level_params(level_).enemy_crossing_s;
}

double Game::blob_speed(const Blob& blob) const {
    const double rage = blob.kind == BlobKind::boss && boss_enraged_ ? kEnrageFactor : 1.0;
    return enemy_speed() * blob.speed * rage;
}

double Game::shot_speed() const {
    return static_cast<double>(cells_) / kShotCrossingS;
}

void Game::step(double dt_s) {
    elapsed_ += dt_s;
    for (Effect& e : effects_) {
        e.age += dt_s;
    }
    std::erase_if(effects_, [](const Effect& e) { return e.age >= e.duration; });
    for (ZoneEffect& z : zone_effects_) {
        z.age += dt_s;
    }
    std::erase_if(zone_effects_, [](const ZoneEffect& z) { return z.age >= z.duration; });
    if (over_) {
        over_time_ += dt_s;
        return;
    }
    cooldown_ = std::max(0.0, cooldown_ - dt_s);
    if (boss_phase_ == BossPhase::fight) {
        update_boss(dt_s);
    }
    move(dt_s);
    resolve_collisions();
    check_reached_cannon();
    if (!over_) {
        spawn_tick(dt_s);
    }
}

void Game::move(double dt_s) {
    for (Blob& b : blobs_) {
        const double distance = blob_speed(b) * dt_s;
        if (b.kind == BlobKind::boss && boss_plan_.type == BossType::gunner) {
            // Der Kanonier läuft nur bis zu seinem Standplatz. Rückt er durch angesteckte
            // Fehlschüsse darüber hinaus vor, bleibt er dort stehen.
            if (b.head > gunner_hold_) {
                b.head = std::max(gunner_hold_, b.head - distance);
            }
            continue;
        }
        b.head -= distance;
    }
    const double shot = shot_speed() * dt_s;
    for (Shot& s : shots_) {
        s.pos += shot;
    }
    // Schüsse ins Leere verschwinden am Ende des Streifens.
    const double end = static_cast<double>(cells_);
    std::erase_if(shots_, [end](const Shot& s) { return s.pos >= end; });
}

void Game::update_boss(double dt_s) {
    const std::size_t bi = boss_index();
    if (bi == blobs_.size()) {
        boss_phase_ = BossPhase::none;
        return;
    }
    switch (boss_plan_.type) {
    case BossType::worm:
        break;
    case BossType::chameleon:
        boss_timer_ -= dt_s;
        while (boss_timer_ <= 0.0) {
            ++chameleon_step_;
            boss_timer_ += boss_plan_.interval_s;
        }
        break;
    case BossType::gunner: {
        if (blobs_[bi].head > gunner_hold_ + 1e-9) {
            break;
        }
        boss_timer_ -= dt_s;
        if (boss_timer_ > 0.0) {
            break;
        }
        // Geschoss eine Zelle Abstand vor dem Kanonier. Liegt dort noch ein Klumpen,
        // wartet der Schuss – sonst klebten Geschosse aneinander.
        const double at = blobs_[bi].head - 2.0;
        if (at < 2.0) {
            break;
        }
        for (const Blob& b : blobs_) {
            if (b.kind != BlobKind::boss && b.head + static_cast<double>(b.colors.size()) + 0.5 > at) {
                return;
            }
        }
        Blob p;
        p.colors = {static_cast<Color>(rng_() % static_cast<std::uint32_t>(colors_in_play()))};
        p.head = at;
        p.speed = kProjectileSpeed;
        blobs_.push_back(std::move(p));
        boss_timer_ = boss_plan_.interval_s;
        events_.push_back({EventKind::boss_shot, Color::red, cell_of(at), static_cast<int>(boss_plan_.type)});
        break;
    }
    }
}

void Game::resolve_collisions() {
    // Der am weitesten geflogene Schuss trifft zuerst. Nach einem Fehlschuss rückt der
    // Klumpen eine Zelle vor – ein dicht folgender Schuss trifft dann im selben Schritt
    // schon die neu angesteckte Farbe, genau wie es auf dem Streifen aussieht.
    std::sort(shots_.begin(), shots_.end(), [](const Shot& a, const Shot& b) { return a.pos > b.pos; });
    std::size_t i = 0;
    while (i < shots_.size()) {
        const std::size_t target = nearest_blob();
        if (target == blobs_.size() || shots_[i].pos + 1.0 < blobs_[target].head) {
            ++i;
            continue;
        }
        const Shot shot = shots_[i];
        shots_.erase(shots_.begin() + static_cast<std::ptrdiff_t>(i));
        hit_blob(target, shot);
        if (over_) {
            return;
        }
    }
}

void Game::hit_blob(std::size_t index, const Shot& shot) {
    Blob& blob = blobs_[index];
    const int front = cell_of(blob.head);

    if (blob.kind == BlobKind::gold) {
        blobs_.erase(blobs_.begin() + static_cast<std::ptrdiff_t>(index));
        const int points = kPointsPerGold * level_;
        score_ += points;
        ++stats_.golds;
        jokers_ = std::min(jokers_ + 1, kMaxJokers);
        add_effect(EffectType::burst, kGold, front, kBurstS);
        add_zone_effect(ZoneEffectType::sparkle, kGold, 0.8);
        events_.push_back({EventKind::gold, shot.color, front, points});
        return;
    }

    const bool match = shot.joker || effective_color(blob, 0) == shot.color;
    if (!match) {
        // Falsche Farbe: steckt sich vorn an. Bewusst kein Punktabzug – die Strafe ist der
        // längere, nähere Klumpen und die verlorene Kombo. Wer hier zusätzlich abzieht,
        // bestraft doppelt.
        blob.colors.insert(blob.colors.begin(), shot.color);
        blob.head -= 1.0;
        combo_ = 0;
        ++stats_.misses;
        add_effect(EffectType::flash, blend(to_rgb(shot.color), kWhite, 0.5), front - 1, kFlashS);
        events_.push_back({EventKind::stuck, shot.color, front - 1, 0});
        return;
    }

    pop_front(blob);
    blob.head += 1.0;
    ++combo_;
    ++stats_.hits;
    stats_.best_combo = std::max(stats_.best_combo, combo_);
    const int points = kPointsPerHit * level_ * multiplier();
    score_ += points;
    add_effect(EffectType::flash, kWhite, front, kFlashS);
    events_.push_back({EventKind::hit, shot.joker ? Color::red : shot.color, front, points});
    if (combo_ % kComboPerJoker == 0 && jokers_ < kMaxJokers) {
        ++jokers_;
        add_zone_effect(ZoneEffectType::sparkle, kGold, 0.6);
        events_.push_back({EventKind::joker_gained, Color::red, 0, jokers_});
    }

    if (blob.kind == BlobKind::boss) {
        if (blob.colors.empty()) {
            defeat_boss(index, front);
            return;
        }
        if (boss_plan_.type == BossType::gunner) {
            // Gegenschuss: Wer den Kanonier trifft, bekommt kurz darauf ein Geschoss zurück.
            boss_timer_ = std::min(boss_timer_, kGunnerRetortS);
        }
        if (boss_plan_.type == BossType::worm && !boss_enraged_ &&
            static_cast<int>(blob.colors.size()) * 2 <= boss_plan_.segments) {
            boss_enraged_ = true;
            add_effect(EffectType::tint, kEnrageTint, 0, kTintS);
            add_zone_effect(ZoneEffectType::flash, kEnrageTint, kTintS);
            events_.push_back({EventKind::boss_enraged, Color::red, front, static_cast<int>(boss_plan_.type)});
        }
        return;
    }

    if (blob.colors.empty()) {
        blobs_.erase(blobs_.begin() + static_cast<std::ptrdiff_t>(index));
        const int bonus = kPointsPerClear * level_;
        score_ += bonus;
        add_effect(EffectType::burst, to_rgb(shot.joker ? Color::red : shot.color), front, kBurstS);
        events_.push_back({EventKind::cleared, shot.color, front, bonus});
        ++cleared_in_level_;
        const LevelParams p = level_params(level_);
        if (!p.boss && cleared_in_level_ >= p.blobs_to_next) {
            level_up();
        }
    }
}

void Game::defeat_boss(std::size_t index, int cell) {
    blobs_.erase(blobs_.begin() + static_cast<std::ptrdiff_t>(index));
    const int bonus = kPointsPerBoss * level_;
    score_ += bonus;
    ++stats_.bosses;
    lives_ = std::min(lives_ + 1, kMaxLives);
    jokers_ = std::min(jokers_ + 1, kMaxJokers);
    // Feuerwerk: versetzte Explosionen in Zufallsfarben rund um die Stelle des Siegs.
    for (int k = 0; k < kFireworkBursts; ++k) {
        const int spread = static_cast<int>(rng_() % 13u) - 6;
        const Rgb color = hue(static_cast<double>(rng_() % 360u) / 360.0);
        add_effect(EffectType::burst, color, std::clamp(cell + spread, 1, cells_ - 1), kBurstS * 1.5, 0.12 * k);
    }
    add_zone_effect(ZoneEffectType::rainbow, kWhite, 2.0);
    events_.push_back({EventKind::boss_defeated, Color::red, cell, bonus});
    boss_phase_ = BossPhase::none;
    boss_enraged_ = false;
    level_up();
}

void Game::check_reached_cannon() {
    std::size_t i = 0;
    while (i < blobs_.size()) {
        Blob& b = blobs_[i];
        // head < 1: die vordere Farbe läge auf Zelle 0, also auf der Kanone.
        if (b.head >= 1.0) {
            ++i;
            continue;
        }
        if (b.kind == BlobKind::gold) {
            // Entwischter Goldpunkt: verpasste Chance, kein Schaden.
            blobs_.erase(blobs_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        combo_ = 0;
        --lives_;
        add_effect(EffectType::tint, kLifeLostTint, 0, kTintS);
        add_zone_effect(ZoneEffectType::flash, kLifeLostTint, kTintS);
        if (b.kind == BlobKind::boss) {
            // Biss: Der Endgegner verliert zwei Glieder (nie das letzte) und weicht zurück.
            // Entfernte man ihn wie einen normalen Klumpen, wäre ein Biss ein Sieg.
            const int bite = std::min(kBiteSegments, static_cast<int>(b.colors.size()) - 1);
            for (int k = 0; k < bite; ++k) {
                pop_front(b);
            }
            const double far = static_cast<double>(cells_) - static_cast<double>(b.colors.size());
            b.head = std::clamp(b.head + kBossKnockbackCells, 1.5, std::max(1.5, far));
            events_.push_back({EventKind::boss_bite, Color::red, 0, lives_});
            ++i;
        } else {
            blobs_.erase(blobs_.begin() + static_cast<std::ptrdiff_t>(i));
            events_.push_back({EventKind::life_lost, Color::red, 0, lives_});
        }
        if (lives_ <= 0) {
            game_over();
            return;
        }
    }
}

void Game::game_over() {
    over_ = true;
    shots_.clear();
    events_.push_back({EventKind::game_over, Color::red, 0, score_});
}

void Game::spawn_tick(double dt_s) {
    if (!auto_spawn_) {
        return;
    }
    if (boss_phase_ == BossPhase::warning) {
        boss_timer_ -= dt_s;
        if (boss_timer_ <= 0.0 && space_for(boss_plan_.segments)) {
            spawn_boss(static_cast<double>(cells_ - boss_plan_.segments));
        }
        return;
    }
    const LevelParams p = level_params(level_);
    if (boss_phase_ == BossPhase::fight || p.boss) {
        return;
    }
    spawn_timer_ -= dt_s;
    if (spawn_timer_ > 0.0) {
        return;
    }
    // Den nächsten Klumpen nur einmal auswürfeln und dann auf Platz warten: würfelte jeder
    // Schritt neu, hinge die Zufallsfolge an der Bildrate und das Seed wäre wertlos.
    // Stufe 1 würfelt kein Gold – so bleibt die Folge dort dieselbe wie vor dem Gold.
    if (pending_.empty()) {
        pending_gold_ = level_ >= 2 && rng_() % kGoldChance == 0;
        if (pending_gold_) {
            pending_ = {Color::yellow};
        } else {
            const auto size = 1 + static_cast<int>(rng_() % static_cast<std::uint32_t>(p.max_blob_size));
            for (int k = 0; k < size; ++k) {
                pending_.push_back(static_cast<Color>(rng_() % static_cast<std::uint32_t>(p.colors)));
            }
        }
    }
    const int size = static_cast<int>(pending_.size());
    if (!space_for(size)) {
        return;
    }
    Blob b;
    b.colors = std::exchange(pending_, {});
    b.head = static_cast<double>(cells_ - size);
    if (pending_gold_) {
        b.kind = BlobKind::gold;
        b.speed = kGoldSpeed;
    }
    blobs_.push_back(std::move(b));
    spawn_timer_ = p.spawn_interval_s;
}

void Game::level_up() {
    ++level_;
    cleared_in_level_ = 0;
    add_effect(EffectType::sweep, kLevelUpColor, 0, kSweepS);
    add_zone_effect(ZoneEffectType::sweep, kLevelUpColor, kSweepS + 0.3);
    events_.push_back({EventKind::level_up, Color::green, 0, level_});
    if (is_boss_level(level_)) {
        enter_boss_warning();
    }
}

void Game::enter_boss_warning() {
    boss_phase_ = BossPhase::warning;
    boss_plan_ = boss_plan(level_, cells_);
    boss_timer_ = kBossWarningS;
    events_.push_back({EventKind::boss_warning, Color::red, cells_ - 1, static_cast<int>(boss_plan_.type)});
}

void Game::add_effect(EffectType type, Rgb color, int cell, double duration, double delay_s) {
    effects_.push_back(Effect{type, color, cell, -delay_s, duration});
}

void Game::add_zone_effect(ZoneEffectType type, Rgb color, double duration) {
    zone_effects_.push_back(ZoneEffect{type, color, 0.0, duration});
}

std::size_t Game::nearest_blob() const {
    std::size_t best = blobs_.size();
    for (std::size_t i = 0; i < blobs_.size(); ++i) {
        if (best == blobs_.size() || blobs_[i].head < blobs_[best].head) {
            best = i;
        }
    }
    return best;
}

std::size_t Game::boss_index() const {
    for (std::size_t i = 0; i < blobs_.size(); ++i) {
        if (blobs_[i].kind == BlobKind::boss) {
            return i;
        }
    }
    return blobs_.size();
}

bool Game::space_for(int size) const {
    // Neuer Klumpen liegt auf cells-size … cells-1; davor muss eine Zelle frei bleiben,
    // sonst verschmelzen zwei Klumpen optisch zu einem.
    const double new_head = static_cast<double>(cells_ - size);
    for (const Blob& b : blobs_) {
        if (b.head + static_cast<double>(b.colors.size()) + 1.0 > new_head) {
            return false;
        }
    }
    return true;
}

}  // namespace ldu
