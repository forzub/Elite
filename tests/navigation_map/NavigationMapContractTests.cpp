#include "world/navigation/map/NavigationMap.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
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

Vec3d add(const Vec3d& a, const Vec3d& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

NavigationMap::DynamicActorInput actorAt(
    NavigationMap::EntityId id,
    const Vec3d& systemPosition,
    const Vec3d& systemVelocity = {},
    const Vec3d& systemAcceleration = {},
    double radiusMeters = 10.0
)
{
    NavigationMap::DynamicActorInput actor;
    actor.entityId = id;
    actor.positionSystemMeters = systemPosition;
    actor.velocitySystemMetersPerSecond = systemVelocity;
    actor.accelerationSystemMetersPerSecond2 = systemAcceleration;
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

    const Vec3d origin {1.0e9, -2.0e9, 3.0e9};

    NavigationMap::DynamicWorldUpdate update;
    update.sourceRevision = 41;
    update.workingFrame.originSystemMeters = origin;
    update.actors = {
        actorAt(1, add(origin, {100.0, 0.0, 0.0})),
        actorAt(
            2,
            add(origin, {0.0, 350.0, 0.0}),
            {0.0, -175.0, 0.0}
        ),
        actorAt(3, add(origin, {0.0, 700.0, 0.0})),
        actorAt(4, add(origin, {1500.0, 0.0, 0.0})),
        actorAt(5, add(origin, {0.0, 0.0, 0.0}), {}, {}, -1.0),
        actorAt(1, add(origin, {-100.0, 0.0, 0.0}))
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

void testRebaseAndStableBasisAreOwnedByMap()
{
    NavigationMap::Config config;
    config.halfExtentMeters = 1000.0;
    config.cellSizeMeters = 100.0;
    config.predictionHorizonSeconds = 0.0;
    config.interactionMarginMeters = 0.0;

    NavigationMap map(config);

    NavigationMap::DynamicWorldUpdate rebaseUpdate;
    rebaseUpdate.sourceRevision = 100;
    rebaseUpdate.workingFrame.originSystemMeters = {1050.0, 2000.0, 3000.0};
    rebaseUpdate.actors.push_back(actorAt(7, {1100.0, 2000.0, 3000.0}));
    map.replaceDynamicWorld(std::move(rebaseUpdate));

    NavigationMap::SphereQuery rebaseQuery;
    rebaseQuery.centerMapMeters = {50.0, 0.0, 0.0};
    rebaseQuery.radiusMeters = 1.0;
    const NavigationMap::QueryResult rebaseResult = map.querySphere(rebaseQuery);
    const NavigationMap::Candidate& rebased = requireCandidate(rebaseResult, 7);
    require(
        nearlyEqual(rebased.positionMapMeters.x, 50.0),
        "NavigationMap must own system-to-working-origin rebasing"
    );

    NavigationMap::DynamicWorldUpdate rotatedUpdate;
    rotatedUpdate.sourceRevision = 101;
    rotatedUpdate.workingFrame.originSystemMeters = {0.0, 0.0, 0.0};
    rotatedUpdate.workingFrame.xAxisSystem = {0.0, 1.0, 0.0};
    rotatedUpdate.workingFrame.yAxisSystem = {-1.0, 0.0, 0.0};
    rotatedUpdate.workingFrame.zAxisSystem = {0.0, 0.0, 1.0};
    rotatedUpdate.actors.push_back(actorAt(8, {0.0, 100.0, 0.0}));
    map.replaceDynamicWorld(std::move(rotatedUpdate));

    NavigationMap::SphereQuery basisQuery;
    basisQuery.centerMapMeters = {100.0, 0.0, 0.0};
    basisQuery.radiusMeters = 1.0;
    const NavigationMap::QueryResult basisResult = map.querySphere(basisQuery);
    const NavigationMap::Candidate& rotated = requireCandidate(basisResult, 8);
    require(
        nearlyEqual(rotated.positionMapMeters.x, 100.0) &&
        nearlyEqual(rotated.positionMapMeters.y, 0.0),
        "NavigationMap must own stable-basis transformation"
    );
}

void testRejectedFrameDoesNotReplaceAcceptedMap()
{
    NavigationMap map;

    NavigationMap::DynamicWorldUpdate accepted;
    accepted.sourceRevision = 7;
    accepted.actors.push_back(actorAt(1, {10.0, 0.0, 0.0}));
    map.replaceDynamicWorld(std::move(accepted));

    const NavigationMap::Stats before = map.stats();

    NavigationMap::DynamicWorldUpdate invalid;
    invalid.sourceRevision = 8;
    invalid.workingFrame.xAxisSystem = {1.0, 0.0, 0.0};
    invalid.workingFrame.yAxisSystem = {1.0, 0.0, 0.0};
    invalid.workingFrame.zAxisSystem = {0.0, 0.0, 1.0};

    bool threw = false;
    try
    {
        map.replaceDynamicWorld(std::move(invalid));
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }

    require(threw, "non-orthogonal NavigationMap frame must be rejected");
    const NavigationMap::Stats after = map.stats();
    require(after.mapRevision == before.mapRevision, "rejected update changed map revision");
    require(after.sourceRevision == before.sourceRevision, "rejected update changed source revision");
    require(after.actorCount == before.actorCount, "rejected update changed owned actor data");
}

} // namespace

int main()
{
    try
    {
        testOwnedShipCenteredSnapshotAndQueries();
        testRebaseAndStableBasisAreOwnedByMap();
        testRejectedFrameDoesNotReplaceAcceptedMap();
        std::cout << "NAVIGATION MAP CONTRACT TESTS: PASS\n";
        std::cout << " - snapshot ownership / sparse queries\n";
        std::cout << " - ship-centered rebase / stable basis\n";
        std::cout << " - atomic rejection of invalid frame\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION MAP CONTRACT TESTS: FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
