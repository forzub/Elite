#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace game::navigation
{

// Immutable execution product accepted from one local planning epoch.
//
// TRANSITIONAL Stage-12 shape:
// this type currently carries a target position/velocity or one fixed
// acceleration plus optional attitude alignment. That is insufficient for the
// final Navigation-v2 contract because the follower can re-derive a different
// control history from the trajectory that was proved.
//
// Migration target: AcceptedManeuverProgram containing the same bounded
// time-parameterized reference state and feed-forward control that passed
// capability + geometry proof:
//     P(t), V(t), A_ff(t), q(t), omega(t), alpha_ff(t)
// Fixed-step execution then samples that program and adds only bounded tracking
// feedback. See NAVIGATION_COMMAND_OWNERSHIP.md.
//
// Until migration completes, validity/monitoring still decides when a later
// planning epoch is required.
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
