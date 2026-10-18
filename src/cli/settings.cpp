#include "cli/settings.hpp"

#include "core/error.hpp"
#include "core/files.hpp"

namespace ldu::cli {
namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

bool parse_yes_no(std::string_view value, const std::string& where) {
    if (value == "ja" || value == "an") {
        return true;
    }
    if (value == "nein" || value == "aus") {
        return false;
    }
    throw Error(Error::Kind::usage, where + "erwartet ja/nein bzw. an/aus, nicht „" + std::string(value) + "“.");
}

}  // namespace

void apply_settings(std::string_view text, Options& o) {
    int line_no = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        ++line_no;
        std::string_view line = trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty() || line.front() == '#') {
            if (end == text.size()) {
                break;
            }
            continue;
        }
        const std::string where = "einstellungen.txt Zeile " + std::to_string(line_no) + ": ";
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            throw Error(Error::Kind::usage, where + "„schlüssel = wert“ erwartet, nicht „" + std::string(line) + "“.");
        }
        const std::string_view key = trim(line.substr(0, eq));
        const std::string_view value = trim(line.substr(eq + 1));
        const std::string what = where + std::string(key);
        if (key == "ip") {
            if (value.empty()) {
                throw Error(Error::Kind::usage, what + " ist leer.");
            }
            o.ip = std::string(value);
        } else if (key == "leds") {
            o.leds = parse_bounded(what, value, kMinLeds, kMaxLeds);
            o.leds_given = true;
        } else if (key == "segmente") {
            o.segments = parse_bounded(what, value, kMinLeds, kMaxLeds);
        } else if (key == "versatz") {
            o.skip = parse_bounded(what, value, 0, kMaxLeds - kMinLeds);
        } else if (key == "effektbereich") {
            o.zone = parse_bounded(what, value, 0, kMaxLeds - kMinLeds);
        } else if (key == "helligkeit") {
            o.brightness = parse_bounded(what, value, 1, 100);
        } else if (key == "fps") {
            o.fps = parse_bounded(what, value, kMinFps, kMaxFps);
        } else if (key == "umgedreht") {
            o.reversed = parse_yes_no(value, what + " ");
        } else if (key == "ton") {
            o.mute = !parse_yes_no(value, what + " ");
        } else {
            throw Error(Error::Kind::usage, where + "unbekannter Schlüssel „" + std::string(key) + "“.");
        }
        if (end == text.size()) {
            break;
        }
    }
}

std::string format_settings(const Options& o) {
    std::string s = "# LichtDuell – Einstellungen. Von Hand änderbar; „LichtDuell --testbild“ und S schreiben sie neu.\n"
                    "# Die Kommandozeile überstimmt jeden Wert hier.\n";
    if (!o.ip.empty()) {
        s += "ip = " + o.ip + "\n";
    }
    if (o.segments > 0) {
        s += "segmente = " + std::to_string(o.segments) + "\n";
    }
    s += "versatz = " + std::to_string(o.skip) + "\n";
    s += "effektbereich = " + std::to_string(o.zone) + "\n";
    if (o.leds_given) {
        s += "leds = " + std::to_string(o.leds) + "\n";
    }
    s += std::string("umgedreht = ") + (o.reversed ? "ja" : "nein") + "\n";
    s += "helligkeit = " + std::to_string(o.brightness) + "\n";
    s += "fps = " + std::to_string(o.fps) + "\n";
    s += std::string("ton = ") + (o.mute ? "aus" : "an") + "\n";
    return s;
}

std::filesystem::path settings_path() {
    return app_data_dir() / L"einstellungen.txt";
}

bool load_settings(const std::filesystem::path& path, Options& o) {
    const auto text = read_text_file(path, "Einstellungen");
    if (!text) {
        return false;
    }
    apply_settings(*text, o);
    return true;
}

void save_settings(const std::filesystem::path& path, const Options& o) {
    write_text_file_atomic(path, format_settings(o), "Einstellungen");
}

}  // namespace ldu::cli
