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
        double crossTrackErrorMeters = 0.0;
        double linearVelocityErrorMps = 0.0;
        double forwardAngleErrorRad = 0.0;
        double angularVelocityErrorRadPerSec = 0.0;
        bool trackingErrorExceeded = false;

        // Bounded B10 correction is kept distinct from the Planner-owned
        // feed-forward actuator schedule all the way to physics.
        glm::dvec3 linearFeedbackLocalMps2 {0.0};
        glm::dvec3 angularFeedbackLocalRadPerSec2 {0.0};

        // Planner-owned actuator schedule sampled for the current interval.
        // Autopilot execution will consume these explicitly; exposing them here
        // prevents downstream code from having to re-infer engine choice from
        // a net acceleration vector.
        bool hasActuatorCommand = false;
        std::size_t actuatorSegmentIndex = 0;
        double rearMainThrottle01 = 0.0;
        double foreMainThrottle01 = 0.0;
        glm::dvec3 manoeuvreAccelerationMapMps2 {0.0};
        bool propulsionFeasible = true;
    };

    [[nodiscard]] static Result follow(
        const AcceptedManeuverProgram& program,
        double universeTimeSeconds,
        const AgentState& agent,
        const ManeuverTrackingController::Policy& trackingPolicy
    ) noexcept;

};

} // namespace game::navigation
