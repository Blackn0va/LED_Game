// Spielt Soundeffekte asynchron über PlaySound (winmm).
//
// Alle Klänge werden beim Start einmal erzeugt und leben so lange wie der Player:
// PlaySound(SND_MEMORY | SND_ASYNC) liest während des Abspielens weiter aus dem Puffer.
// Der Destruktor hält die Wiedergabe deshalb an, BEVOR die Puffer frei werden.
//
// Threads: ein Objekt, ein Thread.
// Fehler: keine. Ohne Audiogerät bleibt es still – kein Grund, das Spiel anzuhalten.

#pragma once

#include "sound/sfx.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace ldu::sound {

class SoundPlayer final {
public:
    SoundPlayer();
    ~SoundPlayer();
    SoundPlayer(const SoundPlayer&) = delete;
    SoundPlayer& operator=(const SoundPlayer&) = delete;

    void play(Sfx sfx, double now_s);

private:
    std::array<std::vector<std::uint8_t>, kSfxCount> wavs_;
    std::array<double, kSfxCount> durations_{};
    SoundScheduler scheduler_;
};

}  // namespace ldu::sound
