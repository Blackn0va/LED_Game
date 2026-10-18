#include "sacn/protocol.hpp"

#include "core/error.hpp"

#include <random>

namespace ldu::sacn {
namespace {

inline constexpr std::string_view kAcnId{"ASC-E1.17\0\0\0", 12};
inline constexpr std::size_t kRootStart = 16;
inline constexpr std::size_t kFramingStart = 38;
inline constexpr std::size_t kDmpStart = 115;
inline constexpr std::size_t kSourceNameSize = 64;

void put16(std::string& p, std::size_t at, std::uint16_t v) {
    p[at] = static_cast<char>(v >> 8);
    p[at + 1] = static_cast<char>(v & 0xFF);
}

void put32(std::string& p, std::size_t at, std::uint32_t v) {
    p[at] = static_cast<char>(v >> 24);
    p[at + 1] = static_cast<char>((v >> 16) & 0xFF);
    p[at + 2] = static_cast<char>((v >> 8) & 0xFF);
    p[at + 3] = static_cast<char>(v & 0xFF);
}

std::uint16_t flags_length(std::size_t total, std::size_t layer_start) {
    return static_cast<std::uint16_t>(0x7000 | ((total - layer_start) & 0x0FFF));
}

}  // namespace

std::string data_packet(const PacketFields& f, std::span<const std::uint8_t> channels) {
    if (channels.size() > static_cast<std::size_t>(kMaxChannels)) {
        throw Error(Error::Kind::protocol, "Ein sACN-Universum hat höchstens 512 Kanäle, nicht " +
                                               std::to_string(channels.size()) + ".");
    }
    if (f.universe < kMinUniverse || f.universe > kMaxUniverse) {
        throw Error(Error::Kind::protocol,
                    "sACN-Universum " + std::to_string(f.universe) + " liegt nicht zwischen 1 und 63999.");
    }
    const std::size_t total = kHeaderSize + channels.size();
    std::string p(total, '\0');
    // Root-Schicht
    put16(p, 0, 0x0010);
    put16(p, 2, 0x0000);
    p.replace(4, kAcnId.size(), kAcnId);
    put16(p, kRootStart, flags_length(total, kRootStart));
    put32(p, 18, 0x00000004);
    for (std::size_t i = 0; i < f.cid.size(); ++i) {
        p[22 + i] = static_cast<char>(f.cid[i]);
    }
    // Framing-Schicht
    put16(p, kFramingStart, flags_length(total, kFramingStart));
    put32(p, 40, 0x00000002);
    const std::string_view name = f.source_name.substr(0, kSourceNameSize - 1);  // Platz für die 0
    p.replace(44, name.size(), name);
    p[108] = static_cast<char>(f.priority);
    put16(p, 109, 0);  // keine Synchronisation
    p[111] = static_cast<char>(f.sequence);
    p[112] = static_cast<char>(f.options);
    put16(p, 113, static_cast<std::uint16_t>(f.universe));
    // DMP-Schicht
    put16(p, kDmpStart, flags_length(total, kDmpStart));
    p[117] = 0x02;
    p[118] = static_cast<char>(0xA1);
    put16(p, 119, 0x0000);
    put16(p, 121, 0x0001);
    put16(p, 123, static_cast<std::uint16_t>(channels.size() + 1));
    p[125] = 0x00;  // Startcode: Helligkeitswerte
    for (std::size_t i = 0; i < channels.size(); ++i) {
        p[kHeaderSize + i] = static_cast<char>(channels[i]);
    }
    return p;
}

std::string multicast_group(int universe) {
    return "239.255." + std::to_string((universe >> 8) & 0xFF) + "." + std::to_string(universe & 0xFF);
}

Cid random_cid() {
    std::random_device rd;
    Cid cid{};
    for (std::size_t i = 0; i < cid.size(); i += 4) {
        const unsigned v = rd();
        cid[i] = static_cast<std::uint8_t>(v);
        cid[i + 1] = static_cast<std::uint8_t>(v >> 8);
        cid[i + 2] = static_cast<std::uint8_t>(v >> 16);
        cid[i + 3] = static_cast<std::uint8_t>(v >> 24);
    }
    cid[6] = static_cast<std::uint8_t>((cid[6] & 0x0F) | 0x40);  // Version 4 (RFC 4122 §4.4)
    cid[8] = static_cast<std::uint8_t>((cid[8] & 0x3F) | 0x80);  // Variante RFC 4122
    return cid;
}

}  // namespace ldu::sacn
