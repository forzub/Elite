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

// Bounded Hermite interpolation between TWO adjacent authoritative
// kinematic snapshots. Position AND velocity are continuous at every
// snapshot boundary. Old-only extrapolation jumped when the selected
// interval advanced to the next packet, even if all clocks agreed.
inline RemoteShipPresentationDeadReckoning interpolateRemoteShipPresentation(
    const ShipSnapshot& older,
    double olderServerTimeSeconds,
    const ShipSnapshot& newer,
    double newerServerTimeSeconds,
    double renderServerTimeSeconds
) noexcept
{
    const double span=newerServerTimeSeconds-olderServerTimeSeconds;
    if(!std::isfinite(span) || span<=1.0e-9)
        return deadReckonRemoteShipPresentation(
            older,olderServerTimeSeconds,renderServerTimeSeconds,0.0);

    const double t=std::clamp(
        (renderServerTimeSeconds-olderServerTimeSeconds)/span,0.0,1.0);
    const double t2=t*t, t3=t2*t;
    const double h00=2*t3-3*t2+1;
    const double h10=t3-2*t2+t;
    const double h01=-2*t3+3*t2;
    const double h11=t3-t2;
    const double dh00=6*t2-6*t;
    const double dh10=3*t2-4*t+1;
    const double dh01=-6*t2+6*t;
    const double dh11=3*t2-2*t;

    const auto& op=older.transform.motion;
    const auto& np=newer.transform.motion;
    const glm::dvec3 localDelta=
        np.localPositionMeters-op.localPositionMeters;
    const glm::dvec3 worldDelta=
        world::coordinates::relativeMeters(
            newer.transform.worldPosition,older.transform.worldPosition);

    RemoteShipPresentationDeadReckoning out;
    out.deltaSeconds=span*t;
    out.worldPosition=world::coordinates::translated(
        older.transform.worldPosition,
        h10*span*op.worldVelocityMps+
        h01*worldDelta+
        h11*span*np.worldVelocityMps);
    out.localPositionMeters=
        h00*op.localPositionMeters+
        h10*span*op.localVelocityMps+
        h01*np.localPositionMeters+
        h11*span*np.localVelocityMps;
    out.localVelocityMetersPerSecond=
        (dh00*op.localPositionMeters+
         dh10*span*op.localVelocityMps+
         dh01*np.localPositionMeters+
         dh11*span*np.localVelocityMps)/span;
    return out;
}

} // namespace game::client
