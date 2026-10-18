#include "core/files.hpp"

#include "core/error.hpp"
#include "core/text.hpp"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

#include <fstream>
#include <sstream>

namespace ldu {

std::filesystem::path app_data_dir() {
    PWSTR raw = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw);
    if (FAILED(hr) || raw == nullptr) {
        if (raw != nullptr) {
            CoTaskMemFree(raw);
        }
        throw Error(Error::Kind::text, "Ordner %LOCALAPPDATA% nicht gefunden.");
    }
    std::filesystem::path base(raw);
    CoTaskMemFree(raw);
    return base / L"StreamWizard" / L"LichtDuell";
}

std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string_view what) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return std::nullopt;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw Error(Error::Kind::text, std::string(what) + " lässt sich nicht lesen: " + narrow(path.native()));
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void write_text_file_atomic(const std::filesystem::path& path, std::string_view text, std::string_view what) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        throw Error(Error::Kind::text, "Ordner für " + std::string(what) + " lässt sich nicht anlegen: " +
                                           narrow(path.parent_path().native()));
    }
    std::filesystem::path tmp = path;
    tmp += L".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << text;
        out.flush();
        if (!out) {
            throw Error(Error::Kind::text, std::string(what) + " lässt sich nicht schreiben: " + narrow(tmp.native()));
        }
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD code = GetLastError();
        std::filesystem::remove(tmp, ec);
        throw Error(Error::Kind::text,
                    std::string(what) + " lässt sich nicht ersetzen (Fehler " + std::to_string(code) + ").");
    }
}

}  // namespace ldu
