#include "game/boss.hpp"

#include <algorithm>

namespace ldu {

bool is_boss_level(int level) {
    return level >= 3 && level % 3 == 0;
}

BossPlan boss_plan(int level, int cells) {
    // Runde 0 = Stufe 3, 1 = Stufe 6, … ; je drei Runden eine Stärkestufe mehr.
    const int round = std::max(0, level / 3 - 1);
    const int strength = round / 3;
    // Höchstens halber Streifen: Ein Endgegner, der den ganzen Streifen füllt, stünde
    // schon beim Erscheinen vor der Kanone.
    const int cap = std::max(4, cells / 2);
    BossPlan p;
    p.type = static_cast<BossType>(round % 3);
    switch (p.type) {
    case BossType::worm:
        p.segments = std::min(cap, 10 + 3 * strength);
        p.speed = 0.5 + 0.05 * strength;
        break;
    case BossType::chameleon:
        p.segments = std::min(cap, 5 + 2 * strength);
        p.speed = 0.4 + 0.05 * strength;
        p.interval_s = std::max(0.8, 1.3 - 0.15 * strength);
        break;
    case BossType::gunner:
        // 8 statt anfangs 6: Mit 6 Gliedern lag er in der Demo nach 8 Schüssen, bevor er
        // ein einziges Mal gefeuert hatte (beobachtet 2026-10-18, --demo --stufe 9).
        p.segments = std::min(cap, 8 + 2 * strength);
        p.speed = 0.6;
        p.interval_s = std::max(1.0, 1.8 - 0.2 * strength);
        break;
    }
    return p;
}

std::string_view boss_name(BossType type) {
    switch (type) {
    case BossType::worm:
        return "Farbwurm";
    case BossType::chameleon:
        return "Chamäleon";
    case BossType::gunner:
        return "Kanonier";
    }
    return "?";
}

std::string_view boss_hint(BossType type) {
    switch (type) {
    case BossType::worm:
        return "Lang und zäh – ab der Hälfte wird er wütend und schneller.";
    case BossType::chameleon:
        return "Wechselt im Takt die Farbe – kurz vor dem Wechsel flackert er.";
    case BossType::gunner:
        return "Bleibt stehen und antwortet auf jeden Treffer – erst die Geschosse, dann er.";
    }
    return "";
}

}  // namespace ldu
