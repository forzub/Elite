#include "world/navigation/local/LocalAvoidancePlanner.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Avoidance = world::navigation::LocalAvoidancePlanner;
using Horizon = world::navigation::LocalHorizonPlanner;
using Map = world::navigation::NavigationMap;
using Space = world::navigation::NavigationSpace;

constexpr double kTolerance = 1.0e-6;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b)
{
    return std::abs(a - b) <= kTolerance;
}

Avoidance::Query baseQuery()
{
    Avoidance::Query query;
    query.horizon.agent.entityId = 100;
    query.horizon.agent.positionMapMeters = {0.0, 0.0, 0.0};
    query.horizon.agent.velocityMapMetersPerSecond = {10.0, 0.0, 0.0};
    query.horizon.agent.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    query.horizon.agent.radiusMeters = 2.0;

    query.horizon.nominalTarget.positionMapMeters = {100.0, 0.0, 0.0};
    query.horizon.nominalTarget.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    query.horizon.nominalTarget.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};

    query.horizon.dynamicResultAgeSeconds = 0.0;
    query.horizon.policy.lookAheadSeconds = 3.0;
    query.horizon.policy.maxResultAgeSeconds = 0.25;
    query.horizon.policy.maxBrakingAccelerationMetersPerSecond2 = 10.0;
    query.horizon.policy.turnDistanceMeters = 0.0;
    query.horizon.policy.safetyMarginMeters = 1.0;
    query.horizon.policy.minimumHorizonMeters = 10.0;

    query.avoidance.primaryDeflectionRadians = 0.2617993877991494;
    query.avoidance.secondaryDeflectionRadians = 0.5235987755982988;
    query.avoidance.azimuthSamples = 8;
    query.avoidance.staticAdditionalClearanceMeters = 0.0;
    return query;
}

Map::QueryResult dynamicResult()
{
    Map::QueryResult result;
    result.mapRevision = 7;
    result.sourceRevision = 11;
    return result;
}

Map::Candidate stationaryCandidate(
    Map::EntityId id,
    Map::Vec3d position,
    double radius,
    double sweptRadius
)
{
    Map::Candidate candidate;
    candidate.entityId = id;
    candidate.positionMapMeters = position;
    candidate.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    candidate.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    candidate.predictedEndPositionMapMeters = position;
    candidate.conservativeSweptCenterMapMeters = position;
    candidate.actorRadiusMeters = radius;
    candidate.conservativeSweptRadiusMeters = sweptRadius;
    candidate.motionRevision = 1;
    return candidate;
}

Map::Candidate movingCandidate(
    Map::EntityId id,
    Map::Vec3d position,
    Map::Vec3d velocity,
    double radius,
    double sweptRadius
)
{
    Map::Candidate candidate = stationaryCandidate(id, position, radius, sweptRadius);
    candidate.velocityMapMetersPerSecond = velocity;
    candidate.predictedEndPositionMapMeters = {
        position.x + velocity.x * 3.0,
        position.y + velocity.y * 3.0,
        position.z + velocity.z * 3.0
    };
    return candidate;
}

Space makeSingleRegionSpace(double lateralHalfExtent)
{
    Space space;
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 31;

    Space::RegionInput region;
    region.regionId = 1;
    region.boundsMapMeters.minMapMeters = {-20.0, -lateralHalfExtent, -lateralHalfExtent};
    region.boundsMapMeters.maxMapMeters = {200.0, lateralHalfExtent, lateralHalfExtent};
    region.clearanceRadiusMeters = 1000.0;
    region.geometryRevision = 1;
    update.regions.push_back(region);

    space.replaceStaticWorld(std::move(update));
    return space;
}

void testNominalClearPassesThroughWithoutProbes()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(100.0);

    const Avoidance::Result result = planner.evaluate(
        query,
        dynamicResult(),
        space
    );

    require(result.status == Avoidance::Status::NominalClear,
            "clear nominal target must pass through avoidance unchanged");
    require(!result.adjustedTarget && result.targetProbesExamined == 0,
            "clear nominal target must not spend lateral probes");
    require(result.target.status == Horizon::Status::Clear,
            "nested horizon result must remain clear");
}

void testSweptCorridorBlockerFindsSameRegionLateralTarget()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(100.0);
    Map::QueryResult dynamic = dynamicResult();

    // Current +X velocity misses this actor by 8 m (> required 4 m), so the
    // closest-approach check is clear. The nominal +X target segment touches the
    // conservative swept envelope exactly, forcing only the corridor check to
    // seek a lateral target.
    dynamic.candidates.push_back(stationaryCandidate(
        200,
        {8.0, 8.0, 0.0},
        1.0,
        5.0
    ));

    const Avoidance::Result result = planner.evaluate(query, dynamic, space);

    require(result.status == Avoidance::Status::AdjustedClear,
            "swept corridor blocker should admit a statically proven lateral target");
    require(result.adjustedTarget,
            "adjusted-clear result must mark adjusted target ownership");
    require(result.target.status == Horizon::Status::Clear,
            "adjusted target must pass dynamic reference checks");
    require(result.target.targetMode == Horizon::TargetMode::PassThrough,
            "adjusted target must be temporary/pass-through");
    require(result.targetProbesExamined >= 1,
            "avoidance must report lateral probe work");
    require(result.startRegionId == 1,
            "same-region static proof must preserve start region identity");
    require(!near(result.target.targetPositionMapMeters.y, 0.0) ||
            !near(result.target.targetPositionMapMeters.z, 0.0),
            "adjusted target must actually deflect laterally");
}

void testHeadOnConflictRemainsFailClosed()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(100.0);
    Map::QueryResult dynamic = dynamicResult();
    dynamic.candidates.push_back(movingCandidate(
        201,
        {30.0, 0.0, 0.0},
        {-10.0, 0.0, 0.0},
        2.0,
        35.0
    ));

    const Avoidance::Result result = planner.evaluate(query, dynamic, space);

    require(result.status == Avoidance::Status::ConflictHold,
            "current-kinematics head-on conflict must remain fail closed");
    require(!result.adjustedTarget,
            "lateral target must not magically erase current head-on kinematics");
    require(result.dynamicRejected > 0,
            "head-on fixture must reject dynamically unsafe lateral probes");
}

void testNarrowStaticRegionRejectsLateralBypass()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(2.5);
    Map::QueryResult dynamic = dynamicResult();
    dynamic.candidates.push_back(stationaryCandidate(
        202,
        {8.0, 8.0, 0.0},
        1.0,
        5.0
    ));

    const Avoidance::Result result = planner.evaluate(query, dynamic, space);

    require(result.status == Avoidance::Status::ConflictHold,
            "lateral bypass without static same-region proof must remain hold");
    require(!result.adjustedTarget,
            "static rejection must not publish adjusted target");
    require(result.staticRejected == result.targetProbesExamined,
            "narrow region should reject every probe statically");
    require(result.dynamicRejected == 0,
            "statically rejected targets must not reach dynamic evaluation");
}

void testStaleDynamicResultSkipsAvoidanceProbes()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    query.horizon.dynamicResultAgeSeconds = 0.5;
    Space space = makeSingleRegionSpace(100.0);
    Map::QueryResult dynamic = dynamicResult();
    dynamic.candidates.push_back(stationaryCandidate(
        203,
        {8.0, 8.0, 0.0},
        1.0,
        5.0
    ));

    const Avoidance::Result result = planner.evaluate(query, dynamic, space);

    require(result.status == Avoidance::Status::StaleHold,
            "stale dynamic result must fail closed before avoidance");
    require(result.targetProbesExamined == 0,
            "stale result must not spend lateral probe work");
}

void testNonTraversableStartFailsStaticHold()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(1.0);
    Map::QueryResult dynamic = dynamicResult();
    dynamic.candidates.push_back(stationaryCandidate(
        204,
        {8.0, 8.0, 0.0},
        1.0,
        5.0
    ));

    const Avoidance::Result result = planner.evaluate(query, dynamic, space);

    require(result.status == Avoidance::Status::StaticHold,
            "agent outside envelope-safe static free space must fail static hold");
    require(result.targetProbesExamined == 0,
            "invalid static start must stop before target probes");
}

} // namespace

int main()
{
    try
    {
        testNominalClearPassesThroughWithoutProbes();
        testSweptCorridorBlockerFindsSameRegionLateralTarget();
        testHeadOnConflictRemainsFailClosed();
        testNarrowStaticRegionRejectsLateralBypass();
        testStaleDynamicResultSkipsAvoidanceProbes();
        testNonTraversableStartFailsStaticHold();

        std::cout << "NAVIGATION LOCAL AVOIDANCE CONTRACT TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION LOCAL AVOIDANCE CONTRACT TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
