#pragma once

#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/simulation/NpcAiSystem.h"
#include "src/game/ship/Ship.h"

namespace game::navigation
{

// Converts a goal-only NPC policy product into one bounded Navigation v2
// acceleration intent. It owns no world search or collision response; those
// higher navigation layers may replace/modify the nominal intent before this
// controller in later end-to-end composition.
class NpcNavigationIntentController final
{
public:
    [[nodiscard]] static NavigationRuntimeControlBridge::Intent buildIntent(
        const Ship& ship,
        const NpcNavigationGoal& goal
    ) noexcept;
};

} // namespace game::navigation
