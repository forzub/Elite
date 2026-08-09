#pragma once

#include <cstdint>

namespace game::simulation
{

enum class SimulationMode : std::uint8_t
{
    Dormant = 0,
    OnDemand,
    Scheduled,
    Coarse,
    Prewarm,
    Active
};

} // namespace game::simulation
