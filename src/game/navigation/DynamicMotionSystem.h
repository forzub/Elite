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

    static void updateLocalFrameMotion(
        DynamicMotionState& motion,
        world::coordinates::WorldPosition& worldPosition,
        const KinematicFrame& frame,
        const ShipParams& params,
        double dt
    );
};

} // namespace game::navigation
