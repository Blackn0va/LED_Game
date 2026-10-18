#include "core/rgb.hpp"

#include <algorithm>
#include <cmath>

namespace ldu {
namespace {

std::uint8_t mix_channel(std::uint8_t a, std::uint8_t b, double t) {
    const double v = static_cast<double>(a) + (static_cast<double>(b) - static_cast<double>(a)) * t;
    return static_cast<std::uint8_t>(std::clamp(std::lround(v), 0L, 255L));
}

}  // namespace

Rgb to_rgb(Color color) {
    // Gelb mit G = 180 statt 255 und Lila mit etwas Rot: Auf dem Bildschirm sind beide so
    // klar von Grün und Blau zu trennen. Auf dem Streifen ist das NICHT geprüft
    // (Gedaechtnis.md → Offen); wer hier dreht, prüft am Gerät nach.
    switch (color) {
    case Color::red:
        return {255, 0, 0};
    case Color::green:
        return {0, 255, 0};
    case Color::blue:
        return {0, 0, 255};
    case Color::yellow:
        return {255, 180, 0};
    case Color::purple:
        return {170, 0, 255};
    }
    return kWhite;
}

std::string_view color_name(Color color) {
    switch (color) {
    case Color::red:
        return "Rot";
    case Color::green:
        return "Grün";
    case Color::blue:
        return "Blau";
    case Color::yellow:
        return "Gelb";
    case Color::purple:
        return "Lila";
    }
    return "?";
}

Rgb blend(Rgb a, Rgb b, double t) {
    const double k = std::clamp(t, 0.0, 1.0);
    return {mix_channel(a.r, b.r, k), mix_channel(a.g, b.g, k), mix_channel(a.b, b.b, k)};
}

Rgb scale(Rgb color, double factor) {
    return blend(kBlack, color, factor);
}

Rgb hue(double turns) {
    const double x = turns - std::floor(turns);
    const double s = x * 6.0;
    const int sector = static_cast<int>(s) % 6;
    const double f = s - std::floor(s);
    auto c = [](double v) { return static_cast<std::uint8_t>(std::clamp(std::lround(v * 255.0), 0L, 255L)); };
    switch (sector) {
    case 0:
        return {255, c(f), 0};
    case 1:
        return {c(1.0 - f), 255, 0};
    case 2:
        return {0, 255, c(f)};
    case 3:
        return {0, c(1.0 - f), 255};
    case 4:
        return {c(f), 0, 255};
    default:
        return {255, 0, c(1.0 - f)};
    }
}

}  // namespace ldu
