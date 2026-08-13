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
    // Tactical AI cadence.
    double activeNpcAiIntervalSeconds = 0.0;
    double prewarmNpcAiIntervalSeconds = 0.10;
    double coarseNpcAiIntervalSeconds = 1.00;
    double maxNpcAiThinkDeltaSeconds = 1.00;

    // Motion-control evaluation (attitude controller + engine command
    // refresh). Stage 4B keeps cheap kinematic propagation on every fixed
    // tick, but non-interacting ships do not need to recompute control forces
    // at 50 Hz. Active remains exactly on the legacy fixed-step path.
    double activeShipMotionControlIntervalSeconds = 0.0;
    double prewarmShipMotionControlIntervalSeconds = 0.04;
    double coarseShipMotionControlIntervalSeconds = 0.20;
    double maxShipMotionControlDeltaSeconds = 0.20;

    // Internal ship service systems (reactor/thermal/cooling/life-support).
    double activeShipSystemsIntervalSeconds = 0.0;
    double prewarmShipSystemsIntervalSeconds = 0.10;
    double coarseShipSystemsIntervalSeconds = 1.00;
    double maxShipSystemsDeltaSeconds = 1.00;

    // Materialized structural maintenance: assembly animation, detached
    // fragments and repair jobs. Dirty hit-volume rebuilds remain immediate.
    double activeShipMaintenanceIntervalSeconds = 0.0;
    double prewarmShipMaintenanceIntervalSeconds = 0.10;
    double coarseShipMaintenanceIntervalSeconds = 1.00;
    double maxShipMaintenanceDeltaSeconds = 1.00;
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
    double executionDeltaSeconds = 0.0;

    // Backward-compatible alias used by the NPC AI consumer/tests. New lanes
    // should read executionDeltaSeconds.
    double thinkDeltaSeconds = 0.0;

    double intervalSeconds = 0.0;
};

inline double materializedIntervalSeconds(
    SimulationMode mode,
    double activeIntervalSeconds,
    double prewarmIntervalSeconds,
    double coarseIntervalSeconds
) noexcept
{
    switch (mode)
    {
        case SimulationMode::Active:
            return std::max(0.0, activeIntervalSeconds);
        case SimulationMode::Prewarm:
            return std::max(0.0, prewarmIntervalSeconds);
        case SimulationMode::Coarse:
            return std::max(0.0, coarseIntervalSeconds);
        case SimulationMode::Scheduled:
        case SimulationMode::OnDemand:
        case SimulationMode::Dormant:
            return std::numeric_limits<double>::infinity();
    }

    return std::numeric_limits<double>::infinity();
}

inline ActivationCadenceDecision advanceActivationCadence(
    ActivationCadenceState& state,
    double intervalSeconds,
    double frameDeltaSeconds,
    double serverTimeSeconds,
    double maxExecutionDeltaSeconds
) noexcept
{
    ActivationCadenceDecision result;

    const double dt = std::max(0.0, frameDeltaSeconds);
    state.timeSinceLastExecutionSeconds += dt;
    result.intervalSeconds = intervalSeconds;

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

    const double maxExecutionDelta =
        std::max(dt, std::max(0.0, maxExecutionDeltaSeconds));

    result.execute = true;
    result.executionDeltaSeconds = std::min(
        std::max(dt, state.timeSinceLastExecutionSeconds),
        maxExecutionDelta
    );
    result.thinkDeltaSeconds = result.executionDeltaSeconds;

    state.timeSinceLastExecutionSeconds = 0.0;
    state.lastExecutionServerTimeSeconds = std::max(0.0, serverTimeSeconds);
    ++state.executionCount;

    return result;
}

inline double npcAiIntervalSeconds(
    SimulationMode mode,
    const ActivationExecutionPolicy& policy
) noexcept
{
    return materializedIntervalSeconds(
        mode,
        policy.activeNpcAiIntervalSeconds,
        policy.prewarmNpcAiIntervalSeconds,
        policy.coarseNpcAiIntervalSeconds
    );
}

inline double shipMotionControlIntervalSeconds(
    SimulationMode mode,
    const ActivationExecutionPolicy& policy
) noexcept
{
    return materializedIntervalSeconds(
        mode,
        policy.activeShipMotionControlIntervalSeconds,
        policy.prewarmShipMotionControlIntervalSeconds,
        policy.coarseShipMotionControlIntervalSeconds
    );
}

inline double shipSystemsIntervalSeconds(
    SimulationMode mode,
    const ActivationExecutionPolicy& policy
) noexcept
{
    return materializedIntervalSeconds(
        mode,
        policy.activeShipSystemsIntervalSeconds,
        policy.prewarmShipSystemsIntervalSeconds,
        policy.coarseShipSystemsIntervalSeconds
    );
}

inline double shipMaintenanceIntervalSeconds(
    SimulationMode mode,
    const ActivationExecutionPolicy& policy
) noexcept
{
    return materializedIntervalSeconds(
        mode,
        policy.activeShipMaintenanceIntervalSeconds,
        policy.prewarmShipMaintenanceIntervalSeconds,
        policy.coarseShipMaintenanceIntervalSeconds
    );
}

inline ActivationCadenceDecision advanceNpcAiCadence(
    ActivationCadenceState& state,
    SimulationMode mode,
    double frameDeltaSeconds,
    double serverTimeSeconds,
    const ActivationExecutionPolicy& policy
) noexcept
{
    return advanceActivationCadence(
        state,
        npcAiIntervalSeconds(mode, policy),
        frameDeltaSeconds,
        serverTimeSeconds,
        policy.maxNpcAiThinkDeltaSeconds
    );
}

inline ActivationCadenceDecision advanceShipMotionControlCadence(
    ActivationCadenceState& state,
    SimulationMode mode,
    double frameDeltaSeconds,
    double serverTimeSeconds,
    const ActivationExecutionPolicy& policy
) noexcept
{
    return advanceActivationCadence(
        state,
        shipMotionControlIntervalSeconds(mode, policy),
        frameDeltaSeconds,
        serverTimeSeconds,
        policy.maxShipMotionControlDeltaSeconds
    );
}

inline ActivationCadenceDecision advanceShipSystemsCadence(
    ActivationCadenceState& state,
    SimulationMode mode,
    double frameDeltaSeconds,
    double serverTimeSeconds,
    const ActivationExecutionPolicy& policy
) noexcept
{
    return advanceActivationCadence(
        state,
        shipSystemsIntervalSeconds(mode, policy),
        frameDeltaSeconds,
        serverTimeSeconds,
        policy.maxShipSystemsDeltaSeconds
    );
}

inline ActivationCadenceDecision advanceShipMaintenanceCadence(
    ActivationCadenceState& state,
    SimulationMode mode,
    double frameDeltaSeconds,
    double serverTimeSeconds,
    const ActivationExecutionPolicy& policy
) noexcept
{
    return advanceActivationCadence(
        state,
        shipMaintenanceIntervalSeconds(mode, policy),
        frameDeltaSeconds,
        serverTimeSeconds,
        policy.maxShipMaintenanceDeltaSeconds
    );
}

} // namespace game::simulation::activation
