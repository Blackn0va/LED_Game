// WLED-Treiber: Spielbilder als DRGB- bzw. DNRGB-Pakete (wled/protocol.hpp).
//
// Bis 490 LEDs ein DRGB-Paket je Bild, darüber DNRGB-Teilbilder zu je 489 LEDs mit
// Startindex. DRGB bleibt für kurze Streifen, weil es seit jeher in WLED steckt; DNRGB kam
// später dazu – ob ältere Firmware es kennt, ist nicht geprüft.
//
// Kein Start-Paket: WLED wechselt mit dem ersten Echtzeit-Paket von selbst in den
// Echtzeitmodus. Jedes Paket trägt eine Frist von kRunTimeoutS Sekunden – stirbt das
// Programm hart (Fenster weggeklickt, Absturz), kehrt WLED nach dieser Frist von allein
// zu seinem Effekt zurück. Am Govee H612C war genau das das Problem (er blieb stehen).
// Das Haltebild geht deshalb alle kKeepaliveS Sekunden, deutlich unter der Frist, damit
// ein einzelnes verlorenes Paket den Echtzeitmodus nicht beendet.
//
// Ende: das letzte Bild noch einmal mit Frist kStopTimeoutS – WLED geht dann nach einer
// Sekunde zurück, statt nach kRunTimeoutS.
//
// Threads: nicht threadsicher; gehört dem Spielthread.
// Fehler: frame() wirft Error(protocol) bei mehr als 65536 Farben (Index des DNRGB-Pakets);
// die Ausgabe lässt ohnehin nur kMaxLeds zu.

#pragma once

#include "lights/output.hpp"

#include <cstdint>

namespace ldu::wled {

inline constexpr std::uint8_t kRunTimeoutS = 2;
inline constexpr std::uint8_t kStopTimeoutS = 1;
inline constexpr double kKeepaliveS = 0.5;
inline constexpr int kMaxLeds = 2040;  // gewählte Grenze, siehe lights/device.hpp

// Pakete für ein Bild: ein DRGB bis 490 LEDs, sonst DNRGB-Teilbilder zu je 489.
std::vector<std::string> frame_packets(std::span<const Rgb> leds, std::uint8_t timeout_s);

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
