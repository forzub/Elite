#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/NavigationControlIntent.h"

namespace game::navigation::autopilot
{

struct RouteFollowerPolicy
{
    double positionGainPerSecond2 = 0.50;
    double velocityGainPerSecond = 1.00;
    double attitudeGainPerSecond2 = 2.00;
    double angularVelocityGainPerSecond = 3.00;
};

struct RouteFollowerAgentState
{
    glm::dvec3 positionMapMeters {0.0};
    glm::dvec3 velocityMapMetersPerSecond {0.0};
    glm::dvec3 forwardMap {0.0, 0.0, -1.0};
    glm::dvec3 rightMap {1.0, 0.0, 0.0};
    glm::dvec3 upMap {0.0, 1.0, 0.0};
    double pitchRateRadPerSec = 0.0;
    double yawRateRadPerSec = 0.0;
    double rollRateRadPerSec = 0.0;
};

enum class RouteFollowerStatus : std::uint8_t
{
    InvalidInput = 0,
    Following,
    Complete
};

struct RouteFollowerResult
{
    RouteFollowerStatus status = RouteFollowerStatus::InvalidInput;
    NavigationLocalControlIntent intent {};

    double remainingDistanceMeters = 0.0;
    glm::dvec3 targetVelocityMapMps {0.0};
    double crossTrackErrorMeters = 0.0;
    double linearVelocityErrorMps = 0.0;
    double envelopePositionErrorMeters = 0.0;
    double envelopeVelocityErrorMps = 0.0;
    double forwardAngleErrorRad = 0.0;
    double envelopeForwardAngleErrorRad = 0.0;
    double angularVelocityErrorRadPerSec = 0.0;
    bool trackingErrorExceeded = false;
    bool angularCorrectionOnly = false;

    bool spatialReference = false;
    std::size_t referenceLowerSampleIndex = 0;
    std::size_t referenceUpperSampleIndex = 0;
    double referenceInterpolation01 = 0.0;
    double referenceSpatialDistanceMeters = 0.0;
    double spatialSpeedScale = 1.0;
};

class RouteFollower final
{
public:
    [[nodiscard]] static RouteFollowerResult follow(
        const AcceptedManeuverProgram& program,
        double universeTimeSeconds,
        const RouteFollowerAgentState& agent,
        const RouteFollowerPolicy& policy,
        std::size_t minimumSpatialSegmentIndex = 0
    ) noexcept;
};

} // namespace game::navigation::autopilot
