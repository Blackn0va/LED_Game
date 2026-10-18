#include "wled/discovery.hpp"

#include "core/error.hpp"
#include "net/http.hpp"
#include "net/mdns.hpp"
#include "net/udp.hpp"

#include <algorithm>
#include <utility>

namespace ldu::wled {
namespace {

using namespace std::chrono_literals;

inline constexpr auto kInfoTimeout = 1500ms;

}  // namespace

std::vector<Found> browse(std::chrono::milliseconds wait) {
    // Port 0: Legacy-Unicast-Anfrage (net/mdns.hpp) – 5353 bleibt dem DNS-Client-Dienst.
    auto socket = net::DiscoverySocket::open(0, "kein freier UDP-Port für die mDNS-Suche");
    std::string error;
    if (socket->send_multicast(std::string(net::kMdnsGroup), net::kMdnsPort, net::mdns_query(kMdnsService), error) ==
        0) {
        throw Error(Error::Kind::network, error);
    }
    std::vector<Found> found;
    const auto problem = socket->receive_until(
        std::chrono::steady_clock::now() + wait, [&found](std::string_view datagram, const std::string& from) {
            const auto instances = net::parse_mdns_ptr(datagram, kMdnsService);
            if (!instances || instances->empty()) {
                return false;
            }
            const bool known =
                std::any_of(found.begin(), found.end(), [&from](const Found& f) { return f.ip == from; });
            if (!known) {
                found.push_back(Found{from, instances->front(), std::nullopt});
            }
            return false;
        });
    if (problem) {
        throw Error(Error::Kind::network, *problem);
    }
    for (Found& f : found) {
        std::string why;
        f.info = fetch_info(f.ip, kInfoTimeout, why);
    }
    return found;
}

std::optional<WledInfo> fetch_info(const std::string& ip, std::chrono::milliseconds timeout, std::string& error) {
    const auto response = net::http_get(ip, kHttpPort, "/json/info", timeout, error);
    if (!response) {
        return std::nullopt;
    }
    if (response->status != 200) {
        error = "HTTP " + std::to_string(response->status) + " auf /json/info";
        return std::nullopt;
    }
    auto info = parse_info(response->body);
    if (!info) {
        error = "/json/info ist keine WLED-Antwort";
    }
    return info;
}

}  // namespace ldu::wled
