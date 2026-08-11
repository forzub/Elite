#pragma once

#include <string>
#include <glm/glm.hpp>

#include "src/game/navigation/DynamicMotionState.h"
#include "src/game/navigation/KinematicFrame.h"

namespace game::simulation
{

// Authoritative spatial context for a ship at one server snapshot tick.
// Local coordinates are the source of truth while valid is true;
// world coordinates remain a derived compatibility representation.
struct ShipReferenceFrameSnapshot
{
    int systemId = -1;
    std::string frameId;
    bool matchedToReferenceFrame = false;

    game::navigation::MotionMode type =
        game::navigation::MotionMode::Inertial;

    std::string bodyId;
    std::string hubId;
    std::string moduleId;

    glm::dvec3 originMeters {0.0};
    glm::dvec3 velocityMetersPerSecond {0.0};
    glm::dvec3 accelerationMetersPerSecond2 {0.0};
    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};
    glm::dvec3 angularAccelerationWorldRadPerSecond2 {0.0};

    glm::dvec3 radialAxis {0.0, 1.0, 0.0};
    glm::dvec3 progradeAxis {1.0, 0.0, 0.0};
    glm::dvec3 normalAxis {0.0, 0.0, 1.0};

    glm::dvec3 localPositionMeters {0.0};
    glm::dvec3 localVelocityMetersPerSecond {0.0};

    double universeTimeSeconds = 0.0;
    bool valid = false;

    game::navigation::KinematicFrame kinematicFrame() const
    {
        game::navigation::KinematicFrame frame;
        frame.systemId = systemId;
        frame.frameId = frameId.empty() ? hubId : frameId;
        frame.originMeters = originMeters;
        frame.linearVelocityMps = velocityMetersPerSecond;
        frame.linearAccelerationMps2 = accelerationMetersPerSecond2;
        frame.localToWorldBasis = glm::dmat3(
            progradeAxis,
            radialAxis,
            normalAxis
        );
        frame.angularVelocityWorldRadPerSecond =
            angularVelocityWorldRadPerSecond;
        frame.angularAccelerationWorldRadPerSecond2 =
            angularAccelerationWorldRadPerSecond2;
        frame.valid = valid;
        return frame;
    }

    glm::dvec3 localToWorldPosition(
        const glm::dvec3& localMeters
    ) const
    {
        return originMeters
            + progradeAxis * localMeters.x
            + radialAxis   * localMeters.y
            + normalAxis   * localMeters.z;
    }

    glm::dvec3 localToWorldVector(
        const glm::dvec3& localVector
    ) const
    {
        return
            progradeAxis * localVector.x +
            radialAxis   * localVector.y +
            normalAxis   * localVector.z;
    }

    glm::dvec3 localToWorldVelocity(
        const glm::dvec3& localPosition,
        const glm::dvec3& localVelocity
    ) const
    {
        const glm::dvec3 worldOffset =
            localToWorldVector(localPosition);

        return
            velocityMetersPerSecond +
            glm::cross(
                angularVelocityWorldRadPerSecond,
                worldOffset
            ) +
            localToWorldVector(localVelocity);
    }
};

} // namespace game::simulation
