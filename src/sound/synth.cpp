#include "sound/synth.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ldu::sound {
namespace {

// Spitzenwert bei volume = 1. Deutlich unter 32767: Rechtecktöne klingen sonst schon
// bei mittlerer Lautstärke kratzig, und mehrere Effekte am Stück sollen nicht erschrecken.
inline constexpr double kPeak = 12000.0;
inline constexpr double kAttackS = 0.003;
inline constexpr double kReleaseS = 0.015;

void put_u16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

void put_tag(std::vector<std::uint8_t>& out, const char (&tag)[5]) {
    out.insert(out.end(), tag, tag + 4);
}

}  // namespace

double total_duration(std::span<const Tone> tones) {
    double sum = 0.0;
    for (const Tone& t : tones) {
        sum += std::max(0.0, t.duration_s);
    }
    return sum;
}

std::vector<std::uint8_t> render_wav(std::span<const Tone> tones, int sample_rate) {
    std::vector<std::int16_t> samples;
    const double rate = static_cast<double>(sample_rate);
    for (const Tone& t : tones) {
        const auto n = static_cast<std::size_t>(std::lround(std::max(0.0, t.duration_s) * rate));
        const double attack = kAttackS * rate;
        // Kurze Töne: Ausklang höchstens ein Drittel, sonst bliebe kein Ton übrig.
        const double release = std::min(kReleaseS * rate, static_cast<double>(n) / 3.0);
        for (std::size_t i = 0; i < n; ++i) {
            if (t.freq_hz <= 0.0) {
                samples.push_back(0);
                continue;
            }
            const double x = static_cast<double>(i);
            const double phase = 2.0 * std::numbers::pi * t.freq_hz * x / rate;
            const double raw = t.wave == Wave::sine ? std::sin(phase) : (std::sin(phase) >= 0.0 ? 1.0 : -1.0);
            double env = 1.0;
            if (x < attack) {
                env = x / attack;
            }
            const double to_end = static_cast<double>(n) - x;
            if (release > 0.0 && to_end < release) {
                env = std::min(env, to_end / release);
            }
            const double v = raw * env * std::clamp(t.volume, 0.0, 1.0) * kPeak;
            samples.push_back(static_cast<std::int16_t>(std::lround(v)));
        }
    }

    const auto data_bytes = static_cast<std::uint32_t>(samples.size() * 2);
    std::vector<std::uint8_t> out;
    out.reserve(44 + data_bytes);
    put_tag(out, "RIFF");
    put_u32(out, 36 + data_bytes);
    put_tag(out, "WAVE");
    put_tag(out, "fmt ");
    put_u32(out, 16);
    put_u16(out, 1);  // PCM
    put_u16(out, 1);  // mono
    put_u32(out, static_cast<std::uint32_t>(sample_rate));
    put_u32(out, static_cast<std::uint32_t>(sample_rate) * 2);  // Bytes je Sekunde
    put_u16(out, 2);                                             // Bytes je Abtastwert
    put_u16(out, 16);                                            // Bit je Abtastwert
    put_tag(out, "data");
    put_u32(out, data_bytes);
    for (const std::int16_t s : samples) {
        put_u16(out, static_cast<std::uint16_t>(s));
    }
    return out;
}

}  // namespace ldu::sound
