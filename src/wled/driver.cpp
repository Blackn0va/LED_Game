#include "wled/driver.hpp"

#include "wled/protocol.hpp"

#include <algorithm>

namespace ldu::wled {

std::vector<std::string> frame_packets(std::span<const Rgb> leds, std::uint8_t timeout_s) {
    if (leds.size() <= static_cast<std::size_t>(kMaxDrgbLeds)) {
        return {drgb_packet(leds, timeout_s)};
    }
    // 489 statt 490 je Teilbild: zwei Byte Startindex mehr im Kopf, gleiche Paketgröße.
    std::vector<std::string> out;
    for (std::size_t first = 0; first < leds.size(); first += static_cast<std::size_t>(kMaxDnrgbLeds)) {
        const std::size_t count = std::min(leds.size() - first, static_cast<std::size_t>(kMaxDnrgbLeds));
        out.push_back(dnrgb_packet(static_cast<int>(first), leds.subspan(first, count), timeout_s));
    }
    return out;
}

std::string WledProtocol::name() const {
    return "WLED";
}

int WledProtocol::max_units() const {
    return kMaxLeds;
}

double WledProtocol::keepalive_s() const {
    return kKeepaliveS;
}

std::vector<std::string> WledProtocol::start(double /*now_s*/) {
    return {};
}

std::vector<std::string> WledProtocol::frame(std::span<const Rgb> leds, double /*now_s*/) {
    return frame_packets(leds, kRunTimeoutS);
}

std::vector<lights::StopStep> WledProtocol::stop(std::span<const Rgb> last) {
    if (last.empty()) {
        return {};  // nie ein Bild gesendet: WLED ist gar nicht im Echtzeitmodus
    }
    // Jedes Teilbild mit kurzer Frist: Bekäme nur das erste sie, hielten die übrigen LEDs
    // den Echtzeitmodus noch bis zur Lauf-Frist fest (nicht am Gerät geprüft, ob WLED die
    // Frist je Paket oder je Streifen führt – so stimmt es in beiden Fällen).
    std::vector<lights::StopStep> steps;
    for (std::string& p : frame_packets(last, kStopTimeoutS)) {
        steps.push_back(lights::StopStep{0.0, std::move(p)});
    }
    return steps;
}

}  // namespace ldu::wled
