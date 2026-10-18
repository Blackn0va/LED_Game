#include "console/console.hpp"

#include "core/error.hpp"

#include <windows.h>

#include <atomic>
#include <limits>
#include <set>
#include <string>
#include <utility>

namespace ldu::console {
namespace {

std::atomic<bool> g_quit{false};
// Wird gesetzt, wenn der Destruktor fertig ist; der Strg-Handler wartet darauf.
std::atomic<HANDLE> g_cleanup_done{nullptr};

// Beim Schließen des Fensters braucht stop() gut 2,3 s (2 s Ruhe für den H612C, siehe
// govee/strip.hpp). Vorher stand hier 2000 – das hätte die Aus-Folge abgeschnitten. Ob
// Windows 4 s gewährt, belegt der Mitschnitt in probes/README.md.
inline constexpr DWORD kCloseWaitMs = 4000;

BOOL WINAPI on_ctrl(DWORD type) {
    g_quit.store(true);
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT) {
        const HANDLE done = g_cleanup_done.load();
        if (done != nullptr) {
            WaitForSingleObject(done, kCloseWaitMs);
        }
    }
    return TRUE;
}

// Bildschirmausgabe ersetzt ungültige UTF-8-Folgen durch U+FFFD, statt wie widen() zu
// werfen: Sie ist der letzte Weg einer Fehlermeldung, und what() fremder Ausnahmen
// (std::system_error) kommt in der ANSI-Codepage. Würfe print dort, endete das
// Programm in std::terminate statt mit einem lesbaren Satz.
std::wstring widen_for_display(std::string_view utf8) {
    if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        utf8 = utf8.substr(0, static_cast<std::size_t>(std::numeric_limits<int>::max()));
    }
    const int in_len = static_cast<int>(utf8.size());
    const int out_len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), in_len, nullptr, 0);
    if (out_len <= 0) {
        return L"?";
    }
    std::wstring out(static_cast<std::size_t>(out_len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), in_len, out.data(), out_len);
    return out;
}

void write_handle(HANDLE h, std::string_view utf8) {
    if (utf8.empty()) {
        return;
    }
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        const std::wstring w = widen_for_display(utf8);
        DWORD written = 0;
        WriteConsoleW(h, w.data(), static_cast<DWORD>(w.size()), &written, nullptr);
        return;
    }
    DWORD written = 0;
    WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
}

}  // namespace

struct Console::Impl final {
    HANDLE in = INVALID_HANDLE_VALUE;
    HANDLE out = INVALID_HANDLE_VALUE;
    DWORD in_mode_before = 0;
    DWORD out_mode_before = 0;
    HANDLE done_event = nullptr;
    std::set<std::uint16_t> held;
    bool resized = false;
};

Console::Console(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

std::unique_ptr<Console> Console::open() {
    auto impl = std::make_unique<Impl>();
    impl->in = GetStdHandle(STD_INPUT_HANDLE);
    impl->out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(impl->out, &impl->out_mode_before) || !GetConsoleMode(impl->in, &impl->in_mode_before)) {
        throw Error(Error::Kind::console,
                    "LichtDuell braucht ein Konsolenfenster – Ein- oder Ausgabe ist umgeleitet.");
    }
    const DWORD out_mode = impl->out_mode_before | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(impl->out, out_mode)) {
        throw Error(Error::Kind::console,
                    "Die Konsole kann keine Farbsteuerzeichen (VT) – Windows 10 oder Windows Terminal nötig.");
    }
    // Ohne ENABLE_LINE_INPUT/ECHO kommt jede Taste sofort; ENABLE_EXTENDED_FLAGS ohne
    // ENABLE_QUICK_EDIT_MODE schaltet die Schnellbearbeitung ab; PROCESSED_INPUT
    // lässt Strg+C beim Handler ankommen.
    const DWORD in_mode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_PROCESSED_INPUT;
    if (!SetConsoleMode(impl->in, in_mode)) {
        SetConsoleMode(impl->out, impl->out_mode_before);
        throw Error(Error::Kind::console, "Tastatureingabe der Konsole lässt sich nicht umschalten.");
    }
    FlushConsoleInputBuffer(impl->in);
    impl->done_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_cleanup_done.store(impl->done_event);
    g_quit.store(false);
    SetConsoleCtrlHandler(on_ctrl, TRUE);

    auto console = std::unique_ptr<Console>(new Console(std::move(impl)));
    // Fenstertitel „LichtDuell“ statt des EXE-Pfads, Alternativpuffer, Cursor aus, Bild leeren.
    console->write("\x1b]0;LichtDuell\x07\x1b[?1049h\x1b[?25l\x1b[2J");
    return console;
}

Console::~Console() {
    write_handle(impl_->out, "\x1b[0m\x1b[?25h\x1b[?1049l");
    SetConsoleMode(impl_->in, impl_->in_mode_before);
    SetConsoleMode(impl_->out, impl_->out_mode_before);
    SetConsoleCtrlHandler(on_ctrl, FALSE);
    if (impl_->done_event != nullptr) {
        SetEvent(impl_->done_event);
        g_cleanup_done.store(nullptr);
        CloseHandle(impl_->done_event);
    }
}

void Console::write(std::string_view utf8) {
    write_handle(impl_->out, utf8);
}

int Console::width() const {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(impl_->out, &info)) {
        return 80;
    }
    return info.srWindow.Right - info.srWindow.Left + 1;
}

std::vector<KeyPress> Console::poll_keys() {
    std::vector<KeyPress> keys;
    for (;;) {
        DWORD pending = 0;
        if (!GetNumberOfConsoleInputEvents(impl_->in, &pending) || pending == 0) {
            break;
        }
        INPUT_RECORD records[32];
        DWORD read = 0;
        if (!ReadConsoleInputW(impl_->in, records, 32, &read)) {
            throw Error(Error::Kind::console, "Tastatur lässt sich nicht lesen (Fehler " +
                                                  std::to_string(GetLastError()) + ").");
        }
        for (DWORD i = 0; i < read; ++i) {
            const INPUT_RECORD& r = records[i];
            if (r.EventType == KEY_EVENT) {
                const auto vk = static_cast<std::uint16_t>(r.Event.KeyEvent.wVirtualKeyCode);
                if (r.Event.KeyEvent.bKeyDown) {
                    if (impl_->held.insert(vk).second) {
                        keys.push_back(KeyPress{vk});
                    }
                } else {
                    impl_->held.erase(vk);
                }
            } else if (r.EventType == FOCUS_EVENT) {
                // Fokuswechsel bei gehaltener Taste: Das Loslassen kommt nie an, die Taste
                // bliebe für immer „gehalten“ und täte nichts mehr.
                impl_->held.clear();
            } else if (r.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                impl_->resized = true;
            }
        }
    }
    return keys;
}

bool Console::take_resized() {
    return std::exchange(impl_->resized, false);
}

void Console::wait_input(int timeout_ms) {
    WaitForSingleObject(impl_->in, static_cast<DWORD>(timeout_ms < 0 ? 0 : timeout_ms));
}

bool Console::quit_requested() const {
    return g_quit.load();
}

void print(std::string_view utf8) {
    write_handle(GetStdHandle(STD_OUTPUT_HANDLE), utf8);
}

}  // namespace ldu::console
