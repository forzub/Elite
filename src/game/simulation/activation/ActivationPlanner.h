#pragma once

#include <vector>

#include "src/game/simulation/activation/ActivationClaim.h"
#include "src/game/simulation/activation/ActivationShadow.h"
#include "src/game/simulation/activation/ActivationStateMachine.h"

namespace game::simulation::activation
{

struct ActivationPlannerDecision
{
    ActivationShadowDecision physicalDecision {};
    ActivationClaimEvaluation claimDecision {};
    ActivationPlanUpdate planUpdate {};
};

inline ActivationPlannerDecision evaluateActivationPlan(
    ActivationPlanState& state,
    const ActivationShadowDecision& physicalDecision,
    int systemId,
    const std::vector<ActivationClaim>& claims,
    double serverTimeSeconds,
    const ActivationHysteresisPolicy& hysteresis
) noexcept
{
    ActivationPlannerDecision result;
    result.physicalDecision = physicalDecision;

    result.claimDecision = evaluateActivationClaims(
        physicalDecision.subjectId,
        systemId,
        physicalDecision.desiredMode,
        claims,
        serverTimeSeconds
    );

    result.planUpdate = updateActivationPlan(
        state,
        result.claimDecision.requestedMode,
        serverTimeSeconds,
        hysteresis
    );

    return result;
}

} // namespace game::simulation::activation
