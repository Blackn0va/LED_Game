// sACN-Treiber: Spielbilder als E1.31-Datenpakete (sacn/protocol.hpp).
//
// Je 170 LEDs ein Universum (3 Kanäle je LED), aufeinanderfolgend ab dem Start-Universum:
// 300 LEDs ab 1 gehen als Universum 1 (LED 0–169) und 2 (LED 170–299). Jedes Universum hat
// seine eigene Sequenznummer – E1.31 verlangt das so (erinnert, nicht im Normtext
// nachgeschlagen); mit einer gemeinsamen sähe ein Empfänger je Universum nur Sprünge.
//
// Ziel ist die IP des Empfängers (Unicast) oder ohne IP je Universum dessen Multicast-
// Gruppe – das erledigt UniverseRouter (sacn/router.hpp), der Treiber baut nur die Pakete.
// Haltebild jede Sekunde: E1.31 wertet eine Quelle nach 2,5 s ohne Paket als verloren
// (E131_NETWORK_DATA_LOSS_TIMEOUT, erinnert, nicht nachgeschlagen).
//
// Ende: je Universum dreimal ein Paket mit gesetztem „Strom endet“ (Optionen 0x40), wie
// E1.31 §6.2.6 es verlangt. Empfänger wie WLED geben das Universum dann sofort frei, statt
// die Frist abzuwarten.
//
// Threads: nicht threadsicher; gehört dem Spielthread.
// Fehler: Konstruktor wirft Error(usage) bei einem Universum außerhalb 1–63999; frame()
// wirft Error(protocol), wenn das Bild über 63999 hinausreichen würde.

#pragma once

#include "lights/output.hpp"
#include "sacn/protocol.hpp"

#include <cstdint>
#include <vector>

namespace ldu::sacn {

inline constexpr double kKeepaliveS = 1.0;
inline constexpr int kMaxLeds = 2040;  // gewählte Grenze, siehe lights/device.hpp
inline constexpr int kTerminateRepeats = 3;

class SacnProtocol final : public lights::LightProtocol {
public:
    SacnProtocol(int universe, Cid cid);

    std::string name() const override;
    int max_units() const override;
    double keepalive_s() const override;
    std::vector<std::string> start(double now_s) override;
    std::vector<std::string> frame(std::span<const Rgb> leds, double now_s) override;
    std::vector<lights::StopStep> stop(std::span<const Rgb> last) override;

private:
    // Ein Paket je Universum; options gilt für alle.
    std::vector<std::string> packets(std::span<const Rgb> leds, std::uint8_t options);

    PacketFields fields_;
    int first_universe_;
    std::vector<std::uint8_t> sequences_;  // je Universum ab Start
};

}  // namespace ldu::sacn
