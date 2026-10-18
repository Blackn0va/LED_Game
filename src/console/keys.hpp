// Tastenbelegung des Spiels.
//
// Farben liegen auf 1–5 (Hauptfeld und Ziffernblock) und zusätzlich auf R, G, B, Y, L
// (Rot, Grün, Blau, Yellow, Lila). Buchstaben laufen über den virtuellen Tastencode,
// der unter QWERTZ wie unter QWERTY der Beschriftung folgt. Die Leertaste ist der Joker;
// neu gestartet wird nur mit Enter – wer nach dem letzten Leben weiter auf die
// Leertaste hämmert, soll nicht versehentlich sofort ein neues Spiel beginnen.
// Pfeiltasten verschieben im Testbild die Grenzen (S speichert) und ändern im Automodus das
// Tempo. A schaltet den Automodus mitten im Spiel an und aus (Wunsch des Nutzers: per
// Tastatur statt nur per --auto).
//
// Threads: reine Funktionen.
// Fehler: keine.

#pragma once

#include "core/rgb.hpp"

#include <cstdint>
#include <optional>

namespace ldu::console {

enum class Action { none, quit, pause, restart, joker, left, right, up, down, save, toggle_auto };

std::optional<Color> color_for_key(std::uint16_t vk);
Action action_for_key(std::uint16_t vk);

}  // namespace ldu::console
