#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "src/world/navigation/NavigationObstacle.h"

namespace world::navigation
{

// NavigationSpace is the ownership boundary for persistent static navigation
// topology in the active ship-centered working space. The first CPU reference
// uses free-space regions + portals internally; callers receive only compact
// query products and never internal acceleration/storage structures.
class NavigationSpace final
{
public:
    using RegionId = std::uint64_t;
    using PortalId = std::uint64_t;
    using Revision = std::uint64_t;

    struct Vec3d
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    struct Bounds3d
    {
        Vec3d minMapMeters {};
        Vec3d maxMapMeters {};
    };

    // Effective required clearance is radius + additionalClearance.
    struct AgentEnvelope
    {
        double radiusMeters = 0.0;
        double additionalClearanceMeters = 0.0;
    };

    // First CPU reference region: an axis-aligned free-space volume in map
    // coordinates. The public boundary deliberately describes free space, not
    // the implementation's search/index structures.
    struct RegionInput
    {
        RegionId regionId = 0;
        Bounds3d boundsMapMeters {};
        double clearanceRadiusMeters = 1.0e30;
        std::uint32_t flags = 0;
        Revision geometryRevision = 0;
    };

    struct PortalInput
    {
        PortalId portalId = 0;
        RegionId regionA = 0;
        RegionId regionB = 0;
        Vec3d centerMapMeters {};
        double clearanceRadiusMeters = 0.0;
        bool bidirectional = true;
        std::uint32_t flags = 0;
        Revision geometryRevision = 0;
    };

    struct StaticSpaceUpdate
    {
        Revision sourceRevision = 0;
        std::vector<RegionInput> regions;
        std::vector<PortalInput> portals;

        // Exact persistent static collision geometry expressed in this
        // NavigationSpace's map frame. Regions/portals remain coarse free-space
        // topology; obstacles are the precision layer used to prove local
        // segments and apertures without replacing OBBs by enclosing spheres.
        std::vector<NavigationObstacle> obstacles;
    };

    // Transactional local patch. Upserts clear invalidation for the supplied
    // records. Removing a region also removes portals that reference it.
    struct LocalPatch
    {
        Revision sourceRevision = 0;
        std::vector<RegionId> removeRegionIds;
        std::vector<PortalId> removePortalIds;
        std::vector<RegionInput> upsertRegions;
        std::vector<PortalInput> upsertPortals;

        std::vector<std::string> removeObstacleIds;
        std::vector<NavigationObstacle> upsertObstacles;
    };

    struct PointQuery
    {
        Vec3d pointMapMeters {};
        AgentEnvelope envelope {};
    };

    struct PointQueryResult
    {
        Revision spaceRevision = 0;
        Revision sourceRevision = 0;
        bool traversable = false;
        RegionId regionId = 0;
        double availableClearanceMeters = 0.0;
        std::size_t regionsExamined = 0;
        std::size_t obstaclesExamined = 0;
        std::string blockingObstacleId;
        std::uint32_t blockingObstacleEntityId = 0;
    };

    struct SegmentQuery
    {
        Vec3d startMapMeters {};
        Vec3d endMapMeters {};
        AgentEnvelope envelope {};

        // LocalAvoidance uses same-region convex proof plus exact obstacle
        // rejection. Route-wide traversal across regions remains the corridor
        // graph's job.
        bool requireSameRegion = true;

        // A selected portal center lies on the shared boundary of two coarse
        // free-space regions. Corridor routing has already proved the portal's
        // envelope clearance, so the precision layer may allow that one
        // endpoint to touch the start region boundary while still proving the
        // complete segment against exact obstacles.
        bool allowEndOnStartRegionBoundary = false;
    };

    struct SegmentQueryResult
    {
        Revision spaceRevision = 0;
        Revision sourceRevision = 0;
        bool traversable = false;
        RegionId startRegionId = 0;
        RegionId endRegionId = 0;
        std::size_t regionsExamined = 0;
        std::size_t obstaclesExamined = 0;
        std::string blockingObstacleId;
        std::uint32_t blockingObstacleEntityId = 0;
    };

    struct CorridorQuery
    {
        Vec3d startMapMeters {};
        Vec3d endMapMeters {};
        AgentEnvelope envelope {};
    };

    struct CorridorDiagnostics
    {
        std::size_t regionsVisited = 0;
        std::size_t portalsExamined = 0;
    };

    struct CorridorResult
    {
        Revision spaceRevision = 0;
        Revision sourceRevision = 0;
        bool found = false;
        CorridorDiagnostics diagnostics {};
        std::vector<RegionId> regionPath;
        std::vector<PortalId> portalPath;

        // Ordered compact steering seam for the selected corridor. Entries
        // correspond one-to-one with portalPath. Callers may steer toward the
        // next portal without reading NavigationSpace internals or rebuilding
        // a second copy of the region graph.
        std::vector<Vec3d> portalCentersMapMeters;
    };

    // Costed corridor policy. Cost units are meter-equivalent: geometric
    // distance contributes distanceWeight * meters; clearancePenaltyMeters is
    // the maximum extra per-edge penalty when traversable clearance is below
    // preferredClearanceMultiple * required agent clearance. When
    // turnPenaltyMetersPerRadian is positive, turn-aware search retains the
    // incoming portal in its state so different arrival headings are not
    // incorrectly collapsed into one region-only state.
    struct CorridorCostPolicy
    {
        double distanceWeight = 1.0;
        double preferredClearanceMultiple = 1.0;
        double clearancePenaltyMeters = 0.0;
        double turnPenaltyMetersPerRadian = 0.0;
    };

    struct CostedCorridorResult
    {
        Revision spaceRevision = 0;
        Revision sourceRevision = 0;
        bool found = false;
        double totalCostMetersEquivalent = 0.0;
        CorridorDiagnostics diagnostics {};
        std::vector<RegionId> regionPath;
        std::vector<PortalId> portalPath;

        // Same ordered compact steering seam as CorridorResult.
        std::vector<Vec3d> portalCentersMapMeters;
    };

    struct InvalidationResult
    {
        Revision spaceRevision = 0;
        Revision sourceRevision = 0;
        std::vector<RegionId> invalidatedRegionIds;
        std::vector<PortalId> invalidatedPortalIds;
    };

    struct Stats
    {
        Revision spaceRevision = 0;
        Revision sourceRevision = 0;
        std::size_t regionCount = 0;
        std::size_t portalCount = 0;
        std::size_t obstacleCount = 0;
        std::size_t invalidatedRegionCount = 0;
        std::size_t invalidatedPortalCount = 0;
    };

    NavigationSpace();
    ~NavigationSpace();

    NavigationSpace(NavigationSpace&&) noexcept;
    NavigationSpace& operator=(NavigationSpace&&) noexcept;

    NavigationSpace(const NavigationSpace&) = delete;
    NavigationSpace& operator=(const NavigationSpace&) = delete;

    void replaceStaticWorld(StaticSpaceUpdate update);
    void applyLocalPatch(LocalPatch patch);

    // Fail-closed local invalidation. Intersecting free-space regions and their
    // portals become non-traversable until replaced by a local patch/full update.
    [[nodiscard]] InvalidationResult invalidateBounds(
        const Bounds3d& boundsMapMeters,
        Revision sourceRevision
    );

    [[nodiscard]] PointQueryResult queryPoint(const PointQuery& query) const;
    [[nodiscard]] SegmentQueryResult querySegment(const SegmentQuery& query) const;
    [[nodiscard]] CorridorResult queryCorridor(const CorridorQuery& query) const;
    [[nodiscard]] CostedCorridorResult queryCostedCorridor(
        const CorridorQuery& query,
        const CorridorCostPolicy& policy
    ) const;
    [[nodiscard]] Stats stats() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace world::navigation
