#include "govee/udp.hpp"

#include "core/error.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace ldu::govee {
namespace {

std::string wsa_text(int code) {
    return "WSA-Fehler " + std::to_string(code);
}

class SocketGuard final {
public:
    explicit SocketGuard(SOCKET s) : s_(s) {}
    ~SocketGuard() {
        if (s_ != INVALID_SOCKET) {
            closesocket(s_);
        }
    }
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;

    SOCKET get() const noexcept {
        return s_;
    }

private:
    SOCKET s_;
};

std::string to_text(const in_addr& addr) {
    char buf[INET_ADDRSTRLEN] = {};
    if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == nullptr) {
        return {};
    }
    return buf;
}

// Alle IPv4-Adressen aktiver Schnittstellen außer Loopback. Leer, wenn Windows die
// Liste nicht liefert – dann sendet scan_devices über die Standardschnittstelle.
std::vector<in_addr> ipv4_interfaces() {
    ULONG size = 16 * 1024;
    // uint64_t-Puffer: IP_ADAPTER_ADDRESSES verlangt 8-Byte-Ausrichtung.
    std::vector<std::uint64_t> buffer;
    ULONG rc = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && rc == ERROR_BUFFER_OVERFLOW; ++attempt) {
        buffer.assign((size + 7) / 8, 0);
        rc = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                  nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
    }
    std::vector<in_addr> out;
    if (rc != NO_ERROR) {
        return out;
    }
    for (auto* a = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); a != nullptr; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        for (auto* u = a->FirstUnicastAddress; u != nullptr; u = u->Next) {
            if (u->Address.lpSockaddr != nullptr && u->Address.lpSockaddr->sa_family == AF_INET) {
                out.push_back(reinterpret_cast<const sockaddr_in*>(u->Address.lpSockaddr)->sin_addr);
            }
        }
    }
    return out;
}

// Bindet s an UDP 4002, wohin die Geräte antworten. Liefert den Grund, wenn es nicht geht.
std::optional<std::string> bind_reply_port(SOCKET s) {
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(kReplyPort);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, reinterpret_cast<const sockaddr*>(&local), static_cast<int>(sizeof(local))) != SOCKET_ERROR) {
        return std::nullopt;
    }
    const int code = WSAGetLastError();
    if (code == WSAEADDRINUSE || code == WSAEACCES) {
        return "UDP-Port 4002 ist belegt – meist läuft Govee Desktop oder ein Smart-Home-Dienst. "
               "Den beenden oder den Streifen direkt mit --ip angeben.";
    }
    return "UDP-Port 4002 lässt sich nicht öffnen (" + wsa_text(code) + ").";
}

// Liest Datagramme bis `deadline` oder bis on_datagram true liefert. Liefert einen Grund,
// wenn Winsock versagt; Fremdes und zu Großes wird übersprungen.
template <typename Handler>
std::optional<std::string> receive_until(SOCKET s, std::chrono::steady_clock::time_point deadline,
                                         Handler&& on_datagram) {
    char buf[2048];
    for (;;) {
        const auto left =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0) {
            return std::nullopt;
        }
        WSAPOLLFD pfd{};
        pfd.fd = s;
        pfd.events = POLLRDNORM;
        const int ready = WSAPoll(&pfd, 1, static_cast<INT>(std::min<long long>(left.count(), 1000)));
        if (ready == SOCKET_ERROR) {
            return "Warten auf Antworten gescheitert (" + wsa_text(WSAGetLastError()) + ").";
        }
        if (ready == 0) {
            continue;
        }
        sockaddr_in from{};
        int from_len = static_cast<int>(sizeof(from));
        const int n =
            recvfrom(s, buf, static_cast<int>(sizeof(buf)), 0, reinterpret_cast<sockaddr*>(&from), &from_len);
        if (n == SOCKET_ERROR) {
            const int code = WSAGetLastError();
            // WSAECONNRESET meldet bei UDP nur ein ICMP „Port unerreichbar“ auf eine
            // frühere Sendung, WSAEMSGSIZE ein zu großes fremdes Datagramm – beides
            // betrifft keine Govee-Antwort, also weiterlesen.
            if (code == WSAECONNRESET || code == WSAEMSGSIZE) {
                continue;
            }
            return "Antwort lässt sich nicht lesen (" + wsa_text(code) + ").";
        }
        if (on_datagram(std::string_view(buf, static_cast<std::size_t>(n)), from)) {
            return std::nullopt;
        }
    }
}

}  // namespace

NetworkSession::NetworkSession() {
    WSADATA data{};
    const int rc = WSAStartup(MAKEWORD(2, 2), &data);
    if (rc != 0) {
        throw Error(Error::Kind::network, "Winsock startet nicht (" + wsa_text(rc) + ").");
    }
}

NetworkSession::~NetworkSession() {
    WSACleanup();
}

struct UdpSender::Impl final {
    SOCKET socket = INVALID_SOCKET;
    sockaddr_in target{};
    std::string ip;

    ~Impl() {
        if (socket != INVALID_SOCKET) {
            closesocket(socket);
        }
    }
};

UdpSender::UdpSender(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

UdpSender::~UdpSender() = default;

std::unique_ptr<UdpSender> UdpSender::open(const std::string& ip) {
    auto impl = std::make_unique<Impl>();
    impl->ip = ip;
    impl->target.sin_family = AF_INET;
    impl->target.sin_port = htons(kCommandPort);
    if (inet_pton(AF_INET, ip.c_str(), &impl->target.sin_addr) != 1) {
        throw Error(Error::Kind::network, "„" + ip + "“ ist keine IPv4-Adresse.");
    }
    impl->socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (impl->socket == INVALID_SOCKET) {
        throw Error(Error::Kind::network, "UDP-Socket lässt sich nicht öffnen (" + wsa_text(WSAGetLastError()) + ").");
    }
    return std::unique_ptr<UdpSender>(new UdpSender(std::move(impl)));
}

bool UdpSender::send(std::string_view payload, std::string& error) {
    if (payload.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = "Nachricht zu groß für ein Datagramm";
        return false;
    }
    const int n = sendto(impl_->socket, payload.data(), static_cast<int>(payload.size()), 0,
                         reinterpret_cast<const sockaddr*>(&impl_->target), static_cast<int>(sizeof(impl_->target)));
    if (n == SOCKET_ERROR) {
        error = "Senden an " + impl_->ip + " gescheitert (" + wsa_text(WSAGetLastError()) + ")";
        return false;
    }
    return true;
}

const std::string& UdpSender::ip() const noexcept {
    return impl_->ip;
}

bool is_ipv4(const std::string& text) {
    in_addr addr{};
    return inet_pton(AF_INET, text.c_str(), &addr) == 1;
}

std::vector<DeviceInfo> scan_devices(std::chrono::milliseconds wait) {
    SocketGuard s(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (s.get() == INVALID_SOCKET) {
        throw Error(Error::Kind::network, "UDP-Socket lässt sich nicht öffnen (" + wsa_text(WSAGetLastError()) + ").");
    }
    if (const auto problem = bind_reply_port(s.get())) {
        throw Error(Error::Kind::network, *problem);
    }

    sockaddr_in group{};
    group.sin_family = AF_INET;
    group.sin_port = htons(kScanPort);
    const std::string group_text(kMulticastGroup);
    inet_pton(AF_INET, group_text.c_str(), &group.sin_addr);
    const std::string request = scan_message();
    auto send_request = [&]() {
        return sendto(s.get(), request.data(), static_cast<int>(request.size()), 0,
                      reinterpret_cast<const sockaddr*>(&group), static_cast<int>(sizeof(group))) != SOCKET_ERROR;
    };

    int sent = 0;
    for (const in_addr& addr : ipv4_interfaces()) {
        // Schnittstelle ohne Multicast (etwa ein getrenntes VPN): überspringen, die
        // übrigen reichen. Scheitern alle, folgt unten die Standardschnittstelle.
        if (setsockopt(s.get(), IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&addr),
                       static_cast<int>(sizeof(addr))) == SOCKET_ERROR) {
            continue;
        }
        if (send_request()) {
            ++sent;
        }
    }
    if (sent == 0) {
        in_addr any{};
        any.s_addr = htonl(INADDR_ANY);
        setsockopt(s.get(), IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&any),
                   static_cast<int>(sizeof(any)));
        if (!send_request()) {
            throw Error(Error::Kind::network,
                        "Suchanfrage lässt sich nicht senden (" + wsa_text(WSAGetLastError()) + ").");
        }
    }

    std::vector<DeviceInfo> found;
    const auto problem = receive_until(s.get(), std::chrono::steady_clock::now() + wait,
                                       [&found](std::string_view datagram, const sockaddr_in& from) {
                                           auto info = parse_scan_reply(datagram);
                                           if (!info) {
                                               return false;
                                           }
                                           // Die IP im JSON ist die, die das Gerät selbst kennt; ist
                                           // sie unbrauchbar, gilt der Absender des Datagramms.
                                           if (!is_ipv4(info->ip)) {
                                               info->ip = to_text(from.sin_addr);
                                           }
                                           const bool known =
                                               std::any_of(found.begin(), found.end(), [&info](const DeviceInfo& d) {
                                                   return d.ip == info->ip;
                                               });
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
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(kCommandPort);
    if (inet_pton(AF_INET, ip.c_str(), &target.sin_addr) != 1) {
        error = "„" + ip + "“ ist keine IPv4-Adresse.";
        return std::nullopt;
    }
    SocketGuard s(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (s.get() == INVALID_SOCKET) {
        error = "UDP-Socket lässt sich nicht öffnen (" + wsa_text(WSAGetLastError()) + ").";
        return std::nullopt;
    }
    if (auto problem = bind_reply_port(s.get())) {
        error = std::move(*problem);
        return std::nullopt;
    }
    // Erst die Suche (Grund siehe udp.hpp). Scheitert sie, wird trotzdem gefragt – mit
    // Firewall-Regel für die EXE kommt die Antwort auch so an.
    sockaddr_in group{};
    group.sin_family = AF_INET;
    group.sin_port = htons(kScanPort);
    const std::string group_text(kMulticastGroup);
    inet_pton(AF_INET, group_text.c_str(), &group.sin_addr);
    const std::string scan = scan_message();
    sendto(s.get(), scan.data(), static_cast<int>(scan.size()), 0, reinterpret_cast<const sockaddr*>(&group),
           static_cast<int>(sizeof(group)));
    const std::string request = status_message();
    std::optional<DeviceStatus> status;
    // Zwei Versuche: Gemessen 2026-10-18 antwortete der H612C in 40 von 40 Abfragen nach
    // 53–217 ms, einmal davor im Spielstart aber gar nicht (nicht nachstellbar). Ein zweites
    // devStatus kostet nichts und fängt so einen Aussetzer ab.
    for (int attempt = 0; attempt < 2 && !status; ++attempt) {
        if (sendto(s.get(), request.data(), static_cast<int>(request.size()), 0,
                   reinterpret_cast<const sockaddr*>(&target), static_cast<int>(sizeof(target))) == SOCKET_ERROR) {
            error = "Zustandsabfrage lässt sich nicht senden (" + wsa_text(WSAGetLastError()) + ").";
            return std::nullopt;
        }
        const auto problem = receive_until(s.get(), std::chrono::steady_clock::now() + wait,
                                           [&status, &target](std::string_view datagram, const sockaddr_in& from) {
                                               // Nur die Antwort genau dieses Geräts zählt.
                                               if (from.sin_addr.s_addr != target.sin_addr.s_addr) {
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
