#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace game::navigation
{

struct NavigationSolution
{
    world::coordinates::WorldPosition estimatedWorldPosition;
    glm::dvec3 estimatedWorldVelocityMps {0.0};

    double epochUniverseTimeSeconds = 0.0;
    double positionUncertaintyMeters = 0.0;
    double velocityUncertaintyMps = 0.0;

    // Changes only when a navigation fix changes the bias/error solution.
    // Ordinary propagation does not increment this revision.
    std::uint64_t fixRevision = 0;
};

} // namespace game::navigation
