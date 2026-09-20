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
using StaticQueries = world::navigation::NavigationStaticQueryApi;

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
    query.horizon.policy.minimumHorizonMeters = 40.0;

    query.avoidance.lateralGridHalfExtentSamples = 5;
    query.avoidance.minimumLateralStepMeters = 2.0;
    query.avoidance.lateralStepEnvelopeMultiplier = 1.0;
    query.avoidance.maximumLateralOffsetMeters = 80.0;
    query.avoidance.longitudinalSamples = 3;
    query.avoidance.projectionPaddingMeters = 1.0;
    query.avoidance.trajectorySamples = 32;
    query.avoidance.staticAdditionalClearanceMeters = 0.0;
    return query;
}

Map::QueryResult dynamicResult()
{
    Map::QueryResult result;
    result.mapRevision = 7;
    result.sourceRevision = 11;
    result.lookAheadSeconds = 3.0;
    return result;
}

Map::Candidate candidate(
    Map::EntityId id,
    Map::Vec3d position,
    Map::Vec3d velocity,
    double radius,
    double sweptRadius
)
{
    Map::Candidate out;
    out.entityId = id;
    out.positionMapMeters = position;
    out.velocityMapMetersPerSecond = velocity;
    out.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    out.predictionHorizonSeconds = 3.0;
    out.predictedEndPositionMapMeters = {
        position.x + velocity.x * 3.0,
        position.y + velocity.y * 3.0,
        position.z + velocity.z * 3.0
    };
    out.conservativeSweptCenterMapMeters = {
        0.5 * (position.x + out.predictedEndPositionMapMeters.x),
        0.5 * (position.y + out.predictedEndPositionMapMeters.y),
        0.5 * (position.z + out.predictedEndPositionMapMeters.z)
    };
    out.actorRadiusMeters = radius;
    out.conservativeSweptRadiusMeters = sweptRadius;
    out.motionRevision = 1;
    return out;
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
    region.boundsMapMeters.minMapMeters =
        {-20.0, -lateralHalfExtent, -lateralHalfExtent};
    region.boundsMapMeters.maxMapMeters =
        {200.0, lateralHalfExtent, lateralHalfExtent};
    region.clearanceRadiusMeters = 1000.0;
    region.geometryRevision = 1;
    update.regions.push_back(region);

    space.replaceStaticWorld(std::move(update));
    return space;
}

void testNominalClearPassesThroughWithoutOffsetSearch()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(100.0);

    const Avoidance::Result result = planner.evaluate(
        query,
        dynamicResult(),
        StaticQueries(space)
    );

    require(result.status == Avoidance::Status::NominalClear,
            "clear nominal trajectory must remain direct");
    require(result.nominalPathClear &&
            !result.adjustedTarget &&
            result.offsetCandidatesExamined == 0,
            "clear nominal trajectory must not spend offset search work");
    require(result.target.status == Horizon::Status::Clear,
            "nested horizon result must remain clear");
}

void testCrossingObstacleProjectsToNormalPlaneAndFindsBypass()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(100.0);
    Map::QueryResult dynamic = dynamicResult();

    // Crosses the nominal +X trajectory during the 3 s visible horizon.
    dynamic.candidates.push_back(candidate(
        200,
        {20.0, 10.0, 0.0},
        {0.0, -5.0, 0.0},
        2.0,
        10.0
    ));

    const Avoidance::Result result =
        planner.evaluate(query, dynamic, StaticQueries(space));

    require(result.status == Avoidance::Status::AdjustedClear,
            "crossing obstacle must produce a local bypass");
    require(result.adjustedTarget,
            "local bypass must own an adjusted target");
    require(result.projectedDynamicObstacles == 1,
            "moving obstacle must enter normal-plane projection");
    require(result.offsetCandidatesExamined > 0 &&
            result.routeCandidatesExamined > 0,
            "visible horizon must evaluate lateral offsets and longitudinal bypass stations");
    require(result.selectedLateralOffsetMeters > 0.0,
            "bypass must leave the blocked centerline");
    require(result.selectedBypassForwardDistanceMeters > 0.0 &&
            result.selectedBypassForwardDistanceMeters <
                result.target.horizonDistanceMeters,
            "bypass station must be a bounded forward point inside the physical horizon");
    require(
        !near(result.selectedLateralOffsetMap.y, 0.0) ||
        !near(result.selectedLateralOffsetMap.z, 0.0),
        "selected bypass must contain a lateral component");
    require(near(result.mergeTargetMapMeters.y, 0.0) &&
            near(result.mergeTargetMapMeters.z, 0.0),
            "reacquisition reference must remain on the original trajectory");
    require(result.target.targetMode == Horizon::TargetMode::PassThrough,
            "bypass target must remain a temporary pass-through target");
}

void testHeadOnObstacleCanBypassWithoutMandatoryStop()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(120.0);
    Map::QueryResult dynamic = dynamicResult();

    dynamic.candidates.push_back(candidate(
        201,
        {30.0, 0.0, 0.0},
        {-3.0, 0.0, 0.0},
        2.0,
        12.0
    ));

    const Avoidance::Result result =
        planner.evaluate(query, dynamic, StaticQueries(space));

    require(result.status == Avoidance::Status::AdjustedClear,
            "head-on visible obstacle with free lateral space must bypass instead of mandatory stop");
    require(result.selectedLateralOffsetMeters > 0.0,
            "head-on bypass must use normal-plane clearance");
    require(!result.localBypassExhausted,
            "successful head-on bypass must not escalate");
}

void testExactStaticBlockerConstrainsOffsetSearch()
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
            glm::dvec3(12.0, 0.0, 0.0),
            glm::dvec3(1.5, 3.0, 3.0)
        )
    );

    Space space;
    space.replaceStaticWorld(std::move(update));

    const Avoidance::Result result = planner.evaluate(
        query,
        dynamicResult(),
        StaticQueries(space)
    );

    require(result.status == Avoidance::Status::AdjustedClear,
            "exact static blocker must admit a safe projected offset when free space exists");
    require(result.nominalStaticBlocked,
            "static blocker must reject the original trajectory");
    require(result.nominalStaticObstacleId == "static_nominal_wall" &&
            result.nominalStaticObstacleEntityId == 900,
            "exact static blocker identity must survive");
    require(result.staticRejected > 0,
            "some projected offsets must be rejected by exact static geometry");
    require(result.selectedLateralOffsetMeters > 0.0,
            "static blocker must cause a lateral bypass");
}

void testBypassDoesNotRequireImmediateReturnToTrajectory()
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

    // The nominal line is blocked late in the current horizon. A safe short
    // off-route segment exists before the wall, but an immediate return from
    // that bypass point to the current on-route reacquisition reference is
    // deliberately impossible. B4 must still keep navigating through the
    // safe short segment and let later replans reacquire the line.
    update.obstacles.push_back(
        staticBox(
            "late_reacquisition_wall",
            901,
            glm::dvec3(35.0, 0.0, 0.0),
            glm::dvec3(1.5, 50.0, 50.0)
        )
    );

    Space space;
    space.replaceStaticWorld(std::move(update));
    const StaticQueries staticQueries(space);

    const Avoidance::Result result = planner.evaluate(
        query,
        dynamicResult(),
        staticQueries
    );

    require(result.status == Avoidance::Status::AdjustedClear,
            "safe short bypass must not be rejected only because same-horizon reacquisition is blocked");
    require(result.nominalStaticBlocked,
            "fixture must block the nominal bounded line");
    require(result.selectedBypassForwardDistanceMeters > 0.0 &&
            result.selectedBypassForwardDistanceMeters <
                result.target.horizonDistanceMeters,
            "selected bypass must remain a bounded short segment");

    StaticQueries::SegmentQuery immediateReturn;
    immediateReturn.startMapMeters = {
        result.target.targetPositionMapMeters.x,
        result.target.targetPositionMapMeters.y,
        result.target.targetPositionMapMeters.z
    };
    immediateReturn.endMapMeters = {
        result.mergeTargetMapMeters.x,
        result.mergeTargetMapMeters.y,
        result.mergeTargetMapMeters.z
    };
    immediateReturn.envelope.radiusMeters =
        query.horizon.agent.radiusMeters;
    immediateReturn.requireSameRegion = true;

    const auto returnProof =
        staticQueries.querySegment(immediateReturn);

    require(!returnProof.traversable,
            "fixture must prove that immediate return is blocked while the short bypass itself is valid");
}

void testDynamicSphereBroadphaseDoesNotSealClearExactObbRoute()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(100.0);
    Map::QueryResult dynamic = dynamicResult();

    Map::Candidate actor = candidate(
        206,
        {18.0, 6.0, 0.0},
        {0.0, 0.0, 0.0},
        6.0,
        6.0
    );
    actor.exactObstacles.push_back(
        staticBox(
            "dynamic_exact_clear_box",
            206,
            glm::dvec3(18.0, 6.0, 0.0),
            glm::dvec3(1.0, 0.25, 0.25)
        )
    );
    dynamic.candidates.push_back(actor);

    const Avoidance::Result result =
        planner.evaluate(query, dynamic, StaticQueries(space));

    require(result.status == Avoidance::Status::NominalClear,
            "exact dynamic OBB must reject a sphere-only nominal false positive");
    require(result.nominalConflictsFound == 0,
            "broadphase overlap must not survive exact dynamic nominal narrow-phase");
    require(!result.adjustedTarget,
            "clear exact dynamic geometry must preserve direct trajectory");
}

void testObstacleGoneReacquiresNominalTrajectoryWithoutPlannerShutdown()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(100.0);
    Map::QueryResult dynamic = dynamicResult();
    dynamic.candidates.push_back(candidate(
        207,
        {20.0, 10.0, 0.0},
        {0.0, -5.0, 0.0},
        2.0,
        10.0
    ));

    const Avoidance::Result bypass =
        planner.evaluate(query, dynamic, StaticQueries(space));
    require(bypass.status == Avoidance::Status::AdjustedClear,
            "fixture must first create a bypass");

    const Avoidance::Result direct =
        planner.evaluate(query, dynamicResult(), StaticQueries(space));

    require(direct.status == Avoidance::Status::NominalClear &&
            direct.nominalPathClear &&
            !direct.adjustedTarget,
            "once the unexpected obstacle is gone the active planner must resume nominal-line reacquisition");
}

void testNarrowStaticRegionFailsClosedWhenNoOffsetFits()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(2.5);
    Map::QueryResult dynamic = dynamicResult();
    dynamic.candidates.push_back(candidate(
        202,
        {20.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
        2.0,
        8.0
    ));

    const Avoidance::Result result =
        planner.evaluate(query, dynamic, StaticQueries(space));

    require(result.status == Avoidance::Status::ConflictHold,
            "no physically available lateral space must fail closed");
    require(!result.adjustedTarget &&
            result.localBypassExhausted,
            "exhausted projected free space must be explicit");
    require(result.staticRejected > 0,
            "narrow static corridor must reject projected offsets");
}

void testStaleDynamicResultSkipsOffsetSearch()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    query.horizon.dynamicResultAgeSeconds = 0.5;
    Space space = makeSingleRegionSpace(100.0);
    Map::QueryResult dynamic = dynamicResult();
    dynamic.candidates.push_back(candidate(
        203,
        {20.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
        2.0,
        8.0
    ));

    const Avoidance::Result result =
        planner.evaluate(query, dynamic, StaticQueries(space));

    require(result.status == Avoidance::Status::StaleHold,
            "stale dynamic truth must fail closed");
    require(result.offsetCandidatesExamined == 0,
            "stale dynamic truth must not spend bypass search work");
}

void testNonTraversableStartFailsStaticHold()
{
    Avoidance planner;
    Avoidance::Query query = baseQuery();
    Space space = makeSingleRegionSpace(1.0);

    const Avoidance::Result result =
        planner.evaluate(query, dynamicResult(), StaticQueries(space));

    require(result.status == Avoidance::Status::StaticHold,
            "agent outside envelope-safe static free space must fail static hold");
    require(result.offsetCandidatesExamined == 0,
            "invalid static start must stop before bypass search");
}

} // namespace

int main()
{
    try
    {
        testNominalClearPassesThroughWithoutOffsetSearch();
        testCrossingObstacleProjectsToNormalPlaneAndFindsBypass();
        testHeadOnObstacleCanBypassWithoutMandatoryStop();
        testExactStaticBlockerConstrainsOffsetSearch();
        testBypassDoesNotRequireImmediateReturnToTrajectory();
        testDynamicSphereBroadphaseDoesNotSealClearExactObbRoute();
        testObstacleGoneReacquiresNominalTrajectoryWithoutPlannerShutdown();
        testNarrowStaticRegionFailsClosedWhenNoOffsetFits();
        testStaleDynamicResultSkipsOffsetSearch();
        testNonTraversableStartFailsStaticHold();

        std::cout
            << "NAVIGATION LOCAL VISIBLE-HORIZON BYPASS TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NAVIGATION LOCAL VISIBLE-HORIZON BYPASS TESTS: FAIL: "
            << error.what() << '\n';
        return 1;
    }
}
