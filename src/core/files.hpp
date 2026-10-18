// Datenordner und kleine Textdateien (Bestenliste, Einstellungen).
//
// Ablage unter %LOCALAPPDATA%\StreamWizard\LichtDuell wie bei allen Werkzeugen des Hauses.
// Geschrieben wird über eine .tmp-Datei und atomares Umbenennen: Ein Absturz mitten im
// Schreiben hinterlässt nie eine halbe Datei.
//
// FALLE (CLAUDE.md, Claude-App als MSIX): Legt ein Prozess aus der App heraus den Ordner
// NEU an, landet er virtualisiert in ...\Packages\Claude_…\LocalCache und ist für normale
// Terminals unsichtbar. Schreiben in einen schon vorhandenen Ordner ist echt.
//
// Threads: frei.
// Fehler: Error(Kind::text) mit deutschem Satz, wenn Ordner oder Datei nicht gehen.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ldu {

std::filesystem::path app_data_dir();

// Inhalt der Datei; nullopt, wenn sie fehlt. Wirft, wenn sie da, aber nicht lesbar ist.
std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string_view what);

// Legt den Ordner bei Bedarf an und ersetzt die Datei atomar. `what` nennt die Datei in
// der Fehlermeldung („Bestenliste“, „Einstellungen“).
void write_text_file_atomic(const std::filesystem::path& path, std::string_view text, std::string_view what);

}  // namespace ldu
