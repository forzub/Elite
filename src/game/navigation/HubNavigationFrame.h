#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

namespace game::navigation
{

struct HubNavigationFrame
{
    std::string hubId;
    std::string parentBodyId;
    std::string primeModuleId;

    glm::dvec3 originMeters {0.0};
    glm::dvec3 velocityMetersPerSecond {0.0};

    // Angular velocity of the rotating hub frame in world coordinates.
    // A point fixed at a non-zero local offset therefore has a different
    // world velocity than the frame origin.
    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};

    // От центра планеты к хабу.
    glm::dvec3 radialAxis {0.0, 1.0, 0.0};

    // Вдоль движения хаба.
    glm::dvec3 progradeAxis {1.0, 0.0, 0.0};

    // Нормаль орбитального коридора.
    glm::dvec3 normalAxis {0.0, 0.0, 1.0};

    bool valid = false;

    glm::dvec3 worldToLocalPosition(
        const glm::dvec3& worldMeters
    ) const
    {
        const glm::dvec3 d =
            worldMeters - originMeters;

        return {
            glm::dot(d, progradeAxis),
            glm::dot(d, radialAxis),
            glm::dot(d, normalAxis)
        };
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

    glm::dvec3 worldToLocalVector(
        const glm::dvec3& worldVector
    ) const
    {
        return {
            glm::dot(worldVector, progradeAxis),
            glm::dot(worldVector, radialAxis),
            glm::dot(worldVector, normalAxis)
        };
    }

    glm::dvec3 worldToLocalVelocity(
        const glm::dvec3& worldPositionMeters,
        const glm::dvec3& worldVelocity
    ) const
    {
        const glm::dvec3 worldOffset =
            worldPositionMeters - originMeters;

        const glm::dvec3 rotatingFrameVelocity =
            glm::cross(
                angularVelocityWorldRadPerSecond,
                worldOffset
            );

        return worldToLocalVector(
            worldVelocity -
            velocityMetersPerSecond -
            rotatingFrameVelocity
        );
    }

    glm::dvec3 localToWorldVelocity(
        const glm::dvec3& localPositionMeters,
        const glm::dvec3& localVelocity
    ) const
    {
        const glm::dvec3 worldOffset =
            localToWorldVector(localPositionMeters);

        return
            velocityMetersPerSecond +
            glm::cross(
                angularVelocityWorldRadPerSecond,
                worldOffset
            ) +
            localToWorldVector(localVelocity);
    }
};

} // namespace game::navigation