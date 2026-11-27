#include "src/core/BuildFlavor.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
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
// DEV: ELITE_DEV_PREVIEW_FLAVOR=1 - the preview's presentation rules in a developer build (to look at them without a package)
bool isPreviewBuild() noexcept
{
    static const bool forced = [] {
        const char* v = std::getenv("ELITE_DEV_PREVIEW_FLAVOR");
        return v && *v == '1';
    }();
    return forced;
}
const char* appDataFolderName() noexcept { return "EliteGame"; }
const wchar_t* appDataFolderNameW() noexcept { return L"EliteGame"; }
#endif
bool capabilityAllowed(Capability) noexcept
{
    // the preview package denies every listed capability; the developer build allows all (one switch, one place)
    return !isPreviewBuild();
}

const char* capabilityName(Capability c) noexcept
{
    switch (c)
    {
        case Capability::DevCommands: return "dev_commands";
        case Capability::DevPanelNotes: return "dev_panel_notes";
        case Capability::UiLanguageIndicator: return "ui_language_indicator";
        case Capability::NozzleAngleReadouts: return "nozzle_angle_readouts";
        case Capability::RadarRangeLabel: return "radar_range_label";
        case Capability::TelemetryColumn: return "telemetry_column";
        case Capability::CockpitRadioWidget: return "cockpit_radio_widget";
        case Capability::GlassTexts: return "glass_texts";
        case Capability::ModernCelestialDefault: return "modern_celestial_default";
        case Capability::DevShortcutStrip: return "dev_shortcut_strip";
    }
    return "?";
}
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

namespace core
{
int scrubDeveloperEnvironmentInPreview()
{
#if defined(ELITE_PREVIEW_BUILD) && defined(_WIN32)
    std::vector<std::string> names;
    for (char** e = _environ; e && *e; ++e)
    {
        const std::string entry(*e);
        // EVERY ELITE_* switch but the preview-safe list (stage 9: the "configuration" class holds cheats and dumps too -
        // ELITE_FUEL_FREE, ELITE_CATALOG_SHIPS, the route dumps; docs/DEV_SWITCHES.md)
        if (entry.rfind("ELITE_", 0) != 0)
            continue;
        const std::string name = entry.substr(0, entry.find('='));
        if (name == "ELITE_SESSION_LOG" || name == "ELITE_SERVER_OWN_CLOCK")
            continue;
        names.push_back(name);
    }
    for (const auto& n : names)
        _putenv_s(n.c_str(), "");   // an empty value removes the variable
    return static_cast<int>(names.size());
#else
    return 0;
#endif
}
} // namespace core
