    #pragma once

#include "src/game/navigation/DynamicMotionState.h"
#include "src/game/navigation/HubNavigationFrame.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::navigation
{

class DynamicMotionSystem
{
public:
    static void applyHubTacticalInput(
        DynamicMotionState& motion,
        const HubNavigationFrame& frame,
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

    static void updateHubTactical(
        DynamicMotionState& motion,
        world::coordinates::WorldPosition& worldPosition,
        const HubNavigationFrame& frame,
        double dt
    );
};

} // namespace game::navigation