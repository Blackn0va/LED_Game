// UDP über Winsock: Bilder an ein Gerät senden und Geräte im Netz suchen.
//
// Gemeinsam für alle Treiber (Govee, WLED, Art-Net, sACN). Vorher lag das in govee/udp
// mit festem Port 4003; jeder weitere Hersteller hätte dieselbe Socket-Arbeit kopiert.
//
// Suchen gehen über JEDE aktive IPv4-Schnittstelle einzeln: Ohne IP_MULTICAST_IF nimmt
// Windows die Schnittstelle der passenden Route, und mit Hyper-V-, WSL- oder VPN-Adaptern
// kann das die falsche sein – der Govee-Streifen hörte die Anfrage dann nie. Broadcasts
// gehen an die gerichtete Broadcast-Adresse jeder Schnittstelle, weil 255.255.255.255
// unter Windows nur über eine Schnittstelle hinausgeht (nicht gemessen, verbreitete
// Angabe – deshalb beides).
//
// Ob die Windows-Firewall Antworten durchlässt, hängt an deren Regeln und ist hier nicht
// geprüft (README → Fehlersuche).
//
// Threads: ein Objekt gehört einem Thread; NetworkSession einmal je Prozess.
// Fehler: Error(Kind::network) bei Winsock-Fehlern, ungültiger IP oder belegtem Port.
// send() wirft nicht, sondern meldet false plus Grund.

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace ldu::net {

// Winsock für die Lebensdauer des Objekts (WSAStartup/WSACleanup).
class NetworkSession final {
public:
    NetworkSession();
    ~NetworkSession();
    NetworkSession(const NetworkSession&) = delete;
    NetworkSession& operator=(const NetworkSession&) = delete;
};

// Ziel für fertige Datagramme. Eigene Schnittstelle, damit die Ausgabe ohne Netz testbar
// ist (Tests schreiben in eine Fake-Senke mit, statt UDP zu senden).
class PacketSink {
public:
    virtual ~PacketSink() = default;
    // false und Grund in error, wenn das Senden scheitert; wirft nicht.
    virtual bool send(std::string_view payload, std::string& error) = 0;
};

class UdpSender final : public PacketSink {
public:
    // Socket für Datagramme an ip:port. Unicast, Multicast (sACN) und Broadcast gehen alle;
    // SO_BROADCAST ist immer gesetzt, damit auch eine Art-Net-Broadcast-Adresse als --ip
    // funktioniert. Wirft bei einer ungültigen IPv4-Adresse.
    static std::unique_ptr<UdpSender> open(const std::string& ip, std::uint16_t port);
    ~UdpSender() override;
    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;

    // false und Grund in error, wenn sendto scheitert. Bewusst kein Wurf: Ein verlorenes
    // Bild ist bei UDP Alltag und kein Grund, das Spiel abzubrechen (fail-open, kein
    // Sicherheitspfad).
    bool send(std::string_view payload, std::string& error) override;
    const std::string& ip() const noexcept;
    std::uint16_t port() const noexcept;

private:
    struct Impl;
    explicit UdpSender(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

// true, wenn text eine IPv4-Adresse in Punktschreibweise ist.
bool is_ipv4(const std::string& text);
// Dieselbe Adresse so, wie receive_until den Absender schreibt – damit ein Vergleich mit
// dem Absender nicht an der Schreibweise scheitert. nullopt, wenn text keine IPv4 ist.
std::optional<std::string> canonical_ipv4(const std::string& text);

// Ein gebundener Socket für Suchen und Abfragen: senden an Multicast, Broadcast oder ein
// einzelnes Gerät, Antworten lesen bis zu einer Frist.
class DiscoverySocket final {
public:
    // Bindet an bind_port (0 = vom System gewählt). Ist der Port belegt, wirft open
    // Error(network) mit busy_hint als Text – der Aufrufer weiß, wer ihn typischerweise
    // belegt (Govee Desktop auf 4002, ein Art-Net-Programm auf 6454).
    static std::unique_ptr<DiscoverySocket> open(std::uint16_t bind_port, std::string_view busy_hint);
    ~DiscoverySocket();
    DiscoverySocket(const DiscoverySocket&) = delete;
    DiscoverySocket& operator=(const DiscoverySocket&) = delete;

    // An group:port über jede aktive Schnittstelle; gelingt dort keine Sendung, über die
    // Standardschnittstelle. Liefert die Zahl gelungener Sendungen (0 = nichts ging raus,
    // Grund in error) – ob das ein Fehler ist, entscheidet der Aufrufer.
    int send_multicast(const std::string& group, std::uint16_t port, std::string_view payload, std::string& error);
    // An die Broadcast-Adresse jeder Schnittstelle und an 255.255.255.255. Rückgabe wie oben.
    int send_broadcast(std::uint16_t port, std::string_view payload, std::string& error);
    // An ein Gerät. false und Grund, wenn sendto scheitert.
    bool send_to(const std::string& ip, std::uint16_t port, std::string_view payload, std::string& error);
    // Der tatsächlich gebundene Port (bei open(0) vom System gewählt).
    std::uint16_t local_port() const;

    // Liest Datagramme bis deadline oder bis handler true liefert. handler bekommt Inhalt
    // und Absender-IP. Liefert einen Grund, wenn Winsock versagt; fremde und zu große
    // Datagramme werden übersprungen.
    using Handler = std::function<bool(std::string_view datagram, const std::string& from_ip)>;
    std::optional<std::string> receive_until(std::chrono::steady_clock::time_point deadline, const Handler& handler);

private:
    struct Impl;
    explicit DiscoverySocket(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}  // namespace ldu::net
