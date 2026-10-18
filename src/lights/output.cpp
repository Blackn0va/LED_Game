#include "lights/output.hpp"

#include "core/error.hpp"

#include <windows.h>

#include <cmath>
#include <utility>

namespace ldu::lights {
namespace {

void real_sleep(double seconds) {
    if (seconds > 0.0) {
        Sleep(static_cast<DWORD>(std::lround(seconds * 1000.0)));
    }
}

}  // namespace

LightOutput::LightOutput(OutputSettings settings, std::unique_ptr<LightProtocol> protocol,
                         std::unique_ptr<net::PacketSink> sink, Sleeper sleeper)
    : settings_(std::move(settings)),
      protocol_(std::move(protocol)),
      sink_(std::move(sink)),
      sleeper_(sleeper ? std::move(sleeper) : Sleeper(real_sleep)),
      pacer_(settings_.max_fps, protocol_->keepalive_s()) {
    if (settings_.layout.segments > protocol_->max_units()) {
        throw Error(Error::Kind::usage, protocol_->name() + " nimmt höchstens " +
                                            std::to_string(protocol_->max_units()) + " Farben je Bild, nicht " +
                                            std::to_string(settings_.layout.segments) + " (--segmente).");
    }
}

LightOutput::~LightOutput() {
    if (!running_) {
        return;
    }
    try {
        stop();
    } catch (const std::exception& e) {
        // Destruktor darf nicht werfen. Folge: Das Gerät bleibt womöglich auf dem letzten
        // Bild stehen (Govee H612C, 2026-10-18) oder fällt erst nach seiner eigenen Frist
        // zurück (WLED, E1.31). Bewusst fail-open, kein Sicherheitspfad.
        OutputDebugStringA("LichtDuell: Gerät beim Beenden nicht zurückgestellt: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
    }
}

void LightOutput::start(double now_s) {
    for (const std::string& p : protocol_->start(now_s)) {
        send(p);
    }
    running_ = true;
}

void LightOutput::push(std::span<const Rgb> frame, double now_s) {
    push(frame, std::span<const Rgb>{}, now_s);
}

void LightOutput::push(std::span<const Rgb> field, std::span<const Rgb> zone, double now_s) {
    if (!running_) {
        return;
    }
    std::vector<Rgb> led = prepare_led_frame(field, zone, settings_.layout, settings_.reversed, settings_.brightness);
    if (!pacer_.should_send(now_s, led != last_sent_)) {
        return;
    }
    for (const std::string& p : protocol_->frame(led, now_s)) {
        send(p);
    }
    pacer_.mark_sent(now_s);
    last_sent_ = std::move(led);
    ++frames_;
}

void LightOutput::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    for (const StopStep& step : protocol_->stop(last_sent_)) {
        sleeper_(step.wait_s);
        send(step.packet);
    }
}

std::string LightOutput::status() const {
    std::string s = protocol_->name() + " ";
    if (!settings_.label.empty()) {
        s += settings_.label + " ";
    }
    s += settings_.target + " · " + std::to_string(static_cast<int>(std::lround(settings_.max_fps))) + " fps · " +
         std::to_string(frames_) + " Bilder";
    if (failures_ > 0) {
        s += " · " + std::to_string(failures_) + " Sendefehler, zuletzt: " + last_error_;
    }
    return s;
}

void LightOutput::send(const std::string& payload) {
    std::string error;
    if (!sink_->send(payload, error)) {
        ++failures_;
        last_error_ = std::move(error);
    }
}

}  // namespace ldu::lights
