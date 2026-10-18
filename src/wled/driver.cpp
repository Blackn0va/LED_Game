#include "wled/driver.hpp"

#include "wled/protocol.hpp"

namespace ldu::wled {

std::string WledProtocol::name() const {
    return "WLED";
}

int WledProtocol::max_units() const {
    return kMaxDrgbLeds;
}

double WledProtocol::keepalive_s() const {
    return kKeepaliveS;
}

std::vector<std::string> WledProtocol::start(double /*now_s*/) {
    return {};
}

std::vector<std::string> WledProtocol::frame(std::span<const Rgb> leds, double /*now_s*/) {
    return {drgb_packet(leds, kRunTimeoutS)};
}

std::vector<lights::StopStep> WledProtocol::stop(std::span<const Rgb> last) {
    if (last.empty()) {
        return {};  // nie ein Bild gesendet: WLED ist gar nicht im Echtzeitmodus
    }
    return {lights::StopStep{0.0, drgb_packet(last, kStopTimeoutS)}};
}

}  // namespace ldu::wled
