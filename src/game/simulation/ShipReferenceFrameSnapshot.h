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

// THE projection of a ship's spatial truth (DynamicMotionState: travel frame + local state) into its replicated
// representation - the same for full ships and lightweight shells, in every motion mode (FreeFlight and J are
// HubTactical with a detached frame). valid <=> the travel frame is valid.
inline ShipReferenceFrameSnapshot projectShipReferenceFrame(
    const game::navigation::DynamicMotionState& motion,
    double universeTimeSeconds)
{
    ShipReferenceFrameSnapshot out;
    out.systemId = motion.systemId;
    out.frameId = motion.travelFrame.frameId;
    out.matchedToReferenceFrame = motion.matchedToReferenceFrame;
    out.type = motion.mode;
    out.bodyId = motion.parentBodyId;
    out.hubId = motion.hubId;
    out.localPositionMeters = motion.localPositionMeters;
    out.localVelocityMetersPerSecond = motion.localVelocityMps;
    out.universeTimeSeconds = universeTimeSeconds;
    if (!motion.travelFrame.valid)
        return out;
    const auto& frame = motion.travelFrame;
    out.systemId = frame.systemId;
    out.frameId = frame.frameId;
    out.originMeters = frame.originMeters;
    out.velocityMetersPerSecond = frame.linearVelocityMps;
    out.accelerationMetersPerSecond2 = frame.linearAccelerationMps2;
    out.angularVelocityWorldRadPerSecond = frame.angularVelocityWorldRadPerSecond;
    out.angularAccelerationWorldRadPerSecond2 = frame.angularAccelerationWorldRadPerSecond2;
    out.progradeAxis = frame.localToWorldBasis[0];
    out.radialAxis = frame.localToWorldBasis[1];
    out.normalAxis = frame.localToWorldBasis[2];
    out.valid = true;
    return out;
}

// The inverse, on the client replica: the motion fields that do NOT ride the wire (the referenceFrame is their one
// representation) are rebuilt from it. Called once per decoded row, before any client reader sees the snapshot.
inline void restoreMotionFromReferenceFrame(
    game::navigation::DynamicMotionState& motion,
    const ShipReferenceFrameSnapshot& frame)
{
    if (!frame.valid)
        return;
    motion.travelFrame = frame.kinematicFrame();
    motion.localPositionMeters = frame.localPositionMeters;
    motion.localVelocityMps = frame.localVelocityMetersPerSecond;
    motion.referenceVelocityMps = frame.localToWorldVelocity(frame.localPositionMeters, glm::dvec3(0.0));
}

} // namespace game::simulation
