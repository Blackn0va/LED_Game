// Govee-LAN-Protokoll: Nachrichten bauen und Scan-Antworten lesen – ohne Netz.
//
// Offiziell (Govee WLAN-Anleitung, openHAB, homebridge-govee): JSON über UDP,
// Suche per Multicast 239.255.255.250:4001, Antworten an Port 4002, Befehle an
// Geräteport 4003. Die offizielle API kennt nur eine Farbe für den ganzen Streifen.
//
// Einzelne Zellen gehen nur über den NICHT dokumentierten „razer“-Befehl (DreamView,
// derselbe Weg wie Govee Desktop und Razer Chroma). Aufbau nach OpenRGB-MR 2172
// (gitlab.com/CalcProgrammer1/OpenRGB/-/merge_requests/2172):
//   Paket  BB 00 <Länge> <Befehl> <Daten …> <XOR aller Bytes davor>
//   B1     Daten = 1 Byte: 1 schaltet den Modus ein, 0 aus
//   B0     Daten = <Verlauf 0/1> <Anzahl> <R G B> × Anzahl
//   JSON   {"msg":{"cmd":"razer","data":{"pt":"<Base64 des Pakets>"}}}
// Ohne neues B0-Paket fällt das Gerät nach einer Minute in seinen alten Modus zurück.
// Ob das zweite Byte ein hohes Längenbyte ist, ist dort offen – deshalb höchstens 84
// Farben (3·84 + 2 = 254 passt in ein Byte). Verifiziert ist das Format laut DMXCore
// nur am H618A; an diesem Streifen ist es noch NICHT geprüft (Gedaechtnis.md → Offen).
//
// Threads: reine Funktionen.
// Fehler: Error(Kind::protocol) bei zu vielen oder keinen Farben.

#pragma once

#include "core/rgb.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ldu::govee {

inline constexpr std::uint16_t kScanPort = 4001;
inline constexpr std::uint16_t kReplyPort = 4002;
inline constexpr std::uint16_t kCommandPort = 4003;
inline constexpr std::string_view kMulticastGroup = "239.255.255.250";
inline constexpr int kMaxRazerColors = 84;

inline constexpr std::uint8_t kRazerHeader = 0xBB;
inline constexpr std::uint8_t kRazerFrame = 0xB0;
inline constexpr std::uint8_t kRazerMode = 0xB1;

struct DeviceInfo final {
    std::string ip;
    std::string device;  // MAC-artige Geräte-ID
    std::string sku;     // Modell, z. B. H619A

    friend bool operator==(const DeviceInfo&, const DeviceInfo&) = default;
};

// Antwort auf devStatus (Govee WLAN-Anleitung: onOff 1/0, brightness 1–100). Laut einem
// Node-RED-Forumsbeitrag antwortet nicht jedes Modell – fehlende Antwort ist kein Fehler.
struct DeviceStatus final {
    bool on = false;
    int brightness = 0;

    friend bool operator==(const DeviceStatus&, const DeviceStatus&) = default;
};

std::string base64_encode(std::span<const std::uint8_t> data);

// BB 00 <Länge> <Befehl> <Daten> <XOR>. Wirft bei mehr als 255 Datenbytes.
std::vector<std::uint8_t> razer_packet(std::uint8_t command, std::span<const std::uint8_t> data);
std::vector<std::uint8_t> razer_mode_packet(bool on);
std::vector<std::uint8_t> razer_frame_packet(std::span<const Rgb> colors, bool gradient);

std::string razer_message(std::span<const std::uint8_t> packet);
std::string turn_message(bool on);
std::string scan_message();
std::string status_message();

// Liest {"msg":{"cmd":"devStatus","data":{"onOff":1,"brightness":100,…}}}. nullopt,
// wenn cmd nicht devStatus ist oder onOff fehlt.
std::optional<DeviceStatus> parse_status_reply(std::string_view json);

// Liest {"msg":{"cmd":"scan","data":{"ip":…,"device":…,"sku":…}}}. nullopt, wenn das
// keine Scan-Antwort ist oder ip fehlt. Werte mit Escapes werden abgelehnt statt
// halb gelesen – kein Gerät schickt sie, ein solcher Wert ist kaputt oder fremd.
std::optional<DeviceInfo> parse_scan_reply(std::string_view json);

}  // namespace ldu::govee
