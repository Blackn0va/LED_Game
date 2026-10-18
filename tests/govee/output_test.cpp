// Ausgabe an den Streifen ohne Netz: Takt (FramePacer) und Streifenbild samt Layout.

#include "govee/frame_pacer.hpp"
#include "govee/strip.hpp"

#include <doctest.h>

#include <vector>

namespace ldu::govee {

TEST_CASE("Takt: erstes Bild geht sofort, danach höchstens max_fps") {
    FramePacer pacer(30.0, 2.0);
    CHECK(pacer.should_send(0.0, false));
    pacer.mark_sent(0.0);
    CHECK_FALSE(pacer.should_send(0.020, true));
    CHECK(pacer.should_send(0.034, true));
}

TEST_CASE("Takt: unverändertes Bild geht erst nach der Haltezeit wieder raus") {
    FramePacer pacer(30.0, 2.0);
    pacer.mark_sent(0.0);
    CHECK_FALSE(pacer.should_send(1.0, false));
    CHECK(pacer.should_send(2.0, false));
}

TEST_CASE("Streifenbild: umgedreht spiegelt, Helligkeit skaliert die Farben") {
    const std::vector<Rgb> frame{{255, 0, 0}, {0, 0, 0}, {0, 0, 200}};
    const auto out = prepare_led_frame(frame, {}, true, 0.5);
    REQUIRE(out.size() == 3);
    CHECK(out[0] == Rgb{0, 0, 100});
    CHECK(out[1] == kBlack);
    CHECK(out[2] == Rgb{128, 0, 0});
    CHECK(prepare_led_frame(frame, {}, false, 1.0) == frame);
}

TEST_CASE("Layout: 80 Einheiten, 5 dunkel am Kanonen-Ende, 30 Zellen gleichmäßig auf 75 verteilt") {
    std::vector<Rgb> cells(30);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        cells[i] = Rgb{static_cast<std::uint8_t>(i + 1), 0, 0};  // jede Zelle unterscheidbar
    }
    const auto out = prepare_led_frame(cells, StripLayout{80, 5}, false, 1.0);
    REQUIRE(out.size() == 80);
    for (int u = 0; u < 5; ++u) {
        CHECK(out[static_cast<std::size_t>(u)] == kBlack);
    }
    CHECK(out[5] == cells.front());   // Kanone direkt hinter dem Versatz
    CHECK(out[79] == cells.back());   // Computerende ganz am Ende
    // Jede Zelle kommt vor, in Reihenfolge, je 2 oder 3 Einheiten.
    std::vector<int> width(30, 0);
    for (std::size_t u = 5; u < 80; ++u) {
        ++width[static_cast<std::size_t>(out[u].r - 1)];
        if (u > 5) {
            CHECK(out[u].r >= out[u - 1].r);
        }
    }
    for (const int w : width) {
        CHECK(w >= 2);
        CHECK(w <= 3);
    }
}

TEST_CASE("Layout: mit --umgedreht bleibt der Versatz am Kanonen-Ende") {
    const std::vector<Rgb> cells(10, kWhite);
    const auto out = prepare_led_frame(cells, StripLayout{20, 4}, true, 1.0);
    REQUIRE(out.size() == 20);
    for (std::size_t u = 16; u < 20; ++u) {
        CHECK(out[u] == kBlack);
    }
    CHECK(out[15] == kWhite);
    CHECK(out[0] == kWhite);
}

TEST_CASE("Layout: dunkel | Kampffeld | Effektbereich – 40 Abschnitte, 9 aus, 24 Feld, 7 Effekte") {
    std::vector<Rgb> cells(24);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        cells[i] = Rgb{static_cast<std::uint8_t>(i + 1), 0, 0};
    }
    std::vector<Rgb> zone(7);
    for (std::size_t i = 0; i < zone.size(); ++i) {
        zone[i] = Rgb{0, static_cast<std::uint8_t>(i + 1), 0};
    }
    const auto out = prepare_led_frame(cells, zone, StripLayout{40, 9, 7}, false, 1.0);
    REQUIRE(out.size() == 40);
    for (std::size_t u = 0; u < 9; ++u) {
        CHECK(out[u] == kBlack);
    }
    for (std::size_t j = 0; j < 24; ++j) {
        CHECK(out[9 + j] == cells[j]);  // 24 Zellen auf 24 Abschnitte: 1:1
    }
    for (std::size_t k = 0; k < 7; ++k) {
        CHECK(out[33 + k] == zone[k]);
    }
    // Ohne Effektbild bleibt der Effektbereich dunkel.
    const auto dark = prepare_led_frame(cells, StripLayout{40, 9, 7}, false, 1.0);
    CHECK(dark[33] == kBlack);
    CHECK(dark[39] == kBlack);
}

TEST_CASE("Layout: ohne Segmente und Versatz bleibt alles 1:1, zu großer Versatz lässt eine Einheit übrig") {
    const std::vector<Rgb> cells{{1, 0, 0}, {2, 0, 0}, {3, 0, 0}};
    CHECK(prepare_led_frame(cells, StripLayout{0, 0}, false, 1.0) == cells);
    const auto out = prepare_led_frame(cells, StripLayout{3, 9}, false, 1.0);
    CHECK(out == std::vector<Rgb>{kBlack, kBlack, Rgb{1, 0, 0}});
}

}  // namespace ldu::govee
