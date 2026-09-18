#pragma once

#include <cstdint>

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
