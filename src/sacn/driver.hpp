// sACN-Treiber: Spielbilder als E1.31-Datenpakete (sacn/protocol.hpp).
//
// Ziel ist die IP des Empfängers (Unicast) oder ohne IP die Multicast-Gruppe des
// Universums – das wählt devices/connect.cpp, der Treiber baut nur die Pakete. Ein
// Universum, 3 Kanäle je LED – höchstens 170 LEDs. Haltebild jede Sekunde: E1.31 wertet
// eine Quelle nach 2,5 s ohne Paket als verloren (E131_NETWORK_DATA_LOSS_TIMEOUT,
// erinnert, nicht nachgeschlagen).
//
// Ende: dreimal ein Paket mit gesetztem „Strom endet“ (Optionen 0x40), wie E1.31 §6.2.6
// es verlangt. Empfänger wie WLED geben das Universum dann sofort frei, statt die Frist
// abzuwarten.
//
// Threads: nicht threadsicher; gehört dem Spielthread.
// Fehler: Konstruktor wirft Error(usage) bei einem Universum außerhalb 1–63999.

#pragma once

#include "lights/output.hpp"
#include "sacn/protocol.hpp"

#include <cstdint>

namespace ldu::sacn {

inline constexpr double kKeepaliveS = 1.0;
inline constexpr int kMaxLeds = 170;
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
    std::string packet(std::span<const Rgb> leds, std::uint8_t options);

    PacketFields fields_;
};

}  // namespace ldu::sacn
