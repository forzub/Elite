#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace game::navigation
{

// Planner/follower-owned command. Every vector is expressed in the single
// Navigation-v2 local frame. This type is intentionally incompatible with the
// system-space control intent consumed by PilotSkillExecutor.
struct NavigationLocalControlIntent
{
    std::uint64_t revision = 0;
    std::uint64_t targetRevision = 0;

    glm::dvec3 idealLinearAccelerationLocalMps2 {0.0};
    glm::dvec3 idealAngularAccelerationLocalRadPerSec2 {0.0};

    bool emergency = false;
    double hazardUrgency01 = 0.0;
};

// Control-side command after the explicit NavigationFrameBoundary. Vectors are
// expressed in authoritative star-system/world axes used by ShipControlState
// and DynamicMotionSystem.
struct NavigationSystemControlIntent
{
    std::uint64_t revision = 0;
    std::uint64_t targetRevision = 0;

    glm::dvec3 idealLinearAccelerationSystemMps2 {0.0};
    glm::dvec3 idealAngularAccelerationSystemRadPerSec2 {0.0};

    bool emergency = false;
    double hazardUrgency01 = 0.0;
};

} // namespace game::navigation
