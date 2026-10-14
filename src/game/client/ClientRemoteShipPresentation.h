#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/simulation/ShipSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::client
{

struct RemoteShipPresentationDeadReckoning
{
    double deltaSeconds = 0.0;
    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 localPositionMeters {0.0};
    glm::dvec3 localVelocityMetersPerSecond {0.0};
};

// Presentation-only extrapolation for low-fidelity remote ships.
// The returned value is disposable client state: authoritative snapshots are
// passed by const reference and are never mutated by this helper.
inline RemoteShipPresentationDeadReckoning
deadReckonRemoteShipPresentation(
    const ShipSnapshot& authoritative,
    double presentationServerTimeSeconds,
    double maximumPredictionSeconds = 0.75
) noexcept
{
    RemoteShipPresentationDeadReckoning out;

    const double maxPrediction =
        std::max(0.0, maximumPredictionSeconds);
    const double requested =
        std::max(
            0.0,
            presentationServerTimeSeconds -
                authoritative.referenceFrame.universeTimeSeconds
        );

    // referenceFrame.universeTimeSeconds is normally the universe epoch and is
    // not the replication server time. Callers that have a snapshot time use
    // the overload below. Keep this overload conservative.
    out.deltaSeconds = std::min(requested, maxPrediction);
    out.worldPosition = world::coordinates::translated(
        authoritative.transform.worldPosition,
        authoritative.transform.motion.worldVelocityMps *
            out.deltaSeconds
    );
    out.localPositionMeters =
        authoritative.transform.motion.localPositionMeters +
        authoritative.transform.motion.localVelocityMps *
            out.deltaSeconds;
    out.localVelocityMetersPerSecond =
        authoritative.transform.motion.localVelocityMps;
    return out;
}

inline RemoteShipPresentationDeadReckoning
deadReckonRemoteShipPresentation(
    const ShipSnapshot& authoritative,
    double authoritativeServerTimeSeconds,
    double presentationServerTimeSeconds,
    double maximumPredictionSeconds
) noexcept
{
    RemoteShipPresentationDeadReckoning out;

    const double requested =
        presentationServerTimeSeconds - authoritativeServerTimeSeconds;
    out.deltaSeconds =
        std::clamp(
            std::isfinite(requested) ? requested : 0.0,
            0.0,
            std::max(0.0, maximumPredictionSeconds)
        );

    out.worldPosition = world::coordinates::translated(
        authoritative.transform.worldPosition,
        authoritative.transform.motion.worldVelocityMps *
            out.deltaSeconds
    );
    out.localPositionMeters =
        authoritative.transform.motion.localPositionMeters +
        authoritative.transform.motion.localVelocityMps *
            out.deltaSeconds;
    out.localVelocityMetersPerSecond =
        authoritative.transform.motion.localVelocityMps;
    return out;
}

} // namespace game::client
