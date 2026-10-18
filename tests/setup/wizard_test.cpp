// Einrichtungs-Assistent ohne Netz und Konsole: Auswahl lesen, Gerät übernehmen.

#include "setup/wizard.hpp"

#include <doctest.h>

#include <ostream>
#include <string>
#include <vector>

namespace ldu::setup {
namespace {

using lights::DeviceType;

devices::Found wled_found(int leds) {
    devices::Found f;
    f.type = DeviceType::wled;
    f.ip = "192.0.2.60";
    f.name = "Flur";
    f.leds = leds;
    return f;
}

}  // namespace

TEST_CASE("Auswahl: Nummern, Buchstaben, Enter, Unsinn") {
    CHECK(parse_choice("2", 3).kind == Choice::Kind::device);
    CHECK(parse_choice("2", 3).index == 1);
    CHECK(parse_choice(" 3 \r\n", 3).index == 2);
    CHECK(parse_choice("", 3).kind == Choice::Kind::device);
    CHECK(parse_choice("", 3).index == 0);
    CHECK(parse_choice("", 0).kind == Choice::Kind::invalid);  // Enter ohne Fund
    CHECK(parse_choice("S", 0).kind == Choice::Kind::sacn_multicast);
    CHECK(parse_choice("i", 1).kind == Choice::Kind::manual);
    CHECK(parse_choice("q", 1).kind == Choice::Kind::quit);
    CHECK(parse_choice("0", 3).kind == Choice::Kind::invalid);
    CHECK(parse_choice("4", 3).kind == Choice::Kind::invalid);
    CHECK(parse_choice("1a", 3).kind == Choice::Kind::invalid);
    CHECK(parse_choice("-1", 3).kind == Choice::Kind::invalid);
}

TEST_CASE("Liste: nummerierte Funde, dann s, i und q") {
    const auto lines = menu_lines({wled_found(150)});
    REQUIRE(lines.size() == 4);
    CHECK(lines[0].rfind("   1  WLED", 0) == 0);
    CHECK(lines[1].rfind("   s  sACN", 0) == 0);
    CHECK(lines[2].rfind("   i  ", 0) == 0);
    CHECK(lines[3].rfind("   q  ", 0) == 0);
}

TEST_CASE("Übernehmen: anderes Gerät setzt Versatz und Co. zurück, WLED-LEDs werden Abschnitte") {
    cli::Options o;
    o.device = DeviceType::govee;
    o.ip = "192.0.2.21";
    o.segments = 40;
    o.skip = 11;
    o.zone = 8;
    o.reversed = true;
    o.brightness = 70;
    apply_found(o, wled_found(150));
    CHECK(o.device == DeviceType::wled);
    CHECK(o.ip == "192.0.2.60");
    CHECK(o.segments == 150);
    CHECK(o.skip == 0);
    CHECK(o.zone == 0);
    CHECK_FALSE(o.reversed);
    CHECK(o.brightness == 70);  // Geschmack bleibt
    CHECK_FALSE(o.universe);
}

TEST_CASE("Übernehmen: dasselbe Gerät behält Versatz und Effektbereich") {
    cli::Options o;
    o.device = DeviceType::wled;
    o.ip = "192.0.2.60";
    o.segments = 150;
    o.skip = 20;
    o.zone = 10;
    apply_found(o, wled_found(150));
    CHECK(o.skip == 20);
    CHECK(o.zone == 10);
    CHECK(o.segments == 150);
}

TEST_CASE("Übernehmen: WLED über 490 LEDs wird gekappt, unter 8 bleibt es bei den Vorgaben") {
    cli::Options big;
    apply_found(big, wled_found(600));
    CHECK(big.segments == 490);
    cli::Options tiny;
    apply_found(tiny, wled_found(5));
    CHECK(tiny.segments == 0);
}

TEST_CASE("Übernehmen: Art-Net nimmt das gemeldete Universum, sACN die Vorgabe") {
    cli::Options o;
    devices::Found art;
    art.type = DeviceType::artnet;
    art.ip = "192.0.2.70";
    art.universe = 4;
    apply_found(o, art);
    CHECK(o.universe == 4);
    devices::Found s;
    s.type = DeviceType::sacn;
    s.universe = 1;
    apply_found(o, s);
    CHECK(o.device == DeviceType::sacn);
    CHECK(o.ip.empty());
    CHECK(o.universe == 1);
}

}  // namespace ldu::setup
