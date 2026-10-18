#include "core/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ldu {
namespace {

class Parser final {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    std::optional<JsonValue> document() {
        skip_space();
        auto value = parse_value(0);
        if (!value) {
            return std::nullopt;
        }
        skip_space();
        if (pos_ != text_.size()) {
            return std::nullopt;
        }
        return value;
    }

private:
    void skip_space() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
                return;
            }
            ++pos_;
        }
    }

    bool take(char c) {
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool take_word(std::string_view word) {
        if (text_.substr(pos_, word.size()) == word) {
            pos_ += word.size();
            return true;
        }
        return false;
    }

    std::optional<JsonValue> parse_value(int depth) {
        if (depth > kJsonMaxDepth || pos_ >= text_.size()) {
            return std::nullopt;
        }
        const char c = text_[pos_];
        if (c == '{') {
            return parse_object(depth);
        }
        if (c == '[') {
            return parse_array(depth);
        }
        if (c == '"') {
            auto s = parse_string();
            if (!s) {
                return std::nullopt;
            }
            return JsonValue::make_string(std::move(*s));
        }
        if (take_word("true")) {
            return JsonValue::make_bool(true);
        }
        if (take_word("false")) {
            return JsonValue::make_bool(false);
        }
        if (take_word("null")) {
            return JsonValue::make_null();
        }
        return parse_number();
    }

    std::optional<JsonValue> parse_object(int depth) {
        ++pos_;  // {
        std::vector<std::pair<std::string, JsonValue>> members;
        skip_space();
        if (take('}')) {
            return JsonValue::make_object(std::move(members));
        }
        for (;;) {
            skip_space();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                return std::nullopt;
            }
            auto key = parse_string();
            if (!key) {
                return std::nullopt;
            }
            skip_space();
            if (!take(':')) {
                return std::nullopt;
            }
            skip_space();
            auto value = parse_value(depth + 1);
            if (!value) {
                return std::nullopt;
            }
            members.emplace_back(std::move(*key), std::move(*value));
            skip_space();
            if (take('}')) {
                return JsonValue::make_object(std::move(members));
            }
            if (!take(',')) {
                return std::nullopt;
            }
        }
    }

    std::optional<JsonValue> parse_array(int depth) {
        ++pos_;  // [
        std::vector<JsonValue> items;
        skip_space();
        if (take(']')) {
            return JsonValue::make_array(std::move(items));
        }
        for (;;) {
            skip_space();
            auto value = parse_value(depth + 1);
            if (!value) {
                return std::nullopt;
            }
            items.push_back(std::move(*value));
            skip_space();
            if (take(']')) {
                return JsonValue::make_array(std::move(items));
            }
            if (!take(',')) {
                return std::nullopt;
            }
        }
    }

    std::optional<unsigned> hex4() {
        if (pos_ + 4 > text_.size()) {
            return std::nullopt;
        }
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9') {
                v |= static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                v |= static_cast<unsigned>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                v |= static_cast<unsigned>(c - 'A' + 10);
            } else {
                return std::nullopt;
            }
        }
        return v;
    }

    static void append_utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::optional<std::string> parse_string() {
        ++pos_;  // "
        std::string out;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') {
                return out;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return std::nullopt;  // Steuerzeichen müssen escaped sein (RFC 8259 §7)
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) {
                return std::nullopt;
            }
            const char e = text_[pos_++];
            switch (e) {
            case '"':
            case '\\':
            case '/':
                out.push_back(e);
                break;
            case 'b':
                out.push_back('\b');
                break;
            case 'f':
                out.push_back('\f');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'u': {
                const auto hi = hex4();
                if (!hi) {
                    return std::nullopt;
                }
                std::uint32_t cp = *hi;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    // Ersatzpaar: Ein hohes Surrogat ohne folgendes tiefes ist kaputt.
                    if (!take_word("\\u")) {
                        return std::nullopt;
                    }
                    const auto lo = hex4();
                    if (!lo || *lo < 0xDC00 || *lo > 0xDFFF) {
                        return std::nullopt;
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (*lo - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return std::nullopt;
                }
                append_utf8(out, cp);
                break;
            }
            default:
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parse_number() {
        // Grammatik nach RFC 8259 §6 prüfen; from_chars allein nähme auch „01“ oder „.5“.
        const std::size_t start = pos_;
        take('-');
        if (take('0')) {
            // keine führenden Nullen
        } else {
            if (pos_ >= text_.size() || text_[pos_] < '1' || text_[pos_] > '9') {
                return std::nullopt;
            }
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
                ++pos_;
            }
        }
        auto digits = [this]() {
            const std::size_t before = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
                ++pos_;
            }
            return pos_ > before;
        };
        if (take('.') && !digits()) {
            return std::nullopt;
        }
        if (take('e') || take('E')) {
            if (!take('+')) {
                take('-');
            }
            if (!digits()) {
                return std::nullopt;
            }
        }
        double value = 0.0;
        const char* first = text_.data() + start;
        const char* last = text_.data() + pos_;
        const auto [end, ec] = std::from_chars(first, last, value);
        if (ec != std::errc{} || end != last || !std::isfinite(value)) {
            return std::nullopt;
        }
        return JsonValue::make_number(value);
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

}  // namespace

const JsonValue* JsonValue::find(std::string_view key) const noexcept {
    if (type_ != Type::object) {
        return nullptr;
    }
    for (const auto& [k, v] : members_) {
        if (k == key) {
            return &v;
        }
    }
    return nullptr;
}

std::optional<std::string> JsonValue::as_string() const {
    if (type_ != Type::string) {
        return std::nullopt;
    }
    return string_;
}

std::optional<double> JsonValue::as_number() const noexcept {
    if (type_ != Type::number) {
        return std::nullopt;
    }
    return number_;
}

std::optional<int> JsonValue::as_int() const noexcept {
    if (type_ != Type::number || std::floor(number_) != number_ ||
        number_ < static_cast<double>(std::numeric_limits<int>::min()) ||
        number_ > static_cast<double>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    return static_cast<int>(number_);
}

std::optional<bool> JsonValue::as_bool() const noexcept {
    if (type_ != Type::boolean) {
        return std::nullopt;
    }
    return bool_;
}

JsonValue JsonValue::make_null() {
    return JsonValue{};
}

JsonValue JsonValue::make_bool(bool b) {
    JsonValue v;
    v.type_ = Type::boolean;
    v.bool_ = b;
    return v;
}

JsonValue JsonValue::make_number(double n) {
    JsonValue v;
    v.type_ = Type::number;
    v.number_ = n;
    return v;
}

JsonValue JsonValue::make_string(std::string s) {
    JsonValue v;
    v.type_ = Type::string;
    v.string_ = std::move(s);
    return v;
}

JsonValue JsonValue::make_array(std::vector<JsonValue> items) {
    JsonValue v;
    v.type_ = Type::array;
    v.items_ = std::move(items);
    return v;
}

JsonValue JsonValue::make_object(std::vector<std::pair<std::string, JsonValue>> members) {
    JsonValue v;
    v.type_ = Type::object;
    v.members_ = std::move(members);
    return v;
}

std::optional<JsonValue> parse_json(std::string_view text) {
    if (text.size() > kJsonMaxBytes) {
        return std::nullopt;
    }
    return Parser(text).document();
}

}  // namespace ldu
