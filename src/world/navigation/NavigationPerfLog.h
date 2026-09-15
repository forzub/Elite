#pragma once

#include <filesystem>
#include <iostream>

namespace world::navigation
{

inline const std::filesystem::path& navigationPerfLogPath()
{
    static const std::filesystem::path path = []
    {
        std::error_code ec;
        const auto absolute = std::filesystem::absolute(
            std::filesystem::path("navigation_perf.log"),
            ec
        );
        if (ec)
            return std::filesystem::path("navigation_perf.log");
        return absolute.lexically_normal();
    }();
    return path;
}

// Inline process-lifetime announcer. Any executable that links the navigation
// smoother prints the exact file it will use before gameplay/tests start. This
// also makes multiple files from different working directories unambiguous.
struct NavigationPerfLogStartupAnnouncer
{
    NavigationPerfLogStartupAnnouncer()
    {
        std::cerr
            << "[NavigationPerf] log_path="
            << navigationPerfLogPath().string()
            << '\n';
    }
};

inline NavigationPerfLogStartupAnnouncer g_navigationPerfLogStartupAnnouncer;

} // namespace world::navigation
