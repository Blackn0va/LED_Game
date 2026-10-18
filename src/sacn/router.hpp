// sACN-Multicast über mehrere Universen: jedes Paket an die Gruppe SEINES Universums.
//
// LightOutput kennt nur eine Senke. Für Unicast reicht das (alle Universen an dieselbe IP),
// für Multicast nicht: Universum 2 gehört an 239.255.0.2, nicht an 239.255.0.1 – ein
// Empfänger, der nur Universum 2 abonniert hat, sähe sonst nichts. Der Router liest das
// Universum aus dem fertigen Paket (Byte 113/114, sacn/protocol.hpp) und hält je Gruppe
// eine eigene Senke. So bleibt die Treiber-Schnittstelle bei „ein Paket, eine Senke“.
//
// Threads: nicht threadsicher; gehört dem Spielthread.
// Fehler: send() wirft nicht (wie jede PacketSink); ein zu kurzes Paket oder eine nicht zu
// öffnende Gruppe liefert false plus Grund.

#pragma once

#include "net/udp.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace ldu::sacn {

class UniverseRouter final : public net::PacketSink {
public:
    // open baut die Senke für eine Multicast-Gruppe (im Spiel: UdpSender an group:5568,
    // im Test: eine Fake-Senke). Je Gruppe wird open genau einmal gerufen.
    using Opener = std::function<std::unique_ptr<net::PacketSink>(const std::string& group)>;

    explicit UniverseRouter(Opener open);

    bool send(std::string_view payload, std::string& error) override;

private:
    Opener open_;
    std::map<int, std::unique_ptr<net::PacketSink>> sinks_;  // Universum → Senke
};

// Universum eines fertigen E1.31-Datenpakets; -1, wenn das Paket zu kurz ist.
int packet_universe(std::string_view payload);

}  // namespace ldu::sacn
