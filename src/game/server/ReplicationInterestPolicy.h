#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "src/game/simulation/SimulationSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::server
{

/*
    Per-client network interest is deliberately separate from simulation
    activation. A ship may be Active because it interacts with player A while
    player B needs only a coarse update, or no replication at all.

    This policy describes transport cost only. It is NOT a sensor/visibility or
    anti-cheat authorization decision. Before sparse publication is enabled,
    the selected tier must also be intersected with whatever world-knowledge
    rules determine whether that client is allowed to know the entity exists.
*/
enum class ShipReplicationInterestTier : std::uint8_t
{
    None = 0,
    Coarse,
    Nearby,
    Tactical,
    Controlled
};

struct ReplicationInterestPolicy
{
    // Provisional transport radii, intentionally centralized/tunable and not
    // treated as gameplay sensor ranges.
    double tacticalRadiusMeters = 100000.0;   // 100 km
    double nearbyRadiusMeters = 5000000.0;    // 5,000 km

    // Target publication intervals. Zero means every normal server snapshot.
    double controlledIntervalSeconds = 0.0;
    double tacticalIntervalSeconds = 0.0;
    double nearbyIntervalSeconds = 0.20;
    double coarseIntervalSeconds = 1.00;
};

struct ShipReplicationInterestDecision
{
    EntityId id {0};
    ShipReplicationInterestTier tier = ShipReplicationInterestTier::None;
    bool sameSystem = false;
    double distanceMeters = std::numeric_limits<double>::infinity();
    double targetIntervalSeconds = std::numeric_limits<double>::infinity();
};

struct ReplicationInterestSummary
{
    std::uint32_t controlledShips = 0;
    std::uint32_t tacticalShips = 0;
    std::uint32_t nearbyShips = 0;
    std::uint32_t coarseShips = 0;
    std::uint32_t outOfScopeShips = 0;
};

struct ShipReplicationInterestPlan
{
    std::vector<ShipReplicationInterestDecision> decisions;
    ReplicationInterestSummary summary;
};

inline double replicationIntervalSeconds(
    ShipReplicationInterestTier tier,
    const ReplicationInterestPolicy& policy
) noexcept
{
    switch (tier)
    {
        case ShipReplicationInterestTier::Controlled:
            return std::max(0.0, policy.controlledIntervalSeconds);
        case ShipReplicationInterestTier::Tactical:
            return std::max(0.0, policy.tacticalIntervalSeconds);
        case ShipReplicationInterestTier::Nearby:
            return std::max(0.0, policy.nearbyIntervalSeconds);
        case ShipReplicationInterestTier::Coarse:
            return std::max(0.0, policy.coarseIntervalSeconds);
        case ShipReplicationInterestTier::None:
            return std::numeric_limits<double>::infinity();
    }

    return std::numeric_limits<double>::infinity();
}

inline ShipReplicationInterestDecision evaluateShipReplicationInterest(
    EntityId controlledEntityId,
    int controlledSystemId,
    const world::coordinates::WorldPosition& controlledPosition,
    const ShipSnapshot& candidate,
    const ReplicationInterestPolicy& policy = {}
) noexcept
{
    ShipReplicationInterestDecision result;
    result.id = candidate.id;

    if (candidate.id == controlledEntityId)
    {
        result.tier = ShipReplicationInterestTier::Controlled;
        result.sameSystem = true;
        result.distanceMeters = 0.0;
        result.targetIntervalSeconds =
            replicationIntervalSeconds(result.tier, policy);
        return result;
    }

    const int candidateSystemId = candidate.transform.motion.systemId;
    result.sameSystem = candidateSystemId == controlledSystemId;

    if (!result.sameSystem)
    {
        result.tier = ShipReplicationInterestTier::None;
        result.targetIntervalSeconds =
            replicationIntervalSeconds(result.tier, policy);
        return result;
    }

    result.distanceMeters = world::coordinates::distanceMeters(
        candidate.transform.worldPosition,
        controlledPosition
    );

    const double tacticalRadius =
        std::max(0.0, policy.tacticalRadiusMeters);
    const double nearbyRadius =
        std::max(tacticalRadius, policy.nearbyRadiusMeters);

    if (result.distanceMeters <= tacticalRadius)
        result.tier = ShipReplicationInterestTier::Tactical;
    else if (result.distanceMeters <= nearbyRadius)
        result.tier = ShipReplicationInterestTier::Nearby;
    else
        result.tier = ShipReplicationInterestTier::Coarse;

    result.targetIntervalSeconds =
        replicationIntervalSeconds(result.tier, policy);
    return result;
}

inline ShipReplicationInterestPlan buildShipReplicationInterestPlan(
    EntityId controlledEntityId,
    const SimulationSnapshot& snapshot,
    const ReplicationInterestPolicy& policy = {}
)
{
    ShipReplicationInterestPlan plan;

    const auto controlledIt = std::find_if(
        snapshot.ships.begin(),
        snapshot.ships.end(),
        [&](const ShipSnapshot& ship)
        {
            return ship.id == controlledEntityId;
        }
    );

    if (controlledIt == snapshot.ships.end())
        return plan;

    const int controlledSystemId =
        controlledIt->transform.motion.systemId;
    const auto controlledPosition =
        controlledIt->transform.worldPosition;

    plan.decisions.reserve(snapshot.ships.size());

    for (const auto& ship : snapshot.ships)
    {
        auto decision = evaluateShipReplicationInterest(
            controlledEntityId,
            controlledSystemId,
            controlledPosition,
            ship,
            policy
        );

        switch (decision.tier)
        {
            case ShipReplicationInterestTier::Controlled:
                ++plan.summary.controlledShips;
                break;
            case ShipReplicationInterestTier::Tactical:
                ++plan.summary.tacticalShips;
                break;
            case ShipReplicationInterestTier::Nearby:
                ++plan.summary.nearbyShips;
                break;
            case ShipReplicationInterestTier::Coarse:
                ++plan.summary.coarseShips;
                break;
            case ShipReplicationInterestTier::None:
                ++plan.summary.outOfScopeShips;
                break;
        }

        plan.decisions.push_back(std::move(decision));
    }

    return plan;
}

} // namespace game::server
