#include "net/udp.hpp"

#include "core/error.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace ldu::net {
namespace {

std::string wsa_text(int code) {
    return "WSA-Fehler " + std::to_string(code);
}

std::string to_text(const in_addr& addr) {
    char buf[INET_ADDRSTRLEN] = {};
    if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == nullptr) {
        return {};
    }
    return buf;
}

struct Interface final {
    in_addr address{};
    in_addr broadcast{};
};

// Alle IPv4-Adressen aktiver Schnittstellen außer Loopback, samt gerichteter
// Broadcast-Adresse. Leer, wenn Windows die Liste nicht liefert – dann senden die Suchen
// über die Standardschnittstelle.
std::vector<Interface> ipv4_interfaces() {
    ULONG size = 16 * 1024;
    // uint64_t-Puffer: IP_ADAPTER_ADDRESSES verlangt 8-Byte-Ausrichtung.
    std::vector<std::uint64_t> buffer;
    ULONG rc = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && rc == ERROR_BUFFER_OVERFLOW; ++attempt) {
        buffer.assign((size + 7) / 8, 0);
        rc = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                  nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
    }
    std::vector<Interface> out;
    if (rc != NO_ERROR) {
        return out;
    }
    for (auto* a = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); a != nullptr; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        for (auto* u = a->FirstUnicastAddress; u != nullptr; u = u->Next) {
            if (u->Address.lpSockaddr == nullptr || u->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }
            Interface i;
            i.address = reinterpret_cast<const sockaddr_in*>(u->Address.lpSockaddr)->sin_addr;
            const unsigned prefix = std::min<unsigned>(u->OnLinkPrefixLength, 32);
            const std::uint32_t mask = prefix == 0 ? 0 : 0xFFFFFFFFu << (32 - prefix);
            i.broadcast.s_addr = htonl(ntohl(i.address.s_addr) | ~mask);
            out.push_back(i);
        }
    }
    return out;
}

sockaddr_in endpoint(const in_addr& addr, std::uint16_t port) {
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(port);
    to.sin_addr = addr;
    return to;
}

bool parse_ipv4(const std::string& text, in_addr& out) {
    return inet_pton(AF_INET, text.c_str(), &out) == 1;
}

bool send_datagram(SOCKET s, const sockaddr_in& to, std::string_view payload) {
    if (payload.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    return sendto(s, payload.data(), static_cast<int>(payload.size()), 0, reinterpret_cast<const sockaddr*>(&to),
                  static_cast<int>(sizeof(to))) != SOCKET_ERROR;
}

SOCKET open_udp_socket() {
    const SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        throw Error(Error::Kind::network, "UDP-Socket lässt sich nicht öffnen (" + wsa_text(WSAGetLastError()) + ").");
    }
    const BOOL on = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), static_cast<int>(sizeof(on)));
    return s;
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
    std::uint16_t port = 0;

    ~Impl() {
        if (socket != INVALID_SOCKET) {
            closesocket(socket);
        }
    }
};

UdpSender::UdpSender(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

UdpSender::~UdpSender() = default;

std::unique_ptr<UdpSender> UdpSender::open(const std::string& ip, std::uint16_t port) {
    auto impl = std::make_unique<Impl>();
    impl->ip = ip;
    impl->port = port;
    in_addr addr{};
    if (!parse_ipv4(ip, addr)) {
        throw Error(Error::Kind::network, "„" + ip + "“ ist keine IPv4-Adresse.");
    }
    impl->target = endpoint(addr, port);
    impl->socket = open_udp_socket();
    return std::unique_ptr<UdpSender>(new UdpSender(std::move(impl)));
}

bool UdpSender::send(std::string_view payload, std::string& error) {
    if (payload.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = "Nachricht zu groß für ein Datagramm";
        return false;
    }
    if (!send_datagram(impl_->socket, impl_->target, payload)) {
        error = "Senden an " + impl_->ip + " gescheitert (" + wsa_text(WSAGetLastError()) + ")";
        return false;
    }
    return true;
}

const std::string& UdpSender::ip() const noexcept {
    return impl_->ip;
}

std::uint16_t UdpSender::port() const noexcept {
    return impl_->port;
}

bool is_ipv4(const std::string& text) {
    in_addr addr{};
    return parse_ipv4(text, addr);
}

std::optional<std::string> canonical_ipv4(const std::string& text) {
    in_addr addr{};
    if (!parse_ipv4(text, addr)) {
        return std::nullopt;
    }
    return to_text(addr);
}

struct DiscoverySocket::Impl final {
    SOCKET socket = INVALID_SOCKET;

    ~Impl() {
        if (socket != INVALID_SOCKET) {
            closesocket(socket);
        }
    }
};

DiscoverySocket::DiscoverySocket(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

DiscoverySocket::~DiscoverySocket() = default;

std::unique_ptr<DiscoverySocket> DiscoverySocket::open(std::uint16_t bind_port, std::string_view busy_hint) {
    auto impl = std::make_unique<Impl>();
    impl->socket = open_udp_socket();
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(bind_port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(impl->socket, reinterpret_cast<const sockaddr*>(&local), static_cast<int>(sizeof(local))) ==
        SOCKET_ERROR) {
        const int code = WSAGetLastError();
        if (code == WSAEADDRINUSE || code == WSAEACCES) {
            throw Error(Error::Kind::network, std::string(busy_hint));
        }
        throw Error(Error::Kind::network,
                    "UDP-Port " + std::to_string(bind_port) + " lässt sich nicht öffnen (" + wsa_text(code) + ").");
    }
    return std::unique_ptr<DiscoverySocket>(new DiscoverySocket(std::move(impl)));
}

int DiscoverySocket::send_multicast(const std::string& group, std::uint16_t port, std::string_view payload,
                                    std::string& error) {
    in_addr group_addr{};
    if (!parse_ipv4(group, group_addr)) {
        error = "„" + group + "“ ist keine IPv4-Adresse.";
        return 0;
    }
    const sockaddr_in to = endpoint(group_addr, port);
    const SOCKET s = impl_->socket;
    int sent = 0;
    for (const Interface& i : ipv4_interfaces()) {
        // Schnittstelle ohne Multicast (etwa ein getrenntes VPN): überspringen, die
        // übrigen reichen. Scheitern alle, folgt unten die Standardschnittstelle.
        if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&i.address),
                       static_cast<int>(sizeof(i.address))) == SOCKET_ERROR) {
            continue;
        }
        if (send_datagram(s, to, payload)) {
            ++sent;
        }
    }
    if (sent == 0) {
        in_addr any{};
        any.s_addr = htonl(INADDR_ANY);
        setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&any), static_cast<int>(sizeof(any)));
        if (send_datagram(s, to, payload)) {
            ++sent;
        } else {
            error = "Suchanfrage lässt sich nicht senden (" + wsa_text(WSAGetLastError()) + ").";
        }
    }
    return sent;
}

int DiscoverySocket::send_broadcast(std::uint16_t port, std::string_view payload, std::string& error) {
    const SOCKET s = impl_->socket;
    int sent = 0;
    std::vector<std::uint32_t> done;
    for (const Interface& i : ipv4_interfaces()) {
        if (std::find(done.begin(), done.end(), i.broadcast.s_addr) != done.end()) {
            continue;
        }
        done.push_back(i.broadcast.s_addr);
        if (send_datagram(s, endpoint(i.broadcast, port), payload)) {
            ++sent;
        }
    }
    in_addr all{};
    all.s_addr = htonl(INADDR_BROADCAST);
    if (send_datagram(s, endpoint(all, port), payload)) {
        ++sent;
    }
    if (sent == 0) {
        error = "Broadcast lässt sich nicht senden (" + wsa_text(WSAGetLastError()) + ").";
    }
    return sent;
}

bool DiscoverySocket::send_to(const std::string& ip, std::uint16_t port, std::string_view payload,
                              std::string& error) {
    in_addr addr{};
    if (!parse_ipv4(ip, addr)) {
        error = "„" + ip + "“ ist keine IPv4-Adresse.";
        return false;
    }
    if (!send_datagram(impl_->socket, endpoint(addr, port), payload)) {
        error = "Senden an " + ip + " gescheitert (" + wsa_text(WSAGetLastError()) + ").";
        return false;
    }
    return true;
}

std::uint16_t DiscoverySocket::local_port() const {
    sockaddr_in local{};
    int len = static_cast<int>(sizeof(local));
    if (getsockname(impl_->socket, reinterpret_cast<sockaddr*>(&local), &len) == SOCKET_ERROR) {
        return 0;
    }
    return ntohs(local.sin_port);
}

std::optional<std::string> DiscoverySocket::receive_until(std::chrono::steady_clock::time_point deadline,
                                                          const Handler& handler) {
    // 4 KiB: Govee- und Art-Net-Antworten sind unter 300 Byte, mDNS-Antworten passen laut
    // RFC 6762 §17 ohne Fragmentierung in die übliche MTU von 1500.
    char buf[4096];
    for (;;) {
        const auto left =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0) {
            return std::nullopt;
        }
        WSAPOLLFD pfd{};
        pfd.fd = impl_->socket;
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
        const int n = recvfrom(impl_->socket, buf, static_cast<int>(sizeof(buf)), 0,
                               reinterpret_cast<sockaddr*>(&from), &from_len);
        if (n == SOCKET_ERROR) {
            const int code = WSAGetLastError();
            // WSAECONNRESET meldet bei UDP nur ein ICMP „Port unerreichbar“ auf eine
            // frühere Sendung, WSAEMSGSIZE ein zu großes fremdes Datagramm – beides
            // betrifft keine gesuchte Antwort, also weiterlesen.
            if (code == WSAECONNRESET || code == WSAEMSGSIZE) {
                continue;
            }
            return "Antwort lässt sich nicht lesen (" + wsa_text(code) + ").";
        }
        if (handler(std::string_view(buf, static_cast<std::size_t>(n)), to_text(from.sin_addr))) {
            return std::nullopt;
        }
    }
}

}  // namespace ldu::net
