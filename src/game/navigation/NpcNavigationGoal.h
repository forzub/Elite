#pragma once

#include <cstdint>
#include <glm/glm.hpp>

enum class NpcNavigationGoalMode : std::uint8_t
{
    Hold = 0,
    MaintainForwardCruise,
    TrackFormationSlot
};

struct NpcNavigationGoal
{
    std::uint64_t revision = 1;
    NpcNavigationGoalMode mode = NpcNavigationGoalMode::Hold;

    double desiredForwardSpeedMps = 0.0;

    // Formation/traffic goals remain vehicle-neutral: they describe a moving
    // system-space slot, not thruster commands. The NPC controller converts
    // position error into a bounded velocity correction before capability is
    // applied by the ordinary pilot/ship stack.
    glm::dvec3 desiredPositionSystemMeters {0.0};
    glm::dvec3 desiredVelocitySystemMps {0.0};
    double positionResponsePerSecond = 0.35;
    double maximumPositionCorrectionSpeedMps = 60.0;

    double velocityResponsePerSecond = 0.75;
    double angularDampingPerSecond = 2.0;

    bool emergency = false;
    double hazardUrgency01 = 0.0;
};
