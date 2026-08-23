#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace game::navigation
{

/*
    Runtime switches for the navigation stack.

    The switches deliberately live outside renderers and planners. Cockpit
    controls, debug tools or ship equipment may change them later without
    teaching those callers about implementation details.

    A computational module and its HUD presentation are separate switches:
    safety evaluation may stay active while the pilot hides guidance graphics.
*/
enum class NavigationModuleId : std::uint8_t
{
    TrajectoryPrediction = 0,
    SafetyEvaluation,
    RoutePlanning,
    OfficialLanePlanning,
    ScheduledTraffic,
    LocalGuidance,
    ServerGuidance,
    SensorFusion,

    HudTargetMarkers,
    HudRouteMarkers,
    HudGuidanceCorridor,
    HudGalacticCompass,
    HudFlightVector,

    Count
};

class NavigationModuleState
{
public:
    NavigationModuleState()
    {
        m_enabled.fill(true);

        // Guidance remains fully calculated but its experimental corridor HUD
        // is opt-in until the cockpit presentation is tuned.
        setEnabled(NavigationModuleId::HudGuidanceCorridor, false);
    }

    bool enabled(NavigationModuleId module) const noexcept
    {
        const std::size_t index = static_cast<std::size_t>(module);
        return index < m_enabled.size() && m_enabled[index];
    }

    void setEnabled(NavigationModuleId module, bool enabled) noexcept
    {
        const std::size_t index = static_cast<std::size_t>(module);
        if (index < m_enabled.size())
            m_enabled[index] = enabled;
    }

    bool toggle(NavigationModuleId module) noexcept
    {
        const bool next = !enabled(module);
        setEnabled(module, next);
        return next;
    }

    void setAllHudLayers(bool enabled) noexcept
    {
        setEnabled(NavigationModuleId::HudTargetMarkers, enabled);
        setEnabled(NavigationModuleId::HudRouteMarkers, enabled);
        setEnabled(NavigationModuleId::HudGuidanceCorridor, enabled);
        setEnabled(NavigationModuleId::HudGalacticCompass, enabled);
        setEnabled(NavigationModuleId::HudFlightVector, enabled);
    }

private:
    std::array<bool, static_cast<std::size_t>(NavigationModuleId::Count)>
        m_enabled {};
};

} // namespace game::navigation
