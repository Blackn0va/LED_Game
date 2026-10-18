// Schwierigkeit je Stufe.
//
// Geschwindigkeiten stehen als „Zeit für die ganze Strecke“, nicht als Zellen je
// Sekunde: Dann spielt sich ein 15-Zellen-Streifen genauso schnell wie einer mit 84
// Zellen, und --leds ändert nur die Auflösung, nicht die Schwierigkeit.
//
// Ablauf: 1–2 normal mit Rot/Grün/Blau, 3 Farbwurm, 4–5 mit Gelb, 6 Chamäleon, 7–8 mit
// Lila, 9 Kanonier, danach immer zwei normale Stufen und ein Endgegner. Neue Farben kommen
// bewusst NACH einem Endgegner, nie mitten im Kampf.
//
// Threads: reine Funktionen.
// Fehler: keine; Stufen unter 1 zählen als Stufe 1.

#pragma once

namespace ldu {

inline constexpr int kMaxStartLevel = 30;

struct LevelParams final {
    double enemy_crossing_s = 0.0;  // so lange braucht ein Klumpen vom Computer bis zur Kanone
    double spawn_interval_s = 0.0;  // Abstand zwischen zwei neuen Klumpen
    int colors = 0;                 // Farben im Spiel (die ersten n aus Color)
    int max_blob_size = 0;          // längster neuer Klumpen
    int blobs_to_next = 0;          // aufgelöste Klumpen bis zur nächsten Stufe
    bool boss = false;              // Endgegner-Stufe: kein Nachschub, Sieg = nächste Stufe
};

LevelParams level_params(int level);

// Erste Stufe, in der mindestens `colors` Farben im Spiel sind.
int first_level_with_colors(int colors);

}  // namespace ldu
