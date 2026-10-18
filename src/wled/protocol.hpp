// WLED: Echtzeit-Bilder per UDP und die Geräteauskunft /json/info – ohne Netz.
//
// Nach der WLED-Doku (kno.wled.ge, „UDP Realtime“ und „JSON API“):
//   DRGB   Byte 0 = 2, Byte 1 = Frist in Sekunden, danach R G B je LED, höchstens 490.
//          Kommt innerhalb der Frist kein neues Paket, kehrt WLED in seinen normalen
//          Modus zurück (255 hieße: bleibt im Echtzeitmodus).
//   DNRGB  Byte 0 = 4, Byte 1 = Frist, Byte 2/3 = Index der ersten LED (hohes Byte zuerst),
//          ab Byte 4 R G B, höchstens 489 LEDs je Paket – laut Doku eigens für mehr als 490
//          LEDs über mehrere Pakete. Eine Gesamtgrenze nennt die Doku nicht (nachgelesen
//          2026-10-18, kno.wled.ge/interfaces/udp-realtime).
//   Port   21324 (Vorgabe, in WLED unter Sync einstellbar; /json/info nennt ihn als udpport).
//   Info   GET /json/info → {"name":…, "leds":{"count":…}, "ver":…, "brand":"WLED", …}
// Gegen ein echtes WLED-Gerät ist nichts davon geprüft (Gedaechtnis.md → Offen).
//
// Threads: reine Funktionen.
// Fehler: drgb_packet wirft Error(protocol) bei 0 oder mehr als kMaxDrgbLeds Farben,
// dnrgb_packet bei 0 oder mehr als kMaxDnrgbLeds Farben oder einem Index über 65535.

#pragma once

#include "core/rgb.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ldu::wled {

inline constexpr std::uint16_t kUdpPort = 21324;
inline constexpr std::uint16_t kHttpPort = 80;
inline constexpr int kMaxDrgbLeds = 490;
inline constexpr int kMaxDnrgbLeds = 489;
inline constexpr std::uint8_t kDrgb = 2;
inline constexpr std::uint8_t kDnrgb = 4;
inline constexpr std::string_view kMdnsService = "_wled._tcp.local";

std::string drgb_packet(std::span<const Rgb> leds, std::uint8_t timeout_s);
// Ein Teilbild ab LED first_index.
std::string dnrgb_packet(int first_index, std::span<const Rgb> leds, std::uint8_t timeout_s);

struct WledInfo final {
    std::string name;      // Gerätename aus den WLED-Einstellungen
    int leds = 0;          // leds.count
    std::string version;   // ver
    std::string brand;     // brand, meist "WLED"
    int udp_port = 0;      // udpport, 0 = nicht gemeldet

    friend bool operator==(const WledInfo&, const WledInfo&) = default;
};

// Liest /json/info. nullopt, wenn das kein JSON-Objekt ist oder leds.count fehlt bzw.
// nicht zwischen 1 und 10000 liegt (mehr kann WLED nicht; ein solcher Wert ist kaputt).
std::optional<WledInfo> parse_info(std::string_view json);

}  // namespace ldu::wled
