#pragma once

#include <string>

#include <glm/glm.hpp>
#include <glm/mat3x3.hpp>

namespace game::navigation
{

// Generic moving/accelerating reference frame used by hubs, ships and future
// travel frames. Coordinates are system-local precise meters; galactic system
// roots and player-relative render coordinates belong to different layers.
struct KinematicFrame
{
    int systemId = -1;
    std::string frameId;

    glm::dvec3 originMeters {0.0};
    glm::dvec3 linearVelocityMps {0.0};
    glm::dvec3 linearAccelerationMps2 {0.0};

    // Columns are local X/Y/Z axes expressed in world coordinates.
    glm::dmat3 localToWorldBasis {1.0};

    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};
    glm::dvec3 angularAccelerationWorldRadPerSecond2 {0.0};

    bool valid = false;

    glm::dvec3 localToWorldVector(const glm::dvec3& localVector) const
    {
        return localToWorldBasis * localVector;
    }

    glm::dvec3 worldToLocalVector(const glm::dvec3& worldVector) const
    {
        return glm::transpose(localToWorldBasis) * worldVector;
    }

    glm::dvec3 localToWorldPosition(const glm::dvec3& localPosition) const
    {
        return originMeters + localToWorldVector(localPosition);
    }

    glm::dvec3 worldToLocalPosition(const glm::dvec3& worldPosition) const
    {
        return worldToLocalVector(worldPosition - originMeters);
    }

    glm::dvec3 localToWorldVelocity(
        const glm::dvec3& localPosition,
        const glm::dvec3& localVelocity) const
    {
        const glm::dvec3 worldOffset = localToWorldVector(localPosition);

        return linearVelocityMps +
            glm::cross(angularVelocityWorldRadPerSecond, worldOffset) +
            localToWorldVector(localVelocity);
    }

    glm::dvec3 worldToLocalVelocity(
        const glm::dvec3& worldPosition,
        const glm::dvec3& worldVelocity) const
    {
        const glm::dvec3 worldOffset = worldPosition - originMeters;

        return worldToLocalVector(
            worldVelocity -
            linearVelocityMps -
            glm::cross(angularVelocityWorldRadPerSecond, worldOffset)
        );
    }

    glm::dvec3 localToWorldAcceleration(
        const glm::dvec3& localPosition,
        const glm::dvec3& localVelocity,
        const glm::dvec3& localAcceleration) const
    {
        const glm::dvec3 worldOffset = localToWorldVector(localPosition);
        const glm::dvec3 relativeWorldVelocity =
            localToWorldVector(localVelocity);

        return linearAccelerationMps2 +
            glm::cross(
                angularAccelerationWorldRadPerSecond2,
                worldOffset
            ) +
            glm::cross(
                angularVelocityWorldRadPerSecond,
                glm::cross(
                    angularVelocityWorldRadPerSecond,
                    worldOffset
                )
            ) +
            2.0 * glm::cross(
                angularVelocityWorldRadPerSecond,
                relativeWorldVelocity
            ) +
            localToWorldVector(localAcceleration);
    }

    glm::dvec3 worldToLocalAcceleration(
        const glm::dvec3& worldPosition,
        const glm::dvec3& worldVelocity,
        const glm::dvec3& worldAcceleration) const
    {
        const glm::dvec3 worldOffset = worldPosition - originMeters;
        const glm::dvec3 localVelocity =
            worldToLocalVelocity(worldPosition, worldVelocity);
        const glm::dvec3 relativeWorldVelocity =
            localToWorldVector(localVelocity);

        const glm::dvec3 frameTerms =
            linearAccelerationMps2 +
            glm::cross(
                angularAccelerationWorldRadPerSecond2,
                worldOffset
            ) +
            glm::cross(
                angularVelocityWorldRadPerSecond,
                glm::cross(
                    angularVelocityWorldRadPerSecond,
                    worldOffset
                )
            ) +
            2.0 * glm::cross(
                angularVelocityWorldRadPerSecond,
                relativeWorldVelocity
            );

        return worldToLocalVector(worldAcceleration - frameTerms);
    }
};

struct LocalKinematicState
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 accelerationMps2 {0.0};
};

struct WorldKinematicState
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 accelerationMps2 {0.0};
};

inline WorldKinematicState localToWorldKinematics(
    const KinematicFrame& frame,
    const LocalKinematicState& local)
{
    return {
        frame.localToWorldPosition(local.positionMeters),
        frame.localToWorldVelocity(local.positionMeters, local.velocityMps),
        frame.localToWorldAcceleration(
            local.positionMeters,
            local.velocityMps,
            local.accelerationMps2
        )
    };
}

inline LocalKinematicState worldToLocalKinematics(
    const KinematicFrame& frame,
    const WorldKinematicState& world)
{
    const glm::dvec3 localPosition =
        frame.worldToLocalPosition(world.positionMeters);
    const glm::dvec3 localVelocity =
        frame.worldToLocalVelocity(
            world.positionMeters,
            world.velocityMps
        );

    return {
        localPosition,
        localVelocity,
        frame.worldToLocalAcceleration(
            world.positionMeters,
            world.velocityMps,
            world.accelerationMps2
        )
    };
}

inline bool rebaseLocalKinematics(
    const KinematicFrame& from,
    const KinematicFrame& to,
    const LocalKinematicState& source,
    LocalKinematicState& rebased)
{
    if (!from.valid || !to.valid || from.systemId != to.systemId)
        return false;

    rebased = worldToLocalKinematics(
        to,
        localToWorldKinematics(from, source)
    );
    return true;
}

} // namespace game::navigation
