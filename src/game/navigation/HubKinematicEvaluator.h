#pragma once

#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/KinematicFrame.h"
#include "src/world/orbits/OrbitalMotion.h"

namespace game::navigation
{

/*
    Shared server/client evaluator for one analytically defined orbital Hub.

    This is physical/navigation state, not presentation state.  Callers supply
    the parent body's world position/velocity at the requested universe epoch;
    the evaluator applies the authored OrbitalMotion once and returns the
    canonical tactical Hub frame:

        X = prograde, Y = radial, Z = normal.

    Server simulation and client-side planning prediction must use this same
    evaluator rather than maintaining separate orbit/basis implementations.
*/
KinematicFrame evaluateOrbitalHubKinematicFrameAt(
    int systemId,
    const std::string& hubId,
    const world::orbits::OrbitalMotion& motion,
    const glm::dvec3& parentPositionMeters,
    const glm::dvec3& parentVelocityMps,
    double universeTimeSeconds
);

} // namespace game::navigation
