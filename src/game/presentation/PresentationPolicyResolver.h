#pragma once

#include <optional>

#include "src/game/motion/MotionModel.h"
#include "src/game/presentation/PresentationPolicy.h"
#include "src/game/simulation/AuthorityPolicy.h"
#include "src/game/simulation/EntityRuntimeContract.h"

namespace game::presentation
{

constexpr std::optional<PresentationPolicy> resolvePresentationPolicy(
    const game::simulation::EntityRuntimeContract& contract,
    PresentationRole role
) noexcept
{
    using game::motion::MotionModel;
    using game::simulation::AuthorityPolicy;

    if (!game::simulation::isRuntimeContractValid(contract))
        return std::nullopt;

    if (role == PresentationRole::DiagnosticReference)
    {
        if (contract.authority != AuthorityPolicy::PresentationOnly)
            return std::nullopt;

        return PresentationPolicy::Analytic;
    }

    if (contract.authority == AuthorityPolicy::PresentationOnly)
        return std::nullopt;

    if (contract.motionModel == MotionModel::DynamicPhysics)
    {
        if (role == PresentationRole::LocalControlled)
        {
            if (
                contract.authority !=
                AuthorityPolicy::ServerAuthoritativeWithClientPrediction)
            {
                return std::nullopt;
            }

            return PresentationPolicy::LocalPredicted;
        }

        return PresentationPolicy::SnapshotInterpolated;
    }

    if (game::simulation::isAnalyticMotion(contract.motionModel))
        return PresentationPolicy::Analytic;

    return std::nullopt;
}

constexpr bool presentationPolicyAllowed(
    const game::simulation::EntityRuntimeContract& contract,
    PresentationRole role,
    PresentationPolicy policy
) noexcept
{
    const auto resolved = resolvePresentationPolicy(contract, role);
    return resolved.has_value() && *resolved == policy;
}

} // namespace game::presentation
