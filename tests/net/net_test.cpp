// Netzmodul: mDNS-Pakete, HTTP-Antworten, und echte Sockets über Loopback (127.0.0.1).
//
// Die mDNS-Bytes sind von Hand nach RFC 1035 §4.1 gebaut (Kopf 12 Byte, Namen als
// Längen-Labels, Zeiger 0xC0xx), nicht mit dem Code unter Test.

#include "core/error.hpp"
#include "net/http.hpp"
#include "net/mdns.hpp"
#include "net/udp.hpp"
#include "support/udp_listener.hpp"

#include <doctest.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <ostream>
#include <string>
#include <thread>

namespace ldu::net {
namespace {

using namespace std::chrono_literals;

std::string bytes(std::initializer_list<int> list) {
    std::string s;
    for (const int b : list) {
        s.push_back(static_cast<char>(b));
    }
    return s;
}

std::string label(std::string_view text) {
    return std::string(1, static_cast<char>(text.size())) + std::string(text);
}

// Antwort mit PTR _wled._tcp.local → wled-a1b2c3._wled._tcp.local (komprimiert) und einem
// A-Datensatz im Zusatzteil, der übersprungen werden muss.
std::string wled_response() {
    std::string p = bytes({0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01});
    p += label("_wled") + label("_tcp") + label("local") + std::string(1, '\0');  // @12
    p += bytes({0x00, 0x0C, 0x00, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x0E});      // PTR IN TTL 120, 14 Byte
    const std::size_t instance_at = p.size();
    p += label("wled-a1b2c3") + bytes({0xC0, 0x0C});
    p += bytes({0xC0, static_cast<int>(instance_at)});                            // Name → Instanz
    p += bytes({0x00, 0x01, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x04, 192, 0, 2, 33});
    return p;
}

// Wie wled_response, aber Antwortname und PTR-Ziel erreichen „_wled._tcp.local“ erst über
// eine Kette aus `chain` Zeigern im Rumpf eines TXT-Datensatzes (1 + chain Sprünge je Name).
std::string chained_response(int chain) {
    std::string p = bytes({0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01});
    const int chain_at = 51;
    p += bytes({0xC0, chain_at});                                                    // @12 Name → Kette
    p += bytes({0x00, 0x0C, 0x00, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x0E});        // @14 PTR, 14 Byte
    p += label("wled-a1b2c3") + bytes({0xC0, chain_at});                            // @24 Ziel → Kette
    p += label("x") + std::string(1, '\0');                                          // @38 Zusatzname
    const int rdlength = 2 * chain + 18;
    p += bytes({0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, rdlength});    // @41 TXT
    for (int i = 0; i < chain; ++i) {
        p += bytes({0xC0, chain_at + 2 * (i + 1)});                                  // @51 Kette
    }
    p += label("_wled") + label("_tcp") + label("local") + std::string(1, '\0');
    return p;
}

struct Session final {
    NetworkSession net;
};

}  // namespace

TEST_CASE("mDNS: Anfrage nach _wled._tcp.local, PTR mit QU-Bit – Byte für Byte") {
    const std::string expected = bytes({0x4C, 0x44, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}) + label("_wled") + label("_tcp") +
                                 label("local") + bytes({0, 0x00, 0x0C, 0x80, 0x01});
    CHECK(mdns_query("_wled._tcp.local") == expected);
}

TEST_CASE("mDNS: Antwort mit komprimiertem Instanznamen liefert wled-a1b2c3") {
    const auto found = parse_mdns_ptr(wled_response(), "_wled._tcp.local");
    REQUIRE(found);
    CHECK(*found == std::vector<std::string>{"wled-a1b2c3"});
    // Groß/klein zählt bei DNS-Namen nicht.
    CHECK(parse_mdns_ptr(wled_response(), "_WLED._tcp.local")->size() == 1);
    // Anderer Dienst: gültige Antwort, nichts gefunden.
    CHECK(parse_mdns_ptr(wled_response(), "_http._tcp.local")->empty());
}

TEST_CASE("mDNS: eigene Anfrage, abgeschnittenes Paket und Zeiger-Kreis werden verworfen") {
    CHECK_FALSE(parse_mdns_ptr(mdns_query("_wled._tcp.local"), "_wled._tcp.local"));  // QR-Bit fehlt
    const std::string full = wled_response();
    for (std::size_t cut = 0; cut < full.size(); ++cut) {
        const auto r = parse_mdns_ptr(full.substr(0, cut), "_wled._tcp.local");
        CHECK_FALSE((r && !r->empty()));  // nie ein Fund aus einem halben Paket
    }
    // Antwortname zeigt auf sich selbst: ohne Sprunggrenze hinge read_name für immer.
    std::string loop = bytes({0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00});
    loop += bytes({0xC0, 0x0C, 0x00, 0x0C, 0x00, 0x01, 0, 0, 0, 0, 0x00, 0x00});
    CHECK_FALSE(parse_mdns_ptr(loop, "_wled._tcp.local"));
}

TEST_CASE("mDNS: kurze Zeigerkette geht, eine Kette über die Sprunggrenze (16) wird verworfen") {
    const auto short_chain = parse_mdns_ptr(chained_response(5), "_wled._tcp.local");  // 6 Sprünge
    REQUIRE(short_chain);
    CHECK(*short_chain == std::vector<std::string>{"wled-a1b2c3"});
    CHECK_FALSE(parse_mdns_ptr(chained_response(20), "_wled._tcp.local"));            // 21 Sprünge
}

TEST_CASE("HTTP: Statuszeile und Rumpf werden getrennt, Fremdes abgelehnt") {
    const auto r = parse_http_response("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"a\":1}");
    REQUIRE(r);
    CHECK(r->status == 200);
    CHECK(r->body == "{\"a\":1}");
    CHECK(parse_http_response("HTTP/1.0 404 Not Found\r\n\r\n")->status == 404);
    CHECK_FALSE(parse_http_response("HTTP/2 200\r\n\r\n"));
    CHECK_FALSE(parse_http_response("HTTP/1.1 200 OK\r\nKein Ende"));
    CHECK_FALSE(parse_http_response("SSH-2.0-OpenSSH\r\n\r\n"));
    CHECK_FALSE(parse_http_response("HTTP/1.1 2x0 OK\r\n\r\n"));
}

TEST_CASE("HTTP über Loopback: GET /json/info mit HTTP/1.0, Antwort kommt vollständig an") {
    const Session session;
    const SOCKET server = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    REQUIRE(server != INVALID_SOCKET);
    sockaddr_in local{};
    local.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);
    REQUIRE(bind(server, reinterpret_cast<const sockaddr*>(&local), static_cast<int>(sizeof(local))) == 0);
    REQUIRE(listen(server, 1) == 0);
    int len = static_cast<int>(sizeof(local));
    getsockname(server, reinterpret_cast<sockaddr*>(&local), &len);
    const std::uint16_t port = ntohs(local.sin_port);
    std::string request;
    std::thread peer([server, &request] {
        const SOCKET c = accept(server, nullptr, nullptr);
        char buf[1024];
        while (request.find("\r\n\r\n") == std::string::npos) {
            const int n = recv(c, buf, static_cast<int>(sizeof(buf)), 0);
            if (n <= 0) {
                break;
            }
            request.append(buf, static_cast<std::size_t>(n));
        }
        const std::string body(5000, 'x');  // mehrere recv-Runden beim Lesen
        const std::string reply = "HTTP/1.1 200 OK\r\nContent-Length: 5000\r\n\r\n" + body;
        send(c, reply.data(), static_cast<int>(reply.size()), 0);
        closesocket(c);
    });
    std::string error;
    const auto r = http_get("127.0.0.1", port, "/json/info", 2000ms, error);
    peer.join();
    closesocket(server);
    REQUIRE(r);
    CHECK(r->status == 200);
    CHECK(r->body.size() == 5000);
    CHECK(request.rfind("GET /json/info HTTP/1.0\r\n", 0) == 0);
    CHECK(request.find("Connection: close\r\n") != std::string::npos);
}

TEST_CASE("HTTP über Loopback: Server schweigt → nullopt nach der Frist, kein Hänger") {
    const Session session;
    const SOCKET server = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    REQUIRE(server != INVALID_SOCKET);
    sockaddr_in local{};
    local.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);
    REQUIRE(bind(server, reinterpret_cast<const sockaddr*>(&local), static_cast<int>(sizeof(local))) == 0);
    REQUIRE(listen(server, 1) == 0);
    int len = static_cast<int>(sizeof(local));
    getsockname(server, reinterpret_cast<sockaddr*>(&local), &len);
    std::string error;
    const auto t0 = std::chrono::steady_clock::now();
    const auto r = http_get("127.0.0.1", ntohs(local.sin_port), "/json/info", 300ms, error);
    const auto took = std::chrono::steady_clock::now() - t0;
    closesocket(server);
    CHECK_FALSE(r);
    CHECK(error.find("Frist") != std::string::npos);
    CHECK(took < 2s);
}

TEST_CASE("HTTP: Pfad mit Zeilenumbruch wird nicht gesendet (CWE-113)") {
    const Session session;
    std::string error;
    CHECK_FALSE(http_get("127.0.0.1", 9, "/a\r\nX: y", 100ms, error));
    CHECK(error == "ungültiger Pfad");
}

TEST_CASE("UDP über Loopback: UdpSender liefert genau die Bytes, auch Nullbytes") {
    const Session session;
    test::UdpListener listener(0);
    auto sender = UdpSender::open("127.0.0.1", listener.port());
    const std::string payload = bytes({0x02, 0x00, 0xFF, 0x00, 0x10});
    std::string error;
    REQUIRE(sender->send(payload, error));
    CHECK(listener.receive() == payload);
    CHECK(sender->port() == listener.port());
}

TEST_CASE("UDP: Suchsocket empfängt mit Absender-IP, belegter Port nennt den Hinweis") {
    const Session session;
    auto socket = DiscoverySocket::open(0, "belegt");
    const std::uint16_t port = socket->local_port();
    REQUIRE(port != 0);
    test::UdpListener peer(0);
    REQUIRE(peer.send_to(port, "hallo"));
    std::string got;
    std::string from;
    const auto problem = socket->receive_until(std::chrono::steady_clock::now() + 1000ms,
                                               [&](std::string_view d, const std::string& ip) {
                                                   got = std::string(d);
                                                   from = ip;
                                                   return true;
                                               });
    CHECK_FALSE(problem);
    CHECK(got == "hallo");
    CHECK(from == "127.0.0.1");
    try {
        DiscoverySocket::open(port, "Port belegt – anderes Programm beenden");
        FAIL("zweites Binden hätte scheitern müssen");
    } catch (const Error& e) {
        CHECK(std::string(e.what()) == "Port belegt – anderes Programm beenden");
    }
}

TEST_CASE("UDP: IPv4-Prüfung und einheitliche Schreibweise") {
    const Session session;
    CHECK(is_ipv4("192.0.2.21"));
    CHECK_FALSE(is_ipv4("192.0.2"));
    CHECK_FALSE(is_ipv4("wled.local"));
    CHECK(canonical_ipv4("192.0.2.21") == "192.0.2.21");
    CHECK_FALSE(canonical_ipv4("nein"));
    CHECK_THROWS_AS(UdpSender::open("kein-host", 1), Error);
}

}  // namespace ldu::net
