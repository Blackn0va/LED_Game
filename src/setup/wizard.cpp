#include "setup/wizard.hpp"

#include <algorithm>
#include <charconv>

namespace ldu::setup {
namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.remove_suffix(1);
    }
    return s;
}

}  // namespace

Choice parse_choice(std::string_view line, std::size_t found_count) {
    const std::string_view s = trim(line);
    Choice c;
    if (s.empty()) {
        if (found_count > 0) {
            c.kind = Choice::Kind::device;
            c.index = 0;
        }
        return c;
    }
    if (s.size() == 1) {
        switch (s.front()) {
        case 's':
        case 'S':
            c.kind = Choice::Kind::sacn_multicast;
            return c;
        case 'i':
        case 'I':
            c.kind = Choice::Kind::manual;
            return c;
        case 'q':
        case 'Q':
        case 'x':
        case 'X':
            c.kind = Choice::Kind::quit;
            return c;
        default:
            break;
        }
    }
    std::size_t n = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (ec == std::errc{} && end == s.data() + s.size() && n >= 1 && n <= found_count) {
        c.kind = Choice::Kind::device;
        c.index = n - 1;
    }
    return c;
}

std::vector<std::string> menu_lines(const std::vector<devices::Found>& found) {
    std::vector<std::string> lines;
    for (std::size_t i = 0; i < found.size(); ++i) {
        const std::string number = std::to_string(i + 1);
        lines.push_back(std::string(number.size() < 2 ? "   " : "  ") + number + "  " + devices::found_line(found[i]));
    }
    lines.push_back("   s  sACN (E1.31) per Multicast – Empfänger melden sich nicht, Universum 1");
    lines.push_back("   i  Gerät von Hand angeben (Art und IP)");
    lines.push_back("   q  abbrechen");
    return lines;
}

void apply_found(cli::Options& o, const devices::Found& f) {
    const bool same_device = o.device == f.type && o.ip == f.ip;
    o.device = f.type;
    o.ip = f.ip;
    if (lights::uses_universe(f.type)) {
        o.universe = f.universe.value_or(o.universe.value_or(lights::default_universe(f.type)));
    } else {
        o.universe.reset();
    }
    if (!same_device) {
        o.segments = 0;
        o.skip = 0;
        o.zone = 0;
        o.leds = cli::kDefaultLeds;
        o.leds_given = false;
        o.reversed = false;
    }
    if (f.type == lights::DeviceType::wled && f.leds > 0) {
        const int units = std::min(f.leds, lights::max_units(f.type));
        if (units - o.skip - o.zone >= cli::kMinLeds) {
            o.segments = units;
        }
    }
}

}  // namespace ldu::setup
