#pragma once

#include <filesystem>

namespace world::navigation
{

// Explicit path composition only.  This header intentionally owns no global
// logger, startup announcer, working-directory lookup, wall clock or file I/O.
// Orchestration may choose a diagnostics root and perform logging outside the
// strict-pure navigation calculation core.
[[nodiscard]] inline std::filesystem::path navigationPerfLogPath(
    const std::filesystem::path& diagnosticsRoot
)
{
    return diagnosticsRoot / "navigation_perf.log";
}

} // namespace world::navigation
