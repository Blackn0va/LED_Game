#include "govee/protocol.hpp"

#include "core/error.hpp"

#include <utility>

namespace ldu::govee {
namespace {

inline constexpr char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Wert des ersten Schlüssels `key` mit Zeichenkettenwert. Reicht für die flachen,
// immer gleich aufgebauten Antworten der Geräte; ein voller JSON-Parser wäre hier
// eine Abhängigkeit für drei Felder.
std::optional<std::string> string_field(std::string_view json, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    std::size_t pos = 0;
    while ((pos = json.find(needle, pos)) != std::string_view::npos) {
        std::size_t p = pos + needle.size();
        while (p < json.size() && is_space(json[p])) {
            ++p;
        }
        if (p >= json.size() || json[p] != ':') {
            pos += needle.size();
            continue;
        }
        ++p;
        while (p < json.size() && is_space(json[p])) {
            ++p;
        }
        if (p >= json.size() || json[p] != '"') {
            return std::nullopt;
        }
        ++p;
        const std::size_t end = json.find('"', p);
        if (end == std::string_view::npos) {
            return std::nullopt;
        }
        const std::string_view value = json.substr(p, end - p);
        if (value.find('\\') != std::string_view::npos) {
            return std::nullopt;
        }
        return std::string(value);
    }
    return std::nullopt;
}

// Ganzzahliger Wert des ersten Schlüssels `key` (ohne Vorzeichen, höchstens 6 Ziffern –
// mehr liefert kein Gerät, ein längerer Wert ist kaputt).
std::optional<int> int_field(std::string_view json, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    std::size_t p = pos + needle.size();
    while (p < json.size() && is_space(json[p])) {
        ++p;
    }
    if (p >= json.size() || json[p] != ':') {
        return std::nullopt;
    }
    ++p;
    while (p < json.size() && is_space(json[p])) {
        ++p;
    }
    int value = 0;
    int digits = 0;
    while (p < json.size() && json[p] >= '0' && json[p] <= '9' && digits < 7) {
        value = value * 10 + (json[p] - '0');
        ++p;
        ++digits;
    }
    if (digits == 0 || digits > 6) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

std::string base64_encode(std::span<const std::uint8_t> data) {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) | data[i + 2];
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 6) & 0x3F]);
        out.push_back(kBase64Alphabet[v & 0x3F]);
    }
    const std::size_t rest = data.size() - i;
    if (rest == 1) {
        const std::uint32_t v = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        out += "==";
    } else if (rest == 2) {
        const std::uint32_t v =
            (static_cast<std::uint32_t>(data[i]) << 16) | (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 6) & 0x3F]);
        out += "=";
    }
    return out;
}

std::vector<std::uint8_t> razer_packet(std::uint8_t command, std::span<const std::uint8_t> data) {
    if (data.size() > 255) {
        throw Error(Error::Kind::protocol, "Razer-Paket zu groß: " + std::to_string(data.size()) +
                                               " Datenbytes, höchstens 255 passen in das Längenbyte.");
    }
    std::vector<std::uint8_t> packet;
    packet.reserve(data.size() + 5);
    packet.push_back(kRazerHeader);
    packet.push_back(0x00);
    packet.push_back(static_cast<std::uint8_t>(data.size()));
    packet.push_back(command);
    packet.insert(packet.end(), data.begin(), data.end());
    std::uint8_t sum = 0;
    for (const std::uint8_t b : packet) {
        sum ^= b;
    }
    packet.push_back(sum);
    return packet;
}

std::vector<std::uint8_t> razer_mode_packet(bool on) {
    const std::uint8_t flag = on ? 1 : 0;
    return razer_packet(kRazerMode, std::span<const std::uint8_t>(&flag, 1));
}

std::vector<std::uint8_t> razer_frame_packet(std::span<const Rgb> colors, bool gradient) {
    if (colors.empty() || colors.size() > static_cast<std::size_t>(kMaxRazerColors)) {
        throw Error(Error::Kind::protocol, "Ein Razer-Bild braucht 1 bis " + std::to_string(kMaxRazerColors) +
                                               " Farben, nicht " + std::to_string(colors.size()) + ".");
    }
    std::vector<std::uint8_t> data;
    data.reserve(2 + colors.size() * 3);
    data.push_back(gradient ? 1 : 0);
    data.push_back(static_cast<std::uint8_t>(colors.size()));
    for (const Rgb& c : colors) {
        data.push_back(c.r);
        data.push_back(c.g);
        data.push_back(c.b);
    }
    return razer_packet(kRazerFrame, data);
}

std::string razer_message(std::span<const std::uint8_t> packet) {
    return R"({"msg":{"cmd":"razer","data":{"pt":")" + base64_encode(packet) + R"("}}})";
}

std::string turn_message(bool on) {
    return on ? R"({"msg":{"cmd":"turn","data":{"value":1}}})" : R"({"msg":{"cmd":"turn","data":{"value":0}}})";
}

std::string scan_message() {
    return R"({"msg":{"cmd":"scan","data":{"account_topic":"reserve"}}})";
}

std::string status_message() {
    return R"({"msg":{"cmd":"devStatus","data":{}}})";
}

std::optional<DeviceStatus> parse_status_reply(std::string_view json) {
    const auto cmd = string_field(json, "cmd");
    if (!cmd || *cmd != "devStatus") {
        return std::nullopt;
    }
    const auto on = int_field(json, "onOff");
    if (!on) {
        return std::nullopt;
    }
    DeviceStatus status;
    status.on = *on != 0;
    status.brightness = int_field(json, "brightness").value_or(0);
    return status;
}

std::optional<DeviceInfo> parse_scan_reply(std::string_view json) {
    const auto cmd = string_field(json, "cmd");
    if (!cmd || *cmd != "scan") {
        return std::nullopt;
    }
    auto ip = string_field(json, "ip");
    if (!ip || ip->empty()) {
        return std::nullopt;
    }
    DeviceInfo info;
    info.ip = std::move(*ip);
    info.device = string_field(json, "device").value_or("");
    info.sku = string_field(json, "sku").value_or("");
    return info;
}

}  // namespace ldu::govee
