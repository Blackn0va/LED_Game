// Kommandozeile: Vorgaben, Werte, Grenzen, Widersprüche.

#include "cli/options.hpp"
#include "core/error.hpp"

#include <doctest.h>

#include <string>
#include <vector>

namespace ldu::cli {
namespace {

Options parse(std::vector<std::string> args) {
    return parse_options(args);
}

}  // namespace

TEST_CASE("Optionen: ohne Angaben gelten die Vorgaben") {
    const Options o = parse({});
    CHECK_FALSE(o.help);
    CHECK_FALSE(o.scan);
    CHECK_FALSE(o.preview_only);
    CHECK_FALSE(o.reversed);
    CHECK(o.ip.empty());
    CHECK(o.leds == kDefaultLeds);
    CHECK(o.fps == kDefaultFps);
    CHECK(o.brightness == 100);
    CHECK_FALSE(o.seed.has_value());
}

TEST_CASE("Optionen: alle Werte werden gelesen") {
    const Options o = parse({"--ip", "192.168.1.50", "--leds", "15", "--fps", "20", "--helligkeit", "40",
                             "--umgedreht", "--seed", "99"});
    CHECK(o.ip == "192.168.1.50");
    CHECK(o.leds == 15);
    CHECK(o.fps == 20);
    CHECK(o.brightness == 40);
    CHECK(o.reversed);
    REQUIRE(o.seed.has_value());
    CHECK(*o.seed == 99u);
    CHECK(parse({"--nur-vorschau"}).preview_only);
    CHECK(parse({"--suchen"}).scan);
    CHECK(parse({"--hilfe"}).help);
}

TEST_CASE("Optionen: Zahlen außerhalb der Grenzen oder mit Resten sind Fehler") {
    CHECK_THROWS_AS(parse({"--leds", "85"}), Error);
    CHECK_THROWS_AS(parse({"--leds", "7"}), Error);
    CHECK_THROWS_AS(parse({"--leds", "30x"}), Error);
    CHECK_THROWS_AS(parse({"--fps", "0"}), Error);
    CHECK_THROWS_AS(parse({"--helligkeit", "101"}), Error);
    CHECK_THROWS_AS(parse({"--seed", "-1"}), Error);
    CHECK_THROWS_AS(parse({"--leds"}), Error);
    CHECK(parse({"--leds", "84"}).leds == 84);
}

TEST_CASE("Optionen: Widersprüche und Unbekanntes werden abgelehnt") {
    CHECK_THROWS_AS(parse({"--nur-vorschau", "--ip", "10.0.0.1"}), Error);
    CHECK_THROWS_AS(parse({"--suchen", "--ip", "10.0.0.1"}), Error);
    CHECK_THROWS_AS(parse({"--suchen", "--demo"}), Error);
    CHECK_THROWS_AS(parse({"--testbild", "--demo"}), Error);
    CHECK_THROWS_AS(parse({"--testbild", "--stufe", "3"}), Error);
    CHECK_THROWS_AS(parse({"--farbe"}), Error);
}

TEST_CASE("Optionen: Segmente und Versatz – mindestens 8 Einheiten müssen sichtbar bleiben") {
    const Options o = parse({"--segmente", "80", "--versatz", "5"});
    CHECK(o.segments == 80);
    CHECK(o.skip == 5);
    CHECK(o.units() == 80);
    CHECK(parse({}).units() == kDefaultLeds);
    CHECK(parse({"--versatz", "22"}).skip == 22);  // 30 - 22 = 8 sichtbar
    CHECK_THROWS_AS(parse({"--versatz", "23"}), Error);
    CHECK_THROWS_AS(parse({"--segmente", "85"}), Error);
    CHECK_THROWS_AS(parse({"--segmente", "10", "--versatz", "3"}), Error);
    CHECK_THROWS_AS(parse({"--versatz", "-1"}), Error);
}

TEST_CASE("Optionen: Effektbereich, Spielzellen aus den Feld-Abschnitten") {
    const Options o = parse({"--segmente", "40", "--versatz", "9", "--effektbereich", "7"});
    CHECK(o.zone == 7);
    CHECK(o.field_units() == 24);
    CHECK(o.game_cells() == 24);  // ohne --leds: jede Spielzelle ein Abschnitt
    CHECK(parse({"--segmente", "40", "--versatz", "9", "--effektbereich", "7", "--leds", "30"}).game_cells() == 30);
    CHECK(parse({}).game_cells() == kDefaultLeds);
    CHECK_THROWS_AS(parse({"--segmente", "40", "--versatz", "20", "--effektbereich", "13"}), Error);
}

TEST_CASE("Optionen: Werte aus der Einstellungsdatei, Kommandozeile überstimmt, Vorschau verdrängt die Datei-IP") {
    Options base;
    base.ip = "192.0.2.21";
    base.segments = 40;
    base.skip = 9;
    const Options plain = parse_options(std::vector<std::string>{}, base);
    CHECK(plain.ip == "192.0.2.21");
    CHECK(plain.skip == 9);
    const Options over = parse_options(std::vector<std::string>{"--versatz", "4"}, base);
    CHECK(over.skip == 4);
    const Options preview = parse_options(std::vector<std::string>{"--nur-vorschau"}, base);
    CHECK(preview.preview_only);
    CHECK(preview.ip.empty());
    CHECK(parse_options(std::vector<std::string>{"--suchen"}, base).scan);  // Datei-IP stört --suchen nicht
}

TEST_CASE("Optionen: --auto langsam/normal/schnell, nicht mit --demo, --testbild oder --suchen") {
    CHECK(parse({}).auto_mode == AutoMode::off);
    CHECK(parse({"--auto", "langsam"}).auto_mode == AutoMode::slow);
    CHECK(parse({"--auto", "normal"}).auto_mode == AutoMode::normal);
    CHECK(parse({"--auto", "schnell"}).auto_mode == AutoMode::fast);
    CHECK_THROWS_AS(parse({"--auto", "turbo"}), Error);
    CHECK_THROWS_AS(parse({"--auto"}), Error);
    CHECK_THROWS_AS(parse({"--auto", "schnell", "--demo"}), Error);
    CHECK_THROWS_AS(parse({"--auto", "schnell", "--testbild"}), Error);
    CHECK_THROWS_AS(parse({"--auto", "schnell", "--suchen"}), Error);
    CHECK(parse({"--auto", "schnell", "--stufe", "6"}).start_level == 6);
}

TEST_CASE("Optionen: Geräteart – Vorgabe Govee, Schlüssel und Schreibvarianten, Unsinn abgelehnt") {
    CHECK(parse({}).device == lights::DeviceType::govee);
    CHECK(parse({"--gerät", "wled"}).device == lights::DeviceType::wled);
    CHECK(parse({"--gerät", "artnet"}).device == lights::DeviceType::artnet);
    CHECK(parse({"--gerät", "e131"}).device == lights::DeviceType::sacn);
    CHECK_THROWS_WITH_AS(parse({"--gerät", "hue"}), doctest::Contains("govee, wled, artnet oder sacn"), Error);
    CHECK(parse({"--geraet", "wled"}).device == lights::DeviceType::wled);  // umlaut-ok: Tastatur ohne ä
}

TEST_CASE("Optionen: --segmente richtet sich nach der Geräteart, auch wenn --gerät danach steht") {
    CHECK(parse({"--segmente", "300", "--gerät", "wled"}).segments == 300);
    CHECK(parse({"--gerät", "wled", "--segmente", "490"}).segments == 490);
    CHECK_THROWS_AS(parse({"--gerät", "wled", "--segmente", "491"}), Error);
    CHECK(parse({"--gerät", "artnet", "--segmente", "170"}).segments == 170);
    CHECK_THROWS_WITH_AS(parse({"--gerät", "sacn", "--segmente", "171"}), doctest::Contains("höchstens 170"), Error);
    CHECK_THROWS_WITH_AS(parse({"--segmente", "85"}), doctest::Contains("Govee nimmt höchstens 84"), Error);
    // Spielzellen bleiben bei höchstens 84, auch auf 490 Abschnitten.
    CHECK(parse({"--gerät", "wled", "--segmente", "490"}).game_cells() == kMaxLeds);
}

TEST_CASE("Optionen: --universum je Geräteart geprüft, bei Govee und WLED ohne Wirkung") {
    CHECK(parse({"--gerät", "artnet", "--universum", "0"}).universe == 0);
    CHECK(parse({"--gerät", "artnet", "--universum", "32767"}).universe == 32767);
    CHECK_THROWS_AS(parse({"--gerät", "artnet", "--universum", "32768"}), Error);
    CHECK_THROWS_AS(parse({"--gerät", "sacn", "--universum", "0"}), Error);
    CHECK(parse({"--gerät", "sacn", "--universum", "63999"}).universe == 63999);
    CHECK_THROWS_AS(parse({"--universum", "64000"}), Error);
    CHECK(parse({"--universum", "0"}).universe == 0);  // Govee: steht drin, stört nicht
}

TEST_CASE("Optionen: --einrichten steht allein, --einstellungen nimmt einen Pfad") {
    CHECK(parse({"--einrichten"}).setup);
    CHECK(parse({"--einrichten", "--einstellungen", "flur.txt"}).settings_file == "flur.txt");
    CHECK_THROWS_AS(parse({"--einrichten", "--ip", "192.0.2.1"}), Error);
    CHECK_THROWS_AS(parse({"--einrichten", "--testbild"}), Error);
    CHECK_THROWS_AS(parse({"--suchen", "--einrichten"}), Error);
    CHECK_THROWS_AS(parse({"--einstellungen"}), Error);
    CHECK_THROWS_AS(parse({"--einstellungen", "a.txt", "--ohne-einstellungen"}), Error);
    const std::vector<std::string> args{"--stufe", "3", "--einstellungen", "C:/x/flur.txt"};
    CHECK(settings_file_argument(args) == "C:/x/flur.txt");
    CHECK_FALSE(settings_file_argument(std::vector<std::string>{"--stufe", "3"}));
    CHECK_THROWS_AS(settings_file_argument(std::vector<std::string>{"--einstellungen"}), Error);
}

TEST_CASE("Optionen: Demo, Startstufe, Ton und Testbild") {
    const Options o = parse({"--demo", "--stufe", "9", "--ohne-ton"});
    CHECK(o.demo);
    CHECK(o.start_level == 9);
    CHECK(o.mute);
    CHECK_FALSE(o.calibration);
    CHECK(parse({"--testbild", "--leds", "20", "--umgedreht"}).calibration);
    CHECK_THROWS_AS(parse({"--stufe", "0"}), Error);
    CHECK_THROWS_AS(parse({"--stufe", "31"}), Error);
    CHECK(parse({}).start_level == 1);
}

}  // namespace ldu::cli
