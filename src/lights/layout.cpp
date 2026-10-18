#include "lights/layout.hpp"

#include <algorithm>

namespace ldu::lights {

std::vector<Rgb> prepare_led_frame(std::span<const Rgb> field, std::span<const Rgb> zone, const StripLayout& layout,
                                   bool reversed, double brightness) {
    const std::size_t cells = field.size();
    const std::size_t units = layout.segments > 0 ? static_cast<std::size_t>(layout.segments) : cells;
    std::vector<Rgb> out(units, kBlack);
    if (cells == 0 || units == 0) {
        return out;
    }
    // Mindestens eine Feld-Einheit; zu große Werte fängt schon die Optionsprüfung ab.
    const std::size_t skip = std::min(static_cast<std::size_t>(std::max(layout.skip, 0)), units - 1);
    const std::size_t zone_units = std::min(static_cast<std::size_t>(std::max(layout.zone, 0)), units - skip - 1);
    const std::size_t field_units = units - skip - zone_units;
    for (std::size_t j = 0; j < field_units; ++j) {
        // Ganzzahlig statt gerundet: jede Zelle bekommt einen zusammenhängenden Bereich,
        // bei 30 Zellen auf 75 Einheiten abwechselnd 2 und 3 Einheiten.
        out[skip + j] = scale(field[j * cells / field_units], brightness);
    }
    if (!zone.empty()) {
        for (std::size_t k = 0; k < zone_units; ++k) {
            out[skip + field_units + k] = scale(zone[k * zone.size() / zone_units], brightness);
        }
    }
    if (reversed) {
        std::reverse(out.begin(), out.end());
    }
    return out;
}

std::vector<Rgb> prepare_led_frame(std::span<const Rgb> frame, const StripLayout& layout, bool reversed,
                                   double brightness) {
    return prepare_led_frame(frame, std::span<const Rgb>{}, layout, reversed, brightness);
}

std::vector<std::uint8_t> rgb_bytes(std::span<const Rgb> leds) {
    std::vector<std::uint8_t> out;
    out.reserve(leds.size() * 3);
    for (const Rgb& c : leds) {
        out.push_back(c.r);
        out.push_back(c.g);
        out.push_back(c.b);
    }
    return out;
}

}  // namespace ldu::lights
