#pragma once

#include <cstdint>

namespace game::simulation
{

enum class TimelineDomain : std::uint8_t
{
    None = 0,
    Universe,
    ServerSimulation
};

} // namespace game::simulation
