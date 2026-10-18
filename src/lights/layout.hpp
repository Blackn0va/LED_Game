// Wie das Spielbild auf den physischen Streifen fällt – für jede Geräteart gleich.
//
// Vom Kanonen-Ende aus:
//   [ skip dunkel ][ Kampffeld ][ zone Effektbereich ]
//   segments  Farben je Bild an das Gerät; 0 = so viele wie Spielzellen (1:1).
//             Grenzen je Geräteart in lights/device.hpp.
//   skip      Einheiten am Kanonen-Ende, die dunkel bleiben (etwa hinter einem Bett).
//   zone      Einheiten am Computer-Ende für den Effektbereich (Game::render_zone).
// Die Spielzellen werden gleichmäßig auf segments - skip - zone Einheiten verteilt.
//
// Threads: reine Funktionen.
// Fehler: keine Würfe.

#pragma once

#include "core/rgb.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace ldu::lights {

struct StripLayout final {
    int segments = 0;
    int skip = 0;
    int zone = 0;
};

// Spielbild → Streifenbild: auf das Layout verteilt (Versatz dunkel), Helligkeit
// skaliert, optional gespiegelt. Gespiegelt wird zuletzt – der Versatz bleibt so immer am
// Kanonen-Ende, auch mit --umgedreht.
std::vector<Rgb> prepare_led_frame(std::span<const Rgb> field, std::span<const Rgb> zone, const StripLayout& layout,
                                   bool reversed, double brightness);
// Ohne Effektbild: der Effektbereich bleibt dunkel.
std::vector<Rgb> prepare_led_frame(std::span<const Rgb> frame, const StripLayout& layout, bool reversed,
                                   double brightness);

// R G B R G B … – die DMX-Kanalfolge für Art-Net und sACN (3 Kanäle je LED, RGB-Reihenfolge;
// GRB-Streifen stellt man am Empfänger um, nicht hier).
std::vector<std::uint8_t> rgb_bytes(std::span<const Rgb> leds);

}  // namespace ldu::lights
