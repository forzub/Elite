#pragma once

#include <cmath>
#include <string>

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

#include "src/game/navigation/KinematicFrame.h"

namespace game::navigation
{

inline glm::dvec3 normalizedReplicatedHubAxis(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double length2 = glm::dot(value, value);
    if (!std::isfinite(length2) || length2 <= 1.0e-18)
        return fallback;
    return value / std::sqrt(length2);
}

/*
    Rebuild the tactical Hub frame from one replicated Hub snapshot.

    Replicated hub.orientation is the visual/model basis:
        X = normal, Y = radial, Z = -prograde.

    Navigation/Hub-map coordinates are:
        X = prograde, Y = radial, Z = normal.

    Keeping this conversion in navigation core prevents map presentation and
    route planning from growing independent axis permutations.
*/
inline KinematicFrame makeReplicatedHubKinematicFrame(
    int systemId,
    const std::string& hubId,
    const glm::dvec3& hubPositionMeters,
    const glm::dvec3& hubVelocityMps,
    const glm::dvec3& angularVelocityWorldRadPerSecond,
    const glm::mat4& replicatedOrientation
) noexcept
{
    KinematicFrame frame;
    frame.systemId = systemId;
    frame.frameId = hubId;
    frame.originMeters = hubPositionMeters;
    frame.linearVelocityMps = hubVelocityMps;
    frame.angularVelocityWorldRadPerSecond =
        angularVelocityWorldRadPerSecond;

    glm::dvec3 normal = normalizedReplicatedHubAxis(
        glm::dvec3(replicatedOrientation[0]),
        glm::dvec3(0.0, 0.0, 1.0)
    );
    const glm::dvec3 radial = normalizedReplicatedHubAxis(
        glm::dvec3(replicatedOrientation[1]),
        glm::dvec3(0.0, 1.0, 0.0)
    );
    glm::dvec3 prograde = normalizedReplicatedHubAxis(
        -glm::dvec3(replicatedOrientation[2]),
        glm::dvec3(1.0, 0.0, 0.0)
    );

    normal = normalizedReplicatedHubAxis(
        glm::cross(prograde, radial),
        normal
    );
    prograde = normalizedReplicatedHubAxis(
        glm::cross(radial, normal),
        prograde
    );

    frame.localToWorldBasis = glm::dmat3(prograde, radial, normal);
    frame.valid =
        systemId >= 0 &&
        !hubId.empty() &&
        std::isfinite(hubPositionMeters.x) &&
        std::isfinite(hubPositionMeters.y) &&
        std::isfinite(hubPositionMeters.z) &&
        std::isfinite(hubVelocityMps.x) &&
        std::isfinite(hubVelocityMps.y) &&
        std::isfinite(hubVelocityMps.z);
    return frame;
}

} // namespace game::navigation
