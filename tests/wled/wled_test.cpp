// WLED ohne Gerät: DRGB-Bytes, /json/info lesen, Ablauf hinter LightOutput.
//
// Erwartete Bytes von Hand nach der WLED-Doku „UDP Realtime“ (Byte 0 = 2, Byte 1 = Frist,
// dann RGB). Die /json/info-Probe ist ein gekürzter, von Hand geschriebener Aufbau nach der
// WLED-Doku „JSON API“ – keine Aufzeichnung eines echten Geräts.

#include "core/error.hpp"
#include "lights/output.hpp"
#include "support/fake_sink.hpp"
#include "wled/driver.hpp"
#include "wled/protocol.hpp"

#include <doctest.h>

#include <ostream>
#include <string>
#include <vector>

namespace ldu::wled {
namespace {

std::string bytes(std::initializer_list<int> list) {
    std::string s;
    for (const int b : list) {
        s.push_back(static_cast<char>(b));
    }
    return s;
}

constexpr std::string_view kInfo = R"({
  "ver": "0.15.0", "vid": 2412100,
  "leds": {"count": 150, "pwr": 0, "fps": 42, "maxpwr": 850, "maxseg": 32,
           "seglc": [1], "lc": 1, "rgbw": false, "wv": 0, "cct": 0},
  "str": false, "name": "Flur", "udpport": 21324, "live": false,
  "wifi": {"bssid": "", "rssi": -60, "signal": 80, "channel": 6},
  "fs": {"u": 12, "t": 983, "pmt": 0},
  "arch": "esp32", "brand": "WLED", "product": "FOSS"
})";

}  // namespace

TEST_CASE("WLED: DRGB ist 02, Frist, dann R G B je LED") {
    const std::vector<Rgb> leds{{255, 0, 0}, {0, 128, 1}};
    CHECK(drgb_packet(leds, 2) == bytes({0x02, 0x02, 255, 0, 0, 0, 128, 1}));
    CHECK(drgb_packet(std::vector<Rgb>(490, kBlack), 1).size() == 2 + 490 * 3);
    CHECK_THROWS_AS(drgb_packet(std::vector<Rgb>(491, kBlack), 1), Error);
    CHECK_THROWS_AS(drgb_packet(std::vector<Rgb>{}, 1), Error);
}

TEST_CASE("WLED: /json/info liefert Name, LED-Zahl, Version und UDP-Port") {
    const auto info = parse_info(kInfo);
    REQUIRE(info);
    CHECK(info->name == "Flur");
    CHECK(info->leds == 150);
    CHECK(info->version == "0.15.0");
    CHECK(info->brand == "WLED");
    CHECK(info->udp_port == 21324);
}

TEST_CASE("WLED: ohne leds.count oder mit Unsinn ist es keine WLED-Antwort") {
    CHECK_FALSE(parse_info(R"({"name":"x"})"));
    CHECK_FALSE(parse_info(R"({"count":5,"leds":{}})"));     // count auf der falschen Ebene
    CHECK_FALSE(parse_info(R"({"leds":{"count":0}})"));
    CHECK_FALSE(parse_info(R"({"leds":{"count":99999}})"));
    CHECK_FALSE(parse_info(R"({"leds":{"count":"30"}})"));
    CHECK_FALSE(parse_info("<html>WLED</html>"));
    const auto minimal = parse_info(R"({"leds":{"count":30}})");
    REQUIRE(minimal);
    CHECK(minimal->udp_port == 0);
    CHECK(minimal->name.empty());
}

TEST_CASE("WLED-Treiber: kein Startpaket, Bilder mit Frist 2 s, Ende mit Frist 1 s und letztem Bild") {
    test::SinkLog log;
    lights::LightOutput out(lights::OutputSettings{}, std::make_unique<WledProtocol>(), test::fake_sink(log),
                            test::recording_sleeper(log));
    out.start(0.0);
    CHECK(log.packets.empty());
    const std::vector<Rgb> frame{{9, 8, 7}, {1, 2, 3}};
    out.push(frame, 0.0);
    CHECK(log.packets == std::vector<std::string>{drgb_packet(frame, 2)});
    log.clear();
    out.stop();
    CHECK(log.packets == std::vector<std::string>{drgb_packet(frame, 1)});
    CHECK(log.waits == std::vector<double>{0.0});
}

TEST_CASE("WLED-Treiber: Haltebild alle 0,5 s – deutlich unter der Frist von 2 s") {
    test::SinkLog log;
    lights::LightOutput out(lights::OutputSettings{}, std::make_unique<WledProtocol>(), test::fake_sink(log),
                            test::recording_sleeper(log));
    out.start(0.0);
    const std::vector<Rgb> frame(4, kWhite);
    out.push(frame, 0.0);
    out.push(frame, 0.4);
    CHECK(log.packets.size() == 1);
    out.push(frame, 0.5);
    CHECK(log.packets.size() == 2);
    CHECK(kKeepaliveS * 2 < kRunTimeoutS);  // ein verlorenes Haltebild beendet den Modus nicht
}

TEST_CASE("WLED-Treiber: nie gesendet → beim Ende nichts senden") {
    test::SinkLog log;
    {
        lights::LightOutput out(lights::OutputSettings{}, std::make_unique<WledProtocol>(), test::fake_sink(log),
                                test::recording_sleeper(log));
        out.start(0.0);
    }
    CHECK(log.packets.empty());
}

}  // namespace ldu::wled
