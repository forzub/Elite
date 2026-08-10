#pragma once

#include <algorithm>
#include <cstdint>

#include "src/game/simulation/SimulationMode.h"

namespace game::simulation::activation
{

struct ActivationHysteresisPolicy
{
    // Once full tactical simulation was required, keep it alive briefly after
    // the demand disappears. This prevents mode chatter around an interaction
    // boundary and gives projectiles/AI/runtime caches time to settle.
    double activeReleaseDelaySeconds = 2.0;

    // After leaving Active, remain prepared before dropping to coarse world
    // simulation. Promotion is always immediate; only demotion is delayed.
    double prewarmReleaseDelaySeconds = 1.0;
};

enum class ActivationPlanTransition : std::uint8_t
{
    None = 0,
    PromoteToPrewarm,
    PromoteToActive,
    DemoteToPrewarm,
    DemoteToCoarse
};

struct ActivationPlanState
{
    game::simulation::SimulationMode plannedMode =
        game::simulation::SimulationMode::Active;

    double modeEnteredServerTimeSeconds = 0.0;
    double releaseNotBeforeServerTimeSeconds = 0.0;

    // Durable transition telemetry. The planner can run more frequently than
    // diagnostics are sampled, so an ephemeral transition flag alone can miss
    // a real mode change between two CSV rows.
    std::uint64_t transitionSerial = 0;
    ActivationPlanTransition lastTransition = ActivationPlanTransition::None;
    double lastTransitionServerTimeSeconds = 0.0;
};

struct ActivationPlanUpdate
{
    game::simulation::SimulationMode previousMode =
        game::simulation::SimulationMode::Active;
    game::simulation::SimulationMode requestedMode =
        game::simulation::SimulationMode::Coarse;
    game::simulation::SimulationMode plannedMode =
        game::simulation::SimulationMode::Active;
    ActivationPlanTransition transition = ActivationPlanTransition::None;

    std::uint64_t transitionSerial = 0;
    ActivationPlanTransition lastTransition = ActivationPlanTransition::None;
    double lastTransitionServerTimeSeconds = 0.0;
};

inline const char* activationPlanTransitionName(
    ActivationPlanTransition transition
) noexcept
{
    switch (transition)
    {
        case ActivationPlanTransition::None:
            return "none";
        case ActivationPlanTransition::PromoteToPrewarm:
            return "promote-prewarm";
        case ActivationPlanTransition::PromoteToActive:
            return "promote-active";
        case ActivationPlanTransition::DemoteToPrewarm:
            return "demote-prewarm";
        case ActivationPlanTransition::DemoteToCoarse:
            return "demote-coarse";
    }

    return "unknown";
}

inline void recordActivationPlanTransition(
    ActivationPlanState& state,
    ActivationPlanUpdate& result,
    ActivationPlanTransition transition,
    double now
) noexcept
{
    result.transition = transition;
    state.lastTransition = transition;
    state.lastTransitionServerTimeSeconds = now;
    ++state.transitionSerial;
}

inline ActivationPlanUpdate updateActivationPlan(
    ActivationPlanState& state,
    game::simulation::SimulationMode requestedMode,
    double serverTimeSeconds,
    const ActivationHysteresisPolicy& policy
) noexcept
{
    using game::simulation::SimulationMode;

    ActivationPlanUpdate result;
    result.previousMode = state.plannedMode;
    result.requestedMode = requestedMode;

    const double now = std::max(0.0, serverTimeSeconds);
    const double activeDelay =
        std::max(0.0, policy.activeReleaseDelaySeconds);
    const double prewarmDelay =
        std::max(0.0, policy.prewarmReleaseDelaySeconds);

    const auto finish = [&]() noexcept
    {
        result.plannedMode = state.plannedMode;
        result.transitionSerial = state.transitionSerial;
        result.lastTransition = state.lastTransition;
        result.lastTransitionServerTimeSeconds =
            state.lastTransitionServerTimeSeconds;
        return result;
    };

    // Promotions are immediate. Active demand also refreshes the release
    // deadline on every evaluation, so Active persists for activeDelay after
    // the final high-demand sample.
    if (requestedMode == SimulationMode::Active)
    {
        state.releaseNotBeforeServerTimeSeconds = now + activeDelay;

        if (state.plannedMode != SimulationMode::Active)
        {
            state.plannedMode = SimulationMode::Active;
            state.modeEnteredServerTimeSeconds = now;
            recordActivationPlanTransition(
                state,
                result,
                ActivationPlanTransition::PromoteToActive,
                now
            );
        }

        return finish();
    }

    if (requestedMode == SimulationMode::Prewarm)
    {
        if (state.plannedMode == SimulationMode::Coarse ||
            state.plannedMode == SimulationMode::Scheduled ||
            state.plannedMode == SimulationMode::OnDemand ||
            state.plannedMode == SimulationMode::Dormant)
        {
            state.plannedMode = SimulationMode::Prewarm;
            state.modeEnteredServerTimeSeconds = now;
            state.releaseNotBeforeServerTimeSeconds = now + prewarmDelay;
            recordActivationPlanTransition(
                state,
                result,
                ActivationPlanTransition::PromoteToPrewarm,
                now
            );
        }
        else if (state.plannedMode == SimulationMode::Active &&
                 now >= state.releaseNotBeforeServerTimeSeconds)
        {
            state.plannedMode = SimulationMode::Prewarm;
            state.modeEnteredServerTimeSeconds = now;
            state.releaseNotBeforeServerTimeSeconds = now + prewarmDelay;
            recordActivationPlanTransition(
                state,
                result,
                ActivationPlanTransition::DemoteToPrewarm,
                now
            );
        }
        else if (state.plannedMode == SimulationMode::Prewarm)
        {
            state.releaseNotBeforeServerTimeSeconds = now + prewarmDelay;
        }

        return finish();
    }

    // Stage 3D's lowest runtime plan is Coarse. Scheduled/Dormant materialized
    // transitions belong to the persistent-universe stage and must not be
    // invented by this tactical activation planner.
    if (state.plannedMode == SimulationMode::Active)
    {
        if (now >= state.releaseNotBeforeServerTimeSeconds)
        {
            state.plannedMode = SimulationMode::Prewarm;
            state.modeEnteredServerTimeSeconds = now;
            state.releaseNotBeforeServerTimeSeconds = now + prewarmDelay;
            recordActivationPlanTransition(
                state,
                result,
                ActivationPlanTransition::DemoteToPrewarm,
                now
            );
        }
    }
    else if (state.plannedMode == SimulationMode::Prewarm)
    {
        if (now >= state.releaseNotBeforeServerTimeSeconds)
        {
            state.plannedMode = SimulationMode::Coarse;
            state.modeEnteredServerTimeSeconds = now;
            state.releaseNotBeforeServerTimeSeconds = now;
            recordActivationPlanTransition(
                state,
                result,
                ActivationPlanTransition::DemoteToCoarse,
                now
            );
        }
    }
    else
    {
        state.plannedMode = SimulationMode::Coarse;
    }

    return finish();
}

} // namespace game::simulation::activation
