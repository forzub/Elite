#pragma once

#include <cstdint>

namespace game::server
{

struct ServerTimeContext
{
    std::uint32_t serverTick = 0;

    // Fixed authoritative gameplay step used by ship physics, controls,
    // repairs, AI and other tick-based simulation systems.
    double gameplayDeltaSeconds = 0.0;

    // Absolute universe timestamp shared by orbital systems, reference
    // frames and snapshots produced during this server tick.
    double universeTimeSeconds = 0.0;

    // Change in universe time since the previous authoritative tick.
    // This can differ from gameplayDeltaSeconds in debug time simulation.
    double universeDeltaSeconds = 0.0;

    bool universeTimeSimulation = false;
    double universeTimeScale = 1.0;
};

} // namespace game::server
