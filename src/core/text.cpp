#include "core/text.hpp"

#include "core/error.hpp"

#include <windows.h>

#include <limits>

namespace ldu {

std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw Error(Error::Kind::text, "Text ist zu lang für die Umwandlung nach UTF-16.");
    }
    const int in_len = static_cast<int>(utf8.size());
    const int out_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), in_len, nullptr, 0);
    if (out_len <= 0) {
        throw Error(Error::Kind::text, "Ungültiges UTF-8 im Text.");
    }
    std::wstring out(static_cast<std::size_t>(out_len), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), in_len, out.data(), out_len);
    return out;
}

std::string narrow(std::wstring_view utf16) {
    if (utf16.empty()) {
        return {};
    }
    if (utf16.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw Error(Error::Kind::text, "Text ist zu lang für die Umwandlung nach UTF-8.");
    }
    const int in_len = static_cast<int>(utf16.size());
    const int out_len =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, utf16.data(), in_len, nullptr, 0, nullptr, nullptr);
    if (out_len <= 0) {
        throw Error(Error::Kind::text, "Ungültiges UTF-16 im Text.");
    }
    std::string out(static_cast<std::size_t>(out_len), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, utf16.data(), in_len, out.data(), out_len, nullptr, nullptr);
    return out;
}

std::string format_number(long long value) {
    // Über unsigned gerechnet: -LLONG_MIN liefe in long long über.
    const bool negative = value < 0;
    unsigned long long magnitude =
        negative ? 0ULL - static_cast<unsigned long long>(value) : static_cast<unsigned long long>(value);
    const std::string digits = std::to_string(magnitude);
    std::string out;
    out.reserve(digits.size() + digits.size() / 3 + 1);
    const std::size_t lead = digits.size() % 3;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        // i >= lead zuerst: sonst liefe i - lead als size_t über.
        if (i != 0 && i >= lead && (i - lead) % 3 == 0) {
            out.push_back('.');
        }
        out.push_back(digits[i]);
    }
    return negative ? "-" + out : out;
}

}  // namespace ldu
