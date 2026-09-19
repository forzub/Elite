#pragma once

#include "world/navigation/space/NavigationSpace.h"

namespace world::navigation
{

// Read-only capability for persistent static navigation state.
//
// NavigationSpace owns publication, mutation, invalidation and storage.
// Calculation layers receive only this capability. There is deliberately no
// accessor back to the owner and no mutation/statistics surface here.
class NavigationStaticQueryApi final
{
public:
    using Revision = NavigationSpace::Revision;
    using RegionId = NavigationSpace::RegionId;
    using PortalId = NavigationSpace::PortalId;
    using Vec3d = NavigationSpace::Vec3d;
    using AgentEnvelope = NavigationSpace::AgentEnvelope;
    using PointQuery = NavigationSpace::PointQuery;
    using PointQueryResult = NavigationSpace::PointQueryResult;
    using SegmentQuery = NavigationSpace::SegmentQuery;
    using SegmentQueryResult = NavigationSpace::SegmentQueryResult;
    using CorridorQuery = NavigationSpace::CorridorQuery;
    using CorridorResult = NavigationSpace::CorridorResult;
    using CorridorDiagnostics = NavigationSpace::CorridorDiagnostics;
    using PortalTraversal = NavigationSpace::PortalTraversal;
    using CorridorCostPolicy = NavigationSpace::CorridorCostPolicy;
    using CostedCorridorResult = NavigationSpace::CostedCorridorResult;

    explicit NavigationStaticQueryApi(
        const NavigationSpace& owner
    ) noexcept
        : owner_(&owner)
    {
    }

    [[nodiscard]] PointQueryResult queryPoint(
        const PointQuery& query
    ) const
    {
        return owner_->queryPoint(query);
    }

    [[nodiscard]] SegmentQueryResult querySegment(
        const SegmentQuery& query
    ) const
    {
        return owner_->querySegment(query);
    }

    [[nodiscard]] CorridorResult queryCorridor(
        const CorridorQuery& query
    ) const
    {
        return owner_->queryCorridor(query);
    }

    [[nodiscard]] CostedCorridorResult queryCostedCorridor(
        const CorridorQuery& query,
        const CorridorCostPolicy& policy
    ) const
    {
        return owner_->queryCostedCorridor(query, policy);
    }

private:
    const NavigationSpace* owner_ = nullptr;
};

} // namespace world::navigation
