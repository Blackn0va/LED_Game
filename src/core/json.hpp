// Kleiner JSON-Leser für Geräteantworten (WLED /json/info).
//
// Die Govee-Antworten sind flach und kommen mit drei Feldern aus (govee/protocol.cpp
// sucht dort gezielt). WLED liefert dagegen ein verschachteltes Objekt, in dem „count“
// unter „leds“ steht und Namen wie „name“ auch in Unterobjekten vorkommen können – eine
// Textsuche griffe dort das falsche Feld. Deshalb hier ein vollständiger, kleiner Leser
// statt einer Fremdbibliothek für ein einziges Objekt.
//
// Eingaben kommen aus dem Netz und sind nicht vertrauenswürdig: Schachtelungstiefe und
// Länge sind begrenzt, Steuerzeichen in Zeichenketten und kaputte \u-Folgen werden
// abgelehnt statt geraten (OWASP A03, ASVS V5.1.4).
//
// Threads: reine Funktionen und Werte.
// Fehler: parse_json liefert nullopt bei jedem Formfehler; wirft nur bei Speichermangel.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ldu {

inline constexpr int kJsonMaxDepth = 32;
inline constexpr std::size_t kJsonMaxBytes = 1024 * 1024;

class JsonValue final {
public:
    enum class Type { null, boolean, number, string, array, object };

    Type type() const noexcept {
        return type_;
    }

    bool is_object() const noexcept {
        return type_ == Type::object;
    }

    // Wert unter `key` in einem Objekt, sonst nullptr (auch wenn this kein Objekt ist).
    const JsonValue* find(std::string_view key) const noexcept;

    // nullopt, wenn der Typ nicht passt.
    std::optional<std::string> as_string() const;
    std::optional<double> as_number() const noexcept;
    // Nur ganze Zahlen im int-Bereich, sonst nullopt.
    std::optional<int> as_int() const noexcept;
    std::optional<bool> as_bool() const noexcept;

    static JsonValue make_null();
    static JsonValue make_bool(bool b);
    static JsonValue make_number(double n);
    static JsonValue make_string(std::string s);
    static JsonValue make_array(std::vector<JsonValue> items);
    static JsonValue make_object(std::vector<std::pair<std::string, JsonValue>> members);

private:
    Type type_ = Type::null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<JsonValue> items_;
    std::vector<std::pair<std::string, JsonValue>> members_;
};

// Liest genau einen JSON-Wert (Leerraum davor und danach erlaubt). nullopt bei Formfehlern,
// zu tiefer Schachtelung oder mehr als kJsonMaxBytes.
std::optional<JsonValue> parse_json(std::string_view text);

}  // namespace ldu
