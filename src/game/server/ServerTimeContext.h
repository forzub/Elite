#pragma once

#include <cstdint>

namespace game::server
{

struct ServerTimeContext
{
    std::uint64_t serverTick = 0;

    // Monotonic authoritative server-timeline step. This continues while
    // gameplay is frozen for accelerated-universe diagnostics.
    double serverDeltaSeconds = 0.0;

    // Gameplay step used by ship physics, controls, repairs and AI. This may
    // be zero while serverDeltaSeconds continues to advance.
    double gameplayDeltaSeconds = 0.0;

    // Absolute universe timestamp shared by orbital systems, reference
    // frames and snapshots produced during this server tick.
    double universeTimeSeconds = 0.0;

    // Change in universe time since the previous authoritative tick.
    // This can differ from both serverDeltaSeconds and gameplayDeltaSeconds.
    double universeDeltaSeconds = 0.0;

    bool universeTimeSimulation = false;
    double universeTimeScale = 1.0;
};

} // namespace game::server
