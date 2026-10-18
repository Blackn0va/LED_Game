// Govee-Protokoll: Base64, Razer-Pakete, JSON-Nachrichten, Scan-Antworten.
//
// Die erwarteten Bytes stammen aus dem Paketaufbau in OpenRGB-MR 2172, die Base64-Werte
// sind von Hand nach RFC 4648 gerechnet – nicht mit dem Code unter Test. Ob ein echter
// Streifen sie annimmt, belegen diese Tests NICHT (Gedaechtnis.md → Offen).

#include "core/error.hpp"
#include "govee/protocol.hpp"

#include <doctest.h>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace ldu::govee {
namespace {

std::string encode(std::string_view text) {
    return base64_encode(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

}  // namespace

TEST_CASE("Base64: Prüfwerte aus RFC 4648") {
    CHECK(encode("") == "");
    CHECK(encode("f") == "Zg==");
    CHECK(encode("fo") == "Zm8=");
    CHECK(encode("foo") == "Zm9v");
    CHECK(encode("foob") == "Zm9vYg==");
    CHECK(encode("fooba") == "Zm9vYmE=");
    CHECK(encode("foobar") == "Zm9vYmFy");
}

TEST_CASE("Razer: Einschalten ist BB 00 01 B1 01 0A, als Base64 uwABsQEK") {
    const auto on = razer_mode_packet(true);
    CHECK(on == std::vector<std::uint8_t>{0xBB, 0x00, 0x01, 0xB1, 0x01, 0x0A});
    CHECK(base64_encode(on) == "uwABsQEK");
    const auto off = razer_mode_packet(false);
    CHECK(off == std::vector<std::uint8_t>{0xBB, 0x00, 0x01, 0xB1, 0x00, 0x0B});
    CHECK(base64_encode(off) == "uwABsQAL");
}

TEST_CASE("Razer: Bildpaket hat Kopf, Länge 2+3n, Farben in RGB-Reihenfolge und stimmige Prüfsumme") {
    const std::vector<Rgb> colors{{1, 2, 3}, {4, 5, 6}};
    const auto p = razer_frame_packet(colors, false);
    REQUIRE(p.size() == 13);
    CHECK(p[0] == 0xBB);
    CHECK(p[1] == 0x00);
    CHECK(p[2] == 8);
    CHECK(p[3] == 0xB0);
    CHECK(p[4] == 0);
    CHECK(p[5] == 2);
    CHECK(std::vector<std::uint8_t>(p.begin() + 6, p.begin() + 12) == std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6});
    // XOR über das ganze Paket samt Prüfsumme ergibt 0, wenn die Prüfsumme stimmt.
    std::uint8_t x = 0;
    for (const std::uint8_t b : p) {
        x ^= b;
    }
    CHECK(x == 0);
    CHECK(razer_frame_packet(colors, true)[4] == 1);
}

TEST_CASE("Razer: 84 Farben passen ins Längenbyte, 85 und keine werden abgelehnt") {
    const std::vector<Rgb> max_colors(84, Rgb{1, 1, 1});
    CHECK(razer_frame_packet(max_colors, false)[2] == 254);
    const std::vector<Rgb> too_many(85, Rgb{1, 1, 1});
    CHECK_THROWS_AS(razer_frame_packet(too_many, false), Error);
    const std::vector<Rgb> none;
    CHECK_THROWS_AS(razer_frame_packet(none, false), Error);
}

TEST_CASE("Nachrichten: JSON-Hülle für razer, turn und scan") {
    CHECK(razer_message(razer_mode_packet(true)) == R"({"msg":{"cmd":"razer","data":{"pt":"uwABsQEK"}}})");
    CHECK(turn_message(true) == R"({"msg":{"cmd":"turn","data":{"value":1}}})");
    CHECK(turn_message(false) == R"({"msg":{"cmd":"turn","data":{"value":0}}})");
    CHECK(scan_message() == R"({"msg":{"cmd":"scan","data":{"account_topic":"reserve"}}})");
}

// Beispielantwort: Feldnamen ip/device wie homebridge-govee (lib/connection/lan.js) sie
// liest; sku und Versionsfelder sind nicht gegen ein Gerät geprüft.
TEST_CASE("Suche: Antwort eines Geräts liefert IP, Modell und Geräte-ID") {
    const auto info = parse_scan_reply(
        R"({"msg":{"cmd":"scan","data":{"ip":"192.168.1.23","device":"1F:80:C5:32:32:36:72:4E",)"
        R"("sku":"H619A","bleVersionHard":"3.01.01","wifiVersionSoft":"1.02.03"}}})");
    REQUIRE(info);
    CHECK(info->ip == "192.168.1.23");
    CHECK(info->device == "1F:80:C5:32:32:36:72:4E");
    CHECK(info->sku == "H619A");
}

TEST_CASE("Suche: Leerraum im JSON stört nicht, fehlende Nebenfelder bleiben leer") {
    const auto info = parse_scan_reply(R"({ "msg" : { "cmd" : "scan", "data" : { "ip" : "10.0.0.5" } } })");
    REQUIRE(info);
    CHECK(info->ip == "10.0.0.5");
    CHECK(info->sku.empty());
}

// Aufbau nach Govees WLAN-Anleitung (onOff 1/0, brightness 1–100); nicht gegen den H612C
// geprüft – ob er devStatus beantwortet, zeigt erst LichtDuell --suchen.
TEST_CASE("Zustand: devStatus-Antwort liefert An/Aus und Helligkeit") {
    CHECK(status_message() == R"({"msg":{"cmd":"devStatus","data":{}}})");
    const auto on = parse_status_reply(
        R"({"msg":{"cmd":"devStatus","data":{"onOff":1,"brightness":80,"color":{"r":255,"g":0,"b":0},"colorTemInKelvin":7200}}})");
    REQUIRE(on);
    CHECK(on->on);
    CHECK(on->brightness == 80);
    const auto off = parse_status_reply(R"({"msg":{"cmd":"devStatus","data":{"onOff" : 0}}})");
    REQUIRE(off);
    CHECK_FALSE(off->on);
    CHECK(off->brightness == 0);
}

TEST_CASE("Zustand: Scan-Antworten, fehlendes onOff und Unsinn werden verworfen") {
    CHECK_FALSE(parse_status_reply(R"({"msg":{"cmd":"scan","data":{"onOff":1}}})"));
    CHECK_FALSE(parse_status_reply(R"({"msg":{"cmd":"devStatus","data":{"brightness":5}}})"));
    CHECK_FALSE(parse_status_reply(R"({"msg":{"cmd":"devStatus","data":{"onOff":"ja"}}})"));
    CHECK_FALSE(parse_status_reply(R"({"msg":{"cmd":"devStatus","data":{"onOff":12345678}}})"));
}

TEST_CASE("Suche: fremde oder kaputte Antworten werden verworfen") {
    CHECK_FALSE(parse_scan_reply(R"({"msg":{"cmd":"devStatus","data":{"onOff":1}}})"));
    CHECK_FALSE(parse_scan_reply(R"({"msg":{"cmd":"scan","data":{"device":"x"}}})"));
    CHECK_FALSE(parse_scan_reply(R"({"msg":{"cmd":"scan","data":{"ip":"1.2.3.4\"x"}}})"));
    CHECK_FALSE(parse_scan_reply(R"({"msg":{"cmd":"scan","data":{"ip":)"));
    CHECK_FALSE(parse_scan_reply("kein json"));
}

}  // namespace ldu::govee
