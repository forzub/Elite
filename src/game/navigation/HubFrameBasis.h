#pragma once

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace game::navigation
{

/*
    Orbital hubs expose two intentionally different local bases:

      tactical/navigation: X=prograde, Y=radial, Z=normal
      visual/model:         X=normal,   Y=radial, Z=-prograde

    The distinction is part of the coordinate contract. Keep all conversions
    here so server simulation, maps and client presentation cannot silently
    evolve different axis permutations.
*/
inline glm::dvec3 hubVisualLocalToWorldVector(
    const glm::dvec3& progradeAxis,
    const glm::dvec3& radialAxis,
    const glm::dvec3& normalAxis,
    const glm::dvec3& localVector
)
{
    return
        normalAxis * localVector.x +
        radialAxis * localVector.y -
        progradeAxis * localVector.z;
}

inline glm::dvec3 hubVisualLocalToWorldPosition(
    const glm::dvec3& originMeters,
    const glm::dvec3& progradeAxis,
    const glm::dvec3& radialAxis,
    const glm::dvec3& normalAxis,
    const glm::dvec3& localPositionMeters
)
{
    return originMeters +
        hubVisualLocalToWorldVector(
            progradeAxis,
            radialAxis,
            normalAxis,
            localPositionMeters);
}

inline glm::mat4 hubVisualOrientation(
    const glm::dvec3& progradeAxis,
    const glm::dvec3& radialAxis,
    const glm::dvec3& normalAxis
)
{
    glm::mat4 orientation(1.0f);
    orientation[0] = glm::vec4(glm::vec3(normalAxis), 0.0f);
    orientation[1] = glm::vec4(glm::vec3(radialAxis), 0.0f);
    orientation[2] = glm::vec4(glm::vec3(-progradeAxis), 0.0f);
    orientation[3] = glm::vec4(0, 0, 0, 1);
    return orientation;
}

inline glm::mat4 hubLocalEulerDegToMatrix(const glm::dvec3& degrees)
{
    glm::mat4 rotation(1.0f);

    if (std::abs(degrees.x) > 0.000001)
    {
        rotation = glm::rotate(
            rotation,
            glm::radians(static_cast<float>(degrees.x)),
            glm::vec3(1.0f, 0.0f, 0.0f));
    }

    if (std::abs(degrees.y) > 0.000001)
    {
        rotation = glm::rotate(
            rotation,
            glm::radians(static_cast<float>(degrees.y)),
            glm::vec3(0.0f, 1.0f, 0.0f));
    }

    if (std::abs(degrees.z) > 0.000001)
    {
        rotation = glm::rotate(
            rotation,
            glm::radians(static_cast<float>(degrees.z)),
            glm::vec3(0.0f, 0.0f, 1.0f));
    }

    return rotation;
}

inline glm::mat4 hubAttachedVisualOrientation(
    const glm::dvec3& progradeAxis,
    const glm::dvec3& radialAxis,
    const glm::dvec3& normalAxis,
    const glm::dvec3& localRotationDeg
)
{
    return
        hubVisualOrientation(progradeAxis, radialAxis, normalAxis) *
        hubLocalEulerDegToMatrix(localRotationDeg);
}

} // namespace game::navigation
