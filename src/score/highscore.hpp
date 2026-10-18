// Bestenliste: die fünf besten Spiele mit Stufe und Datum.
//
// Ablage als Textdatei (eine Zeile je Eintrag: Punkte, Stufe, Datum, durch Tabs
// getrennt) unter %LOCALAPPDATA%\StreamWizard\LichtDuell\bestenliste.txt, wie alle
// Werkzeuge des Hauses. Kaputte Zeilen werden übersprungen statt die ganze Liste zu
// verwerfen; gespeichert wird über eine .tmp-Datei und atomares Umbenennen, damit ein
// Absturz beim Schreiben nie eine halbe Liste hinterlässt.
//
// Threads: reine Funktionen bzw. Dateizugriff im aufrufenden Thread.
// Fehler: load/save/default_path werfen Error(Kind::text) mit deutschem Satz; parse und
// insert werfen nicht.

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ldu::score {

inline constexpr std::size_t kMaxEntries = 5;

struct ScoreEntry final {
    int points = 0;
    int level = 1;
    std::string date;  // JJJJ-MM-TT, Ortszeit des Spielers

    friend bool operator==(const ScoreEntry&, const ScoreEntry&) = default;
};

std::vector<ScoreEntry> parse_highscores(std::string_view text);
std::string format_highscores(std::span<const ScoreEntry> table);

// Fügt ein, sortiert absteigend, kürzt auf kMaxEntries. Liefert den Platz (0 = Erster)
// oder nullopt, wenn der Eintrag nicht in die Liste kommt. Bei Gleichstand bleibt der
// ältere Eintrag vorn.
std::optional<std::size_t> insert_score(std::vector<ScoreEntry>& table, ScoreEntry entry);

std::filesystem::path default_highscore_path();
std::vector<ScoreEntry> load_highscores(const std::filesystem::path& path);  // fehlt die Datei: leer
void save_highscores(const std::filesystem::path& path, std::span<const ScoreEntry> table);

// Heutiges Datum (Ortszeit) als JJJJ-MM-TT.
std::string today();

}  // namespace ldu::score
