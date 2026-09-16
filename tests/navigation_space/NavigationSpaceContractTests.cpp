#include "src/world/navigation/space/NavigationSpace.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using Space = world::navigation::NavigationSpace;

[[noreturn]] void fail(const std::string& message)
{
    throw std::runtime_error(message);
}

void require(bool condition, const std::string& message)
{
    if (!condition)
        fail(message);
}

Space::RegionInput region(
    Space::RegionId id,
    double minX,
    double maxX,
    double clearance = 4.0
)
{
    Space::RegionInput result;
    result.regionId = id;
    result.boundsMapMeters.minMapMeters = {minX, 0.0, 0.0};
    result.boundsMapMeters.maxMapMeters = {maxX, 10.0, 10.0};
    result.clearanceRadiusMeters = clearance;
    result.geometryRevision = 1;
    return result;
}

Space::RegionInput regionBox(
    Space::RegionId id,
    double cx,
    double cy,
    double cz,
    double hx,
    double hy,
    double hz,
    double clearance = 10.0
)
{
    Space::RegionInput result;
    result.regionId = id;
    result.boundsMapMeters.minMapMeters = {cx - hx, cy - hy, cz - hz};
    result.boundsMapMeters.maxMapMeters = {cx + hx, cy + hy, cz + hz};
    result.clearanceRadiusMeters = clearance;
    result.geometryRevision = 1;
    return result;
}

Space::PortalInput portal(
    Space::PortalId id,
    Space::RegionId a,
    Space::RegionId b,
    double x,
    double clearance
)
{
    Space::PortalInput result;
    result.portalId = id;
    result.regionA = a;
    result.regionB = b;
    result.centerMapMeters = {x, 5.0, 5.0};
    result.clearanceRadiusMeters = clearance;
    result.bidirectional = true;
    result.geometryRevision = 1;
    return result;
}

Space::PortalInput portalAt(
    Space::PortalId id,
    Space::RegionId a,
    Space::RegionId b,
    double x,
    double y,
    double z,
    double clearance
)
{
    Space::PortalInput result;
    result.portalId = id;
    result.regionA = a;
    result.regionB = b;
    result.centerMapMeters = {x, y, z};
    result.clearanceRadiusMeters = clearance;
    result.bidirectional = true;
    result.geometryRevision = 1;
    return result;
}

Space::StaticSpaceUpdate baselineWorld()
{
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 10;

    // Deliberately publish in non-sorted order. Query output must still be
    // deterministic because NavigationSpace owns ordering internally.
    update.regions = {
        region(3, 20.0, 30.0), // interior
        region(1, 0.0, 10.0),  // outside
        region(4, 40.0, 50.0), // disconnected pocket
        region(2, 10.0, 20.0)  // vestibule
    };

    update.portals = {
        portal(102, 2, 3, 20.0, 1.25),
        portal(101, 1, 2, 10.0, 2.0)
    };

    return update;
}

Space::AgentEnvelope envelope(double radius, double extra = 0.0)
{
    return {radius, extra};
}

void testPointClearanceAndEnvelope()
{
    Space space;
    space.replaceStaticWorld(baselineWorld());

    Space::PointQuery center;
    center.pointMapMeters = {5.0, 5.0, 5.0};
    center.envelope = envelope(1.0);

    const auto centerResult = space.queryPoint(center);
    require(centerResult.traversable, "open-space center must be traversable");
    require(centerResult.regionId == 1, "center must resolve to outside region 1");
    require(centerResult.availableClearanceMeters == 4.0,
            "authored region clearance cap must bound point clearance");

    Space::PointQuery nearWall = center;
    nearWall.pointMapMeters = {5.0, 0.5, 5.0};
    const auto nearWallResult = space.queryPoint(nearWall);
    require(!nearWallResult.traversable,
            "solid free-space boundary must reject insufficient clearance");
    require(nearWallResult.regionId == 1,
            "failed clearance query should still identify containing region");

    Space::PointQuery exactLarge = center;
    exactLarge.envelope = envelope(4.0);
    require(space.queryPoint(exactLarge).traversable,
            "agent matching authored clearance must fit");

    Space::PointQuery oversized = center;
    oversized.envelope = envelope(4.01);
    require(!space.queryPoint(oversized).traversable,
            "same geometry must reject a larger agent envelope");
}

void testConnectedDisconnectedAndNarrowPortal()
{
    Space space;
    space.replaceStaticWorld(baselineWorld());

    Space::CorridorQuery connected;
    connected.startMapMeters = {5.0, 5.0, 5.0};
    connected.endMapMeters = {15.0, 5.0, 5.0};
    connected.envelope = envelope(1.0);

    const auto direct = space.queryCorridor(connected);
    require(direct.found, "connected regions must return a corridor");
    require(direct.regionPath == std::vector<Space::RegionId>({1, 2}),
            "direct corridor region order is wrong");
    require(direct.portalPath == std::vector<Space::PortalId>({101}),
            "direct corridor portal order is wrong");

    Space::CorridorQuery outsideToInterior = connected;
    outsideToInterior.endMapMeters = {25.0, 5.0, 5.0};
    const auto interior = space.queryCorridor(outsideToInterior);
    require(interior.found, "outside-to-interior portal chain must be routable");
    require(interior.regionPath == std::vector<Space::RegionId>({1, 2, 3}),
            "outside-to-interior region path is not deterministic");
    require(interior.portalPath == std::vector<Space::PortalId>({101, 102}),
            "outside-to-interior portal path is not deterministic");

    Space::CorridorQuery oversized = outsideToInterior;
    oversized.envelope = envelope(1.5);
    require(!space.queryCorridor(oversized).found,
            "narrow portal must reject oversized agent envelope");

    Space::CorridorQuery disconnected = connected;
    disconnected.endMapMeters = {45.0, 5.0, 5.0};
    require(!space.queryCorridor(disconnected).found,
            "disconnected free-space region must not produce a route");
}

void testWallApertureAdmission()
{
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 30;
    update.regions = {
        regionBox(1, 0.0, 0.0, 0.0, 5.0, 5.0, 5.0),
        regionBox(2, 10.0, 0.0, 0.0, 5.0, 5.0, 5.0)
    };
    update.portals = {
        portalAt(501, 1, 2, 5.0, 0.0, 0.0, 2.0)
    };

    Space space;
    space.replaceStaticWorld(std::move(update));

    Space::CorridorQuery query;
    query.startMapMeters = {0.0, 0.0, 0.0};
    query.endMapMeters = {10.0, 0.0, 0.0};
    query.envelope = envelope(1.0);

    const auto small = space.queryCorridor(query);
    require(small.found && small.portalPath == std::vector<Space::PortalId>({501}),
            "small agent must be allowed through a traversable wall aperture");

    query.envelope = envelope(2.01);
    require(!space.queryCorridor(query).found,
            "oversized agent must not pass through the same wall aperture");
}

void testCostedCanyonVsOverflight()
{
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 40;
    update.regions = {
        regionBox(1, 0.0, 0.0, 0.0, 5.0, 10.0, 5.0),
        regionBox(2, 10.0, 0.0, 0.0, 5.0, 5.0, 5.0),
        regionBox(3, 20.0, 0.0, 0.0, 5.0, 5.0, 5.0),
        regionBox(4, 10.0, 15.0, 0.0, 5.0, 5.0, 5.0),
        regionBox(5, 20.0, 15.0, 0.0, 5.0, 5.0, 5.0),
        regionBox(6, 30.0, 0.0, 0.0, 5.0, 10.0, 5.0)
    };
    update.portals = {
        // Short canyon branch: physically shorter, but only 2 m clearance.
        portalAt(101, 1, 2, 5.0, 0.0, 0.0, 2.0),
        portalAt(102, 2, 3, 15.0, 0.0, 0.0, 2.0),
        portalAt(103, 3, 6, 25.0, 0.0, 0.0, 2.0),

        // Longer overflight branch: generous clearance.
        portalAt(201, 1, 4, 5.0, 10.0, 0.0, 10.0),
        portalAt(202, 4, 5, 15.0, 15.0, 0.0, 10.0),
        portalAt(203, 5, 6, 25.0, 10.0, 0.0, 10.0)
    };

    Space space;
    space.replaceStaticWorld(std::move(update));

    Space::CorridorQuery query;
    query.startMapMeters = {0.0, 0.0, 0.0};
    query.endMapMeters = {30.0, 0.0, 0.0};
    query.envelope = envelope(1.0);

    Space::CorridorCostPolicy shortest;
    const auto canyon = space.queryCostedCorridor(query, shortest);
    require(canyon.found,
            "costed corridor must find the short canyon branch");
    require(canyon.regionPath == std::vector<Space::RegionId>({1, 2, 3, 6}),
            "distance-only policy must prefer the shorter canyon route");
    require(canyon.portalPath == std::vector<Space::PortalId>({101, 102, 103}),
            "distance-only canyon portal path is wrong");

    Space::CorridorCostPolicy cautious;
    cautious.preferredClearanceMultiple = 3.0;
    cautious.clearancePenaltyMeters = 30.0;
    const auto overflight = space.queryCostedCorridor(query, cautious);
    require(overflight.found,
            "cautious costed corridor must still find an alternate route");
    require(overflight.regionPath == std::vector<Space::RegionId>({1, 4, 5, 6}),
            "clearance-aware policy must prefer open overflight when canyon is too tight");
    require(overflight.portalPath == std::vector<Space::PortalId>({201, 202, 203}),
            "clearance-aware overflight portal path is wrong");
    require(overflight.totalCostMetersEquivalent <
            canyon.totalCostMetersEquivalent + 30.0,
            "costed corridor must expose a finite meter-equivalent route cost");

    query.envelope = envelope(2.01);
    const auto oversized = space.queryCostedCorridor(query, shortest);
    require(oversized.found,
            "oversized canyon agent must still find the open branch");
    require(oversized.regionPath == std::vector<Space::RegionId>({1, 4, 5, 6}),
            "agent that cannot fit the canyon must route over it");
}

void testLocalInvalidationAndPatch()
{
    Space space;
    space.replaceStaticWorld(baselineWorld());

    const auto before = space.stats();
    require(before.regionCount == 4 && before.portalCount == 2,
            "baseline static-space counts are wrong");
    require(before.invalidatedRegionCount == 0 && before.invalidatedPortalCount == 0,
            "baseline must not start invalidated");

    Space::Bounds3d damageBounds;
    damageBounds.minMapMeters = {14.0, 2.0, 2.0};
    damageBounds.maxMapMeters = {16.0, 8.0, 8.0};

    const auto invalidated = space.invalidateBounds(damageBounds, 11);
    require(invalidated.invalidatedRegionIds == std::vector<Space::RegionId>({2}),
            "local invalidation must affect only intersecting region 2");
    require(invalidated.invalidatedPortalIds == std::vector<Space::PortalId>({101, 102}),
            "portals touching invalidated region must fail closed");

    const auto afterInvalidation = space.stats();
    require(afterInvalidation.spaceRevision > before.spaceRevision,
            "local invalidation must advance static-space revision");
    require(afterInvalidation.sourceRevision == 11,
            "local invalidation source revision was not recorded");
    require(afterInvalidation.invalidatedRegionCount == 1,
            "local invalidation must not semantically reset all regions");

    Space::CorridorQuery route;
    route.startMapMeters = {5.0, 5.0, 5.0};
    route.endMapMeters = {25.0, 5.0, 5.0};
    route.envelope = envelope(1.0);
    require(!space.queryCorridor(route).found,
            "route through invalidated local region must fail closed");

    Space::LocalPatch patch;
    patch.sourceRevision = 12;
    patch.upsertRegions = {region(2, 10.0, 20.0)};
    patch.upsertPortals = {
        portal(101, 1, 2, 10.0, 2.0),
        portal(102, 2, 3, 20.0, 1.25)
    };
    space.applyLocalPatch(std::move(patch));

    const auto afterPatch = space.stats();
    require(afterPatch.sourceRevision == 12,
            "local patch source revision was not recorded");
    require(afterPatch.invalidatedRegionCount == 0 &&
            afterPatch.invalidatedPortalCount == 0,
            "local patch must restore replaced region/portal validity");
    require(space.queryCorridor(route).found,
            "local patch must restore corridor without full-world replacement");
}

void testTransactionalValidation()
{
    Space space;
    space.replaceStaticWorld(baselineWorld());
    const auto before = space.stats();

    Space::LocalPatch badPatch;
    badPatch.sourceRevision = 20;
    badPatch.upsertPortals = {portal(999, 1, 9999, 10.0, 1.0)};

    bool threw = false;
    try
    {
        space.applyLocalPatch(std::move(badPatch));
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }

    require(threw, "invalid local patch must be rejected");
    const auto after = space.stats();
    require(after.spaceRevision == before.spaceRevision &&
            after.sourceRevision == before.sourceRevision &&
            after.regionCount == before.regionCount &&
            after.portalCount == before.portalCount,
            "rejected local patch must not partially mutate NavigationSpace");
}

} // namespace

int main()
{
    try
    {
        testPointClearanceAndEnvelope();
        testConnectedDisconnectedAndNarrowPortal();
        testWallApertureAdmission();
        testCostedCanyonVsOverflight();
        testLocalInvalidationAndPatch();
        testTransactionalValidation();

        std::cout << "NAVIGATION SPACE CONTRACT TESTS: PASS\n";
        std::cout << " - free-space clearance is agent-envelope aware\n";
        std::cout << " - region/portal corridors are deterministic\n";
        std::cout << " - explicit wall apertures admit only fitting agents\n";
        std::cout << " - costed routing can choose canyon or overflight by policy\n";
        std::cout << " - narrow portals reject oversized agents\n";
        std::cout << " - disconnected regions fail closed\n";
        std::cout << " - local invalidation and transactional patching work\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "NAVIGATION SPACE CONTRACT TESTS: FAIL: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
