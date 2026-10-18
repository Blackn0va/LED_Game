#include "artnet/driver.hpp"

#include "artnet/protocol.hpp"
#include "core/error.hpp"
#include "lights/device.hpp"
#include "lights/layout.hpp"

#include <algorithm>

namespace ldu::artnet {

ArtNetProtocol::ArtNetProtocol(int port_address) : port_address_(port_address) {
    if (port_address < 0 || port_address > kMaxPortAddress) {
        throw Error(Error::Kind::usage,
                    "Art-Net-Universum " + std::to_string(port_address) + " liegt nicht zwischen 0 und 32767.");
    }
}

std::string ArtNetProtocol::name() const {
    return "Art-Net";
}

int ArtNetProtocol::max_units() const {
    return kMaxLeds;
}

double ArtNetProtocol::keepalive_s() const {
    return kKeepaliveS;
}

std::vector<std::string> ArtNetProtocol::start(double /*now_s*/) {
    return {};
}

std::vector<std::string> ArtNetProtocol::frame(std::span<const Rgb> leds, double /*now_s*/) {
    return packets(leds, false);
}

std::vector<lights::StopStep> ArtNetProtocol::stop(std::span<const Rgb> last) {
    std::vector<lights::StopStep> steps;
    for (std::string& p : packets(last, true)) {
        steps.push_back(lights::StopStep{0.0, std::move(p)});
    }
    return steps;
}

std::vector<std::string> ArtNetProtocol::packets(std::span<const Rgb> leds, bool black) {
    const auto per_universe = static_cast<std::size_t>(lights::kLedsPerUniverse);
    std::vector<std::string> out;
    for (std::size_t first = 0, i = 0; first < leds.size(); first += per_universe, ++i) {
        const std::span<const Rgb> part = leds.subspan(first, std::min(leds.size() - first, per_universe));
        std::vector<std::uint8_t> channels = lights::rgb_bytes(part);
        if (black) {
            std::fill(channels.begin(), channels.end(), std::uint8_t{0});
        }
        // art_dmx wirft, wenn port_address_ + i über 32767 hinausgeht.
        out.push_back(art_dmx(port_address_ + static_cast<int>(i), next_sequence(i), channels));
    }
    return out;
}

std::uint8_t ArtNetProtocol::next_sequence(std::size_t universe_index) {
    if (sequences_.size() <= universe_index) {
        sequences_.resize(universe_index + 1, 0);
    }
    std::uint8_t& s = sequences_[universe_index];
    // 1–255 umlaufend; 0 hieße „Reihenfolge nicht prüfen“ und bleibt ausgespart.
    s = s == 255 ? 1 : static_cast<std::uint8_t>(s + 1);
    return s;
}

}  // namespace ldu::artnet
