// Art-Net ohne Gerät: ArtDmx/ArtPoll Byte für Byte nach der Feldtabelle der Spezifikation,
// ArtPollReply lesen, Ablauf hinter LightOutput.
//
// Die Offsets in den Prüfungen stehen bewusst als Zahlen da (aus artnet/protocol.hpp
// übernommen, das sie aus der Art-Net-4-Tabelle hat) – ein Test, der die Konstanten des
// Codes benutzte, prüfte nur sich selbst.

#include "artnet/driver.hpp"
#include "artnet/protocol.hpp"
#include "core/error.hpp"
#include "lights/output.hpp"
#include "support/fake_sink.hpp"

#include <doctest.h>

#include <ostream>
#include <string>
#include <vector>

namespace ldu::artnet {
namespace {

int at(const std::string& p, std::size_t i) {
    return static_cast<unsigned char>(p[i]);
}

// ArtPollReply in voller Art-Net-4-Länge (239 Byte), Felder an den Offsets der Spezifikation.
std::string poll_reply(const std::string& ip4, int net, int sub, int swout, const std::string& short_name) {
    std::string p(239, '\0');
    p.replace(0, 8, std::string("Art-Net\0", 8));
    p[8] = 0x00;
    p[9] = 0x21;  // OpCode 0x2100 little-endian
    for (std::size_t i = 0; i < 4; ++i) {
        p[10 + i] = ip4[i];
    }
    p[14] = 0x36;
    p[15] = 0x19;  // Port 6454 little-endian
    p[18] = static_cast<char>(net);
    p[19] = static_cast<char>(sub);
    p.replace(26, short_name.size(), short_name);
    p.replace(44, 9, "Langer Na");
    p[190] = static_cast<char>(swout);
    return p;
}

}  // namespace

TEST_CASE("Art-Net: ArtDmx – Kennung, OpCode 0x5000 LE, ProtVer 14, Sequenz, Universum, Länge BE") {
    const std::vector<std::uint8_t> ch{10, 20, 30, 40, 50, 60};
    const std::string p = art_dmx(0x0123, 7, ch);
    REQUIRE(p.size() == 18 + 6);
    CHECK(p.substr(0, 8) == std::string("Art-Net\0", 8));
    CHECK(at(p, 8) == 0x00);
    CHECK(at(p, 9) == 0x50);
    CHECK(at(p, 10) == 0);
    CHECK(at(p, 11) == 14);
    CHECK(at(p, 12) == 7);     // Sequence
    CHECK(at(p, 13) == 0);     // Physical
    CHECK(at(p, 14) == 0x23);  // SubUni: unteres Byte der Port-Adresse
    CHECK(at(p, 15) == 0x01);  // Net: Bits 14–8
    CHECK(at(p, 16) == 0);
    CHECK(at(p, 17) == 6);     // Länge big-endian
    CHECK(at(p, 18) == 10);
    CHECK(at(p, 23) == 60);
}

TEST_CASE("Art-Net: ungerade Kanalzahl wird auf gerade aufgefüllt, leer auf zwei") {
    const std::vector<std::uint8_t> nine(9, 0xAA);
    const std::string p = art_dmx(0, 1, nine);
    CHECK(p.size() == 18 + 10);
    CHECK(at(p, 17) == 10);
    CHECK(at(p, 27) == 0);  // Füllbyte
    CHECK(art_dmx(0, 1, std::vector<std::uint8_t>{}).size() == 20);
    CHECK_THROWS_AS(art_dmx(0, 1, std::vector<std::uint8_t>(513, 0)), Error);
    CHECK_THROWS_AS(art_dmx(32768, 1, nine), Error);
    CHECK(at(art_dmx(32767, 1, nine), 15) == 0x7F);
}

TEST_CASE("Art-Net: ArtPoll ist 14 Byte, OpCode 0x2000 LE") {
    const std::string p = art_poll();
    REQUIRE(p.size() == 14);
    CHECK(at(p, 8) == 0x00);
    CHECK(at(p, 9) == 0x20);
    CHECK(at(p, 11) == 14);
}

TEST_CASE("Art-Net: ArtPollReply liefert IP, Namen und die Port-Adresse des ersten Ausgangs") {
    const auto node = parse_poll_reply(poll_reply(std::string("\xC0\x00\x02\x2A", 4), 1, 2, 3, "Pixelbox"));
    REQUIRE(node);
    CHECK(node->ip == "192.0.2.42");
    CHECK(node->short_name == "Pixelbox");
    CHECK(node->long_name == "Langer Na");
    CHECK(node->port_address == (1 << 8 | 2 << 4 | 3));
}

TEST_CASE("Art-Net: ArtPoll, kurze Pakete und Fremdes sind keine Antwort; Steuerzeichen im Namen entschärft") {
    CHECK_FALSE(parse_poll_reply(art_poll()));
    const std::string full = poll_reply(std::string(4, '\0'), 0, 0, 0, "x");
    CHECK_FALSE(parse_poll_reply(full.substr(0, 107)));
    const auto old = parse_poll_reply(full.substr(0, 150));  // alt: ohne SwOut
    REQUIRE(old);
    CHECK_FALSE(old->port_address);
    CHECK_FALSE(parse_poll_reply("Art-Nix" + full.substr(7)));
    const auto evil = parse_poll_reply(poll_reply(std::string(4, '\0'), 0, 0, 0, "a\x1b[2Jb"));
    REQUIRE(evil);
    CHECK(evil->short_name == "a?[2Jb");  // ESC hätte die Konsole umgestellt (CWE-150)
}

TEST_CASE("Art-Net-Treiber: Sequenz 1–255 umlaufend ohne 0, Ende als schwarzes Bild") {
    test::SinkLog log;
    lights::LightOutput out(lights::OutputSettings{}, std::make_unique<ArtNetProtocol>(5), test::fake_sink(log),
                            test::recording_sleeper(log));
    out.start(0.0);
    CHECK(log.packets.empty());
    for (int i = 0; i < 256; ++i) {
        out.push(std::vector<Rgb>{Rgb{static_cast<std::uint8_t>(i), 1, 2}}, i * 1.0);
    }
    REQUIRE(log.packets.size() == 256);
    CHECK(at(log.packets[0], 12) == 1);
    CHECK(at(log.packets[254], 12) == 255);
    CHECK(at(log.packets[255], 12) == 1);  // nach 255 wieder 1, nie 0
    CHECK(at(log.packets[0], 14) == 5);
    log.clear();
    out.stop();
    REQUIRE(log.packets.size() == 1);
    const std::string& black = log.packets.front();
    CHECK(at(black, 17) == 4);  // 3 Kanäle, auf 4 aufgefüllt
    CHECK(at(black, 18) == 0);
    CHECK(at(black, 19) == 0);
    CHECK(at(black, 20) == 0);
}

TEST_CASE("Art-Net-Treiber: Universum außerhalb 0–32767 ist ein Bedienfehler") {
    CHECK_THROWS_AS(ArtNetProtocol(-1), Error);
    CHECK_THROWS_AS(ArtNetProtocol(32768), Error);
    CHECK(ArtNetProtocol(0).max_units() == 2040);
}

TEST_CASE("Art-Net-Treiber: 170 LEDs ein Universum, 171 zwei – fortlaufend, je Universum eigene Sequenz") {
    test::SinkLog log;
    lights::LightOutput out(lights::OutputSettings{}, std::make_unique<ArtNetProtocol>(5), test::fake_sink(log),
                            test::recording_sleeper(log));
    out.start(0.0);
    out.push(std::vector<Rgb>(170, Rgb{1, 2, 3}), 0.0);
    REQUIRE(log.packets.size() == 1);
    CHECK(at(log.packets[0], 17) == 254);  // 510 Kanäle = 0x01FE
    CHECK(at(log.packets[0], 16) == 1);
    log.clear();
    std::vector<Rgb> leds(171, Rgb{1, 2, 3});
    leds[170] = Rgb{9, 8, 7};  // die erste LED des zweiten Universums
    out.push(leds, 1.0);
    REQUIRE(log.packets.size() == 2);
    CHECK(at(log.packets[0], 14) == 5);
    CHECK(at(log.packets[1], 14) == 6);
    CHECK(at(log.packets[0], 12) == 2);   // Universum 5: zweites Bild
    CHECK(at(log.packets[1], 12) == 1);   // Universum 6: erstes Bild – eigene Zählung
    CHECK(at(log.packets[1], 17) == 4);   // 3 Kanäle, auf 4 aufgefüllt
    CHECK(at(log.packets[1], 18) == 9);
    CHECK(at(log.packets[1], 20) == 7);
    log.clear();
    out.stop();
    REQUIRE(log.packets.size() == 2);     // beide Universen schwarz
    CHECK(at(log.packets[0], 14) == 5);
    CHECK(at(log.packets[1], 14) == 6);
    CHECK(at(log.packets[1], 18) == 0);
    CHECK(at(log.packets[0], 18 + 509) == 0);
}

TEST_CASE("Art-Net-Treiber: Port-Adresse zählt über die Sub-Net-Grenze, Überlauf über 32767 wirft") {
    test::SinkLog log;
    lights::LightOutput out(lights::OutputSettings{}, std::make_unique<ArtNetProtocol>(15), test::fake_sink(log),
                            test::recording_sleeper(log));
    out.start(0.0);
    out.push(std::vector<Rgb>(341, kWhite), 0.0);
    REQUIRE(log.packets.size() == 3);
    CHECK(at(log.packets[0], 14) == 0x0F);  // Sub-Net 0, Universum 15
    CHECK(at(log.packets[1], 14) == 0x10);  // Sub-Net 1, Universum 0
    CHECK(at(log.packets[2], 14) == 0x11);
    test::SinkLog edge;
    lights::LightOutput last(lights::OutputSettings{}, std::make_unique<ArtNetProtocol>(32767), test::fake_sink(edge),
                             test::recording_sleeper(edge));
    last.start(0.0);
    CHECK_NOTHROW(last.push(std::vector<Rgb>(170, kWhite), 0.0));
    CHECK_THROWS_AS(last.push(std::vector<Rgb>(171, kWhite), 1.0), Error);
}

}  // namespace ldu::artnet
