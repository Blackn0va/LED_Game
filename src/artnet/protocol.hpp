// Art-Net 4: ArtDmx (Bilddaten), ArtPoll (Suche) und ArtPollReply lesen – ohne Netz.
//
// Aufbau nach der Art-Net-4-Spezifikation (Artistic Licence), alle Pakete mit „Art-Net\0“
// und OpCode little-endian an Byte 8:
//   ArtDmx        OpCode 0x5000, ProtVer 14 (big-endian), Sequence @12 (0 = aus, sonst
//                 1–255 umlaufend), Physical @13, SubUni @14 (unteres Byte der
//                 Port-Adresse), Net @15 (Bits 14–8), Length big-endian @16 (gerade,
//                 2–512), Daten ab @18.
//   ArtPoll       OpCode 0x2000, ProtVer 14, Flags @12, DiagPriority @13 – 14 Byte.
//   ArtPollReply  OpCode 0x2100, IP @10, Port @14, NetSwitch @18, SubSwitch @19,
//                 ShortName @26 (18 Byte), LongName @44 (64 Byte), SwOut @190 (4 Byte).
// Port 6454 für Senden und Empfangen. Gegen einen echten Art-Net-Knoten ist nichts davon
// geprüft (Gedaechtnis.md → Offen).
//
// Threads: reine Funktionen.
// Fehler: art_dmx wirft Error(protocol) bei mehr als 512 Kanälen oder einer
// Port-Adresse über 32767.

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ldu::artnet {

inline constexpr std::uint16_t kPort = 6454;
inline constexpr std::uint16_t kOpPoll = 0x2000;
inline constexpr std::uint16_t kOpPollReply = 0x2100;
inline constexpr std::uint16_t kOpDmx = 0x5000;
inline constexpr std::uint8_t kProtocolVersion = 14;
inline constexpr int kMaxChannels = 512;
inline constexpr int kMaxPortAddress = 32767;

// Ungerade Kanalzahlen werden mit einer 0 aufgefüllt, leere auf zwei Kanäle – die
// Spezifikation verlangt eine gerade Länge von 2 bis 512.
std::string art_dmx(int port_address, std::uint8_t sequence, std::span<const std::uint8_t> channels);
std::string art_poll();

struct ArtNode final {
    std::string ip;            // aus dem Paket; 0.0.0.0 ersetzt der Aufrufer durch den Absender
    std::string short_name;
    std::string long_name;
    std::optional<int> port_address;  // des ersten Ausgangs, wenn das Paket ihn enthält

    friend bool operator==(const ArtNode&, const ArtNode&) = default;
};

// nullopt, wenn das kein ArtPollReply ist oder vor dem Ende von LongName abbricht.
std::optional<ArtNode> parse_poll_reply(std::string_view packet);

}  // namespace ldu::artnet
