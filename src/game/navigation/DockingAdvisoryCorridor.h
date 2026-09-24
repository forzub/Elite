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

// Displayed extent = actual ship extent + allowed center motion. At the final
// gate this equals the dock's usable opening (opening minus wall clearance).
inline double dockingAdvisoryFrameExtentMeters(
    double shipExtentMeters,
    double centerToleranceMeters
) noexcept
{
    return std::max(0.0, shipExtentMeters) +
        2.0 * std::max(0.0, centerToleranceMeters);
}

// Crossing nominal begins warning; it does not immediately destroy a manual
// route. The expanded release box and time grace reject a real departure while
// ignoring centimetre-scale sampling/jitter excursions.
inline DockingAdvisoryCrossSection dockingAdvisoryReleaseCrossSection(
    const DockingAdvisoryCrossSection& nominal,
    double marginFraction = 0.25,
    double minimumMarginMeters = 10.0
) noexcept
{
    const auto expand = [&](double tolerance)
    {
        const double safeTolerance = std::max(0.0, tolerance);
        return safeTolerance + std::max(
            std::max(0.0, minimumMarginMeters),
            safeTolerance * std::max(0.0, marginFraction)
        );
    };
    return {
        expand(nominal.lateralToleranceMeters),
        expand(nominal.verticalToleranceMeters)
    };
}

inline bool dockingAdvisoryNearBoundary(
    double lateralOffsetMeters,
    double verticalOffsetMeters,
    double longitudinalOffsetMeters,
    const DockingAdvisoryCrossSection& nominal,
    double longitudinalToleranceMeters,
    double warningFraction = 0.80
) noexcept
{
    const double fraction = std::clamp(warningFraction, 0.0, 1.0);
    const auto nearBoundary = [&](double value, double tolerance)
    {
        return tolerance > 0.0 &&
            std::abs(value) >= tolerance * fraction;
    };
    return
        nearBoundary(lateralOffsetMeters, nominal.lateralToleranceMeters) ||
        nearBoundary(verticalOffsetMeters, nominal.verticalToleranceMeters) ||
        nearBoundary(longitudinalOffsetMeters, longitudinalToleranceMeters);
}

enum class DockingAdvisoryTrackingResult
{
    AwaitingEntry,
    Inside,
    Warning,
    Left
};

class DockingAdvisoryCorridorTracker
{
public:
    DockingAdvisoryTrackingResult observe(
        bool insideNominal,
        bool insideRelease,
        double serverTimeSeconds,
        double releaseGraceSeconds = 0.35
    ) noexcept
    {
        if (insideNominal)
        {
            m_entered = true;
            m_outsideReleaseSinceServerSeconds = -1.0;
            return DockingAdvisoryTrackingResult::Inside;
        }

        if (!m_entered)
        {
            m_outsideReleaseSinceServerSeconds = -1.0;
            return DockingAdvisoryTrackingResult::AwaitingEntry;
        }

        if (insideRelease)
        {
            m_outsideReleaseSinceServerSeconds = -1.0;
            return DockingAdvisoryTrackingResult::Warning;
        }

        if (!std::isfinite(serverTimeSeconds))
            return DockingAdvisoryTrackingResult::Warning;

        if (m_outsideReleaseSinceServerSeconds < 0.0 ||
            serverTimeSeconds < m_outsideReleaseSinceServerSeconds)
        {
            m_outsideReleaseSinceServerSeconds = serverTimeSeconds;
            return DockingAdvisoryTrackingResult::Warning;
        }

        if (serverTimeSeconds - m_outsideReleaseSinceServerSeconds >=
            std::max(0.0, releaseGraceSeconds))
        {
            return DockingAdvisoryTrackingResult::Left;
        }

        return DockingAdvisoryTrackingResult::Warning;
    }

    bool entered() const noexcept
    {
        return m_entered;
    }

private:
    bool m_entered = false;
    double m_outsideReleaseSinceServerSeconds = -1.0;
};

} // namespace game::navigation
