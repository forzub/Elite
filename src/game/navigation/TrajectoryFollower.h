#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/ManeuverTrackingController.h"
#include "src/game/navigation/NavigationControlIntent.h"

namespace game::navigation
{

// Fixed-step execution seam.
//
// Navigation-v2 path:
//   AcceptedManeuverProgram
//     -> ManeuverProgramSampler (B9)
//     -> ManeuverTrackingController (B10)
//     -> NavigationLocalControlIntent
//
class TrajectoryFollower final
{
public:
    struct AgentState
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

    enum class Status : std::uint8_t
    {
        InvalidInput = 0,
        Following,
        Complete
    };

    struct Result
    {
        Status status = Status::InvalidInput;
        NavigationLocalControlIntent intent {};

        double remainingDistanceMeters = 0.0;
        glm::dvec3 targetVelocityMapMps {0.0};
        double crossTrackErrorMeters = 0.0;
        double linearVelocityErrorMps = 0.0;
        double envelopePositionErrorMeters = 0.0;
        double envelopeVelocityErrorMps = 0.0;
        double forwardAngleErrorRad = 0.0;
        double angularVelocityErrorRadPerSec = 0.0;
        bool trackingErrorExceeded = false;
        bool angularCorrectionOnly = false;

        bool spatialReference = false;
        std::size_t referenceLowerSampleIndex = 0;
        std::size_t referenceUpperSampleIndex = 0;
        double referenceInterpolation01 = 0.0;
        double referenceSpatialDistanceMeters = 0.0;

        // The accepted reference is a vehicle-motion program. Tracking
        // feedback remains inside intent, with no propulsion allocation here.
    };

    [[nodiscard]] static Result follow(
        const AcceptedManeuverProgram& program,
        double universeTimeSeconds,
        const AgentState& agent,
        const ManeuverTrackingController::Policy& trackingPolicy,
        std::size_t minimumSpatialSegmentIndex = 0
    ) noexcept;

};

} // namespace game::navigation
