#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/simulation/ShipReferenceFrameSnapshot.h"

namespace game::client
{

inline bool sameReferenceFrameIdentity(
    const game::simulation::ShipReferenceFrameSnapshot& a,
    const game::simulation::ShipReferenceFrameSnapshot& b
) noexcept
{
    return
        a.valid && b.valid &&
        a.systemId == b.systemId &&
        a.type == b.type &&
        a.bodyId == b.bodyId &&
        a.hubId == b.hubId &&
        a.moduleId == b.moduleId;
}

inline glm::dvec3 safeNormalizePresentationAxis(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double len = glm::length(value);
    return len > 1.0e-12 ? value / len : fallback;
}

inline game::simulation::ShipReferenceFrameSnapshot
interpolateReferenceFramePresentation(
    const game::simulation::ShipReferenceFrameSnapshot& from,
    const game::simulation::ShipReferenceFrameSnapshot& to,
    double alpha
)
{
    if (!sameReferenceFrameIdentity(from, to))
        return to;

    const double t = std::clamp(alpha, 0.0, 1.0);

    game::simulation::ShipReferenceFrameSnapshot out = to;

    out.originMeters =
        from.originMeters +
        (to.originMeters - from.originMeters) * t;

    out.velocityMetersPerSecond =
        from.velocityMetersPerSecond +
        (to.velocityMetersPerSecond - from.velocityMetersPerSecond) * t;

    out.accelerationMetersPerSecond2 =
        from.accelerationMetersPerSecond2 +
        (to.accelerationMetersPerSecond2 - from.accelerationMetersPerSecond2) * t;

    out.angularVelocityWorldRadPerSecond =
        from.angularVelocityWorldRadPerSecond +
        (to.angularVelocityWorldRadPerSecond -
         from.angularVelocityWorldRadPerSecond) * t;

    out.angularAccelerationWorldRadPerSecond2 =
        from.angularAccelerationWorldRadPerSecond2 +
        (to.angularAccelerationWorldRadPerSecond2 -
         from.angularAccelerationWorldRadPerSecond2) * t;

    /*
        A reference-frame basis is presentation state too. Taking the newest
        authoritative basis directly makes every co-frame object advance in
        snapshot-sized angular steps even while positions/module animation are
        sampled at renderServerTimeSeconds.

        Interpolate the triad at that same render epoch, then rebuild an
        orthonormal right-handed basis using the server convention:
            normal = cross(prograde, radial).
    */
    glm::dvec3 radial =
        safeNormalizePresentationAxis(
            from.radialAxis + (to.radialAxis - from.radialAxis) * t,
            to.radialAxis
        );

    glm::dvec3 progradeCandidate =
        from.progradeAxis +
        (to.progradeAxis - from.progradeAxis) * t;

    glm::dvec3 prograde =
        safeNormalizePresentationAxis(
            progradeCandidate -
                radial * glm::dot(progradeCandidate, radial),
            to.progradeAxis
        );

    glm::dvec3 normal =
        safeNormalizePresentationAxis(
            glm::cross(prograde, radial),
            to.normalAxis
        );

    prograde =
        safeNormalizePresentationAxis(
            glm::cross(radial, normal),
            prograde
        );

    out.radialAxis = radial;
    out.progradeAxis = prograde;
    out.normalAxis = normal;

    out.localPositionMeters =
        from.localPositionMeters +
        (to.localPositionMeters - from.localPositionMeters) * t;

    out.localVelocityMetersPerSecond =
        from.localVelocityMetersPerSecond +
        (to.localVelocityMetersPerSecond -
         from.localVelocityMetersPerSecond) * t;

    out.universeTimeSeconds =
        from.universeTimeSeconds +
        (to.universeTimeSeconds - from.universeTimeSeconds) * t;

    out.valid = true;
    return out;
}

} // namespace game::client
