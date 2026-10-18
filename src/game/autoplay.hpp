// Automodus: der Computer spielt allein – langsam als Lichtstimmung, schnell als Feuerwerk.
//
// Voreinstellungen (--auto):
//   langsam  Tempo ×0,5, ruhiger Spieler (überlegt lange, selten daneben), 8 s Pause
//            nach dem Spielende – eher Zimmerlicht als Wettkampf.
//   normal   Tempo ×1, wie die Demo.
//   schnell  Tempo ×4, Blitzreflexe, Joker ohne Zögern, 3 s Pause – die Decke wird zum
//            Feuerwerk; der Streifen bekommt dabei weiter seine 30 Bilder je Sekunde.
// Das Tempo lässt sich im Spiel stufenweise ändern (kTempoSteps).
//
// GameClock teilt die Echtzeit in feste Spielschritte von 1/60 s – bei jedem Tempo gleich.
// So trifft der Computerspieler seine Entscheidungen im selben Takt wie in den Tests, und
// ×8 ist nicht ×1 mit achtmal so großen, ungenaueren Schritten. Zeitlupe (etwa beim Sieg
// über einen Endgegner) dämpft das Tempo für kurze Zeit.
//
// Threads: reine Werte bzw. ein Objekt je Thread.
// Fehler: keine.

#pragma once

#include "game/autopilot.hpp"

#include <array>
#include <string>
#include <string_view>

namespace ldu {

enum class AutoSpeed { slow, normal, fast };

struct AutoPreset final {
    double tempo = 1.0;
    AutopilotProfile profile;
    double restart_s = 6.0;  // Echtzeit zwischen Spielende und neuer Partie
    std::string_view name;
};

AutoPreset auto_preset(AutoSpeed speed);

inline constexpr std::array<double, 9> kTempoSteps{0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 8.0};
inline constexpr double kGameStepS = 1.0 / 60.0;

// Index der Stufe, die `tempo` am nächsten liegt.
std::size_t nearest_tempo_step(double tempo);
// „×0,5“, „×4“ – deutsches Komma, ohne überflüssige Nullen.
std::string tempo_label(double tempo);

class GameClock final {
public:
    explicit GameClock(double tempo = 1.0) : tempo_(tempo) {}

    void set_tempo(double tempo) noexcept {
        tempo_ = tempo;
    }
    double tempo() const noexcept {
        return tempo_;
    }
    // Für `duration_s` Echtzeit ab `now_s` läuft das Spiel mit `factor` des Tempos.
    void slow_motion(double now_s, double duration_s, double factor);
    bool slow_motion_active(double now_s) const noexcept {
        return now_s < slow_until_s_;
    }

    // Echtzeit seit dem letzten Aufruf → Zahl der Spielschritte (je kGameStepS), höchstens
    // max_steps; Reste bleiben für das nächste Mal. Nach einem Hänger (riesiges real_dt)
    // wird nicht nachgeholt, sondern verworfen – das Spiel soll nicht „springen“.
    int advance(double now_s, double real_dt_s, int max_steps = 40);

private:
    double tempo_;
    double slow_until_s_ = -1.0;
    double slow_factor_ = 1.0;
    double accumulator_s_ = 0.0;
};

}  // namespace ldu
