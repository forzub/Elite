#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "world/navigation/local/LocalHorizonPlanner.h"
#include "world/navigation/space/NavigationStaticQueryApi.h"

namespace world::navigation
{

// Deterministic first-stage local adjusted-target selector.
//
// Ownership rules:
// - consumes accepted LocalHorizonPlanner dynamic products;
// - consumes static state only through NavigationStaticQueryApi;
// - owns no actor table, spatial index, static topology or global route;
// - never accepts a lateral target unless static free-space safety is proven.
class LocalAvoidancePlanner final
{
public:
    using StaticQueries = NavigationStaticQueryApi;
    using Vec3d = LocalHorizonPlanner::Vec3d;
    using EntityId = LocalHorizonPlanner::EntityId;

    struct Policy
    {
        // Two deterministic deflection rings around the nominal travel direction.
        // They are intentionally small: this stage selects a temporary target,
        // not a route-wide bypass.
        double primaryDeflectionRadians = 0.2617993877991494;   // 15 deg
        double secondaryDeflectionRadians = 0.5235987755982988; // 30 deg

        // Ordinary free-space transit widens the visibility search only as far
        // as needed. primary/secondary define the first step and increment;
        // the default sequence is 15, 30, 45, 60, 75 degrees. This remains a
        // bounded local search, never a route-wide path solve.
        double maximumDeflectionRadians = 1.3089969389957472; // 75 deg

        // Number of azimuth samples around each ring. The first reference pins
        // this to a small bounded fan so per-agent work remains predictable.
        std::size_t azimuthSamples = 8;

        // Extra static clearance required in addition to the agent radius.
        double staticAdditionalClearanceMeters = 0.0;

        // Set only when the nominal target is a corridor-selected portal center
        // whose envelope clearance was already proved by the static query API.
        // Adjusted probes never inherit this exception.
        bool nominalTargetIsProvenPortalBoundary = false;
    };

    struct Query
    {
        LocalHorizonPlanner::Query horizon {};
        Policy avoidance {};

        // Optional continuity hint owned by the accepted/execution layer.
        // On a bounded replan this is the direction of the previously accepted
        // local progress segment. It is not hidden planner memory.
        bool preferredDirectionValid = false;
        Vec3d preferredDirectionMap {0.0, 0.0, 0.0};
    };

    enum class Status
    {
        NominalClear,
        AdjustedClear,
        ConflictHold,
        StaleHold,
        StaticHold
    };

    struct Result
    {
        Status status = Status::StaticHold;
        LocalHorizonPlanner::Result target {};

        bool adjustedTarget = false;

        // Visibility-steering diagnostics. Nominal visibility means the direct
        // bounded corridor to the accepted target was clear. When false, the
        // selected deflection is the smallest tested angular deviation that
        // passed both exact-static and dynamic horizon proofs.
        bool nominalVisibilityClear = false;
        double selectedDeflectionRadians = 0.0;

        // True only after the complete ordinary progress-preserving fan
        // (up to maximumDeflectionRadians) was evaluated without a safe
        // target. This is an escalation signal for the higher maneuver layer,
        // not permission to disable control.
        bool ordinarySearchExhausted = false;

        bool nominalStaticBlocked = false;
        std::size_t targetProbesExamined = 0;
        std::size_t staticRejected = 0;
        std::size_t dynamicRejected = 0;
        std::size_t staticObstaclesExamined = 0;

        // Exact static blocker that rejected the nominal bounded segment.
        // This is independent from NavigationMap dynamic conflict identity.
        std::string nominalStaticObstacleId;
        std::uint32_t nominalStaticObstacleEntityId = 0;

        // Preserve the conflict that rejected the unmodified nominal target.
        // Once an adjusted probe is Clear, target.primaryConflictEntityId is
        // correctly zero; diagnostics still need to know what caused the
        // deviation without re-running a second planner.
        EntityId nominalPrimaryConflictEntityId = 0;
        std::size_t nominalConflictsFound = 0;

        StaticQueries::Revision spaceRevision = 0;
        StaticQueries::Revision spaceSourceRevision = 0;
        StaticQueries::RegionId startRegionId = 0;
    };

    [[nodiscard]] Result evaluate(
        const Query& query,
        const NavigationMap::QueryResult& dynamicCandidates,
        const StaticQueries& staticQueries
    ) const;
};

} // namespace world::navigation
