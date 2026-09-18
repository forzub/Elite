#pragma once

#include <glm/glm.hpp>

#include "src/game/navigation/NavigationControlIntent.h"
#include "src/game/navigation/NpcNavigationGoal.h"

namespace game::navigation
{

struct NpcNavigationKinematicState
{
    glm::dvec3 relativeSystemVelocityMps {0.0};

    glm::dvec3 forwardSystem {0.0, 0.0, -1.0};
    glm::dvec3 rightSystem {1.0, 0.0, 0.0};
    glm::dvec3 upSystem {0.0, 1.0, 0.0};

    double pitchRateRadPerSec = 0.0;
    double yawRateRadPerSec = 0.0;
    double rollRateRadPerSec = 0.0;
};

// Converts a goal-only NPC policy product plus a compact authoritative
// kinematic snapshot into one nominal system-space acceleration intent for the runtime control bridge.
//
// The controller deliberately does not depend on the full live ship runtime, world search,
// collision response, rendering or vehicle capability. GameSimulation adapts
// the live Ship into NpcNavigationKinematicState at the ownership boundary.
class NpcNavigationIntentController final
{
public:
    [[nodiscard]] static NavigationSystemControlIntent buildIntent(
        const NpcNavigationKinematicState& state,
        const NpcNavigationGoal& goal
    ) noexcept;
};

} // namespace game::navigation
