#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace game::navigation
{

// Immutable execution product accepted from one local planning epoch.
//
// The planner owns obstacle/topology search. Once this product is accepted,
// fixed-step execution consumes it without invoking those searches again.
// Validity/monitoring decides when a later planning epoch is required.
struct AcceptedShortSegment
{
    enum class LinearMode : std::uint8_t
    {
        VelocityTracking = 0,
        FixedAcceleration
    };

    struct CapabilitySnapshot
    {
        double maxForwardAccelerationMetersPerSec2 = 0.0;
        double maxReverseAccelerationMetersPerSec2 = 0.0;
        double maxLateralAccelerationMetersPerSec2 = 0.0;
        double maxVerticalAccelerationMetersPerSec2 = 0.0;
        double maxAngularAccelerationRadPerSec2 = 0.0;
        double maxAngularSpeedRadPerSec = 0.0;
    };

    bool valid = false;

    std::uint64_t revision = 0;
    std::uint64_t goalRevision = 0;

    double acceptedAtUniverseTimeSeconds = 0.0;
    double validUntilUniverseTimeSeconds = 0.0;

    std::uint64_t mapRevision = 0;
    std::uint64_t mapSourceRevision = 0;
    std::uint64_t spaceRevision = 0;
    std::uint64_t spaceSourceRevision = 0;

    glm::dvec3 startPositionMapMeters {0.0};
    glm::dvec3 targetPositionMapMeters {0.0};
    glm::dvec3 targetVelocityMapMetersPerSecond {0.0};

    LinearMode linearMode = LinearMode::VelocityTracking;
    glm::dvec3 fixedLinearAccelerationMapMps2 {0.0};
    double velocityResponsePerSecond = 0.0;

    bool alignForward = false;
    glm::dvec3 desiredForwardMap {0.0, 0.0, -1.0};
    double angularDampingPerSecond = 0.0;
    double orientationResponsePerSecond2 = 0.0;
    double maximumAngularAccelerationRadPerSec2 = 0.0;

    bool completionTriggersReplan = true;
    double completionRadiusMeters = 0.0;
    double trackingEnvelopeRadiusMeters = 0.0;

    bool emergency = false;
    double hazardUrgency01 = 0.0;

    CapabilitySnapshot capability {};
};

} // namespace game::navigation
