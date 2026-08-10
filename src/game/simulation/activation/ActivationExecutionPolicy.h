#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "src/game/simulation/SimulationMode.h"

namespace game::simulation::activation
{

struct ActivationExecutionPolicy
{
    // Active tactical NPCs keep the existing per-fixed-tick AI behaviour.
    double activeNpcAiIntervalSeconds = 0.0;

    // Prewarm keeps AI responsive without paying the full 50 Hz think cost.
    double prewarmNpcAiIntervalSeconds = 0.10;

    // Coarse materialized ships remain physically integrated for now, but
    // tactical decision making is reduced to a cheap maintenance cadence.
    double coarseNpcAiIntervalSeconds = 1.00;

    // Never hand an AI system an arbitrarily huge dt after a long dormant gap.
    double maxNpcAiThinkDeltaSeconds = 1.00;
};

struct ActivationCadenceState
{
    double timeSinceLastExecutionSeconds = 0.0;
    double lastExecutionServerTimeSeconds = 0.0;
    std::uint64_t executionCount = 0;
    std::uint64_t skippedFrameCount = 0;
};

struct ActivationCadenceDecision
{
    bool execute = false;
    double thinkDeltaSeconds = 0.0;
    double intervalSeconds = 0.0;
};

inline double npcAiIntervalSeconds(
    SimulationMode mode,
    const ActivationExecutionPolicy& policy
) noexcept
{
    switch (mode)
    {
        case SimulationMode::Active:
            return std::max(0.0, policy.activeNpcAiIntervalSeconds);
        case SimulationMode::Prewarm:
            return std::max(0.0, policy.prewarmNpcAiIntervalSeconds);
        case SimulationMode::Coarse:
            return std::max(0.0, policy.coarseNpcAiIntervalSeconds);
        case SimulationMode::Scheduled:
        case SimulationMode::OnDemand:
        case SimulationMode::Dormant:
            return std::numeric_limits<double>::infinity();
    }

    return std::numeric_limits<double>::infinity();
}

inline ActivationCadenceDecision advanceNpcAiCadence(
    ActivationCadenceState& state,
    SimulationMode mode,
    double frameDeltaSeconds,
    double serverTimeSeconds,
    const ActivationExecutionPolicy& policy
) noexcept
{
    ActivationCadenceDecision result;

    const double dt = std::max(0.0, frameDeltaSeconds);
    state.timeSinceLastExecutionSeconds += dt;
    result.intervalSeconds = npcAiIntervalSeconds(mode, policy);

    if (!std::isfinite(result.intervalSeconds))
    {
        ++state.skippedFrameCount;
        return result;
    }

    const bool due =
        result.intervalSeconds <= 0.0 ||
        state.timeSinceLastExecutionSeconds + 1e-9 >= result.intervalSeconds;

    if (!due)
    {
        ++state.skippedFrameCount;
        return result;
    }

    const double maxThinkDelta =
        std::max(dt, std::max(0.0, policy.maxNpcAiThinkDeltaSeconds));

    result.execute = true;
    result.thinkDeltaSeconds = std::min(
        std::max(dt, state.timeSinceLastExecutionSeconds),
        maxThinkDelta
    );

    state.timeSinceLastExecutionSeconds = 0.0;
    state.lastExecutionServerTimeSeconds = std::max(0.0, serverTimeSeconds);
    ++state.executionCount;

    return result;
}

} // namespace game::simulation::activation
