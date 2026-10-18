// Govee-Treiber: Spielbilder über den nicht dokumentierten Razer-Modus (govee/protocol.hpp).
//
// Start: Gerät einschalten („turn“), Razer-Modus an. Je Bild ein B0-Paket; alle kRearmS
// Sekunden wird der Razer-Modus erneut eingeschaltet (DMXCore macht das ebenso): Fällt
// das Gerät aus dem Modus, weil jemand die App bedient oder das WLAN kurz weg war, kommt
// das Spielbild von selbst zurück.
//
// Ende: 2 s Ruhe nach dem letzten Bild, dann den Razer-Modus DREIMAL aus. Gemessen
// 2026-10-18 am H612C (Nutzer schaute hin):
//   1× Aus direkt nach dem letzten Bild           → blieb minutenlang auf dem Bild stehen,
//                                                   auch das laut OpenRGB erwartete
//                                                   Zurückfallen nach einer Minute kam nicht
//   3× Aus nach 0,15 s (Fenster geschlossen)      → blieb stehen
//   3× Aus nach 2 s                               → zurück zum Zustand davor (Tageslicht)
//   3× Aus nach Minuten                           → zurück zur Szene
// Kürzere Pausen als 2 s sind nicht als sicher belegt. Wer die Ruhe kürzt, prüft am
// Streifen nach. War der Streifen vor dem Spiel aus (devStatus, siehe query_status), geht
// er danach wieder aus; ohne bekannten Zustand bleibt er an.
//
// Threads: nicht threadsicher; gehört dem Spielthread.
// Fehler: frame() wirft Error(protocol) bei mehr als kMaxRazerColors Farben.

#pragma once

#include "lights/output.hpp"

namespace ldu::govee {

inline constexpr double kKeepaliveS = 2.0;
inline constexpr double kRearmS = 2.0;

struct GoveeOptions final {
    bool turn_off_at_end = false;  // war vor dem Spiel aus → beim Beenden wieder aus
    // Beenden: erst stop_settle_s Ruhe nach dem letzten Bild, dann stop_repeats-mal Aus im
    // Abstand stop_gap_s (Messung siehe Kopf).
    double stop_settle_s = 2.0;
    int stop_repeats = 3;
    double stop_gap_s = 0.1;
};

class GoveeProtocol final : public lights::LightProtocol {
public:
    explicit GoveeProtocol(GoveeOptions options = {});

    std::string name() const override;
    int max_units() const override;
    double keepalive_s() const override;
    std::vector<std::string> start(double now_s) override;
    std::vector<std::string> frame(std::span<const Rgb> leds, double now_s) override;
    std::vector<lights::StopStep> stop(std::span<const Rgb> last) override;

private:
    GoveeOptions options_;
    double last_mode_s_ = 0.0;
};

}  // namespace ldu::govee
