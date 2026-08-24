#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/game/navigation/KinematicFrame.h"

namespace game::navigation
{

/*
    Short-horizon circular-orbit continuation of a Hub reference frame.

    The server already models orbital hubs as a circular orbit around the
    parent body. Local docking/navigation must therefore not extrapolate the
    hub on the instantaneous world-space tangent while the ship continues to
    follow gravity. This seed keeps the common parent translation separate and
    advances the hub's relative radius/prograde basis as one co-moving frame.

    It is deliberately a local-guidance primitive, not a long-range ephemeris.
    Parent motion is linear over the short planning horizon; the hub's relative
    circular motion is exact for the current radius/angular rate.
*/
struct HubCoMovingFrameSeed
{
    int systemId = -1;
    std::string hubId;
    double epochUniverseTimeSeconds = 0.0;

    glm::dvec3 hubPositionMeters {0.0};
    glm::dvec3 hubVelocityMps {0.0};
    glm::dvec3 parentPositionMeters {0.0};
    glm::dvec3 parentVelocityMps {0.0};

    glm::dvec3 progradeAxis {1.0, 0.0, 0.0};
    glm::dvec3 radialAxis {0.0, 1.0, 0.0};
    glm::dvec3 normalAxis {0.0, 0.0, 1.0};

    bool valid = false;
};

inline glm::dvec3 normalizedHubFrameAxis(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double length2 = glm::dot(value, value);
    if (!std::isfinite(length2) || length2 <= 1.0e-18)
        return fallback;
    return value / std::sqrt(length2);
}

inline HubCoMovingFrameSeed makeHubCoMovingFrameSeed(
    int systemId,
    const std::string& hubId,
    double epochUniverseTimeSeconds,
    const glm::dvec3& hubPositionMeters,
    const glm::dvec3& hubVelocityMps,
    const glm::dvec3& parentPositionMeters,
    const glm::dvec3& parentVelocityMps,
    const glm::dvec3& fallbackPrograde = glm::dvec3(1.0, 0.0, 0.0),
    const glm::dvec3& fallbackRadial = glm::dvec3(0.0, 1.0, 0.0),
    const glm::dvec3& fallbackNormal = glm::dvec3(0.0, 0.0, 1.0)
)
{
    HubCoMovingFrameSeed seed;
    seed.systemId = systemId;
    seed.hubId = hubId;
    seed.epochUniverseTimeSeconds = epochUniverseTimeSeconds;
    seed.hubPositionMeters = hubPositionMeters;
    seed.hubVelocityMps = hubVelocityMps;
    seed.parentPositionMeters = parentPositionMeters;
    seed.parentVelocityMps = parentVelocityMps;

    seed.radialAxis = normalizedHubFrameAxis(
        hubPositionMeters - parentPositionMeters,
        fallbackRadial
    );

    glm::dvec3 relativeVelocity = hubVelocityMps - parentVelocityMps;
    relativeVelocity -= seed.radialAxis *
        glm::dot(relativeVelocity, seed.radialAxis);
    seed.progradeAxis = normalizedHubFrameAxis(
        relativeVelocity,
        fallbackPrograde
    );

    seed.normalAxis = normalizedHubFrameAxis(
        glm::cross(seed.progradeAxis, seed.radialAxis),
        fallbackNormal
    );
    seed.progradeAxis = normalizedHubFrameAxis(
        glm::cross(seed.radialAxis, seed.normalAxis),
        seed.progradeAxis
    );

    seed.valid =
        systemId >= 0 &&
        !hubId.empty() &&
        std::isfinite(epochUniverseTimeSeconds) &&
        glm::length(hubPositionMeters - parentPositionMeters) > 1.0;
    return seed;
}

inline KinematicFrame predictHubCoMovingFrameAt(
    const HubCoMovingFrameSeed& seed,
    double universeTimeSeconds
)
{
    KinematicFrame frame;
    frame.systemId = seed.systemId;
    frame.frameId = seed.hubId;
    if (!seed.valid || !std::isfinite(universeTimeSeconds))
        return frame;

    const double dt = universeTimeSeconds - seed.epochUniverseTimeSeconds;
    const glm::dvec3 epochRelative =
        seed.hubPositionMeters - seed.parentPositionMeters;
    const double radiusMeters = glm::length(epochRelative);

    glm::dvec3 relativeVelocity =
        seed.hubVelocityMps - seed.parentVelocityMps;
    relativeVelocity -= seed.radialAxis *
        glm::dot(relativeVelocity, seed.radialAxis);
    const double tangentialSpeedMps = glm::length(relativeVelocity);
    const double angularSpeedRadPerSecond = radiusMeters > 1.0
        ? tangentialSpeedMps / radiusMeters
        : 0.0;

    // Basis convention: X=prograde, Y=radial, Z=normal and
    // normal=cross(prograde, radial), so positive prograde orbit rotates
    // around -normal.
    const glm::dvec3 angularVelocity =
        -seed.normalAxis * angularSpeedRadPerSecond;

    glm::dquat delta(1.0, 0.0, 0.0, 0.0);
    if (angularSpeedRadPerSecond > 1.0e-12 && std::abs(dt) > 1.0e-12)
    {
        delta = glm::normalize(glm::angleAxis(
            angularSpeedRadPerSecond * dt,
            angularVelocity / angularSpeedRadPerSecond
        ));
    }

    const glm::dvec3 parentPosition =
        seed.parentPositionMeters + seed.parentVelocityMps * dt;
    const glm::dvec3 relativePosition = delta * epochRelative;

    frame.originMeters = parentPosition + relativePosition;
    frame.linearVelocityMps =
        seed.parentVelocityMps + glm::cross(angularVelocity, relativePosition);
    frame.linearAccelerationMps2 =
        glm::cross(
            angularVelocity,
            glm::cross(angularVelocity, relativePosition)
        );
    frame.angularVelocityWorldRadPerSecond = angularVelocity;

    const glm::dvec3 prograde = normalizedHubFrameAxis(
        delta * seed.progradeAxis,
        seed.progradeAxis
    );
    const glm::dvec3 radial = normalizedHubFrameAxis(
        delta * seed.radialAxis,
        seed.radialAxis
    );
    const glm::dvec3 normal = normalizedHubFrameAxis(
        glm::cross(prograde, radial),
        seed.normalAxis
    );
    const glm::dvec3 orthogonalPrograde = normalizedHubFrameAxis(
        glm::cross(radial, normal),
        prograde
    );

    frame.localToWorldBasis = glm::dmat3(
        orthogonalPrograde,
        radial,
        normal
    );
    frame.valid = true;
    return frame;
}

} // namespace game::navigation
