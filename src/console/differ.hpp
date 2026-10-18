// Schreibt vom Bildschirm nur die Zeilen neu, die sich geändert haben.
//
// Gemessen 2026-10-18 (Demo, 15 s, klassisches Konsolenfenster): Das Spiel selbst brauchte
// 0,9–1,6 % eines Kerns, conhost fürs Zeichnen aber 7,4–8,6 % – bei jedem Bild ging der
// ganze Bildschirm samt Rahmen und Tastenhilfe neu hinaus. Rahmen, Hilfe und Fußzeile ändern
// sich fast nie; nur Streifen, Kopf und Meldung müssen neu.
//
// Eingang ist der fertige Bildschirm aus render_screen/render_calibration (\x1b[H vorn,
// Zeilen mit \x1b[K\n, \x1b[J am Ende). Ausgang sind Cursor-Sprünge plus geänderte Zeilen;
// leer, wenn sich nichts geändert hat.
//
// Threads: ein Objekt, ein Thread.
// Fehler: keine.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ldu::console {

class ScreenDiffer final {
public:
    std::string update(std::string_view full_screen);
    // Nach einer Größenänderung: alles beim nächsten Mal neu schreiben.
    void reset();

private:
    std::vector<std::string> lines_;
};

}  // namespace ldu::console
