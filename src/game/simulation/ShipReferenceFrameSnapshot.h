#pragma once

#include <string>
#include <glm/glm.hpp>

#include "src/game/navigation/DynamicMotionState.h"

namespace game::simulation
{

// Authoritative spatial context for a ship at one server snapshot tick.
// Local coordinates are the source of truth while valid is true;
// world coordinates remain a derived compatibility representation.
struct ShipReferenceFrameSnapshot
{
    game::navigation::MotionMode type =
        game::navigation::MotionMode::Inertial;

    std::string bodyId;
    std::string hubId;
    std::string moduleId;

    glm::dvec3 originMeters {0.0};
    glm::dvec3 velocityMetersPerSecond {0.0};

    glm::dvec3 radialAxis {0.0, 1.0, 0.0};
    glm::dvec3 progradeAxis {1.0, 0.0, 0.0};
    glm::dvec3 normalAxis {0.0, 0.0, 1.0};

    glm::dvec3 localPositionMeters {0.0};
    glm::dvec3 localVelocityMetersPerSecond {0.0};

    double universeTimeSeconds = 0.0;
    bool valid = false;

    glm::dvec3 localToWorldPosition(
        const glm::dvec3& localMeters
    ) const
    {
        return originMeters
            + progradeAxis * localMeters.x
            + radialAxis   * localMeters.y
            + normalAxis   * localMeters.z;
    }

    glm::dvec3 localToWorldVelocity(
        const glm::dvec3& localVelocity
    ) const
    {
        return velocityMetersPerSecond
            + progradeAxis * localVelocity.x
            + radialAxis   * localVelocity.y
            + normalAxis   * localVelocity.z;
    }
};

} // namespace game::simulation
