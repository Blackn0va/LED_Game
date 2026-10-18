#include "artnet/driver.hpp"

#include "artnet/protocol.hpp"
#include "core/error.hpp"
#include "lights/layout.hpp"

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
    return {art_dmx(port_address_, next_sequence(), lights::rgb_bytes(leds))};
}

std::vector<lights::StopStep> ArtNetProtocol::stop(std::span<const Rgb> last) {
    if (last.empty()) {
        return {};
    }
    const std::vector<std::uint8_t> black(last.size() * 3, 0);
    return {lights::StopStep{0.0, art_dmx(port_address_, next_sequence(), black)}};
}

std::uint8_t ArtNetProtocol::next_sequence() {
    // 1–255 umlaufend; 0 hieße „Reihenfolge nicht prüfen“ und bleibt ausgespart.
    sequence_ = sequence_ == 255 ? 1 : static_cast<std::uint8_t>(sequence_ + 1);
    return sequence_;
}

}  // namespace ldu::artnet
