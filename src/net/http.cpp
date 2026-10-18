#include "net/http.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <charconv>

namespace ldu::net {
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

int left_ms(std::chrono::steady_clock::time_point deadline) {
    const auto left =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    return static_cast<int>(std::clamp<long long>(left, 0, 60'000));
}

// Wartet auf events; false bei Fristablauf oder Fehler (Grund in error).
bool wait_for(SOCKET s, SHORT events, std::chrono::steady_clock::time_point deadline, std::string& error) {
    for (;;) {
        const int ms = left_ms(deadline);
        if (ms <= 0) {
            error = "keine Antwort in der Frist";
            return false;
        }
        WSAPOLLFD pfd{};
        pfd.fd = s;
        pfd.events = events;
        const int ready = WSAPoll(&pfd, 1, ms);
        if (ready == SOCKET_ERROR) {
            error = "Warten gescheitert (" + wsa_text(WSAGetLastError()) + ")";
            return false;
        }
        if (ready > 0) {
            // POLLHUP/POLLERR zählen als bereit: recv bzw. SO_ERROR liefern dann den Grund.
            return true;
        }
    }
}

}  // namespace

std::optional<HttpResponse> parse_http_response(std::string_view raw) {
    const std::size_t line_end = raw.find("\r\n");
    if (line_end == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view status_line = raw.substr(0, line_end);
    if (status_line.substr(0, 7) != "HTTP/1." || status_line.size() < 12 || status_line[8] != ' ') {
        return std::nullopt;
    }
    int status = 0;
    const char* first = status_line.data() + 9;
    const char* last = status_line.data() + 12;
    const auto [end, ec] = std::from_chars(first, last, status);
    if (ec != std::errc{} || end != last || status < 100 || status > 599) {
        return std::nullopt;
    }
    const std::size_t head_end = raw.find("\r\n\r\n");
    if (head_end == std::string_view::npos) {
        return std::nullopt;
    }
    HttpResponse r;
    r.status = status;
    r.body = std::string(raw.substr(head_end + 4));
    return r;
}

std::optional<HttpResponse> http_get(const std::string& ip, std::uint16_t port, const std::string& path,
                                     std::chrono::milliseconds timeout, std::string& error, std::size_t max_bytes) {
    // Pfad kommt aus dem Programm, nicht vom Nutzer; die Prüfung hält trotzdem jede
    // Kopfzeilen-Einschleusung fern (CWE-113).
    if (path.empty() || path.front() != '/' ||
        path.find_first_of("\r\n \t") != std::string::npos) {
        error = "ungültiger Pfad";
        return std::nullopt;
    }
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &to.sin_addr) != 1) {
        error = "„" + ip + "“ ist keine IPv4-Adresse";
        return std::nullopt;
    }
    SocketGuard s(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (s.get() == INVALID_SOCKET) {
        error = "TCP-Socket lässt sich nicht öffnen (" + wsa_text(WSAGetLastError()) + ")";
        return std::nullopt;
    }
    u_long non_blocking = 1;
    if (ioctlsocket(s.get(), FIONBIO, &non_blocking) == SOCKET_ERROR) {
        error = "Socket lässt sich nicht umstellen (" + wsa_text(WSAGetLastError()) + ")";
        return std::nullopt;
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    if (connect(s.get(), reinterpret_cast<const sockaddr*>(&to), static_cast<int>(sizeof(to))) == SOCKET_ERROR) {
        const int code = WSAGetLastError();
        if (code != WSAEWOULDBLOCK) {
            error = "Verbindung zu " + ip + " scheitert (" + wsa_text(code) + ")";
            return std::nullopt;
        }
        if (!wait_for(s.get(), POLLWRNORM, deadline, error)) {
            error = "Verbindung zu " + ip + ": " + error;
            return std::nullopt;
        }
        int so_error = 0;
        int len = static_cast<int>(sizeof(so_error));
        getsockopt(s.get(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len);
        if (so_error != 0) {
            error = "Verbindung zu " + ip + " scheitert (" + wsa_text(so_error) + ")";
            return std::nullopt;
        }
    }
    const std::string request =
        "GET " + path + " HTTP/1.0\r\nHost: " + ip + "\r\nConnection: close\r\nAccept: application/json\r\n\r\n";
    std::size_t sent = 0;
    while (sent < request.size()) {
        if (!wait_for(s.get(), POLLWRNORM, deadline, error)) {
            return std::nullopt;
        }
        const int n = ::send(s.get(), request.data() + sent, static_cast<int>(request.size() - sent), 0);
        if (n == SOCKET_ERROR) {
            const int code = WSAGetLastError();
            if (code == WSAEWOULDBLOCK) {
                continue;
            }
            error = "Anfrage lässt sich nicht senden (" + wsa_text(code) + ")";
            return std::nullopt;
        }
        sent += static_cast<std::size_t>(n);
    }
    std::string raw;
    char buf[4096];
    for (;;) {
        if (!wait_for(s.get(), POLLRDNORM, deadline, error)) {
            return std::nullopt;
        }
        const int n = recv(s.get(), buf, static_cast<int>(sizeof(buf)), 0);
        if (n == 0) {
            break;  // Server hat geschlossen: Antwort vollständig (HTTP/1.0)
        }
        if (n == SOCKET_ERROR) {
            const int code = WSAGetLastError();
            if (code == WSAEWOULDBLOCK) {
                continue;
            }
            error = "Antwort lässt sich nicht lesen (" + wsa_text(code) + ")";
            return std::nullopt;
        }
        raw.append(buf, static_cast<std::size_t>(n));
        if (raw.size() > max_bytes) {
            error = "Antwort größer als " + std::to_string(max_bytes / 1024) + " KiB";
            return std::nullopt;
        }
    }
    auto response = parse_http_response(raw);
    if (!response) {
        error = "keine gültige HTTP-Antwort";
    }
    return response;
}

}  // namespace ldu::net
