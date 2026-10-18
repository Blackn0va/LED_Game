// UDP über Winsock: Befehle an ein Gerät und die Multicast-Suche.
//
// Die Suche geht über JEDE aktive IPv4-Schnittstelle einzeln: Ohne IP_MULTICAST_IF
// nimmt Windows die Schnittstelle der passenden Route, und mit Hyper-V-, WSL- oder
// VPN-Adaptern kann das die falsche sein – der Streifen hörte die Anfrage dann nie.
// Gesendet wird vom selben Socket, der auf 4002 lauscht, weil die Geräte dorthin
// antworten. Ob die Windows-Firewall die Antwort durchlässt, hängt an deren Regeln
// und ist hier nicht geprüft (README → Fehlersuche).
//
// Threads: ein Objekt gehört einem Thread.
// Fehler: Error(Kind::network) bei Winsock-Fehlern, ungültiger IP oder belegtem
// Port 4002. send() wirft nicht, sondern meldet false plus Grund.

#pragma once

#include "govee/protocol.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ldu::govee {

// Winsock für die Lebensdauer des Objekts (WSAStartup/WSACleanup).
class NetworkSession final {
public:
    NetworkSession();
    ~NetworkSession();
    NetworkSession(const NetworkSession&) = delete;
    NetworkSession& operator=(const NetworkSession&) = delete;
};

// Ziel für fertige Nachrichten. Eigene Schnittstelle, damit StripOutput ohne Netz testbar
// ist (Tests schreiben in eine Fake-Senke mit, statt UDP zu senden).
class PacketSink {
public:
    virtual ~PacketSink() = default;
    // false und Grund in error, wenn das Senden scheitert; wirft nicht.
    virtual bool send(std::string_view payload, std::string& error) = 0;
};

class UdpSender final : public PacketSink {
public:
    // Socket für Befehle an ip:4003. Wirft bei einer ungültigen IPv4-Adresse.
    static std::unique_ptr<UdpSender> open(const std::string& ip);
    ~UdpSender() override;
    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;

    // false und Grund in error, wenn sendto scheitert. Bewusst kein Wurf: Ein verlorenes
    // Bild ist bei UDP Alltag und kein Grund, das Spiel abzubrechen (fail-open, kein
    // Sicherheitspfad).
    bool send(std::string_view payload, std::string& error) override;
    const std::string& ip() const noexcept;

private:
    struct Impl;
    explicit UdpSender(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

// true, wenn text eine IPv4-Adresse in Punktschreibweise ist.
bool is_ipv4(const std::string& text);

// Sucht Govee-Geräte mit eingeschalteter LAN-Steuerung und wartet `wait` auf Antworten.
// Wirft, wenn Port 4002 belegt ist (etwa durch Govee Desktop) oder Winsock versagt.
std::vector<DeviceInfo> scan_devices(std::chrono::milliseconds wait);

// Fragt An/Aus und Helligkeit eines Geräts ab. nullopt und Grund in error, wenn es nicht
// antwortet oder Port 4002 belegt ist. Wirft bewusst nicht: Der Zustand entscheidet nur,
// ob der Streifen beim Beenden wieder ausgeht – ohne ihn bleibt er an (fail-open, kein
// Sicherheitspfad). Schickt vorher die Multicast-Suche vom selben Socket: Gemessen
// 2026-10-18 kam bei einer EXE ohne Firewall-Regel auf devStatus allein keine Antwort,
// mit vorheriger Suche schon. Vermutete Ursache: Windows lässt Antworten auf eigenen
// Multicast kurz durch – nicht getrennt geprüft.
std::optional<DeviceStatus> query_status(const std::string& ip, std::chrono::milliseconds wait, std::string& error);

}  // namespace ldu::govee
