#include "sacn/router.hpp"

#include "core/error.hpp"
#include "sacn/protocol.hpp"

#include <utility>

namespace ldu::sacn {

int packet_universe(std::string_view payload) {
    if (payload.size() < kHeaderSize) {
        return -1;
    }
    return (static_cast<unsigned char>(payload[113]) << 8) | static_cast<unsigned char>(payload[114]);
}

UniverseRouter::UniverseRouter(Opener open) : open_(std::move(open)) {}

bool UniverseRouter::send(std::string_view payload, std::string& error) {
    const int universe = packet_universe(payload);
    if (universe < kMinUniverse || universe > kMaxUniverse) {
        error = "kein sACN-Datenpaket mit gültigem Universum";
        return false;
    }
    auto it = sinks_.find(universe);
    if (it == sinks_.end()) {
        try {
            it = sinks_.emplace(universe, open_(multicast_group(universe))).first;
        } catch (const Error& e) {
            // Bewusst fail-open wie jeder Sendefehler (lights/output.hpp): ein Universum, das
            // sich nicht öffnen lässt, zählt in der Statuszeile, das Spiel läuft weiter.
            error = e.what();
            return false;
        }
    }
    return it->second->send(payload, error);
}

}  // namespace ldu::sacn
