#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "src/game/navigation/AcceptedShortSegment.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"

namespace game::navigation
{

// Fixed-step executor for one AcceptedShortSegment.
//
// This class deliberately has no NavigationMap/NavigationSpace dependency and
// cannot perform obstacle search. It converts the already accepted local
// execution product plus current kinematics into control intent.
class TrajectoryFollower final
{
public:
    using Bridge = NavigationRuntimeControlBridge;

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
        Bridge::Intent intent {};

        double remainingDistanceMeters = 0.0;
        double crossTrackErrorMeters = 0.0;
        bool trackingErrorExceeded = false;
    };

    [[nodiscard]] static Result follow(
        const AcceptedShortSegment& segment,
        const AgentState& agent
    ) noexcept;
};

} // namespace game::navigation
