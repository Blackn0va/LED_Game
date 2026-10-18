// Einrichtungs-Assistent („LichtDuell --einrichten“): die Teile ohne Netz und Konsole.
//
// Ablauf in app/main.cpp: alle Hersteller suchen → nummerierte Liste → Auswahl lesen →
// Werte übernehmen und speichern → Testbild zum Einstellen von Versatz und Effektbereich.
// Hier stehen nur Auswahl-Lesen und Übernehmen, damit beides ohne Gerät testbar ist.
//
// Übernehmen setzt die streifenbezogenen Werte (Abschnitte, Versatz, Effektbereich,
// Spielzellen, umgedreht) zurück, wenn ein ANDERES Gerät gewählt wird – sonst trüge ein
// neuer 150-LED-WLED-Streifen den Versatz des alten Govee-Streifens. Helligkeit, fps und
// Ton bleiben, die gehören zum Geschmack, nicht zum Streifen.
//
// Threads: reine Funktionen.
// Fehler: keine Würfe; unbrauchbare Eingaben liefern Choice::Kind::invalid.

#pragma once

#include "cli/options.hpp"
#include "devices/connect.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ldu::setup {

struct Choice final {
    enum class Kind { device, sacn_multicast, manual, quit, invalid };
    Kind kind = Kind::invalid;
    std::size_t index = 0;  // bei device: 0-basiert in der Fundliste
};

// "3" → drittes Gerät, "s" → sACN-Multicast, "i" → von Hand, "q"/"x" → abbrechen,
// leer → erstes Gerät (gibt es keins: invalid). Groß/klein egal, Leerraum ringsum egal.
Choice parse_choice(std::string_view line, std::size_t found_count);

// Die Zeilen der Auswahlliste, ohne Kopf und Eingabeaufforderung.
std::vector<std::string> menu_lines(const std::vector<devices::Found>& found);

// Übernimmt das Gerät in die Optionen (siehe Kopf zum Zurücksetzen). WLED mit gemeldeter
// LED-Zahl bekommt sie als Abschnitte, wenn das Kampffeld dann mindestens kMinLeds hat.
void apply_found(cli::Options& o, const devices::Found& f);

}  // namespace ldu::setup
