#include "sound/player.hpp"

#include <windows.h>
#include <mmsystem.h>

namespace ldu::sound {

SoundPlayer::SoundPlayer() {
    for (int i = 0; i < kSfxCount; ++i) {
        const auto tones = sfx_tones(static_cast<Sfx>(i));
        wavs_[static_cast<std::size_t>(i)] = render_wav(tones);
        durations_[static_cast<std::size_t>(i)] = total_duration(tones);
    }
}

SoundPlayer::~SoundPlayer() {
    // Laufende Wiedergabe beenden, bevor wavs_ frei wird. Wer das weglässt, lässt winmm
    // nach dem Freigeben weiter im Puffer lesen.
    PlaySoundW(nullptr, nullptr, 0);
}

void SoundPlayer::play(Sfx sfx, double now_s) {
    const auto i = static_cast<std::size_t>(sfx);
    if (!scheduler_.admit(sfx, now_s, durations_[i])) {
        return;
    }
    // Rückgabe bewusst nicht ausgewertet: Ohne Audiogerät liefert PlaySound FALSE, das
    // Spiel läuft dann still weiter.
    PlaySoundW(reinterpret_cast<LPCWSTR>(wavs_[i].data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

}  // namespace ldu::sound
