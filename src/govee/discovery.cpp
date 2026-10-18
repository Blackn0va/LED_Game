#include "govee/discovery.hpp"

#include "core/error.hpp"
#include "net/udp.hpp"

#include <algorithm>
#include <utility>

namespace ldu::govee {
namespace {

inline constexpr std::string_view kBusyHint =
    "UDP-Port 4002 ist belegt – meist läuft Govee Desktop oder ein Smart-Home-Dienst. "
    "Den beenden oder den Streifen direkt mit --ip angeben.";

}  // namespace

std::vector<DeviceInfo> scan_devices(std::chrono::milliseconds wait) {
    auto socket = net::DiscoverySocket::open(kReplyPort, kBusyHint);
    std::string error;
    if (socket->send_multicast(std::string(kMulticastGroup), kScanPort, scan_message(), error) == 0) {
        throw Error(Error::Kind::network, error);
    }
    std::vector<DeviceInfo> found;
    const auto problem = socket->receive_until(
        std::chrono::steady_clock::now() + wait, [&found](std::string_view datagram, const std::string& from) {
            auto info = parse_scan_reply(datagram);
            if (!info) {
                return false;
            }
            // Die IP im JSON ist die, die das Gerät selbst kennt; ist sie unbrauchbar, gilt
            // der Absender des Datagramms.
            if (!net::is_ipv4(info->ip)) {
                info->ip = from;
            }
            const bool known = std::any_of(found.begin(), found.end(),
                                           [&info](const DeviceInfo& d) { return d.ip == info->ip; });
            if (!known) {
                found.push_back(std::move(*info));
            }
            return false;
        });
    if (problem) {
        throw Error(Error::Kind::network, *problem);
    }
    return found;
}

std::optional<DeviceStatus> query_status(const std::string& ip, std::chrono::milliseconds wait, std::string& error) {
    const auto canonical = net::canonical_ipv4(ip);
    if (!canonical) {
        error = "„" + ip + "“ ist keine IPv4-Adresse.";
        return std::nullopt;
    }
    std::unique_ptr<net::DiscoverySocket> socket;
    try {
        socket = net::DiscoverySocket::open(kReplyPort, kBusyHint);
    } catch (const Error& e) {
        // Bewusst fail-open (siehe discovery.hpp): ohne Zustand bleibt der Streifen am Ende an.
        error = e.what();
        return std::nullopt;
    }
    // Erst die Suche (Grund siehe discovery.hpp). Scheitert sie, wird trotzdem gefragt –
    // mit Firewall-Regel für die EXE kommt die Antwort auch so an.
    std::string ignored;
    socket->send_multicast(std::string(kMulticastGroup), kScanPort, scan_message(), ignored);
    const std::string request = status_message();
    std::optional<DeviceStatus> status;
    // Zwei Versuche: Gemessen 2026-10-18 antwortete der H612C in 40 von 40 Abfragen nach
    // 53–217 ms, einmal davor im Spielstart aber gar nicht (nicht nachstellbar). Ein zweites
    // devStatus kostet nichts und fängt so einen Aussetzer ab.
    for (int attempt = 0; attempt < 2 && !status; ++attempt) {
        if (!socket->send_to(ip, kCommandPort, request, error)) {
            error = "Zustandsabfrage lässt sich nicht senden (" + error + ")";
            return std::nullopt;
        }
        const auto problem = socket->receive_until(std::chrono::steady_clock::now() + wait,
                                                   [&status, &canonical](std::string_view datagram,
                                                                         const std::string& from) {
                                                       // Nur die Antwort genau dieses Geräts zählt.
                                                       if (from != *canonical) {
                                                           return false;
                                                       }
                                                       status = parse_status_reply(datagram);
                                                       return status.has_value();
                                                   });
        if (problem) {
            error = *problem;
            return std::nullopt;
        }
    }
    if (!status) {
        error = "keine Antwort auf die Zustandsabfrage";
    }
    return status;
}

}  // namespace ldu::govee
