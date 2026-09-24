#pragma once

#include <algorithm>
#include <cmath>

namespace game::navigation
{

struct DockingAdvisoryCrossSection
{
    double lateralToleranceMeters = 0.0;
    double verticalToleranceMeters = 0.0;
};

// The open-flight corridor is wider than the dock aperture. Narrow it only
// along the final approach, where the ship must fit the actual opening.
inline DockingAdvisoryCrossSection dockingAdvisoryCrossSection(
    double distanceToStopMeters,
    double terminalLateralToleranceMeters,
    double terminalVerticalToleranceMeters,
    double transitToleranceMeters = 60.0,
    double narrowingDistanceMeters = 700.0
) noexcept
{
    const double fraction = std::clamp(
        distanceToStopMeters / narrowingDistanceMeters, 0.0, 1.0);
    return {
        terminalLateralToleranceMeters +
            (transitToleranceMeters - terminalLateralToleranceMeters) * fraction,
        terminalVerticalToleranceMeters +
            (transitToleranceMeters - terminalVerticalToleranceMeters) * fraction
    };
}

enum class DockingAdvisoryTrackingResult
{
    AwaitingEntry,
    Inside,
    Left
};

class DockingAdvisoryCorridorTracker
{
public:
    DockingAdvisoryTrackingResult observe(bool inside) noexcept
    {
        if (inside)
        {
            m_entered = true;
            return DockingAdvisoryTrackingResult::Inside;
        }
        return m_entered
            ? DockingAdvisoryTrackingResult::Left
            : DockingAdvisoryTrackingResult::AwaitingEntry;
    }

private:
    bool m_entered = false;
};

} // namespace game::navigation
