#pragma once

// The build flavour (HantedPreview, 2026-11-27): ONE translation unit (BuildFlavor.cpp) carries ELITE_PREVIEW_BUILD -
// the rest of the code asks here, so the preview package needs no global rebuild and no second code path.
#include <string>

namespace core
{
// true in the closed preview package (HantedPreview): English by default, the developer's diagnostic overlays hidden
bool isPreviewBuild() noexcept;

// THE ONE CAPABILITY POLICY (stabilization stage 9): every difference of the preview package is a named capability with
// its reason - the code asks capabilityAllowed(c), never the flavour. Developer build: all allowed.
enum class Capability
{
    DevCommands,             // DEV server commands (full restore, free refit apply, DEV economy ops) - refused by the server
    DevPanelNotes,           // developer notes / DEV buttons on the service panels
    UiLanguageIndicator,     // the UI language corner indicator
    NozzleAngleReadouts,     // the engine indicator's nozzle angle numbers (overlapped the pad percentages)
    RadarRangeLabel,         // the radar range caption (a developer readout)
    TelemetryColumn,         // the left telemetry column (printed over the HUD terminal)
    CockpitRadioWidget,      // the cockpit radio widget (not part of the preview)
    GlassTexts,              // the glass texts other than the HUD terminal (the author's preview rule)
    ModernCelestialDefault,  // the "modern" celestial style as the default (the preview defaults to Elite Classic)
    DevShortcutStrip,        // the developer shortcut strip (its keys stay; the package README lists them)
};
[[nodiscard]] bool capabilityAllowed(Capability c) noexcept;
[[nodiscard]] const char* capabilityName(Capability c) noexcept;
// the per-user data folder under %LOCALAPPDATA% (logs, preferences, journal, WebView2 sessions): "EliteGame" for the
// developer build, "HantedPreview" for the package - the two never share preferences or logs
const char* appDataFolderName() noexcept;
const wchar_t* appDataFolderNameW() noexcept;
// a relocatable package (HantedPreview): the game reads "assets/..." relative to the working directory - started from
// a shortcut with another "Start in", it would find nothing. When the working directory has no assets/ but the
// executable's folder has, switch to it (the developer layout - cwd = the build dir - is left alone). Returns the
// folder switched to (empty = unchanged).
std::string anchorWorkingDirectoryToExecutable();
// the preview package started by a double click: hide the console window Windows opened for it (the session log file
// keeps everything). Started from a console (cmd / a script) the console stays. No-op outside the preview.
void hideOwnConsoleInPreview();
// the preview package (stabilization stage 9): every ELITE_* environment variable but the preview-safe ones
// (ELITE_SESSION_LOG, ELITE_SERVER_OWN_CLOCK) is removed from the process before
// anything reads it - the developer switches (docs/DEV_SWITCHES.md) do not exist in the package. Returns how many were
// removed. No-op in the developer build (ELITE_DEV_PREVIEW_FLAVOR=1 keeps them: it tests the presentation only).
int scrubDeveloperEnvironmentInPreview();
} // namespace core
