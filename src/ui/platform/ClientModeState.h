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
    std::string coordinateDisplayFormatId = "hierarchical";
    // Celestial bodies: "elite" | "modern" | "anime"
    // (render::celestial_flight::CelestialRenderStyle).
    std::string celestialRenderStyleId = "modern";
    // Difficulty layers (author 2026-11-08): optional gameplay layers the player switches on or off.
    // capsuleSurvival - after the ship is lost the pilot lives in the escape capsule until the rescue
    // arrives and keeps it alive (air, CO2, power, repairs - "как в Tin Can"); off = the wait is simply
    // waited out. The layer is laid in; its gameplay is a later slice.
    bool capsuleSurvivalEnabled = false;
    std::uint64_t revision = 1;

    bool setCapsuleSurvivalEnabled(bool enabled) noexcept
    {
        if (capsuleSurvivalEnabled == enabled)
            return false;
        capsuleSurvivalEnabled = enabled;
        ++revision;
        return true;
    }

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

    bool setCelestialRenderStyleId(std::string id)
    {
        if ((id != "elite" && id != "modern" && id != "anime") || id == celestialRenderStyleId)
            return false;
        celestialRenderStyleId = std::move(id);
        ++revision;
        return true;
    }

    bool setCoordinateDisplayFormatId(std::string id)
    {
        if (id.empty() || id == coordinateDisplayFormatId)
            return false;
        coordinateDisplayFormatId = std::move(id);
        ++revision;
        return true;
    }
};

} // namespace ui::platform
