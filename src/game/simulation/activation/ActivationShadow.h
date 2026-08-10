#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "src/game/simulation/SimulationMode.h"
#include "src/game/simulation/activation/InteractionHorizon.h"
#include "src/scene/EntityID.h"

namespace game::simulation::activation
{

enum class ActivationAnchorKind : std::uint8_t
{
    Ship = 0,
    StaticObject
};

enum class ActivationReason : std::uint8_t
{
    None = 0,
    PlayerPinnedActive,
    CurrentInteraction,
    PredictedInteraction,
    NoInteractionWithinHorizon,
    NoComparableAnchors
};

struct ActivationAnchor
{
    EntityId id {0};
    ActivationAnchorKind kind = ActivationAnchorKind::Ship;
    int systemId = -1;
    KinematicPoint kinematics {};
};

struct ActivationShadowDecision
{
    EntityId subjectId {0};
    SimulationMode currentMode = SimulationMode::Active;
    SimulationMode desiredMode = SimulationMode::Coarse;
    ActivationReason reason = ActivationReason::NoComparableAnchors;

    bool hasAnchor = false;
    EntityId anchorId {0};
    ActivationAnchorKind anchorKind = ActivationAnchorKind::Ship;
    InteractionPrediction prediction {};

    // Stage 3D diagnostics: how much of the system-level anchor set survived
    // the conservative spatial broad-phase before exact CPA evaluation.
    std::size_t candidateAnchorCount = 0;
    std::size_t comparableAnchorCount = 0;
    bool broadphaseFallback = false;
    double broadphaseQueryRadiusMeters = 0.0;
    std::size_t broadphaseVisitedCellCount = 0;
    double broadphaseSubjectResidualSpeedMetersPerSecond = 0.0;
    double broadphaseMaxAnchorResidualSpeedMetersPerSecond = 0.0;
};

inline const char* activationAnchorKindName(ActivationAnchorKind kind) noexcept
{
    switch (kind)
    {
        case ActivationAnchorKind::Ship:
            return "ship";
        case ActivationAnchorKind::StaticObject:
            return "static-object";
    }

    return "unknown";
}

inline const char* simulationModeName(SimulationMode mode) noexcept
{
    switch (mode)
    {
        case SimulationMode::Dormant:
            return "Dormant";
        case SimulationMode::OnDemand:
            return "OnDemand";
        case SimulationMode::Scheduled:
            return "Scheduled";
        case SimulationMode::Coarse:
            return "Coarse";
        case SimulationMode::Prewarm:
            return "Prewarm";
        case SimulationMode::Active:
            return "Active";
    }

    return "Unknown";
}

inline const char* activationReasonName(ActivationReason reason) noexcept
{
    switch (reason)
    {
        case ActivationReason::None:
            return "none";
        case ActivationReason::PlayerPinnedActive:
            return "player-pinned";
        case ActivationReason::CurrentInteraction:
            return "current-interaction";
        case ActivationReason::PredictedInteraction:
            return "predicted-interaction";
        case ActivationReason::NoInteractionWithinHorizon:
            return "no-interaction";
        case ActivationReason::NoComparableAnchors:
            return "no-comparable-anchors";
    }

    return "unknown";
}

inline bool isSameEntityAnchor(
    EntityId subjectId,
    const ActivationAnchor& anchor
) noexcept
{
    return
        anchor.kind == ActivationAnchorKind::Ship &&
        anchor.id == subjectId;
}

inline bool isBetterCurrentInteraction(
    const InteractionPrediction& candidate,
    const InteractionPrediction& best
) noexcept
{
    return
        candidate.currentSurfaceDistanceMeters <
        best.currentSurfaceDistanceMeters;
}

inline bool isBetterPredictedInteraction(
    const InteractionPrediction& candidate,
    const InteractionPrediction& best
) noexcept
{
    if (candidate.timeToClosestSeconds != best.timeToClosestSeconds)
    {
        return
            candidate.timeToClosestSeconds <
            best.timeToClosestSeconds;
    }

    return
        candidate.closestSurfaceDistanceMeters <
        best.closestSurfaceDistanceMeters;
}

inline ActivationShadowDecision evaluateActivationShadowCandidates(
    EntityId subjectId,
    int subjectSystemId,
    const KinematicPoint& subject,
    SimulationMode currentMode,
    bool pinnedActive,
    const std::vector<ActivationAnchor>& anchors,
    const std::vector<std::size_t>& candidateIndices,
    const InteractionHorizonPolicy& policy,
    bool broadphaseFallback = false
) noexcept
{
    ActivationShadowDecision result;
    result.subjectId = subjectId;
    result.currentMode = currentMode;
    result.candidateAnchorCount = candidateIndices.size();
    result.broadphaseFallback = broadphaseFallback;

    if (pinnedActive)
    {
        result.desiredMode = SimulationMode::Active;
        result.reason = ActivationReason::PlayerPinnedActive;
        return result;
    }

    bool foundComparableAnchor = false;
    bool foundCurrentInteraction = false;
    bool foundPredictedInteraction = false;

    InteractionPrediction bestCurrent;
    bestCurrent.currentSurfaceDistanceMeters =
        std::numeric_limits<double>::infinity();

    InteractionPrediction bestPredicted;
    bestPredicted.timeToClosestSeconds =
        std::numeric_limits<double>::infinity();
    bestPredicted.closestSurfaceDistanceMeters =
        std::numeric_limits<double>::infinity();

    ActivationAnchor bestCurrentAnchor;
    ActivationAnchor bestPredictedAnchor;

    for (const std::size_t candidateIndex : candidateIndices)
    {
        if (candidateIndex >= anchors.size())
            continue;

        const auto& anchor = anchors[candidateIndex];

        if (anchor.systemId != subjectSystemId)
            continue;

        if (isSameEntityAnchor(subjectId, anchor))
            continue;

        foundComparableAnchor = true;
        ++result.comparableAnchorCount;

        const auto prediction =
            evaluateInteractionHorizon(
                subject,
                anchor.kinematics,
                policy
            );

        if (prediction.currentlyWithinEnvelope)
        {
            if (!foundCurrentInteraction ||
                isBetterCurrentInteraction(prediction, bestCurrent))
            {
                foundCurrentInteraction = true;
                bestCurrent = prediction;
                bestCurrentAnchor = anchor;
            }

            continue;
        }

        if (prediction.entersEnvelopeWithinHorizon)
        {
            if (!foundPredictedInteraction ||
                isBetterPredictedInteraction(prediction, bestPredicted))
            {
                foundPredictedInteraction = true;
                bestPredicted = prediction;
                bestPredictedAnchor = anchor;
            }
        }
    }

    if (foundCurrentInteraction)
    {
        result.desiredMode = SimulationMode::Active;
        result.reason = ActivationReason::CurrentInteraction;
        result.hasAnchor = true;
        result.anchorId = bestCurrentAnchor.id;
        result.anchorKind = bestCurrentAnchor.kind;
        result.prediction = bestCurrent;
        return result;
    }

    if (foundPredictedInteraction)
    {
        result.desiredMode = SimulationMode::Prewarm;
        result.reason = ActivationReason::PredictedInteraction;
        result.hasAnchor = true;
        result.anchorId = bestPredictedAnchor.id;
        result.anchorKind = bestPredictedAnchor.kind;
        result.prediction = bestPredicted;
        return result;
    }

    result.desiredMode = SimulationMode::Coarse;
    result.reason = foundComparableAnchor
        ? ActivationReason::NoInteractionWithinHorizon
        : ActivationReason::NoComparableAnchors;

    return result;
}

inline ActivationShadowDecision evaluateActivationShadow(
    EntityId subjectId,
    int subjectSystemId,
    const KinematicPoint& subject,
    SimulationMode currentMode,
    bool pinnedActive,
    const std::vector<ActivationAnchor>& anchors,
    const InteractionHorizonPolicy& policy
) noexcept
{
    std::vector<std::size_t> candidateIndices;
    candidateIndices.reserve(anchors.size());
    for (std::size_t index = 0; index < anchors.size(); ++index)
        candidateIndices.push_back(index);

    return evaluateActivationShadowCandidates(
        subjectId,
        subjectSystemId,
        subject,
        currentMode,
        pinnedActive,
        anchors,
        candidateIndices,
        policy,
        false
    );
}

} // namespace game::simulation::activation
