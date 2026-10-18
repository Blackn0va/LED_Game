// Testbild zum Einrichten des Streifens (--testbild).
//
// Beantwortet am Streifen, was kein Test hier beantworten kann: Wo beginnt das Kampffeld,
// wo endet es, wie groß ist der Effektbereich, liegt die Kanone am richtigen Ende? Das
// frühere Muster (eine Farbe je Spielzelle, Rot/Grün/Blau/Gelb) ist im Blockbild aufgegangen.
//
// Threads: reine Funktion.
// Fehler: keine; unsinnige Werte werden auf den Streifen begrenzt.

#pragma once

#include "core/rgb.hpp"

#include <vector>

namespace ldu {

inline constexpr double kCalibrationDim = 0.6;
inline constexpr double kHiddenDim = 0.25;
inline constexpr Rgb kZoneMarker{255, 140, 0};

// Block-Testbild (--testbild): eine Farbe je Abschnitt, drei Bereiche vom Kanonen-Ende:
//   Versatz (skip)        dunkel wie im Spiel
//   erster Feld-Abschnitt pulsiert weiß – hier sitzt die Kanone
//   Kampffeld             Grün und Blau im Wechsel je Abschnitt, jeder 10. weiß,
//                         der letzte lila – das Computerende
//   Effektbereich (zone)  Orange hell/dunkel im Wechsel je Abschnitt
// Mit den Pfeiltasten verschiebt der Nutzer die Grenzen, bis Weiß an der linken
// Deckenecke und Lila an der rechten sitzt (Wunsch des Nutzers, 2026-10-18).
std::vector<Rgb> segment_calibration_frame(int segments, int skip, int zone, double t_s);

}  // namespace ldu
