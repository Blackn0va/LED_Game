// Spiellogik von LichtDuell – ohne Netz, ohne Konsole, ohne Uhr.
//
// Zelle 0 ist die Kanone des Spielers, Zelle cells-1 das Ende des Computers. Der
// Computer schickt Klumpen (ein oder mehrere Farbpunkte hintereinander) Richtung
// Kanone. Ein Schuss trifft immer die vorderste Farbe des vordersten Klumpens:
//   gleiche Farbe  → die Farbe verschwindet, der Klumpen wird kürzer;
//   falsche Farbe  → der Schuss steckt sich vorn an, der Klumpen wird länger und
//                    rückt eine Zelle näher – die angesteckte Farbe muss als Erstes weg.
// Erreicht ein Klumpen die Kanone, kostet das ein Leben.
//
// Dazu:
//   Kombo     Treffer in Folge ohne Fehlschuss; ab 5 doppelte Punkte, ab 10 dreifache …
//   Joker     Regenbogenschuss (passt auf jede Farbe); einer zum Start, einer je 8er-Kombo,
//             je Goldpunkt und je besiegtem Endgegner, höchstens drei.
//   Goldpunkt funkelt, ist schnell, jeder Schuss trifft; entwischt er, kostet das nichts.
//   Endgegner jede dritte Stufe, siehe game/boss.hpp.
//
// Die Zeit kommt von außen (update(dt)), Zufall nur aus dem Seed: Dasselbe Seed mit
// derselben dt-Folge ergibt dasselbe Spiel. Darauf bauen die Tests und die Demo.
//
// Threads: nicht threadsicher; ein Spiel gehört einem Thread.
// Fehler: Error(Kind::usage) bei ungültiger Konfiguration im Konstruktor, sonst keine.

#pragma once

#include "core/rgb.hpp"
#include "game/boss.hpp"

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace ldu {

inline constexpr int kMinCells = 8;
inline constexpr int kMaxCells = 300;
inline constexpr int kMaxLives = 5;
inline constexpr int kMaxJokers = 3;
inline constexpr int kComboPerJoker = 8;
inline constexpr double kShotCrossingS = 0.6;  // ein Schuss braucht so lange für die ganze Strecke
inline constexpr double kShotCooldownS = 0.1;  // Sperrzeit der Kanone nach jedem Schuss
inline constexpr double kStartDelayS = 2.0;    // Ruhe vor dem ersten Klumpen
inline constexpr double kBossWarningS = 2.5;   // Warnblinken, bevor ein Endgegner erscheint
inline constexpr double kBossKnockbackCells = 6.0;
inline constexpr Rgb kCannonIdle{48, 48, 48};
inline constexpr Rgb kGold{255, 190, 40};

struct Shot final {
    Color color = Color::red;
    double pos = 0.0;  // Zelle als Gleitkommawert, wächst Richtung Computer
    bool joker = false;
};

enum class BlobKind : std::uint8_t { normal, gold, boss };

struct Blob final {
    std::vector<Color> colors;  // colors[0] steht vorn, dem Spieler am nächsten
    double head = 0.0;          // Position der vorderen Zelle, sinkt Richtung Kanone
    BlobKind kind = BlobKind::normal;
    double speed = 1.0;  // Faktor auf die Stufengeschwindigkeit
    // Chamäleon: die letzten `shifting` Farben wechseln gemeinsam im Takt. Angesteckte
    // Fehlschüsse kommen immer vorn dazu, Treffer nehmen vorn weg – die wechselnden
    // Glieder bilden deshalb stets das Ende der Kette.
    int shifting = 0;

    friend bool operator==(const Blob&, const Blob&) = default;
};

enum class EventKind {
    fired,
    hit,
    cleared,
    stuck,
    life_lost,
    level_up,
    game_over,
    gold,
    joker_gained,
    boss_warning,
    boss_spawned,
    boss_enraged,
    boss_shot,
    boss_bite,
    boss_defeated
};

struct GameEvent final {
    EventKind kind = EventKind::fired;
    Color color = Color::red;
    int cell = 0;
    // Punkte bei hit/cleared/gold/boss_defeated, neue Stufe bei level_up, verbleibende
    // Leben bei life_lost/boss_bite, Joker-Zahl bei joker_gained, BossType bei boss_*,
    // 1 = Joker bei fired.
    int value = 0;
};

struct GameStats final {
    int shots = 0;
    int hits = 0;
    int misses = 0;
    int best_combo = 0;
    int bosses = 0;
    int golds = 0;

    friend bool operator==(const GameStats&, const GameStats&) = default;
};

struct BossStatus final {
    BossType type = BossType::worm;
    int segments = 0;  // verbleibende Glieder (Chamäleon: nur die wechselnden)
    int max_segments = 0;
    bool enraged = false;
    bool warning = false;  // noch nicht erschienen, Warnblinken läuft
};

struct GameConfig final {
    int cells = 30;
    std::uint32_t seed = 1;
    int lives = 3;
    int jokers = 1;
    int start_level = 1;
    bool auto_spawn = true;  // Tests schalten das ab und setzen Klumpen selbst
    bool intro = true;       // Regenbogen-Lauf beim Start (stört Darstellungstests)
};

class Game final {
public:
    explicit Game(const GameConfig& config);

    // Feuert einen Farbschuss. false, wenn die Kanone noch sperrt, die Farbe in dieser
    // Stufe noch nicht im Spiel ist oder das Spiel vorbei ist.
    bool fire(Color color);
    // Feuert einen Joker (passt auf jede Farbe). false ohne Joker oder bei Sperre.
    bool fire_joker();

    // Schreitet um dt_s Sekunden fort (intern in Schritten von höchstens 1/240 s, damit
    // schnelle Schüsse keinen Klumpen überspringen; mehr als 0,25 s am Stück werden
    // gekappt, etwa nach einem Hänger der Konsole).
    void update(double dt_s);

    // Bild für den Streifen: eine Farbe je Zelle, Zelle 0 zuerst.
    std::vector<Rgb> render() const;

    // Effektbereich hinter dem Computerende (z. B. um die Zimmerecke): `length` Einheiten,
    // Index 0 direkt hinter dem Feld. Zeigt Leben (rot), Joker (gold) und eine Kombo-Leiste
    // zum nächsten Joker; im Endgegnerkampf dessen Lebensleiste; darüber Warnblinken,
    // Stufen-Lauf, Lebensverlust-Blitz, Sieg-Regenbogen, Gold-Funkeln.
    std::vector<Rgb> render_zone(int length) const;

    // Ereignisse seit dem letzten Aufruf (für Meldungen und Ton).
    std::vector<GameEvent> take_events();

    // Setzt einen Klumpen direkt aufs Feld (Tests, und intern beim Nachschub).
    void spawn_blob(std::vector<Color> colors, double head);
    void spawn_blob(Blob blob);
    // Setzt den Endgegner dieser Stufe sofort aufs Feld. false außerhalb einer
    // Endgegner-Stufe oder wenn er schon kämpft.
    bool spawn_boss(double head);

    int cells() const noexcept {
        return cells_;
    }
    int score() const noexcept {
        return score_;
    }
    int level() const noexcept {
        return level_;
    }
    int lives() const noexcept {
        return lives_;
    }
    int base_lives() const noexcept {
        return base_lives_;
    }
    int jokers() const noexcept {
        return jokers_;
    }
    int combo() const noexcept {
        return combo_;
    }
    int multiplier() const noexcept;
    bool over() const noexcept {
        return over_;
    }
    bool ready() const noexcept {
        return !over_ && cooldown_ <= 0.0;
    }
    const GameStats& stats() const noexcept {
        return stats_;
    }
    int colors_in_play() const;
    const std::vector<Blob>& blobs() const noexcept {
        return blobs_;
    }
    const std::vector<Shot>& shots() const noexcept {
        return shots_;
    }
    std::optional<BossStatus> boss() const;

    // Farbe, die das Glied `index` von `blob` gerade hat (Chamäleon eingerechnet).
    Color effective_color(const Blob& blob, std::size_t index) const;
    // Farbe des Chamäleons in ahead_s Sekunden (für den Autopiloten).
    Color chameleon_color_in(double ahead_s) const;
    double blob_speed(const Blob& blob) const;  // Zellen je Sekunde
    double shot_speed() const;                  // Zellen je Sekunde

private:
    enum class EffectType { flash, burst, tint, sweep, rainbow };
    enum class BossPhase { none, warning, fight };

    struct Effect final {
        EffectType type = EffectType::flash;
        Rgb color;
        int cell = 0;
        double age = 0.0;  // negativ = startet verzögert
        double duration = 0.0;
    };

    // Ereignis-Effekte im Effektbereich (getrennt von effects_, die am Feld hängen).
    enum class ZoneEffectType { flash, sweep, rainbow, sparkle };

    struct ZoneEffect final {
        ZoneEffectType type = ZoneEffectType::flash;
        Rgb color;
        double age = 0.0;
        double duration = 0.0;
    };

    void add_zone_effect(ZoneEffectType type, Rgb color, double duration);
    void step(double dt_s);
    void move(double dt_s);
    void update_boss(double dt_s);
    void resolve_collisions();
    void hit_blob(std::size_t index, const Shot& shot);
    void defeat_boss(std::size_t index, int cell);
    void check_reached_cannon();
    void spawn_tick(double dt_s);
    void level_up();
    void enter_boss_warning();
    void game_over();
    void after_fire(Color color, bool joker);
    void add_effect(EffectType type, Rgb color, int cell, double duration, double delay_s = 0.0);
    std::size_t nearest_blob() const;
    std::size_t boss_index() const;
    bool space_for(int size) const;
    double enemy_speed() const;

    int cells_;
    bool auto_spawn_;
    std::mt19937 rng_;
    int score_ = 0;
    int level_ = 1;
    int lives_;
    int base_lives_;
    int jokers_;
    int combo_ = 0;
    int cleared_in_level_ = 0;
    bool over_ = false;
    double elapsed_ = 0.0;
    double over_time_ = 0.0;
    double cooldown_ = 0.0;
    double spawn_timer_ = kStartDelayS;
    Color last_shot_color_ = Color::red;
    bool last_shot_joker_ = false;
    std::vector<Color> pending_;  // nächster Klumpen, wartet ggf. auf Platz am Ende
    bool pending_gold_ = false;
    GameStats stats_;

    BossPhase boss_phase_ = BossPhase::none;
    BossPlan boss_plan_;
    double boss_timer_ = 0.0;  // Warnung: Restzeit; Kampf: bis zum nächsten Farbwechsel/Schuss
    int chameleon_step_ = 0;
    bool boss_enraged_ = false;
    double gunner_hold_ = 0.0;

    std::vector<Blob> blobs_;
    std::vector<Shot> shots_;
    std::vector<Effect> effects_;
    std::vector<ZoneEffect> zone_effects_;
    std::vector<GameEvent> events_;
};

}  // namespace ldu
