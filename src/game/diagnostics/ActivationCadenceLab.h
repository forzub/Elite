#pragma once

#include <cmath>
#include <cstdint>

#include "src/game/simulation/SimulationMode.h"

namespace game::diagnostics
{

// Runtime-only diagnostic actor used to prove that Stage 3E NPC AI cadence
// actually executes in the real GameSimulation loop. Unlike Hub Motion Lab
// probes, this ship is intentionally eligible for production NpcAiSystem.
inline constexpr bool ActivationCadenceLabEnabled = true;
inline constexpr std::uint64_t ActivationCadenceLabInstanceId = 9010;
inline constexpr const char* ActivationCadenceLabLabel = "ACTIVATION AI PROBE";
inline constexpr const char* ActivationCadenceLabHubId = "earth_orbital_hub";
inline constexpr double ActivationCadenceLabLocalOffsetMeters = 30000.0;

inline constexpr double ActivationCadenceLabCycleSeconds = 24.0;
inline constexpr double ActivationCadenceLabCoarseEndSeconds = 6.0;
inline constexpr double ActivationCadenceLabPrewarmEndSeconds = 12.0;
inline constexpr double ActivationCadenceLabActiveEndSeconds = 18.0;

struct ActivationCadenceLabDemand
{
    const char* phase = "coarse";
    bool hasClaim = false;
    game::simulation::SimulationMode minimumMode =
        game::simulation::SimulationMode::Coarse;
};

inline ActivationCadenceLabDemand activationCadenceLabDemand(
    double serverTimeSeconds
) noexcept
{
    const double safeTime = std::max(0.0, serverTimeSeconds);
    const double phaseTime = std::fmod(
        safeTime,
        ActivationCadenceLabCycleSeconds
    );

    if (phaseTime < ActivationCadenceLabCoarseEndSeconds)
    {
        return {
            "coarse",
            false,
            game::simulation::SimulationMode::Coarse
        };
    }

    if (phaseTime < ActivationCadenceLabPrewarmEndSeconds)
    {
        return {
            "prewarm-claim",
            true,
            game::simulation::SimulationMode::Prewarm
        };
    }

    if (phaseTime < ActivationCadenceLabActiveEndSeconds)
    {
        return {
            "active-claim",
            true,
            game::simulation::SimulationMode::Active
        };
    }

    return {
        "release",
        false,
        game::simulation::SimulationMode::Coarse
    };
}

} // namespace game::diagnostics
