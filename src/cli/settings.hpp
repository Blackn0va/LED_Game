// einstellungen.txt: die Streifen-Werte, damit nach dem Einrichten „LichtDuell“ reicht.
//
// Eine Zeile je Wert, „schlüssel = wert“, # leitet Kommentare ein. Gespeichert werden nur
// Werte, die zum Streifen gehören (gerät, ip, universum, segmente, versatz, leds,
// umgedreht, helligkeit, fps, ton) – Spielwerte wie Stufe oder Seed bleiben Sache der
// Kommandozeile. Fehlt „gerät“ (Dateien von vor der Herstellerwahl), gilt govee. Der
// Schlüssel geht auch ohne Umlaut (für Tastaturen ohne ä); geschrieben wird immer „gerät“.
//
// Eine kaputte Zeile ist ein Fehler mit Zeilennummer, kein stilles Überspringen: Die Datei
// wird von Hand oder per S im Testbild geschrieben, ein Tippfehler soll auffallen statt
// still mit Vorgaben zu spielen.
//
// Threads: reine Funktionen bzw. Dateizugriff im aufrufenden Thread.
// Fehler: Error(Kind::usage) mit „einstellungen.txt Zeile N: …“; Dateifehler Error(Kind::text).

#pragma once

#include "cli/options.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace ldu::cli {

void apply_settings(std::string_view text, Options& o);
std::string format_settings(const Options& o);

std::filesystem::path settings_path();
// Liest die Datei, falls vorhanden, auf `o`. false, wenn es keine gibt.
bool load_settings(const std::filesystem::path& path, Options& o);
void save_settings(const std::filesystem::path& path, const Options& o);

}  // namespace ldu::cli
