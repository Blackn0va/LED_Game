#include "artnet/protocol.hpp"

#include "core/error.hpp"

#include <string_view>

namespace ldu::artnet {
namespace {

inline constexpr std::string_view kId{"Art-Net\0", 8};

void header(std::string& p, std::uint16_t opcode) {
    p.append(kId);
    p.push_back(static_cast<char>(opcode & 0xFF));  // OpCode little-endian
    p.push_back(static_cast<char>(opcode >> 8));
    p.push_back(0);                                  // ProtVerHi
    p.push_back(static_cast<char>(kProtocolVersion));  // ProtVerLo
}

// Feste Textfelder sind nullterminiert; ohne Null gilt das ganze Feld.
std::string fixed_text(std::string_view packet, std::size_t at, std::size_t size) {
    std::string_view field = packet.substr(at, size);
    const std::size_t nul = field.find('\0');
    if (nul != std::string_view::npos) {
        field = field.substr(0, nul);
    }
    std::string out;
    for (const char c : field) {
        // Nur Druckbares: der Name landet in der Konsole, Steuerzeichen könnten dort
        // VT-Folgen auslösen (CWE-150).
        out.push_back(static_cast<unsigned char>(c) < 0x20 || c == 0x7F ? '?' : c);
    }
    return out;
}

std::uint8_t byte_at(std::string_view p, std::size_t at) {
    return static_cast<std::uint8_t>(p[at]);
}

}  // namespace

std::string art_dmx(int port_address, std::uint8_t sequence, std::span<const std::uint8_t> channels) {
    if (channels.size() > static_cast<std::size_t>(kMaxChannels)) {
        throw Error(Error::Kind::protocol, "Ein Art-Net-Universum hat höchstens 512 Kanäle, nicht " +
                                               std::to_string(channels.size()) + ".");
    }
    if (port_address < 0 || port_address > kMaxPortAddress) {
        throw Error(Error::Kind::protocol,
                    "Art-Net-Universum " + std::to_string(port_address) + " liegt nicht zwischen 0 und 32767.");
    }
    std::size_t length = channels.size() + (channels.size() % 2);
    if (length < 2) {
        length = 2;
    }
    std::string p;
    p.reserve(18 + length);
    header(p, kOpDmx);
    p.push_back(static_cast<char>(sequence));
    p.push_back(0);                                            // Physical
    p.push_back(static_cast<char>(port_address & 0xFF));       // SubUni
    p.push_back(static_cast<char>((port_address >> 8) & 0x7F));  // Net
    p.push_back(static_cast<char>(length >> 8));
    p.push_back(static_cast<char>(length & 0xFF));
    for (const std::uint8_t b : channels) {
        p.push_back(static_cast<char>(b));
    }
    p.resize(18 + length, '\0');
    return p;
}

std::string art_poll() {
    std::string p;
    header(p, kOpPoll);
    p.push_back(0);  // Flags: einmal antworten, keine Diagnose
    p.push_back(0);  // DiagPriority
    return p;
}

std::optional<ArtNode> parse_poll_reply(std::string_view packet) {
    if (packet.size() < 108 || packet.substr(0, 8) != kId) {
        return std::nullopt;
    }
    const std::uint16_t opcode = static_cast<std::uint16_t>(byte_at(packet, 8) | (byte_at(packet, 9) << 8));
    if (opcode != kOpPollReply) {
        return std::nullopt;
    }
    ArtNode node;
    node.ip = std::to_string(byte_at(packet, 10)) + "." + std::to_string(byte_at(packet, 11)) + "." +
              std::to_string(byte_at(packet, 12)) + "." + std::to_string(byte_at(packet, 13));
    node.short_name = fixed_text(packet, 26, 18);
    node.long_name = fixed_text(packet, 44, 64);
    if (packet.size() >= 194) {
        node.port_address = ((byte_at(packet, 18) & 0x7F) << 8) | ((byte_at(packet, 19) & 0x0F) << 4) |
                            (byte_at(packet, 190) & 0x0F);
    }
    return node;
}

}  // namespace ldu::artnet
