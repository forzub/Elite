#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

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
    [[nodiscard]] CorridorResult queryCorridor(const CorridorQuery& query) const;
    [[nodiscard]] Stats stats() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace world::navigation
