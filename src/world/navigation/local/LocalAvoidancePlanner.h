#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "world/navigation/local/LocalHorizonPlanner.h"
#include "world/navigation/space/NavigationStaticQueryApi.h"

namespace world::navigation
{

// Stateless visible-horizon local bypass selector.
//
// The global/topology planner already owns the planned route and static world.
// This layer only reacts to a bounded conflict in front of the vehicle:
//
// nominal trajectory tangent
//     -> project predicted dynamic occupancy onto the normal plane
//     -> choose the smallest reachable lateral offset that remains free
//     -> prove the temporary segment against exact static geometry
//     -> publish a bypass target plus the original on-route merge target.
//
// It owns no branch memory, left/right commitment, angular ray fan or
// branch-switch recovery state.
class LocalAvoidancePlanner final
{
public:
    using StaticQueries = NavigationStaticQueryApi;
    using Vec3d = LocalHorizonPlanner::Vec3d;
    using EntityId = LocalHorizonPlanner::EntityId;

    struct Policy
    {
        // Cartesian search on the plane perpendicular to the nominal route.
        // Candidate offsets are multiples of a scale derived from the vehicle
        // envelope and safety margin, never angular rays.
        std::size_t lateralGridHalfExtentSamples = 4;
        double minimumLateralStepMeters = 2.0;
        double lateralStepEnvelopeMultiplier = 1.0;
        double maximumLateralOffsetMeters = 120.0;

        // Candidate bypass stations are sampled longitudinally inside the
        // bounded nominal horizon. A candidate proves only the safe short
        // segment that can be executed now. Reacquisition of the nominal line
        // is handled by later receding-horizon updates; there is no mandatory
        // same-horizon merge.
        std::size_t longitudinalSamples = 3;

        // Additional inflation for predicted dynamic occupancy in the normal
        // plane and in the time-coupled candidate check.
        double projectionPaddingMeters = 2.0;

        // Number of temporal samples used to prove the selected short bypass
        // segment against predicted moving actors. Physical maneuver
        // compilation/continuous proof remains downstream ownership.
        std::size_t trajectorySamples = 24;

        // Extra exact-static clearance beyond the agent radius.
        double staticAdditionalClearanceMeters = 0.0;

        // Set only when the nominal target is a corridor-selected portal center
        // whose envelope clearance was already proved by the static query API.
        // Temporary bypass targets never inherit this exception.
        bool nominalTargetIsProvenPortalBoundary = false;
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
        bool nominalPathClear = false;
        bool localBypassExhausted = false;

        // Temporary off-route target plus an on-route reacquisition reference.
        // The reference is diagnostic/planning context, not an endpoint that
        // must be reached inside the current visible horizon.
        Vec3d selectedLateralOffsetMap {};
        double selectedLateralOffsetMeters = 0.0;
        double selectedBypassForwardDistanceMeters = 0.0;
        Vec3d mergeTargetMapMeters {};

        // Visible-horizon projection diagnostics.
        std::size_t projectedDynamicObstacles = 0;
        std::size_t offsetCandidatesExamined = 0;
        std::size_t routeCandidatesExamined = 0;
        std::size_t projectionRejected = 0;
        std::size_t staticRejected = 0;
        std::size_t dynamicRejected = 0;
        std::size_t staticObstaclesExamined = 0;
        double selectedProjectedClearanceMeters = 0.0;

        // Exact static blocker that rejected the nominal bounded segment.
        std::string nominalStaticObstacleId;
        std::uint32_t nominalStaticObstacleEntityId = 0;
        bool nominalStaticBlocked = false;

        // Preserve the dynamic conflict that rejected the nominal trajectory.
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
