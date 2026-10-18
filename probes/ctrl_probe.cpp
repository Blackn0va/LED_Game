// Schließen-Probe: Bekommt ein Konsolenprogramm beim Schließen des Fensters noch Zeit?
//
// Registriert einen Strg-Handler und schreibt in <datei>, wann welches Ereignis kam. Beim
// Schließen-Ereignis (CTRL_CLOSE_EVENT) schreibt der Handler alle 250 ms „lebe noch“, bis
// Windows den Prozess beendet – die letzte Zeile zeigt, wie lange das Aufräumen dauern
// darf. Kommt gar kein Ereignis, wurde der Prozess hart beendet.
//
//   ldu_probe_ctrl <datei>
//
// Anlass (2026-10-18): Beim Schließen des Terminal-Tabs kam beim Lauscher keine einzige
// Razer-Aus-Nachricht an; bei Strg+C schon.

#include <windows.h>

#include <string>

namespace {

HANDLE g_file = INVALID_HANDLE_VALUE;
ULONGLONG g_t0 = 0;

void log_line(const std::string& text) {
    const std::string line = std::to_string(GetTickCount64() - g_t0) + " ms  " + text + "\r\n";
    DWORD written = 0;
    WriteFile(g_file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    FlushFileBuffers(g_file);
}

BOOL WINAPI on_ctrl(DWORD type) {
    log_line("Ereignis " + std::to_string(type) +
             (type == CTRL_C_EVENT ? " (Strg+C)" : type == CTRL_CLOSE_EVENT ? " (Fenster schließen)" : ""));
    if (type == CTRL_CLOSE_EVENT) {
        for (int i = 1; i <= 40; ++i) {
            Sleep(250);
            log_line("lebe noch");
        }
    }
    return TRUE;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        return 2;
    }
    g_t0 = GetTickCount64();
    g_file = CreateFileW(argv[1], GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_file == INVALID_HANDLE_VALUE) {
        return 1;
    }
    log_line("Start");
    SetConsoleCtrlHandler(on_ctrl, TRUE);
    for (;;) {
        Sleep(1000);
    }
}
