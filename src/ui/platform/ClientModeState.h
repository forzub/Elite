#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace ui::platform
{

// Authoritative client-side mode state. Renderers, localization resources and
// WebViews are projections/consumers of this state; they must not independently
// decide the active user mode.
struct ClientModeState
{
    std::string uiLocale = "en";
    bool constellationsEnabled = false;
    std::string skyCultureId;
    std::uint64_t revision = 1;

    bool setUiLocale(std::string locale)
    {
        if (locale.empty() || locale == uiLocale)
            return false;
        uiLocale = std::move(locale);
        ++revision;
        return true;
    }

    bool setConstellationsEnabled(bool enabled) noexcept
    {
        if (constellationsEnabled == enabled)
            return false;
        constellationsEnabled = enabled;
        ++revision;
        return true;
    }

    bool toggleConstellations() noexcept
    {
        return setConstellationsEnabled(!constellationsEnabled);
    }

    bool setSkyCultureId(std::string id)
    {
        if (id.empty() || id == skyCultureId)
            return false;
        skyCultureId = std::move(id);
        ++revision;
        return true;
    }
};

} // namespace ui::platform
