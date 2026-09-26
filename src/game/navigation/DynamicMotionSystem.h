#pragma once

#include "src/game/navigation/DynamicMotionState.h"
#include "src/game/navigation/KinematicFrame.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::navigation
{

class DynamicMotionSystem
{
public:
    static void applyLocalFrameInput(
        DynamicMotionState& motion,
        const KinematicFrame& frame,
        const ShipParams& params,
        float dt,
        float targetSpeedRate,
        bool cruiseActive,
        float forwardInput,
        float liftInput,
        float strafeInput,
        const glm::vec3& shipForward,
        const glm::vec3& shipRight,
        const glm::vec3& shipUp
    );

    // Navigation/autopilot direct system-frame acceleration demand.
    //
    // Newtonian maps longitudinal main thrust to the aft/forward source only;
    // reverse demand must use bounded RCS unless the accepted maneuver has
    // physically flipped the hull.
    //
    // Assisted/aircraft-like maps longitudinal demand to symmetric aft/fore
    // controlled thrust. Lateral/vertical remainder always uses the bounded
    // six-direction manoeuvre/RCS authority.
    //
    // Final controlled-speed and manoeuvre-gas limits are still enforced by
    // updateLocalFrameMotion().
    static void applySystemAccelerationDemand(
        DynamicMotionState& motion,
        const ShipParams& params,
        const glm::dvec3& linearAccelerationDemandSystemMps2,
        const glm::vec3& shipForward
    );

    // Executes one Planner-owned actuator sample plus bounded Follower
    // correction. The nominal rear/fore main schedule is preserved; only
    // feedback may consume remaining main/RCS authority.
    static void applyNavigationActuatorProgram(
        DynamicMotionState& motion,
        const ShipParams& params,
        double rearMainThrottle01,
        double foreMainThrottle01,
        const glm::dvec3& manoeuvreAccelerationSystemMps2,
        const glm::dvec3& feedbackAccelerationSystemMps2,
        const glm::vec3& shipForward
    );

    static void updateLocalFrameMotion(
        DynamicMotionState& motion,
        world::coordinates::WorldPosition& worldPosition,
        const KinematicFrame& frame,
        const ShipParams& params,
        double dt
    );
};

} // namespace game::navigation
