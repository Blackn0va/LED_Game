// Konsole für das Spiel: VT-Ausgabe im Alternativpuffer und rohe Tastatureingabe.
//
// Warum ReadConsoleInput statt _kbhit/_getch: Nur so kommen Taste-loslassen-Ereignisse
// an. Ohne sie gäbe eine gehaltene Taste über die Tastenwiederholung Dauerfeuer, und
// falsche Farben stapelten sich in Sekunden bis zur Kanone. So feuert jede Taste genau
// einmal je Druck.
//
// Schnellbearbeitung (QuickEdit) wird abgeschaltet: Ein Mausklick ins Fenster hielte
// sonst die ganze Ausgabe an, das Spiel liefe blind weiter. Beim Schließen stellt der
// Destruktor alle Modi, Cursor und Bildschirm wieder her.
//
// Strg+C und das Schließen des Fensters setzen quit_requested(). Beim Schließen wartet
// der Handler bis zu 2 s auf den Destruktor, damit main den Streifen noch aus dem
// Razer-Modus holen kann – Windows beendet den Prozess, sobald der Handler zurückkehrt.
//
// Threads: ein Objekt, ein Thread; nur der Strg-Handler läuft auf einem Fremdthread
// und berührt ausschließlich atomare Werte und ein Ereignis.
// Fehler: open() wirft Error(Kind::console), wenn keine Konsole da ist (Ausgabe
// umgeleitet) oder die Modi sich nicht setzen lassen.

#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace ldu::console {

struct KeyPress final {
    std::uint16_t vk = 0;  // virtueller Tastencode (VK_…)
};

class Console final {
public:
    static std::unique_ptr<Console> open();
    ~Console();
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

    void write(std::string_view utf8);
    int width() const;

    // Neu gedrückte Tasten seit dem letzten Aufruf; gehaltene Tasten zählen einmal.
    std::vector<KeyPress> poll_keys();

    // true, wenn sich die Fenstergröße geändert hat (dann einmal ganz neu zeichnen).
    bool take_resized();

    // Wartet bis zu timeout_ms auf Eingabe.
    void wait_input(int timeout_ms);

    bool quit_requested() const;

private:
    struct Impl;
    explicit Console(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

// Schreibt UTF-8 auf die Standardausgabe – in eine Konsole als UTF-16, in eine
// Umleitung als UTF-8-Bytes. Für Hilfe, Suche und Fehlermeldungen außerhalb des Spiels.
void print(std::string_view utf8);

}  // namespace ldu::console
