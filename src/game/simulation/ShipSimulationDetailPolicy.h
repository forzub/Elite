#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/simulation/ShipSimulationTier.h"
#include "src/game/simulation/activation/ActivationClaim.h"

namespace game::simulation
{

enum class ShipMotionFidelityReason : std::uint8_t
{
    None = 0,
    ExplicitInteraction,
    CollisionProximity,
    AimCone,
    ObservedNear,
    Visible,
    Background
};

struct ShipObserverView
{
    EntityId observerId {0};
    int systemId = -1;
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 forward {0.0, 0.0, -1.0};
    bool valid = false;
};

struct ShipMotionFidelityPolicy
{
    // Server-side observability envelope. Exact camera visibility may later
    // arrive as a client interest claim, but shared simulation truth is always
    // resolved from the strongest observer demand.
    double kinematicVisibleDistanceMeters = 15000.0;
    double responseVisibleDistanceMeters = 4000.0;
    double fullControlAimDistanceMeters = 3000.0;

    // Independent of view direction: nearby ships may collide or enter combat.
    double responseProximityMeters = 1200.0;
    double fullControlProximityMeters = 350.0;

    // Half-angle cosines: ~75 deg visible, ~35 deg observed-near, ~4 deg aim.
    double visibleCosine = 0.25881904510252074;
    double responseCosine = 0.8191520442889918;
    double aimCosine = 0.9975640502598242;

    // Promotion is immediate. Demotion is deliberately staged.
    double fullControlReleaseDelaySeconds = 1.5;
    double responseReleaseDelaySeconds = 2.5;
    double kinematicReleaseDelaySeconds = 4.0;

    // Relevance itself does not need a 50 Hz pass.
    double evaluationIntervalSeconds = 0.10;

    // Background authoritative motion cadence. The client owns smooth visual
    // interpolation; the server retains coarse truth for multiplayer.
    double routeOnlyServerMotionIntervalSeconds = 0.50;
    double kinematicServerMotionIntervalSeconds = 0.10;
};

inline int shipMotionFidelityPriority(
    ShipMotionFidelity fidelity
) noexcept
{
    return static_cast<int>(fidelity);
}

inline ShipMotionFidelity strongerMotionFidelity(
    ShipMotionFidelity a,
    ShipMotionFidelity b
) noexcept
{
    return shipMotionFidelityPriority(a) >= shipMotionFidelityPriority(b)
        ? a
        : b;
}

struct ShipMotionFidelityDemand
{
    ShipMotionFidelity requested = ShipMotionFidelity::RouteOnly;
    ShipMotionFidelityReason reason = ShipMotionFidelityReason::Background;
    EntityId observerId {0};
    double distanceMeters = std::numeric_limits<double>::infinity();
    double forwardAlignment = -1.0;
};

inline bool claimRequiresFullControl(
    const activation::ActivationClaim& claim,
    EntityId subjectId,
    int systemId,
    double serverTimeSeconds
) noexcept
{
    return
        claim.subjectId == subjectId &&
        claim.systemId == systemId &&
        activation::activationClaimIsLive(claim, serverTimeSeconds) &&
        claim.kind != activation::ActivationClaimKind::None;
}

inline ShipMotionFidelityDemand evaluateShipMotionFidelityDemand(
    EntityId subjectId,
    int subjectSystemId,
    const glm::dvec3& subjectPositionMeters,
    const std::vector<ShipObserverView>& observers,
    const std::vector<activation::ActivationClaim>& claims,
    double serverTimeSeconds,
    const ShipMotionFidelityPolicy& policy = {}
) noexcept
{
    ShipMotionFidelityDemand result;

    for (const auto& claim : claims)
    {
        if (!claimRequiresFullControl(
                claim,
                subjectId,
                subjectSystemId,
                serverTimeSeconds))
        {
            continue;
        }

        result.requested = ShipMotionFidelity::FullControl;
        result.reason = ShipMotionFidelityReason::ExplicitInteraction;
        result.observerId = claim.sourceId;
        return result;
    }

    for (const auto& observer : observers)
    {
        if (!observer.valid ||
            observer.systemId != subjectSystemId ||
            observer.observerId == subjectId)
        {
            continue;
        }

        const glm::dvec3 delta =
            subjectPositionMeters - observer.positionMeters;
        const double distance = glm::length(delta);

        if (!std::isfinite(distance))
            continue;

        ShipMotionFidelity candidate = ShipMotionFidelity::RouteOnly;
        ShipMotionFidelityReason reason =
            ShipMotionFidelityReason::Background;
        double alignment = -1.0;

        if (distance <= policy.fullControlProximityMeters)
        {
            candidate = ShipMotionFidelity::FullControl;
            reason = ShipMotionFidelityReason::CollisionProximity;
        }
        else if (distance <= policy.responseProximityMeters)
        {
            candidate = ShipMotionFidelity::Response;
            reason = ShipMotionFidelityReason::CollisionProximity;
        }

        if (distance > 1.0e-9)
        {
            const glm::dvec3 direction = delta / distance;
            const double forwardLength = glm::length(observer.forward);
            if (forwardLength > 1.0e-9)
            {
                alignment = glm::dot(
                    direction,
                    observer.forward / forwardLength
                );

                if (distance <= policy.fullControlAimDistanceMeters &&
                    alignment >= policy.aimCosine)
                {
                    candidate = strongerMotionFidelity(
                        candidate,
                        ShipMotionFidelity::FullControl
                    );
                    if (candidate == ShipMotionFidelity::FullControl)
                        reason = ShipMotionFidelityReason::AimCone;
                }
                else if (
                    distance <= policy.responseVisibleDistanceMeters &&
                    alignment >= policy.responseCosine)
                {
                    candidate = strongerMotionFidelity(
                        candidate,
                        ShipMotionFidelity::Response
                    );
                    if (candidate == ShipMotionFidelity::Response)
                        reason = ShipMotionFidelityReason::ObservedNear;
                }
                else if (
                    distance <= policy.kinematicVisibleDistanceMeters &&
                    alignment >= policy.visibleCosine)
                {
                    candidate = strongerMotionFidelity(
                        candidate,
                        ShipMotionFidelity::Kinematic
                    );
                    if (candidate == ShipMotionFidelity::Kinematic)
                        reason = ShipMotionFidelityReason::Visible;
                }
            }
        }

        if (shipMotionFidelityPriority(candidate) >
            shipMotionFidelityPriority(result.requested))
        {
            result.requested = candidate;
            result.reason = reason;
            result.observerId = observer.observerId;
            result.distanceMeters = distance;
            result.forwardAlignment = alignment;
        }
    }

    return result;
}

struct ShipMotionFidelityRuntimeState
{
    ShipMotionFidelity planned = ShipMotionFidelity::RouteOnly;
    double releaseNotBeforeServerTimeSeconds = 0.0;
    std::uint64_t transitionSerial = 0;
};

struct ShipMotionFidelityUpdate
{
    ShipMotionFidelity previous = ShipMotionFidelity::RouteOnly;
    ShipMotionFidelity requested = ShipMotionFidelity::RouteOnly;
    ShipMotionFidelity planned = ShipMotionFidelity::RouteOnly;
    bool changed = false;
};

inline double shipMotionFidelityReleaseDelaySeconds(
    ShipMotionFidelity fidelity,
    const ShipMotionFidelityPolicy& policy
) noexcept
{
    switch (fidelity)
    {
        case ShipMotionFidelity::FullControl:
            return std::max(0.0, policy.fullControlReleaseDelaySeconds);
        case ShipMotionFidelity::Response:
            return std::max(0.0, policy.responseReleaseDelaySeconds);
        case ShipMotionFidelity::Kinematic:
            return std::max(0.0, policy.kinematicReleaseDelaySeconds);
        case ShipMotionFidelity::RouteOnly:
            return 0.0;
    }

    return 0.0;
}

inline ShipMotionFidelity oneStepTowardLowerFidelity(
    ShipMotionFidelity current,
    ShipMotionFidelity requested
) noexcept
{
    if (shipMotionFidelityPriority(current) <=
        shipMotionFidelityPriority(requested))
    {
        return current;
    }

    const int next = std::max(
        shipMotionFidelityPriority(requested),
        shipMotionFidelityPriority(current) - 1
    );
    return static_cast<ShipMotionFidelity>(next);
}

inline ShipMotionFidelityUpdate updateShipMotionFidelityPlan(
    ShipMotionFidelityRuntimeState& state,
    ShipMotionFidelity requested,
    double serverTimeSeconds,
    const ShipMotionFidelityPolicy& policy = {}
) noexcept
{
    ShipMotionFidelityUpdate result;
    result.previous = state.planned;
    result.requested = requested;

    const double now = std::max(0.0, serverTimeSeconds);
    const int requestedPriority = shipMotionFidelityPriority(requested);
    const int currentPriority = shipMotionFidelityPriority(state.planned);

    if (requestedPriority > currentPriority)
    {
        state.planned = requested;
        state.releaseNotBeforeServerTimeSeconds =
            now + shipMotionFidelityReleaseDelaySeconds(
                state.planned,
                policy
            );
        ++state.transitionSerial;
        result.changed = true;
    }
    else if (requestedPriority == currentPriority)
    {
        state.releaseNotBeforeServerTimeSeconds =
            now + shipMotionFidelityReleaseDelaySeconds(
                state.planned,
                policy
            );
    }
    else if (now >= state.releaseNotBeforeServerTimeSeconds)
    {
        state.planned =
            oneStepTowardLowerFidelity(state.planned, requested);
        state.releaseNotBeforeServerTimeSeconds =
            now + shipMotionFidelityReleaseDelaySeconds(
                state.planned,
                policy
            );
        ++state.transitionSerial;
        result.changed = true;
    }

    result.planned = state.planned;
    return result;
}

inline double shellMotionIntervalSeconds(
    ShipMotionFidelity fidelity,
    const ShipMotionFidelityPolicy& policy = {}
) noexcept
{
    return fidelity == ShipMotionFidelity::Kinematic
        ? std::max(0.0, policy.kinematicServerMotionIntervalSeconds)
        : std::max(0.0, policy.routeOnlyServerMotionIntervalSeconds);
}

} // namespace game::simulation
