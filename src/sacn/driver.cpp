#include "sacn/driver.hpp"

#include "core/error.hpp"
#include "lights/layout.hpp"

namespace ldu::sacn {

SacnProtocol::SacnProtocol(int universe, Cid cid) {
    if (universe < kMinUniverse || universe > kMaxUniverse) {
        throw Error(Error::Kind::usage,
                    "sACN-Universum " + std::to_string(universe) + " liegt nicht zwischen 1 und 63999.");
    }
    fields_.universe = universe;
    fields_.cid = cid;
}

std::string SacnProtocol::name() const {
    return "sACN";
}

int SacnProtocol::max_units() const {
    return kMaxLeds;
}

double SacnProtocol::keepalive_s() const {
    return kKeepaliveS;
}

std::vector<std::string> SacnProtocol::start(double /*now_s*/) {
    return {};
}

std::vector<std::string> SacnProtocol::frame(std::span<const Rgb> leds, double /*now_s*/) {
    return {packet(leds, 0)};
}

std::vector<lights::StopStep> SacnProtocol::stop(std::span<const Rgb> last) {
    if (last.empty()) {
        return {};  // nie gesendet: kein Strom, den ein Empfänger kennt
    }
    // Wer auf ein einzelnes Ende-Paket kürzt: geht es verloren, hält der Empfänger das Bild
    // bis zu seiner Frist – E1.31 verlangt drei.
    std::vector<lights::StopStep> steps;
    for (int i = 0; i < kTerminateRepeats; ++i) {
        steps.push_back(lights::StopStep{0.0, packet(last, kOptionTerminated)});
    }
    return steps;
}

std::string SacnProtocol::packet(std::span<const Rgb> leds, std::uint8_t options) {
    fields_.options = options;
    std::string p = data_packet(fields_, lights::rgb_bytes(leds));
    fields_.sequence = static_cast<std::uint8_t>(fields_.sequence + 1);  // 0–255 umlaufend (§6.2.5)
    return p;
}

}  // namespace ldu::sacn
