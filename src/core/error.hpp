// Ausnahme mit deutschem Nutzertext.
//
// Alles, was bis zu main durchschlägt, landet als ein Satz vor dem Nutzer. Kind trennt
// Bedienfehler (falsche Option) von Umgebungsfehlern (Netz, Konsole), damit main den
// passenden Hinweis geben kann, ohne Meldungstexte zu vergleichen.
//
// Threads: reine Werte.
// Fehler: keine eigenen.

#pragma once

#include <stdexcept>
#include <string>

namespace ldu {

class Error final : public std::runtime_error {
public:
    enum class Kind { usage, network, console, protocol, text };

    Error(Kind kind, const std::string& message) : std::runtime_error(message), kind_(kind) {}

    Kind kind() const noexcept {
        return kind_;
    }

private:
    Kind kind_;
};

}  // namespace ldu
