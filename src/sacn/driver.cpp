#include "sacn/driver.hpp"

#include "core/error.hpp"
#include "lights/device.hpp"
#include "lights/layout.hpp"

#include <algorithm>

namespace ldu::sacn {

SacnProtocol::SacnProtocol(int universe, Cid cid) : first_universe_(universe) {
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
    return packets(leds, 0);
}

std::vector<lights::StopStep> SacnProtocol::stop(std::span<const Rgb> last) {
    if (last.empty()) {
        return {};  // nie gesendet: kein Strom, den ein Empfänger kennt
    }
    // Wer auf ein einzelnes Ende-Paket kürzt: geht es verloren, hält der Empfänger das Bild
    // bis zu seiner Frist – E1.31 verlangt drei. Wer nur das erste Universum beendet, lässt
    // die übrigen bis zur Frist stehen.
    std::vector<lights::StopStep> steps;
    for (int i = 0; i < kTerminateRepeats; ++i) {
        for (std::string& p : packets(last, kOptionTerminated)) {
            steps.push_back(lights::StopStep{0.0, std::move(p)});
        }
    }
    return steps;
}

std::vector<std::string> SacnProtocol::packets(std::span<const Rgb> leds, std::uint8_t options) {
    const auto per_universe = static_cast<std::size_t>(lights::kLedsPerUniverse);
    std::vector<std::string> out;
    fields_.options = options;
    for (std::size_t first = 0, i = 0; first < leds.size(); first += per_universe, ++i) {
        if (sequences_.size() <= i) {
            sequences_.resize(i + 1, 0);
        }
        const std::span<const Rgb> part = leds.subspan(first, std::min(leds.size() - first, per_universe));
        fields_.universe = first_universe_ + static_cast<int>(i);
        fields_.sequence = sequences_[i];
        // data_packet wirft, wenn das Universum über 63999 hinausgeht.
        out.push_back(data_packet(fields_, lights::rgb_bytes(part)));
        sequences_[i] = static_cast<std::uint8_t>(sequences_[i] + 1);  // 0–255 umlaufend (§6.2.5)
    }
    return out;
}

}  // namespace ldu::sacn
