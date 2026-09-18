#pragma once

#include "game/ship/Ship.h"
#include "src/game/navigation/NpcNavigationGoal.h"
#include "src/world/navigation/control/PilotSkillExecutor.h"

class NpcAiSystem
{
public:
    // NPC AI owns goals/policy only. It must not emit ShipControlState or
    // direct acceleration commands; Navigation v2 owns maneuver intent.
    [[nodiscard]] NpcNavigationGoal computeGoal(
        const Ship& ship,
        float dt
    ) const noexcept;

    [[nodiscard]] world::navigation::PilotSkillExecutor::PilotSkillProfile
    pilotSkillProfile(const Ship& ship) const noexcept;
};
