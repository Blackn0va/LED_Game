// Art-Net-Treiber: Spielbilder als ArtDmx an einen Knoten (artnet/protocol.hpp).
//
// Je 170 LEDs ein Universum (3 Kanäle je LED, RGB), aufeinanderfolgend ab der
// Start-Port-Adresse: 300 LEDs ab 0 gehen als Universum 0 (LED 0–169) und 1 (LED 170–299).
// Die Port-Adresse zählt einfach weiter, auch über Sub-Net-Grenzen (15 → 16 heißt Sub-Net 1,
// Universum 0) – so nummerieren Pixel-Controller und xLights fortlaufende Universen.
//
// Sequenz je Universum, nicht gemeinsam: Ob Art-Net 4 sie je Port-Adresse oder je Knoten
// meint, war nicht nachzulesen (Spezifikations-PDF nicht lesbar, 2026-10-18). Je Universum
// passt zu beiden Lesarten: Ein Empfänger, der je Port-Adresse prüft, sieht lückenlos
// 1, 2, 3 …; einer, der je Knoten prüft, sieht innerhalb eines Bildes dieselbe Zahl für jedes
// Universum, aber nie eine kleinere – und verworfen werden laut gängigen Umsetzungen nur
// kleinere (nicht in der Spezifikation geprüft).
//
// Unicast an die IP des Knotens; Art-Net 4 verlangt für ArtDmx Unicast, Broadcast ist nur
// Altbestand. Das Haltebild geht jede Sekunde – sicher unter den Fristen, nach denen
// Empfänger den Strom als abgerissen werten (Art-Net 4: einige Sekunden; WLED: 2,5 s
// Vorgabe in dessen Sync-Einstellungen). Beide Werte sind erinnert, nicht nachgemessen.
//
// Ende: je Universum ein schwarzes Bild. Art-Net kennt kein „Strom beendet“; ein Knoten
// ohne Rückfallmodus hielte sonst das letzte Spielbild.
//
// Threads: nicht threadsicher; gehört dem Spielthread.
// Fehler: Konstruktor wirft Error(usage) bei einer Start-Port-Adresse außerhalb 0–32767;
// frame() wirft Error(protocol), wenn das Bild über 32767 hinausreichen würde.

#pragma once

#include "lights/output.hpp"

#include <cstdint>
#include <vector>

namespace ldu::artnet {

inline constexpr double kKeepaliveS = 1.0;
inline constexpr int kMaxLeds = 2040;  // gewählte Grenze, siehe lights/device.hpp

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
    std::vector<std::string> packets(std::span<const Rgb> leds, bool black);
    std::uint8_t next_sequence(std::size_t universe_index);

    int port_address_;
    std::vector<std::uint8_t> sequences_;  // je Universum ab Start
};

}  // namespace ldu::artnet
