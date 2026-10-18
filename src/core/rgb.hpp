// Farbwert einer LED-Zelle und die Spielfarben.
//
// Ein eigener kleiner Typ statt QColor oder COLORREF: Spiellogik, Govee-Protokoll und
// Konsolenbild teilen dieselben Werte, und keiner dieser Teile soll Windows- oder
// GUI-Header brauchen. Die Reihenfolge der Farben ist zugleich die Reihenfolge, in der
// sie mit den Stufen dazukommen, und die Tastenbelegung 1–5.
//
// Threads: reine Werte, beliebig nutzbar.
// Fehler: keine.

#pragma once

#include <cstdint>
#include <string_view>

namespace ldu {

struct Rgb final {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    friend bool operator==(const Rgb&, const Rgb&) = default;
};

inline constexpr Rgb kBlack{0, 0, 0};
inline constexpr Rgb kWhite{255, 255, 255};

enum class Color : std::uint8_t { red, green, blue, yellow, purple };
inline constexpr int kColorCount = 5;

Rgb to_rgb(Color color);
std::string_view color_name(Color color);

// Mischt a und b linear; t = 0 ergibt a, t = 1 ergibt b (t wird auf 0..1 begrenzt).
Rgb blend(Rgb a, Rgb b, double t);

// Skaliert die Helligkeit; factor wird auf 0..1 begrenzt.
Rgb scale(Rgb color, double factor);

// Vollgesättigte Farbe auf dem Farbkreis; turns in Umdrehungen (0 = Rot, 1/3 = Grün,
// 2/3 = Blau), wird beliebig umgebrochen. Für Regenbogen-Effekte und den Joker.
Rgb hue(double turns);

}  // namespace ldu
