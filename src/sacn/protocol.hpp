// sACN (ANSI E1.31): Datenpakete bauen – ohne Netz.
//
// Aufbau E1.31-2016 §4–§7, alle Mehrbyte-Felder big-endian:
//   Root     @0   Preamble 0x0010, @2 Postamble 0, @4 „ASC-E1.17\0\0\0“,
//            @16  Flags+Länge, @18 Vector 0x00000004, @22 CID (16 Byte)
//   Framing  @38  Flags+Länge, @40 Vector 0x00000002, @44 Quellname (64 Byte, UTF-8, 0-terminiert),
//            @108 Priorität, @109 Sync-Adresse, @111 Sequenz, @112 Optionen, @113 Universum
//   DMP      @115 Flags+Länge, @117 Vector 0x02, @118 Adress-/Datentyp 0xA1,
//            @119 erste Adresse 0, @121 Schrittweite 1, @123 Zahl der Werte (1 + Kanäle),
//            @125 Startcode 0, @126 Kanäle
// Flags+Länge = 0x7000 | (Paketlänge − Beginn der Schicht). Optionen: 0x40 = Strom endet
// (§6.2.6: beim Beenden dreimal senden). Multicast an 239.255.<hi>.<lo>, Port 5568.
// Die Werte stammen aus der Erinnerung an die Norm, nicht aus dem Normtext; der Test prüft
// zusätzlich den aus Mitschnitten bekannten Root-Wert 0x726E eines vollen Universums – auch
// der ist erinnert. Gegen ein Gerät ist nichts geprüft (Gedaechtnis.md → Offen).
//
// Threads: reine Funktionen; random_cid nutzt std::random_device.
// Fehler: data_packet wirft Error(protocol) bei mehr als 512 Kanälen oder einem
// Universum außerhalb 1–63999.

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace ldu::sacn {

inline constexpr std::uint16_t kPort = 5568;
inline constexpr std::uint8_t kDefaultPriority = 100;
inline constexpr std::uint8_t kOptionTerminated = 0x40;
inline constexpr int kMaxChannels = 512;
inline constexpr int kMinUniverse = 1;
inline constexpr int kMaxUniverse = 63999;
inline constexpr std::size_t kHeaderSize = 126;

using Cid = std::array<std::uint8_t, 16>;

struct PacketFields final {
    Cid cid{};
    std::string_view source_name = "LichtDuell";
    std::uint8_t priority = kDefaultPriority;
    int universe = 1;
    std::uint8_t sequence = 0;
    std::uint8_t options = 0;
};

std::string data_packet(const PacketFields& fields, std::span<const std::uint8_t> channels);

// "239.255.<universe / 256>.<universe % 256>"
std::string multicast_group(int universe);

// Zufällige UUID (Version 4) als Quellkennung. Je Programmstart neu: Die Norm wünscht eine
// beständige CID je Quelle, ein Wechsel kostet aber nur, dass der Empfänger die alte Quelle
// nach seiner Frist vergisst – nach dem Beenden-Paket ohnehin sofort.
Cid random_cid();

}  // namespace ldu::sacn
