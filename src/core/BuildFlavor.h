#pragma once

// The build flavour (HantedPreview, 2026-11-27): ONE translation unit (BuildFlavor.cpp) carries ELITE_PREVIEW_BUILD -
// the rest of the code asks here, so the preview package needs no global rebuild and no second code path.
#include <string>

namespace core
{
// true in the closed preview package (HantedPreview): English by default, the developer's diagnostic overlays hidden
bool isPreviewBuild() noexcept;
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
} // namespace core
