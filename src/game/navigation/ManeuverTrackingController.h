#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/NavigationControlIntent.h"

namespace game::navigation
{

// B10 — bounded tracking controller.
//
// Consumes one already-sampled B9 reference plus actual vehicle kinematics.
// It owns no world queries and may only add bounded feedback inside the reserve
// that B6/B8 left available for execution.
class ManeuverTrackingController final
{
public:
    struct Policy
    {
        double positionGainPerSecond2 = 0.50;
        double velocityGainPerSecond = 1.00;
        double attitudeGainPerSecond2 = 2.00;
        double angularVelocityGainPerSecond = 1.00;
    };

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
        Tracking,
        EnvelopeExceeded
    };

    struct Result
    {
        Status status = Status::InvalidInput;
        NavigationLocalControlIntent intent {};

        glm::dvec3 linearFeedbackMapMps2 {0.0};
        glm::dvec3 angularFeedbackMapRadPerSec2 {0.0};

        double positionErrorMeters = 0.0;
        double linearVelocityErrorMps = 0.0;
        double forwardAngleErrorRad = 0.0;
        double angularVelocityErrorRadPerSec = 0.0;
    };

    [[nodiscard]] static Result track(
        const AcceptedManeuverProgram& program,
        const AcceptedManeuverProgram::ReferenceSample& reference,
        const AgentState& agent
    ) noexcept;

    [[nodiscard]] static Result track(
        const AcceptedManeuverProgram& program,
        const AcceptedManeuverProgram::ReferenceSample& reference,
        const AgentState& agent,
        const Policy& policy
    ) noexcept;
};

} // namespace game::navigation
