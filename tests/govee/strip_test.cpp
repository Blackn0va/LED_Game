// StripOutput ohne Netz: Was geht in welcher Reihenfolge an den Streifen?
//
// Die Fake-Senke schreibt jede Nachricht mit. Dass der H612C dreimal Razer-Aus braucht,
// um in seine Szene zurückzukehren, ist am Gerät gemessen (2026-10-18, siehe
// govee/strip.hpp); hier wird nur belegt, dass das Programm es auch so sendet.

#include "govee/protocol.hpp"
#include "govee/strip.hpp"

#include <doctest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ldu::govee {
namespace {

class FakeSink final : public PacketSink {
public:
    FakeSink(std::vector<std::string>& log, bool& fail) : log_(log), fail_(fail) {}

    bool send(std::string_view payload, std::string& error) override {
        if (fail_) {
            error = "Netz weg";
            return false;
        }
        log_.emplace_back(payload);
        return true;
    }

private:
    std::vector<std::string>& log_;
    bool& fail_;
};

StripSettings quick() {
    StripSettings s;
    s.ip = "10.0.0.2";
    s.stop_settle_s = 0.0;
    s.stop_gap_s = 0.0;
    return s;
}

struct Rig final {
    std::vector<std::string> log;
    bool fail = false;

    std::unique_ptr<StripOutput> open(StripSettings s = quick()) {
        return std::make_unique<StripOutput>(std::move(s), std::make_unique<FakeSink>(log, fail));
    }

    long count(const std::string& message) const {
        return static_cast<long>(std::count(log.begin(), log.end(), message));
    }
};

const std::string kOn = razer_message(razer_mode_packet(true));
const std::string kOff = razer_message(razer_mode_packet(false));
const std::vector<Rgb> kFrameA(8, Rgb{255, 0, 0});
const std::vector<Rgb> kFrameB(8, Rgb{0, 0, 255});

}  // namespace

TEST_CASE("Streifen: Start schaltet das Gerät ein und den Razer-Modus an") {
    Rig rig;
    auto strip = rig.open();
    strip->start(0.0);
    CHECK(rig.log == std::vector<std::string>{turn_message(true), kOn});
}

TEST_CASE("Streifen: Beenden schickt dreimal Razer-Aus und sonst nichts") {
    Rig rig;
    auto strip = rig.open();
    strip->start(0.0);
    strip->push(kFrameA, 0.0);
    rig.log.clear();
    strip->stop();
    CHECK(rig.log == std::vector<std::string>{kOff, kOff, kOff});
    strip->stop();  // zweites stop ist wirkungslos
    CHECK(rig.count(kOff) == 3);
}

TEST_CASE("Streifen: Vorgabe sind 2 s Ruhe und dreimal Aus – am H612C gemessen, kürzer fror er ein") {
    const StripSettings defaults;
    CHECK(defaults.stop_settle_s >= 2.0);
    CHECK(defaults.stop_repeats >= 3);
}

TEST_CASE("Streifen: war er vor dem Spiel aus, geht er nach dem Razer-Aus wieder aus") {
    Rig rig;
    StripSettings s = quick();
    s.turn_off_at_end = true;
    auto strip = rig.open(s);
    strip->start(0.0);
    rig.log.clear();
    strip->stop();
    CHECK(rig.log == std::vector<std::string>{kOff, kOff, kOff, turn_message(false)});
}

TEST_CASE("Streifen: vergessenes stop holt der Destruktor nach") {
    Rig rig;
    {
        auto strip = rig.open();
        strip->start(0.0);
    }
    CHECK(rig.count(kOff) == 3);
}

TEST_CASE("Streifen: Bilder gedrosselt, Gleiches nur als Haltebild, Razer-Modus alle 2 s neu an") {
    Rig rig;
    auto strip = rig.open();
    strip->start(0.0);
    const std::string a = razer_message(razer_frame_packet(kFrameA, false));
    const std::string b = razer_message(razer_frame_packet(kFrameB, false));
    rig.log.clear();
    strip->push(kFrameA, 0.0);
    strip->push(kFrameA, 0.01);  // unverändert
    strip->push(kFrameB, 0.02);  // zu früh nach dem letzten Bild (30 fps)
    CHECK(rig.log == std::vector<std::string>{a});
    strip->push(kFrameB, 0.05);
    CHECK(rig.log.back() == b);
    rig.log.clear();
    strip->push(kFrameB, 1.0);  // unverändert, Haltezeit nicht um
    CHECK(rig.log.empty());
    strip->push(kFrameB, 2.1);  // Haltebild – und der Razer-Modus wird neu eingeschaltet
    CHECK(rig.log == std::vector<std::string>{kOn, b});
}

TEST_CASE("Streifen: umgedreht und Helligkeit stecken im gesendeten Paket") {
    Rig rig;
    StripSettings s = quick();
    s.reversed = true;
    s.brightness = 0.5;
    auto strip = rig.open(s);
    strip->start(0.0);
    rig.log.clear();
    strip->push(std::vector<Rgb>{{255, 0, 0}, kBlack}, 0.0);
    const std::vector<Rgb> expected{kBlack, Rgb{128, 0, 0}};
    CHECK(rig.log == std::vector<std::string>{razer_message(razer_frame_packet(expected, false))});
}

TEST_CASE("Streifen: Sendefehler zählen in der Statuszeile und werfen nicht") {
    Rig rig;
    auto strip = rig.open();
    rig.fail = true;
    strip->start(0.0);
    strip->push(kFrameA, 0.0);
    const std::string status = strip->status();
    CHECK(status.find("3 Sendefehler") != std::string::npos);
    CHECK(status.find("Netz weg") != std::string::npos);
    rig.fail = false;
}

}  // namespace ldu::govee
