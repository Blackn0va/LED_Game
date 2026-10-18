// sACN (E1.31) ohne Gerät: Datenpaket Byte für Byte, Längenfelder, Ende-Pakete.
//
// Die Offsets und Konstanten stehen als Zahlen aus der Feldtabelle in sacn/protocol.hpp
// (aus der Erinnerung an E1.31-2016, nicht aus dem Normtext). Der volle 512-Kanal-Wert
// 0x726E der Root-Schicht ist der aus Mitschnitten bekannte – ebenfalls erinnert.

#include "core/error.hpp"
#include "lights/output.hpp"
#include "sacn/driver.hpp"
#include "sacn/protocol.hpp"
#include "support/fake_sink.hpp"

#include <doctest.h>

#include <ostream>
#include <string>
#include <vector>

namespace ldu::sacn {
namespace {

int at(const std::string& p, std::size_t i) {
    return static_cast<unsigned char>(p[i]);
}

int be16(const std::string& p, std::size_t i) {
    return at(p, i) << 8 | at(p, i + 1);
}

Cid counting_cid() {
    Cid c{};
    for (std::size_t i = 0; i < c.size(); ++i) {
        c[i] = static_cast<std::uint8_t>(0xA0 + i);
    }
    return c;
}

}  // namespace

TEST_CASE("sACN: Datenpaket – Root, Framing und DMP an den Offsets der Norm") {
    PacketFields f;
    f.cid = counting_cid();
    f.universe = 0x0102;
    f.sequence = 9;
    const std::vector<std::uint8_t> ch{11, 22, 33};
    const std::string p = data_packet(f, ch);
    REQUIRE(p.size() == 126 + 3);
    CHECK(be16(p, 0) == 0x0010);
    CHECK(be16(p, 2) == 0x0000);
    CHECK(p.substr(4, 12) == std::string("ASC-E1.17\0\0\0", 12));
    CHECK(be16(p, 16) == (0x7000 | (129 - 16)));
    CHECK(p.substr(18, 4) == std::string("\0\0\0\x04", 4));
    CHECK(at(p, 22) == 0xA0);
    CHECK(at(p, 37) == 0xAF);
    CHECK(be16(p, 38) == (0x7000 | (129 - 38)));
    CHECK(p.substr(40, 4) == std::string("\0\0\0\x02", 4));
    CHECK(p.substr(44, 11) == std::string("LichtDuell\0", 11));
    CHECK(at(p, 108) == 100);  // Priorität
    CHECK(be16(p, 109) == 0);  // Sync-Adresse
    CHECK(at(p, 111) == 9);    // Sequenz
    CHECK(at(p, 112) == 0);    // Optionen
    CHECK(be16(p, 113) == 0x0102);
    CHECK(be16(p, 115) == (0x7000 | (129 - 115)));
    CHECK(at(p, 117) == 0x02);
    CHECK(at(p, 118) == 0xA1);
    CHECK(be16(p, 119) == 0x0000);
    CHECK(be16(p, 121) == 0x0001);
    CHECK(be16(p, 123) == 4);  // Startcode + 3 Kanäle
    CHECK(at(p, 125) == 0x00);
    CHECK(at(p, 126) == 11);
    CHECK(at(p, 128) == 33);
}

TEST_CASE("sACN: volles Universum – Root 0x726E, Framing 0x7258, DMP 0x720B, 513 Werte") {
    const std::string p = data_packet(PacketFields{}, std::vector<std::uint8_t>(512, 0));
    REQUIRE(p.size() == 638);
    CHECK(be16(p, 16) == 0x726E);
    CHECK(be16(p, 38) == 0x7258);
    CHECK(be16(p, 115) == 0x720B);
    CHECK(be16(p, 123) == 513);
}

TEST_CASE("sACN: Grenzen – 512 Kanäle, Universum 1–63999") {
    PacketFields f;
    CHECK_THROWS_AS(data_packet(f, std::vector<std::uint8_t>(513, 0)), Error);
    f.universe = 0;
    CHECK_THROWS_AS(data_packet(f, std::vector<std::uint8_t>(3, 0)), Error);
    f.universe = 64000;
    CHECK_THROWS_AS(data_packet(f, std::vector<std::uint8_t>(3, 0)), Error);
    CHECK_THROWS_AS(SacnProtocol(0, Cid{}), Error);
}

TEST_CASE("sACN: Multicast-Gruppe 239.255.<hi>.<lo>") {
    CHECK(multicast_group(1) == "239.255.0.1");
    CHECK(multicast_group(256) == "239.255.1.0");
    CHECK(multicast_group(63999) == "239.255.249.255");
}

TEST_CASE("sACN: Quellkennung ist eine UUID Version 4 und je Aufruf neu") {
    const Cid a = random_cid();
    const Cid b = random_cid();
    CHECK(a != b);
    CHECK((a[6] & 0xF0) == 0x40);
    CHECK((a[8] & 0xC0) == 0x80);
}

TEST_CASE("sACN-Treiber: Sequenz zählt je Paket, Ende sind drei Pakete mit „Strom endet“ (0x40)") {
    test::SinkLog log;
    lights::LightOutput out(lights::OutputSettings{}, std::make_unique<SacnProtocol>(7, counting_cid()),
                            test::fake_sink(log), test::recording_sleeper(log));
    out.start(0.0);
    CHECK(log.packets.empty());
    out.push(std::vector<Rgb>{Rgb{1, 2, 3}}, 0.0);
    out.push(std::vector<Rgb>{Rgb{4, 5, 6}}, 1.0);
    REQUIRE(log.packets.size() == 2);
    CHECK(at(log.packets[0], 111) == 0);
    CHECK(at(log.packets[1], 111) == 1);
    CHECK(be16(log.packets[0], 113) == 7);
    CHECK(at(log.packets[1], 126) == 4);
    log.clear();
    out.stop();
    REQUIRE(log.packets.size() == 3);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(at(log.packets[i], 112) == 0x40);
        CHECK(at(log.packets[i], 111) == static_cast<int>(2 + i));
    }
}

}  // namespace ldu::sacn
