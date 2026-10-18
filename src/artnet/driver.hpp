// Art-Net-Treiber: Spielbilder als ArtDmx an einen Knoten (artnet/protocol.hpp).
//
// Ein Universum, 3 Kanäle je LED (RGB) – höchstens 170 LEDs. Unicast an die IP des
// Knotens; Art-Net 4 verlangt für ArtDmx Unicast, Broadcast ist nur Altbestand. Das
// Haltebild geht jede Sekunde – sicher unter den Fristen, nach denen Empfänger den Strom
// als abgerissen werten (Art-Net 4: einige Sekunden; WLED: 2,5 s Vorgabe in dessen
// Sync-Einstellungen). Beide Werte sind erinnert, nicht nachgemessen.
//
// Ende: ein schwarzes Bild. Art-Net kennt kein „Strom beendet“; ein Knoten ohne
// Rückfallmodus hielte sonst das letzte Spielbild.
//
// Threads: nicht threadsicher; gehört dem Spielthread.
// Fehler: Konstruktor wirft Error(usage) bei einem Universum außerhalb 0–32767.

#pragma once

#include "lights/output.hpp"

#include <cstdint>

namespace ldu::artnet {

inline constexpr double kKeepaliveS = 1.0;
inline constexpr int kMaxLeds = 170;

class ArtNetProtocol final : public lights::LightProtocol {
public:
    explicit ArtNetProtocol(int port_address);

    std::string name() const override;
    int max_units() const override;
    double keepalive_s() const override;
    std::vector<std::string> start(double now_s) override;
    std::vector<std::string> frame(std::span<const Rgb> leds, double now_s) override;
    std::vector<lights::StopStep> stop(std::span<const Rgb> last) override;

private:
    std::uint8_t next_sequence();

    int port_address_;
    std::uint8_t sequence_ = 0;
};

}  // namespace ldu::artnet
