// Alle vier Treiber durch echte Sockets über Loopback: open_output → LightOutput →
// UdpSender → 127.0.0.1. Belegt Zielport und Paketfolge auf dem Draht, ohne ein Gerät und
// ohne den Rechner zu verlassen. Multicast (sACN ohne IP) wird NICHT gesendet – das ginge
// ins LAN; dort prüft der Test nur das Ziel in der Statuszeile.

#include "artnet/protocol.hpp"
#include "devices/connect.hpp"
#include "govee/protocol.hpp"
#include "net/udp.hpp"
#include "sacn/protocol.hpp"
#include "sacn/router.hpp"
#include "support/udp_listener.hpp"
#include "wled/protocol.hpp"

#include <doctest.h>

#include <chrono>
#include <ostream>
#include <string>
#include <vector>

namespace ldu::devices {
namespace {

using lights::DeviceType;
using namespace std::chrono_literals;

Target loopback(DeviceType type) {
    Target t;
    t.type = type;
    t.ip = "127.0.0.1";
    t.universe = lights::default_universe(type);
    return t;
}

const std::vector<Rgb> kFrame{{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};

}  // namespace

TEST_CASE("Loopback WLED: DRGB mit Frist 2 an den gemeldeten Port, Ende mit Frist 1") {
    const net::NetworkSession session;
    test::UdpListener listener(0);
    Target t = loopback(DeviceType::wled);
    t.port = listener.port();
    auto out = open_output(t, lights::OutputSettings{});
    out->start(0.0);
    out->push(kFrame, 0.0);
    CHECK(listener.receive() == wled::drgb_packet(kFrame, 2));
    out->stop();
    CHECK(listener.receive() == wled::drgb_packet(kFrame, 1));
    CHECK(out->status().rfind("WLED 127.0.0.1", 0) == 0);
}

TEST_CASE("Loopback Govee: Port 4003, Einschalten, Razer, Bild, nach 2 s dreimal Aus") {
    const net::NetworkSession session;
    test::UdpListener listener(govee::kCommandPort);
    auto out = open_output(loopback(DeviceType::govee), lights::OutputSettings{});
    out->start(0.0);
    out->push(kFrame, 0.0);
    CHECK(listener.receive() == govee::turn_message(true));
    CHECK(listener.receive() == govee::razer_message(govee::razer_mode_packet(true)));
    CHECK(listener.receive() == govee::razer_message(govee::razer_frame_packet(kFrame, false)));
    const auto t0 = std::chrono::steady_clock::now();
    out->stop();
    const auto took = std::chrono::steady_clock::now() - t0;
    const std::string off = govee::razer_message(govee::razer_mode_packet(false));
    CHECK(listener.receive() == off);
    CHECK(listener.receive() == off);
    CHECK(listener.receive() == off);
    CHECK_FALSE(listener.receive(200ms));  // war an: kein Ausschalten
    CHECK(took >= 2s);                     // die gemessene Ruhe wird wirklich gewartet
}

TEST_CASE("Loopback Art-Net: Port 6454, ArtDmx im Universum, Ende schwarz") {
    const net::NetworkSession session;
    test::UdpListener listener(artnet::kPort);
    Target t = loopback(DeviceType::artnet);
    t.universe = 3;
    auto out = open_output(t, lights::OutputSettings{});
    out->start(0.0);
    out->push(kFrame, 0.0);
    const auto frame = listener.receive();
    REQUIRE(frame);
    CHECK(*frame == artnet::art_dmx(3, 1, std::vector<std::uint8_t>{255, 0, 0, 0, 255, 0, 0, 0, 255}));
    out->stop();
    CHECK(listener.receive() == artnet::art_dmx(3, 2, std::vector<std::uint8_t>(9, 0)));
    CHECK(out->status().find("127.0.0.1 · Universum 3") != std::string::npos);
}

TEST_CASE("Loopback sACN: Unicast an Port 5568, drei Ende-Pakete") {
    const net::NetworkSession session;
    test::UdpListener listener(sacn::kPort);
    auto out = open_output(loopback(DeviceType::sacn), lights::OutputSettings{});
    out->start(0.0);
    out->push(kFrame, 0.0);
    const auto frame = listener.receive();
    REQUIRE(frame);
    REQUIRE(frame->size() == 126 + 9);
    CHECK(static_cast<unsigned char>((*frame)[112]) == 0);
    out->stop();
    for (int i = 0; i < 3; ++i) {
        const auto end = listener.receive();
        REQUIRE(end);
        CHECK(static_cast<unsigned char>((*end)[112]) == sacn::kOptionTerminated);
    }
}

TEST_CASE("Loopback lange Streifen: WLED 600 LEDs als zwei DNRGB, Art-Net und sACN 171 LEDs als zwei Universen") {
    const net::NetworkSession session;
    lights::OutputSettings s;
    s.layout.segments = 600;
    {
        test::UdpListener listener(0);
        Target t = loopback(DeviceType::wled);
        t.port = listener.port();
        auto out = open_output(t, s);
        out->start(0.0);
        out->push(std::vector<Rgb>(30, kWhite), 0.0);
        const auto a = listener.receive();
        const auto b = listener.receive();
        REQUIRE(a);
        REQUIRE(b);
        CHECK(a->size() == 4 + 489 * 3);
        CHECK(static_cast<unsigned char>((*b)[0]) == wled::kDnrgb);
        CHECK(b->substr(2, 2) == std::string("\x01\xE9", 2));
        CHECK(b->size() == 4 + 111 * 3);
    }
    s.layout.segments = 171;
    {
        test::UdpListener listener(artnet::kPort);
        Target t = loopback(DeviceType::artnet);
        t.universe = 3;
        auto out = open_output(t, s);
        CHECK(out->status().find("127.0.0.1 · Universen 3–4") != std::string::npos);
        out->start(0.0);
        out->push(std::vector<Rgb>(30, kWhite), 0.0);
        const auto a = listener.receive();
        const auto b = listener.receive();
        REQUIRE(a);
        REQUIRE(b);
        CHECK(static_cast<unsigned char>((*a)[14]) == 3);
        CHECK(static_cast<unsigned char>((*b)[14]) == 4);
    }
    {
        test::UdpListener listener(sacn::kPort);
        auto out = open_output(loopback(DeviceType::sacn), s);
        out->start(0.0);
        out->push(std::vector<Rgb>(30, kWhite), 0.0);
        const auto a = listener.receive();
        const auto b = listener.receive();
        REQUIRE(a);
        REQUIRE(b);
        CHECK(sacn::packet_universe(*a) == 1);
        CHECK(sacn::packet_universe(*b) == 2);
    }
}

TEST_CASE("sACN ohne IP zielt auf die Multicast-Gruppe des Universums (ohne zu senden)") {
    const net::NetworkSession session;
    Target t;
    t.type = DeviceType::sacn;
    t.universe = 258;
    auto out = open_output(t, lights::OutputSettings{});
    CHECK(out->status().find("239.255.1.2 · Universum 258") != std::string::npos);
}

TEST_CASE("Ziel: Art-Net und sACN mit IP brauchen keine Suche, Universum aus der Anfrage") {
    const net::NetworkSession session;
    std::vector<std::string> said;
    const Say say = [&said](const std::string& s) { said.push_back(s); };
    const Target a = resolve(Request{DeviceType::artnet, "192.0.2.50", 12}, say);
    CHECK(a.ip == "192.0.2.50");
    CHECK(a.universe == 12);
    const Target s = resolve(Request{DeviceType::sacn, "", std::nullopt}, say);
    CHECK(s.ip.empty());
    CHECK(s.universe == 1);
    CHECK(said.empty());  // keine Suche angekündigt, also keine gelaufen
}

TEST_CASE("Liste und Hinweise: eine Zeile je Fund, Hinweis je Geräteart") {
    Found f;
    f.type = DeviceType::wled;
    f.ip = "192.0.2.60";
    f.name = "Flur";
    f.detail = "150 LEDs";
    CHECK(found_line(f) == "WLED     192.0.2.60       Flur · 150 LEDs");
    for (const DeviceType t : lights::kDeviceTypes) {
        CHECK_FALSE(no_device_hint(t).empty());
    }
    Found govee_without_name;
    govee_without_name.ip = "192.0.2.21";
    CHECK(found_line(govee_without_name) == "Govee    192.0.2.21       ?");
}

}  // namespace ldu::devices
