#include "NpcAiSystem.h"

#include <algorithm>

NpcNavigationGoal NpcAiSystem::computeGoal(
    const Ship& ship,
    float dt
) const noexcept
{
    (void)dt;

    NpcNavigationGoal goal;

    const auto& params = ship.core().desc().physics;

    // Baseline live-ownership goal only. Higher-level mission/traffic/repair
    // systems may replace this later. The important stage-11B invariant is that
    // AI publishes a goal, not control-surface input.
    goal.revision = 1;
    goal.mode = NpcNavigationGoalMode::MaintainForwardCruise;
    goal.desiredForwardSpeedMps = std::clamp(
        static_cast<double>(params.maxCombatSpeed) * 0.10,
        2.0,
        20.0
    );
    goal.velocityResponsePerSecond = 0.75;
    goal.angularDampingPerSecond = 2.0;
    return goal;
}

world::navigation::PilotSkillExecutor::PilotSkillProfile
NpcAiSystem::pilotSkillProfile(
    const Ship& ship
) const noexcept
{
    (void)ship;

    world::navigation::PilotSkillExecutor::PilotSkillProfile profile;

    // Deterministic competent baseline. NPC archetypes may tune these values
    // later without changing vehicle capability or navigation geometry.
    profile.execution.reactionDelaySeconds = 0.12;
    profile.execution.perceptionDecisionRateHz = 12.0;
    profile.execution.commandLatencySeconds = 0.06;
    profile.execution.responseFrequencyHz = 2.0;
    profile.execution.dampingRatio = 0.85;
    profile.execution.commandGain = 1.0;
    profile.execution.maxLinearCommandSlewMetersPerSec3 = 80.0;
    profile.execution.maxAngularCommandSlewRadPerSec3 = 8.0;
    profile.execution.deterministicLinearNoiseAmplitudeMetersPerSec2 = 0.02;
    profile.execution.deterministicAngularNoiseAmplitudeRadPerSec2 = 0.002;
    profile.execution.deterministicSeed =
        0x9E3779B97F4A7C15ull ^
        static_cast<std::uint64_t>(ship.id().value);
    profile.execution.emergencyResponseThreshold01 = 0.75;
    profile.execution.emergencyReactionDelayScale = 0.35;

    profile.policy.anticipationSeconds = 2.0;
    profile.policy.riskPreference01 = 0.45;
    profile.policy.comfortPreference01 = 0.65;
    return profile;
}
