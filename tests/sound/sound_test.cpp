// Ton ohne Lautsprecher: WAV-Aufbau, Effektlisten, Vorrang beim Abspielen.
//
// Ob die Klänge gut klingen und ob PlaySound sie abspielt, belegen diese Tests NICHT –
// das hört man nur am Gerät.

#include "sound/sfx.hpp"
#include "sound/synth.hpp"

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace ldu::sound {
namespace {

std::uint32_t u32_at(const std::vector<std::uint8_t>& b, std::size_t i) {
    return static_cast<std::uint32_t>(b[i]) | (static_cast<std::uint32_t>(b[i + 1]) << 8) |
           (static_cast<std::uint32_t>(b[i + 2]) << 16) | (static_cast<std::uint32_t>(b[i + 3]) << 24);
}

std::uint16_t u16_at(const std::vector<std::uint8_t>& b, std::size_t i) {
    return static_cast<std::uint16_t>(b[i] | (b[i + 1] << 8));
}

std::string tag_at(const std::vector<std::uint8_t>& b, std::size_t i) {
    return std::string(b.begin() + static_cast<std::ptrdiff_t>(i), b.begin() + static_cast<std::ptrdiff_t>(i + 4));
}

}  // namespace

TEST_CASE("WAV: Kopf ist RIFF/WAVE, PCM mono 16 Bit, Längen stimmen") {
    const std::vector<Tone> tones{{440, 0.1, 0.5, Wave::square}, {0, 0.05, 0, Wave::square}};
    const auto wav = render_wav(tones);
    const std::size_t samples = static_cast<std::size_t>(0.15 * kSampleRate + 0.5);
    REQUIRE(wav.size() == 44 + samples * 2);
    CHECK(tag_at(wav, 0) == "RIFF");
    CHECK(u32_at(wav, 4) == wav.size() - 8);
    CHECK(tag_at(wav, 8) == "WAVE");
    CHECK(tag_at(wav, 12) == "fmt ");
    CHECK(u32_at(wav, 16) == 16);
    CHECK(u16_at(wav, 20) == 1);
    CHECK(u16_at(wav, 22) == 1);
    CHECK(u32_at(wav, 24) == kSampleRate);
    CHECK(u32_at(wav, 28) == kSampleRate * 2);
    CHECK(u16_at(wav, 34) == 16);
    CHECK(tag_at(wav, 36) == "data");
    CHECK(u32_at(wav, 40) == samples * 2);
}

TEST_CASE("WAV: Pausen sind still, Töne bleiben unter dem Spitzenwert und klingen ein") {
    const std::vector<Tone> tones{{1000, 0.05, 1.0, Wave::square}, {0, 0.02, 1.0, Wave::square}};
    const auto wav = render_wav(tones);
    const std::size_t tone_samples = static_cast<std::size_t>(0.05 * kSampleRate + 0.5);
    int peak = 0;
    for (std::size_t i = 0; i < tone_samples; ++i) {
        const auto s = static_cast<std::int16_t>(u16_at(wav, 44 + 2 * i));
        peak = std::max(peak, std::abs(static_cast<int>(s)));
    }
    CHECK(peak > 8000);
    CHECK(peak <= 12000);
    CHECK(static_cast<std::int16_t>(u16_at(wav, 44)) == 0);  // Einschwingen beginnt bei 0
    for (std::size_t i = tone_samples; 44 + 2 * i < wav.size(); ++i) {
        CHECK(u16_at(wav, 44 + 2 * i) == 0);
    }
}

TEST_CASE("Effekte: jeder Effekt hat Töne, die Fanfare ist länger als ein Schuss") {
    for (int i = 0; i < kSfxCount; ++i) {
        CHECK_FALSE(sfx_tones(static_cast<Sfx>(i)).empty());
    }
    CHECK(total_duration(sfx_tones(Sfx::boss_defeated)) > total_duration(sfx_tones(Sfx::shot)));
    CHECK(sfx_priority(Sfx::boss_defeated) > sfx_priority(Sfx::shot));
}

TEST_CASE("Vorrang: ein Schuss schneidet die Fanfare nicht ab, danach und darüber geht alles") {
    SoundScheduler s;
    CHECK(s.admit(Sfx::boss_defeated, 0.0, 0.8));
    CHECK_FALSE(s.admit(Sfx::shot, 0.3, 0.03));
    CHECK(s.admit(Sfx::game_over, 0.4, 1.1));  // gleich wichtig: darf übernehmen
    CHECK(s.admit(Sfx::shot, 1.6, 0.03));      // alles ist ausgeklungen
    CHECK(s.admit(Sfx::hit, 1.61, 0.09));      // wichtiger als der laufende Schuss
}

}  // namespace ldu::sound
