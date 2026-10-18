// Tastatur-Probe: Kommen echte Konsolen-Tastenereignisse im Spiel an?
//
// Die Unit-Tests prüfen nur die Tastenbelegung, nicht den Weg ReadConsoleInput → Spiel.
// Diese Probe startet LichtDuell.exe --nur-vorschau --ohne-ton --seed 5 in einer eigenen,
// fensterlosen Konsole (CREATE_NO_WINDOW), hängt sich an diese Konsole (AttachConsole),
// schreibt Tastenereignisse in deren Eingabepuffer (WriteConsoleInputW) und liest den
// Bildschirm zurück (ReadConsoleOutputCharacterW). Geprüft wird:
//   P → „PAUSE“ erscheint, P → verschwindet
//   Leertaste → Joker ★☆☆ wird ☆☆☆, „Schüsse 1“
//   '1' fünfmal gedrückt, ohne loszulassen (Tastenwiederholung) → genau EIN Schuss
//   '4' in Stufe 1 → „Gelb kommt erst ab Stufe 4.“
//   A → Automodus an, der Computer schießt; A → wieder aus
//   Esc → Programm endet mit Exitcode 0
//
// Aufruf: ldu_probe_keys <pfad zu LichtDuell.exe>. Exit 0 = alles bestanden, 1 = etwas
// gescheitert, 2 = falscher Aufruf. Ergebnis zusätzlich in key_probe.log neben der Probe.

#include <windows.h>

#include <fstream>
#include <string>
#include <string_view>

namespace {

std::string g_log;
int g_failed = 0;

void note(const std::string& line) {
    g_log += line + "\n";
}

void check(const std::string& what, bool ok) {
    note(std::string(ok ? "  ok     " : "  FEHLER ") + what);
    if (!ok) {
        ++g_failed;
    }
}

std::wstring screen_text() {
    const HANDLE h = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::wstring text;
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(h, &info)) {
        const SHORT width = info.dwSize.X;
        std::wstring row(static_cast<std::size_t>(width), L' ');
        for (SHORT y = info.srWindow.Top; y <= info.srWindow.Bottom; ++y) {
            DWORD read = 0;
            ReadConsoleOutputCharacterW(h, row.data(), static_cast<DWORD>(width), COORD{0, y}, &read);
            // Ohne Trenner aneinander: Umbrochene Zeilen bleiben so durchsuchbar.
            text.append(row.data(), read);
        }
    }
    CloseHandle(h);
    return text;
}

bool wait_for(std::wstring_view needle, bool present, DWORD timeout_ms) {
    const ULONGLONG end = GetTickCount64() + timeout_ms;
    do {
        if ((screen_text().find(needle) != std::wstring::npos) == present) {
            return true;
        }
        Sleep(30);
    } while (GetTickCount64() < end);
    return false;
}

void key(HANDLE in, WORD vk, bool down, wchar_t ch) {
    INPUT_RECORD r{};
    r.EventType = KEY_EVENT;
    r.Event.KeyEvent.bKeyDown = down ? TRUE : FALSE;
    r.Event.KeyEvent.wRepeatCount = 1;
    r.Event.KeyEvent.wVirtualKeyCode = vk;
    r.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    r.Event.KeyEvent.uChar.UnicodeChar = ch;
    DWORD written = 0;
    WriteConsoleInputW(in, &r, 1, &written);
}

void press(HANDLE in, WORD vk, wchar_t ch) {
    key(in, vk, true, ch);
    key(in, vk, false, ch);
}

void report(HANDLE original_out, const std::wstring& log_path) {
    std::ofstream(log_path, std::ios::binary) << g_log;
    // Die Probe hat ihre Konsole gegen die des Spiels getauscht. Eine Umleitung (Pipe,
    // Datei) bleibt dabei gültig; sonst über die Elternkonsole ausgeben.
    if (GetFileType(original_out) == FILE_TYPE_PIPE || GetFileType(original_out) == FILE_TYPE_DISK) {
        DWORD written = 0;
        WriteFile(original_out, g_log.data(), static_cast<DWORD>(g_log.size()), &written, nullptr);
        return;
    }
    FreeConsole();
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        const HANDLE out = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (out != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(out, g_log.data(), static_cast<DWORD>(g_log.size()), &written, nullptr);
            CloseHandle(out);
        }
    }
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        return 2;
    }
    const HANDLE original_out = GetStdHandle(STD_OUTPUT_HANDLE);
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring log_path(self);
    log_path = log_path.substr(0, log_path.find_last_of(L'\\') + 1) + L"key_probe.log";

    const std::wstring exe = argv[1];
    // --ohne-einstellungen: Die Probe soll nicht von einer einstellungen.txt des Nutzers abhängen.
    std::wstring cmd = L"\"" + exe + L"\" --nur-vorschau --ohne-ton --seed 5 --ohne-einstellungen";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    note("Tastatur-Probe LichtDuell (CREATE_NO_WINDOW + AttachConsole + WriteConsoleInputW)");
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                        &pi)) {
        check("LichtDuell.exe startet (Fehler " + std::to_string(GetLastError()) + ")", false);
        report(original_out, log_path);
        return 1;
    }

    FreeConsole();
    bool attached = false;
    for (int i = 0; i < 100 && !attached; ++i) {
        attached = AttachConsole(pi.dwProcessId) != FALSE;
        if (!attached) {
            Sleep(50);
        }
    }
    check("an die Konsole des Spiels angehängt", attached);
    if (attached) {
        const HANDLE in = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr, OPEN_EXISTING, 0, nullptr);
        check("Spielbild erscheint (Stufe 1, Joker ♦♦♦)", wait_for(L"Stufe 1", true, 8000) &&
                                                             wait_for(L"Joker ♦♦♦", true, 2000));
        Sleep(300);

        press(in, 'P', L'p');
        check("P zeigt PAUSE", wait_for(L"PAUSE", true, 2000));
        press(in, 'P', L'p');
        check("P setzt fort", wait_for(L"PAUSE", false, 2000));

        // Wie viele Joker da sind, zeigt seit 2026-10-18 nur noch die Farbe der ♦ (die liest
        // die Probe nicht). Belegt wird es über das Verhalten: Der erste Druck schießt, der
        // zweite findet keinen Joker mehr.
        press(in, VK_SPACE, L' ');
        check("Leertaste feuert den Joker (Schüsse 1)", wait_for(L"Schüsse 1 ·", true, 2000));
        Sleep(200);  // Sperrzeit der Kanone (0,1 s) sicher vorbei
        press(in, VK_SPACE, L' ');
        check("… zweite Leertaste: „Kein Joker“, kein weiterer Schuss",
              wait_for(L"Kein Joker", true, 2000) && screen_text().find(L"Schüsse 2 ·") == std::wstring::npos);

        Sleep(200);
        for (int i = 0; i < 5; ++i) {
            key(in, '1', true, L'1');  // Tastenwiederholung: Drücken ohne Loslassen
            Sleep(40);
        }
        key(in, '1', false, L'1');
        check("gehaltene 1 feuert genau einmal (Schüsse 2)", wait_for(L"Schüsse 2 ·", true, 2000));
        Sleep(400);
        check("… und nicht öfter", screen_text().find(L"Schüsse 3 ·") == std::wstring::npos);

        press(in, '4', L'4');
        check("4 in Stufe 1: „Gelb kommt erst ab Stufe 4.“", wait_for(L"Gelb kommt erst ab Stufe 4.", true, 2000));

        // Taste A (Wunsch 2026-10-18: Automodus per Tastatur an und aus).
        press(in, 'A', L'a');
        check("A schaltet den Automodus ein (Titel AUTO ×1)", wait_for(L"LICHTDUELL · AUTO ×1", true, 2000));
        check("… und der Computer schießt selbst (über 2 Schüsse hinaus)",
              wait_for(L"Schüsse 2 ·", false, 8000));
        press(in, 'A', L'a');
        check("A noch einmal: Automodus aus, du spielst", wait_for(L"LICHTDUELL · AUTO", false, 2000) &&
                                                           wait_for(L"Automodus aus", true, 2000));

        press(in, VK_ESCAPE, 0x1B);
        const bool ended = WaitForSingleObject(pi.hProcess, 3000) == WAIT_OBJECT_0;
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        check("Esc beendet das Spiel mit Exitcode 0 (" + std::to_string(code) + ")", ended && code == 0);
        if (in != INVALID_HANDLE_VALUE) {
            CloseHandle(in);
        }
    }
    if (WaitForSingleObject(pi.hProcess, 0) != WAIT_OBJECT_0) {
        TerminateProcess(pi.hProcess, 99);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Teil 2: Testbild – Pfeiltasten verschieben die Grenzen. Anlass: Beim Einrichten am
    // 2026-10-18 kamen die Pfeile des Nutzers nicht an (Tab ohne Fokus?), und Teil 1 prüft
    // keine Pfeile. Bewusst OHNE S: Speichern schriebe die echte einstellungen.txt.
    std::wstring cmd2 = L"\"" + exe + L"\" --testbild --nur-vorschau --ohne-einstellungen --segmente 40";
    PROCESS_INFORMATION pi2{};
    if (!CreateProcessW(exe.c_str(), cmd2.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                        &pi2)) {
        check("Testbild startet (Fehler " + std::to_string(GetLastError()) + ")", false);
    } else {
        FreeConsole();
        bool attached2 = false;
        for (int i = 0; i < 100 && !attached2; ++i) {
            attached2 = AttachConsole(pi2.dwProcessId) != FALSE;
            if (!attached2) {
                Sleep(50);
            }
        }
        check("an die Konsole des Testbilds angehängt", attached2);
        if (attached2) {
            const HANDLE in2 = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                           nullptr, OPEN_EXISTING, 0, nullptr);
            check("Testbild erscheint (aus 0, Effektbereich 0)", wait_for(L"aus 0  ·", true, 8000));
            Sleep(300);
            press(in2, VK_RIGHT, 0);
            press(in2, VK_RIGHT, 0);
            check("→ → schiebt die Kanone um zwei (aus 2)", wait_for(L"aus 2  ·", true, 2000));
            press(in2, VK_UP, 0);
            check("↑ vergrößert den Effektbereich (Effektbereich 1)", wait_for(L"Effektbereich 1", true, 2000));
            press(in2, VK_LEFT, 0);
            check("← schiebt zurück (aus 1)", wait_for(L"aus 1  ·", true, 2000));
            press(in2, VK_ESCAPE, 0x1B);
            const bool ended2 = WaitForSingleObject(pi2.hProcess, 3000) == WAIT_OBJECT_0;
            DWORD code2 = 1;
            GetExitCodeProcess(pi2.hProcess, &code2);
            check("Esc beendet das Testbild mit Exitcode 0 (" + std::to_string(code2) + ")", ended2 && code2 == 0);
            if (in2 != INVALID_HANDLE_VALUE) {
                CloseHandle(in2);
            }
        }
        if (WaitForSingleObject(pi2.hProcess, 0) != WAIT_OBJECT_0) {
            TerminateProcess(pi2.hProcess, 99);
        }
        CloseHandle(pi2.hThread);
        CloseHandle(pi2.hProcess);
    }
    note(g_failed == 0 ? "Ergebnis: alles bestanden" : "Ergebnis: " + std::to_string(g_failed) + " gescheitert");
    report(original_out, log_path);
    return g_failed == 0 ? 0 : 1;
}
