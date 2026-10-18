// Soundeffekte des Spiels: welche Töne, wie wichtig, wer wen unterbrechen darf.
//
// PlaySound spielt immer nur einen Klang; ein neuer schneidet den alten ab. Ohne
// Vorrang würde das Schussgeräusch jede Fanfare abwürgen, weil ständig geschossen wird.
// Der SoundScheduler lässt deshalb einen Klang nur durch, wenn nichts Wichtigeres mehr
// läuft.
//
// Threads: reine Werte bzw. ein Scheduler je Thread.
// Fehler: keine.

#pragma once

#include "sound/synth.hpp"

#include <cstdint>
#include <vector>

namespace ldu::sound {

enum class Sfx : std::uint8_t {
    shot,
    joker_shot,
    hit,
    clear,
    stuck,
    life_lost,
    level_up,
    boss_warning,
    boss_shot,
    boss_bite,
    boss_defeated,
    joker,
    gold,
    game_over
};
inline constexpr int kSfxCount = 14;

std::vector<Tone> sfx_tones(Sfx sfx);
int sfx_priority(Sfx sfx);

class SoundScheduler final {
public:
    // true: jetzt abspielen (und als laufend merken). false: etwas Wichtigeres läuft noch.
    bool admit(Sfx sfx, double now_s, double duration_s);

private:
    double busy_until_s_ = 0.0;
    int priority_ = -1;
};

}  // namespace ldu::sound
