#pragma once

#include <cstdint>

namespace game::motion
{

enum class MotionModel : std::uint8_t
{
    Static = 0,
    Orbital,
    HubAttached,
    Kinematic,
    ScheduledTrajectory,
    FleetFormation,
    DynamicPhysics
};

} // namespace game::motion
