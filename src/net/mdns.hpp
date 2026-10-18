// mDNS-Suche nach einem Dienst (RFC 6762/6763) – für WLED („_wled._tcp.local“).
//
// Als „Legacy-Unicast“-Anfrage von einem beliebigen Quellport statt von 5353: Laut RFC 6762
// §6.7 MUSS ein Responder dann per Unicast an genau diesen Port antworten. So braucht das
// Programm Port 5353 nicht, den unter Windows schon der DNS-Client-Dienst hält. Ob die
// WLED-Firmware (ESP32/ESP8266) das einhält, ist an keinem Gerät geprüft (Gedaechtnis.md
// → Offen); ohne Antwort hilft --ip.
//
// Antworten kommen aus dem Netz: Namen mit Kompressionszeigern werden mit Sprung- und
// Längengrenze gelesen, damit ein Zeiger-Kreis kein Hänger wird (CWE-835).
//
// Threads: reine Funktionen.
// Fehler: keine Würfe; kaputte Pakete liefern nullopt.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ldu::net {

inline constexpr std::string_view kMdnsGroup = "224.0.0.251";
inline constexpr std::uint16_t kMdnsPort = 5353;
inline constexpr std::uint16_t kMdnsQueryId = 0x4C44;  // „LD“; Legacy-Antworten wiederholen sie

// PTR-Anfrage nach service (z. B. "_wled._tcp.local"). Wirft nicht; ein Label über 63
// Zeichen wird abgeschnitten (kommt bei festen Dienstnamen nicht vor).
std::string mdns_query(std::string_view service);

// Instanznamen aller PTR-Antworten auf service in einer mDNS-Antwort, jeweils nur das
// erste Label („wled-a1b2c3“). nullopt, wenn das Paket keine gültige Antwort ist; leer,
// wenn es gültig ist, aber nichts zu service enthält.
std::optional<std::vector<std::string>> parse_mdns_ptr(std::string_view packet, std::string_view service);

}  // namespace ldu::net
