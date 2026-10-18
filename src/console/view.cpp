#include "console/view.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <string_view>

namespace ldu::console {
namespace {

inline constexpr Rgb kMuted{120, 120, 120};
inline constexpr Rgb kFrame{90, 70, 150};
inline constexpr Rgb kUnlit{38, 38, 46};  // unbeleuchtete LED: dunkler Block statt Lücke
inline constexpr Rgb kHeart{230, 40, 60};
inline constexpr Rgb kAccent{139, 92, 246};
inline constexpr Rgb kBossColor{255, 90, 40};
inline constexpr Rgb kJokerColor{255, 215, 80};
inline constexpr int kBossBarMax = 20;
inline constexpr int kMinInner = 40;
inline constexpr int kMaxInner = 104;  // breiter wirkt das Bild auf großen Fenstern verloren
inline constexpr int kMaxCellWidth = 4;
inline constexpr int kZoneCellWidth = 2;
inline constexpr int kZoneGap = 2;

inline constexpr const char* kReset = "\x1b[0m";
inline constexpr const char* kBold = "\x1b[1m";

std::string fg(Rgb c) {
    return "\x1b[38;2;" + std::to_string(static_cast<int>(c.r)) + ";" + std::to_string(static_cast<int>(c.g)) + ";" +
           std::to_string(static_cast<int>(c.b)) + "m";
}

std::string repeat(std::string_view s, int n) {
    std::string out;
    for (int i = 0; i < n; ++i) {
        out += s;
    }
    return out;
}

std::string spaces(int n) {
    return std::string(static_cast<std::size_t>(std::max(n, 0)), ' ');
}

// Bildschirm mit Rahmen: jede Zeile wird auf dieselbe sichtbare Breite aufgefüllt, damit
// der rechte Rand gerade steht – egal wie viele Farbfolgen in der Zeile stecken.
class Screen final {
public:
    explicit Screen(int console_width) : inner_(std::clamp(console_width - 4, kMinInner, kMaxInner)) {
        out_.reserve(8192);
        out_ += "\x1b[H";
    }

    int inner() const noexcept {
        return inner_;
    }

    void top(std::string_view title, std::string_view right) {
        // ╔═ TITEL ══…══ rechts ═╗
        const std::string t = " " + std::string(title) + " ";
        const std::string r = right.empty() ? std::string() : " " + std::string(right) + " ";
        const int fill = inner_ + 2 - 1 - visible_width(t) - visible_width(r) - 1;
        out_ += fg(kFrame) + "╔═" + kReset + fg(kAccent) + kBold + t + kReset + fg(kFrame) + repeat("═", fill) +
                kReset + r + fg(kFrame) + "═╗" + kReset;
        end();
    }

    void row(const std::string& content) {
        out_ += fg(kFrame) + "║" + kReset + " " + content + kReset + spaces(inner_ - visible_width(content)) + " " +
                fg(kFrame) + "║" + kReset;
        end();
    }

    // Links und rechts ausgerichtet in einer Zeile.
    void row(const std::string& left, const std::string& right) {
        row(left + spaces(inner_ - visible_width(left) - visible_width(right)) + right);
    }

    void sep() {
        out_ += fg(kFrame) + "╟" + repeat("─", inner_ + 2) + "╢" + kReset;
        end();
    }

    void bottom() {
        out_ += fg(kFrame) + "╚" + repeat("═", inner_ + 2) + "╝" + kReset;
        end();
    }

    std::string finish() {
        out_ += "\x1b[0m\x1b[J";
        return std::move(out_);
    }

private:
    void end() {
        out_ += "\x1b[K\n";
    }

    int inner_;
    std::string out_;
};

// Spielfarbe als Schriftfarbe: aufgehellt, Blau am stärksten – reines (0,0,255) war im
// klassischen Konsolenfenster auf Schwarz kaum lesbar (Bildschirmfoto 2026-10-18). Am
// Streifen bleiben die reinen Farben.
Rgb text_color(Color c) {
    return blend(to_rgb(c), kWhite, c == Color::blue ? 0.45 : 0.15);
}

std::string cell(Rgb c, int width) {
    return fg(c == kBlack ? kUnlit : c) + repeat("█", width);
}

// Streifen zweizeilig plus Beschriftung; der Effektbereich hängt wie an der Wand hinter
// dem Computerende in derselben Zeile.
void strip_rows(Screen& s, std::span<const Rgb> field, std::span<const Rgb> zone, bool labels) {
    const int cells = std::max(1, static_cast<int>(field.size()));
    const int zone_cols = zone.empty() ? 0 : static_cast<int>(zone.size()) * kZoneCellWidth + kZoneGap;
    const int cell_w = std::clamp((s.inner() - zone_cols) / cells, 1, kMaxCellWidth);
    std::string line;
    for (const Rgb& c : field) {
        line += cell(c, cell_w);
    }
    if (!zone.empty()) {
        line += spaces(kZoneGap);
        for (const Rgb& c : zone) {
            line += cell(c, kZoneCellWidth);
        }
    }
    s.row(line);
    s.row(line);
    if (!labels) {
        return;
    }
    const int field_cols = static_cast<int>(field.size()) * cell_w;
    const std::string left = "▲ Kanone";
    const std::string right = "Computer ▲";
    std::string label = fg(kMuted) + left;
    label += spaces(field_cols - visible_width(left) - visible_width(right)) + right;
    if (!zone.empty()) {
        label += spaces(kZoneGap) + (zone.size() * kZoneCellWidth >= 7 ? "Effekte" : "Eff.");
    }
    s.row(label);
}

}  // namespace

int visible_width(std::string_view text) {
    int width = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == 0x1B && i + 1 < text.size()) {
            const char kind = text[i + 1];
            i += 2;
            if (kind == '[') {
                // CSI: bis zum Abschlusszeichen 0x40–0x7E
                while (i < text.size() && !(text[i] >= 0x40 && text[i] <= 0x7E)) {
                    ++i;
                }
            } else if (kind == ']') {
                // OSC: bis BEL
                while (i < text.size() && text[i] != '\x07') {
                    ++i;
                }
            }
            continue;
        }
        if ((c & 0xC0) != 0x80) {
            ++width;
        }
    }
    return width;
}

std::string render_screen(std::span<const Rgb> frame, const HudInfo& hud, int console_width) {
    Screen s(console_width);
    s.top(hud.mode_label.empty() ? "LICHTDUELL" : "LICHTDUELL · " + hud.mode_label,
          "Stufe " + std::to_string(hud.level));

    // Kopfzeile: Punkte links, Leben/Joker/Kombo rechts.
    std::string hearts;
    for (int i = 0; i < std::max(hud.lives, hud.base_lives); ++i) {
        hearts += (i < hud.lives ? fg(kHeart) : fg(kUnlit)) + "♥";
    }
    std::string jokers;
    for (int i = 0; i < hud.max_jokers; ++i) {
        jokers += (i < hud.jokers ? fg(kJokerColor) : fg(kUnlit)) + "♦";
    }
    std::string right = "Leben " + hearts + kReset + "   Joker " + jokers + kReset;
    if (hud.combo >= 2) {
        right += "   Kombo " + std::to_string(hud.combo);
        if (hud.multiplier > 1) {
            right += " " + fg(kJokerColor) + kBold + "×" + std::to_string(hud.multiplier) + kReset;
        }
    }
    s.row(std::string(kBold) + "Punkte " + format_number(hud.score) + kReset, right);
    s.sep();

    s.row("");
    strip_rows(s, frame, hud.zone, true);
    s.row("");
    s.sep();

    // Endgegner: zwei Zeilen, auch wenn keiner da ist – das Bild springt sonst.
    if (hud.boss) {
        const BossBar& b = *hud.boss;
        if (b.warning) {
            s.row(fg(kBossColor) + kBold + "ACHTUNG – Endgegner naht: " + b.name);
        } else {
            const int max = std::max(1, b.max_segments);
            const int width = std::min(max, kBossBarMax);
            const int filled = std::clamp((std::min(b.segments, max) * width + max - 1) / max, 0, width);
            std::string line = fg(kBossColor) + kBold + "ENDGEGNER " + b.name + kReset + "  " + fg(kBossColor) +
                               repeat("█", filled) + fg(kMuted) + repeat("░", width - filled) + kReset + "  " +
                               std::to_string(b.segments) + "/" + std::to_string(b.max_segments);
            if (b.enraged) {
                line += "  " + fg(kBossColor) + kBold + "WÜTEND!";
            }
            s.row(line);
        }
        s.row(fg(kMuted) + b.hint);
    } else {
        s.row(fg(kMuted) + "Kein Endgegner – der nächste wartet auf Stufe " +
              std::to_string(((hud.level / 3) + 1) * 3) + ".");
        s.row("");
    }
    s.sep();

    static constexpr const char* kLetters[kColorCount] = {"R", "G", "B", "Y", "L"};
    std::string colors;
    for (int i = 0; i < kColorCount; ++i) {
        const auto col = static_cast<Color>(i);
        const bool active = i < hud.colors_in_play;
        if (i > 0) {
            colors += "  ";
        }
        colors += (active ? fg(text_color(col)) + kBold : fg(kMuted)) + "[" + std::to_string(i + 1) + "/" + kLetters[i] +
                  "] " + std::string(color_name(col)) + kReset;
        if (!active) {
            colors += fg(kMuted) + " (ab " + std::to_string(hud.unlock_level[static_cast<std::size_t>(i)]) + ")" +
                      kReset;
        }
    }
    s.row(colors);
    if (hud.auto_mode) {
        s.row(fg(kAccent) + kBold + "[A] selbst spielen   [← / →] Tempo" + kReset + fg(kMuted) +
              "   [P] Pause   [Enter] neue Partie   [Esc] Ende");
        s.row(fg(kMuted) + "Der Computer spielt allein – zurücklehnen und zuschauen.");
    } else {
        s.row((hud.jokers > 0 ? fg(kJokerColor) : fg(kMuted)) + "[Leertaste] Joker" + kReset + fg(kAccent) +
              "   [A] Computer spielt" + kReset + fg(kMuted) + "   [P] Pause   [Esc] Ende");
        s.row(fg(kMuted) + "Gleiche Farbe löst den vordersten Punkt, falsche steckt sich an.");
    }
    s.sep();

    if (hud.over) {
        s.row(fg(kHeart) + kBold + "Spiel vorbei – " + format_number(hud.score) + " Punkte, Stufe " +
              std::to_string(hud.level) + ".");
        s.row(fg(kMuted) + "Enter: neues Spiel · Esc: Ende");
        for (const std::string& line : hud.highscore_lines) {
            s.row(line);
        }
    } else if (hud.paused) {
        s.row(std::string(kBold) + "PAUSE" + kReset + " – P setzt fort");
        s.row(hud.info_line.empty() ? std::string() : fg(kMuted) + hud.info_line);
    } else {
        s.row(hud.message.empty() ? std::string() : fg(hud.message_color) + kBold + "► " + hud.message);
        s.row(hud.info_line.empty() ? std::string() : fg(kMuted) + hud.info_line);
    }
    s.sep();
    s.row(fg(kMuted) + hud.stats_line);
    s.row(fg(kMuted) + hud.link_status);
    s.bottom();
    return s.finish();
}

std::string render_calibration(std::span<const Rgb> frame, std::span<const std::string> lines, int console_width) {
    Screen s(console_width);
    s.top("LICHTDUELL · TESTBILD", "");
    s.row("");
    strip_rows(s, frame, {}, false);
    s.row("");
    s.sep();
    for (const std::string& line : lines) {
        s.row(line);
    }
    s.bottom();
    return s.finish();
}

}  // namespace ldu::console
