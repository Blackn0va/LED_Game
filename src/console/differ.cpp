#include "console/differ.hpp"

namespace ldu::console {
namespace {

constexpr std::string_view kHome = "\x1b[H";
constexpr std::string_view kTail = "\x1b[0m\x1b[J";

std::vector<std::string> split_lines(std::string_view s) {
    if (s.starts_with(kHome)) {
        s.remove_prefix(kHome.size());
    }
    if (s.ends_with(kTail)) {
        s.remove_suffix(kTail.size());
    }
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < s.size()) {
        std::size_t end = s.find('\n', start);
        if (end == std::string_view::npos) {
            end = s.size();
        }
        lines.emplace_back(s.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

}  // namespace

std::string ScreenDiffer::update(std::string_view full_screen) {
    std::vector<std::string> next = split_lines(full_screen);
    std::string out;
    for (std::size_t i = 0; i < next.size(); ++i) {
        if (i < lines_.size() && lines_[i] == next[i]) {
            continue;
        }
        out += "\x1b[" + std::to_string(i + 1) + ";1H" + next[i];
    }
    if (next.size() < lines_.size()) {
        // Bild ist kürzer geworden (etwa Bestenliste weg): Rest darunter löschen.
        out += "\x1b[" + std::to_string(next.size() + 1) + ";1H\x1b[0m\x1b[J";
    }
    lines_ = std::move(next);
    return out;
}

void ScreenDiffer::reset() {
    lines_.clear();
}

}  // namespace ldu::console
