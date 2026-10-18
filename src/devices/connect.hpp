// Geräte finden, ein Ziel festlegen und die passende Ausgabe bauen – für alle Hersteller.
//
// Hier laufen die Treiber zusammen; das Spiel (app/main.cpp) kennt nur DeviceType,
// Target und lights::LightOutput. Ein neuer Hersteller bekommt in connect.cpp je einen
// Fall in discover(), resolve() und open_output() (CONTRIBUTING.md).
//
// Die gemeinsame Suche fragt Govee (Multicast, Port 4002), WLED (mDNS) und Art-Net
// (ArtPoll, Port 6454) GLEICHZEITIG, je mit eigenem Socket: nacheinander wären es gut 6 s
// statt 2. sACN-Empfänger melden sich in E1.31 nicht von selbst – sie stehen nie in der
// Liste und werden per --gerät sacn (Multicast) oder --ip angesprochen.
//
// Threads: discover_all startet je Hersteller einen Thread und wartet auf alle; sonst ein
// Thread. NetworkSession muss bestehen.
// Fehler: resolve wirft Error(network) mit Hinweis, wenn nichts antwortet, und
// Error(usage) mit Liste, wenn mehrere Geräte passen. discover_all wirft nicht – eine
// gescheiterte Suche (Port belegt) steht in Report::problems, die übrigen laufen weiter.

#pragma once

#include "lights/device.hpp"
#include "lights/output.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ldu::devices {

struct Found final {
    lights::DeviceType type = lights::DeviceType::govee;
    std::string ip;
    std::string name;              // Modell (Govee), Gerätename (WLED), Kurzname (Art-Net)
    int leds = 0;                  // vom Gerät gemeldet (WLED), 0 = unbekannt
    std::optional<int> universe;   // Art-Net: Port-Adresse des ersten Ausgangs
    std::string detail;            // Zusatz für die Liste (Geräte-ID, Version …)
};

struct Report final {
    std::vector<Found> found;
    std::vector<std::string> problems;
};

Report discover(lights::DeviceType type, std::chrono::milliseconds wait);
Report discover_all(std::chrono::milliseconds wait);

// Ergänzt ein von Hand angegebenes Gerät um das, was es selbst meldet (WLED: Name und
// LED-Zahl über /json/info). Leer = nichts zu melden, sonst ein Satz, warum das Gerät nicht
// antwortete (fail-open: dann gelten die Vorgaben).
std::string complete_manual(Found& f);

// Eine Zeile für Listen: "Govee    192.168.1.5      H612C · …".
std::string found_line(const Found& f);
// Was tun, wenn ein Gerät dieser Art nicht antwortet.
std::string no_device_hint(lights::DeviceType type);

struct Request final {
    lights::DeviceType type = lights::DeviceType::govee;
    std::string ip;                // leer = suchen (sACN: Multicast)
    std::optional<int> universe;   // leer = Vorgabe bzw. aus der Suche
};

struct Target final {
    lights::DeviceType type = lights::DeviceType::govee;
    std::string ip;                // leer = sACN-Multicast
    std::string label;             // Name oder Modell, darf leer sein
    int leds = 0;                  // WLED: gemeldete LED-Zahl, 0 = unbekannt
    int universe = 0;              // Art-Net/sACN
    int port = 0;                  // UDP-Zielport, 0 = Vorgabe der Geräteart
    bool turn_off_at_end = false;  // Govee: war vor dem Spiel aus
    std::string note;              // Zusatz für die Statuszeile
};

using Say = std::function<void(const std::string&)>;

// Ziel aus der IP oder aus einer Suche nur dieser Geräteart. Fragt Govee nach seinem
// Zustand und WLED nach /json/info – beides ändert nichts am Licht.
Target resolve(const Request& request, const Say& say);

// Ausgabe für das Ziel; settings.target setzt open_output selbst.
std::unique_ptr<lights::LightOutput> open_output(const Target& target, lights::OutputSettings settings);

}  // namespace ldu::devices
