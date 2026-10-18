// Bestenliste: Lesen, Schreiben, Einsortieren, Dateiablage (nur im Bauordner).

#include "core/error.hpp"
#include "score/highscore.hpp"

#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ldu::score {
namespace {

// Eigener Ordner unter dem Testausgabeordner – kein %LOCALAPPDATA%, kein Netz.
std::filesystem::path temp_dir(const char* name) {
    const std::filesystem::path dir = std::filesystem::path(LDU_TEST_BINARY_DIR) / "tmp" / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

}  // namespace

TEST_CASE("Bestenliste: Text wird gelesen, sortiert und auf fünf gekürzt, kaputte Zeilen fallen weg") {
    const std::string text = "100\t1\t2026-10-01\n"
                             "kaputt\n"
                             "900\t4\t2026-10-02\r\n"
                             "300\t2\t2026-10-03\n"
                             "-5\t1\t2026-10-04\n"
                             "200\t0\t2026-10-05\n"
                             "400\t3\t26-10-06\n"
                             "500\t3\t2026-10-07\n"
                             "600\t3\t2026-10-08\n"
                             "700\t3\t2026-10-09\n";
    const auto table = parse_highscores(text);
    REQUIRE(table.size() == 5);
    CHECK(table[0] == ScoreEntry{900, 4, "2026-10-02"});
    CHECK(table[4].points == 300);
    CHECK(parse_highscores(format_highscores(table)) == table);
}

TEST_CASE("Bestenliste: Einsortieren liefert den Platz, Gleichstand lässt den Älteren vorn") {
    std::vector<ScoreEntry> table{{500, 3, "2026-10-01"}, {300, 2, "2026-10-02"}};
    CHECK(insert_score(table, {400, 3, "2026-10-03"}) == std::optional<std::size_t>(1));
    CHECK(insert_score(table, {500, 4, "2026-10-04"}) == std::optional<std::size_t>(1));
    CHECK(table[0].date == "2026-10-01");
    CHECK(insert_score(table, {0, 1, "2026-10-05"}) == std::nullopt);
    CHECK(insert_score(table, {10, 1, "2026-10-06"}) == std::optional<std::size_t>(4));
    REQUIRE(table.size() == 5);
    CHECK(insert_score(table, {5, 1, "2026-10-07"}) == std::nullopt);
    CHECK(insert_score(table, {9999, 9, "2026-10-08"}) == std::optional<std::size_t>(0));
    CHECK(table.size() == 5);
    CHECK(table.back().points == 300);
}

TEST_CASE("Bestenliste: Speichern und Laden ergeben dieselbe Liste, ohne .tmp-Rest; fehlende Datei = leer") {
    const auto dir = temp_dir("bestenliste");
    const auto file = dir / "unter" / "bestenliste.txt";
    CHECK(load_highscores(file).empty());
    const std::vector<ScoreEntry> table{{1240, 5, "2026-10-18"}, {90, 1, "2026-10-17"}};
    save_highscores(file, table);
    CHECK(load_highscores(file) == table);
    CHECK_FALSE(std::filesystem::exists(dir / "unter" / "bestenliste.txt.tmp"));
    const std::vector<ScoreEntry> second{{5000, 9, "2026-10-19"}};
    save_highscores(file, second);
    CHECK(load_highscores(file) == second);
}

TEST_CASE("Bestenliste: Datum hat die Form JJJJ-MM-TT, Ablage liegt unter StreamWizard\\LichtDuell") {
    const std::string d = today();
    REQUIRE(d.size() == 10);
    CHECK(d[4] == '-');
    CHECK(d[7] == '-');
    const auto path = default_highscore_path();
    CHECK(path.filename() == "bestenliste.txt");
    CHECK(path.parent_path().filename() == "LichtDuell");
    CHECK(path.parent_path().parent_path().filename() == "StreamWizard");
}

}  // namespace ldu::score
