#include "artnet/discovery.hpp"

#include "core/error.hpp"
#include "net/udp.hpp"

#include <algorithm>
#include <utility>

namespace ldu::artnet {

std::vector<ArtNode> browse(std::chrono::milliseconds wait) {
    auto socket = net::DiscoverySocket::open(
        kPort, "UDP-Port 6454 ist belegt – läuft schon ein Art-Net-Programm (Jinx, xLights, QLC+)? "
               "Das beenden oder den Knoten direkt mit --ip angeben.");
    std::string error;
    if (socket->send_broadcast(kPort, art_poll(), error) == 0) {
        throw Error(Error::Kind::network, error);
    }
    std::vector<ArtNode> found;
    const auto problem = socket->receive_until(
        std::chrono::steady_clock::now() + wait, [&found](std::string_view datagram, const std::string& from) {
            // Das eigene ArtPoll kommt über den Broadcast zurück; parse_poll_reply verwirft es.
            auto node = parse_poll_reply(datagram);
            if (!node) {
                return false;
            }
            if (node->ip == "0.0.0.0" || !net::is_ipv4(node->ip)) {
                node->ip = from;
            }
            // Knoten mit mehreren Ausgangsgruppen antworten je Gruppe einmal: nur die erste zählt.
            const bool known = std::any_of(found.begin(), found.end(),
                                           [&node](const ArtNode& n) { return n.ip == node->ip; });
            if (!known) {
                found.push_back(std::move(*node));
            }
            return false;
        });
    if (problem) {
        throw Error(Error::Kind::network, *problem);
    }
    return found;
}

}  // namespace ldu::artnet
