#include "net/mdns.hpp"

#include <cstddef>

namespace ldu::net {
namespace {

inline constexpr std::uint16_t kTypePtr = 12;
inline constexpr std::uint16_t kClassIn = 1;
inline constexpr std::uint16_t kUnicastResponse = 0x8000;  // QU-Bit (RFC 6762 §5.4)
inline constexpr int kMaxJumps = 16;
inline constexpr std::size_t kMaxNameLength = 255;  // RFC 1035 §3.1

void put16(std::string& out, std::uint16_t v) {
    out.push_back(static_cast<char>(v >> 8));
    out.push_back(static_cast<char>(v & 0xFF));
}

std::uint16_t get16(std::string_view p, std::size_t at) {
    return static_cast<std::uint16_t>((static_cast<unsigned char>(p[at]) << 8) | static_cast<unsigned char>(p[at + 1]));
}

char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool same_name(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (lower(a[i]) != lower(b[i])) {
            return false;
        }
    }
    return true;
}

// Liest einen Namen ab at (mit Kompression, RFC 1035 §4.1.4). next ist die Stelle hinter
// dem Namen im Datensatz (nicht hinter einem Sprungziel). Labels mit Punkten verbunden.
bool read_name(std::string_view p, std::size_t at, std::string& name, std::size_t& next) {
    name.clear();
    std::size_t pos = at;
    bool jumped = false;
    int jumps = 0;
    for (;;) {
        if (pos >= p.size()) {
            return false;
        }
        const auto len = static_cast<unsigned char>(p[pos]);
        if (len == 0) {
            if (!jumped) {
                next = pos + 1;
            }
            return true;
        }
        if ((len & 0xC0) == 0xC0) {
            if (pos + 1 >= p.size() || ++jumps > kMaxJumps) {
                return false;  // abgeschnitten oder Zeiger-Kreis
            }
            if (!jumped) {
                next = pos + 2;
            }
            pos = static_cast<std::size_t>(get16(p, pos) & 0x3FFF);
            jumped = true;
            continue;
        }
        if ((len & 0xC0) != 0) {
            return false;  // erweiterte Label-Typen gibt es in mDNS nicht
        }
        if (pos + 1 + len > p.size()) {
            return false;
        }
        if (!name.empty()) {
            name.push_back('.');
        }
        name.append(p.substr(pos + 1, len));
        if (name.size() > kMaxNameLength) {
            return false;
        }
        pos += 1 + len;
    }
}

}  // namespace

std::string mdns_query(std::string_view service) {
    std::string q;
    put16(q, kMdnsQueryId);
    put16(q, 0);  // Flags: Standardanfrage
    put16(q, 1);  // eine Frage
    put16(q, 0);
    put16(q, 0);
    put16(q, 0);
    std::size_t start = 0;
    while (start < service.size()) {
        std::size_t dot = service.find('.', start);
        if (dot == std::string_view::npos) {
            dot = service.size();
        }
        std::string_view label = service.substr(start, dot - start);
        if (label.size() > 63) {
            label = label.substr(0, 63);
        }
        if (!label.empty()) {
            q.push_back(static_cast<char>(label.size()));
            q.append(label);
        }
        start = dot + 1;
    }
    q.push_back('\0');
    put16(q, kTypePtr);
    put16(q, static_cast<std::uint16_t>(kClassIn | kUnicastResponse));
    return q;
}

std::optional<std::vector<std::string>> parse_mdns_ptr(std::string_view packet, std::string_view service) {
    if (packet.size() < 12) {
        return std::nullopt;
    }
    const std::uint16_t flags = get16(packet, 2);
    if ((flags & 0x8000) == 0) {
        return std::nullopt;  // eine Anfrage, keine Antwort (etwa die eigene)
    }
    const std::size_t questions = get16(packet, 4);
    const std::size_t records =
        static_cast<std::size_t>(get16(packet, 6)) + get16(packet, 8) + get16(packet, 10);
    std::size_t pos = 12;
    std::string name;
    for (std::size_t i = 0; i < questions; ++i) {
        std::size_t next = 0;
        if (!read_name(packet, pos, name, next) || next + 4 > packet.size()) {
            return std::nullopt;
        }
        pos = next + 4;
    }
    std::vector<std::string> instances;
    for (std::size_t i = 0; i < records; ++i) {
        std::size_t next = 0;
        if (!read_name(packet, pos, name, next) || next + 10 > packet.size()) {
            return std::nullopt;
        }
        const std::uint16_t type = get16(packet, next);
        const std::size_t rdlength = get16(packet, next + 8);
        const std::size_t rdata = next + 10;
        if (rdata + rdlength > packet.size()) {
            return std::nullopt;
        }
        if (type == kTypePtr && same_name(name, service)) {
            std::string target;
            std::size_t after = 0;
            if (!read_name(packet, rdata, target, after)) {
                return std::nullopt;
            }
            const std::size_t dot = target.find('.');
            std::string instance = target.substr(0, dot);
            if (!instance.empty()) {
                instances.push_back(std::move(instance));
            }
        }
        pos = rdata + rdlength;
    }
    return instances;
}

}  // namespace ldu::net
