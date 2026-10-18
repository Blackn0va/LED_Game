// LightOutput mit einem Spieltreiber: was für JEDEN Hersteller gilt – Takt, Grenzen,
// Abschluss, Destruktor, Statuszeile. Dazu die festen Werte je Geräteart.

#include "core/error.hpp"
#include "lights/device.hpp"
#include "lights/output.hpp"
#include "support/fake_sink.hpp"

#include <doctest.h>

#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace ldu::lights {
namespace {

// Schreibt in Klartext, was der Ausgabe zugereicht wird.
class ScriptProtocol final : public LightProtocol {
public:
    std::string name() const override {
        return "Probe";
    }
    int max_units() const override {
        return 10;
    }
    double keepalive_s() const override {
        return 1.0;
    }
    std::vector<std::string> start(double /*now_s*/) override {
        return {"start"};
    }
    std::vector<std::string> frame(std::span<const Rgb> leds, double /*now_s*/) override {
        return {"bild " + std::to_string(leds.size()) + " r0=" + std::to_string(leds.front().r)};
    }
    std::vector<StopStep> stop(std::span<const Rgb> last) override {
        return {StopStep{0.5, "ende " + std::to_string(last.size())}, StopStep{0.25, "ende2"}};
    }
};

std::unique_ptr<LightOutput> open(test::SinkLog& log, OutputSettings s = {}) {
    s.target = "192.0.2.21";
    return std::make_unique<LightOutput>(std::move(s), std::make_unique<ScriptProtocol>(), test::fake_sink(log),
                                         test::recording_sleeper(log));
}

}  // namespace

TEST_CASE("Ausgabe: Start, Bild über das Layout, Ende mit Wartezeiten des Treibers") {
    test::SinkLog log;
    OutputSettings s;
    s.layout = StripLayout{6, 2, 0};
    auto out = open(log, s);
    out->push(std::vector<Rgb>(4, kWhite), 0.0);  // vor start(): nichts
    CHECK(log.packets.empty());
    out->start(0.0);
    out->push(std::vector<Rgb>(4, kWhite), 0.0);
    CHECK(log.packets == std::vector<std::string>{"start", "bild 6 r0=0"});  // Versatz 2 dunkel
    log.clear();
    out->stop();
    CHECK(log.packets == std::vector<std::string>{"ende 6", "ende2"});
    CHECK(log.waits == std::vector<double>{0.5, 0.25});
}

TEST_CASE("Ausgabe: Haltebild nach der Haltezeit des Treibers, nicht früher") {
    test::SinkLog log;
    auto out = open(log);
    out->start(0.0);
    const std::vector<Rgb> frame(3, kWhite);
    out->push(frame, 0.0);
    out->push(frame, 0.9);
    CHECK(log.packets.size() == 2);
    out->push(frame, 1.0);
    CHECK(log.packets.size() == 3);
}

TEST_CASE("Ausgabe: mehr Abschnitte als der Treiber kann → Bedienfehler mit Treibernamen") {
    test::SinkLog log;
    OutputSettings s;
    s.layout.segments = 11;
    try {
        open(log, s);
        FAIL("hätte werfen müssen");
    } catch (const Error& e) {
        CHECK(e.kind() == Error::Kind::usage);
        CHECK(std::string(e.what()).find("Probe nimmt höchstens 10") != std::string::npos);
    }
}

TEST_CASE("Ausgabe: Destruktor holt stop nach, ohne start passiert nichts") {
    test::SinkLog log;
    {
        auto out = open(log);
        out->start(0.0);
    }
    CHECK(log.count("ende2") == 1);
    test::SinkLog quiet;
    {
        auto out = open(quiet);
    }
    CHECK(quiet.packets.empty());
}

TEST_CASE("Ausgabe: Statuszeile nennt Treiber, Bezeichnung, Ziel und Fehler") {
    test::SinkLog log;
    OutputSettings s;
    s.label = "Flur";
    auto out = open(log, s);
    out->start(0.0);
    CHECK(out->status() == "Probe Flur 192.0.2.21 · 30 fps · 0 Bilder");
    log.fail = true;
    out->push(std::vector<Rgb>(2, kWhite), 0.0);
    CHECK(out->status().find("1 Sendefehler, zuletzt: Netz weg") != std::string::npos);
    log.fail = false;
}

TEST_CASE("Gerätearten: Schlüssel hin und zurück, Schreibvarianten, Grenzen") {
    for (const DeviceType t : kDeviceTypes) {
        CHECK(parse_device_type(device_key(t)) == t);
    }
    CHECK(parse_device_type("e131") == DeviceType::sacn);
    CHECK(parse_device_type("art-net") == DeviceType::artnet);
    CHECK_FALSE(parse_device_type("hue"));
    CHECK(max_units(DeviceType::govee) == 84);
    CHECK(max_units(DeviceType::wled) == 2040);
    CHECK(max_units(DeviceType::artnet) == 2040);
    CHECK(max_units(DeviceType::sacn) == 2040);
    CHECK(universes_needed(0) == 1);
    CHECK(universes_needed(170) == 1);
    CHECK(universes_needed(171) == 2);
    CHECK(universes_needed(340) == 2);
    CHECK(universes_needed(341) == 3);
    CHECK(universes_needed(2040) == 12);
    CHECK_FALSE(uses_universe(DeviceType::wled));
    CHECK(min_universe(DeviceType::sacn) == 1);
    CHECK(max_universe(DeviceType::sacn) == 63999);
    CHECK(max_universe(DeviceType::artnet) == 32767);
    CHECK(default_universe(DeviceType::artnet) == 0);
}

TEST_CASE("Kanäle: RGB je LED hintereinander") {
    const std::vector<Rgb> leds{{1, 2, 3}, {4, 5, 6}};
    CHECK(rgb_bytes(leds) == std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6});
}

}  // namespace ldu::lights
