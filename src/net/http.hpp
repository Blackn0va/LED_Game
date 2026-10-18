// Minimaler HTTP-GET im LAN – nur für WLED /json/info (Name und LED-Zahl).
//
// Bewusst HTTP/1.0 mit „Connection: close“: Der Server darf dann kein chunked senden und
// schließt nach der Antwort, das Ende ist also eindeutig. Kein HTTPS, keine Umleitungen,
// keine Cookies – WLED spricht im LAN nur einfaches HTTP, und mehr braucht die Abfrage
// nicht. Die Antwort ist auf max_bytes begrenzt und gilt als nicht vertrauenswürdig.
//
// Threads: frei (jeder Aufruf hat seinen eigenen Socket).
// Fehler: http_get wirft nicht – nullopt und Grund (fail-open: ohne Antwort fragt das
// Programm nach der LED-Zahl bzw. nimmt die Einstellungen).

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ldu::net {

inline constexpr std::size_t kHttpMaxBytes = 256 * 1024;

struct HttpResponse final {
    int status = 0;
    std::string body;
};

// Zerlegt eine vollständige Antwort (Statuszeile, Kopf, Leerzeile, Rumpf). nullopt, wenn
// die Statuszeile nicht „HTTP/1.x <Zahl>“ ist oder die Leerzeile fehlt.
std::optional<HttpResponse> parse_http_response(std::string_view raw);

// GET http://ip:port/path. timeout gilt für Verbinden und Lesen zusammen.
std::optional<HttpResponse> http_get(const std::string& ip, std::uint16_t port, const std::string& path,
                                     std::chrono::milliseconds timeout, std::string& error,
                                     std::size_t max_bytes = kHttpMaxBytes);

}  // namespace ldu::net
