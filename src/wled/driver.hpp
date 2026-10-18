// WLED-Treiber: Spielbilder als DRGB-Pakete (wled/protocol.hpp).
//
// Kein Start-Paket: WLED wechselt mit dem ersten Echtzeit-Paket von selbst in den
// Echtzeitmodus. Jedes Bild trägt eine Frist von kRunTimeoutS Sekunden – stirbt das
// Programm hart (Fenster weggeklickt, Absturz), kehrt WLED nach dieser Frist von allein
// zu seinem Effekt zurück. Am Govee H612C war genau das das Problem (er blieb stehen).
// Das Haltebild geht deshalb alle kKeepaliveS Sekunden, deutlich unter der Frist, damit
// ein einzelnes verlorenes Paket den Echtzeitmodus nicht beendet.
//
// Ende: das letzte Bild noch einmal mit Frist kStopTimeoutS – WLED geht dann nach einer
// Sekunde zurück, statt nach kRunTimeoutS.
//
// Threads: nicht threadsicher; gehört dem Spielthread.
// Fehler: frame() wirft Error(protocol) bei mehr als kMaxDrgbLeds Farben.

#pragma once

#include "lights/output.hpp"

#include <cstdint>

namespace ldu::wled {

inline constexpr std::uint8_t kRunTimeoutS = 2;
inline constexpr std::uint8_t kStopTimeoutS = 1;
inline constexpr double kKeepaliveS = 0.5;

class WledProtocol final : public lights::LightProtocol {
public:
    std::string name() const override;
    int max_units() const override;
    double keepalive_s() const override;
    std::vector<std::string> start(double now_s) override;
    std::vector<std::string> frame(std::span<const Rgb> leds, double now_s) override;
    std::vector<lights::StopStep> stop(std::span<const Rgb> last) override;
};

}  // namespace ldu::wled
