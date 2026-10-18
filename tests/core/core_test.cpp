// Kern: Farbmischung und UTF-8/UTF-16.

#include "core/error.hpp"
#include "core/rgb.hpp"
#include "core/text.hpp"

#include <doctest.h>

// doctest gibt string_view bei einem Fehlschlag über operator<< aus; ohne <ostream>
// bricht schon das Übersetzen ab (gemessen mit MSVC 14.44, C2027 basic_ostream).
#include <ostream>
#include <string>

namespace ldu {

TEST_CASE("Farben: Mischen trifft die Enden und rundet in der Mitte") {
    const Rgb a{0, 0, 0};
    const Rgb b{255, 100, 10};
    CHECK(blend(a, b, 0.0) == a);
    CHECK(blend(a, b, 1.0) == b);
    CHECK(blend(a, b, 0.5) == Rgb{128, 50, 5});
    CHECK(blend(a, b, 2.0) == b);
    CHECK(scale(b, 0.0) == kBlack);
}

TEST_CASE("Farben: jede Spielfarbe hat einen deutschen Namen und einen eigenen Farbwert") {
    for (int i = 0; i < kColorCount; ++i) {
        for (int j = i + 1; j < kColorCount; ++j) {
            CHECK(to_rgb(static_cast<Color>(i)) != to_rgb(static_cast<Color>(j)));
        }
    }
    CHECK(color_name(Color::green) == "Grün");
}

TEST_CASE("Farben: Farbkreis trifft Rot, Grün, Blau und bricht über eine Umdrehung um") {
    CHECK(hue(0.0) == Rgb{255, 0, 0});
    CHECK(hue(1.0 / 3.0) == Rgb{0, 255, 0});
    CHECK(hue(2.0 / 3.0) == Rgb{0, 0, 255});
    CHECK(hue(1.25) == hue(0.25));
    CHECK(hue(-0.75) == hue(0.25));
}

TEST_CASE("Zahlen: deutscher Tausenderpunkt an der richtigen Stelle") {
    CHECK(format_number(0) == "0");
    CHECK(format_number(999) == "999");
    CHECK(format_number(1000) == "1.000");
    CHECK(format_number(12345) == "12.345");
    CHECK(format_number(1234567) == "1.234.567");
    CHECK(format_number(-5000) == "-5.000");
}

TEST_CASE("Text: UTF-8 und UTF-16 gehen verlustfrei hin und zurück, kaputtes UTF-8 wirft") {
    const std::string text = "Grün · ♥";
    CHECK(narrow(widen(text)) == text);
    const std::string broken = "a\xC3";
    CHECK_THROWS_AS(widen(broken), Error);
}

}  // namespace ldu
