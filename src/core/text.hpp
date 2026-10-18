// UTF-8 ↔ UTF-16 an der Win32-Grenze.
//
// Intern ist jeder Text UTF-8; nur Konsole und Kommandozeile sprechen UTF-16. Beide
// Richtungen werfen bei ungültigen Folgen, statt still „?“ einzusetzen – eine
// verstümmelte IP-Adresse aus der Kommandozeile soll als Fehler auffallen.
//
// Threads: frei.
// Fehler: Error(Kind::text) bei ungültigem UTF-8/UTF-16.

#pragma once

#include <string>
#include <string_view>

namespace ldu {

std::wstring widen(std::string_view utf8);
std::string narrow(std::wstring_view utf16);

// Ganze Zahl mit deutschem Tausenderpunkt: 1240 → „1.240“, -5000 → „-5.000“.
std::string format_number(long long value);

}  // namespace ldu
