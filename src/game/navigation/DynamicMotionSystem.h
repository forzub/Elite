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
    // Generic/legacy direct-acceleration seam. This remains available to
    // non-program callers and diagnostics, but it is NOT the canonical
    // Assisted autopilot path.
    //
    // Newtonian direct demand maps longitudinal main thrust to installed main
    // authority and residual vector demand to bounded physical RCS. A proper
    // accepted Newtonian transit should instead author hull rotation + main
    // burn explicitly.
    //
    // Assisted accepted programs MUST use applyNavigationAssistedFlightModel()
    // below so manual and automatic flight share the same nose-coupled game
    // law. Do not route ordinary Assisted curvature through this generic seam.
    static void applySystemAccelerationDemand(
        DynamicMotionState& motion,
        const ShipParams& params,
        const glm::dvec3& linearAccelerationDemandSystemMps2,
        const glm::vec3& shipForward
    );

    // Executes Assisted autopilot translation through the exact same local
    // flight law as manual Assisted control. The nominal command is a target
    // velocity vector with pilot-executed acceleration compensation for the
    // manual speed-controller response lag. The ship derives its forward
    // setpoint and bounded lateral correction from its measured motion.
    static void applyNavigationAssistedFlightModel(
        DynamicMotionState& motion,
        const KinematicFrame& frame,
        const ShipParams& params,
        float dt,
        const glm::dvec3& targetVelocitySystemMps,
        const glm::dvec3& executedAccelerationDemandSystemMps2,
        const glm::vec3& shipForward,
        const glm::vec3& shipRight,
        const glm::vec3& shipUp
    );

    // Precision placement/STOP executor. It never wakes a main engine.
    // The manoeuvre/RCS system removes the measured velocity error as quickly
    // as its physical authority and this control step allow.
    static void applyNavigationPrecisionVelocityTrim(
        DynamicMotionState& motion,
        const KinematicFrame& frame,
        const ShipParams& params,
        float dt,
        const glm::dvec3& targetVelocitySystemMps
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
