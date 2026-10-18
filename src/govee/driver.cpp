#include "govee/driver.hpp"

#include "govee/protocol.hpp"

#include <algorithm>
#include <utility>

namespace ldu::govee {

GoveeProtocol::GoveeProtocol(GoveeOptions options) : options_(options) {}

std::string GoveeProtocol::name() const {
    return "Govee";
}

int GoveeProtocol::max_units() const {
    return kMaxRazerColors;
}

double GoveeProtocol::keepalive_s() const {
    return kKeepaliveS;
}

std::vector<std::string> GoveeProtocol::start(double now_s) {
    last_mode_s_ = now_s;
    return {turn_message(true), razer_message(razer_mode_packet(true))};
}

std::vector<std::string> GoveeProtocol::frame(std::span<const Rgb> leds, double now_s) {
    std::vector<std::string> out;
    if (now_s - last_mode_s_ >= kRearmS) {
        out.push_back(razer_message(razer_mode_packet(true)));
        last_mode_s_ = now_s;
    }
    out.push_back(razer_message(razer_frame_packet(leds, false)));
    return out;
}

std::vector<lights::StopStep> GoveeProtocol::stop(std::span<const Rgb> /*last*/) {
    // Ruhe, dann mehrfach Aus (Messung siehe driver.hpp). Wer auf ein einzelnes Aus direkt
    // nach dem letzten Bild zurückdreht, lässt den H612C eingefroren stehen.
    const std::string off = razer_message(razer_mode_packet(false));
    std::vector<lights::StopStep> steps;
    for (int i = 0; i < std::max(1, options_.stop_repeats); ++i) {
        steps.push_back(lights::StopStep{i == 0 ? options_.stop_settle_s : options_.stop_gap_s, off});
    }
    if (options_.turn_off_at_end) {
        steps.push_back(lights::StopStep{0.0, turn_message(false)});
    }
    return steps;
}

}  // namespace ldu::govee
