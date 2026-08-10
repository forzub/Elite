#pragma once

#include <cstdint>
#include <vector>

#include "src/game/simulation/SimulationMode.h"
#include "src/scene/EntityID.h"

namespace game::simulation::activation
{

enum class ActivationClaimKind : std::uint8_t
{
    None = 0,
    Combat,
    ProjectileThreat,
    Docking,
    ScriptedCritical
};

inline const char* activationClaimKindName(ActivationClaimKind kind) noexcept
{
    switch (kind)
    {
        case ActivationClaimKind::None:
            return "none";
        case ActivationClaimKind::Combat:
            return "combat";
        case ActivationClaimKind::ProjectileThreat:
            return "projectile-threat";
        case ActivationClaimKind::Docking:
            return "docking";
        case ActivationClaimKind::ScriptedCritical:
            return "scripted-critical";
    }

    return "unknown";
}

struct ActivationClaim
{
    EntityId subjectId {0};
    EntityId sourceId {0};
    int systemId = -1;

    // Claims may only raise simulation demand. Coarse means no additional
    // demand; Prewarm prepares tactical runtime; Active requires full tactical
    // simulation. Sensor visibility is deliberately not an activation claim.
    game::simulation::SimulationMode minimumMode =
        game::simulation::SimulationMode::Coarse;

    ActivationClaimKind kind = ActivationClaimKind::None;

    // <= 0 means the claim is valid until explicitly removed by its owner.
    double expiresAtServerTimeSeconds = 0.0;
};

inline bool activationClaimCanRaise(
    const ActivationClaim& claim
) noexcept
{
    using game::simulation::SimulationMode;
    return
        claim.minimumMode == SimulationMode::Prewarm ||
        claim.minimumMode == SimulationMode::Active;
}

inline bool activationClaimIsLive(
    const ActivationClaim& claim,
    double serverTimeSeconds
) noexcept
{
    return
        claim.kind != ActivationClaimKind::None &&
        activationClaimCanRaise(claim) &&
        (claim.expiresAtServerTimeSeconds <= 0.0 ||
         serverTimeSeconds <= claim.expiresAtServerTimeSeconds);
}

inline int activationModePriority(
    game::simulation::SimulationMode mode
) noexcept
{
    using game::simulation::SimulationMode;

    switch (mode)
    {
        case SimulationMode::Active:
            return 5;
        case SimulationMode::Prewarm:
            return 4;
        case SimulationMode::Coarse:
            return 3;
        case SimulationMode::Scheduled:
            return 2;
        case SimulationMode::OnDemand:
            return 1;
        case SimulationMode::Dormant:
            return 0;
    }

    return 0;
}

inline game::simulation::SimulationMode raiseActivationDemand(
    game::simulation::SimulationMode base,
    const ActivationClaim& claim,
    double serverTimeSeconds
) noexcept
{
    if (!activationClaimIsLive(claim, serverTimeSeconds))
        return base;

    return activationModePriority(claim.minimumMode) >
            activationModePriority(base)
        ? claim.minimumMode
        : base;
}

struct ActivationClaimEvaluation
{
    game::simulation::SimulationMode requestedMode =
        game::simulation::SimulationMode::Coarse;
    bool hasClaim = false;
    ActivationClaimKind kind = ActivationClaimKind::None;
    EntityId sourceId {0};
};

inline ActivationClaimEvaluation evaluateActivationClaims(
    EntityId subjectId,
    int systemId,
    game::simulation::SimulationMode baseMode,
    const std::vector<ActivationClaim>& claims,
    double serverTimeSeconds
) noexcept
{
    ActivationClaimEvaluation result;
    result.requestedMode = baseMode;

    int strongestClaimPriority = -1;

    for (const auto& claim : claims)
    {
        if (!(claim.subjectId == subjectId) ||
            claim.systemId != systemId ||
            !activationClaimIsLive(claim, serverTimeSeconds))
        {
            continue;
        }

        const int claimPriority =
            activationModePriority(claim.minimumMode);

        if (claimPriority > strongestClaimPriority)
        {
            strongestClaimPriority = claimPriority;
            result.hasClaim = true;
            result.kind = claim.kind;
            result.sourceId = claim.sourceId;
        }

        if (claimPriority > activationModePriority(result.requestedMode))
            result.requestedMode = claim.minimumMode;
    }

    return result;
}

} // namespace game::simulation::activation
