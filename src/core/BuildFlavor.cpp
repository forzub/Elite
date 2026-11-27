#include "src/core/BuildFlavor.h"

#include <filesystem>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

namespace core
{
#ifdef ELITE_PREVIEW_BUILD
bool isPreviewBuild() noexcept { return true; }
const char* appDataFolderName() noexcept { return "HantedPreview"; }
const wchar_t* appDataFolderNameW() noexcept { return L"HantedPreview"; }
#else
bool isPreviewBuild() noexcept { return false; }
const char* appDataFolderName() noexcept { return "EliteGame"; }
const wchar_t* appDataFolderNameW() noexcept { return L"EliteGame"; }
#endif
} // namespace core

namespace core
{
std::string anchorWorkingDirectoryToExecutable()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::exists(fs::current_path(ec) / "assets", ec))
        return {};
#ifdef _WIN32
    wchar_t buffer[32768] = {};
    const DWORD n = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])));
    if (n == 0 || n >= sizeof(buffer) / sizeof(buffer[0]))
        return {};
    const fs::path exeDir = fs::path(buffer).parent_path();
    if (!fs::exists(exeDir / "assets", ec))
        return {};
    fs::current_path(exeDir, ec);
    return ec ? std::string() : exeDir.string();
#else
    return {};
#endif
}
} // namespace core

namespace core
{
void hideOwnConsoleInPreview()
{
#ifdef _WIN32
    if (!isPreviewBuild())
        return;
    DWORD processes[2] = {};
    // only this process attached: the console was created for it (a double click), not inherited from a shell
    if (GetConsoleProcessList(processes, 2) == 1)
        if (HWND console = GetConsoleWindow())
            ShowWindow(console, SW_HIDE);
#endif
}
} // namespace core
