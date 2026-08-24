#include "src/game/navigation/HubKinematicEvaluator.h"

#include <cmath>

namespace game::navigation
{
namespace
{
bool finiteVec(const glm::dvec3& value) noexcept
{
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

glm::dvec3 normalizedAxis(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double length2 = glm::dot(value, value);
    if (!std::isfinite(length2) || length2 <= 1.0e-18)
        return fallback;
    return value / std::sqrt(length2);
}
}

KinematicFrame evaluateOrbitalHubKinematicFrameAt(
    int systemId,
    const std::string& hubId,
    const world::orbits::OrbitalMotion& motion,
    const glm::dvec3& parentPositionMeters,
    const glm::dvec3& parentVelocityMps,
    double universeTimeSeconds
)
{
    KinematicFrame out;
    out.systemId = systemId;
    out.frameId = hubId;

    if (systemId < 0 || hubId.empty() || !motion.enabled ||
        !std::isfinite(universeTimeSeconds) ||
        !finiteVec(parentPositionMeters) ||
        !finiteVec(parentVelocityMps))
    {
        return out;
    }

    world::orbits::OrbitalMotion resolvedMotion = motion;
    resolvedMotion.centerMeters = parentPositionMeters;

    const glm::dvec3 positionMeters =
        world::orbits::computeOrbitPositionMeters(
            resolvedMotion,
            universeTimeSeconds
        );
    const glm::dvec3 relativeVelocityMps =
        world::orbits::computeOrbitVelocityMetersPerSecond(
            resolvedMotion,
            universeTimeSeconds
        );
    const glm::dvec3 velocityMps =
        parentVelocityMps + relativeVelocityMps;

    const glm::dvec3 relativePositionMeters =
        positionMeters - parentPositionMeters;
    const double radiusMeters = glm::length(relativePositionMeters);
    if (!std::isfinite(radiusMeters) || radiusMeters <= 1.0)
        return out;

    const glm::dvec3 radial = normalizedAxis(
        relativePositionMeters,
        glm::dvec3(0.0, 1.0, 0.0)
    );

    glm::dvec3 tangentialVelocityMps =
        relativeVelocityMps -
        radial * glm::dot(relativeVelocityMps, radial);
    const glm::dvec3 prograde = normalizedAxis(
        tangentialVelocityMps,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 normal = normalizedAxis(
        glm::cross(prograde, radial),
        glm::dvec3(0.0, 0.0, 1.0)
    );
    const glm::dvec3 orthogonalPrograde = normalizedAxis(
        glm::cross(radial, normal),
        prograde
    );

    const double tangentialSpeedMps = glm::length(tangentialVelocityMps);
    const double angularSpeedRadPerSecond =
        tangentialSpeedMps / radiusMeters;
    const glm::dvec3 angularVelocityWorldRadPerSecond =
        -normal * angularSpeedRadPerSecond;

    out.originMeters = positionMeters;
    out.linearVelocityMps = velocityMps;
    out.linearAccelerationMps2 = glm::cross(
        angularVelocityWorldRadPerSecond,
        glm::cross(
            angularVelocityWorldRadPerSecond,
            relativePositionMeters
        )
    );
    out.angularVelocityWorldRadPerSecond =
        angularVelocityWorldRadPerSecond;
    out.localToWorldBasis = glm::dmat3(
        orthogonalPrograde,
        radial,
        normal
    );
    out.valid =
        finiteVec(out.originMeters) &&
        finiteVec(out.linearVelocityMps) &&
        finiteVec(out.linearAccelerationMps2) &&
        finiteVec(out.angularVelocityWorldRadPerSecond);
    return out;
}

} // namespace game::navigation
