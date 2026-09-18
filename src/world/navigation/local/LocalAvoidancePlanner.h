#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "world/navigation/local/LocalHorizonPlanner.h"
#include "world/navigation/space/NavigationSpace.h"

namespace world::navigation
{

// Deterministic first-stage local adjusted-target selector.
//
// Ownership rules:
// - consumes accepted LocalHorizonPlanner dynamic products;
// - consumes NavigationSpace only through its public point-query boundary;
// - owns no actor table, spatial index, static topology or global route;
// - never accepts a lateral target unless static free-space safety is proven.
class LocalAvoidancePlanner final
{
public:
    using Vec3d = LocalHorizonPlanner::Vec3d;
    using EntityId = LocalHorizonPlanner::EntityId;

    struct Policy
    {
        // Two deterministic deflection rings around the nominal travel direction.
        // They are intentionally small: this stage selects a temporary target,
        // not a route-wide bypass.
        double primaryDeflectionRadians = 0.2617993877991494;   // 15 deg
        double secondaryDeflectionRadians = 0.5235987755982988; // 30 deg

        // Number of azimuth samples around each ring. The first reference pins
        // this to a small bounded fan so per-agent work remains predictable.
        std::size_t azimuthSamples = 8;

        // Extra static clearance required in addition to the agent radius.
        double staticAdditionalClearanceMeters = 0.0;
    };

    struct Query
    {
        LocalHorizonPlanner::Query horizon {};
        Policy avoidance {};
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

        NavigationSpace::Revision spaceRevision = 0;
        NavigationSpace::Revision spaceSourceRevision = 0;
        NavigationSpace::RegionId startRegionId = 0;
    };

    [[nodiscard]] Result evaluate(
        const Query& query,
        const NavigationMap::QueryResult& dynamicCandidates,
        const NavigationSpace& staticSpace
    ) const;
};

} // namespace world::navigation
