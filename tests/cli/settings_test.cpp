// einstellungen.txt: Lesen, Schreiben, Fehler mit Zeilennummer, Datei im Bauordner.

#include "cli/options.hpp"
#include "cli/settings.hpp"
#include "core/error.hpp"

#include <doctest.h>

#include <filesystem>
#include <string>

namespace ldu::cli {

TEST_CASE("Einstellungen: alle Schlüssel werden gelesen, Kommentare und Leerzeilen übersprungen") {
    Options o;
    apply_settings("# Kommentar\n"
                   "\n"
                   "ip = 192.0.2.21\r\n"
                   "segmente = 40\n"
                   "versatz=9\n"
                   "  effektbereich =  7  \n"
                   "leds = 24\n"
                   "umgedreht = ja\n"
                   "helligkeit = 80\n"
                   "fps = 25\n"
                   "ton = aus",
                   o);
    CHECK(o.ip == "192.0.2.21");
    CHECK(o.segments == 40);
    CHECK(o.skip == 9);
    CHECK(o.zone == 7);
    CHECK(o.leds == 24);
    CHECK(o.leds_given);
    CHECK(o.reversed);
    CHECK(o.brightness == 80);
    CHECK(o.fps == 25);
    CHECK(o.mute);
}

TEST_CASE("Einstellungen: Schreiben und wieder Lesen ergibt dieselben Werte") {
    Options a;
    a.ip = "192.0.2.21";
    a.segments = 40;
    a.skip = 9;
    a.zone = 7;
    a.brightness = 60;
    Options b;
    apply_settings(format_settings(a), b);
    CHECK(b.ip == a.ip);
    CHECK(b.segments == 40);
    CHECK(b.skip == 9);
    CHECK(b.zone == 7);
    CHECK(b.brightness == 60);
    CHECK_FALSE(b.leds_given);  // leds wird nur geschrieben, wenn es gesetzt war
    CHECK_FALSE(b.reversed);
    CHECK_FALSE(b.mute);
}

TEST_CASE("Einstellungen: Tippfehler und falsche Werte nennen die Zeile, statt still Vorgaben zu nehmen") {
    Options o;
    CHECK_THROWS_WITH_AS(apply_settings("ip = 1.2.3.4\nversatz = viel\n", o), doctest::Contains("Zeile 2"), Error);
    CHECK_THROWS_WITH_AS(apply_settings("helligkeit = 101\n", o), doctest::Contains("Zeile 1"), Error);
    CHECK_THROWS_WITH_AS(apply_settings("farbe = rot\n", o), doctest::Contains("unbekannter Schlüssel"), Error);
    CHECK_THROWS_WITH_AS(apply_settings("nur text\n", o), doctest::Contains("schlüssel = wert"), Error);
    CHECK_THROWS_AS(apply_settings("umgedreht = vielleicht\n", o), Error);
}

TEST_CASE("Einstellungen: Geräteart und Universum hin und zurück, alte Dateien ohne gerät sind Govee") {
    Options legacy;
    apply_settings("ip = 192.0.2.21\nsegmente = 40\n", legacy);
    CHECK(legacy.device == lights::DeviceType::govee);
    Options a;
    a.device = lights::DeviceType::artnet;
    a.ip = "192.0.2.70";
    a.universe = 4;
    a.segments = 170;
    const std::string text = format_settings(a);
    CHECK(text.find("gerät = artnet\n") != std::string::npos);
    CHECK(text.find("universum = 4\n") != std::string::npos);
    Options b;
    apply_settings(text, b);
    CHECK(b.device == lights::DeviceType::artnet);
    CHECK(b.universe == 4);
    CHECK(b.segments == 170);
    // Govee und WLED schreiben kein Universum, das nur verwirren würde.
    CHECK(format_settings(Options{}).find("universum") == std::string::npos);
    CHECK(format_settings(Options{}).find("gerät = govee\n") != std::string::npos);
    Options c;
    CHECK_THROWS_WITH_AS(apply_settings("gerät = hue\n", c), doctest::Contains("govee, wled"), Error);
    Options plain;
    apply_settings("geraet = sacn\n", plain);  // umlaut-ok: Datei von einer Tastatur ohne ä
    CHECK(plain.device == lights::DeviceType::sacn);
}

TEST_CASE("Einstellungen: Datei im Bauordner speichern und laden; fehlende Datei ändert nichts") {
    const std::filesystem::path dir = std::filesystem::path(LDU_TEST_BINARY_DIR) / "tmp" / "einstellungen";
    std::filesystem::remove_all(dir);
    const auto file = dir / "einstellungen.txt";
    Options none;
    CHECK_FALSE(load_settings(file, none));
    CHECK(none.ip.empty());
    Options a;
    a.ip = "10.0.0.7";
    a.segments = 40;
    a.skip = 3;
    save_settings(file, a);
    Options b;
    REQUIRE(load_settings(file, b));
    CHECK(b.ip == "10.0.0.7");
    CHECK(b.skip == 3);
    CHECK(settings_path().filename() == "einstellungen.txt");
    CHECK(settings_path().parent_path().filename() == "LichtDuell");
}

}  // namespace ldu::cli
