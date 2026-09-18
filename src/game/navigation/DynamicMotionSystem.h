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

    // Navigation/autopilot direct world-acceleration demand. This maps the
    // requested vector onto the real forward main-engine authority plus the
    // remaining six-direction manoeuvre-thruster authority. Final controlled
    // speed and manoeuvre-gas limits are still enforced by
    // updateLocalFrameMotion().
    static void applyWorldAccelerationDemand(
        DynamicMotionState& motion,
        const ShipParams& params,
        const glm::dvec3& linearAccelerationDemandMapMps2,
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
