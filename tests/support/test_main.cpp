// Gemeinsamer Einstieg aller Test-Programme (doctest 2.4.11).
// Eigenes main statt DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN: Die Konsole wird auf UTF-8
// gestellt, damit Testnamen und Meldungen mit Umlauten lesbar bleiben.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <windows.h>

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
