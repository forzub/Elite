#include "world/navigation/map/NavigationMap.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using NavigationMap = world::navigation::NavigationMap;
using Vec3d = NavigationMap::Vec3d;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool nearlyEqual(double a, double b, double epsilon = 1.0e-9)
{
    return std::abs(a - b) <= epsilon;
}

NavigationMap::DynamicActorInput actorAt(
    NavigationMap::EntityId id,
    const Vec3d& mapPosition,
    const Vec3d& mapVelocity = {},
    const Vec3d& mapAcceleration = {},
    double radiusMeters = 10.0
)
{
    NavigationMap::DynamicActorInput actor;
    actor.entityId = id;
    actor.positionMapMeters = mapPosition;
    actor.velocityMapMetersPerSecond = mapVelocity;
    actor.accelerationMapMetersPerSecond2 = mapAcceleration;
    actor.radiusMeters = radiusMeters;
    actor.motionRevision = id * 10;
    return actor;
}

const NavigationMap::Candidate& requireCandidate(
    const NavigationMap::QueryResult& result,
    NavigationMap::EntityId id
)
{
    for (const NavigationMap::Candidate& candidate : result.candidates)
    {
        if (candidate.entityId == id)
            return candidate;
    }
    throw std::runtime_error("expected NavigationMap candidate was not returned");
}

void testOwnedShipCenteredSnapshotAndQueries()
{
    NavigationMap::Config config;
    config.halfExtentMeters = 1000.0;
    config.cellSizeMeters = 100.0;
    config.predictionHorizonSeconds = 2.0;
    config.interactionMarginMeters = 0.0;

    NavigationMap map(config);

    NavigationMap::DynamicWorldUpdate update;
    update.sourceRevision = 41;
    update.actors = {
        actorAt(1, {100.0, 0.0, 0.0}),
        actorAt(2, {0.0, 350.0, 0.0}, {0.0, -175.0, 0.0}),
        actorAt(3, {0.0, 700.0, 0.0}),
        actorAt(4, {1500.0, 0.0, 0.0}),
        actorAt(5, {0.0, 0.0, 0.0}, {}, {}, -1.0),
        actorAt(1, {-100.0, 0.0, 0.0})
    };

    map.replaceDynamicWorld(std::move(update));

    const NavigationMap::Stats stats = map.stats();
    require(stats.mapRevision == 1, "first publication must advance map revision");
    require(stats.sourceRevision == 41, "source revision must be retained");
    require(stats.actorCount == 4, "accepted actor ownership count mismatch");
    require(stats.indexedActorCount == 3, "indexed actor count mismatch");
    require(stats.outOfBoundsActorCount == 1, "out-of-bounds count mismatch");
    require(stats.rejectedActorCount == 2, "rejected actor count mismatch");

    NavigationMap::CorridorQuery corridor;
    corridor.startMapMeters = {-500.0, 0.0, 0.0};
    corridor.endMapMeters = {500.0, 0.0, 0.0};
    corridor.radiusMeters = 5.0;

    const NavigationMap::QueryResult corridorResult = map.queryCorridor(corridor);
    require(corridorResult.mapRevision == 1, "query must identify its map revision");
    require(corridorResult.sourceRevision == 41, "query must identify source revision");
    require(corridorResult.candidates.size() == 2, "corridor candidate count mismatch");
    require(corridorResult.candidates[0].entityId == 1, "candidate ordering must be deterministic");
    require(corridorResult.candidates[1].entityId == 2, "predicted crossing actor must be retained");
    require(
        corridorResult.diagnostics.actorsExamined < stats.actorCount,
        "small corridor query must not degrade into an accepted-actor full scan"
    );

    const NavigationMap::Candidate& moving = requireCandidate(corridorResult, 2);
    require(
        nearlyEqual(moving.predictedEndPositionMapMeters.y, 0.0),
        "constant-acceleration prediction endpoint mismatch"
    );
    require(
        nearlyEqual(moving.conservativeSweptRadiusMeters, 360.0),
        "conservative swept radius mismatch"
    );

    NavigationMap::SphereQuery sphere;
    sphere.centerMapMeters = {100.0, 0.0, 0.0};
    sphere.radiusMeters = 1.0;

    const NavigationMap::QueryResult sphereResult = map.querySphere(sphere);
    require(sphereResult.candidates.size() == 1, "local sphere query should isolate actor 1");
    require(sphereResult.candidates.front().entityId == 1, "local sphere result id mismatch");
}

void testMapOwnsOnlyNavLocalCoordinates()
{
    NavigationMap map;

    NavigationMap::DynamicWorldUpdate update;
    update.sourceRevision = 100;

    auto actor = actorAt(
        7,
        {50.0, -20.0, 10.0},
        {3.0, 4.0, 5.0},
        {0.25, 0.5, 0.75}
    );
    actor.angularVelocityMapRadPerSecond = {0.1, 0.2, 0.3};
    update.actors.push_back(actor);

    map.replaceDynamicWorld(std::move(update));

    NavigationMap::SphereQuery query;
    query.centerMapMeters = {50.0, -20.0, 10.0};
    query.radiusMeters = 1.0;
    const auto result = map.querySphere(query);
    const auto& local = requireCandidate(result, 7);

    require(nearlyEqual(local.positionMapMeters.x, 50.0) &&
            nearlyEqual(local.positionMapMeters.y, -20.0) &&
            nearlyEqual(local.positionMapMeters.z, 10.0),
            "NavigationMap must preserve already-converted NavLocal position");
    require(nearlyEqual(local.velocityMapMetersPerSecond.x, 3.0) &&
            nearlyEqual(local.velocityMapMetersPerSecond.y, 4.0) &&
            nearlyEqual(local.velocityMapMetersPerSecond.z, 5.0),
            "NavigationMap must not reinterpret NavLocal velocity");
    require(nearlyEqual(local.angularVelocityMapRadPerSecond.x, 0.1) &&
            nearlyEqual(local.angularVelocityMapRadPerSecond.y, 0.2) &&
            nearlyEqual(local.angularVelocityMapRadPerSecond.z, 0.3),
            "NavigationMap must not own system/frame angular conversion");
}

void testInvalidActorsAreRejectedWithoutChangingCoordinateSemantics()
{
    NavigationMap map;

    NavigationMap::DynamicWorldUpdate update;
    update.sourceRevision = 7;
    update.actors.push_back(actorAt(1, {10.0, 0.0, 0.0}));

    auto invalid = actorAt(2, {20.0, 0.0, 0.0});
    invalid.radiusMeters = -1.0;
    update.actors.push_back(invalid);

    map.replaceDynamicWorld(std::move(update));

    const NavigationMap::Stats stats = map.stats();
    require(stats.actorCount == 1, "valid NavLocal actor was lost");
    require(stats.rejectedActorCount == 1, "invalid NavLocal actor was not rejected");
}

void testQueryLookAheadOverridesMapDefault()
{
    NavigationMap::Config config;
    config.halfExtentMeters = 1000.0;
    config.cellSizeMeters = 50.0;
    config.predictionHorizonSeconds = 1.0;
    config.interactionMarginMeters = 0.0;

    NavigationMap map(config);

    NavigationMap::DynamicWorldUpdate update;
    update.sourceRevision = 200;
    update.actors.push_back(
        actorAt(
            90,
            {0.0, 500.0, 0.0},
            {0.0, -100.0, 0.0}
        )
    );
    map.replaceDynamicWorld(std::move(update));

    NavigationMap::SphereQuery shortQuery;
    shortQuery.centerMapMeters = {0.0, 0.0, 0.0};
    shortQuery.radiusMeters = 1.0;

    const auto shortResult = map.querySphere(shortQuery);
    require(shortResult.candidates.empty(),
            "default 1 s broadphase must not invent a distant crossing");
    require(nearlyEqual(shortResult.lookAheadSeconds, 1.0),
            "query result must publish the effective default look-ahead");

    NavigationMap::SphereQuery physicalQuery = shortQuery;
    physicalQuery.lookAheadSeconds = 5.0;

    const auto physicalResult = map.querySphere(physicalQuery);
    const auto& crossing = requireCandidate(physicalResult, 90);

    require(nearlyEqual(physicalResult.lookAheadSeconds, 5.0),
            "query-time physical look-ahead override was lost");
    require(nearlyEqual(crossing.predictionHorizonSeconds, 5.0),
            "candidate must identify the horizon used for its prediction");
    require(nearlyEqual(crossing.predictedEndPositionMapMeters.y, 0.0),
            "extended physical horizon must predict the incoming crossing");
    require(nearlyEqual(crossing.conservativeSweptRadiusMeters, 510.0),
            "extended broadphase sweep must cover the full requested horizon");
}

} // namespace

int main()
{
    try
    {
        testOwnedShipCenteredSnapshotAndQueries();
        testMapOwnsOnlyNavLocalCoordinates();
        testInvalidActorsAreRejectedWithoutChangingCoordinateSemantics();
        testQueryLookAheadOverridesMapDefault();
        std::cout << "NAVIGATION MAP CONTRACT TESTS: PASS\n";
        std::cout << " - snapshot ownership / sparse queries\n";
        std::cout << " - NavLocal-only publication / no hidden frame transform\n";
        std::cout << " - invalid actor rejection without coordinate reinterpretation\n";
        std::cout << " - query-time physical look-ahead expands dynamic broadphase\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION MAP CONTRACT TESTS: FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
