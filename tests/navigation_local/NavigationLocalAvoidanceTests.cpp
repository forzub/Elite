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

world::navigation::NavigationObstacle staticBox(
    const std::string& id,
    std::uint32_t entityId,
    const glm::dvec3& center,
    const glm::dvec3& halfExtents
)
{
    world::navigation::NavigationObstacle obstacle;
    obstacle.id = id;
    obstacle.entityId = entityId;
    obstacle.shape =
        world::navigation::NavigationObstacleShape::Box;
    obstacle.centerMeters = center;
    obstacle.localToWorldBasis = glm::dmat3(1.0);
    obstacle.halfExtentsMeters = halfExtents;
    return obstacle;
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

void testVisibilitySteeringWidensThenReturnsToDirectLine()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    query.avoidance.maximumDeflectionRadians =
        1.3089969389957472; // 75 deg.
    Space space = makeSingleRegionSpace(100.0);

    Map::QueryResult blocked = dynamicResult();

    // The direct corridor and the first small deflections remain inside the
    // candidate's conservative swept occupancy, while a wider local steering
    // direction is clear. Current closest-approach kinematics are deliberately
    // non-conflicting so this fixture isolates bounded visibility steering.
    Map::Candidate wideFanBlocker = stationaryCandidate(
        205,
        {8.0, 4.0, 0.0},
        0.25,
        3.0
    );

    // Keep current closest-approach kinematics clear by leaving the actual
    // actor off the +X line, but center the conservative swept occupancy on
    // the nominal corridor. That makes every 15/30-degree azimuth remain
    // blocked while a wider ring becomes available.
    wideFanBlocker.conservativeSweptCenterMapMeters = {8.0, 0.0, 0.0};
    blocked.candidates.push_back(wideFanBlocker);

    const Avoidance::Result bypass =
        planner.evaluate(query, blocked, space);

    require(bypass.status == Avoidance::Status::AdjustedClear,
            "bounded visibility steering must widen until a safe corridor exists");
    require(bypass.adjustedTarget &&
            !bypass.nominalVisibilityClear,
            "blocked direct line must publish a temporary visibility bypass");
    require(bypass.selectedDeflectionRadians >
                query.avoidance.secondaryDeflectionRadians,
            "fixture must require more than the legacy 15/30 degree fan");
    require(bypass.selectedDeflectionRadians <=
                query.avoidance.maximumDeflectionRadians + kTolerance,
            "visibility bypass must remain inside the bounded angular search");

    // Receding-horizon recovery is intentionally stateless: on the next update
    // the direct A->B corridor is always tested first.
    const Avoidance::Result recovered =
        planner.evaluate(query, dynamicResult(), space);

    require(recovered.status == Avoidance::Status::NominalClear &&
            recovered.nominalVisibilityClear &&
            !recovered.adjustedTarget &&
            near(recovered.selectedDeflectionRadians, 0.0),
            "once direct visibility returns the planner must immediately resume A->B");
}

void testExactStaticBlockerTriggersAvoidanceWithoutDynamicCandidate()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();

    Space::StaticSpaceUpdate update;
    update.sourceRevision = 32;

    Space::RegionInput region;
    region.regionId = 1;
    region.boundsMapMeters.minMapMeters = {-20.0, -100.0, -100.0};
    region.boundsMapMeters.maxMapMeters = {200.0, 100.0, 100.0};
    region.clearanceRadiusMeters = 1000.0;
    region.geometryRevision = 1;
    update.regions.push_back(region);
    update.obstacles.push_back(
        staticBox(
            "static_nominal_wall",
            900,
            glm::dvec3(8.0, 0.0, 0.0),
            glm::dvec3(1.0, 0.25, 0.25)
        )
    );

    Space space;
    space.replaceStaticWorld(std::move(update));

    const Avoidance::Result result = planner.evaluate(
        query,
        dynamicResult(),
        space
    );

    require(result.status == Avoidance::Status::AdjustedClear,
            "exact static blocker must trigger bounded avoidance even when NavigationMap is clear");
    require(result.adjustedTarget,
            "static-only avoidance must publish adjusted target ownership");
    require(result.nominalStaticBlocked,
            "static-only avoidance must record that the nominal segment was blocked");
    require(result.nominalStaticObstacleId == "static_nominal_wall" &&
            result.nominalStaticObstacleEntityId == 900,
            "static-only avoidance must retain exact blocker identity");
    require(result.nominalConflictsFound == 0 &&
            result.nominalPrimaryConflictEntityId == 0,
            "static-only blocker must not fabricate a dynamic conflict");
    require(result.targetProbesExamined > 0 &&
            result.staticRejected > 0,
            "static-only blocker must spend bounded probes and reject intersecting ones");
    require(result.staticObstaclesExamined > 0,
            "static-only avoidance must expose exact obstacle work");
}

void testDynamicConflictStillPreservesExactStaticNominalProof()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();

    Space::StaticSpaceUpdate update;
    update.sourceRevision = 33;

    Space::RegionInput region;
    region.regionId = 1;
    region.boundsMapMeters.minMapMeters = {-20.0, -100.0, -100.0};
    region.boundsMapMeters.maxMapMeters = {200.0, 100.0, 100.0};
    region.clearanceRadiusMeters = 1000.0;
    region.geometryRevision = 1;
    update.regions.push_back(region);
    update.obstacles.push_back(
        staticBox(
            "shared_static_dynamic_blocker",
            901,
            glm::dvec3(8.0, 0.0, 0.0),
            glm::dvec3(1.0, 0.25, 0.25)
        )
    );

    Space space;
    space.replaceStaticWorld(std::move(update));

    Map::QueryResult dynamic = dynamicResult();
    dynamic.candidates.push_back(stationaryCandidate(
        901,
        {8.0, 0.0, 0.0},
        1.5,
        1.5
    ));

    const Avoidance::Result result =
        planner.evaluate(query, dynamic, space);

    require(result.nominalConflictsFound > 0 &&
            result.nominalPrimaryConflictEntityId == 901,
            "fixture must retain the dynamic conflict identity");
    require(result.nominalStaticBlocked,
            "dynamic ConflictHold must not collapse exact-static nominal proof to a zero segment");
    require(result.nominalStaticObstacleId ==
                "shared_static_dynamic_blocker" &&
            result.nominalStaticObstacleEntityId == 901,
            "same physical blocker must remain identifiable in exact static proof");
    require(result.staticObstaclesExamined > 0,
            "dynamic conflict must not bypass exact static query work");
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
        testVisibilitySteeringWidensThenReturnsToDirectLine();
        testExactStaticBlockerTriggersAvoidanceWithoutDynamicCandidate();
        testDynamicConflictStillPreservesExactStaticNominalProof();
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
