#include "world/navigation/local/LocalHorizonPlanner.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Planner = world::navigation::LocalHorizonPlanner;
using Map = world::navigation::NavigationMap;

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

Planner::Query baseQuery()
{
    Planner::Query query;
    query.agent.entityId = 100;
    query.agent.positionMapMeters = {0.0, 0.0, 0.0};
    query.agent.velocityMapMetersPerSecond = {10.0, 0.0, 0.0};
    query.agent.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    query.agent.radiusMeters = 2.0;

    query.nominalTarget.positionMapMeters = {100.0, 0.0, 0.0};
    query.nominalTarget.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    query.nominalTarget.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};

    query.dynamicResultAgeSeconds = 0.0;
    query.policy.lookAheadSeconds = 3.0;
    query.policy.maxResultAgeSeconds = 0.25;
    query.policy.maxBrakingAccelerationMetersPerSecond2 = 10.0;
    query.policy.turnDistanceMeters = 0.0;
    query.policy.safetyMarginMeters = 5.0;
    query.policy.minimumHorizonMeters = 10.0;
    return query;
}

Map::Candidate candidate(
    Map::EntityId id,
    Map::Vec3d position,
    Map::Vec3d velocity,
    double radius = 2.0,
    double sweptRadius = 2.0
)
{
    Map::Candidate result;
    result.entityId = id;
    result.positionMapMeters = position;
    result.velocityMapMetersPerSecond = velocity;
    result.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    result.predictedEndPositionMapMeters = {
        position.x + velocity.x * 3.0,
        position.y + velocity.y * 3.0,
        position.z + velocity.z * 3.0
    };
    result.conservativeSweptCenterMapMeters = position;
    result.actorRadiusMeters = radius;
    result.conservativeSweptRadiusMeters = sweptRadius;
    result.motionRevision = 1;
    return result;
}

Map::QueryResult resultWithRevision()
{
    Map::QueryResult result;
    result.mapRevision = 7;
    result.sourceRevision = 11;
    return result;
}

void testClearPassThroughIsBoundedByPhysicalHorizon()
{
    Planner planner;
    Planner::Query query = baseQuery();
    Map::QueryResult dynamic = resultWithRevision();
    dynamic.candidates.push_back(candidate(
        200,
        {0.0, 1000.0, 0.0},
        {0.0, 0.0, 0.0}
    ));

    const Planner::Result result = planner.evaluate(query, dynamic);

    require(result.status == Planner::Status::Clear,
            "clear candidate set must remain clear");
    require(result.targetMode == Planner::TargetMode::PassThrough,
            "far nominal target must become bounded pass-through target");
    require(result.safeProgressTargetDemonstrated,
            "clear bounded target must be demonstrated safe");
    require(near(result.horizonDistanceMeters, 10.0),
            "physical horizon must include braking + margin and minimum bound");
    require(near(result.targetPositionMapMeters.x, 10.0) &&
            near(result.targetPositionMapMeters.y, 0.0),
            "pass-through target must be clipped to physical horizon");
    require(result.mapRevision == 7 && result.sourceRevision == 11,
            "local result must preserve dynamic snapshot revisions");
    require(result.candidatesExamined == 1 && result.conflictsFound == 0,
            "clear fixture diagnostics mismatch");
}

void testTerminalTargetPreservesTerminalKinematics()
{
    Planner planner;
    Planner::Query query = baseQuery();
    query.agent.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    query.nominalTarget.positionMapMeters = {5.0, 0.0, 0.0};
    query.nominalTarget.velocityMapMetersPerSecond = {1.0, 2.0, 3.0};
    query.nominalTarget.accelerationMapMetersPerSecond2 = {0.1, 0.2, 0.3};

    const Planner::Result result = planner.evaluate(query, resultWithRevision());

    require(result.status == Planner::Status::Clear,
            "terminal fixture must be clear");
    require(result.targetMode == Planner::TargetMode::Terminal,
            "nominal target inside horizon must remain terminal");
    require(near(result.targetPositionMapMeters.x, 5.0),
            "terminal target position changed");
    require(near(result.targetVelocityMapMetersPerSecond.y, 2.0) &&
            near(result.targetAccelerationMapMetersPerSecond2.z, 0.3),
            "terminal kinematics must be preserved");
}

void testCrossingActorFailsClosed()
{
    Planner planner;
    Planner::Query query = baseQuery();
    Map::QueryResult dynamic = resultWithRevision();
    dynamic.candidates.push_back(candidate(
        201,
        {5.0, 20.0, 0.0},
        {0.0, -20.0, 0.0}
    ));

    const Planner::Result result = planner.evaluate(query, dynamic);

    require(result.status == Planner::Status::ConflictHold,
            "crossing actor must block progress");
    require(result.targetMode == Planner::TargetMode::Hold,
            "crossing conflict must fail closed to hold");
    require(!result.safeProgressTargetDemonstrated,
            "conflict must not claim a safe progress target");
    require(result.primaryConflictEntityId == 201,
            "crossing conflict identity mismatch");
    require(result.conflictsFound == 1,
            "crossing fixture must record one conflict");
    require(near(result.targetPositionMapMeters.x, 0.0),
            "hold target must remain at current position");
}

void testHeadOnActorFailsClosed()
{
    Planner planner;
    Planner::Query query = baseQuery();
    Map::QueryResult dynamic = resultWithRevision();
    dynamic.candidates.push_back(candidate(
        202,
        {30.0, 0.0, 0.0},
        {-10.0, 0.0, 0.0}
    ));

    const Planner::Result result = planner.evaluate(query, dynamic);

    require(result.status == Planner::Status::ConflictHold,
            "head-on actor must block progress");
    require(result.primaryConflictEntityId == 202,
            "head-on conflict identity mismatch");
    require(result.primaryTimeToClosestSeconds > 1.4 &&
            result.primaryTimeToClosestSeconds < 1.6,
            "head-on closest-approach time mismatch");
    require(result.primaryClosestDistanceMeters < 0.001,
            "head-on closest distance must be near zero");
}

void testStaleSnapshotFailsClosedBeforeCandidateWork()
{
    Planner planner;
    Planner::Query query = baseQuery();
    query.dynamicResultAgeSeconds = 0.5;

    Map::QueryResult dynamic = resultWithRevision();
    dynamic.candidates.push_back(candidate(
        203,
        {1000.0, 1000.0, 0.0},
        {0.0, 0.0, 0.0}
    ));

    const Planner::Result result = planner.evaluate(query, dynamic);

    require(result.status == Planner::Status::StaleHold,
            "stale dynamic snapshot must fail closed");
    require(result.targetMode == Planner::TargetMode::Hold,
            "stale snapshot must request hold");
    require(result.candidatesExamined == 0,
            "stale result must fail before per-candidate work");
    require(!result.safeProgressTargetDemonstrated,
            "stale result cannot demonstrate safe progress");
}

void testSelfCandidateIsIgnored()
{
    Planner planner;
    Planner::Query query = baseQuery();
    Map::QueryResult dynamic = resultWithRevision();
    dynamic.candidates.push_back(candidate(
        query.agent.entityId,
        query.agent.positionMapMeters,
        query.agent.velocityMapMetersPerSecond
    ));

    const Planner::Result result = planner.evaluate(query, dynamic);

    require(result.status == Planner::Status::Clear,
            "self candidate must not create a conflict");
    require(result.candidatesExamined == 0 && result.conflictsFound == 0,
            "self candidate must be filtered before diagnostics");
}

void testInvalidCandidateIsRejected()
{
    Planner planner;
    Planner::Query query = baseQuery();
    Map::QueryResult dynamic = resultWithRevision();
    Map::Candidate invalid = candidate(
        204,
        {20.0, 0.0, 0.0},
        {0.0, 0.0, 0.0}
    );
    invalid.conservativeSweptRadiusMeters = 1.0;
    invalid.actorRadiusMeters = 2.0;
    dynamic.candidates.push_back(invalid);

    bool threw = false;
    try
    {
        (void)planner.evaluate(query, dynamic);
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    require(threw, "invalid candidate contract must be rejected");
}

} // namespace

int main()
{
    try
    {
        testClearPassThroughIsBoundedByPhysicalHorizon();
        testTerminalTargetPreservesTerminalKinematics();
        testCrossingActorFailsClosed();
        testHeadOnActorFailsClosed();
        testStaleSnapshotFailsClosedBeforeCandidateWork();
        testSelfCandidateIsIgnored();
        testInvalidCandidateIsRejected();

        std::cout << "NAVIGATION LOCAL HORIZON CONTRACT TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "NAVIGATION LOCAL HORIZON CONTRACT TESTS: FAIL: "
                  << e.what() << '\n';
        return 1;
    }
}
