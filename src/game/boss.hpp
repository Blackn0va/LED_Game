// Endgegner: jede dritte Stufe ist ein Kampf gegen einen von drei Gegnern.
//
//   Farbwurm  (Stufe 3, 12, …): lange Kette aus Farbläufen, langsam. Ab der Hälfte wird
//             er wütend und schneller – wer trödelt, verliert.
//   Chamäleon (Stufe 6, 15, …): alle Glieder wechseln gemeinsam im Takt die Farbe; kurz
//             vor dem Wechsel flackert der Körper. Wer trifft, muss den Takt lesen.
//   Kanonier  (Stufe 9, 18, …): läuft bis gut zur Mitte, bleibt stehen und schießt schnelle
//             Einzelpunkte – im Takt und als Gegenschuss auf jeden Treffer. Die Geschosse
//             müssen zuerst weg, dann er: Treffer und Abwehr wechseln sich ab.
//
// Je Runde durch alle drei (ab Stufe 12) werden sie länger und schneller. Ein Sieg bringt
// ein Leben, einen Joker und viele Punkte; ein Biss an der Kanone kostet ein Leben, der
// Endgegner verliert dabei zwei Glieder und weicht zurück.
//
// Threads: reine Funktionen.
// Fehler: keine.

#pragma once

#include <cstdint>
#include <string_view>

namespace ldu {

enum class BossType : std::uint8_t { worm, chameleon, gunner };

struct BossPlan final {
    BossType type = BossType::worm;
    int segments = 0;         // Länge beim Erscheinen
    double speed = 1.0;       // Faktor auf die Stufengeschwindigkeit
    double interval_s = 0.0;  // Chamäleon: Farbtakt; Kanonier: Schusstakt
};

bool is_boss_level(int level);
BossPlan boss_plan(int level, int cells);
std::string_view boss_name(BossType type);
std::string_view boss_hint(BossType type);

}  // namespace ldu
