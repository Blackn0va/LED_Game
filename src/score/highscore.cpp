#include "score/highscore.hpp"

#include "core/files.hpp"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <iterator>

namespace ldu::score {
namespace {

std::optional<int> to_int(std::string_view text) {
    int value = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

bool valid_date(std::string_view d) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') {
        return false;
    }
    for (std::size_t i = 0; i < d.size(); ++i) {
        if (i != 4 && i != 7 && (d[i] < '0' || d[i] > '9')) {
            return false;
        }
    }
    return true;
}

void sort_table(std::vector<ScoreEntry>& table) {
    std::stable_sort(table.begin(), table.end(),
                     [](const ScoreEntry& a, const ScoreEntry& b) { return a.points > b.points; });
    if (table.size() > kMaxEntries) {
        table.resize(kMaxEntries);
    }
}

}  // namespace

std::vector<ScoreEntry> parse_highscores(std::string_view text) {
    std::vector<ScoreEntry> table;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        const std::size_t t1 = line.find('\t');
        const std::size_t t2 = t1 == std::string_view::npos ? t1 : line.find('\t', t1 + 1);
        if (t2 == std::string_view::npos) {
            continue;
        }
        const auto points = to_int(line.substr(0, t1));
        const auto level = to_int(line.substr(t1 + 1, t2 - t1 - 1));
        const std::string_view date = line.substr(t2 + 1);
        if (!points || !level || *points < 0 || *level < 1 || !valid_date(date)) {
            continue;
        }
        table.push_back(ScoreEntry{*points, *level, std::string(date)});
    }
    sort_table(table);
    return table;
}

std::string format_highscores(std::span<const ScoreEntry> table) {
    std::string out;
    for (const ScoreEntry& e : table) {
        out += std::to_string(e.points) + "\t" + std::to_string(e.level) + "\t" + e.date + "\n";
    }
    return out;
}

std::optional<std::size_t> insert_score(std::vector<ScoreEntry>& table, ScoreEntry entry) {
    if (entry.points <= 0) {
        return std::nullopt;
    }
    // Hinter allen Gleichständen einsortieren: Wer zuerst so viel hatte, bleibt vorn.
    const auto pos = std::find_if(table.begin(), table.end(),
                                  [&entry](const ScoreEntry& e) { return e.points < entry.points; });
    const auto rank = static_cast<std::size_t>(std::distance(table.begin(), pos));
    if (rank >= kMaxEntries) {
        return std::nullopt;
    }
    table.insert(pos, std::move(entry));
    if (table.size() > kMaxEntries) {
        table.resize(kMaxEntries);
    }
    return rank;
}

std::filesystem::path default_highscore_path() {
    return app_data_dir() / L"bestenliste.txt";
}

std::vector<ScoreEntry> load_highscores(const std::filesystem::path& path) {
    const auto text = read_text_file(path, "Bestenliste");
    return text ? parse_highscores(*text) : std::vector<ScoreEntry>{};
}

void save_highscores(const std::filesystem::path& path, std::span<const ScoreEntry> table) {
    write_text_file_atomic(path, format_highscores(table), "Bestenliste");
}

std::string today() {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    char buf[16] = {};
    std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u", static_cast<unsigned>(t.wYear), static_cast<unsigned>(t.wMonth),
                  static_cast<unsigned>(t.wDay));
    return buf;
}

}  // namespace ldu::score
