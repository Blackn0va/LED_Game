#include "govee/strip.hpp"

#include "govee/protocol.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace ldu::govee {

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

StripOutput::StripOutput(StripSettings settings)
    : settings_(std::move(settings)),
      sink_(UdpSender::open(settings_.ip)),
      pacer_(settings_.max_fps, kKeepaliveS) {}

StripOutput::StripOutput(StripSettings settings, std::unique_ptr<PacketSink> sink)
    : settings_(std::move(settings)), sink_(std::move(sink)), pacer_(settings_.max_fps, kKeepaliveS) {}

StripOutput::~StripOutput() {
    if (!running_) {
        return;
    }
    try {
        stop();
    } catch (const std::exception& e) {
        // Destruktor darf nicht werfen. Folge: Der Streifen bleibt womöglich auf dem letzten
        // Bild stehen – das laut OpenRGB erwartete Zurückfallen nach einer Minute kam am
        // H612C nicht (2026-10-18). Bewusst fail-open, kein Sicherheitspfad.
        OutputDebugStringA("LichtDuell: Razer-Modus beim Beenden nicht ausgeschaltet: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
    }
}

void StripOutput::start(double now_s) {
    send(turn_message(true));
    send(razer_message(razer_mode_packet(true)));
    last_mode_s_ = now_s;
    running_ = true;
}

void StripOutput::push(std::span<const Rgb> frame, double now_s) {
    push(frame, std::span<const Rgb>{}, now_s);
}

void StripOutput::push(std::span<const Rgb> field, std::span<const Rgb> zone, double now_s) {
    if (!running_) {
        return;
    }
    std::vector<Rgb> led = prepare_led_frame(field, zone, settings_.layout, settings_.reversed, settings_.brightness);
    if (!pacer_.should_send(now_s, led != last_sent_)) {
        return;
    }
    if (now_s - last_mode_s_ >= kRearmS) {
        send(razer_message(razer_mode_packet(true)));
        last_mode_s_ = now_s;
    }
    send(razer_message(razer_frame_packet(led, false)));
    pacer_.mark_sent(now_s);
    last_sent_ = std::move(led);
    ++frames_;
}

void StripOutput::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    // Ruhe, dann mehrfach Aus (Messung siehe strip.hpp). Wer auf ein einzelnes Aus direkt
    // nach dem letzten Bild zurückdreht, lässt den H612C eingefroren stehen.
    auto pause = [](double s) {
        if (s > 0.0) {
            Sleep(static_cast<DWORD>(std::lround(s * 1000.0)));
        }
    };
    pause(settings_.stop_settle_s);
    const std::string off = razer_message(razer_mode_packet(false));
    for (int i = 0; i < std::max(1, settings_.stop_repeats); ++i) {
        if (i > 0) {
            pause(settings_.stop_gap_s);
        }
        send(off);
    }
    if (settings_.turn_off_at_end) {
        send(turn_message(false));
    }
}

std::string StripOutput::status() const {
    std::string s = "Govee ";
    if (!settings_.label.empty()) {
        s += settings_.label + " ";
    }
    s += settings_.ip + " · " + std::to_string(static_cast<int>(std::lround(settings_.max_fps))) + " fps · " +
         std::to_string(frames_) + " Bilder";
    if (failures_ > 0) {
        s += " · " + std::to_string(failures_) + " Sendefehler, zuletzt: " + last_error_;
    }
    return s;
}

void StripOutput::send(const std::string& payload) {
    std::string error;
    if (!sink_->send(payload, error)) {
        ++failures_;
        last_error_ = std::move(error);
    }
}

}  // namespace ldu::govee
