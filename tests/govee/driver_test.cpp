// Govee-Treiber hinter LightOutput ohne Netz: Was geht in welcher Reihenfolge an den Streifen?
//
// Die Fake-Senke schreibt jede Nachricht und jede Wartezeit mit. Dass der H612C 2 s Ruhe
// und dreimal Razer-Aus braucht, um in seine Szene zurückzukehren, ist am Gerät gemessen
// (2026-10-18, siehe govee/driver.hpp); hier wird nur belegt, dass das Programm es auch so
// sendet. Die Fälle sind dieselben wie vor dem Umzug in lights::LightOutput – sie belegen,
// dass sich am Govee-Verhalten nichts geändert hat.

#include "core/error.hpp"
#include "govee/driver.hpp"
#include "govee/protocol.hpp"
#include "lights/output.hpp"
#include "support/fake_sink.hpp"

#include <doctest.h>

#include <memory>
#include <string>
#include <vector>

namespace ldu::govee {
namespace {

struct Rig final {
    test::SinkLog log;

    std::unique_ptr<lights::LightOutput> open(GoveeOptions g = {}, lights::OutputSettings s = {}) {
        s.target = "192.0.2.21";
        return std::make_unique<lights::LightOutput>(std::move(s), std::make_unique<GoveeProtocol>(g),
                                                     test::fake_sink(log), test::recording_sleeper(log));
    }
};

const std::string kOn = razer_message(razer_mode_packet(true));
const std::string kOff = razer_message(razer_mode_packet(false));
const std::vector<Rgb> kFrameA(8, Rgb{255, 0, 0});
const std::vector<Rgb> kFrameB(8, Rgb{0, 0, 255});

}  // namespace

TEST_CASE("Govee: Start schaltet das Gerät ein und den Razer-Modus an") {
    Rig rig;
    auto strip = rig.open();
    strip->start(0.0);
    CHECK(rig.log.packets == std::vector<std::string>{turn_message(true), kOn});
}

TEST_CASE("Govee: Beenden wartet 2 s, schickt dann dreimal Razer-Aus im Abstand 0,1 s und sonst nichts") {
    Rig rig;
    auto strip = rig.open();
    strip->start(0.0);
    strip->push(kFrameA, 0.0);
    rig.log.clear();
    strip->stop();
    CHECK(rig.log.packets == std::vector<std::string>{kOff, kOff, kOff});
    CHECK(rig.log.waits == std::vector<double>{2.0, 0.1, 0.1});
    strip->stop();  // zweites stop ist wirkungslos
    CHECK(rig.log.count(kOff) == 3);
}

TEST_CASE("Govee: Vorgabe sind 2 s Ruhe und dreimal Aus – am H612C gemessen, kürzer fror er ein") {
    const GoveeOptions defaults;
    CHECK(defaults.stop_settle_s >= 2.0);
    CHECK(defaults.stop_repeats >= 3);
}

TEST_CASE("Govee: war er vor dem Spiel aus, geht er nach dem Razer-Aus wieder aus") {
    Rig rig;
    GoveeOptions g;
    g.turn_off_at_end = true;
    auto strip = rig.open(g);
    strip->start(0.0);
    rig.log.clear();
    strip->stop();
    CHECK(rig.log.packets == std::vector<std::string>{kOff, kOff, kOff, turn_message(false)});
}

TEST_CASE("Govee: vergessenes stop holt der Destruktor nach") {
    Rig rig;
    {
        auto strip = rig.open();
        strip->start(0.0);
    }
    CHECK(rig.log.count(kOff) == 3);
}

TEST_CASE("Govee: Bilder gedrosselt, Gleiches nur als Haltebild, Razer-Modus alle 2 s neu an") {
    Rig rig;
    auto strip = rig.open();
    strip->start(0.0);
    const std::string a = razer_message(razer_frame_packet(kFrameA, false));
    const std::string b = razer_message(razer_frame_packet(kFrameB, false));
    rig.log.clear();
    strip->push(kFrameA, 0.0);
    strip->push(kFrameA, 0.01);  // unverändert
    strip->push(kFrameB, 0.02);  // zu früh nach dem letzten Bild (30 fps)
    CHECK(rig.log.packets == std::vector<std::string>{a});
    strip->push(kFrameB, 0.05);
    CHECK(rig.log.packets.back() == b);
    rig.log.clear();
    strip->push(kFrameB, 1.0);  // unverändert, Haltezeit nicht um
    CHECK(rig.log.packets.empty());
    strip->push(kFrameB, 2.1);  // Haltebild – und der Razer-Modus wird neu eingeschaltet
    CHECK(rig.log.packets == std::vector<std::string>{kOn, b});
}

TEST_CASE("Govee: umgedreht und Helligkeit stecken im gesendeten Paket") {
    Rig rig;
    lights::OutputSettings s;
    s.reversed = true;
    s.brightness = 0.5;
    auto strip = rig.open({}, s);
    strip->start(0.0);
    rig.log.clear();
    strip->push(std::vector<Rgb>{{255, 0, 0}, kBlack}, 0.0);
    const std::vector<Rgb> expected{kBlack, Rgb{128, 0, 0}};
    CHECK(rig.log.packets == std::vector<std::string>{razer_message(razer_frame_packet(expected, false))});
}

TEST_CASE("Govee: Sendefehler zählen in der Statuszeile und werfen nicht") {
    Rig rig;
    auto strip = rig.open();
    rig.log.fail = true;
    strip->start(0.0);
    strip->push(kFrameA, 0.0);
    const std::string status = strip->status();
    CHECK(status.find("Govee 192.0.2.21") != std::string::npos);
    CHECK(status.find("3 Sendefehler") != std::string::npos);
    CHECK(status.find("Netz weg") != std::string::npos);
    rig.log.fail = false;
}

TEST_CASE("Govee: mehr als 84 Abschnitte lehnt schon der Aufbau ab") {
    Rig rig;
    lights::OutputSettings s;
    s.layout.segments = 85;
    CHECK_THROWS_AS(rig.open({}, s), Error);
}

}  // namespace ldu::govee
