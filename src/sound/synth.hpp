// Klänge ohne Dateien: kurze Rechteck- und Sinustöne, im Speicher als WAV-Abbild.
//
// Warum selbst erzeugt statt .wav-Beilagen: keine Dateien neben der EXE, keine
// Lizenzfrage, und PlaySound(SND_MEMORY) spielt ein WAV-Abbild direkt aus dem Speicher.
// Ein Ton mit freq_hz = 0 ist eine Pause.
//
// Threads: reine Funktionen.
// Fehler: keine; negative Dauern zählen als 0.

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ldu::sound {

enum class Wave : std::uint8_t { square, sine };

struct Tone final {
    double freq_hz = 0.0;
    double duration_s = 0.0;
    double volume = 0.0;  // 0..1
    Wave wave = Wave::square;
};

inline constexpr int kSampleRate = 22050;

// RIFF/WAVE, PCM, mono, 16 Bit.
std::vector<std::uint8_t> render_wav(std::span<const Tone> tones, int sample_rate = kSampleRate);
double total_duration(std::span<const Tone> tones);

}  // namespace ldu::sound
