#pragma once

#include <algorithm>
#include <cmath>

#include "src/game/navigation/planner/RouteEntryBinder.h"

namespace game::navigation::planner
{

// Spatial/live-time admission for asynchronously built docking routes.
// A finished route need not be rebuilt if the present AND forecast
// ballistic craft remain inside the first authored straight. Moving
// forward along an identical line changes route progress, not topology.
//
// Connector computation time is independent of flight time to the
// first curve. Use the measured last connector worker once available;
// the bootstrap lead is only used before the first measurement.
// A stale/failed predicted entry is still bounded by SpaceState's
// MaxEntryConnectorMisses retry budget.
class BallisticEntryHandoffPolicy final
{
public:
    inline static constexpr double BootstrapConnectorSeconds = 0.75;
    inline static constexpr double WorkerLatencyFactor = 1.25;

    [[nodiscard]] static double forecastLeadSeconds(
        double measuredConnectorWorkerSeconds
    ) noexcept
    {
        if (!(std::isfinite(measuredConnectorWorkerSeconds) &&
              measuredConnectorWorkerSeconds > 0.0))
            return BootstrapConnectorSeconds;
        return std::max(
            BootstrapConnectorSeconds,
            measuredConnectorWorkerSeconds * WorkerLatencyFactor
        );
    }

    [[nodiscard]] static bool canReuseStraightPrefix(
        const RoutePlan& plan,
        const glm::dvec3& livePositionMeters,
        const glm::dvec3& liveVelocityMetersPerSecond,
        double allowedCrossTrackMeters,
        double connectorLeadSeconds
    ) noexcept
    {
        const auto live = RouteEntryBinder::inspectBallisticPrefix(
            plan, livePositionMeters, allowedCrossTrackMeters
        );
        if (live.disposition != BallisticPrefixDisposition::OnPrefix)
            return false;

        if (!(std::isfinite(connectorLeadSeconds) &&
              connectorLeadSeconds >= 0.0))
            return false;

        const auto predicted =
            RouteEntryBinder::inspectBallisticPrefix(
                plan,
                livePositionMeters +
                    liveVelocityMetersPerSecond * connectorLeadSeconds,
                allowedCrossTrackMeters
            );
        return predicted.disposition ==
            BallisticPrefixDisposition::OnPrefix;
    }
};

} // namespace game::navigation::planner
