#pragma once

#include <cstdint>

#include "game/ship/Ship.h"
#include "src/world/navigation/control/PilotSkillExecutor.h"

enum class NpcNavigationGoalMode : std::uint8_t
{
    Hold = 0,
    MaintainForwardCruise
};

struct NpcNavigationGoal
{
    std::uint64_t revision = 1;
    NpcNavigationGoalMode mode = NpcNavigationGoalMode::Hold;

    double desiredForwardSpeedMps = 0.0;
    double velocityResponsePerSecond = 0.75;
    double angularDampingPerSecond = 2.0;

    bool emergency = false;
    double hazardUrgency01 = 0.0;
};

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
