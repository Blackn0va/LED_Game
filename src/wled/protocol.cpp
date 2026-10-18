#include "wled/protocol.hpp"

#include "core/error.hpp"
#include "core/json.hpp"

namespace ldu::wled {

std::string drgb_packet(std::span<const Rgb> leds, std::uint8_t timeout_s) {
    if (leds.empty() || leds.size() > static_cast<std::size_t>(kMaxDrgbLeds)) {
        throw Error(Error::Kind::protocol, "Ein WLED-DRGB-Bild braucht 1 bis " + std::to_string(kMaxDrgbLeds) +
                                               " Farben, nicht " + std::to_string(leds.size()) + ".");
    }
    std::string p;
    p.reserve(2 + leds.size() * 3);
    p.push_back(static_cast<char>(kDrgb));
    p.push_back(static_cast<char>(timeout_s));
    for (const Rgb& c : leds) {
        p.push_back(static_cast<char>(c.r));
        p.push_back(static_cast<char>(c.g));
        p.push_back(static_cast<char>(c.b));
    }
    return p;
}

std::string dnrgb_packet(int first_index, std::span<const Rgb> leds, std::uint8_t timeout_s) {
    if (leds.empty() || leds.size() > static_cast<std::size_t>(kMaxDnrgbLeds)) {
        throw Error(Error::Kind::protocol, "Ein WLED-DNRGB-Teilbild braucht 1 bis " + std::to_string(kMaxDnrgbLeds) +
                                               " Farben, nicht " + std::to_string(leds.size()) + ".");
    }
    if (first_index < 0 || first_index > 0xFFFF) {
        throw Error(Error::Kind::protocol,
                    "WLED-DNRGB-Index " + std::to_string(first_index) + " passt nicht in zwei Byte.");
    }
    std::string p;
    p.reserve(4 + leds.size() * 3);
    p.push_back(static_cast<char>(kDnrgb));
    p.push_back(static_cast<char>(timeout_s));
    p.push_back(static_cast<char>(first_index >> 8));
    p.push_back(static_cast<char>(first_index & 0xFF));
    for (const Rgb& c : leds) {
        p.push_back(static_cast<char>(c.r));
        p.push_back(static_cast<char>(c.g));
        p.push_back(static_cast<char>(c.b));
    }
    return p;
}

std::optional<WledInfo> parse_info(std::string_view json) {
    const auto root = parse_json(json);
    if (!root || !root->is_object()) {
        return std::nullopt;
    }
    const JsonValue* leds = root->find("leds");
    const JsonValue* count = leds != nullptr ? leds->find("count") : nullptr;
    const auto n = count != nullptr ? count->as_int() : std::nullopt;
    if (!n || *n < 1 || *n > 10000) {
        return std::nullopt;
    }
    WledInfo info;
    info.leds = *n;
    auto text = [&root](std::string_view key) {
        const JsonValue* v = root->find(key);
        return v != nullptr ? v->as_string().value_or("") : std::string();
    };
    info.name = text("name");
    info.version = text("ver");
    info.brand = text("brand");
    if (const JsonValue* port = root->find("udpport")) {
        const auto p = port->as_int();
        if (p && *p > 0 && *p <= 65535) {
            info.udp_port = *p;
        }
    }
    return info;
}

}  // namespace ldu::wled
